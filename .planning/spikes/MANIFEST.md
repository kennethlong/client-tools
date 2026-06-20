# Spike Manifest

## Idea
Fix the **Mos Eisley cantina door-snap** — a real, pre-existing engine collision bug (NOT
bitness/renderer; survives into x64 gl05/D3D9). Crossing the main doorway, the avatar snaps/stutters.
Two measured faces: **A** = rare ~4 m vertical pop (cross-cell, stale foyer floor contact rewinds the
player back up); **B** = frequent ~75% ~0.1–0.5 m horizontal stutter (same-cell, footprint circle
suspected of grazing seam edges). Root-caused in CONSULT-36; this spike-set proves a fix kills A+B at
the door WITHOUT regressing movement elsewhere, before a real fix phase is written. **Spike =
throwaway; high blast radius (all movement, all interiors).**

## Requirements
[Design decisions that emerged — non-negotiable for the real build. Updated as spikes progress.]

- Any fix MUST be validated beyond the cantina (other interiors, ramps, narrow doorways, multi-cell
  POBs) — floor-walk/collision-response changes affect ALL movement.
- Measurement is via the **non-debugger DBWIN capture** (`tools/setup/dbwin-cornersnap-capture.ps1`),
  NOT cdb — `Report.cpp:166` resets FPU precision under a real debugger and perturbs this
  float-sensitive bug.
- The CORNERSNAP `_DEBUG` probe harness is the acceptance instrument; keep it in until the fix is
  verified (VERIFY-03 / Phase-36 plan 36-02 stays deferred until then).
- Do NOT re-attempt the 3 reverted dead-ends (cell-reversal guard a6df32348, prior resetPos tweak,
  _fpreset/MXCSR).

## Crew findings folded in (CONSULT-37, 2026-06-20)
- **TWO distinct roots** (Opus, verified in code): Root **A** = cross-cell stale contact (center walk
  `pathWalkPoint`, cell-mismatch). Root **B** = same-cell circle-grazes-seam, born in the SEPARATE
  circle machine `pathWalkCircle`→`pathWalkCircleGetIds`→`IntersectCircleSeg` (FloorMesh.cpp
  :2506/:2665/:2826), contact recorded ~:2899. `canMove` calls `pathWalkCircle` (:3333), NOT
  `pathWalkPoint`. **No single patch covers both.**
- **Fix #4 (portalId hoist in `pathWalkPoint`) REJECTED for face B** — wrong organ; the circle contact
  is recorded outside `pathWalkPoint`.
