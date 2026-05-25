# Phase 7: Dead Code Removal Track A — Pattern Map

**Mapped:** 2026-05-07
**Files analyzed:** 47 files to delete or modify across 3 steps
**Analogs found:** N/A — this is a deletion phase; patterns describe what currently depends on each
removed artifact so the planner knows what else to touch.

---

## Overview

Phase 7 removes dead, disabled, or stub-blocked code in three ordered steps:

1. **Directory deletes** — `trackIR/`, `stationapi/`, `SwgClientSetup/`
2. **CMake unlinks** — `lcdui_src`, `binkw32`+Bink source files, VideoCapture libs+cmake module
3. **Source removals** — Vivox voice chat cluster + XPCOM browser cluster + `/Zc:wchar_t-`

Each section below lists the files to delete/edit, their current CMake dependents, and any
hidden source-level dependents that must also be edited for the build to succeed.

---

## Step 1: Directory Deletes

### 1-A. `src/external/3rd/library/trackIR/`

**Contents:**
- `src/external/3rd/library/trackIR/include/NPClient.h` — sole file, header-only

**CMake references:**
- `src/engine/client/library/clientGame/src/CMakeLists.txt` line 817:
  `${SWG_EXTERNALS_FIND}/trackIR/include` in `include_directories()`

**Source-level dependents** (files that `#include "NPClient.h"`):
- `src/engine/client/library/clientGame/src/shared/core/ClientHeadTracking.cpp`
- `src/game/client/application/SwgClientSetup/src/win32/ClientMachine.cpp`
- `src/game/client/application/SwgClientSetup/src/win32/ClientMachine.h`
- `src/game/client/application/SwgClientSetup/src/win32/PageInformation.cpp`
- `src/game/client/application/SwgClientSetup/src/win32/PageInformation.h`
- `src/game/client/application/SwgClientSetup/src/win32/SwgClientSetup.rc`

Note: `SwgClientSetup` is itself deleted in step 1-C, removing those dependents.
`ClientHeadTracking.cpp` remains in clientGame; its `#include "NPClient.h"` will become
a missing header unless guarded. The file is listed in
`src/engine/client/library/clientGame/src/CMakeLists.txt` lines 161-162 and must either
be excluded from the CMake source list or the include guarded at source level.

**Other references to `ClientHeadTracking`:**
- `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiOptControls.cpp`
  (includes `clientGame/ClientHeadTracking.h` — must be patched if ClientHeadTracking is removed)
- `src/engine/client/library/clientGame/src/shared/core/SetupClientGame.cpp`
- `src/engine/client/library/clientGame/src/shared/camera/CockpitCamera.cpp`

**Action required in CMake:**
- Remove `${SWG_EXTERNALS_FIND}/trackIR/include` from
  `src/engine/client/library/clientGame/src/CMakeLists.txt` line 817.

---

### 1-B. `src/external/3rd/library/stationapi/`

**Contents:** 24 files — `.cpp`, `.h`, `.lib`, `.dsp`, `.dsw`, demo subdirectory. No CMakeLists.txt.

**CMake references:** None found. No `CMakeLists.txt` anywhere references `stationapi`.

**Source-level dependents:** None found in `src/engine/` or `src/game/`.

**Action required:** Directory delete only — no CMake or source edits needed.

---

### 1-C. `src/game/client/application/SwgClientSetup/`

**Contents:** `src/win32/` directory — 34 `.cpp`/`.h` files plus `.rc` and `res/` subdirectory.
SwgClientSetup is a standalone Win32 MFC setup launcher for the SOE era — not compiled
by the modern CMake build.

**CMake references:** None. No `add_subdirectory(SwgClientSetup)` anywhere in any CMakeLists.txt.

**Source-level dependents:**
- Internally includes `NPClient.h` (trackIR) and various MFC/Windows headers — but this whole
  tree is self-contained and not linked from SwgClient.

**Action required:** Directory delete only — no CMake or source edits needed.

---

## Step 2: CMake Unlinks

### 2-A. `lcdui_src` library

**CMake files to edit:**

