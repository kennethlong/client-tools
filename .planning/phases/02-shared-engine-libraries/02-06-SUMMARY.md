---
phase: 02-shared-engine-libraries
plan: 06
subsystem: build-system
tags: [cmake, tier5-libs, sharedObject, sharedTerrain, sharedPathfinding, sharedGame, interface-stubs, whitengold-divergences]

# Dependency graph
requires:
  - phase: 02-shared-engine-libraries
    plan: 05
    provides: Tier 5 network cluster (sharedNetwork, sharedLog, sharedNetworkMessages, sharedUtility) — all Phase 2 Tier 2-5 deps for final 4 libs now present
provides:
  - sharedObject STATIC lib (build/lib/Debug/sharedObject.lib) — Tier 5 Object, Appearance, Container, Portal, Customization, Property
  - sharedTerrain STATIC lib (build/lib/Debug/sharedTerrain.lib) — Tier 5 TerrainAppearance, FloraManager, 47 generator files
  - sharedPathfinding STATIC lib (build/lib/Debug/sharedPathfinding.lib) — Tier 5 PathGraph, A* search, navigation
  - sharedGame STATIC lib (build/lib/Debug/sharedGame.lib) — Tier 5 139+ cpp across 13 subdirs; largest game-shared lib
  - Phase 2 COMPLETE: all 24 static .libs in Debug AND Release; SC-1/SC-2/SC-4/SC-5 all PASS
affects:
  - 03 (Phase 3 Client Engine Libraries — all Phase 2 deps satisfied, Phase 3 now unblocked)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "PATTERN A top-level entry for all 4 new libs"
    - "PATTERN B compiled lib with SHARED_SOURCES + PLATFORM_SOURCES"
    - "include/private path required for sharedObject (CustomizationData_Directory.h in include/private/sharedObject/)"
    - "Whitengold source-authoritative divergence: sharedGame adds SpaceStringIds, ShipHitEffectsManager, SharedObjectTemplateClientData/Interface; removes SharedMissionData/ListEntry/Token templates"
    - "INTERFACE stub linkage confirmed: sharedCollision + sharedFractal (both INTERFACE stubs) satisfy sharedObject and sharedTerrain link-time deps at configure + build time"

key-files:
  created:
    - src/engine/shared/library/sharedObject/CMakeLists.txt
    - src/engine/shared/library/sharedObject/src/CMakeLists.txt
    - src/engine/shared/library/sharedTerrain/CMakeLists.txt
    - src/engine/shared/library/sharedTerrain/src/CMakeLists.txt
    - src/engine/shared/library/sharedPathfinding/CMakeLists.txt
    - src/engine/shared/library/sharedPathfinding/src/CMakeLists.txt
    - src/engine/shared/library/sharedGame/CMakeLists.txt
    - src/engine/shared/library/sharedGame/src/CMakeLists.txt
  modified:
    - src/engine/shared/library/CMakeLists.txt (4 final add_subdirectory calls; 23 total engine shared)

key-decisions:
  - "sharedObject requires include/private — CustomizationData_Directory.h, CustomizationData_LocalDirectory.h, and CustomizationData_RemoteDirectory.h live in include/private/sharedObject/ (not public); added include/private path to sharedObject/CMakeLists.txt following same pattern as sharedNetwork (Rule 2 fix)"
  - "sharedGame whitengold source divergences fully catalogued — 5 whitengold-only files (SpaceStringIds.cpp/h, ShipHitEffectsManager.cpp/h, SharedObjectTemplateClientData.cpp/h, SharedObjectTemplateInterface.cpp/h) added; 6 swg-main-only files (SharedMissionDataObjectTemplate, SharedMissionListEntryObjectTemplate, SharedTokenObjectTemplate pairs) removed per D-02 (whitengold source authoritative)"
  - "sharedPathfinding has no target_link_libraries — include-only deps; linker resolves symbols at executable link time; matches swg-main exactly"
  - "Phase 2 SC-4 verified: 3 consecutive parallel Debug builds pass with no PCH race-condition C2065/C1010 errors"
  - "Phase 2 SC-5 verified: 6 time_t_probe.obj files found (3 libs x 2 configs) confirming _USE_32BIT_TIME_T=1 reaches all representative tiers"

requirements-completed: [BUILD-03]

# Metrics
duration: 30min
completed: 2026-05-04
---

