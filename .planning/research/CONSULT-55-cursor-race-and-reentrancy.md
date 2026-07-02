# CONSULT-55 (Cursor) — Exact race / re-entrancy in the async-load + TreeFile-cache path

Read `.planning/research/CONSULT-55-EVIDENCE-BRIEF.md` first (facts are GIVEN). Repo: `D:\Code\swg-client-v2`.

## Your angle (most detailed code reader — pinpoint the defect, file:line):
Read closely and report the exact bug:
- `AsynchronousLoader.cpp`: `threadRoutine()` (~:429) — what the BACKGROUND thread does to `request`,
  `cachedFiles`, and any shared state while the MAIN thread is in the completed-request block (:600-660).
- The completed-request block (:600-660): the add→callback→clearCachedFiles sequence. Is
  `TreeFile::clearCachedFiles()` able to delete an `AbstractFile` that the callback's IN-PROGRESS parse
  still references? Can the callback itself (nested asset loads → nested completed-request pumping, or a
  nested async load) trigger `clearCachedFiles`/eviction MID-PARSE (re-entrancy)?
- `TreeFile.cpp` cache: `addCachedFile` (:935), `getCachedFile` (:669-684), `clearCachedFiles`,
  `CachedFilesComparator` / the `const char*` KEY ownership — who owns the key string, and can it be freed
  while still in the map?
- The `getCachedFile` at :669 releases the lock between the `ms_haveCachedFiles` check and the map find,
  and returns a raw `AbstractFile*` after leaving the lock — assess whether that returned pointer can be
  invalidated before the caller finishes reading it.

Deliverable: the single most likely concrete defect (race OR lifetime OR re-entrancy) with file:line, and
whether the `suspendAsynchronousLoaderThread` debug flag would make it disappear (a confirming test).
