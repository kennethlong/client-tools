---
phase: 38-utinni-advertised-client-coverage-completion
plan: 03
subsystem: infra
tags: [utinni, getenginehookpoints, msvc-abi, thunk, fastcall, multiple-inheritance, chatwindow, version-bump, bucket4, win32]

# Dependency graph
requires:
  - phase: 38-02
    provides: "82-row table (78 from 37-03 + 4 client/config) + 90-name .inc + the __fastcall MI-thunk precedent + the count static_assert + runtime utinni_verifyNoNullNoDup() self-check + UTINNI_HOOKPOINTS_VERSION still at 1"
provides:
  - "4 advertised cui::chatWindow::* rows (enableTextInput/writeToAllTabs/writeToCurrentTab/chatEnterHandler) over a triple-MI class via __fastcall thunks (never pmfToVoid)"
  - "UTINNI_HOOKPOINTS_VERSION bumped 1->2 + the 37-era PINNED-at-1 comment rewritten to the D-03 bump-on-name-change policy (single bump covers all Phase-38 growth)"
  - "Bucket-4 remaining-full-set confirm-or-OMIT ledger (0 net new rows; every spec §6 candidate accounted for)"
  - "94-name .inc / 94-row table in lockstep; count static_assert holds at version 2"
affects: [38-04, utinni]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Triple-MI class public method -> free __fastcall(SwgCuiChatWindow*, int /*edx*/, args) thunk (PMF inflated by triple-inheritance -> pmfToVoid would trip the C2338 sizeof guard); PUBLIC targets need NO friend decl (unlike the 38-01 private forwarders / 38-02 private-static shim)"
    - "Single contract version bump (1->2) batched at the catalog wave covers ALL names added across the phase's plans, not per-plan"
    - "Bucket-4 confirm-or-OMIT discipline (D-04): a candidate that source-confirms as virtual/inline/protected/MI-ctor/private-no-accessor/NONEXISTENT is OMITTED-with-reason in a ledger comment, never added as a guessed &fn"

key-files:
  created: []
  modified:
    - src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp
    - src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.inc
    - src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.h

key-decisions:
  - "All 4 SwgCuiChatWindow target methods are PUBLIC non-virtual (acceptTextInput :112 / appendToAllTabs :172 / appendTextToCurrentTab :174 / performEnterKey :214) -> reached directly by __fastcall thunks; NO friend decl needed and NO engine-library edit (the 38-01/38-02 C2248 friend correction does NOT apply here -- those targeted private members). The whole change is exe-local to utinni_advertise.cpp + .inc + .h."
  - "cuiChatWindow::ctor DEFERRED (MI ctor + required live UIPage& arg; 37-03 already deferred it) -- no ctor row, no .inc name. Flagged for the EPA-08 handback."
  - "chatEnterHandler advertises the CLEAN-entry performEnterKey() ONLY. The Issue #11 mid-function chat-routing NOP (SWGEmu-only, mid-body patch) is a SEPARATE joint decision (38-CONTEXT.md OUT-OF-SCOPE) -- NOT implemented; no offset arithmetic enters the contract."
  - "Bucket-4 NET NEW ROWS = ZERO. 37-03 took the plain-&fn bulk; every remaining spec §6 candidate source-confirmed in this tree this wave is virtual/inline/protected/MI-ctor/private-no-accessor/NONEXISTENT -> all OMITTED-with-reason in a ledger comment (D-04: a wrong & is worse than a missing row)."
  - "UTINNI_HOOKPOINTS_VERSION bumped 1->2 (this plan owns the single bump). The 37-era 'PINNED at 1 / do NOT bump per wave' comment rewritten to the D-03 bump-on-name-change policy. One bump covers all Phase-38 growth (38-01 x8 + 38-02 x4 + 38-03 x4)."

patterns-established:
  - "Pattern: triple-MI public-method advertisement = free __fastcall(pThis, int /*edx*/, args) thunk in utinni_advertise.cpp, no friend decl (target is public), no engine .cpp edit."

requirements-completed: [EPA-05, EPA-07]

# Metrics
duration: 12min
completed: 2026-06-22
---

# Phase 38 Plan 03: cui::chatWindow MI Thunks + Version Bump Summary

