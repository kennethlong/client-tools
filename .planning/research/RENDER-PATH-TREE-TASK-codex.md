# Build the D3D9-vs-D3D11 render-path call tree from the PLUGIN-divergence side (read-only, exhaustive)

Repo: D:\Code\swg-client-v2. DEBUGGING-TOOL deliverable. Be exhaustive, every method call, file:line everywhere. A peer (Cursor) is building the same tree top-down; you build it from the renderer-plugin boundary so the two cross-check.

## Context
D3D11-only "spike" bug: certain skinned NPC meshes render ~10-30 TAIL vertices flung to a stale WORLD-space position. The vertex data is correct in the shared skinning output but spiked in the D3D11 dynamic VB the GPU reads. The shared `clientSkeletalAnimation` code is identical for D3D9 (gl05, src/engine/client/application/Direct3d9/) and D3D11 (gl11, src/engine/client/application/Direct3d11/). **Since the bug is D3D11-only, the cause is a behavioral DIVERGENCE between the two plugins at some call the skeletal path makes.** We need every such call enumerated and compared.

## Deliverable: write to D:\Code\swg-client-v2\.planning\research\RENDER-PATH-TREE-codex.md
1. **Enumerate every `Graphics::` / `Gl_api` call the skeletal render path makes** (dynamic VB create/lock/unlock, `VertexBufferWriteIterator::copy`, `setVertexBuffer`, `setVertexBufferVector`, `setIndexBuffer`, `drawIndexedTriangleList` / partial / strip, `setObjectToWorldTransformAndScale`, etc.), in call order, from `SoftwareBlendSkeletalShaderPrimitive::prepareToDraw`/`draw` and `FullGeometrySkeletalAppearanceBatchRenderer::renderBuffers` down to the GPU.
2. For EACH such call, give a **side-by-side D3D9 ∥ D3D11 table**:
   | call | Direct3d9 impl (file:line) | Direct3d11 impl (file:line) | behavioral divergence |
   Cover especially the **dynamic vertex buffer**: how D3D9 backs/locks a `DynamicVertexBuffer` (offset, discard, NOOVERWRITE, per-frame reset, the VB the GPU binds) vs D3D11's shared ring (`Direct3d11_DynamicVertexBufferData`: WRITE_DISCARD/WRITE_NO_OVERWRITE, `ringUsed`, `m_offset`, the bind byte-offset, BaseVertexLocation). And `VertexBufferWriteIterator::copy` (clientGraphics, shared — memcpy at VertexBufferIterator.h:884) and what `m_descriptor->vertexSize`/`m_data` resolve to under each backend.
3. **Rank the divergences by how well each could produce a TAIL-only, world-space, ~1692-vert spike that is sane in source but corrupt in the GPU-bound VB.**
4. Trace the SELECTOR that picks `m_skinningMode` (SoftwareBlendSkeletalShaderPrimitive.cpp:1138 — where does the `skinningMode` arg come from?) and whether D3D9 vs D3D11 could land the SAME mesh in different modes/primitive classes.

## Format
Method-call call-graph (every call, file:line), with the D3D9∥D3D11 comparison table at each plugin boundary, and a ranked "divergence suspects" section at the end.
