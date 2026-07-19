# CONSULT-69 evidence pack — interior-layout (.ilf) objects in a live world editor

Treat everything here as GIVEN (measured/source-verified in this repo). Do not re-derive.

## Context (neutral)

This SWG NGE client (from-source, MSBuild, `src/engine`, `src/game`) advertises an
engine-hookpoint contract (v21, 143 names) consumed by an injected in-process world-editor
overlay. The editor can already: enumerate/mutate/save the WorldSnapshot (.ws) layer
(id-keyed rows over `WorldSnapshotReaderWriter`), pick via an advertised screen-ray
(`utinni_collideScreenRay`: returns nearest-hit NetworkId + world point; walks up
`Object::getParent()` to the nearest networked ancestor; id 0 for id-less hits), read camera
matrices, and toggle `CuiPreferences::allowTargetAnything`.

A live world is THREE content layers:
1. **Snapshot (.ws)**: client-spawned statics. `WorldSnapshot::createObject`
   (WorldSnapshot.cpp:240-310) — `setClientCached()` (:272), `setNetworkId(authored node id)`
   (:274), registered in `NetworkIdManager`; spawn SKIPPED if any live object already holds
   the id (:244 `CEC_objectAlreadyExists`).
2. **Server-streamed**: baselines from a live server (hybrid sessions exist: server login +
   client-side editor scene; the server keeps streaming/messaging).
3. **Interior-layout (.ilf)**: `ClientInteriorLayoutManager` (clientGame,
   ClientInteriorLayoutManager.cpp:100-236) spawns per-cell decoration when a POB cell
   becomes visible: `ObjectTemplateList::createObject(templateName)` →
   `tangibleObject->addClientOnlyInteriorLayoutObject(obj)` → `setParentCell(cell)` →
   `RenderWorld::addObjectNotifications` → `endBaselines()` → `addToWorld()`.
   **No NetworkId is ever assigned** (stays 0/invalid; NOT in NetworkIdManager). Objects are
   destroyed on cell unload; creation is budget-throttled per frame with a resume cursor.
   The SAME .ilf populates EVERY instance of that building template, every session.

## Engine facts (verified, file:line)

- Targeting is id-keyed: `CreatureObject::m_lookAtTarget` is a `CachedNetworkId`;
  `setLookAtTarget(NetworkId)`. An id-0 object cannot be referenced by it.
- The hud pick itself is Object*-keyed BEFORE id-keying: `SwgCuiHud.cpp:1436`
  `m_lastSelectedObject = foundObject` (a `Watcher<Object>`), fed by
  `findAllTargettableObjects` → `testFindObject` (SwgCuiHud.cpp:156-226) which under
  `allowTargetAnything==true` filters only appearance-less objects. `getRootParent()`
  (Object.cpp:2105) walks only while `m_childObject` — cell-contained objects attach with
  `asChildObject=false` (`Object::setParentCell` → `attachToObject_w(&cellOwner,false)`,
  Object.cpp:1387/1405), so a picked chair resolves to itself.
- The pick ray reaches in-cell contents: `ClientWorld::internalCollideObject` child-walk
  (ClientWorld.cpp:1330) traverses `!isChildObject()` children unconditionally.
- `m_lastSelectedObject` is already advertised to the consumer (`cuiHud::getTarget` row =
  `SwgCuiHud::getLastSelectedObject`).
- `NetworkId` is int64; `isValid()` is only `m_value != 0` (NetworkId.h:116). Servers and
  snapshots mint positive ids only (snapshot on-disk ids are positive int32). The editor's
  ws id-allocator excludes out-of-band ids from seeding.
- `InteriorLayoutReaderWriter` (sharedUtility) has a COMPLETE writer:
  `save(fileName)`, `clear()`, `addObject(cellName, templateName, transform_o2p)`.
- A loose-file searchPath override dir exists and wins over TREs; the ws editor already
  saves its .ws there and invalidates the negative-lookup cache for written names.
- In hybrid sessions the client uplinks the lookAt-target id to the server via controller
  messages (unverified how a given emu server handles unknown ids).

## The product question

The world-editor wants interior decoration objects (the .ilf layer) to be (1) selectable
with the mouse, (2) manipulatable with the in-game gizmo, and ideally (3) persistable.
Today they are none of these through the id-keyed paths, because they have no ids.

## Constraints

- Contract boundary is primitives/pointers only (ABI RULE); consumer is in-process, can
  call advertised rows on the game thread per-frame.
- The provider strongly prefers fail-closed, minimal-diff, kill-switchable changes.
- Boot gate: client must remain bootable; retail behavior must not regress for
  non-editor users (allowTargetAnything defaults false).