**4 source-confirmed cui::chatWindow::* rows added to GetEngineHookPoints via __fastcall thunks over the triple-MI SwgCuiChatWindow public methods (enableTextInput->acceptTextInput, writeToAllTabs->appendToAllTabs, writeToCurrentTab->appendTextToCurrentTab, chatEnterHandler->performEnterKey clean-entry), ctor DEFERRED (MI + live UIPage&), the Bucket-4 remaining full-set fully confirm-or-OMIT classified with ZERO net new rows, and UTINNI_HOOKPOINTS_VERSION bumped 1->2 (the 37-era PINNED-at-1 comment rewritten to the D-03 bump-on-name-change policy). Debug+Release link 0 unresolved, undecorated GetEngineHookPoints export on both exes (ord 52/51), count static_assert holds at version 2 (94-row table == 94-name .inc).**

## Performance

- **Duration:** ~12 min
- **Started:** 2026-06-22T21:18:45Z
- **Completed:** 2026-06-22T21:31:00Z
- **Tasks:** 2 auto + 1 checkpoint (boot-smoke DEFERRED per the BUILD-GATE-autonomous execution mode)
- **Files modified:** 3 (0 created, 3 modified) -- all exe-local; NO engine-library edit this plan

## Accomplishments

- `cui::chatWindow::enableTextInput` -> `utinni_chatWindowAcceptTextInput` (3-bool `__fastcall` thunk over `acceptTextInput(bool,bool,bool)` [SwgCuiChatWindow.h:112]).
- `cui::chatWindow::writeToAllTabs` -> `utinni_chatWindowAppendToAllTabs` (`const Unicode::String&` thunk over `appendToAllTabs` [SwgCuiChatWindow.h:172]).
- `cui::chatWindow::writeToCurrentTab` -> `utinni_chatWindowAppendTextToCurrentTab` (`const Unicode::String&` thunk over `appendTextToCurrentTab` [SwgCuiChatWindow.h:174]).
- `cui::chatWindow::chatEnterHandler` -> `utinni_chatWindowPerformEnterKey` (no-arg thunk over the CLEAN-entry `performEnterKey()` [SwgCuiChatWindow.h:214]); Issue #11 mid-function NOP separated out as a deferred joint decision.
- All 4 thunks are free `__fastcall(SwgCuiChatWindow*, int /*edx*/, args)` -- triple-MI class (`SwgCuiLockableMediator` + `UINotification` + `MessageDispatch::Receiver` [SwgCuiChatWindow.h:58-61]) so `pmfToVoid` would trip the C2338 sizeof guard; all 4 targets PUBLIC so NO friend decl / NO engine .cpp edit.
- `cuiChatWindow::ctor` DEFERRED (MI ctor needs a live `UIPage&` arg; 37-03 precedent) -- no ctor row, no `.inc` name; flagged for EPA-08.
- 4 matching `UTINNI_HOOKPOINT(cuiChatWindow, *)` names added to the `.inc` with the group/name tokens matching the 4 `"cuiChatWindow::*"` row names exactly (name-set-equality self-check stays satisfiable).
- Bucket-4 remaining full-set classified confirm-or-OMIT: ZERO net new rows; every candidate accounted for in a ledger comment (no silent drop, no spec-faith add).
- `UTINNI_HOOKPOINTS_VERSION` bumped 1->2; the 37-era "PINNED at 1 / do NOT bump per wave" comment rewritten to the D-03 bump-on-name-change policy.

## Task Commits

1. **Task 1: cui::chatWindow MI thunks (4 rows) + ctor DEFER + .inc** - `ccc5373af` (feat)
2. **Task 2: Bucket-4 confirm-or-OMIT ledger (0 net adds) + version 1->2 bump** - `112f6fb18` (feat)
3. **Task 3: boot-smoke checkpoint** - DEFERRED (see Build-Gate Evidence)

**Plan metadata:** (this SUMMARY + STATE/ROADMAP/REQUIREMENTS) committed separately.

## Files Created/Modified