# Phase 2 Plan 06: Final Tier 5 Libs + Phase 2 Completion Summary

**All 24 Phase 2 static .libs built in Debug AND Release under VS 2022 — sharedObject/Terrain/Pathfinding/Game authored, Phase 2 SC-1 through SC-5 verified, Phase 3 (Client Engine Libraries) unblocked**

## Performance

- **Duration:** 30 min
- **Started:** 2026-05-04T14:05:00Z
- **Completed:** 2026-05-04T14:35:00Z
- **Tasks:** 3 automated + 1 checkpoint
- **Files modified:** 9 (8 new CMakeLists + 1 updated library CMakeLists)

## Accomplishments

- sharedObject CMakeLists pair authored — mirrors swg-main + W-only PulseDynamics.cpp/h; include/private fix for CustomizationData_Directory.h; PCH at shared/core/FirstSharedObject.h; links sharedCollision (INTERFACE), sharedFoundation, sharedGame, sharedTerrain, sharedUtility, swgSharedUtility
- sharedTerrain CMakeLists pair authored — 47 .cpp files across appearance/core/flora/generator/object/ subdirs; PCH at shared/core/FirstSharedTerrain.h; links sharedCollision (INTERFACE) + sharedFractal (INTERFACE)
- sharedPathfinding CMakeLists pair authored — 24-file source list; no target_link_libraries (include-only per swg-main); PCH at shared/FirstSharedPathfinding.h
- sharedGame CMakeLists pair authored — 139+ .cpp files across 13 subdirs (appearance/collision/combat/command/core/dynamics/mount/object/objectTemplate/quest/space/sui/travel); whitengold divergences fully reconciled; Boost_INCLUDE_DIR included; links sharedFoundation sharedObject sharedUtility archive; PCH at shared/core/FirstSharedGame.h
- Phase 2 finalized: all 24 libs in build/lib/Debug/ (34 total including Phase 1); Release build passes; 3x deterministic Debug build passes; 6 time_t_probe.obj files confirmed

## Phase 2 Final Verification Results

- **SC-1 PASS** — All 24 Phase 2 .lib files present in build/lib/Debug/ (34 total: 24 Phase 2 + 10 Phase 1)
- **SC-2 PASS** — Release build: cmake --build build --config Release --parallel 8 exit 0
- **SC-3 PASS** — cmake configure: no "Target not found" errors; tier ordering correct
- **SC-4 PASS** — 3 consecutive cmake --build Debug --parallel 8 all exit 0; no C2065/C1010 PCH race errors
- **SC-5 PASS** — 6 time_t_probe.obj files (sharedDebug/sharedFoundation/sharedNetwork x Debug/Release); static_asserts all passed

## Phase 2 Cumulative Stats

- Total plans: 6
- Total lib pairs authored: 24 (23 engine shared + swgSharedUtility)
- INTERFACE stubs: 3 (sharedCollision, sharedFractal, sharedSkillSystem)
- time_t probe template + 3 instrumented TUs
- src/game/ subtree wiring (Plans 01-02)
- Total .lib files in build/lib/Debug/: 34

## Task Commits

1. **Task 1: sharedObject + sharedTerrain CMakeLists pairs** — `7d77f645f` (feat)
2. **Task 2: sharedPathfinding + sharedGame CMakeLists pairs** — `8ddb1b5a9` (feat)
3. **Task 3: Final phase verification** — (no commit — verification only)

## Files Created/Modified

- `src/engine/shared/library/sharedObject/CMakeLists.txt` — PATTERN A + include/private for CustomizationData_Directory.h
- `src/engine/shared/library/sharedObject/src/CMakeLists.txt` — swg-main mirror + W-only PulseDynamics; PCH shared/core/FirstSharedObject.h; INTERFACE stub link deps
- `src/engine/shared/library/sharedTerrain/CMakeLists.txt` — PATTERN A top-level entry
- `src/engine/shared/library/sharedTerrain/src/CMakeLists.txt` — 47 files; links sharedCollision + sharedFractal (both INTERFACE); PCH shared/core/FirstSharedTerrain.h
- `src/engine/shared/library/sharedPathfinding/CMakeLists.txt` — PATTERN A top-level entry
- `src/engine/shared/library/sharedPathfinding/src/CMakeLists.txt` — 24 files; no target_link_libraries; PCH shared/FirstSharedPathfinding.h
- `src/engine/shared/library/sharedGame/CMakeLists.txt` — PATTERN A top-level entry
- `src/engine/shared/library/sharedGame/src/CMakeLists.txt` — 139+ .cpp; whitengold divergences; Boost_INCLUDE_DIR; links sharedFoundation sharedObject sharedUtility archive; PCH shared/core/FirstSharedGame.h
- `src/engine/shared/library/CMakeLists.txt` — 4 final add_subdirectory calls (sharedObject/Terrain/Pathfinding/Game); 23 total engine shared subdirectories