- **THE KEYSTONE UNKNOWN (gates fix family #1/#4/#5):** does face B's grazed edge carry
  `getPortalId != -1`? If it's a plain non-portal threshold edge, all portalId-based fixes miss → the
  root is footprint-radius-vs-door-aperture geometry (Sonnet H1, converges with the framerate/step-size
  evidence). Discriminant = log `triId/edgeId/getPortalId/getEdgeType/hasNeighbor` at the face-B
  rewind cluster + measure door gap vs avatar footprint radius.

## Candidate fixes (post-crew verdict — see CONSULT-37-doorsnap-SYNTHESIS.md)
**Face A (cross-cell vertical pop):**
- **#3 cell-gate** (LEAD) — skip floor contacts whose `getCell()` != object `getParentCell()`
  (CollisionResolve.cpp:741). Narrowest, safest (Codex+Opus). Verify on ramps/multi-cell POBs.
- **#2 rewind gate** — symptomatic fallback if #3 regresses handoffs.

**Face B (same-cell circle-graze, ~75%) — post CONSULT-38/39 crew. portalId fixes DEAD; F3 DEMOTED:**
- **Variant A (LEAD):** mirror the PEN `wallEpsilon` guard onto the unguarded FWD branch
  (FloorMesh.cpp:2909-2916), gated on `centerWalk==PWR_WalkOk`. Detection-side, zero extra resolves,
  cheapest/sharpest (Cursor #1). CAUTION: must not become a naive wall-clip.
- **Resolution fix:** relax `:452` slide-discard for shallow horizontal grazes and/or rewind to contact
  point not `getBegin` (:320). Addresses the measured full-frame rewind; higher blast.
- **F3 sub-step (DEMOTED):** 4-for-4 adversarial verdict = slice/mitigation knob, not root fix
  (SingleSlide linear → sums to same error; `:452` sign-only → scale-invariant). If ever used: H1 not
  H2, and beware N× portal-walk notifications. See CONSULT-39-F3-review-SYNTHESIS.
- DECISIVE PROBE: instrument `:452` bail + NET displacement (intended vs actual, not rewind) → picks
  Variant A vs resolution fix.

**DEAD (do NOT propose):** #4 hoist-portalId (redundant+unsafe), #5 data-side promote (no-op) — both
killed by `getEdgeType()` portal virtualization (FloorTri.h:253). Plus CONSULT-36's reverted trio.

## Spikes

| # | Name | Type | Validates | Verdict | Tags |
|---|------|------|-----------|---------|------|
| 001 | mechanism-probe | standard (instrument) | KEYSTONE. Log circle-contact `portalId/edgeType/neighbor/centerWalk/min/step/radius` + correlate with rewinds | **VALIDATED** | collision, instrument, keystone |
| 002 | resolution-probe | standard (instrument) | Log `:452` slide-discard + NET intended-vs-actual displacement → settle detection-vs-resolution | **VALIDATED** | collision, instrument, resolution |
| 003 | faceB-fix | standard | Suppress circle contact when `centerWalk==PWR_WalkOk` (edge is walkable) → kill avatar floor-seam rubberband | **VALIDATED** (rewinds 160→1; first-person ride-the-walls = no snap) | collision, fix |
| 004 | camera-zoom-lurch | standard (recon) | Residual snap = chase-camera wall-avoidance pull-in sets `m_currentZoom` INSTANTLY (FreeChaseCamera.cpp:691/739), no smoothing → lurch | **CONFIRMED** (first-person: snap gone) | camera, view |

## OUTCOME: the cantina door-snap was TWO stacked bugs
1. **Avatar footprint-circle grazing floor-mesh seams** (face B, ~75%) → cross-axis rubberband.
   **FIXED** (spike 003): `FloorMesh::pathWalkCircleGetIds` — suppress circle contact while the center
   walks the tri cleanly (`PWR_WalkOk` ⇒ edge is walkable). Measured: rewinds 160→1, circle-grazes →0,
   no wall-clip in first-person ride test. Resolution was never broken (`:452` bail 0/160; slide
   preserves tangential exactly).
2. **Chase-camera instant-assignment snaps** (the residual, view-side) → camera lurch/jump.
   **DIAGNOSED + FIXED** (spikes 004-008). The CORNERSNAP-CAM jump probe (FreeChaseCamera.cpp, logs
   per-frame >8cm jumps) localized it to several INSTANT assignments, fixed in order of impact:
   - 005: zoom recovery was a fixed per-frame fraction (602) → time-scaled by elapsedTime (FPS bug; user's
     framerate hypothesis confirmed by CONSULT-40 4-AI). Mirrors CockpitCamera.cpp:815.
   - 006: hysteresis hold (~50ms) on the wall-collision clamp to bridge ray hit/miss flicker.
   - 007: **the big one** — the actual snap was the INSTANT pull-IN (probe: dZoom -2.1m in ONE frame,
     colliding=1). Rate-limited inward zoom velocity (cs_cameraPullInSpeed=8 m/s) → eases in. Crew had
     said keep pull-in instant (wall-clip), but user accepts brief camera-through-wall.
   - 008: shoulder offset (m_offset.x) was zeroed INSTANTLY by the shoulder-clip block (line 685) →
     0.396m sideways snap nearly every crossing. Eased the same way.
   **VERIFIED** (probe + user): zoom snap -2.1m→smooth ~0.3/frame glide; shoulder 0.396→0.156; user
   confirms "not snapping anymore". First-person was always clean (rays disabled).

## Spikes 005-008 (camera fix stack) — all VALIDATED
| 005 | camera-zoom-timescale | fix | VALIDATED | 006 | camera-hysteresis | fix | VALIDATED (partial; 007 was the real fix) |
| 007 | camera-pullin-ratelimit | fix | VALIDATED (the key fix) | 008 | camera-shoulder-ease | fix | VALIDATED |

## REMAINING (not blocking — the door-snap is solved end-to-end)
- These are SPIKE changes in real engine source (FloorMesh.cpp, CollisionResolve.cpp, FreeChaseCamera.cpp
  + .h). Productionize into a fix phase + REMOVE the CORNERSNAP-* `_DEBUG` probes when verified.
- Tuning knobs: cs_cameraPullInSpeed (pull-in ease speed), cs_wallCollisionHoldSeconds, shoulder residual.
- Systemic backlog (CONSULT-40 Codex): other non-dt-scaled per-frame constants — ShipTurretCamera.cpp:181,
  FreeCamera.cpp:278+, RemoteCreatureController.cpp:64/468, CreatureController.cpp:400. Player movement is
  already time-scaled (FPS-safe).

### 001 verdict (2026-06-20): portalId fix family DEAD (all contacts portalId=-1, neighbor=-1, FET_Uncrossable). Face B = avatar 0.5 circle grazing uncrossable boundary edges, center WalkOk, FWD branch (:2895), 105 coincident rewinds. **SURPRISE: pervasive, not doorway-local** (136/161 not near a portal) → #7b insufficient; min spans 0–0.8 → #7c insufficient alone. Survivors: **#7a** (suppress-on-WalkOk, high blast) or **resolution-side slide fix**. See spike README + CONSULT-37-doorsnap-SYNTHESIS.
