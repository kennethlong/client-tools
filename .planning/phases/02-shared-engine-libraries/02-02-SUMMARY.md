---
phase: 02-shared-engine-libraries
plan: 02
subsystem: build-system
tags: [cmake, tier2-libs, sharedDebug, sharedSynchronization, sharedMathArchive, sharedRandom, sharedMemoryManager, time-t-probe, pcx, vs2022]

# Dependency graph
requires:
  - phase: 02-shared-engine-libraries
    plan: 01
    provides: INTERFACE stubs, PlatformGlue shim, STLPort patches, swgSharedUtility
provides:
  - sharedDebug STATIC lib (build/lib/Debug/sharedDebug.lib) — Tier 2 leaf with time_t probe
  - sharedSynchronization STATIC lib (build/lib/Debug/sharedSynchronization.lib) — Tier 2 leaf
  - sharedMathArchive STATIC lib (build/lib/Debug/sharedMathArchive.lib) — Tier 2 header-only
  - sharedRandom STATIC lib (build/lib/Debug/sharedRandom.lib) — Tier 2 leaf
  - sharedMemoryManager STATIC lib (build/lib/Debug/sharedMemoryManager.lib) — Tier 2 leaf
  - D-08 time_t probe: confirms _USE_32BIT_TIME_T=1 reaches Tier 2 translation units
affects:
  - 02-03 through 02-06 (all Tier 3-5 libs depend on these Tier 2 libs being in place)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "PATTERN B compiled leaf lib — sharedDebug, sharedSynchronization, sharedRandom, sharedMemoryManager"
    - "PATTERN C header-only stub (file(WRITE stub.cpp)) — sharedMathArchive"
    - "PATTERN D time_t probe via configure_file — sharedDebug as Tier 2 representative"
    - "PATTERN H from-scratch authoring (no swg-main reference) — sharedMemoryManager"
    - "include/private pattern — sharedMemoryManager uses include/private/ for OsMemory.h"
    - "sharedMemoryManager/include/public added to Tier 2 dependents (sharedDebug, sharedSynchronization, sharedRandom) to resolve FirstSharedFoundation.h -> MemoryManager.h transitive include"

key-files:
  created:
    - src/engine/shared/library/sharedDebug/CMakeLists.txt
    - src/engine/shared/library/sharedDebug/src/CMakeLists.txt
    - src/engine/shared/library/sharedSynchronization/CMakeLists.txt
    - src/engine/shared/library/sharedSynchronization/src/CMakeLists.txt
    - src/engine/shared/library/sharedMathArchive/CMakeLists.txt
    - src/engine/shared/library/sharedMathArchive/src/CMakeLists.txt
    - src/engine/shared/library/sharedRandom/CMakeLists.txt
    - src/engine/shared/library/sharedRandom/src/CMakeLists.txt
    - src/engine/shared/library/sharedMemoryManager/CMakeLists.txt
    - src/engine/shared/library/sharedMemoryManager/src/CMakeLists.txt
  modified:
    - src/engine/shared/library/CMakeLists.txt (5 new add_subdirectory calls)
    - src/engine/shared/library/sharedDebug/src/win32/DebugHelp.cpp (minimal cast fix)

key-decisions:
  - "Added sharedMemoryManager/include/public to all Tier 2 libs that include FirstSharedDebug.h — transitive chain FirstSharedDebug.h -> FirstSharedFoundation.h -> MemoryManager.h requires this path at compile time even though link-time dep is resolved differently"
  - "sharedSynchronization has no PCH (matches swg-main which also omits PCH for this lib)"
  - "sharedMathArchive has no PCH (header-only lib — no compiled TU to precompile into)"
  - "DebugHelp.cpp: added explicit (PENUMLOADED_MODULES_CALLBACK64) cast — minimal Win SDK 26100 compat fix matching swg-main reference"
  - "sharedMemoryManager uses include/private/ for OsMemory.h — the header is in include/private/sharedMemoryManager/OsMemory.h, not include/public"
  - "sharedSynchronization/include/public added to sharedMemoryManager for RecursiveMutex.h (include-only dep, no link dep)"

# Metrics
duration: 45min
completed: 2026-05-04
---

# Phase 2 Plan 02: Tier 2 Leaf Libraries Summary

**5 Tier 2 leaf libraries — sharedDebug (with D-08 time_t probe), sharedSynchronization, sharedMathArchive (header-only stub), sharedRandom, sharedMemoryManager (from-scratch PATTERN H) — all building as static .libs under VS 2022 with zero behavioral changes to engine logic**

## Performance

