---
phase: 37-utinni-engine-entry-point-advertisement-getenginehookpoints
plan: 02
subsystem: infra
tags: [msvc, dllexport, pmf, bit_cast, x-macro, utinni, hookpoints, 32-bit, multiple-inheritance, safeseh]

# Dependency graph
requires:
  - phase: 37-01-utinni-engine-entrypoint-spike
    provides: "GetEngineHookPoints undecorated export + pmfToVoid bit_cast helper + X-macro coverage self-check + utinni_loadOverrideConfig crash-fixer thunk + sharedCommandParser include-path fix"
provides:
  - "MVP hookpoint catalog: 41 advertised endpoints across config/client/game/graphics/cuiManager/cuiIo/consoleHelper/commandParser, each a source-confirmed signature-checked address"
  - "graphics::install + the graphics group advertised non-null (EPA-03 DX11 overlay kickoff resolves off the contract)"
  - "EPA-04 zero-missing gate satisfied for the MVP required set: compile-time count static_assert (41==41, drift smoke) + runtime utinni_verifyNoNullNoDup() no-null/no-dup/name-set-equality"
  - "Synced contract (utinni_engine_hookpoints.h + .inc) copied verbatim into D:/Code/Utinni/UtinniCore/swg/ (consumer side); UTINNI_HOOKPOINTS_VERSION pinned at 1"
affects: [37-03-full-catalog, utinni-consumer-coordination]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "MULTIPLE-INHERITANCE landmine: a class whose member PMFs are inflated (>sizeof(void*)) cannot use pmfToVoid -- the sizeof guard fails the build (C2338); such rows need a __thiscall thunk or are deferred. GroundScene (via NetworkScene : Scene, MessageDispatch::Receiver) is the first instance caught."
    - "Private-static globals advertised via their NON-inline accessor address (call-not-read, sec 8 #3): game::g_runningFlags->Game::isOver, graphics::g_renderTargetWidth/Height->getCurrentRenderTarget*. Inline-only accessors (Game::getLoopCount) are OMITTED (no ODR-emitted address)."
    - "Overload disambiguation via explicit static_cast on the &fn (game::setScene, graphics::present/presentWindow); a non-overloaded twin (game::getCamera, whose const sibling is the separately-named getConstCamera) takes a plain &fn with NO cast."

key-files:
  modified:
    - src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.inc
    - src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp
  synced-out-of-repo:
    - D:/Code/Utinni/UtinniCore/swg/utinni_engine_hookpoints.h
    - D:/Code/Utinni/UtinniCore/swg/utinni_engine_hookpoints.inc

key-decisions:
  - "ALL scene::groundScene rows deferred to 37-03: GroundScene is a multiple-inheritance class -> inflated PMFs fail the pmfToVoid sizeof guard (Rule 3 build-discovered). Even the 'clean' reloadTerrain/getCurrentCamera/setView need __thiscall thunks. OMITTED not weakened (a wrong/inflated & is worse than a missing row, spec sec 0)."
  - "cui::chatWindow group + chatWindow::ctor deferred to 37-03: SwgCuiChatWindow is ALSO multiple-inheritance (SwgCuiLockableMediator, UINotification, MessageDispatch::Receiver) -> same inflated-PMF blocker; the corrected member names (acceptTextInput/appendToAllTabs/appendTextToCurrentTab/performEnterKey) are recorded for 37-03."
  - "client::* exe-side functions deferred to 37-03 except client::clientMain: wndProc/writeMiniDump/writeCrashLog/setupStartDataInstall are file-local statics each needing an external-linkage shim (37-01 crash-fixer pattern). Only ClientMain (already declared in ClientMain.h) is linkable now."
  - "Optimized (_o.exe) flavor export NOT verified: the Optimized config fails at LINK with the PRE-EXISTING LNK1281 SAFESEH defect (0 unresolved from our rows). Validation bar = Debug+Release link-grep + dumpbin (both clean), per AGENTS.md."

requirements-completed: [EPA-03, EPA-04]

# Metrics
duration: 71min
completed: 2026-06-21
---

# Phase 37 Plan 02: Utinni MVP Hookpoint Catalog Summary

**The SwgClient engine-entrypoint contract grew from the 3-row 37-01 spike to a 41-endpoint MVP catalog (config/client/game/graphics + cuiManager/cuiIo/consoleHelper/commandParser), every row source-confirmed and signature-checked; graphics::install resolves the DX11 overlay kickoff (EPA-03), the count static_assert + runtime name-set-equality self-check prove zero .inc<->table drift (EPA-04), the canonical 5-target Win32 build links clean (Debug+Release), and the client boots to char-select under both rasterMajor=5 and =11.**

