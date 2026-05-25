---
phase: 02-shared-engine-libraries
plan: 01
subsystem: infra
tags: [cmake, stlport, msvc, windows-sdk, build-system, swgSharedUtility, compatibility]

# Dependency graph
requires:
  - phase: 01-cmake-skeleton-foundations-spike-and-lock
    provides: root CMakeLists.txt, FindSTLPort.cmake, Phase 1 static libs baseline
provides:
  - src/cmake/stubs/time_t_probe.cpp.in (time_t probe template for D-08 targets)
  - INTERFACE stubs: sharedCollision, sharedFractal, sharedSkillSystem in engine/shared/library/CMakeLists.txt
  - src/game/ subtree wiring: root → game → shared → library → swgSharedUtility
  - swgSharedUtility STATIC lib (Debug + Release verified)
  - src/cmake/compat/sharedFoundation/PlatformGlue.h (VS 2022 / Win SDK 26100+ snprintf fix)
  - Updated FindSTLPort.cmake with staged-stdio redirect and _threads.h patch
affects:
  - 02-02 through 02-06 (all depend on INTERFACE stubs and compat shim being in place)
  - Phase 3 (client engine libs depend on swgSharedUtility include paths)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "INTERFACE stub targets for out-of-scope Phase 2 libs (sharedCollision, sharedFractal, sharedSkillSystem)"
    - "CMake compat shim directory (src/cmake/compat/) prepended to include path for source-edit-free VS 2022 compatibility"
    - "PlatformGlue.h extern-C wrapper shim prevents MSVC C2732 on UCRT snprintf redeclaration"
    - "STLPort staged stdio.h redirect prevents duplicate physical-file inclusion of UCRT headers"

key-files:
  created:
    - src/cmake/stubs/time_t_probe.cpp.in
    - src/cmake/compat/sharedFoundation/PlatformGlue.h
    - src/game/CMakeLists.txt
    - src/game/shared/CMakeLists.txt
    - src/game/shared/library/CMakeLists.txt
    - src/game/shared/library/swgSharedUtility/CMakeLists.txt
    - src/game/shared/library/swgSharedUtility/src/CMakeLists.txt
  modified:
    - src/CMakeLists.txt (add_subdirectory(game), prepend compat include)
    - src/engine/shared/library/CMakeLists.txt (INTERFACE stub targets)
    - src/cmake/win32/FindSTLPort.cmake (stdio.h redirect + _threads.h patch)

key-decisions:
  - "PlatformGlue.h shim in src/cmake/compat/ intercepts include without modifying source — no C++ source edits required"
  - "STLPort staged stdio.h replaced with redirect to real UCRT path to prevent C2732 from duplicate physical file inclusion"
  - "_threads.h patched with _WINDOWS_ guard so Interlocked* redeclaration is suppressed when windows.h was included first"
  - "swgSharedUtility PCH removed (matches swg-main pattern) — PCH chain transitively depends on sharedMemoryManager which is not yet built in Plan 02-01"
  - "sharedGame/include/public added to swgSharedUtility includes for CombatEngineData.h → AttribMod.h dependency"
  - "sharedMath/include/public added to swgSharedUtility includes for CachedNetworkId.h → Object.h → Transform.h chain"

patterns-established:
  - "INTERFACE stubs: use add_library(X INTERFACE) + target_include_directories for out-of-scope libs"
  - "compat shim: create src/cmake/compat/<lib>/<header>.h to fix include-time issues without source edits"
  - "STLPort stdio redirect: staged include/ stdio.h uses #include \"<ucrt-path>/stdio.h\" not a copy"

requirements-completed: [BUILD-03]

# Metrics
duration: 33min
completed: 2026-05-04
---

# Phase 2 Plan 01: Wave-0 Prerequisites and swgSharedUtility Summary

