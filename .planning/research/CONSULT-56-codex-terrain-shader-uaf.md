# CONSULT-56 (Codex) — cross-thread call-graph of the terrain shader cache + shader refcounts

FIRST read `.planning/research/CONSULT-56-EVIDENCE-terrain-shader-uaf.md` (same dir) — the GIVEN
evidence block. Do not re-derive it.

Your angle (repo tracer / call-graph):

Build the cross-thread call graph for the two shared-state clusters below, labeling every edge with
the thread it executes on during ground-scene zone-in (main loop vs the "ClientTerrain"
MemberFunctionThreadZero thread vs any other worker):

Cluster A — ClientProceduralTerrainAppearance::ShaderCache instance state:
  nodeList, cache[][], blendingShader[]/blendingShaderSpecular[]:
  all callers of findCachedShader / createBlendedShader / destroyShader / alter / flushCache /
  getTextures / preloadShaders. Which of these can run CONCURRENTLY? Which mutate?

Cluster B — Shader / ShaderTemplate reference counts (plain non-atomic ints):
  every fetch()/release() edge reachable (transitively) from
  (a) ClientProceduralTerrainAppearance::threadRoutine and
  (b) the main game loop during zone-in (chunk destruction, LevelOfDetail, alter, rendering,
      Graphics flush/reset, ShaderTemplateList operations, GroundEnvironment, flora/radar).
  Flag every object whose refcount gets ++/-- from BOTH threads.

Also trace: ClientChunk lifetime — where chunks are deleted (which thread), and what shader
references a ClientChunk holds/releases at destruction (ShaderSet::m_shader is borrowed — so who
owns the blended shader's reference, and who calls ShaderCache::destroyShader).

Search hints: MemberFunctionThreadZero("ClientTerrain"), threadRoutine, m_completedChunkRequestInfoList,
LevelOfDetail, ClientTerrainSorter, ShaderTemplateList::, fetchModifiable, destroyStaticShader.

Deliverable per the evidence doc §Deliverable: concrete mechanism chain(s) with file:line, ranked;
minimal fix; verification probe.
