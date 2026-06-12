# CONSULT-24 — D3D11 garment spike: VB data PROVEN correct at draw, yet still spikes

Follow-up to CONSULT-23 (same bug, same repo D:/Code/swg-client-v2, branch koogie-msvc-cpp20-base,
in-house D3D11 `gl11` plugin reimplementing legacy SWG `Gl_api`). CONSULT-23's convergent answer
(stale vector bind offset → rebind at draw) was IMPLEMENTED and FALSIFIED. New gold-standard data
below. **Do NOT re-propose any falsified hypothesis (see list). Explain the discard inversion.**

## The ask
Given that the vertex data the GPU actually reads at the garment draw is PROVEN correct (staging
read-back, below), what produces the live spike, and what is the minimal fix? Our leading
hypothesis is an index-range-exceeds-locked-vertex-count read-past; confirm/refute and explain
why it is D3D11-only.

## Symptom (unchanged)
Close, soft-skinned robed/garment NPCs render with vertical "texture spikes": most of a draw's
vertices collapse to ONE point, a few stay correct, triangles fan out. D3D9 (`gl05`) renders the
same content clean. Soft-skin = CPU-skin into a dynamic VB each frame (close LOD). Robe-specific
(player body skins fine; the worn robe spikes). Flickers mild↔extreme.

## PROVEN ground truth (cumulative; trust these)
1. **CPU skin is CLEAN.** Engine probe replicating fillVertexBuffer's blend: 600 sampled skinned
   draws while spiking, ZERO collapse, max identical-run=3, no out-of-range bone indices.
2. **The D3D11 dynamic-VB WRITE is CLEAN.** Probe in `Direct3d11_DynamicVertexBufferData::unlock()`
   reading the just-written slice before Unmap: the garment writes numVerts DISTINCT verts (e.g.
   vc=110, pos=[0.22,0.92,0.026]); 500 samples, ZERO collapse.
3. **The bound offset at the draw is CORRECT.** After adding a rebind at draw() (CONSULT-23 fix),
   the draw's vector bind offset matches the write offset. STILL SPIKED → bind/offset was never it.
4. **GOLD STANDARD — staging read-back at the garment draw.** Added a probe in
   `Direct3d11_StateCache::drawPartialIndexedTriangleList`: right before `DrawIndexed`,
   `CopySubresourceRegion` the BOUND ring region → a STAGING buffer → `Map(READ)` → scan. This is
   EXACTLY what the GPU is about to fetch. Result (cape-stage-probe.txt):
   - The garment's verts **[0, numVerts) are CORRECT and ANIMATING** (firstPos matches the write
     and drifts frame-to-frame). Beyond numVerts the copy reads **pure ZERO [0,0,0]**. The zero
     boundary `runStart` == the write `numVerts` EXACTLY (2670→110==110, 1028→179==179).
   - i.e. the written slice is intact; the ring memory PAST the slice is zero.
5. **The per-draw `Map(READ)` forces a full GPU FLUSH at every sampled garment draw — and the
   spikes STILL appear.** So it is NOT a timing/async race that per-draw serialization fixes
   (frame-level full CPU↔GPU serialization also failed earlier).
6. **The engine COLLIDE path explicitly guards against indices >= m_vertexCount.**
   `SoftwareBlendSkeletalShaderPrimitive.cpp:1037-1040`: for HUD-target raycast it does
   `if (i0<0||i0>=m_vertexCount||...) continue;` with the comment "a stale or corrupt index
   buffer can never advance the iterator past the system stream's allocation." This guard EXISTS
   because index buffer values CAN be >= m_vertexCount. **The RENDER path has NO such guard** — it
   hands the raw IB to `DrawIndexed`.

## LEADING HYPOTHESIS (please confirm/refute with file:line)
The soft-skin index buffer references vertex indices **>= m_vertexCount** (the value passed to
`m_dynamicStream->lock(m_vertexCount)`). The D3D11 lock writes only `[0, m_vertexCount)`; the
shared dynamic ring memory past the slice is zero (fresh WRITE_DISCARD generation). `DrawIndexed`
(no vertex-range bound in D3D11) fetches those out-of-range indices → reads the ZERO tail →
those vertices collapse to the WORLD ORIGIN (0,0,0) → spike fanning from the mesh to origin.
- Fits ALL constraints: skin/write/bind all clean; data the draw reads for in-range indices is
  correct; serialization can't help (not a race; the zero tail is deterministic); per-buffer
  isolation can't help (the index range is the problem, not the buffer).
