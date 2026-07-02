# CONSULT-55 — SYNTHESIS: intermittent "Unknown shader template tag" async-load corruption

## Root cause (verified)
Concurrent **unlocked** `std::map` find/insert on `TreeFile::SearchCache::m_cachedFileMap`:
- MAIN thread `insert`s across many zone-in frames via `CachedFileManager::preloadSomeAssets`
  (`SearchCache::addCachedFile` → `m_cachedFileMap->insert`, `TreeFile_SearchNode.cpp:1116`) — the
  space path also runs its preload pump concurrently.
- LOADER thread `find`s via `TreeFile::open` → `SearchCache::open` (`:1200`) and `exists` (`:1154`) —
  and that traversal runs OUTSIDE `TreeFile::ms_criticalSection`; `SearchCache` took NO lock (verified).
- Concurrent find-during-insert on a red-black tree = UB → torn/wrong node → the fetched `.sht` gets
  the wrong file's bytes / a wild buffer → `iff.getCurrentName()` returns garbage (`"??]'"`).

**Amplifier (unanimous):** `ZlibFile::decompress` (`ZlibFile.cpp:152`) discarded `ZlibCompressor::expand`'s
return, so a failed/partial inflate returned an uninitialized `new byte[]` as "valid" IFF data → the
garbage surfaced far away as the ShaderTemplateList FATAL instead of a clean failure at the source.

**"Frequent after rebuild":** cold OS/file caches after a rebuild slow preloading → the main-thread
`insert` stream stretches across many more zone-in frames, exactly while the loader hammers
`TreeFile::open` → the find/insert collision window widens. Warm client finishes preload in 1-2 frames
(or hits the named-ShaderTemplate cache = zero I/O) → tiny window → rare. Crashing asset was a spaceport
shader (space preload pumps run concurrently with async loads) — fits.

## Crew convergence-from-divergence
- **Codex** — refuted the brief's leading suspicion (the async `cachedFilesMap` free/steal path is safe;
  `Iff::open` takes a private copy via `readEntireFileAndClose`). Cleared the red herring.
- **Cursor** — found the concurrent READER (loader thread also calls `TreeFile::open`, `:488`) + the
  engine's own main-thread-only "unexpected cache miss" debug tell. (Proposed the cachedFilesMap
  isMainThread guard; Opus showed the real unlocked map is SearchCache, one layer deeper.)
- **Sonnet** — found the ZlibFile swallowed-`expand()` amplifier + the cold-cache/after-rebuild window
  mechanism + the un-retried `OsFile::read` short-read gap.
- **Opus** — pinned the exact unlocked structure (`SearchCache::m_cachedFileMap`) and proved a minimal
  deadlock-free leaf-mutex fix.

## Fix applied (2026-07-01, uncommitted)
1. `TreeFile_SearchNode.{h,cpp}` — per-instance `mutable Mutex m_cachedFileMapMutex` serializing ONLY the
   map find/insert in `open`/`exists`/`getFileSize`/`addCachedFile`; disk I/O + `createAbstractFile`
   stay outside the lock (strict leaf, deadlock-free). Entries are never evicted, so the captured
   `CachedFile*` stays valid after unlock.
2. `ZlibFile.cpp:decompress` — FATAL if `expand()` != expected length (fail-loud, self-diagnosing).
3. `stage/client.cfg` — `validateIff=true` (temporary): FATALs at IFF open with filename+CRC if bytes
   are corrupt at open. Remove after the fix is confirmed.

## Verification
- Was "frequent after rebuild" → the rebuild + repeated fresh launches IS the test. If it stops
  recurring, fixed.
- If anything residual: `ZlibFile` FATAL now names the failing decompress; `validateIff` names the file
  at open; Debug build's short-read `DEBUG_FATAL`s are live. Confirming toggle: `suspendAsynchronousLoaderThread`.

Per-consultant outputs: `CONSULT-55-{codex,cursor,sonnet,opus}.out`.
