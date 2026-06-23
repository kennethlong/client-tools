---
phase: 38-utinni-advertised-client-coverage-completion
plan: 08
subsystem: swgClientUserInterface (HUD window manager)
tags: [hud, deactivate, teardown, null-deref, m_callback, exitchain, garbage-collect, finding-1, release-safe, gap-closure, build-gate]

# Dependency graph
requires:
  - phase: 38-utinni-advertised-client-coverage-completion
    provides: "38-06 established the Finding-1 deactivate-teardown null-deref class (CuiMediator::deactivate guard on m_objectCallbackVector). This plan is the SECOND, distinct member of that family in a different file/member, captured by a real in-game shutdown mdmp (no Utinni)."
provides:
  - "SwgCuiHudWindowManagerGround::handlePerformDeactivate now guards its 6 m_callback->disconnect(...) calls with `if (m_callback)` (+ Release-visible WARNING on the torn-down path). The BASE SwgCuiHudWindowManager::handlePerformDeactivate is self-guarded (early-return on !m_WindowManagerActive); the Ground override never mirrored that guard and null-derefed at the first disconnect when m_callback (owned+deleted by the base dtor) was already torn down (garbageCollect -> deactivate during ExitChain shutdown / focus churn). THIS IS THE FIX."
affects: [swgClientUserInterface, clientUserInterface]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Override-must-mirror-base-guard: when a base virtual (handlePerformDeactivate) early-returns on a torn-down sentinel (m_WindowManagerActive) but its OWN cleanup is gated, a subclass override that runs MORE cleanup after calling the base inherits NONE of that guard. The override must re-check the same nullable member (here m_callback, owned by the base) before its extra disconnects. Keep the `Base::handlePerformDeactivate()` call OUTSIDE/BEFORE the guard (the base is self-guarded)."
    - "Per-site teardown guard (symptom fix) vs. teardown-ordering fix (root cure): 38-06 and 38-08 each null-guard one deactivate site. The deeper root is CuiMediator::garbageCollect deactivating a (partly) destructed mediator; a teardown-ordering fix would be the real cure. Per-site guards are correct + behavior-preserving stopgaps; the ordering fix is deferred for a joint decision."

key-files:
  created:
    - .planning/handoff/2026-06-23-hud-deactivate-null-guard.md
    - .planning/phases/38-utinni-advertised-client-coverage-completion/38-08-SUMMARY.md
  modified:
    - src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHudWindowManagerGround.cpp

key-decisions:
  - "DISTINCT crash: this is a SECOND deactivate-teardown null-deref, separate from BOTH 38-07 (the Tatooine NVIDIA texture-create driver null, nvwgf2um) AND 38-06 (CuiMediator::deactivate m_objectCallbackVector). Real crash captured 2026-06-23 in stage/SwgClient_r.exe-unknown.0-20260623152839.mdmp: null m_callback deref in SwgCuiHudWindowManagerGround::handlePerformDeactivate during ExitChain shutdown teardown (garbageCollect -> deactivate on a torn-down manager). No Utinni involved -- it is the in-game/shutdown version of the Utinni gap-doc Finding-1 CuiMediator-deactivate crash."
  - "Root cause: m_callback is owned by the BASE SwgCuiHudWindowManager (new'd in ctor :155, `delete m_callback; m_callback = 0;` in dtor :324-325). Base handlePerformDeactivate (:487) is SAFE -- it early-returns on `!m_WindowManagerActive` (:489), and m_callback is only nulled in the dtor (by which point m_WindowManagerActive is already false). The Ground override (:659) calls the base THEN runs its own 6 unguarded `m_callback->disconnect(...)` (:663-669) -> on a torn-down manager (m_callback==0) the base bails but Ground null-derefs at the first disconnect. Ground never mirrored the base's guard."
  - "Fix = wrap the SIX disconnects in `if (m_callback) { ...verbatim... } else { WARNING(...) }`; the leading `SwgCuiHudWindowManager::handlePerformDeactivate();` stays BEFORE the guard (base is self-guarded). Behavior-preserving: m_callback non-null -> all 6 disconnects run exactly as before. m_callback torn down -> skip + a Release-visible named WARNING so a future occurrence self-logs."
  - "Scope is ONLY SwgCuiHudWindowManagerGround. The Space variant does NOT override handlePerformDeactivate (uses the guarded base). The dozens of other mediators own separate m_callback members and are not implicated by this crash -- left untouched."
  - "WARNING(true, (...)) (sharedFoundation/Fatal.h, Release-visible) chosen over DEBUG_WARNING (the base file uses DEBUG_WARNING at :567/:591, but that is a no-op in the Release client where the crash lives). WARNING(true, (...)) is the established idiom across sibling files in this same dir (SwgCuiAvatarSelection/SwgCuiToolbar/SwgCuiTicketPurchase/SwgCuiShipView/SwgCuiQuestBuilder) -> in scope via the FirstSwgClientUserInterface PCH chain, no new include."
  - "Touches NO shared header (single .cpp) -> NO plugin ABI cascade. UTINNI_HOOKPOINTS_VERSION untouched (no edit to the contract source); still 3. No dumpbin needed (contract source unmodified)."
  - "Built BOTH platforms (the edited file lives in swgClientUserInterface, linked into SwgClient on Win32 + x64): the crash repro is Win32 (the ...152839.mdmp is SwgClient_r.exe) but the active milestone is x64, so both staging dirs must carry the fix."

