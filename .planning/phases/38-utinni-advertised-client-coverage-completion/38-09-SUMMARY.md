---
phase: 38-utinni-advertised-client-coverage-completion
plan: 09
subsystem: ui
tags: [cuimediator, garbagecollect, use-after-free, reentrancy, shutdown, exitchain, clientUserInterface]

# Dependency graph
requires:
  - phase: 38-utinni-advertised-client-coverage-completion
    provides: "38-06/38-08 per-site deactivate null-guards (symptom fixes for the same freed-mediator-deactivate UAF)"
provides:
  - "CuiMediator::garbageCollect re-entrancy guard + erase-first/restart loop — ROOT fix for the intermittent shutdown UAF (eip=0 null-vtable call)"
  - "Supersedes the per-site member guards 38-06 (m_objectCallbackVector) + 38-08 (m_callback); they remain as harmless defense-in-depth"
affects: [shutdown-teardown, exitchain, cui-mediators, utinni-provider]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Re-entrancy guard (static bool) for self-cascading static-container teardown"
    - "Erase-before-deactivate/delete + restart-on-mutation scan (cascade-safe static-vector reaping)"

key-files:
  created:
    - .planning/phases/38-utinni-advertised-client-coverage-completion/38-09-SUMMARY.md
    - .planning/handoff/2026-06-23-cuimediator-garbagecollect-reentrancy-fix.md
  modified:
    - src/engine/client/library/clientUserInterface/src/shared/core/CuiMediator.cpp

key-decisions:
  - "Fixed at the garbageCollect layer (re-entrancy guard) — NOT the dtor self-remove (Fix1 DROPPED: Codex+Sonnet proved no external direct-delete; the re-entrant cascade is the real mechanism)"
  - "Did NOT touch SwgCuiAllTargets or CuiWorkspace — the guard alone breaks the re-entrant nested-delete-during-iteration invariant violation"
  - "Kept UIManager::garbageCollect() OUTSIDE the guard (after s_collecting=false), matching original semantics"

patterns-established:
  - "Static-container teardown self-cascade: guard re-entry, defer nested reaps to the outer/next pass"

requirements-completed: []

# Metrics
duration: 18min
completed: 2026-06-23
---

# Phase 38 Plan 09: CuiMediator garbageCollect Re-entrancy Fix Summary

**Root-fixed the intermittent shutdown use-after-free (eip=0 null-vtable call in CuiMediator::deactivate) by adding a re-entrancy guard + erase-first/restart loop to CuiMediator::garbageCollect — the nested garbageCollect cascade (outer GC -> CuiWorkspace::updateMediatorEnabledStates -> SwgCuiAllTargets::removeUnusedStatusPages -> nested GC -> CuiWorkspace::removeMediator) that deleted a mediator mid-iteration is now blocked.**

## Performance

- **Duration:** ~18 min
- **Started:** 2026-06-23T18:25Z (approx)
- **Completed:** 2026-06-23T18:43Z (approx)
- **Tasks:** 1 (atomic edit) + build gate (8 builds)
- **Files modified:** 1 source (`CuiMediator.cpp`)

## Accomplishments
- Rewrote `CuiMediator::garbageCollect(bool force)` with:
  - A `static bool s_collecting` re-entrancy guard — a nested (re-entrant) call returns early; refcount-0 mediators are reaped by the continuing outer loop (force=true at shutdown) or the next frame's collect.
  - Erase-BEFORE-deactivate/delete + restart-on-mutation scan (`removedAny` / `break`) so a cascade that mutates `s_mediators` can never invalidate a live iterator.
- `UIManager::garbageCollect()` kept outside the guard (after `s_collecting=false`), preserving original semantics.
- Dtor, `SwgCuiAllTargets`, and `CuiWorkspace` left untouched (per spec).
- No shared-header change -> no plugin ABI cascade; `UTINNI_HOOKPOINTS_VERSION` still **3** (confirmed at `utinni_engine_hookpoints.h:54`).

## The Measured Root (CONSULT-48)

- Crash: `c0000005`, **`eip=00000000`** — a CALL through a null vtable slot (`getCallbackObject`, `CuiMediator.h:366`, virtual, `+0x30`). A null slot ⇒ the mediator's vtable is freed/mid-destruction ⇒ `garbageCollect` is calling `deactivate()` on an **already-freed mediator** (UAF).
- Stack: fires during **ExitChain SHUTDOWN** teardown (`CuiManager::remove <- ExitChain::run`), no Utinni injected.
- Mechanism (Codex-traced, Debug-FATAL-confirmed): a `deactivate()` running inside `CuiWorkspace::updateMediatorEnabledStates` (`m_iteratingEnabledStates`) cascades into `SwgCuiAllTargets::removeUnusedStatusPages`, which calls `garbageCollect` **RE-ENTRANTLY**; the nested `delete -> CuiWorkspace::removeMediator` violates the iteration invariant. In Debug this is `DEBUG_FATAL CuiWorkspace.cpp:335 "Can't removeMediator [SwgCuiGroundRadar] during m_iteratingEnabledStates"`; in Release (DEBUG_FATAL is a no-op) it is the use-after-free.
- This is the SAME freed-mediator-deactivate behind the three prior faces: 38-06 (`m_objectCallbackVector`), 38-08 (`m_callback`), and this null-vtable call. **This plan supersedes the per-site member guards** (which remain as harmless defense-in-depth).

