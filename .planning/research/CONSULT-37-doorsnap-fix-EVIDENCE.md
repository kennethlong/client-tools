# CONSULT-37 — door-snap FIX evaluation — neutral evidence (treat as given)

You are one of several independent consultants. Below are **measured facts and verified code
locations** for a collision bug in an MSVC C++20 fork of the SWG (Star Wars Galaxies) engine. Treat
everything in this file as **given ground truth — do not re-derive it.** Reason only about the
specific question in your task file. Do NOT assume the asker's framing is correct; if the evidence
points elsewhere, say so.

## The bug (measured, not hypothesized)
Crossing the Mos Eisley cantina main door, the player avatar snaps/stutters. Two measured faces:
- **Face A (rare ~4 m vertical pop):** cross-cell. Captured red-handed at frame 2850:
  `FLOORCELL floorCell 'foyer1'(1) objCell 'world'(0) walk 6 contact 1` then a `RESOLVE` rewind
  carrying world-Y 1.06 → 5.10. The player legitimately stepped from foyer1 floor (Y~5.10) onto
  terrain (world cell, Y~1.06) but a stale foyer1 floor contact rewinds them back up.
- **Face B (frequent ~75%, ~0.07–0.5 m horizontal stutter):** SAME-cell. Measured as 20–30
  consecutive `RESOLVE` rewinds (frames 1539–1569) where Y is constant (~-0.895) and the player's
  in-plane position is shoved back ~0.1–0.5 m toward a fixed line each frame.
- The avatar never touches the door frame / walls (user-confirmed). The footprint CENTER threads the
  portal cleanly; the suspicion is the footprint CIRCLE (~0.5 m radius) catches a seam edge.

## Verified code path (current tree, line numbers exact)
All paths under `src/engine/shared/library/sharedCollision/src/shared/core/`.

`FloorMesh::pathWalkPoint` (FloorMesh.cpp ~2120–2233), per hit edge `hitEdgeId`:
1. `:2167` calls `canExitEdge(startLoc, delta, id, false)`.
2. If `!canExitEdge` → returns `PWR_InContact` (:2182, very close) or **`PWR_HitEdge`** (:2190) — and
   RETURNS IMMEDIATELY.
3. Only if `canExitEdge` is TRUE does it reach `:2197 neighborTriId = getNeighborIndex(hitEdgeId)`;
   if `neighborTriId == -1` (mesh boundary) it computes `:2215 portalId = getPortalId(hitEdgeId)` →
   `portalId == -1` → `PWR_ExitedMesh` (:2223), else → **`PWR_HitPortalEdge`** (:2231, the CLEAN
   pass-through, walk 7).

`FloorMesh::canExitEdge` (FloorMesh.cpp 1913–1943): keys ONLY on the edge's crossable flag:
- `:1920 FET_Uncrossable → return false`
- `:1924 FET_Crossable → return true`
- else → geometric `intersectEdge` recursion.

`FloorMesh::flagCrossableEdges` (FloorMesh.cpp :791, called at load): marks an edge crossable using
**within-mesh triangle adjacency only**. Edges start `FET_Uncrossable` (FloorMesh.cpp:1061-1063). A
doorway portal seam is a mesh boundary (`neighborTriId == -1`) → no in-mesh neighbor → stays
`FET_Uncrossable`. The per-edge `portalId` is a SEPARATE attribute (`FloorTri::getPortalId`).

`CollisionResolve::findContactWithFloor` (CollisionResolve.cpp 741–821): runs `floor->canMove(...)`
(which calls pathWalkPoint); at :788-790 it ACCEPTS `{HitEdge, InContact, HitBeforeEnter, HitPast,
CenterHitEdge, CenterInContact}` as a real contact (note `PWR_HitPortalEdge`=7 is correctly EXCLUDED
from this set). A real contact → `RR_Resolved`.

`CollisionResolve::resolveCollisions` (CollisionResolve.cpp 284-360): on `RR_Resolved` (:315) it
rewinds `resetPos = moveSeg.getBegin(moveSeg.m_cellB)` (:320), `setPosition_p` (:337),
`moveObjectAlong(positions)` (:339). For face A, `getBegin(m_cellB)` carries the stale foyer floor Y.

## Candidate fixes on the table (do NOT assume any is correct)
1. **Circle-seam passability** — when center is on/crossing the portal side, treat a portal-adjacent
   uncrossable seam edge as passable for the circle.
2. **Cross-cell rewind gate** — in RR_Resolved, when `m_cellA != m_cellB`, skip the stale
   `getBegin(m_cellB)` rewind. (Fixes only face A.)
3. **Floor-contact cell gate** — in findContactWithFloor, skip floor contacts whose
   `getCell() != object's getParentCell()`. (Prior synthesis favored this; later evidence suggests it
   only helps face A, not same-cell face B.)
4. **Hoist portalId above canExitEdge** — in pathWalkPoint, if the hit edge has `getPortalId != -1`,
   return `PWR_HitPortalEdge` BEFORE the `canExitEdge`/HitEdge return at :2167-2190.

## Already REVERTED — do NOT propose these (each was tried and failed)
- Frame-scoped cell-reversal guard in `CellProperty::positionChanged` (commit a6df32348) — pinned cell
  membership wrong → cantina dropped to skybox. Portal ping-pong is SELF-CORRECTING; do not suppress.
- A prior `CollisionResolve` resetPos tweak — reverted.
- `_fpreset`/MXCSR restore in `endScene` — only ~10% effect, reverted.

## NEW measured evidence (2026-06-20, user-reported — treat as given, weigh heavily)
- The snap happens **LESS in Release** builds than Debug.
- The snap happens **LESS with the gl11 (D3D11) renderer** than gl05 (D3D9).
- The snap **does NOT happen in the retail SWG client** (what SWGEmu players run) — same source
  lineage this repo forked, same `.flr` collision data.
Implication to weigh (do not assume — test against the code): same algorithm + same data, retail
binary = clean, our MSVC v145/C++20 rebuild = snap → suggests a **floating-point / toolchain
numerical regression**, not a logic defect. Release/gl11 both change FRAME PACING → per-frame
movement `delta` magnitude → severity scaling with step size is the fingerprint of a **borderline
epsilon/threshold clip test** being tipped (candidates: `canExitEdge`/`intersectEdge` geometric
tolerance, `wallEpsilon` at FloorMesh.cpp:2173, `calcHitTime`). NB: a prior `_fpreset`/MXCSR restore
gave only ~10% and an x64 (deterministic-SSE) build did NOT fix it — so a naive global-FP-mode flip is
NOT the answer; the divergence is likely in a SPECIFIC borderline geometric test.

## Hard constraints
- **High blast radius:** floor-walk / collision-response changes affect ALL movement in ALL
  interiors, ramps, narrow doorways, multi-cell POBs. Any proposed fix must be argued for
  non-regression, not just door-correctness.
- Reference clients SWG Restoration (x64 D3D9) and SWGEmu traverse this door with NO snap (binaries
  only, no source).
