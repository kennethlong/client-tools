# CONSULT-39 — ADVERSARIAL REVIEW of F3 (sub-step the collision delta)

You are reviewing ONE concrete proposed fix for the SWG cantina door-snap "face B". Your job is to
**stress-test it and find what's wrong** — correctness holes, regressions, perf, edge cases, wrong
hook. Be adversarial; do not rubber-stamp. If it's sound, say precisely why and under what conditions.

Background facts are LOCKED (measured; do NOT dispute) — see
`.planning/research/CONSULT-38-doorsnap-resolution-EVIDENCE.md` and `CONSULT-38-doorsnap-SYNTHESIS.md`.

## The bug (one line)
Walking, the avatar footprint CENTER walks cleanly (`PWR_WalkOk`) but the offset CIRCLE (r=0.5) grazes
an uncrossable mesh-boundary edge; the whole frame's move (`step`=delta.magnitude, 0.08-0.44 m) is
swept in ONE `pathWalkCircle` call, the graze records a hard contact, the move truncates at
`circleHitTime`, the tangential remainder `step*(1-min)` is lost → per-frame backslide stutter.
Magnitude scales with `step` → worse at low FPS (Debug/gl05), absent in retail (small steps, identical
assets).

## The F3 proposal (Sonnet) — sub-step large walk deltas to match retail's small steps
**Observation:** the engine ALREADY sub-steps, but only above 2 m:
`CollisionWorld::update` (CollisionWorld.cpp:635-706): `moveLength = delta.magnitude();
segments = ceil(moveLength / 2.0f); if(segments<=1) updateSegment(collider,time); else { reset to
frame-start; for each of `segments`: setPosition_p(pos+segmentDelta_p); updateSegment(collider,
segmentTime); } }`. Walk deltas (≤0.44 m) never exceed 2 m → always a SINGLE un-subdivided collision
resolve. The 2 m threshold is a warp safeguard, not a walk-speed one.

**Proposed change:** make sub-meter walk moves subdivide too, so each collision resolve sees a small
delta (like retail's ~30 FPS small steps). Two candidate hook sites:
- (H1) Lower the divisor at CollisionWorld.cpp:639 from `2.0f` to ~`0.15f` (≈2×footprint radius) so the
  EXISTING segment loop (695-706) runs for walk moves. Reuses the proven pattern verbatim.
- (H2) Sonnet's original: cap delta INSIDE `findContactWithFloor` (CollisionResolve.cpp:741) before
  `floor->canMove()`, looping canMove over sub-segments, returning the first blocking contact with
  `circleHitTime` rescaled from sub-step [0,1] to full-frame [0,1].

**Claimed non-regression:** a solid wall still blocks (any sub-step hitting `PWR_HitEdge` on a
non-crossable edge returns a blocking contact); slide still works (normals unchanged), just finer bites.

## QUESTIONS TO ATTACK (answer with file:line)
1. **Does it ELIMINATE the stutter or just SLICE it?** If the avatar walks PARALLEL and close to a wall
   (grazing at EVERY sub-step), does the per-frame total backslide = sum of N small rewinds ≈ the same
   visible error, or does the smaller remainder make each sub-step's SLIDE actually recover the
   tangential motion (so net error → ~0)? Reason from the resolve/slide math (CollisionResolve.cpp:
   421-463, SingleSlide). This is the make-or-break question.
2. **H1 vs H2 — which hook is correct, and what breaks?** H1 re-runs the FULL `updateSegment` (footprint
   snap, cell transitions, obstacle sweep) N× per frame — enumerate side effects (per-substep
   `snapToCellFloor` at :719? cell crossing mid-substep? `storePosition`/`getLastPos` cadence? perf for
   every NPC/creature with a footprint?). H2 changes only floor-contact granularity but leaves obstacle/
   wall-extent collision on the full step — does that desync? Which is safer?
3. **circleHitTime rescaling (H2):** is the contact time the resolver consumes (`contact.m_time`,
   CollisionResolve.cpp:421 atParam) still correct if it came from sub-step k of N? Show the arithmetic
   that would be wrong if naive.
4. **Perf / cadence:** at 0.15 m chunks, max walk 0.44 m → ~3× resolves/frame for the avatar, ×N for
   every footprinted creature in the zone. Acceptable? Any per-substep cost (footprint snap, contact
   list rebuild) that makes it worse than linear?
5. **Does it match retail by COINCIDENCE or by MECHANISM?** Retail ran ~30 FPS (small delta). Is
   "subdivide our large delta" truly equivalent to "retail's small delta", or are there per-frame
   state updates (input, velocity integration) between retail's frames that a pure geometric
   subdivision does NOT reproduce?
6. **Any cheaper/cleaner variant** that gets the same "small effective step" without N× full resolves?

Output: a verdict (sound / sound-with-conditions / broken), the single biggest risk, the better hook
(H1/H2/other), and what to measure to confirm. Cite file:line. Return as text, not a file (Opus/Sonnet)
or to your .out (Codex/Cursor).