- `src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp` - `#include "swgClientUserInterface/SwgCuiChatWindow.h"`; 4 `__fastcall` chatWindow thunks; 4 table rows + a ctor DEFER comment; the Bucket-4 confirm-or-OMIT ledger comment block.
- `src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.inc` - 4 new `UTINNI_HOOKPOINT(cuiChatWindow, *)` names (group `cuiChatWindow`, names `enableTextInput`/`writeToAllTabs`/`writeToCurrentTab`/`chatEnterHandler`).
- `src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.h` - `#define UTINNI_HOOKPOINTS_VERSION 2`; the PINNED-at-1 comment block rewritten to the D-03 bump-on-name-change policy.

## chatWindow rows + group token (for the EPA-08 handback)

| contract name (in `.inc`/table) | thunk | engine symbol (this tree) | file:line | mechanism |
|---|---|---|---|---|
| `cuiChatWindow::enableTextInput` | `utinni_chatWindowAcceptTextInput` | `acceptTextInput(bool,bool,bool)` | SwgCuiChatWindow.h:112 | triple-MI `__fastcall` thunk |
| `cuiChatWindow::writeToAllTabs` | `utinni_chatWindowAppendToAllTabs` | `appendToAllTabs(const Unicode::String&)` | SwgCuiChatWindow.h:172 | triple-MI `__fastcall` thunk |
| `cuiChatWindow::writeToCurrentTab` | `utinni_chatWindowAppendTextToCurrentTab` | `appendTextToCurrentTab(const Unicode::String&)` | SwgCuiChatWindow.h:174 | triple-MI `__fastcall` thunk |
| `cuiChatWindow::chatEnterHandler` | `utinni_chatWindowPerformEnterKey` | `performEnterKey()` (clean entry) | SwgCuiChatWindow.h:214 | triple-MI `__fastcall` thunk |

**Group token chosen:** `cuiChatWindow` (matching the existing dotted-style realization of `cuiManager` / `cuiIo`; the spec contract form `cui::chatWindow::<method>` is realized as the X-macro `cuiChatWindow::<method>` pair, mirroring how `cui::manager`/`cui::io` are realized as `cuiManager`/`cuiIo`). The `.inc` `#g "::" #n` expansion and the table row strings agree (`cuiChatWindow::enableTextInput`, etc.).

## Bucket-4 confirm-or-OMIT ledger (EPA-08 handback) — NET NEW ROWS: ZERO

Every spec §6 "remaining ~30 subsystems" candidate not already advertised was source-confirmed in THIS tree this wave; none is a clean plain-`&fn` not already present. Full accounting (also in the `.cpp` ledger comment):

