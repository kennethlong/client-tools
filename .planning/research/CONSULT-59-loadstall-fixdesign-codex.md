You are consulting on a fix design for measured load-time frame stalls in this repo
(D:\Code\swg-client-v2). First read the shared evidence file — its contents are measured
ground truth, treat as GIVEN:

  .planning/research/CONSULT-59-loadstall-fixdesign-EVIDENCE.md

YOUR ANGLE (repo tracing / call-graph — do not spend time on the other consultants' angles
of budget mechanics, localization, or lateral scans):

Stall class 1: `ClientProceduralTerrainAppearanceTemplate::PreloadManager` synchronously
fetches the whole planet's terrain shaders + flora appearances inside the GroundScene
constructor. We are considering deferring/spreading that work. Trace the call graph and
answer with file:line evidence:

1. DEPENDENTS: What code, if any, ASSUMES the preload completed by the time GroundScene's
   constructor returns? Specifically: when terrain chunks are later created/rendered
   (ClientProceduralTerrainAppearance and its chunk creation), do they fetch the same
   shader templates / flora appearances by name through the same caches (ShaderTemplateList /
   AppearanceTemplateList), such that a missing preload merely demand-loads (slower, safe),
   or is there any path that hard-requires the preloaded pointers (crash/FATAL if absent)?

2. WHO ELSE calls ClientProceduralTerrainAppearanceTemplate::preloadAssets() or constructs
   the template? (Space scenes? Editor/tools? SwgGodClient?) — i.e., the blast radius of
   changing its timing.

3. PUMP CONTEXT: During GroundScene::GroundScene / GroundScene::load, what is pumping the
   loading screen (CuiLoadingManager / LoadingScreen)? Is there an existing per-item
   "load one thing then tick the loading screen + audio" pump loop anywhere in the scene
   load path we could hook the preload list into (search for how the loading screen
   percentage advances during ground scene load)?

4. ASYNC SEAM: The engine has AsynchronousLoader (sharedFile) with add()/callback semantics
   used by MeshAppearanceTemplate. Does anything ALREADY use it (or TreeFile::cacheFile /
   addCachedFile) for bulk pre-caching lists of files? Is there an existing "preload list
   via async loader" precedent we should mirror?

Deliverable: concise findings per question with file:line, then a one-paragraph verdict:
is demand-load-on-miss safe for terrain shaders/flora if the preload becomes incremental,
YES/NO and why.