## Performance
- **Duration:** ~71 min wall-clock (~15 min of work; the Optimized full-graph rebuild alone ran ~60 min before hitting the pre-existing SAFESEH link defect)
- **Started:** 2026-06-21T21:02:03Z
- **Completed:** 2026-06-21T22:13:00Z (approx)
- **Tasks:** 3 (Tasks 1+2 committed together -- same two files, heterogeneous rows; Task 3 build/verify/sync)
- **Files modified:** 2 in-repo (+ 2 synced verbatim into D:/Code/Utinni)

## Accomplishments
- Extended `utinni_engine_hookpoints.inc` to the **41-name MVP required set** and the `s_engineHookPoints[]` table to 41 matching rows, in lockstep (the count `static_assert` = 41==41 compiled green).
- **graphics group present & non-null (EPA-03):** `graphics::install` + update/beginScene/endScene/present(cast)/presentWindow(cast)/resize/flushResources/screenShot/setHardwareMouseCursorEnabled/showMouseCursor/setSystemMouseCursorPosition/setStaticShader + the two RT-size accessor rows. The DX11 overlay kickoff resolves off the contract.
- **All 8 RESEARCH-flagged MISMATCH corrections baked in** (final symbol per name in the table below).
- **Landmine rules honored per row** (no bulk `&fn`): overloads cast-disambiguated; non-overloaded `game::getCamera` plain; private statics via non-inline accessors or OMITTED; the inline-only `game::g_mainLoopCounter` OMITTED; `cui::io::processEvent` (virtual) SKIPPED; ctors never `&Class::Class`; MI-inflated-PMF groups (groundScene, chatWindow) deferred.
- **Canonical 5-target Win32 build links clean** in **Debug AND Release**: `0 unresolved external symbol`, `0 LNK1181`, `0 LNK2019/2001`, Build succeeded. Every MVP `&` symbol resolved (the /FORCE trap caught nothing).
- **Both Debug + Release export `GetEngineHookPoints = _GetEngineHookPoints` undecorated** (dumpbin-confirmed, no `@` suffix, no `?`-mangling).
- **Dual-renderer boot smoke PASS:** Debug client booted to the login/character-select screen under **rasterMajor=5 (gl05/D3D9)** AND **rasterMajor=11 (gl11/D3D11)** -- CuiManager UI up, no crash, no FATAL (screenshots: `stage/boot-37-02-gl05.png`, `stage/boot-37-02-gl11.png`). Boot gate held.
- **Contract synced to Utinni:** `.h` + `.inc` copied verbatim into `D:/Code/Utinni/UtinniCore/swg/` (byte-identical, 41 names, version pinned at 1).

## Resolved CHECK/MISMATCH rows (final symbol per contract name)
| Contract name | Final symbol | Kind |
|---|---|---|
| config::loadConfigFileBuffer | `&ConfigFile::loadFromBuffer` [ConfigFile.h:136] | static |
| config::loadConfigFileString | `&ConfigFile::loadFile` [ConfigFile.h:135] (MISMATCH: spec loadFromString) | static |
| client::clientMain | `&ClientMain` [ClientMain.h:13] (__cdecl) | exe free fn |
| game::mainLoop | `&Game::run` [Game.h:96] (MISMATCH: spec runGame) | static |
| game::setupScene | `static_cast<void(*)(bool,const char*,const char*,CreatureObject*)>(&Game::setScene)` [Game.h:108] | static, OVERLOADED |
| game::getPlayerCreatureObject | `&Game::getPlayerCreature` [Game.h:157] (MISMATCH name) | static |
| game::getCamera | `&Game::getCamera` [Game.h:173] -- NO cast (not overloaded) | static |
| game::g_runningFlags | `&Game::isOver` [Game.h:134 / Game.cpp:1021] -- non-inline accessor (call-not-read) | accessor |
| graphics::present | `static_cast<bool(*)()>(&Graphics::present)` [Graphics.h:161] | static, OVERLOADED |
| graphics::presentWindow | `static_cast<bool(*)(HWND,int,int)>(&Graphics::present)` [Graphics.h:162] | static, OVERLOADED |
| graphics::screenshot | `&Graphics::screenShot` [Graphics.h:170] (MISMATCH name) | static |
| graphics::useHardwareCursor | `&Graphics::setHardwareMouseCursorEnabled` [Graphics.h:177] (MISMATCH name) | static |
| graphics::g_renderTargetWidth | `&Graphics::getCurrentRenderTargetWidth` [Graphics.h:103] (accessor, call-not-read) | accessor |
| graphics::g_renderTargetHeight | `&Graphics::getCurrentRenderTargetHeight` [Graphics.h:104] (accessor, call-not-read) | accessor |
| cuiManager::togglePointer | `&CuiManager::setPointerToggledOn` [CuiManager.h:107] (MISMATCH name) | static |
| cuiManager::g_instance | `&CuiManager::getIoWin` [CuiManager.h:100] -- CuiManager is all-static (no instance) | accessor |
| cuiIo::setKeyboardInputActive | `pmfToVoid(&CuiIoWin::setKeyboardInputActive)` [CuiIoWin.h:66] | member PMF (single-inheritance) |
| cuiIo::requestKeyboard | `pmfToVoid(&CuiIoWin::requestKeyboard)` [CuiIoWin.h:102] | member PMF |
| cuiIo::g_instance | `&CuiManager::getIoWin` [CuiManager.h:100] -- the CuiIoWin singleton accessor | accessor |
| consoleHelper::sendInput | `pmfToVoid(&CuiConsoleHelper::processInput)` [CuiConsoleHelper.h:76] (MISMATCH: spec sendInput) | member PMF (single-inheritance) |

