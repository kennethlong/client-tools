# CONSULT-57 EVIDENCE — three concurrency changes under review (2026-07-03)

Neutral facts. Treat as given; do not re-derive.

## Codebase / threads
- 2003-era C++ MMO client (SWG), MSVC v145, Win32/x64. Repo root = this directory.
- Thread inventory: MAIN thread (render + alter loops), ClientTerrain WORKER thread
  (`ClientProceduralTerrainAppearance` chunk creation via `threadRoutine`), AsynchronousLoader
  WORKER thread (asset loads), Miles AUDIO IO thread (file callbacks). All four call
  `TreeFile::open`. The terrain worker creates chunks; chunk DELETION is main-thread-only.
- `Mutex` (sharedSynchronization) wraps a Win32 CRITICAL_SECTION → recursive. `RecursiveMutex`
  likewise. `Guard` is RAII enter/leave.
- Prior landed fixes in this defect family (do not re-litigate): `9c03f53c5`
  (TreeFile::SearchCache map leaf mutex), `f344d1035` (terrain ShaderCache nodeList
  RecursiveMutex; Shader::m_users interlocked; CrashReportInformation no-realloc rewrite).

## The diff under review
`.planning/research/CONSULT-57-threading-audit-fixes.diff` (395 lines, 11 files). Three
independent changes:

### Change 1 — TreeFile search-node traversals (sharedFile)
`TreeFile::ms_searchNodes` is a priority-sorted `std::vector<SearchNode*>`. All mutators
(`addSearchNode`/`remove`/`removeAllSearches`) already hold the static `ms_criticalSection`.
`addSearchTree` is reachable at RUNTIME via an advertised engine hookpoint. Before the diff,
production read paths traversed the vector with no lock. The diff adds
`TreeFile::copySearchNodes(SearchNode **snapshot, int maxNodes)` (copies the pointer array under
`ms_criticalSection`, capacity 64, WARNING on truncation) and converts six readers
(`find`, `getFileSize`, `open`, `getNumberOfSearchPaths`, `getSearchPath`,
`stripTreeFileSearchPathFromFile`) to iterate a stack snapshot; per-node blocking I/O
(`SearchNode::open` etc.) runs outside the lock. Nodes are deleted only at shutdown
(ExitChain `remove`, after workers join). `debugReportPaths`/`enumerateFiles` already locked.

### Change 2 — Texture refcount + two pass-object fetches (clientGraphics)
Before: `Texture::fetch/release` were bare `++/--` on `mutable int m_referenceCount` with
`delete this` + `TextureList::removeFromList(this)` at zero; `TextureList::create`
(fetch-by-name) does map find + `texture->fetch()` under `TextureList`'s file-local
`ms_criticalSection`. The terrain worker's `StaticShader` copy-ctor/dtor fetch/release textures
(`StaticShader.cpp:190/229`) concurrently with main-thread fetch/release. Peer classes
(`ShaderEffect`, `ShaderImplementation`, `ShaderTemplate`, `Video`) serialize BOTH fetch and
release under their list's global mutex.
The diff: `TextureList` exposes `enterCriticalSection/leaveCriticalSection` (the
ShaderEffectList idiom); `Texture::fetch` and `Texture::release` now wrap their count mutation
(and, for release, `removeFromList` + `delete this`) in that CS. `m_referenceCount` stays
`mutable int` (layout unchanged). Also: `ShaderImplementationPassVertexShader::fetch` and
`ShaderImplementationPassPixelShaderProgram::fetch` were bare `++` while their `release` was
mutex-serialized; the diff locks the fetches on the same per-class mutex.

### Change 3 — terrain blended-shader cache eviction wiring (clientTerrain)
`ShaderCache` (`ClientProceduralTerrainAppearance_ShaderCache.{h,cpp}`): nodeList of
`BlendedShaderCacheNode{shader, referenceCount, timeout, texture keys}` guarded by
`m_nodeListLock` (RecursiveMutex, from f344d1035). `createBlendedShader` (find-or-create under
the lock) ++es `referenceCount` per HIT and initializes it to 1 per CREATE; it is called once
per terrain TILE from `ClientChunk::createTileShader`. Each such call is followed by exactly one
`ShaderSet::addPrimitive` in `ClientChunk::create`'s tile loop; tiles sharing a shader collapse
into one `ShaderSet` per shader (`m_shaderSetList`). `alter()` (main thread) evicts a node only
when `referenceCount == 0` and it has idled > 5 s (releases the node's cloned StaticShader).
`destroyShader(const Shader*)` (--referenceCount for the matching node) had ZERO callers since
the original SOE import, so counts never fell and eviction never fired (retail-era slow leak;
full reclaim only at whole-appearance teardown via ~ShaderCache).
The diff: `ShaderSet::getNumberOfPrimitives()` accessor; `ClientChunk::~ClientChunk`
(main-thread-only; the single funnel for all chunk deletion) calls
`shaderCache->destroyShader(set->getShader())` once per primitive per ShaderSet (null-guarded);
`destroyShader` gains a guard: `DEBUG_FATAL` on decrement-at-<=0, and in Release refuses the
decrement (leaves the node pinned) with a WARNING.

## Build/ABI constraints
- No public struct layout changed anywhere in the diff (verify).
- SwgClient links with /FORCE → unresolved externals downgrade to warnings; the gate is
  grep-zero on "unresolved external symbol".
