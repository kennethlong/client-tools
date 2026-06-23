---
phase: 38-utinni-advertised-client-coverage-completion
plan: 06
subsystem: client-ui
tags: [utinni, getenginehookpoints, cui-mediator, null-deref, embed-focus-churn, win32, gap-closure, confirm-only]

# Dependency graph
requires:
  - phase: 38-utinni-advertised-client-coverage-completion
    provides: "94-row GetEngineHookPoints at VERSION 3 with the 4 DETOURED rows advertising REAL engine entries (38-05); utinni_osWindowProc->Os::WindowProc WM_SIZE->displayModeChanged embed-resize path (38-02 / Phase-19 / DX11 spec §6)"
provides:
  - "CuiMediator::deactivate() null-guard on m_objectCallbackVector (behavior-preserving) + one-shot WARNING diagnostic -- fixes the ~5-min client crash under Utinni embed focus/activate churn (WM_ACTIVATE INACTIVE)"
  - "CONFIRMED: 4 detoured rows advertise real entries at VERSION 3 (item-1, already delivered 38-05; no new code)"
  - "CONFIRMED: utinni_osWindowProc -> Os::WindowProc WM_SIZE -> ms_displayModeChangedHookFunction -> ResizeBuffers embed-resize path on the provider side (item-3; no code; hwnd==ms_window forwarding heads-up captured)"
affects: [utinni]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Teardown null-guard: a pointer member dereferenced in a deactivate/cleanup loop must be null-checked when a virtual performDeactivate() (subclass-overridable) runs first and can half-tear-down the object -- mirror the existing guarded twin (unregisterMediatorObjects :1170) rather than trust an outer gate (eventCallback)"
    - "One-shot WARNING (static bool flag, codebase WARNING macro that fires in Release) at a guarded teardown skip = a live-confirmation diagnostic the maintainer's next inject either silences (resolved) or surfaces with the mediator debug-name (faults elsewhere)"

key-files:
  created:
    - .planning/handoff/2026-06-23-utinni-provider-followup-2.md
    - .planning/phases/38-utinni-advertised-client-coverage-completion/38-06-SUMMARY.md
  modified:
    - src/engine/client/library/clientUserInterface/src/shared/core/CuiMediator.cpp
  confirmed-no-change:
    - src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp
    - src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.h
    - src/engine/shared/library/sharedFoundation/src/win32/Os.cpp

key-decisions:
  - "The real fault in deactivate() is the UNGUARDED m_objectCallbackVector->begin() loop (:513), NOT the symbolized saveSettings()/:494 line. m_thePage (a reference) is PROVEN VALID at entry (SetSelected at :467 runs pre-crash); saveSettings() is small+const -> inlined -> symbolizer attributes the fault near its inlined call site. The +0x14 target == ->begin() on a null MSVC vector* (~null+0x14)."
  - "Fix = change `if (eventCallback)` to `if (eventCallback && m_objectCallbackVector)` (behavior-preserving: normal path unchanged, torn-down path skips the loop) + a one-shot WARNING with m_mediatorDebugName. Mirrors the already-guarded twin at unregisterMediatorObjects() :1170, which proves this member is known-nullable."
  - "HONEST: the exact null member is not 100%-confirmable without the live repro. The fix targets the one genuinely-unguarded deref in the symbolized path; the diagnostic confirms on the maintainer's next live inject (silence = resolved; WARNING fire = faults elsewhere, debug-name pinpoints the next member)."
  - "Item-1 (v3 + 4 real-entry rows) was ALREADY delivered in 38-05 -- 38-06 only re-verified via source read + dumpbin; NO code change. Item-3 (embed-resize path) is read-only CONFIRMED on the provider side; the gap is the consumer binding client::wndProc post-v3, plus the hwnd==ms_window forwarding heads-up."
  - "No UB null-check added on the m_thePage reference (it is valid). Change kept surgical to the one unguarded vector deref."

requirements-completed: []

metrics:
  duration: ~35min
  tasks: 3
  files-changed: 1
  completed: 2026-06-22
---

# Phase 38 Plan 06: Utinni Provider Follow-Up #2 — deactivate() crash fix + detour/resize confirms Summary

CuiMediator::deactivate() null-guard on the unguarded `m_objectCallbackVector` cleanup loop (the real
fault behind the symbolized :494 crash) + a one-shot WARNING diagnostic — fixes the ~5-min client crash
under Utinni's embed focus/activate churn; item-1 (v3 detour real-entries) and item-3 (embed-resize
path) confirmed read-only with no code change.

## What this plan did

Three deliverables in priority order, acting on the two 2026-06-22 Utinni handoffs.

### Item 1 — detour endpoints re-advertise by REAL entry: CONFIRM-ONLY (already delivered 38-05)

The four DETOURED rows (`groundScene::{update,handleInputMapEvent}`,
`cuiChatWindow::{enableTextInput,chatEnterHandler}`) already advertise their REAL engine code entry,
and `UTINNI_HOOKPOINTS_VERSION` is already **3** — shipped + maintainer-approved in 38-05. Verified by
source read:
- `utinni_advertise.cpp` :344/:346 use `utinni_groundScene*RealEntry()` (in-TU friend accessors,
  private methods); :357/:360 use `pmfRealEntry(&SwgCuiChatWindow::acceptTextInput / ::performEnterKey)`
  (PMF code-component, public non-virtual, delta==0).
