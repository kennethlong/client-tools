---
phase: 38-utinni-advertised-client-coverage-completion
plan: 02
subsystem: infra
tags: [utinni, getenginehookpoints, external-linkage-shim, stdcall, friend-decl, win32, wr-05, abi-neutral]

# Dependency graph
requires:
  - phase: 38-01
    provides: "8 groundScene rows + the friend-decl-for-private-member mechanism + the exe-local forwarder-header precedent + the committed WR-05 consoleHelper::sendInput thunk"
provides:
  - "4 advertised client/config rows: client::wndProc (__stdcall Os.cpp shim), client::writeMiniDump (DebugHelp.cpp shim), config::setModalChat + config::getModalChat (CuiPreferences statics, plain &fn)"
  - "exe-local utinni_clientShims_forward.h declaring the 2 client::* shims"
  - "ABI-neutral friend decl in Os.h granting utinni_osWindowProc access to the private Os::WindowProc"
  - "WR-05 consoleHelper::sendInput VERIFIED present (committed in 38-01); 2 NONEXISTENT spec symbols OMITTED-with-reason for the EPA-08 handback"
affects: [38-03, 38-04, utinni]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "file-local/private exe static -> external-linkage utinni_* shim DEFINED in the symbol's own win32 TU (Os.cpp / DebugHelp.cpp), declared extern in an exe-local forwarder header; the exe never sees the win32-private include dir"
    - "private static member (Os::WindowProc) reached by a free __stdcall shim STILL needs a friend decl in the class header (same-TU is not enough for private access -- the 38-01 C2248 lesson applies to shims too), ABI-neutral so no plugin cascade"
    - "__stdcall/CALLBACK convention preserved verbatim in a window-proc shim (NOT the __fastcall/__thiscall member-call emulation used for MI methods)"

key-files:
  created:
    - src/game/client/application/SwgClient/src/win32/utinni_clientShims_forward.h
  modified:
    - src/engine/shared/library/sharedFoundation/src/win32/Os.cpp
    - src/engine/shared/library/sharedFoundation/src/win32/Os.h
    - src/engine/shared/library/sharedDebug/src/win32/DebugHelp.cpp
    - src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp
    - src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.inc

key-decisions:
  - "client::wndProc shim needs a friend decl in Os.h (the SAME C2248 correction as 38-01 Deviation #1). A free __stdcall function in Os.cpp does NOT get private access just by being in the same TU; C++ grants private access only to members/friends. Friend decls are ABI-neutral (no data member / vtable change) so no shared-header plugin cascade."
  - "config::setModalChat/getModalChat advertised as plain (void*)&CuiPreferences::static (37-02 CORRECTION confirmed: they are CuiPreferences statics [CuiPreferences.h:94-95], NOT ConfigFile; non-overloaded, no cast)."
  - "client::writeCrashLog + client::setupStartDataInstall OMITTED (NONEXISTENT -- 0 source hits; SWGEmu Pre-CU). Recorded in both the .cpp OMIT comment and the .inc, flagged for the EPA-08 handback."
  - "WR-05 consoleHelper::sendInput thunk was ALREADY committed in 38-01 (b99515648) -- verified present + pointing at &utinni_consoleHelperSendInput; NOT re-authored."
  - "UTINNI_HOOKPOINTS_VERSION left at 1; 38-03 owns the single 1->2 bump."

patterns-established:
  - "Pattern: client/config external-linkage shim bucket = utinni_* shim in the defining win32 TU + (for a private member) an ABI-neutral friend decl in the class header + an exe-local extern-decl forwarder header included ONLY by utinni_advertise.cpp."

requirements-completed: [EPA-06]

# Metrics
duration: 18min
completed: 2026-06-22
---

# Phase 38 Plan 02: client/config Shim Bucket + WR-05 Summary

**4 source-confirmed client/config rows added to GetEngineHookPoints -- client::wndProc (__stdcall/CALLBACK shim in Os.cpp over the PRIVATE Os::WindowProc, friend-granted access) + client::writeMiniDump (shim in DebugHelp.cpp over the win32-private DebugHelp::writeMiniDump) + config::setModalChat/getModalChat (plain &fn on the 37-02-corrected CuiPreferences statics), with client::writeCrashLog + setupStartDataInstall OMITTED as NONEXISTENT and the parked WR-05 consoleHelper::sendInput thunk verified already-committed in 38-01. Debug+Release link 0 unresolved, undecorated GetEngineHookPoints export on both exes, count static_assert holds (+4 rows == +4 .inc names).**