| File | Lines | Change |
|------|-------|--------|
| `src/external/3rd/library/CMakeLists.txt` | line 9 | Remove `add_subdirectory(lcdui_src)` |
| `src/game/client/application/SwgClient/src/CMakeLists.txt` | line 158 | Remove `lcdui_src` from `target_link_libraries` |
| `src/game/client/application/SwgClient/src/CMakeLists.txt` | lines 123-124 | Remove two `${SWG_EXTERNALS_FIND}/lcdui_src/src/win32` include_directories entries |
| `src/game/client/library/swgClientUserInterface/src/CMakeLists.txt` | lines 123-124 | Remove two `lcdui_src` include_directories entries |
| `src/game/client/library/swgClientUserInterface/src/CMakeLists.txt` | line 143 | Remove `${LOGITECHLCD_LIBRARY}` from target_link_libraries |
| `src/game/client/application/SwgClient/src/CMakeLists.txt` | line 164 | Remove `${LOGITECHLCD_LIBRARY}` from target_link_libraries |
| `src/engine/client/library/clientUserInterface/src/CMakeLists.txt` | line 403 | Remove `${LOGITECHLCD_INCLUDE_DIR}` from include_directories |
| `src/engine/client/library/clientUserInterface/src/CMakeLists.txt` | line 419 | Remove `${LOGITECHLCD_LIBRARY}` from target_link_libraries |
| `src/CMakeLists.txt` | line 59 | Remove `find_package(LogitechLCD REQUIRED)` |

**Companion cmake module:** `src/cmake/win32/FindLogitechLCD.cmake` — delete this file.

**Source-level dependents of lcdui:** The lcdui library is linked into SwgClient,
swgClientUserInterface, and clientUserInterface. No C++ source files in the scanned trees
directly `#include` LCDUI headers outside the `lcdui_src` subdirectory itself. The include
paths pulled in from `LOGITECHLCD_INCLUDE_DIR` point to `src/external/3rd/library/lcdui/`
(the prebuilt `.lib` + `lglcd.h`). Verify no remaining `#include "lglcd.h"` after removal.

---

### 2-B. Bink / `binkw32`

**CMake files to edit:**

| File | Lines | Change |
|------|-------|--------|
| `src/engine/client/library/clientGraphics/src/CMakeLists.txt` | lines 3-8 | Remove 6 Bink source file entries from `SHARED_SOURCES` |
| `src/engine/client/library/clientGraphics/src/CMakeLists.txt` | line 166 | Remove `${CMAKE_CURRENT_SOURCE_DIR}/Bink` from include_directories |
| `src/engine/client/library/clientGraphics/src/CMakeLists.txt` | line 186 | Remove `${BINK_INCLUDE_DIR}` from include_directories |
| `src/game/client/application/SwgClient/src/CMakeLists.txt` | line 208 | Remove the `binkw32.dll` POST_BUILD copy_if_different line |
| `src/CMakeLists.txt` | line 54 | Remove `find_package(Bink REQUIRED)` |

**Companion cmake module:** `src/cmake/win32/FindBink.cmake` — delete this file.

**Source-level dependents of Bink headers** (files that `#include` Bink headers —
these live inside the `Bink/` subdirectory being deleted, so they self-eliminate):
- `src/engine/client/library/clientGraphics/src/Bink/BinkDLL.cpp`
- `src/engine/client/library/clientGraphics/src/Bink/BinkVideo.cpp`
- `src/engine/client/library/clientGraphics/src/Bink/BinkTreeFileIO.cpp`

Verify no `#include "Bink/BinkVideo.h"` or `#include "clientGraphics/BinkVideo.h"` remain
in other files in clientGraphics or clientGame after removal.

---

### 2-C. VideoCapture libs + `FindVideoCapture.cmake`

**CMake files to edit:**

| File | Lines | Change |
|------|-------|--------|
| `src/engine/client/library/clientGraphics/src/CMakeLists.txt` | line 818 | Remove `${VIDEOCAPTURE_ROOT}` from include_directories (clientGame) |
| `src/CMakeLists.txt` | line 60 | Remove `find_package(VideoCapture REQUIRED)` |
| `src/game/client/application/SwgClient/src/CMakeLists.txt` | line 162 | Remove `${VIDEOCAPTURE_LIBRARY}` from target_link_libraries |

