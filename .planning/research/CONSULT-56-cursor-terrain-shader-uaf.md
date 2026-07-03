# CONSULT-56 (Cursor) — lifetime audit: who can free the family-tile-shader material map

FIRST read `.planning/research/CONSULT-56-EVIDENCE-terrain-shader-uaf.md` (same dir) — the GIVEN
evidence block. Do not re-derive it.

Your angle (detailed file:line lifetime audit — you are the byte-map reader of the crew):

Enumerate EVERY site in this tree that can `fetch()`, `release()`, `delete`, or otherwise end the
lifetime of (a) a SHARED StaticShader returned by `ShaderTemplateList::fetchShader`, and (b) its
StaticShaderTemplate (whose m_materialMap the shared shader aliases). For each site state WHICH
THREAD it runs on (main loop / terrain "ClientTerrain" thread / async TreeFile loader thread) and
under what conditions it fires during ground-scene zone-in.

Files to walk (at minimum):
- src/engine/client/library/clientGraphics/src/shared/StaticShader.cpp / StaticShaderTemplate.cpp
- src/engine/client/library/clientGraphics/src/shared/ShaderTemplateList.cpp (fetch/release/purge/reload paths)
- src/engine/client/library/clientTerrain/src/shared/appearance/ClientProceduralTerrainAppearance_ShaderCache.cpp
- src/engine/client/library/clientTerrain/src/shared/appearance/ClientProceduralTerrainAppearance_ClientChunk.cpp
  (what the chunk does with inputShaders / the blended shader after createTileShader; chunk dtor)
- src/engine/client/library/clientTerrain/src/shared/appearance/ClientProceduralTerrainAppearance.cpp
  (threadRoutine, createClientChunk, alter, chunk destruction path)
- Any device-reset / flush / purge path that releases shaders (Graphics::flushResources etc.).

Pay special attention to:
- StaticShaderTemplate::fetchShader returning a CACHED shared StaticShader singleton per template
  (m_shared) — who else on which thread can be holding/releasing that SAME instance during zone-in
  (flora, radar, environment, ShaderPrimitiveSorter, other appearances using the same tile .sht)?
- StaticShaderTemplate::destroyStaticShader and ~StaticShaderTemplate — what deletes m_materialMap.
- Whether ANY main-thread system releases tile-family shaders/templates while the terrain thread
  runs (LevelOfDetail chunk eviction? ShaderTemplateList::stopLoading/purge?).

Deliverable per the evidence doc §Deliverable. Output = a numbered chain (site -> thread -> effect)
for each credible mechanism, exact file:line everywhere, then minimal fix + verification.
