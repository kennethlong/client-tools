# Cantina door-snap — SPIKE START (root cause found, fix not written)

**Date:** 2026-06-20 · **Status:** root-caused, PARKED at v3.0 close, ready to spike a fix.
**Milestone context:** v3.0 x64 Port SHIPPED + tagged (`v3.0`). This is the carried-forward
VERIFY-01. Doing the fix **ad-hoc (spike → single fix phase), NOT a new milestone** (too small for
its own milestone; would be folded into a future milestone only if batched with other work).

---

## TL;DR for the next session

The cantina **door-snap** (avatar snaps/stutters crossing the Mos Eisley cantina main door) is a
**real, pre-existing engine bug — NOT bitness- or renderer-related.** It survives into the x64
gl05/D3D9 build, which **falsified** the long-held "x64 deterministic-SSE / D3D11 fixes it"
hypothesis. Root cause is **collision response at the doorway floor-mesh seam**, fully characterized
below. No fix is written yet — that's the spike. **Do NOT re-attempt the 3 already-reverted fixes.**

---

## Root cause (high confidence — live capture + 4-AI crew, see CONSULT-36)

When the player crosses the cantina doorway, the floor-collision walk reacts to the **doorway
FLOOR-MESH SEAM**, not to any wall (user confirmed never touching the door frame/walls). Mechanism:

- The footprint **center** threads the portal cleanly, but the footprint **circle** (~0.5 m radius)
  clips an **uncrossable seam edge** beside the door.
- `FloorMesh::pathWalkPoint` returns a mesh-boundary edge: **with** a valid `portalId` →
  `PWR_HitPortalEdge` (walk code **7**, clean pass-through, NO contact); **without** → `PWR_HitEdge`
  (walk **6**) / `PWR_CenterHitEdge` (**11**) — an *accepted* hit → `RR_Resolved`.
- `CollisionResolve::resolveCollisions` (RR_Resolved branch, `CollisionResolve.cpp:315-339`) then
  rewinds the player to `resetPos = moveSeg.getBegin(moveSeg.m_cellB)` — the **previous frame's
  position**. Two visible faces:
  - **A — rare ~4 m VERTICAL pop:** cross-cell. `getBegin(world)` carries the foyer floor altitude
    (world-Y ~5.10) into the world cell while the player legitimately stands on terrain (~1.06).
    Captured red-handed at frame 2850: `FLOORCELL floorCell 'foyer1' objCell 'world' walk 6 contact 1`
    + `RESOLVE rewind Y 1.063→5.100`.
  - **B — frequent (~75%) ~0.07–0.14 m HORIZONTAL stutter (the everyday complaint):** same-cell, the
    footprint circle grazing seam edges across 6–10 consecutive frames in the tight foyer → ~0.1 m
    sideways shove per frame = "character shifts left/right like a frame was skipped."

Intermittency = whether the seam edge classifies `PWR_HitEdge` (6, snaps) vs `PWR_HitPortalEdge`
(7, clean) for the footprint circle that frame. Door floor-collision is CORRECT for the center
(walk 7) — the bug is the **circle** catching seam edges the center passes.

### Key code (all in `src/engine/shared/library/sharedCollision/src/shared/core/`)
- `CollisionResolve.cpp:284-360` — `resolveCollisions`; the RR_Resolved rewind at **:315-339**
  (`resetPos = moveSeg.getBegin(moveSeg.m_cellB)` :320, `setPosition_p` :337, `moveObjectAlong` :339).
- `CollisionResolve.cpp:741-821` — `findContactWithFloor` (zeroes `delta.y` :762; accepts hit set
  `{HitEdge, InContact, HitBeforeEnter, HitPast, CenterHitEdge, CenterInContact}` at :790 — note
  `PWR_HitPortalEdge`=7 is correctly EXCLUDED).
- `FloorMesh.cpp:2197-2232` — `pathWalkPoint`: boundary edge with `portalId != -1` → PWR_HitPortalEdge,
  else PWR_ExitedMesh. The circle-walk path returns PWR_HitEdge/PWR_CenterHitEdge elsewhere.
- `FloorMesh.cpp:791` `flagCrossableEdges()` — edges flagged crossable/uncrossable by **within-mesh
  triangle adjacency only** (a cross-mesh portal seam has no same-mesh neighbor → risks uncrossable).
- `CollisionEnums.h:229` — `PathWalkResult` enum (0..13). Walk codes seen: 7=HitPortalEdge (clean),
  6=HitEdge, 11=CenterHitEdge, 9=InContact, 3=HitBeforeEnter, 4=WalkOk.

---

## Candidate fixes (for the spike to evaluate — none applied)

