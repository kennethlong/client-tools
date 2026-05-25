---
plan: "07-03"
phase: 7
status: complete
completed: "2026-05-07"
---

# Summary: 07-03 — Vivox + XPCOM Source Removal

## What Was Built

Removed Vivox voice chat (CLEAN-03) and XPCOM/Mozilla browser (CLEAN-04) at the source level, with full CMake graph unlinks. The client now starts ~2x faster due to 16 DLLs eliminated from the Windows import table (12 Mozilla + 4 Vivox). Boot to character select and into world confirmed.

## Vivox Removal (Sub-step 3a)

### Source Files Deleted (23 files)

**clientUserInterface internal sources:**
- `CuiVoiceChatManager.cpp/.h`, `CuiVoiceChatEventHandler.cpp/.h`, `CuiVoiceChatGlue.h`
- `clientUserInterface/include/public/clientUserInterface/CuiVoiceChatManager.h`

**swgClientUserInterface sources:**
- `SwgCuiVoiceFlyBar.cpp/.h`, `SwgCuiVoiceFlyBarMessageQueue.cpp/.h`
- `SwgCuiVoiceActiveSpeakers.cpp/.h`, `SwgCuiVoiceActiveSpeakers_TableModel.cpp/.h`
- `SwgCuiCommandParserVoice.cpp/.h`, `SwgCuiOptVoice.cpp/.h`
- Public headers: `SwgCuiVoiceFlyBar.h`, `SwgCuiVoiceActiveSpeakers.h`, `SwgCuiVoiceActiveSpeakers_TableModel.h`, `SwgCuiCommandParserVoice.h`, `SwgCuiOptVoice.h`

**External library deleted:**
- `src/external/3rd/library/vivoxSharedWrapper/` (entire directory)

### Cascade Edits (12 files)

| File | Changes |
|------|---------|
| `CuiManagerManager.cpp` | Removed include, install/remove/update calls |
| `CuiPreferences.cpp` | Removed include, 6 ms_voice* declarations, 6 REGISTER_OPTION_USER(voice*), 4 CuiVoiceChatManager::set*() calls, 12 getter/setter implementations |
| `CuiPreferences.h` | Removed 12 voice getter/setter declarations |
| `CuiRadialMenuManager.cpp` | Removed include, isLoggedIn() call + if-block |
| `SwgCuiCommandParserDefault.cpp` | Removed include, addSubCommand(new SwgCuiCommandParserVoice) |
| `SwgCuiMediatorFactorySetup.cpp` | Removed 2 includes, 2 MAKE_SWG_CTOR_WS entries |
| `SwgCuiOpt.cpp` | Removed include, OT_voice page construction |
| `SwgCuiHudWindowManager.cpp` | Removed activateInWorkspace(WS_VoiceFlyBar) |
| `SwgCuiHudAction.cpp` | Removed include, toggleVoiceFlyBar action, 2 if-branches |
| `SwgCuiStatusGround.cpp` | Removed include, updateVoiceChatIcon() call + implementation |
| `SwgCuiActions.h` | Removed toggleVoiceFlyBar + toggleVoiceActiveSpeakers MAKE_ACTION entries |
| `SwgCuiMediatorTypes.h` | Removed WS_VoiceFlyBar + WS_VoiceActiveSpeakers MAKE_MEDIATOR_TYPE entries |

### CMake Edits
- `src/CMakeLists.txt`: removed `find_package(Vivox REQUIRED)`
- `clientUserInterface/src/CMakeLists.txt`: removed `${VIVOX_INCLUDE_DIRS}`, `${VIVOX_LIBRARIES}`, explicit `CuiVoiceChat*` source entries
- `swgClientUserInterface/src/CMakeLists.txt`: removed `${VIVOX_INCLUDE_DIRS}`, `${VIVOX_LIBRARIES}`
- `SwgClient/src/CMakeLists.txt`: removed `${VIVOX_LIBRARIES}`, 4 POST_BUILD DLL copies (vivoxsdk, vivoxplatform, ortp, alut)
- `FindVivox.cmake`: deleted

## XPCOM/Mozilla Removal (Sub-step 3b)

### Source Files Deleted (9 files + 2 support files)
- `SwgCuiWebBrowserManager.cpp/.h`, `SwgCuiWebBrowserWidget.cpp/.h`, `SwgCuiWebBrowserWindow.cpp/.h`
- Public headers: `SwgCuiWebBrowserManager.h`, `SwgCuiWebBrowserWidget.h`, `SwgCuiWebBrowserWindow.h`
- `src/cmake/stubs/libMozilla_stub.cpp` (deleted)
- `src/cmake/win32/FindMozilla.cmake` (deleted)