**Companion cmake module:** `src/cmake/win32/FindVideoCapture.cmake` — delete this file.

**Stub file that replaces real implementation:**
`src/cmake/stubs/SwgVideoCapture_stub.cpp` — this stub was added in Phase 4 (D-13) to
avoid compiling `SwgVideoCapture.cpp` (which requires `VideoCapture/VideoCapture.h`).
After removing `find_package(VideoCapture)`, the stub remains in the build
(it only includes `clientGraphics/SwgVideoCapture.h` which is kept).
The `VIDEOCAPTURE_LIBRARY` variable in `SwgClient`'s `target_link_libraries` (line 162)
will expand to empty/missing — removing the line is the correct fix.

**Note:** `src/engine/client/library/clientGraphics/src/CMakeLists.txt` line 818 is under
`clientGame`'s `include_directories` (`${VIDEOCAPTURE_ROOT}` = the videocapture sdk root),
not clientGraphics. The variable `VIDEOCAPTURE_ROOT` is set inside `FindVideoCapture.cmake`
as `${SWG_EXTERNALS_FIND}/videocapture`. After removing the find_package call this variable
becomes undefined — remove the reference from clientGame's CMakeLists.txt line 818.

---

## Step 3a: Vivox Voice Chat Removal

### Voice-specific source files to DELETE

**In `src/engine/client/library/clientUserInterface/`:**
- `src/shared/core/CuiVoiceChatManager.cpp`
- `src/shared/core/CuiVoiceChatManager.h`
- `src/shared/core/CuiVoiceChatEventHandler.cpp`
- `src/shared/core/CuiVoiceChatEventHandler.h`
- `src/shared/core/CuiVoiceChatGlue.h`
- `include/public/clientUserInterface/CuiVoiceChatManager.h`

**In `src/game/client/library/swgClientUserInterface/`:**
- `src/shared/page/SwgCuiVoiceFlyBar.cpp`
- `src/shared/page/SwgCuiVoiceFlyBar.h`
- `src/shared/page/SwgCuiVoiceFlyBarMessageQueue.cpp`
- `src/shared/page/SwgCuiVoiceFlyBarMessageQueue.h`
- `src/shared/page/SwgCuiVoiceActiveSpeakers.cpp`
- `src/shared/page/SwgCuiVoiceActiveSpeakers.h`
- `src/shared/page/SwgCuiVoiceActiveSpeakers_TableModel.cpp`
- `src/shared/page/SwgCuiVoiceActiveSpeakers_TableModel.h`
- `src/shared/parser/SwgCuiCommandParserVoice.cpp`
- `src/shared/parser/SwgCuiCommandParserVoice.h`
- `src/shared/page/SwgCuiOptVoice.cpp`
- `src/shared/page/SwgCuiOptVoice.h`
- `include/public/swgClientUserInterface/SwgCuiVoiceFlyBar.h`
- `include/public/swgClientUserInterface/SwgCuiVoiceActiveSpeakers.h`
- `include/public/swgClientUserInterface/SwgCuiVoiceActiveSpeakers_TableModel.h`
- `include/public/swgClientUserInterface/SwgCuiCommandParserVoice.h`
- `include/public/swgClientUserInterface/SwgCuiOptVoice.h`

**vivoxSharedWrapper library to DELETE:**
`src/external/3rd/library/vivoxSharedWrapper/` — entire directory:
- `Eq2Vivox.h`, `Vivox.cpp`, `Vivox.h`, `Vivox.inl`
- `include/vivoxSharedWrapper/Vivox.h`
- `lib/vivoxSharedWrapper_Debug.lib`
- `lib/vivoxSharedWrapper_Release.lib`

### Voice CMake edits