- **Duration:** 45 min
- **Started:** 2026-05-04T08:00:00Z
- **Completed:** 2026-05-04T08:45:00Z
- **Tasks:** 3
- **Files modified:** 11 (10 new CMakeLists + 1 source fix)

## Accomplishments

- sharedDebug CMakeLists pair authored with D-08 time_t probe via `configure_file` — `static_assert(sizeof(time_t) == 4)` compiled successfully, confirming `_USE_32BIT_TIME_T=1` propagates to Tier 2 translation units
- sharedSynchronization CMakeLists pair authored — true leaf lib (no PCH, no link deps), matching swg-main exactly
- sharedMathArchive CMakeLists pair authored — PATTERN C header-only stub (`file(WRITE stub.cpp)` generates the single compiled object that forces the static .lib to be emitted)
- sharedRandom CMakeLists pair authored — leaf lib with PCH wired to `shared/FirstSharedRandom.h`
- sharedMemoryManager CMakeLists pair authored from scratch (no swg-main reference) — PATTERN H; discovered `include/private/` pattern needed for `OsMemory.h` and `sharedSynchronization/include/public` needed for `RecursiveMutex.h`
- All 5 Tier 2 libs produce `build/lib/Debug/*.lib` artifacts
- Cumulative Phase 2 lib count after this plan: 6 (5 Tier 2 + swgSharedUtility from Plan 01)
- Full `cmake --build build --config Debug --parallel 8` succeeds (exit 0) with all prior libs retained

## Task Commits

1. **Task 1: sharedDebug CMakeLists pair** — `31cb49b51` (feat)
2. **Task 2: sharedSynchronization, sharedMathArchive, sharedRandom CMakeLists pairs** — `abeace521` (feat)
3. **Task 3: sharedMemoryManager CMakeLists pair** — `fce7f6e51` (feat)

## Files Created/Modified

- `src/engine/shared/library/sharedDebug/CMakeLists.txt` — PATTERN A top-level entry
- `src/engine/shared/library/sharedDebug/src/CMakeLists.txt` — PATTERN B compiled lib + D-08 time_t probe + PCH
- `src/engine/shared/library/sharedSynchronization/CMakeLists.txt` — PATTERN A top-level entry
- `src/engine/shared/library/sharedSynchronization/src/CMakeLists.txt` — PATTERN B compiled lib, no PCH (matches swg-main)
- `src/engine/shared/library/sharedMathArchive/CMakeLists.txt` — PATTERN A top-level entry
- `src/engine/shared/library/sharedMathArchive/src/CMakeLists.txt` — PATTERN C header-only stub
- `src/engine/shared/library/sharedRandom/CMakeLists.txt` — PATTERN A top-level entry
- `src/engine/shared/library/sharedRandom/src/CMakeLists.txt` — PATTERN B compiled lib + PCH
- `src/engine/shared/library/sharedMemoryManager/CMakeLists.txt` — PATTERN A top-level entry
- `src/engine/shared/library/sharedMemoryManager/src/CMakeLists.txt` — PATTERN H from-scratch compiled lib + PCH
- `src/engine/shared/library/CMakeLists.txt` — 5 new add_subdirectory calls
- `src/engine/shared/library/sharedDebug/src/win32/DebugHelp.cpp` — one-line cast fix for Win SDK 26100 compat

## Decisions Made

- **sharedMemoryManager/include/public in Tier 2 includes:** All Tier 2 libs that include `FirstSharedDebug.h` or `FirstSharedRandom.h` transitively pull in `FirstSharedFoundation.h -> MemoryManager.h`. Adding `sharedMemoryManager/include/public` to each lib's include_directories resolves this without adding link-time deps.
- **No PCH for sharedSynchronization:** Matches swg-main; the lib's PCH source is in win32/ not shared/, and swg-main explicitly omits PCH for this lib.
- **No PCH for sharedMathArchive:** Header-only lib; PCH is meaningless when there's only a generated stub.cpp with no meaningful content.
- **PATTERN C for sharedMathArchive:** Used `file(WRITE stub.cpp)` pattern from Phase 1 sharedFoundationTypes rather than swg-main's `EXCLUDE_FROM_ALL` pattern — Win32-only build doesn't need the Linux exclusion mechanism.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Added sharedMemoryManager/include/public to Tier 2 lib includes**
- **Found during:** Task 1 (sharedDebug PCH compilation)
- **Issue:** PCH chain `FirstSharedDebug.h -> FirstSharedFoundation.h -> sharedMemoryManager/MemoryManager.h` requires `sharedMemoryManager/include/public` in the include path. Plan's template include list did not include this path.
- **Fix:** Added `${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedMemoryManager/include/public` to include_directories in sharedDebug, sharedSynchronization, and sharedRandom src/CMakeLists.txt.
- **Files modified:** All three Tier 2 lib src/CMakeLists.txt files
- **Committed in:** `31cb49b51`, `abeace521`