### Cascade Edits (8 files)

| File | Changes |
|------|---------|
| `SwgCuiManager.cpp` | Removed include, install/remove/update calls |
| `SwgCuiTcgManager.cpp` | Removed include, createWebBrowserPage/setURL calls |
| `SwgCuiHud.cpp` | Removed include |
| `SwgCuiHudAction.cpp` | Removed include, createWebBrowserPage/setURL calls |
| `SwgCuiCommandParserUI.cpp` | Removed include, browser command handlers |
| `CuiIoWin.cpp` | Removed mozilla include |
| `IMEManager.cpp` | Removed mozilla reference |
| `Game.cpp` | Removed libMozilla::init()/release() calls, include |

### CMake Edits
- `src/CMakeLists.txt`: removed `find_package(Mozilla REQUIRED)`
- `src/engine/client/library/CMakeLists.txt`: removed inline `add_library(libMozilla ...)` block (lines 5-17)
- `clientUserInterface/src/CMakeLists.txt`: removed `${MOZILLA_PUBLIC_INCLUDE_DIR}`, `libMozilla`
- `clientGame/src/CMakeLists.txt`: removed `${MOZILLA_PUBLIC_INCLUDE_DIR}`, `libMozilla`
- `swgClientUserInterface/src/CMakeLists.txt`: removed `SwgCuiWebBrowserManager.cpp/.h` explicit entries, `${MOZILLA_PUBLIC_INCLUDE_DIR}`, `libMozilla`
- `SwgClient/src/CMakeLists.txt`: removed `${SWG_EXTERNALS_FIND}/libMozilla/include`, `libMozilla`, 12 Mozilla DLL POST_BUILD copies, `copy_directory mozilla`

## Deviation: Task 7 — /Zc:wchar_t- Deferred to Phase 9

Removing `/Zc:wchar_t-` from `src/CMakeLists.txt` revealed that `unicode_char_t` (`unsigned short`) flows into `_wcsicmp`, `_wcsnicmp`, `_snwprintf`, L-literal constructors and `UIString` assignments across the `ui` library and `clientUserInterface`. This is the full Phase 9 STL-03 `unicode_char_t → wchar_t` migration — it cannot be done with point edits.

The flag remains in `src/CMakeLists.txt` lines 43-44 (both Debug and Release). STLPort's own `/Zc:wchar_t-` at `stlport453/src/CMakeLists.txt` is preserved (unchanged).

**Phase 9 scope update:** XPCOM is now gone (the only external consumer of the old wchar_t ABI), so Phase 9 STL-03 can proceed without the XPCOM constraint. The flag removal is now the first step of Phase 9 (paired with the unicode_char_t migration).

## Build Results

All three intermediate builds clean:
- After Task 3 (Vivox removed): exit 0
- After Task 6 (XPCOM removed): exit 0
- Final: `cmake --build build --config Debug --target SwgClient` → exit 0

`build/bin/Debug/SwgClient_d.exe` — rebuilt clean

## dumpbin Verification

`dumpbin /imports SwgClient_d.exe | Select-String '(xpcom|xul|nspr4|nss3|vivoxsdk|vivoxplatform|ortp|alut)'` → **zero matches**

## Boot Verify

Client launched against SWGSource StellaBellum VM at 192.168.1.200.  
**Result: approved** — character select reached, world load confirmed (~2x faster due to 16 DLLs removed from import table), no new crashes, no missing-DLL loader errors.

## Self-Check: PASSED (with noted deviation)

- [x] All 23 Vivox source files deleted; vivoxSharedWrapper deleted
- [x] All 9 XPCOM source files deleted; libMozilla_stub.cpp deleted; FindMozilla.cmake deleted
- [x] Inline add_library(libMozilla) block removed from engine/client/library/CMakeLists.txt
- [x] FindVivox.cmake deleted
- [x] 12 voice cascade files cleaned; 8 XPCOM cascade files cleaned
- [x] 6 CMakeLists.txt files updated for both removals
- [x] dumpbin shows zero Vivox/XPCOM imports
- [x] cmake --build Debug exits 0
- [x] Boot verify approved; world load confirmed
- [ ] /Zc:wchar_t- NOT removed from src/CMakeLists.txt — deferred to Phase 9 STL-03