## Decisions Made

- **sharedObject include/private:** CustomizationData_Directory.h lives in include/private/sharedObject/ only (not in public headers). The source files include it as "sharedObject/CustomizationData_Directory.h" requiring the include/private parent path. Pattern established by sharedNetwork extended to sharedObject.
- **sharedGame whitengold divergences:** D-02 (whitengold source authoritative) applied. 5 files present in whitengold but not swg-main are included; 6 swg-main-only files removed. The removed files (SharedMissionData/ListEntry/Token templates) are NGE-era additions to swg-main that were not in the original 2010 leak.
- **sharedPathfinding: no link deps:** Matches swg-main exactly. The pathfinding algorithms depend on sharedObject and sharedCollision at header level only; the symbols resolve transitively when executables link.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical Functionality] Missing include/private path in sharedObject/CMakeLists.txt**
- **Found during:** Task 1 (sharedObject build)
- **Issue:** `CustomizationData.cpp`, `CustomizationData_LocalDirectory.cpp`, and `CustomizationData_RemoteDirectory.cpp` include `sharedObject/CustomizationData_Directory.h` which is in `include/private/sharedObject/`, not `include/public/`. The plan's PATTERN A template omitted the private include path.
- **Fix:** Added `include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/private)` to `sharedObject/CMakeLists.txt`, matching the sharedNetwork pattern established in Plan 05.
- **Files modified:** `sharedObject/CMakeLists.txt`
- **Committed in:** `7d77f645f`

**2. [Rule 1 - Bug] sharedGame source list had 6 swg-main-only files and was missing 5 whitengold-only files**
- **Found during:** Task 2 (CMake configure — "Cannot find source file: SharedMissionDataObjectTemplate.cpp")
- **Issue:** The plan's action block mirrored swg-main verbatim. Whitengold is missing SharedMissionDataObjectTemplate, SharedMissionListEntryObjectTemplate, SharedTokenObjectTemplate (all pairs). Whitengold additionally has SpaceStringIds.cpp/h, ShipHitEffectsManager.cpp/h, SharedObjectTemplateClientData.cpp/h, SharedObjectTemplateInterface.cpp/h.
- **Fix:** Removed 6 missing swg-main files; added 8 whitengold-only files. Per D-02 (whitengold source authoritative).
- **Files modified:** `sharedGame/src/CMakeLists.txt`
- **Committed in:** `8ddb1b5a9`

---

**Total deviations:** 2 auto-fixed (1 Rule 2 — missing include path; 1 Rule 1 — source list divergences from swg-main)
**Impact on plan:** Minimal. Both are standard whitengold adaptations — include/private pattern established by sharedNetwork, source divergences consistent with D-02 policy throughout Phase 2.

## Known Stubs

None — all CMakeLists files are build system configuration only. No UI or data path components.

## Threat Flags

None — this plan creates only CMake build system files. No network endpoints, auth paths, file access patterns, or schema changes introduced.

## Self-Check: PASSED

All files verified:
- `src/engine/shared/library/sharedObject/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedObject/src/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedTerrain/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedTerrain/src/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedPathfinding/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedPathfinding/src/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedGame/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedGame/src/CMakeLists.txt` — FOUND
- `build/lib/Debug/sharedObject.lib` — FOUND
- `build/lib/Debug/sharedTerrain.lib` — FOUND
- `build/lib/Debug/sharedPathfinding.lib` — FOUND
- `build/lib/Debug/sharedGame.lib` — FOUND

All 2 task commits verified in git log:
- `7d77f645f` feat(02-06): sharedObject + sharedTerrain CMakeLists pairs — FOUND
- `8ddb1b5a9` feat(02-06): sharedPathfinding + sharedGame CMakeLists pairs — FOUND

---
*Phase: 02-shared-engine-libraries*
*Completed: 2026-05-04*
