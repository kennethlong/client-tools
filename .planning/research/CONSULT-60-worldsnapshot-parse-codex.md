You are consulting on moving a synchronous world-snapshot parse off the scene-constructor
frame in this repo (D:\Code\swg-client-v2). Read the shared evidence first — measured
ground truth, treat as GIVEN:

  .planning/research/CONSULT-60-worldsnapshot-parse-EVIDENCE.md

YOUR ANGLE (repo tracing / call-graph — other consultants own the parser internals,
design ranking, and lateral consumer-behavior analysis):

1. CONSUMER WINDOW: Enumerate EVERY caller of WorldSnapshot's static API
   (load/unload/update/preloadSomeAssets/donePreloading/getLoadingPercent/isClientCached/
   loadIfClientCached/removeFromWorld/getLoadedList or similar) with file:line, and for
   each: can it execute in the window between GroundScene::postload returning and
   GroundScene::isFinishedLoading() going true? (Trace what drives it: network message,
   UI, per-frame update, teardown.) The dangerous ones are those that read the reader's
   node data mid-parse.

2. BASELINES DURING LOADING: Precisely which object baselines can arrive BEFORE
   finishedLoading/setSceneChannel (player, containers, initial creature set?), and can
   any of them reach ClientObject::setContainedBy -> WorldSnapshot::loadIfClientCached
   in that window? (Trace receiveCmdStartScene onward — what does the server send before
   the scene channel is set?)

3. THREAD-SAFETY DEPENDENCIES (for the S2/S3 worker options): Is
   ObjectTemplateList::lookUp(crc) safe to call from a non-main thread (locking in
   ObjectTemplateList)? Is DataTable::load / DataTableManager safe off-main? Is
   SharedBuildoutAreaManager::load called anywhere else / does it mutate shared state
   read by the main thread during loading?

4. TEARDOWN: Trace GroundScene teardown ordering (quit during loading /
   startScene->startScene): where do WorldSnapshot::unload/removeFromWorld get called
   relative to the loading pump stopping? Any path that could re-enter WorldSnapshot::load
   while a previous load is mid-flight?

Deliverable: findings per question with file:line, and a verdict line: which consumers
MUST be gated/queued for a deferred parse, and whether the buildout half has any
off-main-thread blocker.
