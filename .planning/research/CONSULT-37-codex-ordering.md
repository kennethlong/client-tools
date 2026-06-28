# Task for Codex (repo tracer / call-graph)

Read `.planning/research/CONSULT-37-doorsnap-fix-EVIDENCE.md` first — treat it as given ground truth.

You are working in this repo (read-only). Your angle: **classification ORDER and the blast radius of
candidate fix #4 (hoist the portalId check above canExitEdge in `FloorMesh::pathWalkPoint`).**

Trace and answer precisely with file:line:
1. Every caller of `FloorMesh::pathWalkPoint` and `FloorMesh::canMove` in the engine. Which call
   sites pass the footprint CIRCLE/radius vs the bare center? (look at `useRadius`, `intersectEdge`,
   `Footprint`, `canMove` overloads).
2. If we return `PWR_HitPortalEdge` for ANY hit edge whose `getPortalId(hitEdgeId) != -1` (before the
   `canExitEdge` gate at :2167), enumerate EVERY downstream consumer of that walk result that would
   change behavior. Which code paths treat `PWR_HitPortalEdge` differently from `PWR_HitEdge`?
   (grep `PWR_HitPortalEdge`, `PWR_ExitedMesh`, `PWR_HitEdge` across the tree). What breaks if a
   NON-doorway uncrossable edge happened to carry a stale portalId?
3. Is there any place where an uncrossable edge legitimately ALSO carries a valid portalId and MUST
   still block (i.e., a portal you are not allowed to walk through on foot)? If so, fix #4 is unsafe.
4. Compare fix #4 vs fix #3 (cell gate in findContactWithFloor) purely on call-graph reach: which
   touches fewer distinct consumers?

Output: concrete file:line findings, a verdict on whether #4 is safe, and any consumer that makes it
unsafe. If you cannot determine portalId provenance from code alone, say what runtime probe would.
