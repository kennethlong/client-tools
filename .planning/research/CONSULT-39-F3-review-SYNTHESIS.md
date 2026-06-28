# CONSULT-39 SYNTHESIS — adversarial review of F3 (sub-step). 4-for-4 verdict.

Date 2026-06-20. Reviewers: Opus (eliminate-vs-slice math), Cursor (mechanics/edge), Codex
(hook/rescaling), + own review. Proposal: CONSULT-39-F3-substep-PROPOSAL.md.

## VERDICT: F3 is a MITIGATION KNOB, not a proven root fix. Demoted.
All four converged independently:
- **Slice, not eliminate.** For the dominant face-B case (persistent parallel-wall grazing, 136/161
  contacts), N sub-steps recover `D·sinθ` and lose `D·cosθ` that **sum to the identical total**
  (`SingleSlide` is a linear projection; CollisionUtils.cpp:249-315). Net per-frame displacement
  unchanged. The visible residual stays ~0.10-0.23 m ≫ ~1-2 cm perception floor.
- **The one nonlinearity (the `:452` slide-discard dot-test) is SIGN-ONLY → scale-invariant** (Opus):
  a small remainder slides in the same direction as a large one, so sub-stepping cannot flip a
  discarded slide into a surviving one for parallel geometry. (It only flips at concave/corners.)
- **F3 "wins" ONLY via detection-threshold** (Cursor): small sub-steps fall below the graze-detection
  band → `PWR_WalkOk` on early chunks → fewer contacts. That is the SAME free relief as higher FPS —
  not better sliding, and not reproducing retail's per-frame input/velocity integration (coincidence,
  not mechanism).
- **Within-frame subdivision doesn't render between sub-steps** (own review) → can't deliver the
  high-FPS visual benefit; only a changed TOTAL outcome helps, which is the resolution unknown.

## If F3 is ever used: H1, never H2
- **H2 (cap delta inside findContactWithFloor) is broken** without refactor: floor sub-stepped while
  extent/obstacle sweeps stay full-step → `findFirstContact` mis-orders contacts by `contact.m_time`
  (CollisionResolve.cpp:672-692). Requires `m_time` rescale `fullT=(k+t)/N` (Cursor+Codex) AND
  per-chunk locator advance, else contacts fire up to (N-1)/N too early.
- **H1 (lower CollisionWorld.cpp:639 divisor) is the only coherent hook** but: runs
  updatePreResolve/resolveCollisions/updatePostResolve/storePosition N× (snapToCellFloor is once,
  after the loop — Codex correction), and **fires setPosition_p→positionChanged→portal-walk N× per
  frame** — multiplying cell-transition notifications, the door-snap's own mechanism. Biggest
  regression surface. ~3× resolves, worse-than-linear in crowded zones.

## REDIRECT — the real root + the leading fix (reviewer-ranked)
Root = RESOLUTION: `resetPos = getBegin(m_cellB)` rewinds to frame-START regardless of contact time
(CollisionResolve.cpp:320,337) + the `:452` slide-discard. And/or DETECTION: the unguarded FWD branch
(FloorMesh.cpp:2909-2916).

Cursor's adversarial fix ranking (best→worst): **A → B(gated H1) → E → raw H1 → H2.**
- **Variant A (LEAD): mirror the PEN `wallEpsilon` guard onto the FWD branch** (FloorMesh.cpp:
  2909-2916), gated on `centerWalk==PWR_WalkOk`. Zero extra resolves, detection-side, `centerWalk`
  available here (lost by resolution). Cheapest, sharpest. CAUTION (Opus/own): must not become a naive
  clip — verify it doesn't let the circle penetrate a wall it should stop at.
- **Resolution fix:** relax `:452` for shallow horizontal-wall grazes, and/or rewind to the contact
  point instead of `getBegin` (:320). Higher blast (global slide / oscillation guard exists for a
  reason).

## DECISIVE MEASUREMENT (settles fix choice; refined by Cursor+Opus+Codex)
Instrument `resolveCollisions` (:446-463) for center-OK floor circle grazes, log: `contact.m_time`,
`|slidDelta|`, `slidDelta·localStartDelta` sign + **whether `:452` bailed**, final pushed waypoint vs
contactPoint, and the **NET displacement (intended `begin+delta` vs actual final) — NOT the rewind**
(rewind ≈ step by design). Then:
- `:452` bails often → slide discarded → resolution fix at `:452`.
- `:452` doesn't bail but net error large → slide runs yet motion lost → deeper look / Variant A.
- net error ≈ `D·cosθ` only (into-wall) → that's CORRECT collision; the "stutter" is the
  full-rewind-then-replay visual → attack `:320`.

## BANNED (carried): portalId fixes, doorway-locality, CONSULT-36 reverted trio, naive global F3 default.
