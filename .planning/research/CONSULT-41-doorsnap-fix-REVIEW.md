# CONSULT-41 — CODE REVIEW of the cantina door-snap fix (final minimal set)

Adversarial code review. The fix is **3 changes** in 2 files (the diff is in
`.planning/research/CONSULT-41-doorsnap-fix.diff` — read it). Find correctness bugs, edge cases,
regressions, perf issues, and blast-radius concerns. Be skeptical; assume it's wrong until shown right.
This will ship into shared engine code used by ALL moving objects.

## Background (LOCKED — measured this session, treat as given)
The "cantina door-snap" was TWO stacked bugs, both root-caused by runtime probes:
1. **Avatar floor collision** — the footprint CIRCLE (radius 0.5) grazed an uncrossable floor-mesh
   boundary edge while the circle CENTER walked the triangle cleanly (`PWR_WalkOk`), producing a hard
   contact → `RR_Resolved` rewind → cross-axis rubberband. Measured: 161/162 such contacts had
   `centerWalk==PWR_WalkOk`, `portalId=-1`, `neighbor=-1`, `FET_Uncrossable`. The resolver's slide was
   verified CORRECT (`:452` bail 0/160; tangential preserved). So the contact itself was spurious.
2. **Chase camera** — two INSTANT assignments snapped: the wall-avoidance zoom pull-in (measured
   `dZoom -2.1m` in ONE frame, `colliding=1`) and the shoulder-clip block zeroing `m_offset.x` (0.4m
   sideways jump). The recovery/pop-out was already gradual and NOT the snap.

## The 3 changes (see the .diff)
**A — FloorMesh.cpp `pathWalkCircleGetIds`** (the uncrossable-edge contact branch, ~:2865): add
`if(centerWalkResult == PWR_WalkOk) continue;` — skip recording the footprint-circle contact when the
center walked the tri cleanly. Rationale: center-walkable ⇒ the edge is walkable floor (doorway/seam),
so the circle contact is spurious; a REAL wall blocks the center too (`centerWalk != PWR_WalkOk`).

**B — FreeChaseCamera.cpp pull-in rate limit**: capture `zoomAtFrameStart = m_currentZoom` (before the
recovery lerp), then at the end of the third-person block cap the inward move:
`if (m_currentZoom < zoomAtFrameStart - elapsedTime*cs_cameraPullInSpeed) m_currentZoom =
zoomAtFrameStart - elapsedTime*cs_cameraPullInSpeed;` (cs_cameraPullInSpeed = 8 m/s). Pop-OUT untouched.

**C — FreeChaseCamera.cpp shoulder ease**: in the shoulder-clip block, replace `m_offset.x = 0.0f;`
with `m_offset.x = linearInterpolate(m_offset.x, 0.0f, clamp(0.f, elapsedTime*cs_cameraPullInSpeed, 1.f));`.

## THE HIGH-RISK QUESTION (scrutinize hardest)
Change A runs in shared floor-walk code used by **EVERY footprinted object — players AND NPCs/creatures**,
in EVERY interior, not just the cantina door. Is `centerWalk == PWR_WalkOk` a SOUND invariant for "this
uncrossable boundary edge is safe to let the footprint circle pass"? The feared counterexample: a thin
wall / pillar / doorway NARROWER than the footprint diameter (1.0 m), where the center's point-path
threads the gap (`PWR_WalkOk`) but the 0.5 m-radius BODY should be blocked. With change A, the circle
contact is suppressed → the body clips through. How real is this? Where would it bite (narrow POB
doorways, pillar bases, thin railings, NPC pathing through gaps)?

## Constraints / accepted trade-offs
- The chase camera is allowed to briefly clip/peek through walls when above/behind (user-accepted) —
  so the camera changes (B,C) easing-with-wall-peek is intended, NOT a bug to flag. Focus B/C review on
  CORRECTNESS (math, edge cases, first-person boundary, elapsedTime spikes), not the wall-peek.
- cs_cameraPullInSpeed = 8 m/s is a first-pass tunable.