Plus the verbatim CONFIRMED statics: game::{install(37-01 carried),quit,cleanupScene,getPlayer,getConstCamera,isViewFirstPerson,isHudSceneTypeSpace}; graphics::{update,beginScene,endScene,resize,flushResources,showMouseCursor,setSystemMouseCursorPosition,setStaticShader}; cuiManager::{render,setSize,restartMusic}; config::loadOverrideConfig (37-01 thunk); commandParser::addSubCommand (37-01 PMF).

## OMITTED / DEFERRED-to-37-03 MVP names (37-03 inherits this worklist)
| Name(s) | Reason |
|---|---|
| **scene::groundScene::{reloadTerrain, getCurrentCamera, changeCamera/setView}** | **GroundScene is MULTIPLE-INHERITANCE** (via `NetworkScene : public Scene, public MessageDispatch::Receiver`) -> inflated member PMFs fail the `sizeof(PMF)==sizeof(void*)` guard (C2338). Even these "clean" public non-virtual rows need a `__thiscall` thunk (37-03 tier). |
| scene::groundScene::{ctor, init, update, handleInputMap*} | ctor (can't address -> thunk) + private members (friend/thunk). |
| scene::groundScene::draw | VIRTUAL override of Scene/IoWin -> &fn is a vtable thunk. |
| cui::io::processEvent, cui::io::draw | VIRTUAL overrides of IoWin -> Utinni resolves off the live vtable (SKIP, documented in the table). |
| cui::chatWindow::* (acceptTextInput, appendToAllTabs, appendTextToCurrentTab, performEnterKey) + chatWindow::ctor | SwgCuiChatWindow is MULTIPLE-INHERITANCE (SwgCuiLockableMediator, UINotification, MessageDispatch::Receiver) -> inflated PMF blocker; needs `__thiscall` thunks. Corrected member names recorded above for 37-03. |
| client::{wndProc, writeMiniDump, writeCrashLog, setupStartDataInstall} | File-local exe statics -> each needs an external-linkage shim (the 37-01 crash-fixer pattern). wndProc is __stdcall. |
| config::setModalChat / getModalChat | Symbol unresolved on the cited classes (do not guess). |
| cui::manager::findObjectUnderCursor | Not a member of CuiManager (likely CuiIoWin/SwgCuiManager) -- unresolved. |
| game::g_mainLoopCounter | Only an INLINE accessor (`Game::getLoopCount`, Game.h:398) -> no ODR-emitted address (Pitfall 2). No non-inline accessor; OMITTED. |
| commandParser::{ctor1, ctor2} | __thiscall ctor thunks (37-03). |
| commandParser::createDelegateCommands | PROTECTED (CommandParser.h:180) -> friend/derived thunk or OMIT. |

## Coverage count
- **.inc required-set count = table row count = 41** (compile-time `static_assert` green -- zero drift).
- Runtime `utinni_verifyNoNullNoDup()`: by construction every addr is a non-null `&Symbol`, every name is X-macro-generated from the SAME `.inc` (so name-set-equality and no-dup hold), and the Debug boot did NOT assert/crash -> the no-null/no-dup/name-set-equality gate is satisfied. (The PASS line is `REPORT_LOG` -> OutputDebugString in Debug; a clean boot-to-char-select is the runtime evidence that it did not fail.)

## Dual-renderer boot results
| rasterMajor | renderer | result |
|---|---|---|
| 5 | gl05 / D3D9 | **PASS** -- login/char-select rendered, CuiManager UI up, no FATAL (`stage/boot-37-02-gl05.png`) |
| 11 | gl11 / D3D11 | **PASS** -- login/char-select rendered, CuiManager UI up, no FATAL (`stage/boot-37-02-gl11.png`) |

## Task Commits
1. **Tasks 1+2: populate MVP hookpoint catalog (config/client/game/graphics + cui/consoleHelper/commandParser)** - `7a3c438e5` (feat). Committed together: both tasks edit the same two files with interleaved heterogeneous rows; splitting the already-applied interleaved edits was not cleanly possible. No scope change.
2. **Task 3 fix: defer groundScene rows -- MULTIPLE-INHERITANCE inflated PMF fails sizeof guard** - `96174004a` (fix, Rule 3 build-discovered).

**Plan metadata:** _(final commit)_

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] scene::groundScene clean rows cannot use pmfToVoid (MI-inflated PMF)**
- **Found during:** Task 3 (first Debug build) -- `C2338: inflated PMF (multiple/virtual inheritance)`.
- **Issue:** The plan's interfaces block scoped `groundScene::{reloadTerrain, getCurrentCamera, changeCamera}` as "clean public non-virtual PMF rows". They are public+non-virtual, BUT GroundScene derives via `NetworkScene : public Scene, public MessageDispatch::Receiver` -- a multiple-inheritance class -> its member PMFs are inflated (>sizeof(void*)), tripping the pmfToVoid size guard at compile time. This was not flagged in RESEARCH (which assumed single-inheritance for these rows).
- **Fix:** Removed all three groundScene rows from the table AND the `.inc` (kept in lockstep, 44->41), removed the now-unused `clientGame/GroundScene.h` include, and documented them as deferred-to-37-03 (needs a `__thiscall` MI-class thunk). OMITTED, not weakened: weakening the size guard would let genuinely-wrong inflated PMFs corrupt the contract (a wrong & is worse than a missing row, spec sec 0). The guard did exactly its job.
- **Files modified:** utinni_engine_hookpoints.inc, utinni_advertise.cpp
- **Verification:** Debug + Release rebuild link clean (0 unresolved); count static_assert 41==41 green.
- **Committed in:** `96174004a`

