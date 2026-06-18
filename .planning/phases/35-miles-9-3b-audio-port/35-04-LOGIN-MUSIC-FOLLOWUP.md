# Follow-up: login/character-select screen music silent under Miles 9.3b

**Status:** open · **Scope:** narrow (pre-game screen music only; all in-game audio works) ·
**Found:** 2026-06-18 during 35-04 UAT · **Requirement:** AUDIO-02 (in-game goal already met)

## Symptom
At the login / character-select screen the title music (`music/mus_title_lp.mp3`, via
`sound/music_main_title.snd`) is silent. The same music "kicks in halfway through the planet loading
screen". Identical on Win32 and x64. UI button SFX play immediately. In-game music/3D/reverb all work.
This is a regression from the 7.2e→9.3b Miles swap (engine code otherwise unchanged on this path).

## Root cause (cdb-proven + Miles 9.3b help docs)
- The title music is large → routed to the **streamed** path: `AIL_open_stream(dig, name, 0)` →
  (later) `AIL_start_stream` (`Audio.cpp` ~604 / ~3170). UI sounds and in-game music use the
  **in-memory** path (`AIL_set_named_sample_file`), which is why they work.
- MSS 9.3b services streams by reading chunks through its **own async I/O subsystem**
  (`MilesAsyncFileRead`; `_STREAM.asyncs[]`/`asyncs_loaded[]` in mss.h:4286), **not** the synchronous
  `AIL_set_file_callbacks` the engine registers. The Miles help confirms: streams are auto-serviced
  "with a secondary thread", and file callbacks "can be called from a background thread ... you must
  make sure your IO callbacks are thread-safe."
- Our file callbacks route through `TreeFile`/`AbstractFile` (TRE archives), which is NOT thread-safe,
  and — critically — `AIL_service_stream` never calls them anyway. **Probe proof:** with auto-service
  OFF and a main-thread per-frame `AIL_service_stream` loop, 542 service calls drove the sync read
  callback ZERO times after open; the stream only ever got the single 8 KB read `AIL_open_stream`
  performs during open. So the stream starves → silent. (Heavy zone-load file I/O is unrelated; zone
  music plays via the buffered path.)

## SESSION 2 UPDATE (2026-06-18) — fix #1 and fix #3 both attempted, both failed, both reverted

**Fix #1 (subfile descriptor) — IMPLEMENTED, did not produce audio.**
- Added `TreeFile::getArchiveSubfile()` (+ a `SearchNode`/`SearchTree` virtual) to expose the backing
  `.tre` OS path + offset + length for uncompressed entries; built the Miles descriptor
  `*<archive>*<size>*<offset>.mp3` in `playSound`. Verified at runtime (cdb): the descriptor WAS built
  (AIL_open_stream received `*...`), AIL_open_stream returned a valid HSTREAM and AIL_start_stream
  fired — so Miles parsed the descriptor, opened the archive at the offset, and read the header. Still
  SILENT. Combined with a per-frame main-thread `AIL_service_stream` pump: still silent. Conclusion:
  with `AIL_set_file_callbacks` set, Miles routes the descriptor's archive I/O back through OUR
  callbacks anyway; the stream opens/starts/full-volume but never produces audio, and the manual
  service calls don't even trigger reads (buffers stay full → stream isn't consuming/outputting).
  Root remains in Miles 9.3b's opaque stream→mixer path. REVERTED.

**Fix #3 (route music to in-memory cached path) — IMPLEMENTED, CRASHED.**
- Forced `isNonBufferedMusic` categories down the cached branch in `playSound` (skip streaming).
- Result: `FATAL: sampleRawData pointer is null` at `Audio.cpp` ~2906. Cause: the cached serve path
  needs the file bytes in `SampleCache::m_sampleRawData`, which are loaded only when the SOUND TEMPLATE
  caches them (`SoundTemplate::addSample`, gated by `forceCacheSample || size<=maxCached2dSampleSize`).
  For large music that is false, so the bytes are never loaded. Worse, the load order blocks an easy
  fix: `Sound2dTemplate::load_0000` calls `addSample` (~737) BEFORE it reads `m_soundCategory` (~757),
  so the category isn't known when the cache decision is made; and `Sample2d::setPath` hardcodes
  `cacheSample=false`. Re-caching post-load would double the reference count (leak). REVERTED.

**Remaining viable directions for a future attempt (both deeper engine work):**
- (a) Make music templates cache their sample DATA: pass `forceCacheSample=true` for music either by
  reading `m_soundCategory` BEFORE `addSample` in each `load_000X`, or by adding a post-load
  cache-only pass that loads `m_sampleRawData` WITHOUT bumping the ref count (needs a new
  "ensure-cached" helper). Then the cached path plays music in-memory (proven mechanism). Watch the
  `MemoryManager::getLimit()` gate in `addSample` and looping behavior of cached music.
