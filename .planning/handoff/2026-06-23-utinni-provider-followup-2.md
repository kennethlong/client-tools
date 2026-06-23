# Utinni Provider Follow-Up #2 — detour re-advertise (confirm), CuiMediator deactivate crash (fix), embed-resize path (confirm)

**Date:** 2026-06-23
**Status:** HANDBACK — provider (swg-client-v2) response to the two 2026-06-22 Utinni handoffs.
**Author:** swg-client-v2 Phase-38 gap-closure 38-06.
**Audience:** Utinni (consumer) + the swg-client-v2 maintainer (live re-test owner).
**Source of truth:** this file lives in swg-client-v2; the consumer pulls v3 from swg-client-v2 HEAD
himself. **The provider did NOT write to `D:/Code/Utinni`** (per the standing constraint — the
consumer re-syncs v3 + wires bindings).

Responds to:
- `2026-06-22-utinni-detour-vs-call-followup.md` (item 1 — 4 detoured endpoints need REAL entry)
- `2026-06-22-utinni-dx11-advertised-client-gap.md`, LIVE-SMOKE RESULTS (item 2 — `CuiMediator::deactivate()`
  null-deref; item 3 — embed resize gap, resolved by the post-v3 `client::wndProc` binding)

---

## Item 1 — detour endpoints re-advertise by REAL entry: **ALREADY DELIVERED in 38-05** (confirm-only, no new code)

The four DETOURED rows already advertise their REAL engine code entry, and
`UTINNI_HOOKPOINTS_VERSION` is already **3**. Shipped + maintainer-approved in 38-05
(`2026-06-23-utinni-detour-entries-fixed.md`). 38-06 only re-verified; **no code changed**.

Verified in `src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp`:

| row | advertised address | mechanism |
|---|---|---|
| `groundScene::update` | `utinni_groundSceneUpdateRealEntry()` (:344) | in-TU friend real-entry accessor (private method) |
| `groundScene::handleInputMapEvent` | `utinni_groundSceneHandleInputMapEventRealEntry()` (:346) | in-TU friend real-entry accessor (private method) |
| `cuiChatWindow::enableTextInput` | `pmfRealEntry(&SwgCuiChatWindow::acceptTextInput)` (:357) | PMF code-component (public non-virtual; delta==0) |
| `cuiChatWindow::chatEnterHandler` | `pmfRealEntry(&SwgCuiChatWindow::performEnterKey)` (:360) | PMF code-component (public non-virtual; delta==0) |

The 6 CALLED rows keep their call-through forwarders (correct there). `pmfRealEntry()` carries a
`delta==0` hard gate (DEBUG_FATAL + nullptr → caught by `utinni_verifyNoNullNoDup()`), so a wrong /
silent-dead entry surfaces as a null row.

`src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.h`: `#define
UTINNI_HOOKPOINTS_VERSION 3` (:54) — "ADDRESS-CORRECTNESS only … SAME 94-name set". The 2→3 bump is
behind an unchanged name set, so the consumer required-set is unaffected.

**dumpbin evidence (this 38-06 build, undecorated export present in both configs):**
```
Debug:    83   52 00B292D0 GetEngineHookPoints = _GetEngineHookPoints
Release:  82   51 00700650 GetEngineHookPoints = _GetEngineHookPoints
```
Row count `static_assert(94 == 94)` holds (94 `UTINNI_HOOKPOINT(...)` rows in the `.inc`; compiles
clean in Debug+Release).

**Consumer action:** nothing to re-sync from the provider beyond pulling v3 from swg-client-v2 HEAD
(the `.h`/`.inc` were re-synced byte-identical in 38-05). The 4 detoured bindings are now safe to wire.

---

## Item 2 — `CuiMediator::deactivate()` null-deref: **FIXED (guard + one-shot diagnostic)**

**File:** `src/engine/client/library/clientUserInterface/src/shared/core/CuiMediator.cpp`,
`CuiMediator::deactivate()` (~:460).

### Diagnosis (from reading the source — not the live repro)

The crash symbolized to `CuiMediator.cpp:494` (the `getSettingsEnabled()`/`saveSettings()` path,
`READ target=0x00000014`). That attribution is a **symbolizer artifact**, not the real fault:

- `m_thePage` is a `UIPage&` **reference** member and is **PROVEN VALID at deactivate entry** —
  `m_thePage.SetSelected(false)` (:467) runs before the crash. So the null is **not** the page, and
  `saveSettings()`'s `getPage().GetSize()` is not the real fault.
- `saveSettings()` (:1230) is small + `const` → **inlined into `deactivate()`**, so the symbolizer
  attributes the fault near its call site at :494 even though the executing code is further down.
- The genuine UNGUARDED null deref in this teardown block is the object-callback cleanup loop (:513):
  `for (it = m_objectCallbackVector->begin(); ...)`. `m_objectCallbackVector` is an
  `ObjectCallbackVector *` **pointer** member (CuiMediator.h:423), dereferenced with only an
  `if (eventCallback)` gate — **no null check on the vector pointer itself**.
- `performDeactivate()` (virtual, empty in base, overridden by subclasses) runs just above (:478) and
  can leave a subclass mediator half-torn-down. During Utinni's embed focus/activate churn
  (`WM_ACTIVATE INACTIVE` immediately after the RESID-04 embed-reassert watchdog re-styled the window),
  `m_objectCallbackVector` can be null → `->begin()` on a null MSVC `std::vector*` reads ~`null+0x14`,
  consistent with the reported `+0x14` target.
- Corroboration that this member is known-nullable: `unregisterMediatorObjects()` (:1170) already
  guards the SAME member with `if (m_objectCallbackVector && !m_objectCallbackVector->empty())`. The
  deactivate-path loop simply lacked the equivalent guard.

