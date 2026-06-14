---
phase: 25-cantina-corner-snap-fix
plan: 25 (phase close-out)
status: closed-by-deferral
closed: 2026-06-14
requirements: [HARD-02]
outcome: superseded-and-deferred
executed: false
---

# Phase 25 — Cantina Corner-Snap Fix — CLOSE-OUT (deferral)

**Status: CLOSED-BY-DEFERRAL. HARD-02 not fixed in v2.3; residual parked for the x64 milestone.**
Plan `25-01` (the frame-scoped A→B→A re-entrancy guard) was **NOT executed** — it was superseded
by the diagnostic finding before implementation.

## What happened

The phase planned a re-entrancy/reversal guard in `CellProperty::Notification::positionChanged`,
on the hypothesis that the corner-snap was a same-frame portal ping-pong driven by collision-replay
re-entry. The CONSULT-43 investigation (2026-06-13) overturned that root cause:

1. **Cantina interior snap → resolved-by-config.** The visible interior corner-snap was eliminated
   by configuration, not by an engine source change.
2. **Residual door-snap → a 32-bit build/codegen float transient**, not a portal-replay re-entrancy
   bug. The transient appears in the cell→world Y transform under the 32-bit toolchain. The real fix
   is the ship-D3D11 path or a 64-bit build — so it is **parked for the x64 milestone**, where the
   codegen environment changes anyway.

Because the root cause was not the modeled re-entrancy pattern, implementing the planned guard would
have been treating the wrong mechanism. The guard was therefore not written.

## Disposition

- **Code:** no source change landed for this phase (guard not implemented). The committed CORNERSNAP
  `_DEBUG` instrumentation (`a9b419daf`) is **intentionally retained** as the acceptance harness for
  the x64 follow-up — see the Phase 26 scope note (D-15 stripped, CORNERSNAP probes KEPT).
- **Requirement HARD-02:** closed-by-deferral; residual door-snap tracked for x64.
- **Follow-up:** carried by the x64 work — `.planning/todos/pending/2026-06-13-64bit-x64-port.md`
  and `.planning/todos/pending/2026-05-15-cantina-corner-snap-engine-improvement.md` (kept open as
  the discrete reminder). Debug record: `.planning/debug/cantina-corner-snap.md`. Memory:
  `project_cantina_corner_snap_engine_quirk`.

## Why this is a legitimate close, not an abandonment

Same shape as Phase 27 (HARD-05 satisfied-by-Fix-A, clean port deferred to x64): the investigation
produced the real root cause and a defensible decision (Kenny, 2026-06-13) to defer the 32-bit-only
fix to the milestone where the build target changes. The phase delivered the diagnosis; the fix is
correctly scheduled where it can actually be made.
