# CONSULT-55 — Intermittent "Unknown shader template tag" async-load heap/buffer corruption

## Treat as GIVEN (verified; do not re-derive)

Repo: `D:\Code\swg-client-v2` (Win32/x64 SWG client, MSBuild). This is a **pre-existing intermittent**,
NOT caused by recent camera work (that's in FreeChaseCamera + Direct3d11 VB — different subsystems,
already ruled out).

### The crash
- FATAL at `ShaderTemplateList::create` (`clientGraphics/.../ShaderTemplateList.cpp:567`):
  `Unknown shader template tag "??]'" for shader/spaceport_luma_lightstrip3_cs8_tato.sht`.
  The garbage value is `targetTag = iff.getCurrentName()` (line 546) — the current IFF tag read back
  garbage. Registered tags (CSHD/OPST/SSHT/SWSH/SWTS) are all present in the map (printed in the dump),
  so the creation-function map is intact.
- **The `.sht` is VALID on disk** (`stage/ilm_extract/shader/spaceport_luma_lightstrip3_cs8_tato.sht`
  begins `FORM....SSHT FORM 0000 ... MATS ...`), and loads fine in many other sessions. So the tag was
  corrupt **at runtime** — a bad/overwritten/freed buffer being parsed, NOT a bad asset.
- **Intermittent; "frequently manifests after a rebuild"** (user report). Cold disk / shader-bytecode
  caches after a rebuild change async-load latency → strongly implies a **timing-dependent race or
  lifetime bug**, not deterministic logic.

### The stack (main thread)
`AsynchronousLoader.cpp:631` (completed-request processing, called from Game.cpp:1133/1052 main update)
→ `request->callback` → `MeshAppearanceTemplate.cpp:233/274/402` → `ShaderPrimitiveSetTemplate.cpp:1435/1455/1471/1551`
→ `ShaderTemplateList::fetch(iff)` [263] → `fetch(iff,error)` [251] → `fetch(crcString,iff,error)` [493]
→ **named-template redirect** [506, `iff.getCurrentName()==TAG_NAME`] → `fetch(TemporaryCrcString(filename,true))` [201→230]
→ `iff.open(name.getString(), true)` [223] → `create(name, iff)` [513] → garbage tag [546→567].

### What is already thread-safe (so NOT the bug)
- `ShaderTemplateList`: map + `create()` run under `ms_criticalSection` (a **RecursiveMutex**); public
  `enter/leaveCriticalSection` hooks exist. Map is guarded.
- `TreeFile` cache (`cachedFilesMap : map<const char*, AbstractFile*, CachedFilesComparator>`) is guarded
  by `TreeFile::ms_criticalSection` (Mutex): `addCachedFile` (:935), `getCachedFile` (:669), etc. all lock.
- `AsynchronousLoader::ms_fileData` is the **preload MANIFEST** buffer (FileRecords), read once at
  install — NOT per-file content. Not the corruption source.

### The async-load model (the suspect arena)
- Background thread: `runNamedThread("AsynchronousLoader", threadRoutine)` (AsynchronousLoader.cpp:210,
  threadRoutine at :429). Guarded by `ms_mutex`; produces `request->cachedFiles` (a `CachedFiles` list of
  `CachedFile{ fileRecord, AbstractFile* file }`, some zlib-compressed).
- Main thread completed-request processing (:600-660): for each completed request —
  1. `TreeFile::addCachedFile(cachedFile.fileRecord->fileName, cachedFile.file); cachedFile.file = NULL;`
     (publish preloaded files into the TreeFile cache),
  2. `(*request->callback)(request->data)` (the actual load — opens the assets via `Iff::open`, which
     reads from the TreeFile cache),
  3. `TreeFile::clearCachedFiles()` (pitch ALL cached files),
  4. then a second loop frees resources the request still owns.

### Leading suspicion (do NOT treat as proven — verify or refute)
The byte buffer the `Iff` parses for a cached `.sht` is **freed / recycled / decompressed-in-place**
out from under the main-thread parse — via cross-thread interplay with the loader thread, OR via
**re-entrancy** (the callback's nested loads trigger another `clearCachedFiles`/eviction mid-parse),
OR a `CachedFile`/`AbstractFile` lifetime bug, OR the cache-key `const char*` lifetime. The
named-template redirect (`Iff::open(name, true)`) buffer source is central.

Goal: find the ROOT cause and a minimal correct fix (no deadlock against the two mutexes; no perf
regression to the async loader). A resilience retry is a fallback, not the goal.