### The fix (minimal, behavior-preserving)

1. Changed the loop gate from `if (eventCallback)` to `if (eventCallback && m_objectCallbackVector)`.
   Normal path: unchanged (vector non-null → loop runs). Torn-down path: skip the loop instead of
   null-deref'ing.
2. Added a ONE-SHOT `WARNING` (the macro `CuiMediator.cpp` already uses at :1472) that fires when
   `eventCallback` is non-null **but** `m_objectCallbackVector` IS null, including
   `m_mediatorDebugName.c_str()` and the text
   `"CuiMediator::deactivate: null m_objectCallbackVector (half-torn-down during focus/activate churn)
   -- skipped callback cleanup"`. `WARNING` (not `DEBUG_WARNING`) so it fires in the maintainer's
   **Release** live re-test; static one-shot flag so it cannot flood.
3. No UB null-check added on the `m_thePage` reference (it is valid). The change is surgical to the
   one genuinely-unguarded vector deref.

### Honest open question

The exact null member could **not be 100%-confirmed without the live repro**. The fix targets the one
genuinely-unguarded deref in the symbolized path (`m_objectCallbackVector`) and adds the diagnostic so
the maintainer's next live run **confirms** whether the crash is fully resolved (no further fault) or
faults elsewhere (in which case the WARNING text + mediator debug-name pinpoints the next member). The
`+0x14` target, the inlined-`saveSettings` symbolizer artifact, the proven-valid page, and the
already-guarded twin at :1170 all point at `m_objectCallbackVector` as the fault — but the diagnostic
is the live confirmation.

**Joint note (per gap-doc Finding 1):** Utinni may also be able to **damp the focus churn** (the
RESID-04 embed-reassert watchdog bouncing focus between the TJT host HWND and the owned popup is what
exposes the half-torn-down window). Provider hardens the deref; Utinni reducing the churn frequency is
complementary.

---

## Item 3 — embed resize path (`wndProc → WM_SIZE → displayModeChanged → ResizeBuffers`): **CONFIRMED on our side** (read-only, no code)

`src/engine/shared/library/sharedFoundation/src/win32/Os.cpp`:

- `utinni_osWindowProc(HWND, UINT, WPARAM, LPARAM)` (:1614, `#if !defined(_WIN64)`) returns
  `Os::WindowProc(h, m, w, l)` — `__stdcall` preserved (it is a real `LRESULT CALLBACK` window-proc, not
  the `__thiscall`/MI emulation). Our v2 shim routes to the real WndProc. Advertised as `client::wndProc`
  (38-02), a CALLED endpoint (`CallWindowProc`).
- `Os::WindowProc` WM_SIZE case (:1277): on `hwnd == ms_window && wParam != SIZE_MINIMIZED`, it drives
  `ms_displayModeChangedHookFunction()` (:1293; deferred to drag-end during an interactive
  ENTERSIZEMOVE/EXITSIZEMOVE bracket, immediate for programmatic/embedded-reparent/maximize/restore).
  That hook is the renderer's resize → `ResizeBuffers` path (Phase-19 / DX11 spec §6, added for exactly
  this embed case; idempotent, self-guards on unchanged/zero size).

**The gap is consumer-side and already understood** (gap-doc Finding 2): on the advertised client
`PanelGame.WndProc` only forwards to SWG's WndProc when `swgWndProcValid`, which is false because
`client::wndProc` is unbound in the current name set. **Fix: bind `client::wndProc` in the post-v3
consumer pass** — already advertised (`utinni_osWindowProc`, 38-02); `getSwgWndProcExport()`
(`client.cpp:326`) returns the resolver-managed `swg::client::wndProc`, so once bound, `swgWndProcValid`
flips true → forwarding restored → `WM_SIZE` reaches the client → `ResizeBuffers` fires. No provider
change. It rides the gated post-v3 binding wiring (it is a CALLED endpoint → call-through shim is the
correct mechanism; not subject to the detour-vs-call finding).

**Consumer heads-up (the one gotcha):** the WM_SIZE handler gates on `hwnd == ms_window`, so the
consumer must forward WM_SIZE with the **SWG window handle (`ms_window`)**, not the host-panel HWND —
otherwise the size branch is skipped and `ResizeBuffers` never fires even with `client::wndProc` bound.

---

## Build-gate evidence (38-06)

BUILD-GATE AUTONOMOUS (live inject-smoke is maintainer-only → DEFERRED). All gates pass:

- `clientUserInterface.lib` rebuilt **Debug + Release** (Win32) with the `CuiMediator.cpp` change
  (`/nodeReuse:false /m:1`, serial).
- `SwgClient` relinked **Debug + Release** (Win32) after deleting the staged + compiled exes to force
  the relink. Both staged fresh into `stage/` (`SwgClient_d.exe`, `SwgClient_r.exe`).
- **0 `unresolved external symbol`** in both link logs (`/FORCE` downgrades them to warnings — grepped
  to 0; exit 0 ≠ clean link, so this grep is the real gate). 0 Error(s).
- **dumpbin** undecorated `GetEngineHookPoints` present in both `SwgClient_d.exe` (`83 52 … =
  _GetEngineHookPoints`) and `SwgClient_r.exe` (`82 51 … = _GetEngineHookPoints`).
- `static_assert(94 == 94)` holds (94 `UTINNI_HOOKPOINT(...)` rows; `utinni_advertise.cpp` compiles
  clean in both configs).

**DEFERRED to the maintainer:** boot-smoke (dual-renderer boot to char-select) + the live Utinni
inject re-test that confirms item 2 (the one-shot WARNING will fire iff the half-torn-down path is hit;
absence of the `0xC0000005` at `CuiMediator::deactivate()` after the prior ~5-min repro = resolved).
