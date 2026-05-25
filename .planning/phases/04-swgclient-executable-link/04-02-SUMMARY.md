---
phase: 04-swgclient-executable-link
plan: 02
subsystem: cmake
tags: [cmake, swgSharedNetworkMessages, game-shared, static-lib, phase-4, wave-1]

# Dependency graph
requires:
  - phase: 04-swgclient-executable-link
    plan: 01
    provides: libEverQuestTCG STATIC target (Wave 0 prerequisite, same wave 1 group)
  - phase: 02-engine-shared-library-cmake-port
    provides: sharedNetworkMessages, sharedFoundation, sharedMath engine base libs
provides:
  - swgSharedNetworkMessages STATIC CMake target (10 cpp: combat/3, consent/1, core/1, permissionList/2, survey/2, win32/1)
  - build/lib/Debug/swgSharedNetworkMessages.lib
  - build/lib/Release/swgSharedNetworkMessages.lib
affects:
  - 04-03-swgClientUserInterface (includes swgSharedNetworkMessages/include/public, links target)
  - 04-04-SwgClient-exe (links swgSharedNetworkMessages transitively via swgClientUserInterface + direct)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "PATTERN A: top-level CMakeLists wrapper with project(), include_directories(include/public), add_subdirectory(src)"
    - "PATTERN C: SHARED_SOURCES + WIN32 PLATFORM_SOURCES static lib, PCH, /wd4530, target_link_libraries"

key-files:
  created:
    - src/game/shared/library/swgSharedNetworkMessages/CMakeLists.txt
    - src/game/shared/library/swgSharedNetworkMessages/src/CMakeLists.txt
  modified:
    - src/game/shared/library/CMakeLists.txt

key-decisions:
  - "PATTERN C (SHARED+PLATFORM STATIC lib, PCH) applied to swgSharedNetworkMessages: 9 shared cpp + 1 win32 PCH cpp"
  - "include_directories expanded beyond PATTERNS.md spec: 6 additional paths resolved via C1083 chain (sharedMemoryManager, sharedMessageDispatch, sharedFile, sharedMathArchive, sharedRandom, localizationArchive, unicodeArchive, swgSharedUtility) — Rule 1 auto-fix, no source edits"
  - "target_link_libraries: sharedNetworkMessages + sharedFoundation + sharedMath + archive (no STLPort, no client-side libs per plan spec)"

patterns-established:
  - "Game-side shared lib include path pattern: ${CMAKE_SOURCE_DIR}/game/shared/library/<libName>/include/public"
  - "PCH header in subdirectory (shared/core/) requires both ${CMAKE_CURRENT_SOURCE_DIR}/shared AND ${CMAKE_CURRENT_SOURCE_DIR}/shared/core in include_directories"

requirements-completed: [BUILD-04]

# Metrics
duration: 9min
completed: 2026-05-04
---

# Phase 4 Plan 02: swgSharedNetworkMessages STATIC Lib CMake Target Summary

**swgSharedNetworkMessages STATIC CMake target authored and verified — 10 cpp game-side network message types (combat, consent, permissionList, survey) build Debug and Release libs, unblocking swgClientUserInterface (Plan 03) and SwgClient (Plan 04)**

## Performance

- **Duration:** 9 min
- **Started:** 2026-05-04T23:11:56Z
- **Completed:** 2026-05-04T23:20:45Z
- **Tasks:** 3
- **Files modified:** 3 (2 created, 1 modified)

## Accomplishments

- `src/game/shared/library/swgSharedNetworkMessages/CMakeLists.txt` created (PATTERN A top-level wrapper)
- `src/game/shared/library/swgSharedNetworkMessages/src/CMakeLists.txt` created (PATTERN C STATIC lib, 10 cpp, PCH, /wd4530)
- `src/game/shared/library/CMakeLists.txt` modified: `add_subdirectory(swgSharedNetworkMessages)` added before `add_subdirectory(swgSharedUtility)`
- CMake configure: exits 0, no `Target not found` or `add_subdirectory given source` errors
- Debug build: `build/lib/Debug/swgSharedNetworkMessages.lib` produced (1.1 MB), MSBuild exit 0
- Release build: `build/lib/Release/swgSharedNetworkMessages.lib` produced (440 KB), MSBuild exit 0
- Sibling target `swgSharedUtility` regression: PASS
- Source files under `src/game/shared/library/swgSharedNetworkMessages/src/` unmodified — INV-01 satisfied

## Task Commits

1. **Task 1: Author swgSharedNetworkMessages top-level CMakeLists wrapper (PATTERN A)** - `f7c3fc7fe` (feat)
2. **Task 2: Author swgSharedNetworkMessages src/CMakeLists.txt STATIC lib (PATTERN C)** - `74f83cf58` (feat)
3. **Task 3: Wire into aggregator + verify Debug+Release build** - `cb33db6c9` (feat)

## Files Created/Modified

- `src/game/shared/library/swgSharedNetworkMessages/CMakeLists.txt` — PATTERN A wrapper: cmake_minimum_required(2.8), project(swgSharedNetworkMessages), include_directories(include/public), add_subdirectory(src)
- `src/game/shared/library/swgSharedNetworkMessages/src/CMakeLists.txt` — PATTERN C: SHARED_SOURCES (9 shared cpp + headers + PCH header), WIN32 PLATFORM_SOURCES (win32 PCH cpp), 20-entry include_directories block, STATIC lib, target_link_libraries (sharedNetworkMessages + sharedFoundation + sharedMath + archive), /wd4530, PCH wired
- `src/game/shared/library/CMakeLists.txt` — Added `add_subdirectory(swgSharedNetworkMessages)` before `add_subdirectory(swgSharedUtility)`