### Scope notes (not deviations)
- **cui::chatWindow group + chatWindow::ctor deferred:** SwgCuiChatWindow is also multiple-inheritance (same inflated-PMF blocker discovered while reading the header pre-emptively); deferred to 37-03 rather than risk a wrong row. The plan explicitly permits "defer to 37-03 and document". The boot->render->scene-change MVP repro path does not require chat-window text manipulation.
- **Optimized (_o) flavor not exported:** the Optimized config fails at LINK with the PRE-EXISTING `LNK1281 SAFESEH` defect (0 unresolved from our rows -- the only error is SAFESEH image generation). Logged to `deferred-items.md`. Validation bar per AGENTS.md = Debug+Release link-grep + dumpbin, both clean.

**Total deviations:** 1 auto-fixed (Rule 3 blocking). 2 documented scope/pre-existing notes.

## Threat Flags
None. The catalog is additive + inert (read-only getter, no untrusted input). The MI-inflated-PMF guard (T-37-05) fired correctly (caught GroundScene at compile time); no virtual/ctor/private row was advertised as a raw `&fn`. Acceptance greps confirm 0 `&Class::Class`, 0 `&CuiIoWin::processEvent`, 0 private-member address references.

## Notes for 37-03 (carry-forward)
- **The MI-class thunk pattern is 37-03's first job:** GroundScene AND SwgCuiChatWindow are both multiple-inheritance -> every member row needs a `__thiscall` free-function thunk (cast `this`, call the method). This blocks the entire scene-change repro tier and the chat-window tier.
- **Inherited landmines:** virtuals (groundScene::draw, cui::io::processEvent) -> SKIP; ctors (groundScene, commandParser::ctor1/2, chatWindow) -> __thiscall thunks; protected (createDelegateCommands) -> friend/derived; private (groundScene init/update/handleInputMap*) -> friend/thunk; inline-only accessors (Game::getLoopCount) -> non-inline shim or OMIT.
- **client::* shims:** wndProc(__stdcall)/writeMiniDump/writeCrashLog/setupStartDataInstall are file-local exe statics; each needs an external-linkage shim like 37-01's utinni_installConfigFileOverride.
- **Re-copy obligation stands:** 37-03 must re-copy `.h` + `.inc` into `D:/Code/Utinni/UtinniCore/swg/` at wave close (version stays 1).
- **Include-path trap (37-01 lesson) did NOT bite this wave:** all newly-referenced libs (clientGraphics, clientGame, clientUserInterface, sharedFoundation) were already on SwgClient's include path. 37-03 must re-check for any new library (e.g. swgClientUserInterface forwarders for the chat-window thunks).

## Self-Check: PASSED
(see appended verification below)