**time_t probe template, three INTERFACE stubs (sharedCollision/Fractal/SkillSystem), game/ subtree wiring, and swgSharedUtility STATIC lib building Debug+Release on VS 2022 with a source-edit-free PlatformGlue.h shim for UCRT snprintf C2732**

## Performance

- **Duration:** 33 min
- **Started:** 2026-05-04T12:26:52Z
- **Completed:** 2026-05-04T13:00:00Z
- **Tasks:** 4
- **Files modified:** 9

## Accomplishments

- time_t probe template created at `src/cmake/stubs/time_t_probe.cpp.in` with `static_assert(sizeof(time_t) == 4)` — ready for D-08 configure_file usage by Plans 02-03, 05
- Three INTERFACE stub targets (sharedCollision, sharedFractal, sharedSkillSystem) added to `engine/shared/library/CMakeLists.txt` — satisfy configure-time target validation for Tier 5 libs without pulling 60+ .cpp files into Phase 2 scope
- game/ subtree wired root → game → shared → library → swgSharedUtility — prerequisite for Plans 02-06 that reference swgSharedUtility include paths
- swgSharedUtility builds as static .lib in both Debug (`build/lib/Debug/swgSharedUtility.lib`) and Release (`build/lib/Release/swgSharedUtility.lib`)
- Root cause of VS 2022 / Win SDK 26100 `C2732 snprintf` linkage conflict identified: `PlatformGlue.h` declares `snprintf` with C++ linkage; fixed via `src/cmake/compat/sharedFoundation/PlatformGlue.h` shim (no C++ source edits)
- STLPort staged `stdio.h` redirect eliminates duplicate physical-file inclusion that was causing a separate instance of C2732

## Task Commits

1. **Task 1: time_t probe template** - `b254e30e9` (chore)
2. **Task 2: INTERFACE stubs** - `634c626fb` (chore)
3. **Task 3: game/ subtree wiring** - `5b3380e6e` (chore)
4. **Task 4: swgSharedUtility CMakeLists pair** - `dfbffeb1d` (feat)

## Files Created/Modified

- `src/cmake/stubs/time_t_probe.cpp.in` — CMake-only time_t size probe template (D-08 target)
- `src/cmake/compat/sharedFoundation/PlatformGlue.h` — extern-C shim wrapping MSVC 7.1-era snprintf declaration
- `src/cmake/win32/FindSTLPort.cmake` — patched with stdio.h redirect + _threads.h _WINDOWS_ guard
- `src/CMakeLists.txt` — added add_subdirectory(game) + prepend cmake/compat/ to include path
- `src/engine/shared/library/CMakeLists.txt` — added INTERFACE stubs for sharedCollision/Fractal/SkillSystem
- `src/game/CMakeLists.txt` — delegates to shared/
- `src/game/shared/CMakeLists.txt` — delegates to library/
- `src/game/shared/library/CMakeLists.txt` — delegates to swgSharedUtility/
- `src/game/shared/library/swgSharedUtility/CMakeLists.txt` — project entry, delegates to src/
- `src/game/shared/library/swgSharedUtility/src/CMakeLists.txt` — STATIC lib, 7 shared + 1 win32 .cpp, full include path set

## Decisions Made

