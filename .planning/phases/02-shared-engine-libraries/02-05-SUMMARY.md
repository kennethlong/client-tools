---
phase: 02-shared-engine-libraries
plan: 05
subsystem: build-system
tags: [cmake, tier5-libs, sharedUtility, sharedNetworkMessages, sharedLog, sharedNetwork, time_t-probe, udplibrary, boost]

# Dependency graph
requires:
  - phase: 02-shared-engine-libraries
    plan: 04
    provides: Tier 4 libs (sharedCompression, sharedRegex, sharedImage, sharedIoWin, sharedMessageDispatch)
provides:
  - sharedUtility STATIC lib (build/lib/Debug/sharedUtility.lib) — Tier 5 DataTable, OptionManager, BakedTerrain, ValueDictionary, name generators
  - sharedNetworkMessages STATIC lib (build/lib/Debug/sharedNetworkMessages.lib) — Tier 5 338 message types across 9 subdirs
  - sharedLog STATIC lib (build/lib/Debug/sharedLog.lib) — Tier 5 LogManager, NetLogObserver, FileLogObserver
  - sharedNetwork STATIC lib (build/lib/Debug/sharedNetwork.lib) — Tier 5 Connection, Service, NetworkHandler, UDP transport; D-08 time_t probe
affects:
  - 02-06 (sharedObject, sharedTerrain, sharedPathfinding, sharedGame now unblocked — all Tier 5 deps satisfied)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "PATTERN A top-level entry for all 4 new libs"
    - "PATTERN B compiled lib with SHARED_SOURCES + PLATFORM_SOURCES"
    - "PATTERN D time_t probe (third and final probe in sharedNetwork/src/CMakeLists.txt)"
    - "PATTERN F multi-subdir source enumeration for sharedNetworkMessages (9 subdirs, 338 .cpp files)"
    - "PATTERN G external dep linkage — sharedNetwork links udplibrary + mswsock + ws2_32"
    - "sharedNetwork requires include/private path for ManagerHandler.h/ConnectionHandler.h"
    - "Boost_INCLUDE_DIR required by sharedUtility (UniqueNameList) and sharedNetworkMessages (sharedGame/CraftingData.h transitive)"

key-files:
  created:
    - src/engine/shared/library/sharedUtility/CMakeLists.txt
    - src/engine/shared/library/sharedUtility/src/CMakeLists.txt
    - src/engine/shared/library/sharedNetworkMessages/CMakeLists.txt
    - src/engine/shared/library/sharedNetworkMessages/src/CMakeLists.txt
    - src/engine/shared/library/sharedLog/CMakeLists.txt
    - src/engine/shared/library/sharedLog/src/CMakeLists.txt
    - src/engine/shared/library/sharedNetwork/CMakeLists.txt
    - src/engine/shared/library/sharedNetwork/src/CMakeLists.txt
  modified:
    - src/engine/shared/library/CMakeLists.txt (4 new add_subdirectory calls)

key-decisions:
  - "sharedUtility and sharedNetworkMessages both require ${Boost_INCLUDE_DIR} — whitengold retains sharedMemoryManager and the transitive boost include chain UniqueNameList.cpp -> boost/smart_ptr.hpp and SetupSharedNetworkMessages.cpp -> sharedGame/CraftingData.h -> boost/smart_ptr.hpp"
  - "sharedNetwork/CMakeLists.txt adds include/private path — ManagerHandler.h and ConnectionHandler.h live in include/private/sharedNetwork/ (not public); swg-main does this at top-level CMakeLists; pattern extended to whitengold"
  - "UdpLibraryMT/ included in sharedNetwork — RESEARCH.md §sharedNetwork incorrectly claimed whitengold had no UdpLibraryMT/ subdirectory; direct listing confirmed 16 files present; D-02 (whitengold source authoritative) applied"
  - "sharedNetworkMessages PCH at shared/core/FirstSharedNetworkMessages.h — PCH header is in core/ subdirectory, not root shared/; same pattern as sharedXml"

# Metrics
duration: 5min
completed: 2026-05-04
---

# Phase 2 Plan 05: Network Cluster Libraries Summary

**8 new CMakeLists files for the 4-library network cluster — sharedUtility (Tier 5 base), sharedNetworkMessages (338 .cpp files, largest lib in Phase 2), sharedLog (upward dep on sharedNetworkMessages), sharedNetwork (links sharedLog + sharedMessageDispatch + udplibrary, D-08 final time_t probe) — all building as static .libs under VS 2022**

