---
plan: "07-01"
phase: 7
status: complete
completed: "2026-05-07"
---

# Summary: 07-01 — Directory Deletes

## What Was Built

Deleted three orphaned directories and three pre-CU header stubs with zero live CMake impact, fixed one `include_directories()` reference, and replaced the resulting stub workaround with a clean source edit. Client builds and boots to character select.

## Files Deleted

| Path | Contents |
|------|----------|
| `src/external/3rd/library/trackIR/` | 1 file — `NPClient.h` header-only TrackIR SDK |
| `src/external/3rd/library/stationapi/` | 55 files — SOE Station authentication SDK |
| `src/game/client/application/SwgClientSetup/` | 55 files — standalone MFC launcher (no CMake target) |
| `src/game/client/library/swgClientUserInterface/include/public/swgClientUserInterface/SwgCuiResourceExtractionOld.h` | orphaned pre-CU header stub |
| `src/game/client/library/swgClientUserInterface/include/public/swgClientUserInterface/SwgCuiResourceExtractionOld_Hopper.h` | orphaned pre-CU header stub |
| `src/game/client/library/swgClientUserInterface/include/public/swgClientUserInterface/SwgCuiResourceExtractionOld_Quantity.h` | orphaned pre-CU header stub |

## CMake Edit

`src/engine/client/library/clientGame/src/CMakeLists.txt`: removed `${SWG_EXTERNALS_FIND}/trackIR/include` from `include_directories()`; excluded `ClientHeadTracking.cpp` from SHARED_SOURCES initially (executor), then replaced with inline edit (see deviation).

## Source Edits (deviation from plan — user-authorized)

**`ClientHeadTracking.cpp`**: Removed `#include "NPClient.h"` and all NPClient-dependent implementation. Replaced with minimal no-op install/remove/isSupported/getEnabled/setEnabled/getYawAndPitch. `ClientHeadTracking_stub.cpp` deleted.

**`SwgVideoCapture.cpp`**: Removed VideoCapture/AudioCapture/SoeUtil SDK includes. Replaced the PRODUCTION==0 implementation with no-op functions inline. `SwgVideoCapture_stub.cpp` deleted.

Both `src/cmake/stubs/` entries eliminated; real source files restored to their respective CMakeLists.txt source lists.

## Build Result

`cmake --build build --config Debug --target SwgClient` → exit 0  
`build/bin/Debug/SwgClient_d.exe` — 27.9 MB, built clean

No new errors. LNK4088 (`/FORCE:MULTIPLE`) is pre-existing STLPort shim — expected.

## Boot Verify

Client launched against SWGSource StellaBellum VM at 192.168.1.200.  
**Result: approved** — character select reached, no new crashes, no new missing-file dialogs.

## Self-Check: PASSED

- [x] trackIR, stationapi, SwgClientSetup absent from disk
- [x] Three SwgCuiResourceExtractionOld*.h headers absent
- [x] clientGame CMakeLists.txt has no trackIR reference
- [x] ClientHeadTracking.cpp and SwgVideoCapture.cpp compile without external SDK headers
- [x] No stub files remain in src/cmake/stubs/ except libMozilla (XPCOM, removed in 07-03)
- [x] cmake --build Debug exits 0
- [x] Boot verify approved by user
