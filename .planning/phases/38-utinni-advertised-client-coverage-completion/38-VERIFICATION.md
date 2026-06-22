---
phase: 38-utinni-advertised-client-coverage-completion
verified: 2026-06-22T23:00:00Z
status: human_needed
score: 9/9 must-haves verified
overrides_applied: 0
human_verification:
  - test: "Boot SwgClient_d.exe under rasterMajor=5 (gl05) to character select and confirm utinni_verifyNoNullNoDup() runtime self-check logs PASS for the 94-name set at version 2 (no null addr / no dup / name-set equal). Check the Debug log output."
    expected: "utinni_verifyNoNullNoDup: PASS (94 rows, 94 required)"
    why_human: "Runtime self-check executes inside the live process on the Debug boot path. Cannot observe log output without running the exe. The compile-time static_assert (94==94) is proven by the successful build, but the runtime null-addr check for all 16 new addresses (groundScene thunks/forwarders, Os.cpp shim, DebugHelp.cpp shim, CuiPreferences statics, SwgCuiChatWindow thunks) requires an actual boot."
  - test: "Boot once under rasterMajor=11 (gl11) — SwgCuiChatWindow.h is now included by utinni_advertise.cpp which is Win32-only but confirm the gl11 renderer DLL still loads and the window comes up with no crash."
    expected: "Window opens, reaches character select, no crash or unhandled exception"
    why_human: "D3D11 non-regression after Phase-38 exe changes. The change is exe-local under #if !defined(_WIN64) / Win32 ClCompile condition, so gl11 DLL is untouched, but shared-header includes in utinni_advertise.cpp (SwgCuiChatWindow.h, CuiPreferences.h) could theoretically expose latent issues. Requires a real boot to rule out."
  - test: "Live inject-smoke out of repo (Utinni injection under rasterMajor=5): inject Utinni into the running SwgClient_d.exe and confirm no 0xC0000005 crash, endpoints resolved 94/94 by name."
    expected: "Utinni logs 'endpoints: resolved 94/94 by name'; no access-violation crash; all 16 new contract names (groundScene::*, client::wndProc/writeMiniDump, config::setModalChat/getModalChat, cuiChatWindow::*) resolve to non-null addresses."
    why_human: "Out-of-repo step requiring the Utinni binary and a live injection. Cannot verify programmatically from this repo. Also validates that the D:/Code/Utinni byte-identical re-sync is consumable by the Utinni binary."
---

# Phase 38: Utinni Advertised-Client Coverage Completion — Verification Report

**Phase Goal:** Finish the provider half of the Utinni integration — extend the GetEngineHookPoints contract to the full addressable engine entry-point set: MI-class __thiscall thunks (groundScene, chatWindow), external-linkage shims for file-local exe statics (client/config), fold in WR-05, populate remaining plain-&fn rows where source-confirmable. Every new name source-confirmed; bump UTINNI_HOOKPOINTS_VERSION 1->2; .inc/.h re-sync byte-exact into D:/Code/Utinni. Client stays Utinni-agnostic; no regression to standalone D3D11 or SWGEmu. DX11 readiness is EVIDENCE-ONLY (provider proven correct — handoff doc, no code).
**Verified:** 2026-06-22T23:00:00Z
**Status:** human_needed
**Re-verification:** No — initial verification

