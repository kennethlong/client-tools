---
spike: 004
name: camera-zoom-lurch
type: standard (recon — diagnosis only, no fix applied yet)
validates: "Given the residual (view-side) cantina door snap, when we trace the chase-camera zoom logic, then the snap is the wall-avoidance pull-in setting m_currentZoom INSTANTLY (no smoothing) when a ray catches the doorframe — a camera lurch, not an object move"
verdict: CONFIRMED (first-person test: snap vanishes — even riding walls/door edges)
related: [001, 002, 003]
tags: [camera, view, door-snap, faceA-residual]
---

# Spike 004: camera-zoom-lurch (recon)

## Context
Spike 003 fixed the AVATAR floor-collision face-B (circle-graze rubberband; rewinds 160→1, measured).
The residual snap the user still feels is **view-dependent** (worse at sharp door-approach angles,
better straight-on, better with camera backed up) and produces **zero collision rewinds** — so it is
NOT avatar collision and NOT a camera *position* pop (obj-0 portal events are constant fixed-length
standoff/collision-ray bookkeeping, ~unchanged pre/post fix). It is a camera/view effect.

## Diagnosis (code trace, FreeChaseCamera::alter, FreeChaseCamera.cpp)
The chase-camera zoom (`m_currentZoom`) has an **asymmetric update**:
- **Smooth ease toward desired zoom** — line **602**: `m_currentZoom = linearInterpolate(m_currentZoom,
  m_zoom, ms_cameraZoomSpeed)`. Rate-limited. ✅
- **Wall-avoidance PULL-IN** — lines **691** (simple) / **739** (4-ray diamond):
  `if (hit) m_currentZoom = linearInterpolate(start_w, end_w, t).magnitudeBetween(start_w)` — set
  **instantly to the collision distance, NO temporal smoothing.** ❌
- Camera moved to it — line **746**: `move_o(Vector::negativeUnitZ * m_currentZoom)`.

So when the camera's `CF_allCamera` collision rays (lines 664/687/721, the fixed ~4/5 m standoff seen
as obj-0 portal events) suddenly catch the **doorframe geometry**, `m_currentZoom` JUMPS inward in one
frame (the lurch); recovery back out is smoothed by line 602. The camera *offset* one block up (line
640) IS smoothed — only the collision pull-in is not.

### Why it matches every observed symptom
- **View-dependent / no collision rewind** — it's the camera, nothing moves.
- **Worse at sharp angle** — the doorframe sweeps across the camera's rays abruptly → frequent
  hit/miss flips → repeated instant pull-ins.
- **Better straight-on** — rays clear or hit cleanly, fewer transitions.
- **Better camera backed-up** — different standoff geometry clears the doorframe.
- **"camera should pass through walls when above/behind"** — the pull-in is exactly what fights that;
  making it instant turns it into a lurch.
- **"a little better after spike 003"** — the avatar collision was a separate, stacked contributor.

## Confirm before fixing
1. **First-person test (user):** zoom fully in → `m_currentZoom < firstPersonDistance` → collision-ray
   block (658-741) is skipped → if the snap vanishes, this is confirmed.
2. **Optional probe:** log `m_currentZoom`, `hit`, `minimumT` per frame near line 739; expect
   m_currentZoom to drop sharply (large negative delta) on the snap frames at the door.

## Candidate fix (NOT applied)
Make the pull-in symmetric with the recovery: rate-limit / interpolate `m_currentZoom` toward the
collision distance instead of snapping (clamp the per-frame *decrease*), or add hysteresis on the
hit→miss transition. The smooth-out path (602) already exists; only the pull-in (691/739) lurches.
Risk: too-slow pull-in lets the near-plane clip the wall briefly — tune the rate.

## Results — CONFIRMED
First-person test (2026-06-20): **no snapping in first-person, even riding interior walls and door
edges.** First-person skips the collision-ray pull-in block (658-741), so the snap vanishing there
proves the residual was entirely the chase-camera zoom-lurch (691/739). The same test also re-confirms
the spike-003 avatar-collision fix holds (riding walls/edges = no rewind snap). The cantina door-snap
was TWO stacked bugs: avatar floor-seam rubberband (FIXED, spike 003) + chase-camera zoom-pull-in
lurch (this — confirmed). Next: smoothing prototype (spike 005).