## Performance

- **Duration:** ~18 min
- **Started:** 2026-06-22T21:08:25Z
- **Completed:** 2026-06-22T21:26:00Z
- **Tasks:** 2 auto + 1 checkpoint (boot-smoke DEFERRED per the BUILD-GATE-autonomous execution mode)
- **Files modified:** 5 (1 created, 4 modified)

## Accomplishments

- `client::wndProc` advertised via `utinni_osWindowProc` -- an external-linkage `__stdcall`/`CALLBACK` shim DEFINED in `Os.cpp` over the **private** `Os::WindowProc` [Os.h:138]. CALLBACK preserved exactly (a real window-proc convention, NOT the `__fastcall` member-call trick).
- `client::writeMiniDump` advertised via `utinni_writeMiniDump` -- an external-linkage shim DEFINED in `DebugHelp.cpp` over `DebugHelp::writeMiniDump` [DebugHelp.h:36], whose win32-private header is not on the exe include path (default args dropped in the shim; both passed through).
- `config::setModalChat` + `config::getModalChat` advertised as plain `(void*)&CuiPreferences::setModalChat` / `getModalChat` -- the **37-02 correction** confirmed from source: they are PUBLIC `CuiPreferences` statics [CuiPreferences.h:94-95], NOT `ConfigFile`; non-overloaded, no cast.
- New exe-local `utinni_clientShims_forward.h` declares both `extern` shims (`#include <windows.h>` for HWND/LRESULT/PEXCEPTION_POINTERS); included ONLY by `utinni_advertise.cpp` (no gl0X plugin pulls it -> no ABI cascade).
- `client::writeCrashLog` + `client::setupStartDataInstall` OMITTED (NONEXISTENT -- 0 source hits; SWGEmu Pre-CU) with reason recorded in the `.cpp` OMIT comment + the `.inc`, FLAGGED for the EPA-08 handback.
- WR-05 `consoleHelper::sendInput` thunk VERIFIED present in HEAD (committed in 38-01 `b99515648`), the row points at `&utinni_consoleHelperSendInput` (NOT `pmfToVoid`) -- NOT re-authored.
- 4 matching `UTINNI_HOOKPOINT` names added to the `.inc` in lockstep; count `static_assert` holds (table +4 == `.inc` +4).

## Task Commits

1. **Task 1: client::wndProc + writeMiniDump external-linkage shims + exe-local header** - `9379fe5cd` (feat)
2. **Task 2: 4 client/config rows + .inc lockstep + OMIT 2 nonexistent + WR-05 verify** - `647def95d` (feat)
3. **Task 3: boot-smoke checkpoint** - DEFERRED (see Build-Gate Evidence + Deviations)

**Plan metadata:** (this SUMMARY + STATE/ROADMAP/REQUIREMENTS) committed separately.

## Files Created/Modified

- `src/game/client/application/SwgClient/src/win32/utinni_clientShims_forward.h` (NEW) -- `extern` declarations of `utinni_osWindowProc` (`__stdcall CALLBACK`) + `utinni_writeMiniDump`; `#include <windows.h>` for the win32 types; included ONLY by `utinni_advertise.cpp` (exe-local; no plugin cascade).
- `src/engine/shared/library/sharedFoundation/src/win32/Os.cpp` -- `utinni_osWindowProc` `__stdcall` shim under `#if !defined(_WIN64)`.
- `src/engine/shared/library/sharedFoundation/src/win32/Os.h` -- a Win32-guarded `friend LRESULT CALLBACK utinni_osWindowProc(...)` decl granting the shim private access to `Os::WindowProc` (ABI-neutral).
- `src/engine/shared/library/sharedDebug/src/win32/DebugHelp.cpp` -- `utinni_writeMiniDump` shim under `#if !defined(_WIN64)`.
- `src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp` -- 2 includes (`utinni_clientShims_forward.h` + `clientUserInterface/CuiPreferences.h`); 4 new table rows; the OMIT comment block for the 2 NONEXISTENT symbols.
- `src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.inc` -- 4 new `UTINNI_HOOKPOINT` names (config setModalChat/getModalChat, client wndProc/writeMiniDump).

