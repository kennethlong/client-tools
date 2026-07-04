# CONSULT-60 — WorldSnapshot::load off the GroundScene-ctor frame — SHARED EVIDENCE (treat as GIVEN)

Follow-up to CONSULT-59 (see CONSULT-59-loadstall-fixdesign-SYNTHESIS.md). After the
terrain-preload fix, the stall watchdog convicted the remaining zone-in freeze:

- **3146 ms single frame** (2026-07-04 verify boot, both minidump time-samples):
  `GameNetwork::receiveCmdStartScene -> Game::_setScene -> GroundScene ctor ->
  GroundScene::init -> GroundScene::postload (GroundScene.cpp:762) -> WorldSnapshot::load
  (WorldSnapshot.cpp:416) -> WorldSnapshotReaderWriter::load (sharedUtility/
  WorldSnapshotReaderWriter.cpp:533) -> load_0001 (:850, per-node `new Node; node->load(iff)`
  loop over snapshot/<scene>.ws)`.
- This runs BEFORE the loading screen is enabled (GroundScene.cpp:815), so it is felt as
  the character-select screen freezing, and it starves the audio pump (music glitch).
- The .ws parse is the sampled cost; `WorldSnapshot::load` ALSO loads buildout datatables
  after it (WorldSnapshot.cpp:456-~700, DataTable per area + per-row node adds via the
  reader) — cost share of buildout vs .ws unmeasured (both samples were in .ws parse).

Structure facts (verified by direct read):
- `WorldSnapshotReaderWriter::load(sceneName)` (:533): `Iff iff; iff.open(file, true)`
  (whole file into memory), `clear()`, `load(iff)` → `load_0001` loop building a private
  Node tree (`m_nodeList`), then a node-stack walk building `m_networkIdNodeMap`.
- `Node::load` reads plain data (tags/ints/floats/strings) — NO engine object creation,
  NO template fetches during parse. The only external call in the .ws path is
  `Crc::calculate` (pure). The buildout path additionally calls DataTable::load and
  `ObjectTemplateList::lookUp(crc)` (WorldSnapshot.cpp:557) per row.
- Consumers of the parsed data (all static, WorldSnapshot.cpp): `preloadSomeAssets` (:706,
  pumped per-frame from GroundScene::updateLoading), `getLoadingPercent` (:750),
  `donePreloading` (:760, gates GroundScene::isFinishedLoading), `update` (creates
  objects near player; first call at GroundScene.cpp:2065 gated on finishedLoading),
  `isClientCached` (:1072), `loadIfClientCached` (:1208, called from
  ClientObject::setContainedBy — baselines can arrive for the player + initial objects
  DURING the loading window, before finishedLoading).
- Scene channel (server object streaming) is only set AFTER finishedLoading
  (GroundScene.cpp:2063-2067) — so the bulk of baselines arrives post-loading, but not all.
- CONSULT-59 precedent shape now in-tree: capture-cheap + budgeted per-frame step pumped
  from GroundScene::updateLoading, completion gating donePreloading-style flags.

Candidate designs to evaluate:
- **S1 chunked main-thread parse**: `WorldSnapshot::load` opens the Iff + enters NODS,
  then a `loadStep(budgetMs)` (pumped from updateLoading alongside the other preloads)
  parses N nodes per frame; buildout tables likewise chunked per-area (or per-N-rows);
  `donePreloading`/`getLoadingPercent` return not-done/0 until parse complete;
  `loadIfClientCached`/`isClientCached` during the window either answer from the
  partial map or queue-and-flush at completion. NO new threads.
- **S2 worker-thread parse**: postload kicks a thread that runs today's whole load body;
  consumers gate on an atomic parse-complete flag; loadIfClientCached queues; teardown
  joins. Requires: TreeFile open on worker (already exercised by AsynchronousLoader +
  recent TreeFile locks), allocator thread-safety (fine), and ObjectTemplateList::lookUp
  thread-safety (UNKNOWN — must be verified) for the buildout half.
- **S3 hybrid**: worker thread for the pure .ws parse only (isolated data, no external
  calls); buildout tables stay on main but chunked.

Constraints: boot gate (charselect must still work — note space scenes and snapshot-less
scenes return early), minimal diff, no shared-plugin-header ABI changes, Win32+x64.
GroundScene teardown mid-load (quit during loading, startScene->startScene) must stay
leak/crash-free. The Iff/TreeFile buffer for a .ws can be tens of MB — lifetime matters.
