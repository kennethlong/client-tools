You are consulting on moving a synchronous world-snapshot parse off the scene-constructor
frame in this repo (D:\Code\swg-client-v2). Read the shared evidence first — measured
ground truth, treat as GIVEN:

  .planning/research/CONSULT-60-worldsnapshot-parse-EVIDENCE.md

YOUR ANGLE (byte-level parser internals — other consultants own the consumer call-graph,
design ranking, and lateral behavior analysis):

Target files: src/engine/shared/library/sharedUtility/src/shared/WorldSnapshotReaderWriter.{h,cpp}
and src/engine/client/library/clientGame/src/shared/core/WorldSnapshot.cpp (the load
function :416-~700), plus sharedFile Iff as needed.

1. CHUNKABILITY: For design S1 (resumable main-thread parse), map exactly what state must
   persist across frames to suspend/resume the `load_0001` NODS loop (:850-869): the Iff
   object (can an Iff stay entered-in-a-form across frames safely? who else could touch
   it?), the reader's partial m_nodeList, and the post-parse networkIdNodeMap build
   (:559-584 — can that walk be folded into the per-node loop instead of a second pass?).
   Identify any Iff/TreeFile global state that makes a long-lived open Iff hazardous
   (file handle held? whole buffer in RAM? memory-mapped?). Note iff.open(filename, true)
   semantics with file:line.

2. HIDDEN GLOBALS in the parse path: statics/globals touched by Node::load / load_0000+
   (:395-530) and read_string allocations — anything that is not instance-private
   (debug flags, shared string pools, Crc tables)? For S2 (worker thread): list every
   symbol the .ws-parse half touches that is NOT owned by the reader instance.

3. PARTIAL-STATE READS: If loadIfClientCached/isClientCached answered from a PARTIALLY
   built m_networkIdNodeMap (S1, main-thread — no data race, just incomplete data),
   trace what a false "not cached" answer does downstream in ClientObject::setContainedBy
   (WorldSnapshot.cpp:1208 + ClientObject.cpp:~655-665) — wrong-but-recoverable, or a
   permanent wrong containment?

4. BUILDOUT HALF: WorldSnapshot.cpp:456-~700 — what per-row work adds nodes to the
   reader (which reader mutators), and is there natural chunk granularity (per-area?
   per-N-rows?) with state that must persist across frames? Any ordering dependency
   between .ws nodes and buildout nodes (objId collisions, container links)?

Deliverable: findings per question with file:line, then a minimal-diff resumable-parse
sketch (pseudocode) for S1 with every hazard flagged.