| spec candidate(s) | disposition | source-confirmed reason |
|---|---|---|
| `Object::move_o` | OMIT inline | `inline void Object::move_o` [Object.h:1216] (no ODR address) |
| `Object::addToWorld/removeFromWorld` | SKIP virtual | [Object.h:120-121] (vtable-resolved) |
| `Object::setParentCell` | SKIP virtual | `virtual void setParentCell` [Object.h:168] |
| `CommandParser::createDelegateCommands` | OMIT protected | under `protected:` [CommandParser.h:178-180] (would need an in-TU forwarder; Utinni's TJT path uses the already-advertised ctors/addSubCommand) |
| `Camera::getViewportX0/Y0/Width/Height` | OMIT inline | `inline int Camera::getViewport*` [Camera.h:210/226/242/..] (no ODR address) |
| `Camera::getViewport(int&,...)/(float&,...)` overloads | OMIT (no distinct slot) | out-of-line + addressable [Camera.h:170-171], BUT the spec lists the camera getters generically under the inline-OMIT bucket with no distinct contract slot -> adding a speculative overloaded `camera::getViewport` row would be a spec-faith add (D-04). Flagged for EPA-08 if Utinni names a concrete slot. |
| `Appearance::render/collide`, `RenderWorld::render`, `clientWorld::collide`, `proceduralTerrain::*` | SKIP virtual | already in the VIRTUAL SKIPS block / 37-03 (vtable-resolved) |
| `hud/loginScreen/gameMenu/radialMenu` + ~20 low-level UI control ctors | DEFER MI ctor | each needs its own placement-new `__fastcall` thunk + injector-supplied args; Utinni resolves via RVA today (same rationale as cuiChatWindow::ctor) |
| read-only globals (player health/stats, hud view-distance, terrain singleton/weather/filename, static-shader) | OMIT private | private members with NO non-inline ODR accessor this wave (§8 #3: never take a private-member address) |
| `memory::deallocateString`, `math::vectorNormalize`, `network idManager*`, `crcString::calculateCrc`, `client::writeCrashLog`, `client::setupStartDataInstall` | OMIT NONEXISTENT | 0 source twin (MemoryManager has `free()` not `deallocate`; the rest grep to 0). Flagged for EPA-08. |

No new `#include` and no new vcxproj include dir were needed (zero adds; `swgClientUserInterface\include\public` for the chatWindow header is already on all 5 SwgClient config blocks).

## DEFER rationale (EPA-08 handback)

- **`cuiChatWindow::ctor`** -- `SwgCuiChatWindow(UIPage&, Game::SceneType, std::string const&)` [SwgCuiChatWindow.h:106]: an MI ctor that requires a LIVE `UIPage&` arg the injector must supply. 37-03 already deferred this exact ctor; matches the groundScene/UI-mediator MI-ctor DEFER rationale. If Utinni confirms it constructs chat windows through a page, add a placement-new `__fastcall` thunk in a follow-up.
- **Issue #11 (chatEnterHandler mid-function chat routing)** -- the CLEAN-entry `performEnterKey()` &fn IS advertised; the mid-function chat-context-routing NOP that Utinni patches mid-body on SWGEmu is a SEPARATE, SWGEmu-only joint decision (38-CONTEXT.md OUT-OF-SCOPE). NOT implemented here -- no offset arithmetic enters the contract. If Utinni's Issue #11 needs the mid-body patch rather than the entry hook, that is a cooperative-API decision for the consumer side.

## Build-Gate Evidence (AUTONOMOUS pass condition for this wave)

- **No engine-library rebuild needed:** the entire change is exe-local (utinni_advertise.cpp + .inc + .h are all compiled into SwgClient; all 4 chatWindow targets are PUBLIC so no GroundScene/Os/DebugHelp-style engine .cpp edit). The 38-01 LNK1120-from-stale-Release-lib trap does not apply this plan.
- **SwgClient Debug** (`SwgClient.vcxproj /p:Configuration=Debug /p:Platform=Win32`, `$env:MSBUILD`, `/nodeReuse:false`, serial, staged exe deleted to force relink): `unresolved external symbol` = **0**, LNK1181 = 0, LNK1120 = 0, `error C` = 0. Build succeeded; staged to `stage/SwgClient_d.exe`.
- **SwgClient Release** (same flags, Configuration=Release): `unresolved external symbol` = **0**, LNK1181 = 0, LNK1120 = 0, `error C` = 0. Build succeeded; staged to `stage/SwgClient_r.exe`.
- **dumpbin /exports** (VS18 BuildTools x64 dumpbin, run from PowerShell): `SwgClient_d.exe` -> `GetEngineHookPoints = _GetEngineHookPoints` (undecorated, ordinal 52); `SwgClient_r.exe` -> `GetEngineHookPoints = _GetEngineHookPoints` (undecorated, ordinal 51).
- **static_assert** `(sizeof s_engineHookPoints / sizeof[0]) == UTINNI_REQUIRED_COUNT`: held -- both builds compiled (table gained exactly 4 rows, `.inc` gained exactly 4 names; no "drift" diagnostic in either log). Total: **94-row table == 94-name `.inc`** (78 from 37-03 + 8 groundScene + 4 client/config + 4 chatWindow).
- **Version:** `#define UTINNI_HOOKPOINTS_VERSION 2` compiled into both binaries' `s_table.version`.
- **Grep checks:** exactly 4 `{ "cuiChatWindow::` rows in advertise.cpp; exactly 4 `UTINNI_HOOKPOINT(cuiChatWindow` lines in the `.inc`; 0 `cuiChatWindow::ctor"` rows; 0 `pmfToVoid(&SwgCuiChatWindow::` table rows (the 2 grep hits are the comment warning "must NEVER be used"); `.inc` name count == table row count (94 == 94).

**Boot-smoke (Task 3 checkpoint:human-verify):** DEFERRED to the final consolidated maintainer pass (per the BUILD-GATE-autonomous execution mode for this run). The gl05 char-select boot, the gl11 window-up non-regression, and the runtime `utinni_verifyNoNullNoDup()` PASS at version 2 were NOT run and are NOT claimed here. The build-gate above is the autonomous pass condition.

## Decisions Made

- **All 4 chatWindow targets are PUBLIC -> no friend decl, no engine edit.** Source-confirmed in the public block of SwgCuiChatWindow.h [:63-231]: `acceptTextInput` :112, `appendToAllTabs` :172, `appendTextToCurrentTab` :174, `performEnterKey` :214. The 38-01/38-02 C2248 friend-access correction (which applied to GroundScene's private methods and Os::WindowProc) does NOT recur here -- the only edits are to the exe-local advertise TU + the shared .inc/.h.
- **Bucket-4 = 0 net adds (confirm-or-OMIT).** The carry-forward + RESEARCH A4 predicted a small/empty add-list; source-confirmation this wave verified every remaining candidate is virtual/inline/protected/MI-ctor/private-no-accessor/NONEXISTENT. Recorded in a ledger comment rather than added on faith (D-04).
- **Single version bump batched here.** 38-01 and 38-02 deliberately left the version at 1; this plan owns the one 1->2 bump that covers all 16 names added across the phase (8+4+4). The 37-era pin comment is rewritten to the D-03 policy.

## Deviations from Plan

None - plan executed exactly as written. (Notably, the carry-forward's "private-member -> friend-decl" caution did NOT trigger this plan because all 4 chatWindow targets are public; no engine-library edit was needed, and no Bucket-4 row qualified for an add.)

## Threat Flags

None -- no new security-relevant surface beyond the read-only, additive export already covered by the plan's threat model (T-38-07..09). All 4 chatWindow rows are source-confirmed (SwgCuiChatWindow.h file:line in each row comment), the triple-MI methods use `__fastcall` thunks never `pmfToVoid` (T-38-07 mitigate), `chatEnterHandler` advertises only the clean-entry `performEnterKey` with no mid-function offset arithmetic (T-38-08 mitigate), and the Bucket-4 pass is strict confirm-or-OMIT with zero guessed `&fn` (T-38-09 mitigate).

## Next Phase Readiness

- chatWindow MI coverage complete; version bumped to 2; Bucket-4 fully classified. Ready for **38-04** (EPA-07 remainder: the byte-exact `.inc`/`.h` re-sync into `D:/Code/Utinni/UtinniCore/swg/` + the EPA-08 DX11-evidence handback doc + the consolidated boot-smoke maintainer pass).
- **EPA-08 handback items accumulated across the phase:** `groundScene::g_instance` OMIT (38-01); `client::writeCrashLog` + `client::setupStartDataInstall` NONEXISTENT OMIT (38-02); `cuiChatWindow::ctor` DEFER + Issue #11 chatEnterHandler mid-patch separation + the full Bucket-4 OMIT ledger above (38-03).
- **38-04 owns** the byte-exact `.h`/`.inc` re-sync to `D:/Code/Utinni` (the phase build must NOT write there) and the consolidated maintainer boot-smoke (gl05 char-select + gl11 window-up + `utinni_verifyNoNullNoDup()` PASS at version 2 for the 94-name set).
- **Contract state at end of 38-03:** 94 advertised endpoints, `UTINNI_HOOKPOINTS_VERSION == 2`, table/`.inc`/static_assert in lockstep.

## Self-Check: PASSED

- FOUND: src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp (4 cuiChatWindow rows)
- FOUND: src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.inc (4 cuiChatWindow names)
- FOUND: src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.h (UTINNI_HOOKPOINTS_VERSION 2)
- FOUND: .planning/phases/38-utinni-advertised-client-coverage-completion/38-03-SUMMARY.md
- FOUND commit ccc5373af (Task 1)
- FOUND commit 112f6fb18 (Task 2)
- FOUND undecorated GetEngineHookPoints export on stage/SwgClient_d.exe (ord 52) + _r.exe (ord 51)

---
*Phase: 38-utinni-advertised-client-coverage-completion*
*Completed: 2026-06-22*
