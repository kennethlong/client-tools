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

## Start here
Begin with fix #1. First verify storage: check whether `music/mus_title_lp.mp3` is stored
uncompressed in its TRE (use `D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py` or the engine's
TreeFile metadata). If uncompressed, wire the subfile descriptor in the stream branch of
`AudioNamespace::playSound`. Tools: `tools/setup/audio-stream-probe.ps1` (cdb probe) confirms the fix
when `SYNC_READ`/file reads continue after `START_STREAM` and audio plays at the idle login screen.
