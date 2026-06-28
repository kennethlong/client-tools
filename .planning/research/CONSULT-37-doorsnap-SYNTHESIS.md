# CONSULT-37 SYNTHESIS — door-snap FIX direction (4-AI crew, code-verified)

Date 2026-06-20. Builds on CONSULT-36 root cause. Crew: Codex (ordering/blast), Cursor (portalId
provenance), Opus (fix-selection math), Sonnet (lateral/data-side). Evidence:
`CONSULT-37-doorsnap-fix-EVIDENCE.md`. **This supersedes CONSULT-36's "cell-gate is THE fix" — the
crew split it into two roots and killed the portalId fix family.**

## CONVERGENT FINDINGS (multi-AI, verified in current tree)

1. **TWO distinct roots, two fixes needed** (Opus; verified):
   - **Root A** — rare ~4 m vertical pop: cross-cell **stale floor contact**. Center walk
     (`pathWalkPoint`), contact cell (`foyer1`) != player parent cell (`world`). Captured frame 2850.
   - **Root B** — frequent ~75% horizontal stutter: **same-cell**, the footprint **circle** grazes an
     uncrossable boundary edge the **center clears**. Born in the SEPARATE circle machine
     `Floor::canMove`→`FloorMesh::pathWalkCircle`→`pathWalkCircleGetIds`→`IntersectCircleSeg`
     (FloorMesh.cpp :3333/:2506/:2665/:2826), contact recorded at **:2899**. `canMove` does NOT call
     `pathWalkPoint` for the contact — only as a center sub-probe (:2737).

2. **The entire portalId fix family is DEAD** (Cursor + Codex, independently, code-verified):
   `FloorTri::getEdgeType()` (FloorTri.h:253) and `isCrossable()` (:222) **virtualize**: any edge with
   `portalId != -1` ALREADY reports `FET_Crossable`. `canExitEdge` reads `getEdgeType()`. Therefore an
   edge that records a circle contact **must have `portalId == -1` by construction.**
   - Fix #4 (hoist portalId in pathWalkPoint): redundant (portal edges never reach the HitEdge return)
     AND unsafe (Codex: stale portalId → `CMR_MoveOK` + wrong-cell recursion via :2018/:1812).
   - Fix #5 (data-side promote portalId→crossable): a **no-op** — getEdgeType already does it virtually.
   - Fix #1 gated on the grazed edge's OWN portalId: never fires (edge is -1).
   - => Face B is NOT a portal-classification problem. It is geometry/numerics.

3. **Fix #3 (cell-gate) is the narrowest, safest fix for Root A** (Codex + Opus). Only touches
   `findContactWithFloor` (CollisionResolve.cpp:741); skip contacts whose `getCell()` != object's
   `getParentCell()`. On the exit frame this drops the stale `foyer1` contact. Risk: a *legitimate*
   cross-cell contact at a real handoff (ramp into the cell you're leaving) — verify on ramps/POBs.

4. **Root B mechanism, exact** (Cursor + code read): in `pathWalkCircleGetIds` the suppression filter
   at **:2835-2838** applies only for center result `ExitedMesh/HitPortalEdge/HitEdge` — it
   **EXCLUDES `PWR_WalkOk`**. So when the center walks cleanly (face-B signature: center fine, circle
   catches), a forward circle graze (:2895 branch) records a hard contact with **NO epsilon guard**
   (unlike the penetrating :2877 branch which checks `wallEpsilon`). => self-sustaining ~0.1–0.5 m
   sideways shoves over 20–30 frames.

## WHY RETAIL/SWGEmu IS CLEAN ON IDENTICAL DATA (reconciles user evidence)
Same `.flr`, same algorithm, same ~0.5 footprint, retail = no snap; our MSVC v145/C++20 rebuild =
snap; LESS in Release, LESS in gl11 (both change frame pacing → per-frame `delta` → whether the graze
lands inside the step window at :2833/:2895). => Root B is a **borderline forward-graze test tipped by
step size + FP codegen**, not a logic difference in classification. The `:2895` branch's missing
epsilon tolerance is the structural reason a borderline graze becomes a hard rewind in our build.
NB: a global FP-mode flip was already tried (~10%) and x64 SSE did not fix it — so the fix is a
SPECIFIC tolerance/gate at :2895, not a blanket FP change.

## CANDIDATE FIXES — post-crew
**Face A:** #3 cell-gate (lead) · #2 cross-cell rewind gate (symptomatic fallback).
**Face B (portal family dead — these remain):**
- **#7a** — extend the :2835-2838 suppression to `PWR_WalkOk`: if the center clears the tri, don't let
  a circle boundary graze generate a blocking contact. Highest value, highest blast (all circle-vs-wall).
- **#7b** — portal-ADJACENCY gate (Opus fallback): suppress the :2899 contact only when the grazed
  uncrossable edge's TRI also carries a portal-bearing edge (doorway tris only). Narrower than #7a.
- **#7c** — add a graze tolerance / `wallEpsilon` guard to the :2895 forward-graze branch (it currently
  has none; :2877 does). Lowest blast, directly matches the step-size/numerical signature; the
  retail-parity-shaped fix.
- **#6** — radius/aperture (art widen threshold, or shrink crossing footprint). Argued AGAINST by
  retail-clean-on-same-geometry; keep as last resort.

## NEXT: KEYSTONE PROBE (spike 001) — disambiguates #7a/#7b/#7c/#6
Instrument `pathWalkCircleGetIds` at the contact-record sites (:2885 and :2899) — per door cross log:
`circleHitTriId, circleHitEdge, getPortalId(edge), raw m_edgeTypes[edge], getEdgeType(edge),
getNeighborIndex(edge), centerWalkResult, hitRange.min/max, stepDistance, contactDistance, wallEpsilon,
contactCell vs parentCell`. Plus measure the cantina door aperture gap vs footprint radius. Expected:
`centerWalkResult==PWR_WalkOk`, `getPortalId==-1`, `:2895` branch, contactDistance small. That picks
#7c (small tolerance) vs #7a/#7b (structural) vs #6 (genuinely too-narrow door).

## DEAD — do NOT propose: #4, #5 (virtualization), + CONSULT-36 reverted trio
(cell-reversal guard a6df32348, prior resetPos tweak, _fpreset/MXCSR).
