# Cantina door-snap fix — tunable values reference

Reference for the tunable constants introduced (and the pre-existing ones they interact with) by the
2026-06-20 cantina door-snap fix. The fix resolved **two stacked bugs**: an avatar floor-collision
rubberband (Change A) and chase-camera instant snaps (Changes B/C). See
`.planning/research/CONSULT-41-REVIEW-SYNTHESIS.md` for the full review.

> **IMPORTANT: these are compile-time C++ constants, NOT `.cfg` settings.** Changing any of them
> requires rebuilding the owning library + relinking `SwgClient` (they are not read from `client.cfg`).
> If runtime tuning is ever wanted, they could be wired to a config key the way `ms_cameraZoomSpeed`
> already is (see "Pre-existing values" below) — that's a future enhancement, not done here.

---

## NEW tunables introduced by the fix

### 1. `cs_seamGrazeEpsilon` — avatar floor-seam suppression depth (Change A)
| | |
|---|---|
| **Value** | `0.05f` (5 cm) |
| **Location** | `src/engine/shared/library/sharedCollision/src/shared/core/FloorMesh.cpp:73` (file-scope global) |
| **Owning lib** | `sharedCollision` |
| **Affects** | ALL footprinted movers (player, NPCs, creatures, pets, mounts) in interiors/POBs |

**What it is:** the penetration-depth threshold below which a footprint-circle contact against an
**uncrossable floor-mesh boundary edge** is suppressed — but only when the circle CENTER walked the
triangle cleanly (`PWR_WalkOk`, i.e. the floor is crossable-connected walkable end-to-end, so the
grazed edge is a lateral seam, not a clearance gate athwart the path).

**Why it exists:** the cantina door rubberband was the footprint circle grazing a flat floor seam and
generating a spurious blocking contact. Measured: 1041 such grazes all came in at **~0.000 m**
penetration — cleanly separated from any real blocking contact. Suppressing only *shallow* grazes
kills the seam contact while a real lateral wall or a narrow gap/pillar (deeper penetration) is kept
**blocked** (this is what makes the change safe engine-wide — it was a code-review NO-SHIP concern).

**How to tune:**
- **Lower** (e.g. 0.02) → stricter: suppresses fewer grazes, preserves more wall-blocking. Safe floor
  for the door is anything above the measured seam depth (~0 m), so it has huge margin.
- **Higher** (e.g. 0.15) → looser: suppresses deeper grazes too. Risk: starts dropping legitimate
  wall-slide / narrow-feature blocking. **Do not raise without re-measuring** that real walls/gaps
  still exceed it.
- Penetration is computed as `startLoc.getRadius() - contactNormal.magnitude()` (footprint radius
  minus the circle-center→edge distance).

---

### 2. `cs_cameraPullInSpeed` — chase-camera inward ease rate (Changes B + C)
| | |
|---|---|
| **Value** | `8.0f` (meters/second) |
| **Location** | `src/engine/client/library/clientGame/src/shared/camera/FreeChaseCamera.cpp:107` (anon-namespace static) |
| **Owning lib** | `clientGame` |
| **Affects** | the third-person chase camera only (view-only; no gameplay/collision effect) |

**What it is:** the maximum speed at which the chase camera may move **inward** in one frame, used by
**two** places:
- **Change B (zoom pull-in):** caps how fast `m_currentZoom` may decrease when the wall-avoidance ray
  hits — so the camera *eases* into a wall instead of the measured ~2 m single-frame snap.
- **Change C (shoulder ease):** the same rate eases the shoulder offset `m_offset.x` toward 0 when the
  side-ray hits a wall, instead of the old instant ~0.4 m sideways jump.

**Trade-off it controls:** higher = faster pull-in = less near-plane "peek through the wall" during
the ease, but snappier (closer to the old behavior). Lower = slower/smoother, but more/longer wall
peek. `8 m/s` is the first-pass value; the chase camera is intentionally allowed to clip/peek through
walls briefly when above/behind the avatar (user-accepted), so a moderate speed is fine.

**How to tune:**
- **Raise** (e.g. 12–16) if the camera feels too floaty entering walls / peeks through too long.
- **Lower** (e.g. 4–6) if the inward ease still feels too snappy.
- Pop-**out** (recovery toward full zoom) is NOT governed by this — it rides `ms_cameraZoomSpeed`.

---

## Pre-existing values these interact with (context — NOT changed by the fix)

| Name | Value / source | Role |
|---|---|---|
| `ms_cameraZoomSpeed` | config `[ClientGame] freeChaseCameraZoomSpeed`, **default 0.2** (`ConfigClientGame.cpp:1122`); also the LocalMachineOptions key `cameraZoomSpeed` | Per-frame fraction for the camera zoom **recovery/pop-out** lerp (`FreeChaseCamera.cpp` recovery line). The Change-B rate limit is anchored *after* this lerp so manual zoom-in still rides it at full speed. (This is the one constant that's already config-driven.) |
| `wallEpsilon` | `0.01f` = 1 cm (`FloorMesh.cpp:66`) | Pre-existing wall-contact tolerance in the floor walk. Sibling concept to `cs_seamGrazeEpsilon` but a different test; left unchanged. |
| Avatar footprint radius | `0.5` m (circle radius in the floor walk) | Drives both the seam-graze geometry (Change A) and the camera collision rays. Penetration in Change A is measured against this radius. |
| `m_firstPersonDistance` | `0.5f` (`FreeChaseCamera.cpp:141`, from `camera/freechasecamera.iff`) | Below this zoom the camera goes first-person and the wall-avoidance rays (hence Changes B/C) are skipped entirely. |
| `frameRateLimit` | `[SharedFoundation] frameRateLimit`, **default 0 = uncapped** (`SetupSharedFoundation.cpp`) | FPS cap. **Investigated but NOT set** — both Restoration and SWGEmu run uncapped, so capping is not how the reference clients avoid the snap. Documented for completeness. |

---

## Quick "where to change what" cheat sheet
- Door rubberband returns / over-suppressing wall contacts → **`cs_seamGrazeEpsilon`** (FloorMesh.cpp:73), rebuild `sharedCollision` + `SwgClient`.
- Camera enters walls too snappy / too floaty → **`cs_cameraPullInSpeed`** (FreeChaseCamera.cpp:107), rebuild `clientGame` + `SwgClient`.
- Camera zoom-in/out (manual scroll) speed → **`freeChaseCameraZoomSpeed`** cfg (no rebuild — it's a real config key).
