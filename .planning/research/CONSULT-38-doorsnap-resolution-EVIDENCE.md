# CONSULT-38 — door-snap face B: detection vs resolution — MEASURED evidence (LOCKED axioms)

Round 2. A runtime capture (x64 Debug, gl05/D3D9, non-debugger DBWIN) now exists. The facts below are
**measured ground truth — LOCKED. Do NOT re-derive or dispute them.** Reason only about the open
question. Background (optional): CONSULT-37-doorsnap-SYNTHESIS.md.

## BANNED framings (falsified by the capture — do not propose)
- Any fix keyed on the grazed edge's **portalId** (hoist, data-side promote, portal-seam crossable).
  RUNTIME-PROVEN IMPOSSIBLE: all recorded circle contacts have `portalId == -1`.
- "It's a **doorway/portal-seam**-specific bug." Falsified: 136/161 contacts are NOT near a portal.
- The CONSULT-36 reverted trio (cell-reversal guard a6df32348, prior resetPos tweak, _fpreset/MXCSR).

## LOCKED measured facts (cantina door-snap "face B", the ~75% horizontal stutter)
Engine: MSVC v145/C++20 fork of SWG. Capture = walking the Mos Eisley cantina + main door.
1. **243 footprint-circle blocking contacts recorded** in `FloorMesh::pathWalkCircleGetIds`. ALL have
   `portalId == -1`, `neighbor == -1` (mesh-boundary edge), `edgeType == 0` (FET_Uncrossable).
2. **Avatar footprint `radius == 0.5`** (162 events; a second radius-0.05 object also present, ignore).
   **161 of 162 avatar contacts are `centerWalkResult == PWR_WalkOk` (4)** — the circle-center walks
   the tri cleanly; only the **offset circle** grazes the boundary edge.
3. The contact is recorded on the **FWD branch (FloorMesh.cpp:2895)** in 118/161 avatar cases (43 on
   the PEN branch :2885). FWD records with NO `wallEpsilon` guard; PEN gates on `contactDist<wallEpsilon`.
4. **Causal:** 105 of 161 avatar contact frames coincide with a `CollisionResolve` `RR_Resolved`
   **rewind**. The rewinds are **same-cell, same-Y, horizontal** shoves ~0.10–0.23 m. Consecutive
   frames show **net forward progress with a per-frame backward clip** (the visible jitter). Example:
   frame 2272 `(3468.80,5.00,-4851.25) -> (3468.97,5.00,-4851.42)`; frame 2436
   `(46.33,0.10,-4.18) -> (46.21,0.10,-4.14)`.
5. **PERVASIVE, not doorway-local:** only 25/161 contact frames are within ±3 frames of a cell
   transition; 136 are not. Hit tris are spread across the mesh (15,121,5,123,26,77,82,117,119,...),
   mostly `edge 1`.
6. **`min` = hitRange.getMin() (circle-edge contact time, fraction of the step) spans the whole range:**
   38 are <0.01, 34 in [0.01,0.1], 28 in [0.1,0.4], 18 >0.4. `step` = delta.magnitude() ≈ 0.08–0.44.
   The recorded contact sets `circleHitTime = min` and rewinds accordingly.
7. **Reference clients are clean on IDENTICAL assets:** retail SWG client (SWGEmu) and SWG Restoration
   x64/D3D9 traverse the same `.flr` floor meshes with NO stutter. The snap is **LESS in Release than
   Debug** and **LESS with gl11 than gl05** — both higher framerate / smaller per-frame `step`.

## The RESOLUTION path (verified code)
`CollisionResolve::resolveCollisions` (CollisionResolve.cpp:284-360): on `RR_Resolved` it sets
`resetPos = moveSeg.getBegin(moveSeg.m_cellB)` (:320). For face B, `m_cellA == m_cellB` so `resetPos`
is the **previous-frame position in the same cell**. Then `setPosition_p(resetPos)` (:337) +
`moveObjectAlong(objectA, positions)` (:339) replays the slide waypoints. `findContactWithFloor`
(:741-821) zeroes `delta.y` (:762) and accepts the circle hit at :788-790.

## THE OPEN QUESTION (split on this)
Given facts 1-7: is the divergence from retail in **DETECTION** (our build records circle-vs-boundary
contacts that retail does not — e.g. a numerical/step-size threshold) or in **RESOLUTION** (the
contacts are legitimate wall-slide contacts that retail resolves as a smooth tangential slide and our
build resolves as a visible backward rewind/jitter)? And what is the SAFE fix, given the blast radius
is ALL avatar movement in ALL interiors (a circle grazing any uncrossable boundary edge is normal
wall-walking)?

## Hard constraint
The fix touches shared floor-walk / collision-response code used by every moving object. Any proposal
must argue non-regression for ordinary wall-sliding (you SHOULD stop at a wall; you should NOT clip
through it), not just door-correctness.

## ADDENDUM — measured rewind-vs-step correlation (added after fan-out; LOCKED)
Per-frame correlation of the avatar (radius 0.5, centerWalk==WalkOk) circle contact's `min`/`step`
against the coincident RESOLVE rewind XZ-magnitude:
- **rewindXZ ≈ FULL `step` in every case, INDEPENDENT of `min`.** e.g. min=0.703 step=0.238 →
  rewindXZ=0.238 (NOT step*(1-min)=0.071); min=0.798 step=0.131 → rewindXZ=0.131; min=0.510
  step=0.167 → rewindXZ=0.167; min≈0 cases also rewindXZ≈step. dy≈0.000 throughout (pure horizontal).
- => `resetPos = getBegin(m_cellB)` is the **frame-START**; the resolver **rewinds the ENTIRE frame's
  motion to frame-start then replays the slide**, even when the contact occurred 70-84% through the
  step (i.e. the move was almost complete and the center walked cleanly). The full-undo magnitude (=
  step) is what scales the visible backslide with framerate.
- This points at RESOLUTION (faithfulness of the slide replay after a full rewind), not detection.