| File | Lines | Change |
|------|-------|--------|
| `src/CMakeLists.txt` | line 57 | Remove `find_package(Vivox REQUIRED)` |
| `src/engine/client/library/clientUserInterface/src/CMakeLists.txt` | line 402 | Remove `${VIVOX_INCLUDE_DIRS}` from include_directories |
| `src/engine/client/library/clientUserInterface/src/CMakeLists.txt` | line 418 | Remove `${VIVOX_LIBRARIES}` from target_link_libraries |
| `src/game/client/library/swgClientUserInterface/src/CMakeLists.txt` | line 121 | Remove `${VIVOX_INCLUDE_DIRS}` from include_directories |
| `src/game/client/library/swgClientUserInterface/src/CMakeLists.txt` | line 142 | Remove `${VIVOX_LIBRARIES}` from target_link_libraries |
| `src/game/client/application/SwgClient/src/CMakeLists.txt` | line 163 | Remove `${VIVOX_LIBRARIES}` from target_link_libraries |
| `src/game/client/application/SwgClient/src/CMakeLists.txt` | lines 214-218 | Remove `vivoxsdk.dll`, `vivoxplatform.dll`, `ortp.dll`, `alut.dll` POST_BUILD copies |

**Companion cmake module:** `src/cmake/win32/FindVivox.cmake` — delete this file.

### Voice source-level cascade edits (files that include or call voice APIs)

These files are NOT deleted — they are edited to remove voice references:

**`src/engine/client/library/clientUserInterface/src/shared/core/CuiManagerManager.cpp`**
- Line 57: Remove `#include "clientUserInterface/CuiVoiceChatManager.h"`
- Lines 112, 164, 219: Remove `CuiVoiceChatManager::install()`, `::remove()`, `::update()` calls

**`src/engine/client/library/clientUserInterface/src/shared/core/CuiPreferences.cpp`**
- Line 22: Remove `#include "clientUserInterface/CuiVoiceChatManager.h"`
- Lines 390-394: Remove 6 `ms_voice*` module-scope bool declarations
- Lines 839-844: Remove 6 `REGISTER_OPTION_USER(voice*)` calls
- Lines 866-869: Remove 4 `CuiVoiceChatManager::set*()` calls
- Lines 3394-3473: Remove all 12 `getVoice*/setVoice*` getter/setter implementations

**`src/engine/client/library/clientUserInterface/src/shared/core/CuiPreferences.h`**
- Lines 535-551: Remove 12 voice getter/setter declarations

**`src/engine/client/library/clientUserInterface/src/shared/core/CuiRadialMenuManager.cpp`**
- Line 60: Remove `#include "clientUserInterface/CuiVoiceChatManager.h"`
- Line 1182: Remove `CuiVoiceChatManager::isLoggedIn()` call (guard the if-branch or remove)

**`src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserDefault.cpp`**
- Line 77: Remove `#include "swgClientUserInterface/SwgCuiCommandParserVoice.h"`
- Line 442: Remove `addSubCommand(new SwgCuiCommandParserVoice)`

**`src/game/client/library/swgClientUserInterface/src/shared/core/SwgCuiMediatorFactorySetup.cpp`**
- Lines 133-134: Remove `#include` of `SwgCuiVoiceActiveSpeakers.h` and `SwgCuiVoiceFlyBar.h`
- Lines 260-261: Remove `MAKE_SWG_CTOR_WS(VoiceFlyBar, ...)` and `MAKE_SWG_CTOR_WS(VoiceActiveSpeakers, ...)`

**`src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiOpt.cpp`**
- Line 32: Remove `#include "swgClientUserInterface/SwgCuiOptVoice.h"`
- Line 113: Remove `(*m_optionPages)[OT_voice] = new SwgCuiOptVoice(*optionPage)` line

**`src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHudWindowManager.cpp`**
- Line 320: Remove `CuiMediatorFactory::activateInWorkspace(CuiMediatorTypes::WS_VoiceFlyBar)` call

**`src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHudAction.cpp`**
- Line 111: Remove `#include "swgClientUserInterface/SwgCuiVoiceFlyBar.h"`
- Line 286: Remove `CuiActionManager::addAction(SwgCuiActions::toggleVoiceFlyBar, ...)` call
- Lines 1587-1594: Remove both `toggleVoiceFlyBar` and `toggleVoiceActiveSpeakers` if-branches

