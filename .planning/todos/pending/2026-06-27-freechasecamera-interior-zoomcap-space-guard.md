---
created: 2026-06-27
title: FreeChaseCamera interior zoom-cap mis-fires in walkable ship interiors (space)
area: client / camera (FreeChaseCamera)
priority: low
status: backlog
files:
  - src/engine/client/library/clientGame/src/shared/camera/FreeChaseCamera.cpp  (alter, ~line 610)
references:
  - commit 810b6c9a9  (fix(camera): interior chase-cam zoom cap + collision pull-in convergence)
  - memory: project_backroom_camera_interior_zoom_cap
  - memory: project_jtl_space_world_empty_render  (found while ruling camera out of the empty-world bug)
---

## What

The interior chase-cam zoom cap added in `810b6c9a9` (`freeChaseCameraInteriorMaximumZoom`, default
3.0 m) is gated only on "target's parent cell is not the world cell":

```cpp
if (ms_interiorMaximumZoom > 0.f)
{
    CellProperty const * const cameraTargetCell = m_target ? m_target->getParentCell () : 0;
    if (cameraTargetCell && cameraTargetCell != CellProperty::getWorldCellProperty ())
        m_zoom = std::min (m_zoom, ms_interiorMaximumZoom);
}
```

A **walkable ship interior in space** (multi-crew POB) is a cell that is not the world cell, so this
cap fires there too — clamping the on-foot chase camera to 3 m inside the ship. The cap was designed
for *building* interiors (cantina back-room portal see-through + prop occlusion); it was never meant
for ship interiors.

## Why it's low priority / not urgent

- Discovered while ruling the camera OUT of the empty-space-world bug — **this is NOT that bug.**
  Space flight uses `CockpitCamera` (`ShipStation_Pilot` → `CI_cockpit`, `GroundScene.cpp:2209`), so
  `FreeChaseCamera` (and this cap) is inert while piloting.
- Only bites when you're *walking* inside a multi-crew ship in space. A single-seat fighter (Scyk,
  flown from the cockpit) never hits it.

## Fix sketch

Skip the cap when the scene/zone is a space zone (or, more precisely, when the containing cell belongs
to a ship rather than a building). Options to evaluate:
- Gate on the active scene being a space zone (cheapest; `GroundScene` already knows).
- Or test whether `cameraTargetCell`'s owning object is a ship (POB-on-a-ship) vs a static building.

Validate: on-foot camera framing inside a multi-crew ship interior in space matches a building interior
intent only where intended; building interiors unaffected; cockpit flight unaffected.
