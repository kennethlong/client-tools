# REQUEST (← SWG-Toolkit live-editor): advertise `object::getContainingBuildingId`

**Date:** 2026-07-30 · **From:** SWG-Toolkit live-editor session · **To:** swg-client-v2 (advertised catalog owner).
**Priority:** the last blocker for model-D interior-decoration persistence going live. One shim, grounded chain below.

## Why (live smoke surfaced this)

The full model-D round trip is wired and working end to end — capture → toolkit assembles the edited `.ilf`
+ derived building template → `wsSetNodeTemplateName` + `wsSaveSnapshot` → result back. The ONLY failing step
is getting the **building's `.ws` node id + building template** from in-game:

- `collideScreenRay` on the decoration returns **id 0** by design (v22: the getParent walk is
  `m_childObject`-gated, so an id-less `.ilf` decoration never dissolves into the building id — correct for
  picking the decoration itself).
- Falling back to a wall/floor click resolves to the **CELL** object, not the building —
  template `object/cell/shared_cell.iff`, which has **no `interiorLayoutFileName`**, so
  `deriveBuildingTemplate` fail-closes. (Confirmed live: captured bldg id 1082878 → `getObjectTemplateName`
  = `object/cell/shared_cell.iff`.)
- `object::getParent` is **not advertised**, so the consumer can't walk cell → building.

The building (the POB) is one level above the cell and carries the `interiorLayoutFileName` param model-D
re-points. The consumer needs its NetworkId (== the `.ws` node id `wsSetNodeTemplateName` takes).

## What's needed — one copy-out shim

```c
// Returns the NetworkId of the POB building that contains `object` (any cell-contained object works:
// an .ilf decoration, a cell, the player). 0 if not inside a POB / no networked building / null arg.
// Chain is all inline / reference-returning → un-advertisable directly (ABI RULE), same as the
// getTransformO2P / camera copy-outs:
//   object->getParentCell()      CellProperty*    [Object.h:166 — already advertised as object::getParentCell]
//     ->getPortalProperty()      PortalProperty*  [CellProperty.h:119 / inline :270]
//     ->getOwner()               Object&  (= the building) [Property.h:34 / inline :57]
//     .getNetworkId().getValue() __int64
// Null-guard every hop (a decoration in the world cell has no PortalProperty → return 0).
extern "C" __int64 __cdecl utinni_getContainingBuildingId(void* object);
```

CALLED, game-thread-only, per-frame-safe (pointer hops + a value read, no alloc). The `Object*` is the
BORROWED consumer pick pointer (same lifetime discipline as `collideScreenRayObject` / `getTransformO2P`).

## Consumer use (removes the wall-click step)

At Arm, call `getContainingBuildingId(rayObject)` → building id → `getObjectById(buildingId)` →
`getObjectTemplateName` = the building template (which HAS `interiorLayoutFileName`). Feeds the capture's
`buildingInstanceId` + `buildingTemplateVfsPath`. Bound by name in `rva_table.cpp` (1 line). This also lets
the user just hover the decoration and Arm — no separate building selection needed.

## Status

Everything else is done and proven live this session: contract + C++ channel mirror + agent capture/rebind +
host writeRebind + renderer orchestrator (override-dir resolution, `.ilf` assemble, derived template, staging)
— all on the current v24 catalog. This is the single accessor that unblocks the in-game smoke.

_(Toolkit-side mirror: `SWG-Toolkit/.planning/handoff/2026-07-30-CHANGE-REQUEST-getContainingBuildingId.md`.)_
