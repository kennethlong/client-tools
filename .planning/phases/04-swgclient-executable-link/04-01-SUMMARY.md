---
phase: 04-swgclient-executable-link
plan: 01
subsystem: cmake
tags: [cmake, libEverQuestTCG, external, static-lib, win32, phase-4]

# Dependency graph
requires:
  - phase: 03-client-engine-libraries-sdk-heavy-tier
    provides: clientUserInterface and clientGame CMake targets that will link libEverQuestTCG
provides:
  - libEverQuestTCG STATIC CMake target (1 cpp, win32-only, LoadLibrary wrapper)
  - build/lib/Debug/libEverQuestTCG.lib
  - build/lib/Release/libEverQuestTCG.lib
affects:
  - 04-02-swgSharedNetworkMessages (no direct dep but part of same wave)
  - 04-03-swgClientUserInterface (links libEverQuestTCG in target_link_libraries)
  - 04-04-SwgClient-exe (links libEverQuestTCG transitively via swgClientUserInterface)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "PATTERN A: top-level CMakeLists wrapper with project(), include_directories(include/public), add_subdirectory(src)"
    - "PATTERN D: win32-only single-cpp STATIC lib with PLATFORM_SOURCES block, no PCH, no target_link_libraries"

key-files:
  created:
    - src/external/3rd/library/libEverQuestTCG/CMakeLists.txt
    - src/external/3rd/library/libEverQuestTCG/src/CMakeLists.txt
  modified:
    - src/external/3rd/library/CMakeLists.txt

key-decisions:
  - "PATTERN D (win32-only STATIC lib, no PCH) applied to libEverQuestTCG: vendored 3rd-party lib has no First*.h, INV-01 prohibits adding one"
  - "No target_link_libraries on libEverQuestTCG: only Windows system headers used; all TCG runtime calls via LoadLibrary/GetProcAddress (A3 verified in RESEARCH.md)"

patterns-established:
  - "PATTERN D wire-in: add_subdirectory(libEverQuestTCG) appended AFTER existing entries in external/3rd/library/CMakeLists.txt"

requirements-completed: [BUILD-04]

# Metrics
duration: 4min
completed: 2026-05-04
---

# Phase 4 Plan 01: libEverQuestTCG Static Lib CMake Target Summary

**libEverQuestTCG STATIC CMake target authored and verified — TCG LoadLibrary wrapper (1 cpp) builds Debug and Release libs unblocking swgClientUserInterface link**

## Performance

- **Duration:** 4 min
- **Started:** 2026-05-04T23:05:31Z
- **Completed:** 2026-05-04T23:08:51Z
- **Tasks:** 3
- **Files modified:** 3 (2 created, 1 modified)

## Accomplishments
- `src/external/3rd/library/libEverQuestTCG/CMakeLists.txt` created (PATTERN A top-level wrapper)
- `src/external/3rd/library/libEverQuestTCG/src/CMakeLists.txt` created (PATTERN D win32-only STATIC lib, no PCH)
- `src/external/3rd/library/CMakeLists.txt` modified to add `add_subdirectory(libEverQuestTCG)`
- CMake configure: exits 0, no Target not found errors
- Debug build: `build/lib/Debug/libEverQuestTCG.lib` produced, MSBuild exit 0
- Release build: `build/lib/Release/libEverQuestTCG.lib` produced, MSBuild exit 0
- Sibling targets `ui` and `udplibrary` regression-verified, both still exit 0
- Source file `libEverQuestTCG.cpp` unmodified (INV-01 satisfied)

## Task Commits

1. **Task 1: Author libEverQuestTCG top-level CMakeLists wrapper (PATTERN A)** - `7c0cdabd3` (feat)
2. **Task 2: Author libEverQuestTCG src/CMakeLists.txt STATIC lib (PATTERN D)** - `6c6fb31e2` (feat)
3. **Task 3: Wire libEverQuestTCG into external/3rd/library aggregator + verify** - `af158619c` (feat)

## Files Created/Modified
- `src/external/3rd/library/libEverQuestTCG/CMakeLists.txt` - PATTERN A wrapper: cmake_minimum_required, project(libEverQuestTCG), include_directories(include/public), add_subdirectory(src)
- `src/external/3rd/library/libEverQuestTCG/src/CMakeLists.txt` - PATTERN D: win32-only PLATFORM_SOURCES with libEverQuestTCG.cpp + libEverQuestTCG.h, include_directories for win32/, STATIC lib, no PCH, no link deps
- `src/external/3rd/library/CMakeLists.txt` - Added `add_subdirectory(libEverQuestTCG)` after `add_subdirectory(ui)`

## Decisions Made
- No PCH on libEverQuestTCG: 3rd-party vendored external lib with no First*.h; INV-01 prohibits source edits to add one
- No target_link_libraries on libEverQuestTCG: RESEARCH.md A3 deep-dive confirmed the .cpp only includes `<windows.h>`, `<stdlib.h>`, `<stdio.h>`, `<assert.h>`, `<malloc.h>` — all Windows system headers; actual TCG calls use LoadLibrary/GetProcAddress runtime

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
- CMake toolset argument order: initial configure attempt used `host=x64,v143` but the existing CMake cache had `v143,host=x64`. Used the matching format for the cache. No file changes needed.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- `libEverQuestTCG` CMake target registered and both Debug/Release `.lib` files verified in `build/lib/`
- Plan 04-02 (swgSharedNetworkMessages) and Plan 04-03 (swgClientUserInterface) can now reference `libEverQuestTCG` in `target_link_libraries` without LNK2019 errors
- The foundational gap closure for Phase 4 is complete; Wave 1 unblocked

---
*Phase: 04-swgclient-executable-link*
*Completed: 2026-05-04*

## Self-Check: PASSED

- FOUND: src/external/3rd/library/libEverQuestTCG/CMakeLists.txt
- FOUND: src/external/3rd/library/libEverQuestTCG/src/CMakeLists.txt
- FOUND: src/external/3rd/library/CMakeLists.txt
- FOUND: .planning/phases/04-swgclient-executable-link/04-01-SUMMARY.md
- FOUND: build/lib/Debug/libEverQuestTCG.lib
- FOUND: build/lib/Release/libEverQuestTCG.lib
- FOUND commit: 7c0cdabd3
- FOUND commit: 6c6fb31e2
- FOUND commit: af158619c