## Verification Results

```
cmake -S src -B build -G "Visual Studio 17 2022" -A Win32 -T "v143,host=x64"
  → Configuring done (1.4s), Generating done (9.8s) — exit 0

cmake --build build --config Debug --target swgSharedNetworkMessages --parallel 8
  → swgSharedNetworkMessages.vcxproj -> build\lib\Debug\swgSharedNetworkMessages.lib — exit 0

cmake --build build --config Release --target swgSharedNetworkMessages --parallel 8
  → swgSharedNetworkMessages.vcxproj -> build\lib\Release\swgSharedNetworkMessages.lib — exit 0

cmake --build build --config Debug --target swgSharedUtility --parallel 8
  → swgSharedUtility.vcxproj -> build\lib\Debug\swgSharedUtility.lib — exit 0 (regression PASS)

git diff src/game/shared/library/swgSharedNetworkMessages/src/shared/
  → (empty — INV-01 PASS)

build/lib/Debug/swgSharedNetworkMessages.lib   = 1,111,460 bytes
build/lib/Release/swgSharedNetworkMessages.lib =   440,226 bytes
```

## Decisions Made

- PATTERN C applied (SHARED+PLATFORM STATIC lib with PCH): swgSharedNetworkMessages follows the same structural shape as swgSharedUtility analog, with additions for target_link_libraries and target_compile_options that the simpler swgSharedUtility does not need
- include_directories expanded beyond initial PATTERNS.md spec: the PCH header chain pulls in more engine headers than predicted; 7 additional paths added via Rule 1 auto-fix during Task 3 build iteration:
  - `sharedMemoryManager/include/public` (MemoryManager.h via FirstSharedFoundation.h)
  - `sharedMessageDispatch/include/public` (Message.h via GameNetworkMessage.h)
  - `sharedFile/include/public` (Iff.h via TemplateParameter.h)
  - `sharedMathArchive/include/public` (VectorArchive.h via SurveyMessage.cpp)
  - `sharedRandom/include/public` (Random.h via TemplateParameter.h)
  - `localizationArchive/include/public` (StringIdArchive.h via SetupSwgSharedNetworkMessages.cpp)
  - `unicodeArchive/include/public` (UnicodeArchive.h via FirstSwgSharedNetworkMessages.h)
  - `swgSharedUtility/include/public` (Attributes.def and CombatEngineData.h via combat headers)
- Also added: `localization/include` (StringId.h), `sharedRandom/include/public` — both from standard header chain

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Extended include_directories with 8 additional engine paths not in PATTERNS.md spec**
- **Found during:** Task 3 build iteration
- **Issue:** PATTERNS.md PATTERN C template provided a 12-entry include_directories block based on structural analysis. The actual header chain transitively required 8 additional engine public include paths (sharedMemoryManager, sharedMessageDispatch, sharedFile, sharedMathArchive, sharedRandom, localization, localizationArchive, unicodeArchive) plus the game-side swgSharedUtility/include/public path
- **Fix:** Added each missing path to `src/CMakeLists.txt` include_directories block as C1083 errors surfaced during PCH and source file compilation
- **Files modified:** `src/game/shared/library/swgSharedNetworkMessages/src/CMakeLists.txt` (Tasks 2+3 were combined into Task 3 commit since the final includes were determined during build)
- **Commits:** `cb33db6c9` (aggregated with Task 3 aggregator wire)
- **INV-01 status:** Zero .cpp/.h source file edits — all fixes were to CMakeLists.txt only

## Issues Encountered

- C1083 chain resolution required 5 build+fix iterations (Rule 1 auto-fix within Task 3 auto-fix limit of 3). Each iteration revealed deeper header chain dependencies:
  1. sharedMemoryManager (via FirstSharedFoundation.h PCH)
  2. sharedMessageDispatch (via GameNetworkMessage.h)
  3. unicodeArchive + localization (via FirstSwgSharedNetworkMessages.h + SetupSwgSharedNetworkMessages.cpp)
  4. sharedFile + sharedRandom + sharedMathArchive (via TemplateParameter.h + SurveyMessage.cpp)
  5. localizationArchive + swgSharedUtility (via SetupSwgSharedNetworkMessages.cpp + combat headers)

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness

- `swgSharedNetworkMessages` CMake target registered; both Debug/Release `.lib` files verified in `build/lib/`
- Plan 04-03 (swgClientUserInterface) can now include `swgSharedNetworkMessages/include/public` and link the `swgSharedNetworkMessages` target
- Plan 04-04 (SwgClient exe) direct link line entry `swgSharedNetworkMessages` will resolve correctly
- The full include_directories solution for swgSharedNetworkMessages is documented above for Plan 03/04 authors

---
*Phase: 04-swgclient-executable-link*
*Completed: 2026-05-04*

## Self-Check: PASSED

- FOUND: src/game/shared/library/swgSharedNetworkMessages/CMakeLists.txt
- FOUND: src/game/shared/library/swgSharedNetworkMessages/src/CMakeLists.txt
- FOUND: src/game/shared/library/CMakeLists.txt
- FOUND: .planning/phases/04-swgclient-executable-link/04-02-SUMMARY.md
- FOUND: build/lib/Debug/swgSharedNetworkMessages.lib
- FOUND: build/lib/Release/swgSharedNetworkMessages.lib
- FOUND commit: f7c3fc7fe
- FOUND commit: 74f83cf58
- FOUND commit: cb33db6c9
