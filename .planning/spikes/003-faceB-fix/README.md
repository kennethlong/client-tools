---
spike: 003
name: faceB-fix
type: standard
validates: "Given the spike-002 verdict (spurious circle contact vs a center-walkable seam), when we suppress the footprint-circle contact while centerWalk==PWR_WalkOk, then the cantina door rubberband dies AND real walls / narrow gaps still block (no clip-through)"
verdict: PENDING
related: [001, 002]
tags: [collision, fix, door-snap, faceB]
---

# Spike 003: faceB-fix (detection gate)

## The fix
`FloorMesh::pathWalkCircleGetIds`, in the uncrossable-edge circle-contact branch (the `else` after
`canExitEdge==false`): **if `centerWalkResult == PWR_WalkOk`, `continue`** (skip recording the
contact). Rationale (measured, spikes 001+002): when the circle-center walks the tri cleanly the edge
is walkable floor (doorway/seam), so the footprint-circle's hard contact is spurious and only serves to
pin the avatar's cross-axis to the seam (errXZ up to 0.30 m = the rubberband). Real walls block the
center too (`centerWalk != WalkOk`) and are unaffected.

Throwaway spike change. The CORNERSNAP probes stay in to validate: with the fix, `CORNERSNAP-CIRCLE
... centerWalk 4` events should drop to ~0 and `CORNERSNAP-RESOLVE`/`-NET` rubberbands at the door
should vanish.

## How to Run
Built (x64 Debug) → `stage-x64/SwgClient_d.exe` (gl05).
1. Monitor: `powershell -File tools/setup/dbwin-cornersnap-capture.ps1`
2. Walk the cantina main door back and forth (slow + fast) — **the door rubberband should be GONE.**
3. **Regression check (critical):** walk tight interiors, narrow doorways, pillars, ramps — confirm
   you still **cannot clip through** thin walls / narrow gaps (the one risk: a wall the center's point
   threads but the 0.5 m body shouldn't).

## What to Expect
- Door: no rubberband/pin; smooth crossing.
- Log: `CORNERSNAP-CIRCLE centerWalk 4` count → ~0 (suppressed); door `RESOLVE` rewinds → gone.
- Regression: thin walls / narrow gaps still block.

## Investigation Trail
- 2026-06-20: built after spike 002 falsified the resolution-side fix (`:452` bail=0) and proved the
  contact itself is spurious for center-walkable seams.

## Results
PENDING — awaiting fix-pilot (rubberband gone? + no wall clip-through?).