## Correctness verification (per spec notes — not blindly trusted)
1. `deactivate()` does NOT depend on `this` being in `s_mediators` (CONSULT-48) -> erase-before-deactivate is safe.
2. The dtor's `find(this)` at ~:265 misses (already erased) -> no double-erase, no spurious dtor FATAL.
3. Trailing `UIManager::garbageCollect()` stays outside the guard.
4. Dtor self-remove "Fix1" intentionally DROPPED (no external direct-delete proven by Codex+Sonnet).
5. `SwgCuiAllTargets` / `CuiWorkspace` untouched.

## Task Commits

1. **Re-entrancy guard + erase-first/restart in CuiMediator::garbageCollect** — see plan metadata commit below (single atomic source change).

## Files Created/Modified
- `src/engine/client/library/clientUserInterface/src/shared/core/CuiMediator.cpp` — rewrote `garbageCollect(bool force)` with the re-entrancy guard + erase-first/restart loop.

## Build Gate (BUILD-GATE AUTONOMOUS — did NOT run the client)

Per AGENTS.md: `$env:MSBUILD` (VS18 v145), PowerShell, `/nodeReuse:false`, serial (`/m:1`); rebuilt `clientUserInterface` (Debug AND Release) before relinking each `SwgClient` config; deleted staged exes first to force relink.

| Config | Platform | clientUserInterface | SwgClient | unresolved external symbol | LNK1181 | staged |
|--------|----------|---------------------|-----------|----------------------------|---------|--------|
| Debug   | Win32 | built | relinked | **0** | 0 | `stage/SwgClient_d.exe` |
| Release | Win32 | built | relinked | **0** | 0 | `stage/SwgClient_r.exe` |
| Debug   | x64   | built | relinked | **0** | 0 | `stage-x64/SwgClient_d.exe` |
| Release | x64   | built | relinked | **0** | 0 | `stage-x64/SwgClient_r.exe` |

All four build logs (`.planning/research/build-38-09-*.log`) grep **0** `unresolved external symbol` and **0** `LNK1181`. All four exes freshly staged (mtimes 13:33–13:34 2026-06-23). The repro is Win32 but the milestone is x64; clientUserInterface links into SwgClient on both — built both.

## Decisions Made
See key-decisions frontmatter. The core call: fix the re-entrant cascade at the `garbageCollect` layer (guard), drop the dtor self-remove, leave the cascade sites (`SwgCuiAllTargets`/`CuiWorkspace`) untouched.

## Deviations from Plan
None — the exact specified edit was applied verbatim. (Note: the objective referenced `CONSULT-48-SYNTHESIS.md`; that file does not exist in the tree — `CONSULT-48-EVIDENCE-mediator-uaf.md` + `CONSULT-48-codex.out` carry the verdict and were used instead. Not a code deviation.)

## Issues Encountered
None during the build gate. The two spurious `NOLOG` lines in an early grep loop were a shell-quirk artifact (`grep -c` emitting `0` then a no-fire `|| echo`), re-verified clean with explicit per-file greps.

## User Setup Required
None.

## Next Phase Readiness
- Boot-smoke + live-inject DEFERRED to maintainer (build-gate-autonomous mode; did not run the client).
- Maintainer control: re-run the SAME exit/shutdown that produced the 18xx dumps — expect NO crash, and the **Debug build should NO LONGER hit the `CuiWorkspace.cpp:335` FATAL** (the re-entrant GC is gone). If the Debug FATAL still fires, the re-entrant path has another caller — paste the new FATAL caller-chain.
- 38-06 + 38-08 per-site guards remain in place as harmless defense-in-depth.

## Known Stubs
None.

## Self-Check: PASSED

- FOUND: `src/engine/client/library/clientUserInterface/src/shared/core/CuiMediator.cpp` (modified)
- FOUND: `.planning/phases/38-utinni-advertised-client-coverage-completion/38-09-SUMMARY.md`
- FOUND: `.planning/handoff/2026-06-23-cuimediator-garbagecollect-reentrancy-fix.md`
- FOUND commit: `09a2c5bd8` (fix(38-09)) — 5 files, 0 accidental deletions
- BUILD-GATE: 4/4 logs (`build-38-09-{dbg,rel}-{win32,x64}.log`) — 0 unresolved external symbol, 0 LNK1181; 4 fresh exes staged

---
*Phase: 38-utinni-advertised-client-coverage-completion*
*Completed: 2026-06-23*