**`src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiStatusGround.cpp`**
- Line 36: Remove `#include "clientUserInterface/CuiVoiceChatManager.h"`
- Line 1228: Remove `updateVoiceChatIcon(*tangible)` call
- Lines 2063-2084: Remove entire `updateVoiceChatIcon()` method implementation

**`src/game/client/library/swgClientUserInterface/src/shared/core/SwgCuiActions.h`**
- Lines 69-70: Remove `MAKE_ACTION(toggleVoiceFlyBar)` and `MAKE_ACTION(toggleVoiceActiveSpeakers)`

**`src/game/client/library/swgClientUserInterface/src/shared/core/SwgCuiMediatorTypes.h`**
- Lines 104-105: Remove `MAKE_MEDIATOR_TYPE(WS_VoiceFlyBar)` and `MAKE_MEDIATOR_TYPE(WS_VoiceActiveSpeakers)`

### swgClientUserInterface CMakeLists voice entries

The `swgClientUserInterface/src/CMakeLists.txt` uses:
- `file(GLOB SHARED_PARSER_SOURCES ...)` — globs all parser `.cpp`/`.h` — after deleting
  `SwgCuiCommandParserVoice.*` files the glob will automatically exclude them.
- `file(GLOB_RECURSE SHARED_PAGE_SOURCES ...)` — globs all page `.cpp`/`.h` — after deleting
  all `SwgCuiVoice*` and `SwgCuiOptVoice*` files the glob will automatically exclude them.
- `set(SHARED_CORE_SOURCES ...)` — explicit list; does NOT include any voice files. No edit needed.

The clientUserInterface CMakeLists uses explicit file lists — search for `CuiVoiceChat*` entries
and remove them. (Verified: `CuiVoiceChatEventHandler.cpp/.h` appear in the source list but
`CuiVoiceChatManager.cpp/.h` may be globbed — check the full clientUserInterface CMakeLists
for explicit entries.)

---

## Step 3b: XPCOM / Mozilla Browser Removal

### Browser source files to DELETE

**In `src/game/client/library/swgClientUserInterface/`:**
- `src/shared/core/SwgCuiWebBrowserManager.cpp`
- `src/shared/core/SwgCuiWebBrowserManager.h`
- `src/shared/page/SwgCuiWebBrowserWidget.cpp`
- `src/shared/page/SwgCuiWebBrowserWidget.h`
- `src/shared/page/SwgCuiWebBrowserWindow.cpp`
- `src/shared/page/SwgCuiWebBrowserWindow.h`
- `include/public/swgClientUserInterface/SwgCuiWebBrowserManager.h`
- `include/public/swgClientUserInterface/SwgCuiWebBrowserWidget.h`
- `include/public/swgClientUserInterface/SwgCuiWebBrowserWindow.h`

**Orphaned public headers (no corresponding `.cpp` in `src/`) — also DELETE:**
- `src/game/client/library/swgClientUserInterface/include/public/swgClientUserInterface/SwgCuiResourceExtractionOld.h`
- `src/game/client/library/swgClientUserInterface/include/public/swgClientUserInterface/SwgCuiResourceExtractionOld_Hopper.h`
- `src/game/client/library/swgClientUserInterface/include/public/swgClientUserInterface/SwgCuiResourceExtractionOld_Quantity.h`

**Stub files to DELETE (XPCOM is fully removed, stub no longer needed):**
- `src/cmake/stubs/libMozilla_stub.cpp`

**The `libMozilla` cmake target itself** is defined inline in
`src/engine/client/library/CMakeLists.txt` lines 5-17 — these lines must be removed.

### Browser CMake edits