---

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | UTINNI_HOOKPOINTS_VERSION == 2 in the .h | VERIFIED | `#define UTINNI_HOOKPOINTS_VERSION 2` at utinni_engine_hookpoints.h:46; the 37-era "PINNED at 1 / do NOT bump per wave" comment replaced with the D-03 bump-on-name-change policy |
| 2 | .inc has 94 names (78 from Phase 37 + 8 groundScene + 4 client/config + 4 cuiChatWindow) and the table has 94 rows | VERIFIED | `grep "^UTINNI_HOOKPOINT("` yields 94 lines; table row count from `grep -E '^\s+\{ "'` yields 94; compile-time static_assert passes (both Debug and Release built with 0 error) |
| 3 | 8 groundScene rows present using __fastcall thunks/forwarders — never pmfToVoid on the MI class | VERIFIED | advertise.cpp lines 303-312 show 8 rows; grep confirms 0 `pmfToVoid(&GroundScene::` table rows (2 hits are only in warning comments); forwarder definitions exist in GroundScene.cpp lines 3876-3897 under `#if !defined(_WIN64)` |
| 4 | Private groundScene methods reached via in-TU __fastcall forwarders with friend declarations (not same-TU assumption) | VERIFIED | utinni_groundSceneInit/Update/HandleInputMapUpdate/HandleInputMapEvent defined in GroundScene.cpp:3876-3897; friend declarations confirmed in GroundScene.h:59-63 under `#if !defined(_WIN64)`; header is exe-local (no gl0X plugin includes confirmed by grep showing only advertise.cpp + planning files reference utinni_groundScene_forward.h) |
| 5 | client::wndProc and client::writeMiniDump advertised via external-linkage shims in Os.cpp / DebugHelp.cpp; client::writeCrashLog + setupStartDataInstall OMITTED as NONEXISTENT | VERIFIED | utinni_osWindowProc defined at Os.cpp:1614 under `#if !defined(_WIN64)`; utinni_writeMiniDump defined at DebugHelp.cpp:746 under same guard; Os.h:148 friend declaration present; grep for `"writeCrashLog` and `"setupStartDataInstall` as table rows returns 0 (only in OMIT comments); utinni_clientShims_forward.h is exe-local |
| 6 | config::setModalChat and config::getModalChat are plain &fn on CuiPreferences statics (37-02 correction) | VERIFIED | advertise.cpp lines 256-257: `(void *)&CuiPreferences::setModalChat` / `getModalChat`; both commented as 37-02 CORRECTION CuiPreferences static |
| 7 | 4 cuiChatWindow rows present using __fastcall thunks over public SwgCuiChatWindow methods — never pmfToVoid; ctor DEFERRED | VERIFIED | advertise.cpp lines 321-325: 4 cuiChatWindow rows; grep confirms 0 `pmfToVoid(&SwgCuiChatWindow::` table rows (2 hits in comments only); ctor absent from both .inc and table; chatEnterHandler maps to clean-entry performEnterKey |
| 8 | .h and .inc re-synced byte-identical into D:/Code/Utinni/UtinniCore/swg/ | VERIFIED | `diff` confirms H_IDENTICAL and INC_IDENTICAL; Utinni copy shows `#define UTINNI_HOOKPOINTS_VERSION 2`; Utinni repo NOT committed by this phase (by design per D-03) |
| 9 | EPA-08 handback doc exists and contains all four required sections (advertised ledger, version 2, DX11 evidence, consumer worklist) | VERIFIED | `.planning/handoff/2026-06-22-utinni-provider-coverage-handback.md` exists; §1 lists all 16 new rows with mechanisms; §2 states version 2 with sha256 hashes; §3 transcribes LOCKED DX11 finding (Direct3d11_Device.cpp:435/574/684 ordering proof, no provider change); §4 lists consumer-side worklist (tryInstall DX11 guard, virtual vtable resolution, installable() skips, cuiChatWindow::ctor DEFER, Issue #11, g_instance OMIT, 2 NONEXISTENT) |

**Score:** 9/9 truths verified

---

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.h` | VERSION == 2 + rewritten policy comment | VERIFIED | `#define UTINNI_HOOKPOINTS_VERSION 2` at line 46; D-03 bump-on-name-change comment block at lines 31-44 |
| `src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.inc` | 94 UTINNI_HOOKPOINT lines including 8 groundScene + 4 cuiChatWindow + client/config adds | VERIFIED | 94 macro calls confirmed; all 8 groundScene names present (ctor/init/reloadTerrain/changeCamera/getCurrentCamera/update/handleInputMapUpdate/handleInputMapEvent); 4 cuiChatWindow names present; client wndProc/writeMiniDump + config setModalChat/getModalChat present |
| `src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp` | 94-row table + 8 groundScene thunks + 4 chatWindow thunks + shim references + WR-05 committed | VERIFIED | 94 rows confirmed; all 8 groundScene rows with correct mechanisms; 4 cuiChatWindow rows; client/config shim references; consoleHelper::sendInput points at `&utinni_consoleHelperSendInput` (not pmfToVoid); Bucket-4 OMIT ledger present |
| `src/game/client/application/SwgClient/src/win32/utinni_groundScene_forward.h` | extern declarations of 4 __fastcall groundScene private-method forwarders | VERIFIED | Exists; declares 4 functions with correct `__fastcall(GroundScene*, int, args)` signatures; forward-declares GroundScene/CreatureObject/IoEvent; no heavy engine headers; included only by advertise.cpp |
| `src/game/client/application/SwgClient/src/win32/utinni_clientShims_forward.h` | extern declarations of utinni_osWindowProc + utinni_writeMiniDump | VERIFIED | Exists; declares `extern LRESULT CALLBACK utinni_osWindowProc(HWND, UINT, WPARAM, LPARAM)` and `extern bool utinni_writeMiniDump(char const *, PEXCEPTION_POINTERS)`; includes `<windows.h>`; exe-local (only advertise.cpp references it) |
| `src/engine/client/library/clientGame/src/shared/scene/GroundScene.cpp` | 4 in-TU __fastcall forwarders under _WIN64 guard | VERIFIED | utinni_groundSceneInit/Update/HandleInputMapUpdate/HandleInputMapEvent defined at lines 3876-3897 under `#if !defined(_WIN64)` |
| `src/engine/client/library/clientGame/src/shared/scene/GroundScene.h` | 4 ABI-neutral friend declarations | VERIFIED | friend declarations at lines 59-63 for all 4 forwarder functions; ABI-neutral (no data member or vtable change) |
| `src/engine/shared/library/sharedFoundation/src/win32/Os.cpp` | utinni_osWindowProc __stdcall/CALLBACK shim | VERIFIED | Defined at line 1614 under `#if !defined(_WIN64)` |
| `src/engine/shared/library/sharedFoundation/src/win32/Os.h` | friend declaration for utinni_osWindowProc | VERIFIED | `friend LRESULT CALLBACK utinni_osWindowProc(HWND, UINT, WPARAM, LPARAM)` at line 148 |
| `src/engine/shared/library/sharedDebug/src/win32/DebugHelp.cpp` | utinni_writeMiniDump shim | VERIFIED | Defined at line 746 under `#if !defined(_WIN64)` |
| `.planning/handoff/2026-06-22-utinni-provider-coverage-handback.md` | Dated EPA-08 handback with all 4 sections | VERIFIED | Exists; dated 2026-06-22; self-contained; all 4 required sections present |

---

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| utinni_advertise.cpp table rows | GroundScene.cpp forwarders | `#include "utinni_groundScene_forward.h"` + `(void*)&utinni_groundScene*` | WIRED | advertise.cpp:39 includes the header; 4 private-method rows reference `&utinni_groundSceneInit` etc. |
| utinni_advertise.cpp table rows | Os.cpp / DebugHelp.cpp shims | `#include "utinni_clientShims_forward.h"` + rows `&utinni_osWindowProc` / `&utinni_writeMiniDump` | WIRED | advertise.cpp:40 includes the header; client::wndProc and client::writeMiniDump rows wired correctly |
| utinni_advertise.cpp table rows | CuiPreferences statics | `#include "clientUserInterface/CuiPreferences.h"` + `(void*)&CuiPreferences::setModalChat/getModalChat` | WIRED | advertise.cpp:41 includes header; lines 256-257 use plain &fn |
| utinni_advertise.cpp table rows | SwgCuiChatWindow public methods | `#include "swgClientUserInterface/SwgCuiChatWindow.h"` + `(void*)&utinni_chatWindow*` thunks | WIRED | advertise.cpp:42 includes header; 4 cuiChatWindow thunks call pThis->acceptTextInput/appendToAllTabs/appendTextToCurrentTab/performEnterKey |
| s_engineHookPoints[] count | utinni_engine_hookpoints.inc count | compile-time static_assert (UTINNI_REQUIRED_COUNT) | WIRED | static_assert at advertise.cpp:483; both Debug and Release builds passed (0 error) confirming 94==94 |
| swg-client-v2 .h/.inc | D:/Code/Utinni/UtinniCore/swg/ | byte-identical copy (sha256 verified) | WIRED | diff confirms H_IDENTICAL and INC_IDENTICAL; version 2 in Utinni copy confirmed |

---

### Data-Flow Trace (Level 4)

Not applicable. This phase produces a compile-time-static read-only export table — there is no dynamic data source to trace. The "data" is the compile-time `&fn` addresses (non-null by construction at the link stage). The runtime `utinni_verifyNoNullNoDup()` null-addr sweep is the equivalent check, deferred to human verification.

---

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| 94 UTINNI_HOOKPOINT macro calls in .inc | `grep "^UTINNI_HOOKPOINT(" utinni_engine_hookpoints.inc \| wc -l` | 94 | PASS |
| 94 table rows in advertise.cpp | `grep -E '^\s+\{ "' utinni_advertise.cpp \| wc -l` | 94 | PASS |
| 8 groundScene rows in table | `grep -c '"groundScene::' utinni_advertise.cpp` | 8 | PASS |
| 4 cuiChatWindow rows in table | `grep -c '"cuiChatWindow::' utinni_advertise.cpp` | 4 | PASS |
| 4 client/config rows in table | grep for wndProc/writeMiniDump/setModalChat/getModalChat | 4 | PASS |
| No pmfToVoid on GroundScene MI class in table | `grep "pmfToVoid(&GroundScene::" utinni_advertise.cpp` (table hits) | 0 table hits | PASS |
| No pmfToVoid on SwgCuiChatWindow MI class in table | `grep "pmfToVoid(&SwgCuiChatWindow::" utinni_advertise.cpp` (table hits) | 0 table hits | PASS |
| NONEXISTENT symbols absent from table | grep for writeCrashLog/setupStartDataInstall as table rows | 0 table rows (only in OMIT comments) | PASS |
| draw and g_instance absent from .inc | grep for `UTINNI_HOOKPOINT(groundScene,.*draw` / `g_instance` | 0 matches | PASS |
| cuiChatWindow::ctor absent from .inc and table | grep in both files | 0 matches | PASS |
| forwarder header is exe-local | `git grep -l utinni_groundScene_forward` (non-planning files) | only advertise.cpp + GroundScene.cpp + GroundScene_forward.h itself | PASS |
| shims header is exe-local | `git grep -l utinni_clientShims_forward` (non-planning files) | only advertise.cpp + Os.cpp + DebugHelp.cpp + shims_forward.h itself | PASS |
| UTINNI_HOOKPOINTS_VERSION 2 in both repos | diff .h between repos | H_IDENTICAL + INC_IDENTICAL | PASS |
| UTINNI_HOOKPOINTS_VERSION 2 in .h | grep for `#define UTINNI_HOOKPOINTS_VERSION` | 2 | PASS |
| All 7 phase commits present in git | git log on commit hashes | abc2eed5d / b99515648 / 9379fe5cd / 647def95d / ccc5373af / 112f6fb18 / a94416360 all confirmed | PASS |
| Runtime self-check on live boot | Boot SwgClient_d.exe under rasterMajor=5 | NOT RUN | SKIP (human_needed) |
| D3D11 non-regression | Boot under rasterMajor=11 | NOT RUN | SKIP (human_needed) |
| Live inject-smoke | Utinni injection into running client | NOT RUN | SKIP (human_needed — out of repo) |

---

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|-------------|--------|----------|
| EPA-05 | 38-01, 38-03 | MI __thiscall thunks: groundScene + chatWindow | SATISFIED | 8 groundScene rows (3 public thunks + 1 ctor thunk + 4 private forwarders) + 4 cuiChatWindow rows; all using __fastcall thunks, never pmfToVoid |
| EPA-06 | 38-02 | External-linkage shims: client/config bucket | SATISFIED | utinni_osWindowProc shim in Os.cpp + utinni_writeMiniDump in DebugHelp.cpp; 2 plain &fn CuiPreferences statics; 2 NONEXISTENT symbols OMITTED-with-reason; WR-05 sendInput thunk verified committed |
| EPA-07 | 38-03, 38-04 | Remaining addressable full-set + version bump + coverage gate | SATISFIED | Bucket-4 confirm-or-OMIT ledger in advertise.cpp (0 net new rows; every spec §6 candidate accounted for); VERSION 2; static_assert (94==94) holds; .inc/.h re-synced byte-identical to Utinni |
| EPA-08 | 38-04 | DX11 evidence handback | SATISFIED | Handback doc at `.planning/handoff/2026-06-22-utinni-provider-coverage-handback.md`; transcribes LOCKED DX11 finding (Direct3d11_Device.cpp:435/574/684 create/destroy ordering proof; no provider window where swapChain!=null && device/context==null); states 0xC0000005 is consumer-side; no provider code change |

---

### Anti-Patterns Found

No blockers. Key patterns checked:

| File | Check | Result | Severity |
|------|-------|--------|----------|
| utinni_advertise.cpp | pmfToVoid on GroundScene (MI class) | 0 table hits — only in warning comments | None |
| utinni_advertise.cpp | pmfToVoid on SwgCuiChatWindow (triple-MI class) | 0 table hits — only in warning comments | None |
| utinni_advertise.cpp | writeCrashLog/setupStartDataInstall as table rows | 0 table rows — only in OMIT comments | None |
| utinni_advertise.cpp | cuiChatWindow::ctor as table row | 0 rows | None |
| utinni_engine_hookpoints.inc | groundScene::draw or g_instance | 0 matches | None |
| utinni_engine_hookpoints.inc | cuiChatWindow::ctor | 0 matches | None |
| utinni_groundScene_forward.h | Heavy engine headers included | 0 — only class forward-decls (GroundScene, CreatureObject, IoEvent) | None |
| utinni_clientShims_forward.h | gl0X plugin pulls this header | 0 — exe-local; no plugin reference found | None |
| GroundScene.h friend decls | ABI-neutral (no data member/vtable change) | 4 friend declarations; no data member or virtual added | None |
| Os.h friend decl | ABI-neutral | 1 friend declaration at line 148; no data member or virtual added | None |

---

### Human Verification Required

#### 1. Runtime self-check PASS at version 2 (94-name set)

**Test:** Boot `SwgClient_d.exe` under `rasterMajor=5` (gl05) to character select. Observe the Debug log for the `utinni_verifyNoNullNoDup()` output line.

**Expected:** `utinni_verifyNoNullNoDup: PASS (94 rows, 94 required)` — no NULL address for any of the 16 new rows (groundScene thunks and forwarders, Os.cpp shim, DebugHelp.cpp shim, CuiPreferences statics, SwgCuiChatWindow thunks), no duplicate name, name-set equal.

**Why human:** The runtime null-addr check executes inside the live process. The compile-time `static_assert (94==94)` is proven by the successful build, but only the live boot confirms that all 16 new function addresses resolve non-null at runtime (the forwarder addresses are emitted by GroundScene.cpp — their non-null status requires the lib to actually be loaded and the symbols present in the exe's symbol table at boot).

#### 2. D3D11 non-regression (rasterMajor=11)

**Test:** Boot once under `rasterMajor=11` — SwgClient window must come up, reach character select, and not crash.

**Expected:** Window opens and client reaches character select with no unhandled exception or crash.

**Why human:** The Phase-38 changes are entirely in Win32/exe code (under `#if !defined(_WIN64)` guards and Win32-only ClCompile conditions), but the new includes in `utinni_advertise.cpp` (`SwgCuiChatWindow.h`, `CuiPreferences.h`, `GroundScene.h`) are compiled into the Win32 exe regardless of which renderer DLL is loaded. A latent initialization order or include-path issue could theoretically surface at runtime under gl11. Requires a real boot to confirm.

#### 3. Live inject-smoke (Utinni injection, out of repo)

**Test:** Inject Utinni into the running `SwgClient_d.exe` (or `_r.exe`) and observe Utinni's endpoint-resolution log.

**Expected:** No `0xC0000005` access violation. Utinni logs confirm `endpoints: resolved 94/94 by name` (all 16 new contract names — `groundScene::*`, `client::wndProc`, `client::writeMiniDump`, `config::setModalChat`, `config::getModalChat`, `cuiChatWindow::*` — resolve to non-null addresses). The DX11 overlay leg additionally requires the consumer-side `tryInstall()` device+context guard (documented in the handback §4a) to avoid the `0xC0000005 WRITE target=0x34`.

**Why human:** Out-of-repo step requiring the Utinni binary. Validates the byte-identical re-sync is consumable by the Utinni consumer, and that the 16 new row names match Utinni's expected contract names exactly (any name-string mismatch would show as unresolved in this smoke).

---

### Gaps Summary

No gaps. All 9 observable truths are VERIFIED at the code level. All required artifacts exist and are substantive. All key links are wired. Anti-pattern scan found no blockers.

The 3 human verification items are the **consolidated maintainer boot-smoke** that was explicitly deferred by design (BUILD-GATE-AUTONOMOUS execution mode for this run, per the verification context). They are human-needed items, not code gaps. The automated build-gate evidence (0 unresolved external symbol, undecorated dumpbin export on _d ord 52 / _r ord 51, static_assert 94==94, byte-identical Utinni re-sync) was produced and documented in the 4 SUMMARY files.

---

_Verified: 2026-06-22T23:00:00Z_
_Verifier: Claude (gsd-verifier)_
