You are consulting on a fix design for measured load-time frame stalls in this repo
(D:\Code\swg-client-v2). First read the shared evidence file — its contents are measured
ground truth, treat as GIVEN:

  .planning/research/CONSULT-59-loadstall-fixdesign-EVIDENCE.md

YOUR ANGLE (precise code reading of AsynchronousLoader — do not spend time on the other
consultants' angles of terrain preload design, localization, or lateral scans):

Stall class 2: `AsynchronousLoader::processCallbacks()`
(src/engine/shared/library/sharedFile/src/shared/AsynchronousLoader.cpp, line ~571) drains
ALL completed requests per frame; each callback does full IFF parse + GPU resource creation
on the main thread. A count budget exists (`asynchronousLoaderCallbacksPerFrame`, config
default 0 = unlimited, consumed at ~line 686). We want to bound the per-frame cost.

Byte-level trace tasks (file:line for everything):

1. REQUEST/CACHE LIFECYCLE: Map the full lifecycle of a Request and its CachedFiles across
   threads: who allocates, when `alreadyCached` flips, what `TreeFile::addCachedFile` /
   `clearCachedFiles` do, and what state persists if we break out of the drain loop early
   (count budget path). Confirm early-break leaves the remaining completed requests fully
   valid for next frame (no leak, no stale TreeFile cache, no alreadyCached flag wedge).

2. ORDERING HAZARDS: Are there dependent-load assumptions between queued callbacks (e.g., a
   callback that expects an earlier callback's side effects same-frame)? Look at the actual
   callback implementations registered via AsynchronousLoader (MeshAppearanceTemplate::
   asynchronousLoadCallback and any others — grep the registration sites). Can a callback
   enqueue new async requests, and does that interact with the drain loop safely?

3. TIME BUDGET FEASIBILITY: If we changed the drain loop to ALSO break on elapsed wall time
   (e.g., Clock/PerformanceTimer check after each callback, budget in ms from a new
   ConfigSharedFile key), identify exactly where the check belongs and any subtlety (e.g.,
   the postponed-requests block at line ~691 — what does it do and must it still run when we
   break early? What about the `bytes` accounting and ms_numberOfCachedBytes?).

4. STARVATION: With a budget in place, confirm requests cannot starve forever (FIFO order
   preserved across frames?) and that shutdown (remove/disable paths) drains regardless.

Deliverable: findings per question with file:line, then a concrete minimal-diff proposal
for the time-budgeted drain (pseudocode ok), flagging any hazard you found.
