# CONSULT-37 — async skeletal-mesh (.mgn) zone-in crash: CACHE/REFCOUNT race, line-level read

You are the most detailed code reader. C++ game client (SWG, MSBuild, x64). Treat the following as GIVEN
facts; do not re-derive or assume a cause. Report only what the code shows, with exact file:line.

## Evidence (facts, as given)
- Symptom: intermittent `c0000005` (access violation) during ground-zone load-in (Tatooine / Mos Eisley).
- PRE-EXISTING: also on 32-bit Release AND Debug, rarely. NOT x64- or D3D11-specific.
- New observation: tends to happen on the **FIRST run after a rebuild** (cold caches), usually first run.
- Crash breadcrumb (a per-category "last asset touched" log, NOT a proven faulting frame) showed in-flight:
  `SkeletalMeshGeneratorTemplateAsync: appearance/mesh/armor_marauder_s01_belt_m_l3.mgn`. Treat as a HINT.
- No captured stack yet.

## Your task (line-level cache + refcount race audit)
Read these and find any data race / use-after-free / observe-half-constructed hazard in the mesh-generator
template CACHE and its asynchronous loader binding:
- `src/engine/client/library/clientSkeletalAnimation/src/shared/appearance/MeshGeneratorTemplateList.cpp`
  — the template cache map, `fetch(const CrcString&, bool create)` (~377), the create=false path
  (`asynchronousLoaderFetchNoCreate` ~362 → `fetch(cfn,false)`), `asynchronousLoaderRelease` (~370),
  `assignAsynchronousLoaderFunctions` (~295), and `meshGeneratorTemplate->fetch()`/release refcounting (~288).
- `src/engine/shared/library/sharedFile/src/shared/AsynchronousLoader.cpp` — the queue, worker thread, and
  the fetch/release callback invocation. Which thread calls the bound fetch/release? Is the cache map
  accessed from >1 thread without a lock?
- `src/engine/client/library/clientSkeletalAnimation/src/shared/appearance/SkeletalMeshGeneratorTemplate.cpp`
  — what happens between allocation and full initialization of the template; is it inserted into the cache
  before it is fully built (so another thread/caller can fetch a half-built entry)?

Specifically:
1. Is the cache map mutated (insert/erase) on one thread while read (fetch) on another, unsynchronized?
2. Can `fetch(create=false)` return a pointer to an entry that is present-but-not-yet-fully-loaded?
3. Can a release (refcount→0, erase+delete) happen while another reference is live (esp. async release vs.
   synchronous consumer)?
4. Anything that is order/timing-sensitive such that a SLOW (cold-cache) async load changes who-wins-the-race.

Output: exact file:line for each hazard, the variable/field involved, and the concrete interleaving that
produces a bad deref. If a path is safe, say so explicitly. Do NOT propose a fix — characterize precisely.
