# Task for Codex (repo tracer / call-graph)

Read `.planning/research/CONSULT-38-doorsnap-resolution-EVIDENCE.md` first — the LOCKED facts are
measured ground truth; do not dispute them. Honor the BANNED framings.

Your angle: **trace the RESOLUTION path and decide if the slide replay recovers the tangential motion
or produces a net backward shove.** Work in-repo (read-only), cite file:line.

1. From the recorded circle contact (`circleHitTime, circleHitTriId, circleHitEdge`,
   `hitAnything=true`) in `pathWalkCircleGetIds`, trace UP through `pathWalkCircle` (FloorMesh.cpp:2506)
   → `Floor::canMove` → `CollisionWorld` → `CollisionResolve::resolveCollisions`. What exactly is built
   into `positions` (the slide waypoints)? Does the slide project the remaining motion TANGENT to the
   hit edge (proper wall-slide), or does it stop/clamp at the contact point with no tangential
   component? Quote the waypoint-construction code.
2. `resetPos = moveSeg.getBegin(moveSeg.m_cellB)` then `moveObjectAlong(positions)`. For a SAME-cell
   contact (m_cellA==m_cellB), compute what net displacement results when `circleHitTime` (min) is
   (a) ~0.0 and (b) ~0.7. Does a min~0.7 contact still yield a backward net move, or forward? Show the
   arithmetic from the code.
3. Is there a code path where a circle-only graze (center PWR_WalkOk) is supposed to be a SLIDE rather
   than a blocking RR_Resolved rewind? i.e., does `resolveCollisions`/`findFirstContact` distinguish
   "center cleared, circle grazed" from "center blocked"? If not, that asymmetry (the :2835-2838 filter
   excludes PWR_WalkOk) is the candidate defect — confirm by tracing what consumes `centerWalkResult`.
4. Compare two fix loci on call-graph reach + regression surface: (A) detection — gate the circle
   contact record at FloorMesh.cpp:2895 on something; (B) resolution — change how the slide is built/
   replayed in CollisionResolve. Which is narrower? Which can preserve correct wall-blocking?

Output: a concrete trace of the slide-replay math, a verdict on detection-vs-resolution as the
divergence locus, and the narrower safe fix with file:line. If undecidable from code, name the exact
runtime probe.
