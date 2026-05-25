# Phase 2: Shared Engine Libraries — Context

**Gathered:** 2026-05-03
**Status:** Ready for planning

<domain>
## Phase Boundary

Author `CMakeLists.txt` files for the 22 shared engine libraries under
`engine/shared/library/*` via 1:1 directory-level adoption from swg-main's
CMake tree. Every library produces a static `.lib` in both Debug and Release.
Three consecutive `cmake --build --config Debug --parallel 8` runs must succeed
deterministically.

**Note:** ROADMAP states "22 libraries" but the success criteria list names 23.
Count from the explicit list in ROADMAP §Phase 2 SC-1 — that list is authoritative.
The planner should confirm the actual count against the whitengold source tree.

**Libraries in scope (from ROADMAP SC-1):**
Tier 2 (leaf): `sharedDebug`, `sharedThread`, `sharedSynchronization`,
`sharedMath`, `sharedMathArchive`
Tier 3 (foundation): `sharedFoundation`, `sharedFile`
Tier 4 (mid): `sharedCompression`, `sharedRandom`, `sharedRegex`, `sharedImage`,
`sharedLog`, `sharedXml`, `sharedIoWin`, `sharedMessageDispatch`, `sharedMemoryManager`
Tier 5 (top): `sharedNetwork`, `sharedNetworkMessages`, `sharedObject`,
`sharedTerrain`, `sharedPathfinding`, `sharedGame`, `sharedUtility`

**Not in scope:** Any `engine/client/` or `game/` libraries — those are Phase 3.
Not in scope: swgServerNetworkMessages or any server-application-level lib.

</domain>

<decisions>
## Implementation Decisions

### swg-main Divergence Policy

- **D-01:** The research phase does a pre-flight diff of whitengold's source
  directory against swg-main's CMakeLists file list for all 22 (23) shared libs
  before any planning starts. The planner receives a complete divergence map and
  never investigates during authoring.
- **D-02:** When whitengold has a `.cpp` file that swg-main's CMakeLists does not
  list — include it. Whitengold's source directory is authoritative. swg-main may
  have dropped files the server no longer needs but the client still uses.
- **D-03:** When swg-main's CMakeLists references a file that does not exist in
  whitengold's source directory — skip it silently. The file was added to swg-main
  post-fork. Note the gap in the divergence map; do not treat it as a blocker.

### Server-Only Source File Exclusion

- **D-04:** For `sharedNetworkMessages` (and any other lib where client/server
  message mixing is known), use the **original VS 2005 `.vcproj` file list** as
  the authoritative source list for the CMakeLists. The `.vcproj` was the working
  client-build file list; it already excludes server-only message types.
- **D-05:** For all other shared libs (the remaining ~21), whitengold's source
  directory is authoritative per D-02. No `.vcproj` extraction needed.
- **D-06:** Research phase locates the relevant `.vcproj` files for the network
  message libs and extracts their source file lists before planning starts.

### D-04/D-06 Resolution (added 2026-05-03)

Research confirmed that **no `.vcproj` file exists** for `sharedNetworkMessages` anywhere
in the whitengold source tree. `find` over `src/build/win32/` and
`src/engine/shared/library/sharedNetworkMessages/` returned empty.

**Explicit override of D-04:** Since the VS 2005 `.vcproj` does not exist, D-04 cannot be
honored. Fall back to D-02: whitengold's source directory is authoritative. Include ALL
338 `.cpp` files across the 9 subdirectories
(`src/shared/`, `shared/chat/`, `shared/clientGameServer/`, `shared/clientLoginServer/`,
`shared/common/`, `shared/core/`, `shared/customerService/`, `shared/planetWatch/`,
`shared/voicechat/`). swg-main's CMakeLists matches this full inventory, providing
additional confidence that the full directory is correct.

**Accepted risk:** `planetWatch/` and `customerService/` subdirectories contain
server-admin message types that SwgClient may never exercise at runtime. They add ~37
.cpp files and compile time but cause no functional problem.

### time_t Probe Mechanism

- **D-07:** The `static_assert(sizeof(time_t) == 4, "_USE_32BIT_TIME_T=1 not
  reaching this TU")` probe is implemented via CMake `configure_file`. A single
  template `src/cmake/stubs/time_t_probe.cpp.in` is configured per target into
  `${CMAKE_CURRENT_BINARY_DIR}/time_t_probe.cpp` and added to that target's
  source list. No edits to existing `.cpp` files; no new files in `src/engine/`.
- **D-08:** The probe is placed in one representative lib per build tier:
  - `sharedDebug` (Tier 2 leaf — verifies root CMake propagation to first lib)
  - `sharedFoundation` (Tier 3 — verifies propagation through foundation dependencies)
  - `sharedNetwork` (Tier 5 top — verifies propagation through full transitive closure)
  Three probes total. All other libs are verified implicitly by the tier coverage.