| File | Lines | Change |
|------|-------|--------|
| `src/CMakeLists.txt` | line 56 | Remove `find_package(Mozilla REQUIRED)` |
| `src/engine/client/library/CMakeLists.txt` | lines 5-17 | Remove `add_library(libMozilla ...)` block |
| `src/engine/client/library/clientUserInterface/src/CMakeLists.txt` | line 400 | Remove `${MOZILLA_PUBLIC_INCLUDE_DIR}` from include_directories |
| `src/engine/client/library/clientUserInterface/src/CMakeLists.txt` | line 417 | Remove `libMozilla` from target_link_libraries |
| `src/engine/client/library/clientGame/src/CMakeLists.txt` | line 810 | Remove `${MOZILLA_PUBLIC_INCLUDE_DIR}` from include_directories |
| `src/engine/client/library/clientGame/src/CMakeLists.txt` | line 828 | Remove `libMozilla` from target_link_libraries |
| `src/game/client/library/swgClientUserInterface/src/CMakeLists.txt` | line 119 | Remove `${MOZILLA_PUBLIC_INCLUDE_DIR}` from include_directories |
| `src/game/client/library/swgClientUserInterface/src/CMakeLists.txt` | line 141 | Remove `libMozilla` from target_link_libraries |
| `src/game/client/application/SwgClient/src/CMakeLists.txt` | line 71 | Remove `${SWG_EXTERNALS_FIND}/libMozilla/include` from include_directories |
| `src/game/client/application/SwgClient/src/CMakeLists.txt` | line 111 | Remove `libMozilla` from target_link_libraries |
| `src/game/client/application/SwgClient/src/CMakeLists.txt` | lines 226-274 | Remove all Mozilla DLL POST_BUILD copies (`xpcom.dll`, `xul.dll`, `nspr4.dll`, `plc4.dll`, `plds4.dll`, `nss3.dll`, `nssckbi.dll`, `smime3.dll`, `softokn3.dll`, `ssl3.dll`, `js3250.dll`, `gksvggdiplus.dll`) AND the `copy_directory mozilla` line |

**Companion cmake module:** `src/cmake/win32/FindMozilla.cmake` — delete this file.

### Browser source-level cascade edits (files that include or call browser APIs)

**`src/game/client/library/swgClientUserInterface/src/shared/core/SwgCuiManager.cpp`**
- Line 72: Remove `#include "swgClientUserInterface/SwgCuiWebBrowserManager.h"`
- Lines 474, 503, 551: Remove `SwgCuiWebBrowserManager::install()`, `::remove()`, `::update()` calls

**`src/game/client/library/swgClientUserInterface/src/shared/core/SwgCuiTcgManager.cpp`**
- Line 19: Remove `#include "swgClientUserInterface/SwgCuiWebBrowserManager.h"`
- Lines 130-148: Remove all `SwgCuiWebBrowserManager::createWebBrowserPage()` and `::setURL()` calls

**`src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHud.cpp`**
- Line 98: Remove `#include "swgClientUserInterface/SwgCuiWebBrowserManager.h"`

**`src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHudAction.cpp`**
- Line 110: Remove `#include "swgClientUserInterface/SwgCuiWebBrowserManager.h"`
- Lines 1185-1186: Remove `SwgCuiWebBrowserManager::createWebBrowserPage()` and `::setURL()` calls

**`src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserUI.cpp`**
- Line 58: Remove `#include "swgClientUserInterface/SwgCuiWebBrowserManager.h"`
- Lines 564-575, 1089: Remove all `SwgCuiWebBrowserManager::*` call sites

**`src/engine/client/library/clientUserInterface/src/shared/core/CuiIoWin.cpp`**
- Has `mozilla` or `WebBrowser` include — check and remove (confirmed by grep hit).

**`src/engine/client/library/clientUserInterface/src/shared/core/IMEManager.cpp`**
- Has `mozilla` or `WebBrowser` include — check and remove (confirmed by grep hit).

**`src/engine/client/library/clientGame/src/shared/core/Game.cpp`**
- Has `mozilla` or `WebBrowser` include — check and remove (confirmed by grep hit).

### swgClientUserInterface CMakeLists browser entries

`SwgCuiWebBrowserManager.cpp/.h` is listed **explicitly** in `SHARED_CORE_SOURCES`
at lines 38-39 of `swgClientUserInterface/src/CMakeLists.txt`. These two lines must be
removed manually.

