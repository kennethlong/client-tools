# CONSULT-38 — CODE REVIEW (call-graph / consequence trace): defensive hardening of async .mgn load

You are the repo tracer (call-graph). Review an UNCOMMITTED diff for DOWNSTREAM CONSEQUENCES. Find
problems — regressions, leaks, unbounded retries, permanently-broken state. Do not rubber-stamp.

## Context (facts, treat as given)
- `clientSkeletalAnimation` async skeletal-mesh (.mgn) load. A `SkeletalMeshGeneratorTemplate` (SMGT) may
  exist UNLOADED until `asynchronousLoadCallback()` runs `load()` on the main thread. Consumers
  (`SkeletalMeshGenerator`) can exist before load.
- Diff hardens a load-time crash:
  - **I4**: `asynchronousLoadCallback()` now clears `m_asynchronousLoadInProgress` on failure too (was
    success-only); leaves `m_uninitializedMeshGenerators` queued and returns.
  - **RANK-1**: `removeAsynchronouslyLoadedMeshGenerator()` NULL-safe early return.
  - **I3**: 7 `SkeletalMeshGenerator` deref methods early-return a default when `!isLoaded()`.

## The diff
Read `.planning/research/CONSULT-38-DIFF.txt`. Anchor files:
- `.../appearance/SkeletalMeshGeneratorTemplate.cpp` (asynchronousLoadCallback, createMeshGenerator
  ~1953, removeAsynchronouslyLoadedMeshGenerator ~3970)
- `.../appearance/SkeletalMeshGenerator.cpp` (the 7 guarded methods, dtor ~217)
- `.../appearance/SkeletalAppearance2.cpp` (rebuildMesh / areAllMeshGeneratorsReadyForDetailLevel /
  the consumers of the SMG methods) — find who calls the I3 methods and the rebuild trigger.

## Answer precisely (with file:line)
1. After I4 clears `m_asynchronousLoadInProgress=false` on a FAILED load, WHAT re-issues the async load?
   Trace `createMeshGenerator()` (~1953): does `(!isLoaded && !inProgress)` re-`add()` to AsynchronousLoader?
   Is the retry BOUNDED, or can a permanently-missing file cause repeated cold re-loads (disk hammer)?
2. On I4 failure the `m_uninitializedMeshGenerators` list is LEFT QUEUED. Trace its lifetime: who frees it
   if the load never succeeds (template destroyed?) — does it LEAK, or get resolved? Do the queued half-
   built SMGs (m_blendValues==NULL) ever get `create()`d, and by what?
3. I3 void guards skip `addShaderPrimitives`/`applySkeletonModifications` when unloaded. Trace the caller
   chain (CompositeMesh -> SkeletalAppearance2::rebuildMesh): is there a DIRTY/rebuild mechanism that
   re-invokes these once `isLoaded()` flips true? If not, the appearance stays incompletely built — flag it.
4. Do any of the 7 I3 methods get called from a path that ASSUMES a non-default return (e.g. iterates
   `getReferencedSkeletonTemplateCount()` then indexes, or relies on `createAppearance()!=NULL`)? List them.

Output: file:line call chains + the concrete consequence (leak / unbounded retry / permanent visual gap /
safe). Don't propose fixes — characterize.
