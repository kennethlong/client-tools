---
spike: 001
name: mechanism-probe
type: standard
validates: "Given a cantina door cross, when we log the footprint-circle blocking-contact record sites in pathWalkCircleGetIds, then we learn which branch (PEN vs FWD) records face B, the hit edge's portalId/edgeType/neighbor, the center walk result, and step/radius — disambiguating fix #7c vs #7a/#7b vs #6"
verdict: VALIDATED
related: []
tags: [collision, instrument, keystone, door-snap]
---

# Spike 001: mechanism-probe (KEYSTONE)

## What This Validates
Given a cantina main-door crossing, when the footprint **circle** records a blocking contact in
`FloorMesh::pathWalkCircleGetIds`, then we capture exactly **why face B (the ~75% horizontal stutter)
forms a hard contact** — settling the fix direction with runtime ground truth instead of static
inference.

Static analysis (CONSULT-37, 4-AI) already predicts: face B's hit edge has `portalId == -1`
(`getEdgeType` virtualizes portal edges to crossable, so a recorded contact edge can't be portal),
`centerWalkResult == PWR_WalkOk` (center clears, circle catches), and the **FWD** branch
(FloorMesh.cpp:2895 — no `wallEpsilon` guard) is the record site. This spike **measures** it.

## The probe (in-tree `_DEBUG` instrumentation — throwaway, kept until fix verifies)
`src/engine/shared/library/sharedCollision/src/shared/core/FloorMesh.cpp`:
- Added `#include "sharedFoundation/Os.h"` (under `_DEBUG`) for the frame id.
- **`CORNERSNAP-CIRCLE` log at the two circle-contact record sites** in `pathWalkCircleGetIds`:
  - **branch PEN** (~:2888) — the already-penetrating branch that *does* check `contactDistance < wallEpsilon`.
  - **branch FWD** (~:2902) — the forward-graze branch with **NO** `wallEpsilon` guard (suspected face-B source).
- Existing `CORNERSNAP-PORTAL` / `CORNERSNAP-RESOLVE` / `CORNERSNAP-CELLJUMP` probes remain (give
  face-A / cross-cell context + the actual rewind).

Logged fields: `frame, branch, tri, edge, portalId, edgeType, neighbor, centerWalk, min, max, step,
[contactDist, wallEps (PEN only)], radius`.

### Decode legend
- **PathWalkResult (centerWalk / walk):** 0 MissedStartTri · 1 CantEnter · 2 DoesntEnter · 3 HitBeforeEnter ·
  **4 WalkOk** · 5 WalkFailed · **6 HitEdge** · **7 HitPortalEdge** · 8 HitPast · 9 InContact ·
  10 ExitedMesh · 11 CenterHitEdge · 12 CenterInContact · 13 StartLocInvalid · 14 Invalid.
- **FloorEdgeType (edgeType):** the **virtualized** value — `portalId != -1` always reports Crossable.
  Raw type only differs when portalId == -1 (the expected face-B case). Types incl. Uncrossable / Crossable / WallBase.
- **neighbor == -1** = mesh-boundary edge (no in-mesh neighbor) — the seam/jamb class.
- `wallEpsilon` hardcoded **0.01** (FloorMesh.cpp:66); `step` = per-frame `delta.magnitude()`.

## How to Run
1. Build x64 Debug SwgClient (probe is `_DEBUG`-only):
   `& $env:MSBUILD src/build/win32/swg.sln /t:SwgClient /p:Configuration=Debug /p:Platform=x64 /nodeReuse:false`
   (delete `src/compile/win32/SwgClient/x64/Debug/SwgClient_d.exe` first to force relink; grep log
   for `unresolved external symbol` == 0). Auto-deploys to `stage-x64/`.
2. Start the **non-debugger** capture (background):
   `powershell -File tools/setup/dbwin-cornersnap-capture.ps1`
   (MUST be this, NOT cdb — a real debugger resets FPU precision and perturbs the bug.)
3. Launch `stage-x64/SwgClient_d.exe` (rasterMajor=5), go to the Mos Eisley cantina, **walk the main
   door back and forth several times** (slow and fast approaches). Logs →
   `stage-x64/cornersnap-x64-capture.log` (CORNERSNAP-only) + `-raw.log`.

## What to Expect
On a face-B stutter frame, a `CORNERSNAP-CIRCLE branch FWD` line with `centerWalk 4` (WalkOk),
`portalId -1`, `neighbor -1`, and a small `step`. Correlate its `frame` with a `CORNERSNAP-RESOLVE`
rewind on the same/next frame. If instead we see `branch PEN` with `contactDist` near `wallEps`, fix
#7c targets the wrong branch. If `portalId != -1` ever appears on a recorded contact, the static model
is wrong and we re-open the portal angle.

## Observability
`tools/setup/dbwin-cornersnap-capture.ps1` — OutputDebugString/DBWIN reader, filters `CORNERSNAP-*`.

## Investigation Trail
- 2026-06-20: probe authored after CONSULT-37 4-AI synthesis killed the portalId fix family and
  localized face B to the unguarded FWD branch at FloorMesh.cpp:2895. Capture pending (user pilots).

## Results — VALIDATED (capture `capture-0620.log`: 243 CIRCLE, 176 RESOLVE, 640 PORTAL)

### CONFIRMED (keystone answered)
- **portalId fix family is DEAD — runtime-proven.** ALL 243 recorded circle contacts:
  `portalId -1`, `neighbor -1`, `edgeType 0 (FET_Uncrossable)`. The snap edge is always a plain
  uncrossable **mesh-boundary** edge with **no portal**. Fixes #1/#4/#5 (anything keyed on the grazed
  edge's portalId) cannot fire. ✓
- **Face B mechanism confirmed exactly.** Avatar = `radius 0.5000` (162 events; a second
  `radius 0.0500` object also present, 81 events). Avatar circle contacts are **161/162
  `centerWalk 4` (PWR_WalkOk)** — center walks cleanly, circle catches. ✓
- **The unguarded FWD branch (FloorMesh.cpp:2895) is the dominant record site** for avatar face-B:
  118 FWD vs 43 PEN. ✓
- **Causal link proven:** 105 of 161 avatar face-B circle-contact frames coincide with a
  `CORNERSNAP-RESOLVE` rewind. Coincident rewinds are **same-Y horizontal** shoves ~0.1–0.23 m
  (e.g. frame 2272 `(3468.80,5.00,-4851.25)->(3468.97,5.00,-4851.42)`; frame 2436
  `(46.33,0.10,-4.18)->(46.21,0.10,-4.14)`) — the face-B stutter. Consecutive frames show net forward
  progress with per-frame backward clipping = the visible jitter. ✓

### SURPRISE — reshapes the fix
- **Face B is PERVASIVE, not doorway-specific.** Only **25 of 161** avatar face-B frames are within
  ±3 frames of a PORTAL (cell-transition) event; **136 are NOT.** The grazes happen wherever the
  0.5 m footprint circle passes close to an uncrossable floor-mesh boundary edge (foyer seams AND
  ordinary interior walls; hit tris spread across 15/121/5/123/26/77/82/...). The cantina door is the
  *most frequent/visible* site (tight foyer, many seams), not a unique one.
  => **#7b (portal-adjacency gate) is INSUFFICIENT** — it would miss the 136 non-doorway grazes.
- **`min` (contact time in step) spans the full range, not just ~0:** 38 near-full-rewind (<0.01),
  34 [0.01–0.1], 28 [0.1–0.4], 18 mostly-allowed (>0.4). => **#7c (a `wallEpsilon`-style guard on the
  FWD branch) is INSUFFICIENT alone** — it only suppresses the ~30% near-zero `min` cases; the
  forward grazes with large `min` still record.

### Fix-direction implication (for spike 002)
Face B is "the footprint circle records a HARD blocking contact (→ horizontal rewind) from grazing an
uncrossable boundary edge **whenever the center walks OK**, everywhere — not just at the door." The
viable levers narrow to:
- **#7a** — suppress/soften the circle contact when `centerWalk == PWR_WalkOk` (catches all cases;
  highest blast: changes footprint-vs-wall everywhere → wall-clip risk; needs broad regression).
- **resolution-side** — the contact may be a *legitimate* wall-slide that our build resolves as a
  visible backward stutter instead of a smooth tangential slide (fits retail-clean-on-identical-assets
  + framerate sensitivity). Would move the fix into `CollisionResolve` slide/replay, not detection.
Disambiguating these two is spike 002's job (a resolution-side probe + crew round).

### Note
Static model was right on portalId/edge classification; WRONG that the bug is doorway-local. The
runtime capture corrected the spatial scope — exactly why we probed rather than coded the fix.
