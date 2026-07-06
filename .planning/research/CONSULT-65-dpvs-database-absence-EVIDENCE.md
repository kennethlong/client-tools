# CONSULT-65 — evidence pack: dPVS portal database absence (CONSULT-64 continuation)

All items MEASURED or source ground-truth. Treat as given; do not re-derive.
Prior arc: CONSULT-64-SYNTHESIS.md (RenderDoc conviction → probe rounds 1-7).

## The defect, fully narrowed (locked, instrumented dPVS build)

Intermittent whole-cell see-through (cantina/foyer, cross-renderer, cross-bitness).
The instrumented library (per-frame counters on every portal rejection path) proved:

- On EVERY collapse frame (portals crossed N→0 at a near-static camera):
  **tested:0 — the camera cell's portal objects never entered
  isObjectVisible_INTERNAL at all.** Not backface-culled (bf), not transition/
  rectangle/frustum-rejected (ct/tt/rect/fr all 0), not visibility-parent-skipped
  (ph:0; the engine never calls setVisibilityParent).
- Healthy frames at the same pose: tested:1-3, portals crossed 1-8.
- The flicker alternates healthy/collapsed every ~1-3 seconds at a BIT-STILL camera
  (camDelta 0.00-0.05m, fwdDot 1.00000), stable root cell, NO door/closed-state
  edges (Portal::setClosed hook instrumented — silent during flicker windows).
- ⇒ **The per-cell spatial database traversal fails to enumerate the cell's portal
  objects on the collapse frames.** The portals are enabled, their dPVS objects
  exist, and the same query succeeds seconds before and after.

## Source anchors already traced (vendored dPVS, src/external/3rd/library/dpvs/)

- `Database::updateObject` (dpvsDatabase.cpp:1523): on update, if
  `areObjectBoundsOK(ob)` is false and the object HAS an instance →
  **removeObject(ob)** (object leaves the database); if it has NO instance and
  bounds are bad → stays out. areObjectBoundsOK (:1434) = AABB::isOK
  (min<=max, dpvsAABB.hpp:99-102 — a FLAT portal quad passes) + isFinite.
- `Database::removeObject` (:1246) sets `ob->setTestState(OC_DONE)` +
  `ob->setTimeStamp(m_timeStamp)` — "it will be skipped during object processing
  no matter what". **Re-add path** `Database::addObject` (:1457) sets
  status HIDDEN/timestamps fresh.
- Dirty-list machinery (dpvsImpObject.cpp): setBoundsDirty (:1424) relinks the
  object to its CELL's dirty list HEAD with `m_dirtyTimeStamp = queryTime` ("check
  again during next query", :402); linkToDirtyListTail sets
  `m_dirtyTimeStamp = queryTime + g_timeUntilStatic` with **g_timeUntilStatic =
  1.0f seconds** (:116, :436) — objects cycle a periodic ~1s re-check even when
  static. THE FLICKER CADENCE MATCHES.
- `ImpObject::setObjectToCell` (:1320-1378) early-outs on identical matrices
  (memEqual :1370) — engine re-setting an unchanged transform does NOT dirty.
- Per-cell Database node tree self-throttles maintenance: 2.5% frame time,
  1/64 nodes per traverse (dpvsDatabase.cpp:3580/:922 area); node dirty/split/
  collapse runs during queries (updateDirtyNode :2299; traverseNode checks
  v->isDirty() :3076).
- Round-7 counters (just landed, awaiting a run): Database::updateObject /
  removeObject / addObject per-frame counts for PORTAL objects (dbUpd/dbRem/dbAdd
  on the [PortalCullProbe] line).

## Engine-side context (locked)

- Portals attached to the building object (Portal.cpp:385-395 →
  Object::addDpvsObject); RenderWorld_OcclusionNotification::transformChanged
  re-sets o2c matrices for ALL an object's dpvs objects whenever the OBJECT's
  transform-changed notification fires (RenderWorld_OcclusionNotification.cpp:140).
  Buildings are static, but notification frequency is UNMEASURED (identical-matrix
  sets are absorbed by dPVS's memEqual).
- Doors: DoorObject::alter drives Portal::setClosed(!open) → dPVS Object
  ENABLED=false (RenderWorld.cpp closed hook). During flicker windows there were
  NO closed-state edges. (Separate contributing issue: doorways close portals
  with no visible door mesh; one door's wake chain starved a whole session —
  parked as its own fix.)
- Camera flow: RenderWorld::drawScene sets dpvsCamera cell+matrix per frame,
  resolveVisibility(commander, depth 8, 0.0f). Occlusion culling OFF.

## The question

By what exact mechanism does a portal object become non-enumerable for EXACTLY
one query (or a few), repeatedly, on a ~1s cadence, with no engine-side state
changes — and what is the minimal, safe fix? Candidate space (NOT verified):
timestamp/teststate aliasing after removeObject/addObject cycles (OC_DONE +
m_timeStamp match skipping a live object); dirty-object lazy bounds recompute
window (object unlinked/relinked mid-query); node split/collapse transiently
losing instances; getCellSpaceAABB recompute producing transiently-bad bounds
(updateObject → removeObject); the 1:16 random assumeVisible demotion
(dpvsVisibilityQuery_Test.cpp:92 — occlusion off should bypass, verify).