- **D-09:** The probe template file lives at `src/cmake/stubs/time_t_probe.cpp.in`
  (same `src/cmake/stubs/` directory used for the Phase 3 XPCOM stub — consistent
  location for all CMake-generated C++ sources).

### Precompiled Headers (carried from Phase 1 / Phase 3 pattern)

- **D-10:** Every shared lib wires `First<LibName>.h` via
  `target_precompile_headers(${LIB} PRIVATE First${LibName}.h)`. This is the
  established pattern; no decision needed — noting it explicitly so the planner
  does not omit it.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase Goals and Success Criteria
- `.planning/ROADMAP.md` §Phase 2 — authoritative 5-point success criteria,
  tier breakdown, and deterministic-build requirement
- `.planning/PROJECT.md` §Key Decisions — locked Phase 1 decisions that carry
  forward (STLPort staged layout, global compile flags, CRT policy)

### Locked Phase 1 Outputs (must exist before Phase 2 research starts)
- `src/CMakeLists.txt` — root CMake entry with all global flags
- `src/cmake/win32/FindSTLPort.cmake` — STLPort header resolution; every
  Phase 2 lib depends on this being correct
- `src/cmake/win32/FindBoost.cmake` — Boost header-only INTERFACE target
- `src/cmake/win32/FindLibXml2.cmake` — libxml2 vendored resolution
  (`sharedXml` depends on this)
- `src/cmake/win32/FindPCRE.cmake` — PCRE vendored resolution
  (`sharedRegex` depends on this)
- `src/cmake/win32/FindZlib.cmake` — zlib vendored resolution
  (`sharedCompression` depends on this)

### swg-main Reference (build pattern source — NOT client CMakeLists)
- `C:\Code\swg-main` — reference CMake structure for shared engine libs. The
  researcher reads swg-main's CMakeLists for each shared lib to extract: target
  name, `target_link_libraries` deps, and any special compile definitions. The
  planner uses this as a template, substituting whitengold's actual source files.

### Source File Authority
- `src/engine/shared/library/<libName>/src/` — authoritative source file list
  for each shared lib (per D-02)
- Original VS 2005 `.vcproj` files for `sharedNetworkMessages` — authoritative
  source file list for the network message tier (per D-04). Researcher must
  locate and extract these before planning.

### Probe Template (to be created in Phase 2)
- `src/cmake/stubs/time_t_probe.cpp.in` — template for the `time_t` size probe
  (per D-07). This file does not exist yet; the planner creates it.

### Research Documents
- `docs/research/swgclient-build.md` — dependency graph, flag inventory; use
  to verify that `sharedNetwork` tier deps are correct in the CMakeLists
- `.planning/phases/01-cmake-skeleton-foundations-spike-and-lock/01-PLAN.md`
  §Execution Results — verified Phase 1 baseline the Phase 2 graph builds upon

</canonical_refs>

<code_context>
## Existing Code Insights

### Phase 1 Outputs Already In Tree
- `src/external/ours/library/*/CMakeLists.txt` — the 8 `ours/` libs are built
  and verified. Phase 2 shared libs link against some of these (e.g., sharedFoundation
  links archive, singleton; sharedFile links fileInterface).
- `src/external/3rd/library/udplibrary/CMakeLists.txt` — built and verified.
  `sharedNetwork` links against udplibrary.
- `src/engine/shared/library/sharedFoundationTypes/CMakeLists.txt` — built and
  verified. Most Phase 2 libs depend on this as a leaf.
- `src/cmake/stubs/` — directory will exist after Phase 2 creates
  `time_t_probe.cpp.in` (and already scoped for Phase 3's libMozilla stub).

### Three-Level Nesting Pattern (Phase 1 establishes this)
- `<lib>/CMakeLists.txt` → `add_subdirectory(src)` only
- `<lib>/src/CMakeLists.txt` → all source enumeration, `add_library`, deps, PCH
- Mirror this for all Phase 2 libs

### Integration Points
- Phase 2 `.lib` outputs are direct dependencies of Phase 3 client engine libs.
  All 22 (23) must build cleanly before Phase 3 can begin.
- `sharedGame` and `sharedObject` are among the most-depended-on libs in the
  client graph — errors here will cascade to Phase 3 and Phase 4.

</code_context>

<specifics>
## Specific Requirements

- The researcher must complete the swg-main vs whitengold source diff (D-01) and
  the `.vcproj` extraction for sharedNetworkMessages (D-06) before the planner
  starts — these are research-phase prerequisites, not planner-phase tasks.
- `src/cmake/stubs/time_t_probe.cpp.in` is created by the planner (or researcher)
  once; the same template file is reused for all three probe targets.
- swg-main's CMakeLists provide the dep graph structure (target_link_libraries);
  whitengold's source dirs provide the actual source file lists. These two inputs
  are combined per-lib during planning.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 2-Shared Engine Libraries*
*Context gathered: 2026-05-03*
