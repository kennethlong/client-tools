---
plan: "07-02"
phase: 7
status: complete
completed: "2026-05-07"
---

# Summary: 07-02 — CMake Unlinks

## What Was Built

Severed three config-disabled middleware features (lcdui_src/G15 LCD, Bink video, VideoCapture) from the CMake graph. Deleted their three Find*.cmake modules. Cleaned six downstream CMakeLists.txt files of all include-dir, link-library, and POST_BUILD references. Applied inline source fixes for four planning gaps. Build exits 0; boot verified.

## Find Modules Deleted

| Module | Purpose |
|--------|---------|
| `src/cmake/win32/FindLogitechLCD.cmake` | Logitech G15 LCD support |
| `src/cmake/win32/FindBink.cmake` | Bink video codec |
| `src/cmake/win32/FindVideoCapture.cmake` | Debug-only video capture |

## CMakeLists.txt Files Modified

| File | Change |
|------|--------|
| `src/CMakeLists.txt` | Removed `find_package(LogitechLCD/Bink/VideoCapture REQUIRED)` |
| `src/external/3rd/library/CMakeLists.txt` | Removed `add_subdirectory(lcdui_src)` |
| `src/engine/client/library/clientUserInterface/src/CMakeLists.txt` | Removed `${LOGITECHLCD_INCLUDE_DIR}`, `${LOGITECHLCD_LIBRARY}` |
| `src/engine/client/library/clientAudio/src/CMakeLists.txt` | Removed `${VIDEOCAPTURE_ROOT}`, `${VIDEOCAPTURE_LIBRARY}` (planning gap — not in plan) |
| `src/engine/client/library/clientGraphics/src/CMakeLists.txt` | Removed 6 Bink source entries, `${CMAKE_CURRENT_SOURCE_DIR}/Bink`, `${BINK_INCLUDE_DIR}`, `${VIDEOCAPTURE_ROOT}` |
| `src/engine/client/library/clientGame/src/CMakeLists.txt` | Removed `${VIDEOCAPTURE_ROOT}` |
| `src/game/client/library/swgClientUserInterface/src/CMakeLists.txt` | Removed lcdui_src includes, `${LOGITECHLCD_LIBRARY}` |
| `src/game/client/application/SwgClient/src/CMakeLists.txt` | Removed lcdui_src includes, `${LOGITECHLCD_LIBRARY}`, `${VIDEOCAPTURE_LIBRARY}`, binkw32.dll POST_BUILD copy |

## Source Cascade Fixes (planning gaps — Rule 1 auto-fixes, inline edits per user policy)

| File | Fix |
|------|-----|
| `clientGraphics/src/shared/VideoList.cpp` | Removed `#include "BinkVideo.h"`, stubbed Bink calls inline |
| `clientAudio/src/shared/SwgAudioCapture.h/.cpp` | Removed `AudioCapture/AudioCapture.h` SDK includes, stubbed inline |
| `clientAudio/src/shared/Audio.cpp` | Removed `SwgAudioCaptureManager` calls |
| `swgClientUserInterface/.../SwgCuiG15Lcd.cpp` | Removed `EZ_LCD.h` include, removed `#define USE_LCD` |
| `swgClientUserInterface/.../SwgCuiHud.cpp` | Removed `EZ_LCD.h` include |

## Preservation

- `src/engine/client/library/clientGraphics/src/Bink/` — source files preserved on disk, not deleted (per CLEAN-02 scope)
- `src/external/3rd/library/bink/`, `src/external/3rd/library/videocapture/`, `src/external/3rd/library/lcdui/` — vendored SDKs preserved
- `src/cmake/stubs/SwgVideoCapture_stub.cpp` — already deleted in 07-01 (real source edit applied)

## Build Result

`cmake --build build --config Debug --target SwgClient` → exit 0  
`build/bin/Debug/SwgClient_d.exe` — 27.9 MB, built clean  
`build/bin/Debug/binkw32.dll` — stale artifact (POST_BUILD copy removed; will not be re-staged)

## Boot Verify

Client launched against SWGSource StellaBellum VM at 192.168.1.200.  
**Result: approved** — character select reached, no new crashes or missing-DLL dialogs.

## Self-Check: PASSED

- [x] FindLogitechLCD.cmake, FindBink.cmake, FindVideoCapture.cmake deleted
- [x] find_package(LogitechLCD|Bink|VideoCapture REQUIRED) absent from src/CMakeLists.txt
- [x] All ${LOGITECHLCD_*}, ${BINK_*}, ${VIDEOCAPTURE_*} references removed from CMake graph
- [x] binkw32.dll POST_BUILD copy removed
- [x] SwgVideoCapture_stub.cpp absent (replaced by inline edit in 07-01)
- [x] No new stubs created — all cascade fixes applied inline
- [x] cmake --build Debug exits 0
- [x] Boot verify approved by user