`SwgCuiWebBrowserWidget.*` and `SwgCuiWebBrowserWindow.*` are in `shared/page/` and
picked up by `file(GLOB_RECURSE SHARED_PAGE_SOURCES ...)` — deleting the files removes them
from the build automatically.

---

## Step 3c: Remove `/Zc:wchar_t-` from Root CMakeLists

**File:** `src/CMakeLists.txt`

**Exact locations:**
- Line 43 in `CMAKE_CXX_FLAGS_DEBUG` — `/Zc:wchar_t-` appears inside the long flags string
- Line 44 in `CMAKE_CXX_FLAGS_RELEASE` — `/Zc:wchar_t-` appears inside the long flags string

The flag must be removed from both strings. The STLPort target at
`src/external/3rd/library/stlport453/src/CMakeLists.txt` line 26 also sets `/Zc:wchar_t-`
as a `target_compile_options` on the STLPort target — that entry is part of STLPort's own
ABI requirements and must be left in place.

**Prerequisite:** `/Zc:wchar_t-` can only be removed after all XPCOM/Mozilla sources are gone,
because `libMozilla` and the XPCOM headers require `wchar_t` to be `unsigned short` (the old
non-builtin ABI). Once `libMozilla` is fully unlinked and Mozilla headers no longer included,
the project-wide `/Zc:wchar_t-` becomes unnecessary. STLPort itself still needs it in its own
translation units, but the game code compiled against MSVC STL does not.

---

## File Classification Table

| File / Path | Type | Step | Dependents That Must Also Change |
|---|---|---|---|
| `src/external/3rd/library/trackIR/` | directory delete | 1-A | clientGame CMakeLists (1 include path), `ClientHeadTracking.cpp` (source edit or exclusion) |
| `src/external/3rd/library/stationapi/` | directory delete | 1-B | None |
| `src/game/client/application/SwgClientSetup/` | directory delete | 1-C | None (not in CMake build) |
| `src/cmake/win32/FindLogitechLCD.cmake` | cmake module delete | 2-A | `src/CMakeLists.txt` line 59 |
| `src/cmake/win32/FindBink.cmake` | cmake module delete | 2-B | `src/CMakeLists.txt` line 54 |
| `src/cmake/win32/FindVideoCapture.cmake` | cmake module delete | 2-C | `src/CMakeLists.txt` line 60 |
| `src/cmake/win32/FindVivox.cmake` | cmake module delete | 3a | `src/CMakeLists.txt` line 57 |
| `src/cmake/win32/FindMozilla.cmake` | cmake module delete | 3b | `src/CMakeLists.txt` line 56 |
| `src/external/3rd/library/vivoxSharedWrapper/` | directory delete | 3a | FindVivox.cmake references it |
| `src/cmake/stubs/libMozilla_stub.cpp` | stub delete | 3b | Inline `add_library(libMozilla ...)` in engine/client/library/CMakeLists.txt |
| `clientUserInterface/CuiVoiceChatManager.cpp/.h` | source delete | 3a | CuiManagerManager, CuiPreferences, CuiRadialMenuManager |
| `clientUserInterface/CuiVoiceChatEventHandler.cpp/.h/.inl` | source delete | 3a | included by CuiVoiceChatManager |
| `clientUserInterface/CuiVoiceChatGlue.h` | header delete | 3a | included by CuiVoiceChatManager |
| `clientUserInterface/include/.../CuiVoiceChatManager.h` | public header delete | 3a | SwgCuiStatusGround, SwgCuiOptVoice |
| `swgClientUserInterface/SwgCuiVoiceFlyBar.*` | source delete | 3a | SwgCuiMediatorFactorySetup, SwgCuiHudAction, SwgCuiHudWindowManager |
| `swgClientUserInterface/SwgCuiVoiceFlyBarMessageQueue.*` | source delete | 3a | SwgCuiVoiceFlyBar |
| `swgClientUserInterface/SwgCuiVoiceActiveSpeakers.*` | source delete | 3a | SwgCuiMediatorFactorySetup |
| `swgClientUserInterface/SwgCuiVoiceActiveSpeakers_TableModel.*` | source delete | 3a | SwgCuiVoiceActiveSpeakers |
| `swgClientUserInterface/SwgCuiCommandParserVoice.*` | source delete | 3a | SwgCuiCommandParserDefault |
| `swgClientUserInterface/SwgCuiOptVoice.*` | source delete | 3a | SwgCuiOpt |
| `swgClientUserInterface/SwgCuiWebBrowserManager.*` | source delete | 3b | SwgCuiManager, SwgCuiTcgManager, SwgCuiHud, SwgCuiHudAction, SwgCuiCommandParserUI |
| `swgClientUserInterface/SwgCuiWebBrowserWidget.*` | source delete | 3b | SwgCuiWebBrowserWindow |
| `swgClientUserInterface/SwgCuiWebBrowserWindow.*` | source delete | 3b | SwgCuiWebBrowserManager |
| `swgClientUserInterface/SwgCuiResourceExtractionOld*.h` | orphan header delete | 3b | None (not included anywhere) |
| `src/CMakeLists.txt` lines 43-44 `/Zc:wchar_t-` | cmake edit | 3c | Prerequisite: all Mozilla removed first |