- **PlatformGlue.h shim vs. source edit:** Plan accepts "no C++ source edits" constraint; used a shadow header in `src/cmake/compat/` prepended to include path rather than wrapping `PlatformGlue.h` in `extern "C"` directly. The shim wraps the real header transparently.
- **STLPort stdio.h redirect:** Rather than copying UCRT headers into staged layout (which created two physical copies of stdio.h causing C2732 from `#pragma once` per-path tracking), replaced staged `include/stdio.h` with a one-line `#include` redirect to the canonical UCRT path.
- **swgSharedUtility PCH removed:** swg-main reference CMakeLists does not use `target_precompile_headers` for swgSharedUtility. Added PCH caused a transitive dependency chain that required sharedMemoryManager headers before Plan 02-02 builds them. Removed to match swg-main.
- **Extended include paths for swgSharedUtility:** Added `sharedGame`, `sharedMath`, `sharedObject`, `sharedUtility` include/public paths — these are header-only dependencies (no link deps added) needed to compile the 7 shared .cpp files.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Removed target_precompile_headers from swgSharedUtility**
- **Found during:** Task 4 (swgSharedUtility build)
- **Issue:** Plan's PATTERN I included `target_precompile_headers(swgSharedUtility PRIVATE shared/FirstSwgSharedUtility.h)` but the PCH chain `FirstSwgSharedUtility.h → FirstSharedFoundation.h → sharedMemoryManager/MemoryManager.h` requires sharedMemoryManager headers that are not yet built in Plan 02-01. swg-main's reference CMakeLists omits PCH for this lib.
- **Fix:** Removed `target_precompile_headers` call from `swgSharedUtility/src/CMakeLists.txt`. Matches swg-main.
- **Files modified:** `src/game/shared/library/swgSharedUtility/src/CMakeLists.txt`
- **Verification:** Debug + Release build succeeds
- **Committed in:** `dfbffeb1d`

**2. [Rule 1 - Bug] Fixed C2732 snprintf linkage conflict (PlatformGlue.h + UCRT)**
- **Found during:** Task 4 (swgSharedUtility build)
- **Issue:** `sharedFoundation/src/win32/PlatformGlue.h` (MSVC 7.1-era compat header) declares `int snprintf(...)` in C++ scope (no extern "C"). When UCRT `stdio.h` is later included (via `<string>` → STLPort → cstdio → MSVC cstdio → `<stdio.h>`), it re-declares `snprintf` with C linkage inside `_CRT_BEGIN_C_HEADER = extern "C"`. MSVC fires C2732: "linkage specification contradicts earlier specification". This occurs on Win SDK 10.0.26100 with VS 2022.
- **Fix:** Created `src/cmake/compat/sharedFoundation/PlatformGlue.h` shim that wraps the real `PlatformGlue.h` in `extern "C"`. Added `src/cmake/compat/` to include path BEFORE `sharedFoundation/include/public/` so the shim intercepts `#include "sharedFoundation/PlatformGlue.h"`. No C++ source edits required.
- **Files modified:** `src/cmake/compat/sharedFoundation/PlatformGlue.h` (created), `src/CMakeLists.txt` (add include path)
- **Verification:** C2732 error eliminated; all 8 swgSharedUtility .cpp files compile
- **Committed in:** `dfbffeb1d`

**3. [Rule 1 - Bug] Patched STLPort staged stdio.h to prevent C2732 from duplicate physical file**
- **Found during:** Task 4 (diagnosing C2732)
- **Issue:** Phase 1 `FindSTLPort.cmake` staged UCRT `stdio.h` into `build/stlport453/include/`. STLPort `cstdio` includes `../include/stdio.h` (staged copy) while other paths use the real UCRT `stdio.h`. Both files have `#pragma once` but are different physical paths, so both could be processed in one TU, re-triggering C2732.
- **Fix:** Replaced staged `include/stdio.h` with a redirect (`#include "real_ucrt_path/stdio.h"`). Both include paths now resolve to the same physical file, so `#pragma once` correctly prevents double-processing.
- **Files modified:** `src/cmake/win32/FindSTLPort.cmake`
- **Committed in:** `dfbffeb1d`

**4. [Rule 1 - Bug] Patched STLPort _threads.h for _WINDOWS_ guard**
- **Found during:** Task 4 (diagnosing _Interlocked* redeclaration)
- **Issue:** STLPort `_threads.h` checks `!defined(_STLP_WINDOWS_H_INCLUDED) && !defined(_WINDOWS_H)` to skip its own Interlocked* declarations when windows.h was already included. Modern Windows SDK sets `_WINDOWS_` (not `_WINDOWS_H`) as the inclusion guard, so the check failed and Interlocked* were redeclared with conflicting signatures.
- **Fix:** Added `&& !defined(_WINDOWS_)` to the guard in the staged `_threads.h`.
- **Files modified:** `src/cmake/win32/FindSTLPort.cmake` (patches staged file)
- **Committed in:** `dfbffeb1d`