## Build-Gate Evidence (AUTONOMOUS pass condition for this wave)

- **Engine libs (prerequisite):** `sharedFoundation.lib` + `sharedDebug.lib` rebuilt Debug AND Release (`$env:MSBUILD`, `/nodeReuse:false`, serial) BEFORE the SwgClient relink -- both compile clean (0 C2248 after the Os.h friend decl; see Deviation #1). `sharedDebug` compiled clean on the first pass.
- **SwgClient Debug** (`SwgClient.vcxproj /p:Configuration=Debug /p:Platform=Win32`, `/nodeReuse:false`, staged exe deleted to force relink): `unresolved external symbol` = **0**, LNK1181 = 0, LNK1120 = 0, `error C` = 0. Build succeeded; staged to `stage/SwgClient_d.exe` (mtime 16:12).
- **SwgClient Release** (same flags, Configuration=Release): `unresolved external symbol` = **0**, LNK1181 = 0, LNK1120 = 0, `error C` = 0. Build succeeded; staged to `stage/SwgClient_r.exe`.
- **dumpbin /exports** (VS18 BuildTools x64 dumpbin, run from PowerShell -- Git Bash mangles `/exports` -> `LNK1181` per AGENTS.md): `SwgClient_d.exe` -> `GetEngineHookPoints = _GetEngineHookPoints` (undecorated, ordinal 52); `SwgClient_r.exe` -> `GetEngineHookPoints = _GetEngineHookPoints` (undecorated, ordinal 51).
- **static_assert** `(sizeof s_engineHookPoints / sizeof[0]) == UTINNI_REQUIRED_COUNT`: held -- both builds compiled (table gained exactly 4 rows, `.inc` gained exactly 4 names; no "drift" diagnostic in either log).
- **Grep checks:** WR-05 `consoleHelper::sendInput` row points at `&utinni_consoleHelperSendInput` (1 hit, not `pmfToVoid`); `writeCrashLog`/`setupStartDataInstall` appear as 0 table rows and 0 `UTINNI_HOOKPOINT` macros (only in OMIT comments); 4 new client/config rows present; 4 new `.inc` names present; `utinni_clientShims_forward.h` is `#include`d only by `utinni_advertise.cpp` (Os.cpp / DebugHelp.cpp reference it by NAME in their banner comments but do NOT `#include` it).

**Boot-smoke (Task 3 checkpoint:human-verify):** DEFERRED to the final consolidated maintainer pass (per the BUILD-GATE-autonomous execution mode for this run). The gl05 char-select boot, the gl11 window-up non-regression, and the runtime `utinni_verifyNoNullNoDup()` PASS were NOT run and are NOT claimed here. The build-gate above is the autonomous pass condition.

## Decisions Made

- **The client::wndProc shim needs a friend decl, not just same-TU.** See Deviation #1 -- exactly the 38-01 carry-forward lesson, now confirmed to apply to shims (not only forwarders): a free `__stdcall` function in `Os.cpp` is NOT a member/friend of `Os`, so naming the private `Os::WindowProc` hit C2248. Resolved with an ABI-neutral Win32-guarded `friend` decl in `Os.h`.
- **config::* are CuiPreferences statics (37-02 correction confirmed).** Source-confirmed `setModalChat(bool)` [CuiPreferences.h:95] + `getModalChat()` [CuiPreferences.h:94] are public, non-overloaded -- plain `&fn`, no cast, via the already-wired `clientUserInterface\include\public`.
- **2 NONEXISTENT symbols OMITTED, flagged for EPA-08.** `writeCrashLog` (crash .txt written inline by `SetupSharedFoundation.cpp:92 sprintf`, no named fn) + `setupStartDataInstall` (no twin) grep to 0 source hits -> never guessed (D-04).
- **Version bump deferred.** `UTINNI_HOOKPOINTS_VERSION` left at 1; 38-03 owns the single 1->2 bump.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Added a `friend` declaration to Os.h for the wndProc shim**
- **Found during:** Task 1 (Os.cpp `utinni_osWindowProc` shim)
- **Issue:** The plan (and 38-RESEARCH / 38-PATTERNS) framed the wndProc shim as having member access simply by being "compiled in Os.cpp (member context)". This is the same C++ access-rules error corrected in 38-01: same-TU grants no private access -- only members and friends may name a private member. `sharedFoundation.lib` Debug failed `Os.cpp(1616): error C2248: 'Os::WindowProc': cannot access private member`.
- **Fix:** Added a `friend LRESULT CALLBACK utinni_osWindowProc(HWND, UINT, WPARAM, LPARAM);` declaration to `Os.h`, guarded `#if !defined(_WIN64)`. The friend decl adds no data member and no virtual, so the `Os` layout is byte-identical -- no shared-header plugin cascade (Os.h is consumed by engine TUs, but the friend decl does not change ABI; sharedFoundation.lib + sharedDebug.lib + SwgClient all rebuilt clean Debug+Release).
- **Files modified:** src/engine/shared/library/sharedFoundation/src/win32/Os.h
- **Verification:** sharedFoundation.lib Debug + Release rebuilt with 0 C2248; SwgClient Debug + Release linked 0 unresolved.
- **Committed in:** 9379fe5cd (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (Rule 3 - blocking)
**Impact on plan:** Deviation #1 is the 38-01 friend-access correction applied to a private static reached by a `__stdcall` shim (the carry-forward explicitly warned this would recur for `client::wndProc`). ABI-neutral, no plugin-cascade risk, no scope creep -- exactly the 2 shims + 2 plain-&fn rows the plan specified. `DebugHelp::writeMiniDump` is public so its shim needed no friend decl.

## Issues Encountered

- **dumpbin `/exports` mangling under Git Bash:** MSYS rewrote `/exports` to `C:\Program Files\Git\exports` -> spurious `LNK1181`. Re-ran dumpbin from PowerShell (AGENTS.md: run tooling from PowerShell, not Git Bash) -> clean undecorated export on both exes. (Operational note for future executors.)
- **Build-log encoding is UTF-16LE** (BOM) under PowerShell `Out-File`; decoded with `iconv -f UTF-16LE` to grep the link-gate lines (same 38-01 note).

## Threat Flags

None -- no new security-relevant surface beyond the read-only, additive export already covered by the plan's threat model (T-38-04..06). All 4 rows are source-confirmed (Os.h / DebugHelp.h / CuiPreferences.h file:line in each row comment): the private wndProc uses a friend-granted `__stdcall` shim (T-38-04 mitigate), the 2 NONEXISTENT spec symbols are OMITTED-with-reason never guessed (T-38-05 mitigate), and config::* are confirmed CuiPreferences statics with no overload ambiguity (T-38-06 mitigate).

## Next Phase Readiness

- client/config shim bucket complete; ready for 38-03 (chatWindow MI thunks + Bucket-4 confirm-or-OMIT + the `UTINNI_HOOKPOINTS_VERSION` 1->2 bump).
- **Cross-cutting note for 38-03/38-04:** the C2248 friend-access correction now applies to BOTH private-method forwarders (38-01) AND private-static shims (38-02). Any future "reach a private member from a same-TU shim" claim is wrong without a friend decl (or a confirmed file-local/namespace-scope target -- the ClientMain.cpp precedent's actual mechanism).
- **Handback items for EPA-08:** `client::writeCrashLog` + `client::setupStartDataInstall` OMITTED (NONEXISTENT in this tree) -- recorded in `utinni_advertise.cpp` + the `.inc`. Plus the 38-01 `groundScene::g_instance` OMIT.
- **38-03 owns** the `UTINNI_HOOKPOINTS_VERSION` 1->2 bump; **38-04 owns** the byte-exact `.inc`/`.h` re-sync to `D:/Code/Utinni` + the EPA-08 DX11-evidence handback doc.

## Self-Check: PASSED

- FOUND: src/game/client/application/SwgClient/src/win32/utinni_clientShims_forward.h
- FOUND: .planning/phases/38-utinni-advertised-client-coverage-completion/38-02-SUMMARY.md
- FOUND commit 9379fe5cd (Task 1)
- FOUND commit 647def95d (Task 2)
- FOUND undecorated GetEngineHookPoints export on stage/SwgClient_d.exe (ord 52) + _r.exe (ord 51)

---
*Phase: 38-utinni-advertised-client-coverage-completion*
*Completed: 2026-06-22*
