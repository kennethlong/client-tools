# CONSULT-37 — async skeletal-mesh (.mgn) zone-in crash: CALL-GRAPH / LIFECYCLE trace

You are tracing a C++ game client (SWG, MSBuild, x64). Treat the following as GIVEN facts (do not
re-derive, do not assume a cause). Report only what the code shows.

## Evidence (facts, as given)
- Symptom: intermittent `c0000005` (access violation) during ground-zone load-in (Tatooine / Mos Eisley).
- It is PRE-EXISTING: occurs on 32-bit Release AND Debug builds too, just rarely. NOT specific to x64 or
  the D3D11 renderer.
- New observation: it tends to happen on the **FIRST run after a rebuild** (i.e. cold caches), and "usually
  on the first run."
- The engine's crash breadcrumb (a per-category "last asset touched" log, NOT a proven faulting frame)
  showed in-flight at crash time: `SkeletalMeshGeneratorTemplateAsync: appearance/mesh/armor_marauder_s01_belt_m_l3.mgn`.
  Treat the specific .mgn as a HINT, not proof — the breadcrumb may mis-attribute.
- No captured call stack yet.

## Your task (call-graph / lifecycle ONLY — you are the repo tracer)
Trace the asynchronous `.mgn` (mesh generator) load path and the SkeletalMeshGenerator/appearance
construction, and map WHERE a pointer produced by the async path is consumed. Anchor files:
- `src/engine/client/library/clientSkeletalAnimation/src/shared/appearance/MeshGeneratorTemplateList.cpp`
  (esp. `fetch(filename, bool create)` ~line 377, `asynchronousLoaderFetchNoCreate` ~362,
  `assignAsynchronousLoaderFunctions` ~295, the cache map + refcount)
- `src/engine/shared/library/sharedFile/src/shared/AsynchronousLoader.cpp` (the async fetch/release/queue/threading)
- `src/engine/client/library/clientSkeletalAnimation/src/shared/appearance/SkeletalAppearanceTemplate.cpp`
  (`preloadAssets`/`preloadAssetsLight` ~437/455; how m_meshGenerators is populated and consumed)
- `src/engine/client/library/clientSkeletalAnimation/src/shared/appearance/SkeletalMeshGenerator.cpp` and `SkeletalMeshGeneratorTemplate.cpp`

Specifically answer:
1. What is the exact ordering: who requests the async .mgn, on which thread, and how does the result get
   handed back? Is there a window where a consumer holds/uses a MeshGeneratorTemplate* (or
   SkeletalMeshGenerator*) that is not yet fully constructed, or already released, or NULL?
2. The `create=false` fetch path (`asynchronousLoaderFetchNoCreate` → `fetch(cfn,false)`): what does it
   return if the entry isn't cached yet? Can a caller deref that result without a null/ready check?
3. Refcount/lifetime: can `asynchronousLoaderRelease` (or a normal release) free a template while another
   path still references it (cache eviction during in-flight use)?
4. Why would COLD CACHES / first-run-after-rebuild change the timing such that this window is hit more
   often? (e.g. async load takes longer, so the consumer runs before the load finishes.)

Output: concrete `file:line` references and the precise pointer/ordering hazard(s). If you find no hazard
in a path, say so. Do NOT propose a fix yet — just map the call graph and the unsafe deref/lifetime points.
