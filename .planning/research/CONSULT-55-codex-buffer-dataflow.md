# CONSULT-55 (Codex) — Producer→consumer data-flow of the cached .sht byte buffer

Read `.planning/research/CONSULT-55-EVIDENCE-BRIEF.md` first (facts are GIVEN). Repo: `D:\Code\swg-client-v2`.

## Your angle (repo tracer / call-graph — LIFETIME of the actual bytes):
Trace the PHYSICAL byte buffer that `Iff` parses when a `.sht` is fetched during an async-load callback,
end to end, with file:line:
1. `request->cachedFiles` → `CachedFile.file` (what concrete `AbstractFile`/`MemoryFile` subclass; where
   is its data buffer allocated; is it zlib-compressed and decompressed where/into what).
2. `TreeFile::addCachedFile` → `cachedFilesMap` → `TreeFile::getCachedFile` → what `Iff::open(name, true)`
   does with the returned `AbstractFile*` (does Iff copy the bytes, or hold a pointer into the cached
   file's buffer?). What is the SECOND arg (`true`) to `Iff::open` — optional? cached? Trace it.
3. Every place that FREES or REUSES that buffer: `TreeFile::clearCachedFiles`, the AsynchronousLoader
   second cleanup loop (:638+), `MemoryFile`/`AbstractFile` dtors, any shared decompression scratch.
4. Answer plainly: between `getCachedFile` returning the buffer and `create()` reading `getCurrentName()`,
   is there ANY path (cross-thread OR re-entrant on the main thread) that can free/overwrite those bytes?
   Where would the fix live (ownership/refcount/lock-scope)?

Output = the buffer lifetime trace with file:line, and the specific free/overwrite window if one exists.
