# Build a COMPLETE render-path tree for skinned/skeletal geometry (read-only, exhaustive)

Repo: D:\Code\swg-client-v2 (D3D11 client). This is a DEBUGGING-TOOL deliverable — a reference tree we will walk to localize a rendering bug. Be exhaustive and precise (file:line everywhere).

## Why
We have a D3D11-only "spike" bug: certain skinned NPC meshes render ~10-30 tail vertices flung to a stale WORLD-space position. We've proven it is NOT in `SoftwareBlendSkeletalShaderPrimitive::fillVertexBuffer` (the SOFT path — instrumented, output sane, OBJECT-space) and NOT in `FullGeometrySkeletalAppearanceBatchRenderer` (dormant this session). The spiking dynamic VB is WORLD-space, vSize=36 (position+normal+color+1 2D texcoord, NO dot3), ~1692/2691 verts. We keep finding paths one at a time. We need the WHOLE tree.

## TWO HARD REQUIREMENTS (Kenny)
1. **EVERY METHOD CALL.** Not just leaves — the FULL call chain, method by method, from the scene/ShaderPrimitiveSorter render down to the GPU draw (`ID3D11DeviceContext::DrawIndexed` for D3D11 / `IDirect3DDevice9::DrawIndexedPrimitive` for D3D9). Include the VB lock, the skin/fill, the copy, `Graphics::setVertexBuffer`, `setIndexBuffer`, the draw — and every method they call through. Depth over brevity.
2. **D3D9 ∥ D3D11 SIDE-BY-SIDE.** The shared `clientSkeletalAnimation` code is identical for both backends; the DIVERGENCE is in the renderer plugin (`Direct3d9/` = gl05 vs `Direct3d11/` = gl11). For every call that crosses `Graphics::` → `Gl_api` → the plugin (lock/unlock/copy/setVertexBuffer/setIndexBuffer/draw/dynamic-VB ring/offset/stride), show BOTH the Direct3d9 implementation (file:line) AND the Direct3d11 implementation (file:line), and **FLAG every behavioral divergence** (lock flags, WRITE_DISCARD/NO_OVERWRITE ring vs D3D9 dynamic VB, byte-offset/BaseVertex handling, stride, copy semantics, vertex-count rounding). **The bug is D3D11-only, so a divergence point IS a prime suspect — call those out explicitly.**

## Deliverable: write a structured tree to D:\Code\swg-client-v2\.planning\research\RENDER-PATH-TREE-cursor.md
Enumerate EVERY path by which a skeletal/skinned (and skeletally-attached rigid) mesh reaches a GPU draw in this client, as a full method-call call-graph with D3D9 ∥ D3D11 columns at every plugin-divergence point. For EACH leaf path, give:
- **Selector**: the exact condition + file:line that routes a mesh down this branch (e.g. what sets `m_skinningMode` at SoftwareBlendSkeletalShaderPrimitive.cpp:1138 — trace the `skinningMode` argument back to its source; what decides batched vs non-batched; multi-stream vs single-stream vs skin-direct-to-dynamic at lines 668/757/769).
- **Primitive class** + the fill function (file:line).
- **Coordinate space of the written vertices**: OBJECT (model/model-to-root) or WORLD (object-to-world applied on CPU). State which transform is applied and where.
- **Vertex buffer**: which VB is locked/written (systemStream, dynamic stream, batch VB, static), the lock site (file:line), and whether there's a systemStream→dynamic copy.
- **Draw call** (drawIndexedTriangleList / partial / strip / etc.) and where.
- **Instrumentation point**: the single best file:line to read the written positions for THIS path.

## Structure (use this shape)
```
SkeletalAppearance2::render (LOD select) → per-shader ShaderPrimitive
├─ SoftwareBlendSkeletalShaderPrimitive   [selector: ...]
│  ├─ SM_softSkinning   → fillVertexBuffer (soft)      [space=?, VB=?, copy=?, draw=?, instrument=?]
│  │   ├─ dot3 branch
│  │   └─ non-dot3 branch
│  ├─ SM_hardSkinning   → fillVertexBufferHard (SSE @2305 / scalar @2543)  [space=?, VB=?, ...]
│  └─ SM_noSkinning     → fillVertexBufferHard                              [space=?, VB=?, ...]
│     └─ stream sub-paths: multi-stream(668) / skin-direct-to-dynamic(757) / system→dynamic copy(769)
├─ FullGeometrySkeletalAppearanceBatchRenderer  [selector: when is an appearance batched?]
├─ <any other skeletal ShaderPrimitive subclasses you find>
└─ hardpoint-attached rigid meshes (MeshAppearance / ShaderPrimitiveSet)  [object→world? where?]
```

## The money question (answer explicitly)
Given a mesh that ends up as a **WORLD-space, vSize=36 (pos+normal+color+1 2D uv, no dot3), ~1692-vert dynamic VB drawn as a skeletal NPC** — **which leaf path produces it?** Trace the selector chain that lands a mesh there. If it's `SM_hardSkinning`/`SM_noSkinning` writing world-space, explain what supplies the world transform and which `transformArray`/`transform` the hard fill uses.