- `utinni_engine_hookpoints.h` :54 `#define UTINNI_HOOKPOINTS_VERSION 3` (same 94-name set).
- dumpbin: undecorated `GetEngineHookPoints` exported from both `SwgClient_d.exe` + `SwgClient_r.exe`.

**No code change.**

### Item 2 — CuiMediator::deactivate() null-deref: THE FIX

`src/engine/client/library/clientUserInterface/src/shared/core/CuiMediator.cpp`, `deactivate()` (~:460).

**Diagnosis (read from source; the symbolized :494 is a red herring):**
- `m_thePage` is a `UIPage&` reference, PROVEN VALID at entry (`SetSelected(false)` at :467 runs before
  the crash). Not the page.
- `saveSettings()` (:1230) is small + `const` → inlined into `deactivate()` → the symbolizer attributes
  the fault near its call site at :494.
- Real fault: the object-callback cleanup loop (:513) `for (it = m_objectCallbackVector->begin(); ...)`.
  `m_objectCallbackVector` is an `ObjectCallbackVector *` pointer member (CuiMediator.h:423), dereferenced
  with only an `if (eventCallback)` gate. `performDeactivate()` (virtual, subclass-overridable) runs at
  :478 and can leave a subclass half-torn-down; during the embed focus churn the vector pointer can be
  null → `->begin()` reads ~`null+0x14`, matching the reported `+0x14` READ.
- Corroboration: `unregisterMediatorObjects()` (:1170) already guards the SAME member with
  `if (m_objectCallbackVector && ...)` — the deactivate loop simply lacked the guard.

**Fix (minimal, behavior-preserving):**
1. `if (eventCallback)` → `if (eventCallback && m_objectCallbackVector)` on the loop gate. Normal path
   unchanged (vector non-null → loop runs); torn-down path skips the loop instead of null-deref'ing.
2. One-shot `WARNING` (the macro CuiMediator.cpp already uses at :1472; fires in Release) when
   `eventCallback` is non-null but `m_objectCallbackVector` is null, including `m_mediatorDebugName` and
   the text "null m_objectCallbackVector (half-torn-down during focus/activate churn) -- skipped callback
   cleanup". Static one-shot flag (no flood).
3. No UB null-check on `m_thePage` (valid). Surgical to the one unguarded vector deref.

### Item 3 — embed-resize path: CONFIRM-ONLY

`src/engine/shared/library/sharedFoundation/src/win32/Os.cpp`:
- `utinni_osWindowProc` (:1614, `#if !defined(_WIN64)`) returns `Os::WindowProc(h,m,w,l)` — `__stdcall`
  preserved.
- `Os::WindowProc` WM_SIZE (:1277): on `hwnd == ms_window && wParam != SIZE_MINIMIZED` drives
  `ms_displayModeChangedHookFunction()` (:1293) → renderer resize → `ResizeBuffers` (Phase-19 / DX11
  spec §6; idempotent).

**No code change.** The gap is consumer-side: bind `client::wndProc` (already advertised v2,
`utinni_osWindowProc`) in the post-v3 pass so `swgWndProcValid` flips true and WM_SIZE reaches the
client. **Heads-up captured in the handback:** the WM_SIZE branch gates on `hwnd == ms_window`, so the
consumer must forward WM_SIZE with the SWG window handle (`ms_window`), not the host-panel HWND.

## Deviations from Plan

None — plan executed exactly as written (one guard + one-shot diagnostic; two read-only confirms).

## Honest open question (carried into the handback)

The exact null member could not be 100%-confirmed without the live repro. The fix targets the one
genuinely-unguarded deref in the symbolized path (`m_objectCallbackVector`); the one-shot WARNING
confirms on the maintainer's next live inject — silence = fully resolved, WARNING fire = faults
elsewhere (the debug-name pinpoints the next member). Utinni may also damp the focus churn (joint, per
gap-doc Finding 1).

## Build-gate evidence (BUILD-GATE AUTONOMOUS — live inject DEFERRED)

- `clientUserInterface.lib` rebuilt **Debug + Release** (Win32) with the CuiMediator change
  (`/nodeReuse:false /m:1`, serial).
- `SwgClient` relinked **Debug + Release** (Win32) after deleting the staged + compiled exes to force
  the relink; both staged fresh into `stage/`.
- **0 `unresolved external symbol`** in both link logs (`/FORCE` masks them → grepped to 0; exit 0 ≠
  clean link). 0 Error(s).
- **dumpbin** undecorated `GetEngineHookPoints`: Debug `83 52 00B292D0 = _GetEngineHookPoints`; Release
  `82 51 00700650 = _GetEngineHookPoints`.
- `static_assert(94 == 94)` holds (94 `UTINNI_HOOKPOINT(...)` rows; `utinni_advertise.cpp` compiles
  clean in both configs).

**DEFERRED to the maintainer:** boot-smoke (dual-renderer boot to char-select) + the live Utinni inject
re-test (the one-shot WARNING is the in-situ confirmation of item-2's root cause).

## Constraints honored

- Client stays Utinni-agnostic (no call into Utinni; no Utinni dependency).
- `D:/Code/Utinni` NOT touched (the consumer re-syncs v3 + wires bindings himself).
- The new guard is a no-op in the normal (non-torn-down) path.

## Self-Check: PASSED
