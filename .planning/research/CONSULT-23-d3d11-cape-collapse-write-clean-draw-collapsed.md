# CONSULT-23 — D3D11 garment "texture spikes": VB clean at WRITE, collapsed at DRAW

## The ask
Identify the mechanism that collapses ~98 of a skinned mesh's 123 vertices to ONE identical
position **between a proven-clean CPU write into the D3D11 dynamic vertex buffer and the GPU's
deferred draw of that buffer.** Propose the minimal fix. **Do NOT re-propose any
buffer-lifetime restructuring** (see Dead Ends) unless you can explain why all 5 prior variants
failed AND why per-buffer isolation didn't help.

## Symptom
D3D11 renderer (in-house `gl11`, reimplements legacy SWG `Gl_api`). Close, soft-skinned
robed/garment NPCs render with vertical "texture spikes": ~98 of a draw's vertices collapse to
one point, a few stay correct, triangles fan from the good verts to the collapsed point. D3D9
(`gl05`, same content) is clean. The same character is clean far away (hard/no skinning) and
tears up close (soft skinning = CPU-skin into a dynamic VB each frame). Flickers mild↔extreme
frame to frame.

## PROVEN ground truth (this session, 2026-06-06) — trust these
1. **CPU skin is CLEAN.** A branch-independent engine probe replicates `fillVertexBuffer`'s
   position blend straight from source verts + bone transforms. Over 600 sampled skinned draws
   *while spikes were on screen*: ZERO collapse, max run of identical positions = 3, zero
   out-of-range bone indices. The garment (vc=110) had a healthy non-degenerate primary bone
   (`rotate(unitX).mag = 0.941`). So the skin produces correct DISTINCT positions.
2. **The D3D11 buffer is CLEAN at WRITE.** A probe in `Direct3d11_DynamicVertexBufferData::unlock()`
   reads back the just-written slice (CPU mapped ptr, before `Unmap`). Garment: numVerts=110,
   vSize=48, position `[0.2197 0.9081 0.0231]` (matches the engine skin), `m_offset (vertex) = 2670`,
   longest identical-run = 1. 500 samples, ZERO collapse at write, max run = 3. So 110 DISTINCT
   verts land cleanly in the D3D11 ring.
3. **The GPU reads COLLAPSED at draw.** RenderDoc capture (baseline build), the spiking draw
   (eid 2497, 123 verts): VB slots 0–24 valid robe geometry, slots 25–122 ALL identical
   `(-0.18310546875, -1.80419921875, 4.15966796875)` (post-WVP). Deterministic in the capture.
4. **The index buffer is NOT the cause.** Capture indices span 0–122 normally (don't all point at
   one slot). It is the VB SLOTS that are collapsed.
5. **The static offset/draw arithmetic is CORRECT.** Single-stream bind:
   `ms_currentVBOffset = data->getOffset() * stride` (vertex index→bytes), `stride =
   getVertexSize() = 48`. Draw: `DrawIndexed(primCount*3, startIndex, BaseVertexLocation=0)`; IB
   holds LOCAL indices [0,vertexCount). So GPU vertex = (m_offset + IB_local)*stride → reads the
   garment's slice. Verified in code; no constant double-count.

⇒ The collapse is born ENTIRELY between the clean write (step 2) and the deferred draw (step 3),
in the D3D11 path — not the skin, not the index buffer, not a static offset bug.

## Architecture facts
- ONE process-wide shared dynamic VB ring `ms_d3dRingBuffer` (`D3D11_USAGE_DYNAMIC`, 2 MB).
  `lock()`: `Map(WRITE_NO_OVERWRITE)` appending at byte `ms_used`; `Map(WRITE_DISCARD)` (resets
  `ms_used=0`, renames the buffer) only when `ms_newFrame || forceDiscard || ms_used+length >
  ms_size`. `m_offset = ms_used/vertexSize` (per-INSTANCE member). `ms_used` is STATIC/shared.
- `beginFrame()` (which sets `ms_newFrame=true`) has ZERO call sites → the ring NEVER resets
  per-frame; `ms_used` grows monotonically across frames, only wrapping (WRITE_DISCARD) on
  overflow. A prior trace measured ~370 WRITE_DISCARDs PER FRAME on baseline (driven by
  `CuiLayerRenderer` locking `getNumberOfLockableDynamicVertices()` = the whole ring and the
  lock-time over-charge `ms_used += length`).
