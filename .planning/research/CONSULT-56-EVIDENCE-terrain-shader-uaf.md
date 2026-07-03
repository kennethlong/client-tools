# CONSULT-56 — Shared evidence block (terrain-thread StaticShader map AV)

Treat everything in this section as GIVEN (measured from a WER minidump with cdb + verified in
source). Do not re-derive it; build on it.

## Measured crash (2026-07-03 07:39, SwgClient_r.exe Win32 Release, gl11 stack, zoning into Tatooine, ~30s process uptime)

Original exception (thread named "ClientTerrain", NOT the main thread), c0000005 READ:

```
SwgClient_r!std::_Tree<std::_Tmap_traits<unsigned long,Material,...>>::find+0xd   <- AV here
SwgClient_r!StaticShader::getMaterial+0x1a
SwgClient_r!ClientProceduralTerrainAppearanceShaderCacheNamespace::setupMaterial+0x6a
SwgClient_r!ClientProceduralTerrainAppearance::ShaderCache::createBlendedShader+0xaf
SwgClient_r!ClientProceduralTerrainAppearance::ClientChunk::createTileShader+0x599
SwgClient_r!ClientProceduralTerrainAppearance::ClientChunk::create+0x45a
SwgClient_r!ClientProceduralTerrainAppearance::createClientChunk+0x2f5
SwgClient_r!ClientProceduralTerrainAppearance::threadRoutine+0x129
```

- The faulting load: the map object's head/root pointer slot contained 0x66207265 — the ASCII
  bytes 'e','r',' ','f'. The accessed address 0x66207272 ('r','r',' ','f') = that value + 0xd.
  I.e. the memory where the std::map<Tag,Material> header lived now holds STRING TEXT (a fragment
  consistent with "...ader file..." / "...err f..."-style loader path strings). Freed-and-reused
  (or overwritten) memory, not a null/uninitialized pointer.
- The map being searched is `m_materialMap` of the StaticShader passed as `shaderData.inputShaders[i]`
  into setupMaterial (ClientProceduralTerrainAppearance_ShaderCache.cpp:406 -> :49).
- Meanwhile many other threads were alive and loading assets (async TreeFile loader, audio, etc.).
- A SECOND nested AV happened later inside the crash handler walking CrashReportInformation entries
  (garbage entry pointer). Separate symptom; not the subject of this consult.

## Source facts (verified in THIS tree, D:\Code\swg-client-v2)

1. `shaderData.inputShaders[i]` are the FAMILY/TILE shaders from
   `ShaderCache::getTextures` -> `cache[sgi.getPriority()][childIndex].shader`
   (ClientProceduralTerrainAppearance_ShaderCache.cpp:287-310; filled from
   ClientProceduralTerrainAppearance_ClientChunk.cpp:369 and :566).
2. Those family shaders are fetched ONCE in `ShaderCache::preloadShaders()` (ctor, via
   `ShaderTemplateList::fetchShader`) and released ONLY in `~ShaderCache`. The
   ClientProceduralTerrainAppearance dtor joins the terrain thread (`m_requestThread->wait()`,
   ClientProceduralTerrainAppearance.cpp:529-533) BEFORE `delete m_shaderCache` (:548) — clean
   teardown ordering.
3. `Shader::release()` is `if (--m_users <= 0) delete this;` with `m_users` a PLAIN (non-atomic)
   int (Shader.cpp:45-49). `fetch()` is the matching plain `++`.
4. For a SHARED StaticShader (fetchShader path, m_shared==true), `m_materialMap` POINTS INTO the
   StaticShaderTemplate's own map: `m_materialMap(const_cast<...>(staticShaderTemplate.m_materialMap))`
   (StaticShader.cpp:93). ~StaticShader deletes m_materialMap only when it differs from the
   template's (StaticShader.cpp:220-222). So the memory read at crash time is owned EITHER by a
   freed StaticShader's own copy (modifiable path) OR — for the shared family shaders — by the
   StaticShaderTemplate.
5. `ShaderCache::nodeList` (blended-shader cache) is mutated with NO locking from BOTH threads:
   - terrain thread: `findCachedShader` (iterates + ++node.referenceCount + timeout=0),
     `createBlendedShader` -> `nodeList.add(node)` (ClientProceduralTerrainAppearance_ShaderCache.cpp:351-434)
   - main thread: `alter()` (iterates, releases timed-out shaders, removeIndexAndCompactList, :453-478),
     `destroyShader` (--referenceCount, :438-449), `flushCache` (clear, :482-485).
   `ClientProceduralTerrainAppearance::alter` calls `m_shaderCache->alter` on the main thread (:889).
6. `createBlendedShader` creates each blended shader via `blendingShader[k]->fetchModifiable()`
   on the TERRAIN thread; every such clone's Shader ctor does `m_template.fetch()` and its eventual
   release (main-thread alter timeout) does `m_template.release()` — i.e. ShaderTemplate refcounts
   are ++/-- from BOTH threads during zone-in.
7. This build already fixed a similar-family bug: TreeFile::SearchCache unlocked std::map raced by
   the async loader (commit 9c03f53c5). That fix is IN this exe. The gl11 renderer is in use.

## Deliverable (each consultant, from your own angle)

Identify the concrete mechanism that frees (or overwrites) the memory holding the material map
that `StaticShader::getMaterial` reads for a ShaderCache FAMILY shader, while the ShaderCache still
holds its reference. Cite exact file:line for every step of your chain. Then propose the minimal,
retail-code-respecting fix, and how to verify it (probe/assert/repro). If you find MULTIPLE
credible mechanisms, rank them and say what evidence would discriminate.