requirements-completed: []

metrics:
  duration: ~25min
  tasks: 1
  files-changed: 1
  completed: 2026-06-23
---

# Phase 38 Plan 08: HUD deactivate-teardown m_callback null-guard Summary

A SECOND, distinct deactivate-teardown null-deref -- the in-game / ExitChain-shutdown version of the
Utinni gap-doc **Finding-1** CuiMediator-deactivate crash. **NOT** the 38-07 Tatooine texture-create
NVIDIA driver null (`nvwgf2um`), and **NOT** the 38-06 `m_objectCallbackVector` null in
`CuiMediator::deactivate` -- a different member (`m_callback`) in a different file
(`SwgCuiHudWindowManagerGround.cpp`). Real crash captured 2026-06-23 in
`stage/SwgClient_r.exe-unknown.0-20260623152839.mdmp` (no Utinni): a null `m_callback` deref in
`SwgCuiHudWindowManagerGround::handlePerformDeactivate` during shutdown teardown
(`garbageCollect -> deactivate` on a torn-down manager).

## Root cause

- `m_callback` is owned by the **base** `SwgCuiHudWindowManager`: `new`'d in its ctor
  (`SwgCuiHudWindowManager.cpp:155`), `delete m_callback; m_callback = 0;` in its dtor (`:324-325`).
- The base `SwgCuiHudWindowManager::handlePerformDeactivate` (`:487`) is **safe**: it early-returns
  `if (!m_WindowManagerActive)` (`:489`); `m_callback` is only nulled in the dtor (by which point
  `m_WindowManagerActive` is already false), so the base's own `m_callback->disconnect(...)` block
  (`:507-513`) is unreachable on a torn-down manager.
- The **bug**: `SwgCuiHudWindowManagerGround::handlePerformDeactivate` (`:659-670`) calls the base
  THEN runs its **own** 6 `m_callback->disconnect(...)` (`:663-669`) with **no guard**. On a torn-down
  manager (`m_callback == 0`) the base bails but Ground null-derefs at the first disconnect (`:663`).
  Ground never mirrored the base's guard.

## The fix

`src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHudWindowManagerGround.cpp`,
`handlePerformDeactivate()`:

- Keep `SwgCuiHudWindowManager::handlePerformDeactivate();` first (the base is self-guarded).
- Wrap the SIX `m_callback->disconnect(...)` calls verbatim in `if (m_callback) { ... }`.
- `else { WARNING(true, ("SwgCuiHudWindowManagerGround::handlePerformDeactivate: m_callback already
  torn down -- skipped disconnects (deactivate on a destructed manager)")); }` -- Release-visible, so
  a future occurrence self-logs by name.
- A comment ties it to the crash + the base-has-a-guard-Ground-didn't root + the Finding-1 family
  (38-06 sibling).

Behavior-preserving for the normal case: `m_callback` non-null -> all 6 disconnects run exactly as
before, byte-for-byte the same order.

## Deeper root (deferred for a joint decision)

The real fault upstream is `CuiMediator::garbageCollect` deactivating a (partly) destructed mediator.
The per-site guards (38-06 `m_objectCallbackVector`, 38-08 `m_callback`) handle the **symptom**; a
`CuiMediator`-teardown-ordering fix would be the real **cure**. Deferred for a joint decision (see
handback).

## Build gate (PASS)

Built serially, `$env:MSBUILD` (VS18 v145), PowerShell, `/nodeReuse:false`, staged exes deleted to
force relink. **4 builds, all 0 `unresolved external symbol` (links under /FORCE; grepped each log),
0 `LNK1181`:**

| Target    | Platform | Debug | Release |
| --------- | -------- | ----- | ------- |
| SwgClient | Win32    | OK    | OK      |
| SwgClient | x64      | OK    | OK      |

Freshly staged: `stage/{SwgClient_d,SwgClient_r}.exe` (Win32) + `stage-x64/{SwgClient_d,SwgClient_r}.exe`
(x64), all re-dated 2026-06-23. No shared-header change -> no plugin ABI cascade; the Utinni contract
source is untouched, so `UTINNI_HOOKPOINTS_VERSION` stays 3 (no dumpbin needed).

## Boot/live verification -- DEFERRED to maintainer

Per execution mode (do NOT run the client), boot-smoke + live-inject are deferred. Maintainer control
(handback): re-run the same exit / shutdown that produced `...152839.mdmp` -- if the named WARNING
fires, the torn-down path is caught (no more null deref); if it crashes elsewhere in `deactivate`,
paste the new stack.

## Deviations from Plan

None of substance. Built both platforms (Win32 + x64) as the execution mode directed; the edited file
links into SwgClient on both. Plan executed exactly as written.

## Self-Check: PASSED

- Modified file present: `SwgCuiHudWindowManagerGround.cpp` (git status confirms the single source file
  dirty).
- Created files present: this SUMMARY + the handback.
- Build artifacts staged: `stage/{SwgClient_d,SwgClient_r}.exe` + `stage-x64/{SwgClient_d,SwgClient_r}.exe`
  all re-dated 2026-06-23.