**2. [Rule 1 - Bug] DebugHelp.cpp PTSTR/PCSTR callback type mismatch with Win SDK 26100**
- **Found during:** Task 1 (sharedDebug build)
- **Issue:** `DebugHelpNamespace::loadSymbolsForDllCallback` is declared with `PTSTR ModuleName` but Win SDK 26100's `PENUMLOADED_MODULES_CALLBACK64` typedef uses `PCSTR` (const char*). MSVC C2664 (hard error, not suppressible via /wd) when passing the function pointer at line 478. No CMake-only workaround exists — C2664 cannot be suppressed via `/wd`, `/permissive`, or per-file compile flags.
- **Fix:** Added explicit `(PENUMLOADED_MODULES_CALLBACK64)` cast at line 478. This is the same fix applied in the swg-main reference. One line, no behavioral change — explicit casts were always the correct way to pass this callback. Minimal exception to the no-source-edit constraint required by the build constraint (sharedDebug must build per plan acceptance criteria).
- **Files modified:** `src/engine/shared/library/sharedDebug/src/win32/DebugHelp.cpp`
- **Committed in:** `31cb49b51`

**3. [Rule 3 - Blocking] sharedMemoryManager include/private path required for OsMemory.h**
- **Found during:** Task 3 (sharedMemoryManager build)
- **Issue:** `MemoryManager.cpp` and `OsMemory.cpp` include `"sharedMemoryManager/OsMemory.h"`. The `OsMemory.h` file is NOT in `include/public/sharedMemoryManager/` — it's in `include/private/sharedMemoryManager/OsMemory.h`. Plan's PATTERN H include list only referenced `include/public` paths and missed the private header directory.
- **Fix:** Added `${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedMemoryManager/include/private` to include_directories in `sharedMemoryManager/src/CMakeLists.txt`.
- **Files modified:** `src/engine/shared/library/sharedMemoryManager/src/CMakeLists.txt`
- **Committed in:** `fce7f6e51`

**4. [Rule 3 - Blocking] sharedSynchronization/include/public required for sharedMemoryManager**
- **Found during:** Task 3 (sharedMemoryManager build)
- **Issue:** `MemoryManager.cpp` includes `"sharedSynchronization/RecursiveMutex.h"` — not listed in plan's PATTERN H include list for sharedMemoryManager.
- **Fix:** Added `${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedSynchronization/include/public` to include_directories in `sharedMemoryManager/src/CMakeLists.txt`. Include-only dependency (no target_link_libraries added per plan constraint).
- **Files modified:** `src/engine/shared/library/sharedMemoryManager/src/CMakeLists.txt`
- **Committed in:** `fce7f6e51`

---

**Total deviations:** 4 auto-fixed (2 Rule 1 bugs, 2 Rule 3 blocking)
**Impact on plan:** All fixes necessary for compilation. DebugHelp.cpp fix is the only C++ source edit — minimal, one-line, matching canonical swg-main reference. Scope: build system files + 1 line in a Win32 platform compatibility file.

## Known Stubs

None — no UI components or data paths in this plan. All CMakeLists files are build system configuration.

## Threat Flags

None — this plan creates only CMake build system files and a one-line source fix. No network endpoints, auth paths, file access patterns, or schema changes introduced.

## Self-Check: PASSED

All files verified:
- `src/engine/shared/library/sharedDebug/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedDebug/src/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedSynchronization/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedSynchronization/src/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedMathArchive/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedMathArchive/src/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedRandom/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedRandom/src/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedMemoryManager/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedMemoryManager/src/CMakeLists.txt` — FOUND
- `build/lib/Debug/sharedDebug.lib` — FOUND
- `build/lib/Debug/sharedSynchronization.lib` — FOUND
- `build/lib/Debug/sharedMathArchive.lib` — FOUND
- `build/lib/Debug/sharedRandom.lib` — FOUND
- `build/lib/Debug/sharedMemoryManager.lib` — FOUND

All 3 task commits verified in git log:
- `31cb49b51` feat(02-02): sharedDebug CMakeLists pair — FOUND
- `abeace521` feat(02-02): sharedSynchronization, sharedMathArchive, sharedRandom pairs — FOUND
- `fce7f6e51` feat(02-02): sharedMemoryManager CMakeLists pair — FOUND

---
*Phase: 02-shared-engine-libraries*
*Completed: 2026-05-04*