- Draws are DEFERRED ACROSS PASSES (proven: adding a ring reset to the per-pass `beginScene`
  shredded geometry; the ring data must persist the whole frame). So a primitive's
  prepareToDraw (skin + lock/write/unlock) and its actual draw (bind + DrawIndexed) are
  separated in time, with other primitives' locks/discards in between.
- The spiking garment is the per-primitive `SoftwareBlendSkeletalShaderPrimitive` path (single
  small VB slice per draw, per-draw CPUWrite→VertexBuffer in the capture), NOT the batcher.
  Multi-stream VBs are OFF at runtime (single-stream).
- Soft-skin VS = plain WVP transform (inputs POSITION/NORMAL/TEXCOORD, NO blend weights →
  CPU-skinned positions). So a collapsed SV_Position can only come from a collapsed POSITION
  input — and the input is the dynamic ring slice.

## THE KEY PARADOX (please explain this)
- **"Fewer ring discards = WORSE; more discards = protective."** Every prior change that REDUCED
  WRITE_DISCARD frequency (D3D9-faithful accounting; per-frame reset) made the tearing WORSE.
  Forcing WRITE_DISCARD on EVERY lock made the spikes largely GONE (but dropped whole other
  meshes). The only "playable" state is the baseline with ~370 discards/frame.
- **The collapse is a CONTIGUOUS TAIL**: slots 0–24 good, 25–122 collapsed to ONE value.
- The collapsed value ×4096 = integers (-750, -7390, 17038) — looks like one specific replicated
  vertex, not random garbage.

## DEAD ENDS — already tried, all FAILED (do not re-propose without explaining the paradox)
1. Per-generation N-buffer ring (advance on discard). → spikes worse + RenderDoc-deterministic.
2. Per-frame `beginFrame()` flip wired into the once-per-frame update hook. → no change.
3. Uniform per-instance dynamic buffers (each VB its own ID3D11Buffer, WRITE_DISCARD/lock). →
   driver crash (nvwgf2um) from resource churn on world load.
4. Fence-retired N-buffer, whole-frame (4 buffers + ID3D11Query EVENT, per-frame flip). → spikes.
5. Per-slice-identity advance-on-discard, fence-retired (each slice carries its own buffer,
   bound at both bind sites). → SAME spikes. **This is the critical one: giving the garment its
   OWN buffer that nothing else discards did NOT help.**
6. Full per-frame CPU↔GPU serialization (`Flush`+`End(query)`+spin after Present). → spikes
   PERSIST. So it is NOT a CPU-write/GPU-read timing race.

## What would help from you
- A mechanism consistent with ALL of: clean-at-write, collapsed-at-draw, correct static offset,
  IB-not-the-cause, per-buffer-isolation-doesn't-help, serialization-doesn't-help,
  fewer-discards-worse, contiguous-25-then-98-identical-tail, D3D11-only, single shared ring,
  deferred-across-passes draws, `ms_used` monotonic / no per-frame reset.
- Specifically: how can a draw read 25 correct verts then 98 identical, from a slice that was
  written 110-distinct, when per-buffer isolation does NOT fix it? Is there an interaction
  between WRITE_NO_OVERWRITE append semantics, the deferred draw, and the IA bind offset we are
  missing? Is `getNumberOfLockableDynamicVertices()` / a consumer locking the WHOLE remaining
  ring (CuiLayer) relevant to a SKINNED slice that already wrote? Could the IA vertex-buffer
  binding be getting reset/changed between the garment's bind and its DrawIndexed by an
  intervening primitive that shares the static `ms_currentVB*` "current" state?

## Key files (repo D:/Code/swg-client-v2, branch koogie-msvc-cpp20-base)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_DynamicVertexBufferData.cpp`
  (lock @220, unlock @292, getOffset/getVertexSize, the ring).
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_StateCache.cpp`
  (setVertexBufferBindState @2233 single-stream bind @2258-2284; applyPreDrawState @1187,
  IASetVertexBuffers @1232/1241; drawPartialIndexedTriangleList @3221).
- `src/engine/client/library/clientSkeletalAnimation/src/shared/appearance/SoftwareBlendSkeletalShaderPrimitive.cpp`
  (prepareToDraw @483, single-stream branches @582-620, fillVertexBuffer @1617,
  RenderCommand::render @306 → draw(baseIndex=0, minVbIndex, numVerts, startIndex, primCount)).