---

## Shared Patterns for the Planner

### Pattern A — CMake `find_package` removal

Removing a `find_package(X REQUIRED)` from `src/CMakeLists.txt` also requires:
1. Deleting the corresponding `src/cmake/win32/FindX.cmake` file.
2. Removing every `${X_INCLUDE_DIR}` / `${X_LIBRARIES}` / `${X_LIBRARY}` from all downstream
   `include_directories()` and `target_link_libraries()` calls in child CMakeLists.txt files.
3. Removing any POST_BUILD `copy_if_different` or `copy_directory` that stages the SDK's runtime
   DLLs. These live in `src/game/client/application/SwgClient/src/CMakeLists.txt`.

Pattern example already established: compare FindBink, FindVivox, FindMozilla, FindLogitechLCD
structures — they all follow the same `find_path` + `find_library` + `find_package_handle_standard_args`
+ `mark_as_advanced` template. Removing them is symmetric.

### Pattern B — Inline cmake target removal (`add_library` inside CMakeLists)

`libMozilla` is defined **inline** (not via `add_subdirectory`) in
`src/engine/client/library/CMakeLists.txt` lines 5-17. Delete those 13 lines and also
delete `src/cmake/stubs/libMozilla_stub.cpp`. This is unlike the `lcdui_src` case where the
target lives in its own subdirectory CMakeLists.

### Pattern C — GLOB-sourced vs explicit-sourced CMakeLists

`swgClientUserInterface` uses:
- **Explicit list** for `SHARED_CORE_SOURCES` (edit required for WebBrowserManager files)
- **`file(GLOB)` for `SHARED_PARSER_SOURCES`** (delete files = automatic removal from build)
- **`file(GLOB_RECURSE)` for `SHARED_PAGE_SOURCES`** (delete files = automatic removal from build)

`clientUserInterface` uses explicit file lists for all sources — must manually remove
`CuiVoiceChatEventHandler.cpp/.h` entries.

### Pattern D — Voice preference key removal in CuiPreferences

Six `ms_voice*` bool variables + six `REGISTER_OPTION_USER(voice*)` calls + four
`CuiVoiceChatManager::set*()` calls in `install()` + twelve voice getter/setter methods.
The `CuiPreferences.h` public declarations of those twelve getters/setters must also go.
No other file calls the `getVoice*/setVoice*` methods except through `CuiPreferences`; the
callers that went through `CuiVoiceChatManager` directly are already handled in the cascade edits.

---

## Metadata

**Analog search scope:** `src/engine/`, `src/game/`, `src/cmake/`, `src/external/3rd/library/`
**Files scanned:** ~90 CMakeLists.txt + source files
**Key cmake modules found:** FindBink, FindVideoCapture, FindVivox, FindMozilla, FindLogitechLCD
**Glob patterns confirmed:** swgClientUserInterface page/ and parser/ use GLOB/GLOB_RECURSE; file deletion suffices for those.
**Pattern extraction date:** 2026-05-07
