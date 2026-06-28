# CONSULT-40 — chase-camera door "snap" residual + the framerate question — EVIDENCE (LOCKED)

Treat the LOCKED facts as measured/confirmed ground truth — do NOT re-derive or dispute. Reason about
the open questions. Engine: MSVC v145/C++20 fork of SWG (leaked 2003-2010 client lineage), now running
at modern framerates (60-144+ FPS) vs the original's ~30 FPS design point.

## Context (prior, LOCKED)
The "cantina door-snap" was TWO stacked bugs. Bug 1 (avatar footprint-circle grazing floor-mesh seams
→ rubberband) is FIXED + verified. Bug 2 (this) is the RESIDUAL, view-side snap:
- **First-person test CONFIRMS it is the chase camera**: in first-person the camera collision-ray block
  is skipped and the snap VANISHES, even riding interior walls and door edges. Zero collision rewinds.
- View-dependent: worse at sharp door-approach angles, better straight-on, better camera-backed-up.

## LOCKED code facts — `FreeChaseCamera::alter` (FreeChaseCamera.cpp)
1. **Zoom recovery (line 602):** `m_currentZoom = linearInterpolate(m_currentZoom, m_zoom,
   ms_cameraZoomSpeed)`. `ms_cameraZoomSpeed` is a **fixed config constant** (per-frame fraction) —
   **NOT scaled by elapsedTime.** Frame-rate DEPENDENT.
2. **Offset smoothing (line 640, adjacent):** `m_offset = linearInterpolate(m_offset, m_desiredOffset,
   elapsedTime)` — IS scaled by elapsedTime. Frame-rate INDEPENDENT. (The engine is inconsistent.)
3. **Wall-avoidance PULL-IN (lines 691 simple / 739 4-ray diamond):** on a collision-ray hit,
   `m_currentZoom = linearInterpolate(start_w, end_w, t).magnitudeBetween(start_w)` — set **INSTANTLY
   to the collision distance, no temporal smoothing.** Then `move_o(-Z * m_currentZoom)` (line 746).
4. Collision rays use `ClientWorld::collide(..., CF_allCamera)` (664/687/721), the fixed ~4/5 m standoff
   (seen as constant obj-0 portal events, ~unchanged before/after bug-1 fix → probe bookkeeping, not a
   position pop).
5. `alter` returns `cms_alterNextFrame` (774) → runs once per render frame; more FPS = more alters.

## Observed dynamics (LOCKED, from the user)
- At sharp door angles the camera lurches inward hard; straight-on / backed-up camera much milder.
- The bug is felt as a sharp pop-in then pop-out (not a slow drift).
- Hypothesis under test: our higher-than-original framerate makes it WORSE.

## OPEN QUESTIONS (reason about these)
1. **Framerate dependence:** quantify how the camera-zoom dynamics change from ~30 FPS to 120 FPS given
   (a) instant pull-in (739) and (b) per-frame-fixed recovery (602, not time-scaled). Does high FPS make
   the lurch/oscillation WORSE (amplitude and/or frequency)? Model m_currentZoom across frames for a
   ray that flickers hit/miss while the avatar crosses the doorframe, at 30 vs 120 FPS.
2. **Correct fix:** is the right fix to (i) time-scale the recovery (use elapsedTime / exponential
   smoothing with dt, like line 640), (ii) smooth/rate-limit the instant pull-in, (iii) add hysteresis
   on hit→miss to kill the flicker, or some combination? What are the side effects of each (near-plane
   wall clip during a slow pull-in; camera lag; etc.)?
3. **Systemic scope:** is `ms_cameraZoomSpeed` the ONLY non-time-scaled per-frame constant, or is this a
   pattern across the camera / movement / animation code that our higher framerate silently degrades?
   (This is the bigger question — name other frame-rate-dependent smoothing/rate constants if found.)

## Hard constraints
- Don't break the protective purpose of the pull-in (camera must not show through walls badly).
- Reference clients (retail/SWGEmu, ~30 FPS-era) do not exhibit this — consistent with a frame-rate
  scaling bug surfaced by modern FPS.