## Performance

- **Duration:** 5 min
- **Started:** 2026-05-04T13:33:27Z
- **Completed:** 2026-05-04T13:38:XX Z
- **Tasks:** 2
- **Files modified:** 9 (8 new CMakeLists + 1 updated library CMakeLists)

## Accomplishments

- sharedUtility CMakeLists pair authored — 62-file source list mirroring swg-main; PCH at shared/FirstSharedUtility.h; links sharedFoundation sharedMath; ${Boost_INCLUDE_DIR} added (Rule 2 fix for UniqueNameList boost::shared_ptr usage)
- sharedNetworkMessages CMakeLists pair authored — 338 .cpp across 9 subdirectories (shared/, chat/, clientGameServer/, clientLoginServer/, common/, core/, customerService/, planetWatch/, voicechat/) mirrored verbatim from swg-main; PCH at shared/core/FirstSharedNetworkMessages.h; ${Boost_INCLUDE_DIR} added (transitive from sharedGame/CraftingData.h); links sharedUtility localization localizationArchive unicode unicodeArchive
- sharedLog CMakeLists pair authored — 20-file source list mirroring swg-main; PCH at shared/FirstSharedLog.h; target_link_libraries(sharedLog sharedNetworkMessages) — Tier 5 dep chain correct
- sharedNetwork CMakeLists pair authored — UdpLibraryMT/ 16 files included (RESEARCH.md correction applied per D-02); include/private added for ManagerHandler.h/ConnectionHandler.h (Rule 2 fix); links sharedFoundation sharedLog sharedMessageDispatch udplibrary + mswsock ws2_32; D-08 THIRD and FINAL time_t probe (Tier 5 representative, verifies _USE_32BIT_TIME_T=1 reaches transitive closure)
- All 3 time_t probes verified — sharedDebug (Tier 2), sharedFoundation (Tier 3), sharedNetwork (Tier 5) — all compile time_t_probe.cpp with no static_assert failures
- Full cmake --build succeeds — 30 total .lib files in build/lib/Debug/
- Phase 2 cumulative lib count: 20 (4 new + 16 from Plans 01-04)

## Task Commits

1. **Task 1: sharedUtility + sharedNetworkMessages CMakeLists pairs** — `fcfad51de` (feat)
2. **Task 2: sharedLog + sharedNetwork CMakeLists pairs** — `a6e663cc7` (feat)

## Files Created/Modified

- `src/engine/shared/library/sharedUtility/CMakeLists.txt` — PATTERN A top-level entry
- `src/engine/shared/library/sharedUtility/src/CMakeLists.txt` — 62-file source list; links sharedFoundation sharedMath; ${Boost_INCLUDE_DIR} added
- `src/engine/shared/library/sharedNetworkMessages/CMakeLists.txt` — PATTERN A top-level entry
- `src/engine/shared/library/sharedNetworkMessages/src/CMakeLists.txt` — 338 .cpp across 9 subdirs; PCH at shared/core/; ${Boost_INCLUDE_DIR} added; links sharedUtility + localization family
- `src/engine/shared/library/sharedLog/CMakeLists.txt` — PATTERN A top-level entry
- `src/engine/shared/library/sharedLog/src/CMakeLists.txt` — 20-file source list; links sharedNetworkMessages; PCH at shared/FirstSharedLog.h
- `src/engine/shared/library/sharedNetwork/CMakeLists.txt` — PATTERN A + include/private path for private headers
- `src/engine/shared/library/sharedNetwork/src/CMakeLists.txt` — UdpLibraryMT/ included; mswsock/ws2_32 WIN32 linkage; D-08 time_t probe
- `src/engine/shared/library/CMakeLists.txt` — 4 new add_subdirectory calls (sharedUtility, sharedNetworkMessages, sharedLog, sharedNetwork)

## Decisions Made