1. **Footprint-circle seam handling (the real fix, fixes A + B):** when the player's **center**
   crosses / is on the portal side, treat a portal-ADJACENT uncrossable seam edge as passable for
   the **circle** (don't generate a hard contact from it). Highest value — targets both faces. Most
   invasive (floor-walk / footprint logic). **Spike this first.**
2. **Cross-cell rewind gate (fixes only A):** in the RR_Resolved branch, when `m_cellA != m_cellB`,
   don't rewind to the stale `getBegin(m_cellB)` (the portal walk already placed the player; CELLJUMP
   fired 0/642 so cell membership is self-correcting). Low-risk but only kills the rare vertical pop.
3. **Floor-contact cell gate (probe-DISCONFIRMED as primary):** skipping floor contacts whose
   `getCell() != getParentCell()` only helps the rare cross-cell case A, NOT the common same-cell B.

## Already REVERTED — do NOT re-attempt (this bug has eaten 3 fixes)
- Frame-scoped cell-reversal guard in `positionChanged` (commit `a6df32348`) — pinned cell membership
  wrong → cantina dropped to skybox. The portal ping-pong is SELF-CORRECTING; do not suppress it.
- A prior `CollisionResolve` resetPos tweak — reverted.
- `_fpreset`/MXCSR restore in `endScene` — only ~10%, reverted.

---

## How to reproduce + measure (the spike's instrument)

**Capture tool (reusable, COMMITTED):** `tools/setup/dbwin-cornersnap-capture.ps1` — a
**non-debugger** DBWIN/OutputDebugString reader. MUST use this, NOT cdb: `Report.cpp:166` resets FPU
precision to 53-bit under a real debugger, perturbing this float-sensitive bug. The DBWIN reader sees
TRUE float behavior.

Workflow:
1. Re-add the `_DEBUG` instrumentation probes (specs in CONSULT-36-doorsnap-SYNTHESIS + git history of
   this session; they were REVERTED at park):
   - `CORNERSNAP-PORTAL/RESOLVE/CELLJUMP` — original harness, still in tree
     (`CellProperty.cpp` + `CollisionResolve.cpp`, `#ifdef _DEBUG`).
   - `CORNERSNAP-FLOORCELL` — in `findContactWithFloor` before `return tempContact`, gated
     `if (floorCell != moveSeg.m_cellB)`, logs floorCell vs objCell + walkResult + contact.
   - `CORNERSNAP-FRAMEPOS` — in `CollisionWorld::update` after `moveLength` (~`CollisionWorld.cpp:636`),
     gated to a door box `x∈(3460,3476) z∈(-4858,-4842)`, logs per-frame pointA_w→pointB_w (the
     GAP between cur[N] and prev[N+1] = the collision rewind that frame). Needs `#include
     "sharedFoundation/Os.h"`.
2. Build x64 Debug: `$env:MSBUILD ... /t:SwgClient /p:Configuration=Debug /p:Platform=x64
   /nodeReuse:false` (delete `src\compile\win32\SwgClient\x64\Debug\SwgClient_d.exe` first to force
   relink; grep build log for `unresolved external symbol` == 0). Auto-deploys to `stage-x64/`.
3. Start the monitor (background): `powershell -File tools/setup/dbwin-cornersnap-capture.ps1`.
4. Launch `stage-x64/SwgClient_d.exe` (rasterMajor=5), cantina, walk the main door. Logs →
   `stage-x64/cornersnap-x64-capture.log` (CORNERSNAP-only) + `-raw.log`.

**Repro spot (from this session's capture):** Mos Eisley cantina main door, foyer1↔world portal at
world ≈ (3467–3468, 5.10/1.06, −4850). Door snaps/stutters ~75% of crossings, worse on fast approach.
Player networkId in this session was `127040355`; chase camera = obj `0` (no footprint → not in
FRAMEPOS, confirming it's the character, not the camera).

**Reference (clean):** SWG Restoration x64 D3D9 at `D:\SWG Restoration\x64` traverses this door with
NO snap — binaries only, no source. SWGEmu client also clean.

---

## Constraints / cautions
- **High blast radius:** floor-walk / collision-response changes affect ALL movement. Validate the
  spike fix beyond the cantina — other interiors, ramps, narrow doorways, multi-cell POBs.
- **Spike = throwaway.** Prove the fix kills A+B at the door AND doesn't break movement elsewhere,
  BEFORE writing a real fix phase.
- The CORNERSNAP `_DEBUG` probes are the acceptance harness — keep them in until the fix is verified
  (VERIFY-03 / plan 36-02 stays deferred until then).

## Artifacts
- `.planning/research/CONSULT-36-doorsnap-SYNTHESIS.md` — full 4-AI root-cause synthesis (READ FIRST).
- `.planning/research/CONSULT-36-EVIDENCE-doorsnap.md` — the neutral measured evidence (frame 7353).
- `.planning/research/CONSULT-36-{codex,cursor}.out` — crew call-graph + data-flow traces.
- `.planning/phases/36-verification-cornersnap-cleanup/36-01-SUMMARY.md` — VERIFY outcome record.
- Memory: `project_cantina_corner_snap_engine_quirk` (full history + the falsified/reverted list).
- `tools/setup/dbwin-cornersnap-capture.ps1` — the non-debugger capture tool.
