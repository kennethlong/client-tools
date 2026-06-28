# Task for Cursor (most detailed code/data reader)

Read `.planning/research/CONSULT-38-doorsnap-resolution-EVIDENCE.md` first — LOCKED facts are measured
ground truth; do not dispute. Honor the BANNED framings.

Your angle: **the geometric quality of the wall-slide — does our `pathWalkCircle` + `CollisionResolve`
produce a smooth tangential slide, and where could per-frame jitter / backward net motion enter?**
Work in-repo (read-only, ask mode), cite file:line.

1. In `pathWalkCircleGetIds` after a contact is recorded (`circleHitTime`), trace how
   `pathWalkCircle` (FloorMesh.cpp:2506-2610) converts that into the returned move result / clipped
   delta. Does it compute a tangential slide vector along the hit edge, or just truncate the move at
   `circleHitTime`? Quote the code that produces the post-contact position/delta.
2. Trace `CollisionResolve::resolveCollisions` second overload (CollisionResolve.cpp:364+) and
   `moveObjectAlong` (find it): how are `positions`/waypoints generated and replayed? For a same-cell
   horizontal circle contact, does the replay slide the avatar ALONG the edge for the remaining
   `(1 - circleHitTime)` of the step, or does it leave them at the contact point (= net backward vs the
   intended end)? This determines whether the stutter is "no slide" vs "bad slide".
3. The measured rewinds are 0.10-0.23 m backward but net-forward across frames. Reconcile with the
   code: is the per-frame backward component = `step * (1 - circleHitTime)` (lost forward motion) or an
   actual reversal past the start? Show which.
4. Numerical angle: `IntersectCircleSeg` (Intersect2d) + `canExitEdge(useRadius=true)` +
   `intersectEdge`. Is there a tolerance / wall-thickness / epsilon in the circle-vs-segment math that,
   under a larger `step` (low FPS) or different FP rounding (our toolchain), would record a contact that
   retail's build would not? Identify the exact comparison(s) where step-size or FP rounding tips the
   result. (This tests whether the divergence is detection-numerical vs resolution.)

Output: a file:line account of the slide construction, a verdict on whether the stutter is missing-
slide vs bad-slide vs spurious-detection, and the specific code site to change. If undecidable
statically, give the exact probe (what to log in pathWalkCircle / moveObjectAlong).
