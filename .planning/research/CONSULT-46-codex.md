Repo tracer / lifecycle analyst. Codebase D:\Code\swg-client-v2 (modern MSVC v145/C++20 rebuild of
2003 SWG client). READ-ONLY. Cite file:line.

FIRST read .planning/research/CONSULT-46-EVIDENCE-layoutthrottle.md (facts are GIVEN).

YOUR ANGLE = Q1 RISK (teardown/lifecycle). Others cover burst-size quantification, do/don't ROI, and
the throttle design -- do NOT duplicate. Trace, concretely:

1. When a building/POB tangible object (the interior-layout owner) or one of its cells UNLOADS --
   player walks out, LOD eviction, distance cull, scene teardown -- what is the destruction path? What
   happens to its client-only interior-layout objects (created via
   TangibleObject::addClientOnlyInteriorLayoutObject)? Where are they freed? Cite the dtor / removal.
2. Does `CellProperty::getAppliedInteriorLayout()/setAppliedInteriorLayout()` reset when a cell
   unloads/reloads? Can a CellProperty pointer be destroyed and its address reused by a new cell?
3. If we throttle interior-layout creation across frames (holding a `CellProperty*` /
   `TangibleObject*` resume pointer or a pending-create queue keyed on those), what invalidation hooks
   exist to drop stale pending entries when the cell/building goes away? Is there an existing
   notification (object destroyed / cell removed / portal property removed) we could subscribe to, or
   would dangling pointers be unguarded?
4. How does the player ENTER/EXIT a cell trigger visibility changes (RenderWorld::getVisibleCells) --
   how fast can a cell appear then disappear (fast in-out)? Is mid-creation teardown realistic?

Deliver: the teardown call-graph (file:line), a clear verdict on whether throttled pending-creates can
dangle and what (if anything) safely invalidates them, and the concrete failure mode if unguarded.