- D3D11-only because D3D9 `DrawIndexedPrimitive(BaseVertexIndex, MinIndex, **NumVertices**,
  StartIndex, PrimCount)` carries a NumVertices vertex-range bound, and/or D3D9's ring tail holds
  non-zero stale geometry that renders less catastrophically. D3D11 `DrawIndexed` has no such bound
  (handoff-confirmed: "D3D11 DrawIndexed has NO min/numVertices bound, unlike D3D9").
- Discard inversion: more WRITE_DISCARDs → the past-slice tail is more often a fresh zeroed/benign
  generation; the read-PAST is the constant; "fixing" discards changed tail contents, not the
  read-past, hence no consistent improvement.

## Questions
1. Does the soft-skin IB actually reference indices >= m_vertexCount? WHERE does the mismatch come
   from — `buildIndexBufferAndRenderCommands` (SoftwareBlendSkeletalShaderPrimitive.cpp:1466) builds
   the IB from `mesh.getTriListTriangle(meshPerShaderData,...)`; `m_vertexCount =
   mesh.getVertexCount(meshPerShaderData)` (:1194). Can per-shader triangle indices exceed the
   per-shader vertex count (e.g. shared/global mesh indices, a tri-strip degenerate, or an LOD
   mismatch)? RenderCommand stores `(maxVbIndex - minVbIndex)+1` as numberOfVertices and `minVbIndex`.
2. If indices are in fact bounded to [0,m_vertexCount), then the VB the draw reads is fully correct
   — so WHAT ELSE makes it spike? (index connectivity wrong-but-in-range? a different primitive we
   are not sampling? a per-draw constant/transform? the dynamic INDEX buffer for a batched path?)
3. Minimal fix: lock `maxIndexUsed+1` instead of `m_vertexCount`? Mirror the collide bound by
   clamping/validating the render IB? Pad/zero-guard is NOT a real fix (degenerate tris at origin
   still draw). Or fix the mesh/index data. Which, and why?

## Falsified — DO NOT re-propose
- CPU-write/GPU-read RACE — frame-level AND per-draw serialization both failed; data is
  deterministic in capture.
- Overwrite of the written slice — staging shows [0,numVerts) correct and animating at draw.
- Stale/clobbered vector bind offset (ms_currentVBVectorOffsets[0]) — rebind at draw() made the
  offset correct; STILL SPIKED.
- 5 dynamic-VB buffer-lifetime redesigns (per-generation, per-frame flip, uniform per-instance →
  driver crash, fence N-buffer, per-slice fenced) — all failed.
- ms_used accounting fix (charge actual at unlock like D3D9) — made it visibly WORSE.

## Key files
- `src/engine/client/library/clientSkeletalAnimation/src/shared/appearance/SoftwareBlendSkeletalShaderPrimitive.cpp`
  — prepareToDraw @483, lock(m_vertexCount) @737/754/776, draw() @823, RenderCommand::render @306
  (draw(baseIndex=0, minVbIndex, numberOfVertices, startIndex, primCount)), m_vertexCount @1194,
  buildIndexBufferAndRenderCommands @1466 (minVbIndex/maxVbIndex/createTriList @1544), collide
  index guard @1037.
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_StateCache.cpp` —
  drawPartialIndexedTriangleList @3221 (DrawIndexed(primCount*3, startIndex, baseIndex=0); NO
  vertex-range bound), single-stream/vector bind @2258/2316.
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_DynamicVertexBufferData.cpp` —
  lock @220 (writes only numberOfVertices*vertexSize), WRITE_DISCARD @250-261.
- D3D9 reference: `src/engine/client/application/Direct3d9/src/win32/Direct3d9_StateCache.cpp`
  DrawIndexedPrimitive (note the NumVertices argument).