- (b) Route screen/title music through the dedicated buffered-music player (`s_bufferedMusicSample` /
  `AIL_set_named_sample_file`, Audio.cpp ~5464) instead of the generic sound system.
- (c) Drop `AIL_set_file_callbacks` for the stream path and rely purely on the subfile descriptor +
  Miles's own file I/O (requires the callbacks not to shadow descriptor I/O — needs investigation of
  whether MSS lets you opt a single stream out of the global callbacks).

## What was already tried (all failed — do NOT repeat)
1. `AIL_auto_service_stream(stream, 1)` — inert (probe: read cb still fired once).
2. Per-frame `AIL_serve()` in `Audio::alter()` — `AIL_serve()` doesn't pump the HSTREAM file path.
3. `AIL_service_stream(stream, 1)` prime + per-frame `AIL_service_stream(stream, 0)` — 542 calls, 0 sync reads.
4. Removing the `s_prioritizedPlayingSounds.size() > 0` gate in `Audio::serve()` — no effect.
5. `AIL_auto_service_stream(stream, 0)` + ungated main-thread service loop — no effect.
(All reverted; the false consensus was "AIL_serve/AIL_service_stream uses our sync callbacks" — it does not.)

## Candidate fixes (ranked)
1. **Subfile descriptor (Miles-recommended).** Instead of passing the logical name to
   `AIL_open_stream`, resolve the file's TRE archive path + byte offset + size via the engine's
   TreeFile API and pass `*<full_tre_path>*<size>*<offset>.mp3`. Miles then does its own
   (thread-safe, async-capable) I/O on the real archive file; auto-service ON works; our callbacks are
   not used for streaming. **Caveat:** only works if the music entry is stored UNCOMPRESSED in the TRE
   (MP3 is already compressed, so likely stored, but must verify per-entry; zlib-compressed entries
   won't work this way). Doc: `AIL_open_stream` "How do I access files from a big packed file?".
2. **`AIL_set_async_callbacks` routed to a thread-safe TRE reader.** Provide async read/cancel/status
   callbacks (+ `MilesAsyncStartup`) that read from TRE. Requires making the read path thread-safe.
3. **Route screen music through the in-memory buffered path** (raise `getMaxCached2dSampleSize` for
   music, or send title music through the `s_bufferedMusicSample` API). Simplest, but loads whole
   tracks into memory and may change behavior for all 2D music.

## Start here — fix #1, feasibility CONFIRMED (2026-06-18)

**Storage verified (gating fact):** `music/mus_title_lp.mp3` is **STORED UNCOMPRESSED**.
- archive: `data_music_00.tre` (resolves under `D:/Code/SWGSource Client v3.0/`)
- offset: `64835379`  length: `1853359`  compressor: `0`
- Verified the offset is the absolute raw-data offset: bytes at that offset == the decoded payload,
  start with `FF FB` (MP3 frame sync), offset+size within the file. So the Miles descriptor
  `*<full_path>/data_music_00.tre*1853359*64835379.mp3` will work.

**Engine already has (archive, offset, length).** `FileStreamerFile` (sharedFile) backs a TRE-resolved
`TreeFile::open`. It stores `m_baseOffset` (entry offset in the archive) + `m_file` (the archive,
which knows its OS path) + length (`FileStreamerFile.cpp:151` reads `m_file->read(m_baseOffset + m_offset, ...)`).

**Implementation plan:**
1. Add **non-virtual accessors** to `FileStreamerFile` (and a thin passthrough where needed) to expose:
   OS archive path, `m_baseOffset`, and length. Non-virtual methods reading existing members do NOT
   change object layout → NO plugin ABI cascade (safe; no need to rebuild gl0x).
2. In `AudioNamespace::playSound` stream branch (Audio.cpp ~604), before `AIL_open_stream`:
   - `TreeFile::open` the logical name; if it's a TRE-backed `FileStreamerFile` AND uncompressed,
     read (archivePath, baseOffset, length), close it, build `*archivePath*length*baseOffset.ext`,
     and pass THAT to `AIL_open_stream`. Leave auto-service at its default (ON) — Miles does its own
     thread-safe I/O on the archive, so the secondary-thread + sync-callback problem disappears.
   - Fallback to the plain logical name (current behavior) for loose files / compressed entries.
3. Keep the engine's `AIL_set_file_callbacks` for non-stream uses; the descriptor path bypasses them
   for streams.

**Verify:** boot to login → title music plays; `tools/setup/audio-stream-probe.ps1` shows the stream
fed continuously (Miles reads the archive itself; our SYNC_READ callback no longer needed for it).
Also re-run the D-06 in-game UAT to confirm no regression to zone music / 3D / reverb.