**5. [Rule 3 - Blocking] Extended include paths for swgSharedUtility source dependencies**
- **Found during:** Task 4 (swgSharedUtility build)
- **Issue:** Plan's PATTERN I include list was insufficient for the actual whitengold source files. `CombatEngineData.h` needs `sharedGame/AttribMod.h` (sharedGame/include/public) and `sharedObject/CachedNetworkId.h` (sharedObject/include/public → sharedMath/include/public via Object.h → Transform.h).
- **Fix:** Added `sharedGame/include/public`, `sharedMath/include/public`, `sharedDebug/include/public` to include_directories in `swgSharedUtility/src/CMakeLists.txt`. These are header-only dependencies (no target_link_libraries added).
- **Files modified:** `src/game/shared/library/swgSharedUtility/src/CMakeLists.txt`
- **Committed in:** `dfbffeb1d`

---

**Total deviations:** 5 auto-fixed (3 Rule 1 bugs, 1 Rule 1 + Rule 3 blocking, 1 Rule 3 blocking)
**Impact on plan:** All fixes necessary for correctness and compilation. No scope creep. swg-main reference omits PCH for this lib; removing PCH aligns with the canonical reference. compat shim and STLPort patches are build-system-only changes per the no-source-edits constraint.

## Issues Encountered

- VS 2022 + Win SDK 26100 introduces stricter C/C++ linkage checking (C2732) that was not triggered with MSVC 7.1 / older SDKs. The `snprintf` conflict took significant diagnostic work to isolate to `PlatformGlue.h`. The fix is contained to CMake/compat files.
- STLPort 4.5.3 was designed for Win SDK XP era. The staged layout mechanism needed for it to find UCRT headers introduced its own issues. These are now fully resolved and will carry forward to all Phase 2 libs that include `FirstSharedFoundation.h`.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- All three INTERFACE stubs are in place — Plan 02-02 through 02-06 can reference sharedCollision, sharedFractal, sharedSkillSystem without configure errors
- swgSharedUtility is built — later plans that link against it or use its include paths will succeed
- PlatformGlue.h shim and STLPort patches are globally applied — any lib that includes FirstSharedFoundation.h + STL headers will compile without C2732
- game/ subtree wired — swgSharedNetworkMessages and other game-tier libs can be added to `src/game/` in later plans

## Self-Check: PASSED

All files verified:
- `src/cmake/stubs/time_t_probe.cpp.in` — FOUND
- `src/cmake/compat/sharedFoundation/PlatformGlue.h` — FOUND
- `src/cmake/win32/FindSTLPort.cmake` — FOUND
- `src/CMakeLists.txt` — FOUND
- `src/engine/shared/library/CMakeLists.txt` — FOUND
- `src/game/CMakeLists.txt` — FOUND
- `src/game/shared/CMakeLists.txt` — FOUND
- `src/game/shared/library/CMakeLists.txt` — FOUND
- `src/game/shared/library/swgSharedUtility/CMakeLists.txt` — FOUND
- `src/game/shared/library/swgSharedUtility/src/CMakeLists.txt` — FOUND
- `build/lib/Debug/swgSharedUtility.lib` — FOUND
- `build/lib/Release/swgSharedUtility.lib` — FOUND

All 4 task commits verified in git log:
- `b254e30e9` chore(02-01): time_t probe template — FOUND
- `634c626fb` chore(02-01): INTERFACE stubs — FOUND
- `5b3380e6e` chore(02-01): game/ subtree wiring — FOUND
- `dfbffeb1d` feat(02-01): swgSharedUtility + VS2022 compat — FOUND

---
*Phase: 02-shared-engine-libraries*
*Completed: 2026-05-04*