- **${Boost_INCLUDE_DIR} required by sharedUtility and sharedNetworkMessages:** whitengold's UniqueNameList.cpp uses boost::shared_ptr (includes boost/smart_ptr.hpp) and sharedNetworkMessages transitively pulls in sharedGame/CraftingData.h which also uses boost. swg-main only adds Boost_INCLUDE_DIR to sharedGame, but whitengold's sharedUtility also needs it. This is consistent with the sharedMemoryManager retention pattern (whitengold retains more legacy dependencies than swg-main).
- **sharedNetwork include/private:** ManagerHandler.h and ConnectionHandler.h are in include/private/sharedNetwork/ not in include/public. swg-main adds this at the top-level CMakeLists.txt. Whitengold required the same fix. Pattern: any lib with an include/private/ directory needs that path in its top-level CMakeLists.
- **UdpLibraryMT/ in sharedNetwork:** RESEARCH.md §sharedNetwork incorrectly claimed whitengold had no UdpLibraryMT/ subdirectory and marked those files as S-only (skip). Direct `ls` of the directory confirmed 16 files are present. Per D-02 (whitengold source authoritative), these files are included. The swg-main reference also includes them — this is a clean match.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical Functionality] Missing ${Boost_INCLUDE_DIR} in sharedUtility**
- **Found during:** Task 1 (sharedUtility build)
- **Issue:** `UniqueNameList.cpp` includes `boost/smart_ptr.hpp`. The boost headers at `src/external/3rd/library/boost/boost/` are not in any globally-included path. The plan's sharedUtility template omitted `${Boost_INCLUDE_DIR}`.
- **Fix:** Added `${Boost_INCLUDE_DIR}` to include_directories in sharedUtility/src/CMakeLists.txt.
- **Files modified:** `sharedUtility/src/CMakeLists.txt`
- **Committed in:** `fcfad51de`

**2. [Rule 2 - Missing Critical Functionality] Missing ${Boost_INCLUDE_DIR} in sharedNetworkMessages**
- **Found during:** Task 1 (sharedNetworkMessages build — same build run)
- **Issue:** `SetupSharedNetworkMessages.cpp` includes `sharedGame/CraftingData.h` which includes `boost/smart_ptr.hpp`. Same root cause as deviation 1.
- **Fix:** Added `${Boost_INCLUDE_DIR}` to include_directories in sharedNetworkMessages/src/CMakeLists.txt.
- **Files modified:** `sharedNetworkMessages/src/CMakeLists.txt`
- **Committed in:** `fcfad51de`

**3. [Rule 2 - Missing Critical Functionality] Missing include/private path in sharedNetwork/CMakeLists.txt**
- **Found during:** Task 2 (sharedNetwork build)
- **Issue:** `ManagerHandler.cpp`, `ConnectionHandler.cpp`, `Service.cpp`, `NetworkHandler.cpp`, `Connection.cpp` all include `sharedNetwork/ManagerHandler.h` or `sharedNetwork/ConnectionHandler.h`. These headers are in `include/private/sharedNetwork/`, not `include/public/`. The plan's PATTERN A template omitted the private include path.
- **Fix:** Added `include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/private)` to `sharedNetwork/CMakeLists.txt`, matching the swg-main pattern exactly.
- **Files modified:** `sharedNetwork/CMakeLists.txt`
- **Committed in:** `a6e663cc7`

---

**Total deviations:** 3 auto-fixed (Rule 2 — missing include paths required for compilation)
**Impact on plan:** Minimal. All three are missing include paths; no structural changes required. boost/smart_ptr.hpp and include/private patterns are consistent across libs (boost needed wherever CraftingData.h or UniqueNameList.cpp are pulled in; include/private needed by any lib that exposes internal-only headers in that directory).

## Known Stubs

None — all CMakeLists files are build system configuration only. No UI or data path components.

## Threat Flags

None — this plan creates only CMake build system files. No network endpoints, auth paths, file access patterns, or schema changes introduced.

## Self-Check: PASSED

All files verified:
- `src/engine/shared/library/sharedUtility/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedUtility/src/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedNetworkMessages/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedNetworkMessages/src/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedLog/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedLog/src/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedNetwork/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedNetwork/src/CMakeLists.txt` — FOUND
- `build/lib/Debug/sharedUtility.lib` — FOUND
- `build/lib/Debug/sharedNetworkMessages.lib` — FOUND
- `build/lib/Debug/sharedLog.lib` — FOUND
- `build/lib/Debug/sharedNetwork.lib` — FOUND

All 2 task commits verified in git log:
- `fcfad51de` feat(02-05): sharedUtility + sharedNetworkMessages CMakeLists pairs — FOUND
- `a6e663cc7` feat(02-05): sharedLog + sharedNetwork CMakeLists pairs (third time_t probe) — FOUND

---
*Phase: 02-shared-engine-libraries*
*Completed: 2026-05-04*
