# Render-Path Reference — skeletal/skinned geometry, D3D9 ∥ D3D11 (debugging tool)

Built 2026-06-07 by merging two independent traces: `research/RENDER-PATH-TREE-cursor.md` (top-down) and `research/RENDER-PATH-TREE-codex.md` (bottom-up, plugin boundary). Purpose: walk every render path to localize the D3D11-only skinned "spike" bug, and serve as a reusable map for future render bugs.

All file:line refs are as of this date — verify against current source before acting.

## The bug (what we're hunting)
Certain skinned NPC meshes render ~10-30 **contiguous TAIL** vertices flung to a stale-looking **WORLD-space** position (`[-2816,0,-6400]`, Y=0), while the bulk is correct. **D3D11-only** (D3D9/gl05 clean, same mesh).
PROVEN: (a) skinning OUTPUT is sane — systemStream has no spike (`cape-skin-spike.txt` empty for the soft path); (b) the dynamic VB the GPU reads HAS the spike (`cape-dynvb-outlier.txt`: numVerts=1692 outliers=27 firstIdx=1665; numVerts=2691 outliers=10); (c) the copy writes every vert (sentinel: 387/387 full, 0 underwrite); (d) bone indices are all in range (clamp caught 0 OOB). ⇒ **The spike enters AFTER skinning, in the D3D11 dynamic-VB / draw layer.**

## Leaf path for the spike mesh (both trees agree)
```
SkeletalAppearance2::render  (LOD select)
  setSkinningMode(...)                         SkeletalAppearance2.cpp:1731-1742  (noSkin if screenFrac<=thresh; HARD if allowed/small/forced; else SOFT)  applied :1769
  useBatcher? NO                               (batch renderer DORMANT — cape-batch-probe absent)
  → SoftwareBlendSkeletalShaderPrimitive
     ms_useMultiStreamVertexBuffers == 0       latched from caps SBSSP.cpp:340  (D3D11 advertises 2 streams + stream offsets Direct3d11.cpp:228/232 — CAN differ from D3D9 caps:1543)
     m_systemStream exists, !m_hasDot3Vector   → single-stream system→dynamic COPY path
     m_skinningMode == SM_hardSkinning         → fillVertexBufferHard (SSE @2305 / scalar @2543)  ★ MONEY LEAF (uninstrumented; soft fill @1832 exonerated)
     m_dynamicStream->lock(m_vertexCount)       SBSSP.cpp:776
     outIt.copy(systemStream, getNumberOfVertices())  SBSSP.cpp:779  → memcpy VertexBufferIterator.h:884
     Graphics::setVertexBuffer(*m_vertexBufferVector)  SBSSP.cpp:819
     Graphics::setIndexBuffer(*m_indexBuffer)          SBSSP.cpp:823
  → SBSSP::draw → RenderCommand::render → drawPartialIndexedTriangleList  SBSSP.cpp:306-314
     → D3D11 DrawIndexed  Direct3d11_StateCache.cpp:3451   (D3D9 DrawIndexedPrimitive Direct3d9.cpp:4157)
```
NOTE on coordinate space: both traces say CPU soft/hard fill writes OBJECT space (model-to-root via `getBindPoseModelToRootTransforms`, SBSSP.cpp:563); world transform is applied by the VS cbuffer (`setObjectToWorldTransformAndScale` SBSSP.cpp:491), NOT in the VB. **A WORLD-space dynamic VB is therefore NOT a normal fill output → it implicates stale-ring / read-past / wrong-slice, i.e. the GPU fetching bytes the fill never wrote.** (Open thread: confirm the spike mesh's fill space directly — fillVertexBufferHard is not yet instrumented.)

## D3D9 ∥ D3D11 divergence table (the bug lives here)
| # | Stage | D3D9 (gl05) | D3D11 (gl11) | Spike risk |
|---|-------|-------------|--------------|------------|
| 1 | **Dynamic VB backing** | ONE shared dynamic VB | **dual ring**: skin ring for vSize≠28,24 (Direct3d11_DynamicVertexBufferData lock:259-335, routing ~:268) | **HIGH** stale tail |
| 2 | **Per-frame ring reset** | rings reset in `beginScene()` Direct3d9.cpp:2485 | plain dynamic VB/IB `beginFrame()` **has NO caller**; only skin-ring reset wired Direct3d11.cpp:297 | **HIGH** cross-frame stale (plain ring) |
| 3 | **Ring advance timing** | advances `ms_used` on **unlock** D3d9_DynVB:231 | advances `ringUsed` during **lock** D3d11_DynVB:335 | MED slice accounting |
| 4 | **Vertex-fetch bound** | `DrawIndexedPrimitive(BaseVertexIndex, MinVertexIndex, NumVertices, ...)` bounds fetch Direct3d9.cpp:4157 | `DrawIndexed(IndexCount,Start,BaseVertex)` — **no Min/Num bound** :3451 | **HIGH** read-past → stale |
| 5 | **Slice offset application** | `SetStreamSource(byteOffset)` + `BaseVertexIndex=sliceFirstVertex+baseIndex` | IA byte offset absorbs slice (VBVectorData:145), draw `BaseVertexLocation=baseIndex` :3451 | MED misbind |
| 6 | **Tri-strip index semantics** | engine IB | strip raw-vs-compacted read-past noted StateCache:3462-3467 | MED if strips |
| 7 | **Multi-stream selection** | caps-dependent Direct3d9.cpp:1543 | advertises 2 streams + offsets :228/232 | LOW-MED layout divergence |

## Ranked suspects (both trees, merged)
1. **D3D11 dynamic-VB ring reuse/generation between lock/unlock and the (GPU-async) draw** — best fit for source-sane/GPU-corrupt/tail-only/stale-world. (Codex #1, Cursor #1)
2. **Read-past: index ≥ written vertex count, unbounded on D3D11** (`DrawIndexed` lacks D3D9's MinVertexIndex/NumVertices). Tail verts fetch stale ring. (Cursor #4) — BUT cape-index-probe reported overRead=0 for tri-lists; re-verify with the ACTUAL draw's m_numberOfVertices vs index range (RenderCommand m_numberOfVertices = maxVbIndex-minVbIndex+1, StateCache:1537 — may differ from the lock count).
3. **Per-frame ring NOT reset on D3D11 plain ring** (no beginFrame caller). Skin ring IS reset (:297) — so this bites the PLAIN ring (vSize 28/24). Confirm which ring the spike mesh's slice is on.
4. **fillVertexBufferHard (SSE) itself** — uninstrumented; could write world-space or a bad tail. Confirm its output space + tail first (cheap: scalar path @2543).

## THE WALK (do in this order)
- [ ] **W1. Instrument `fillVertexBufferHard` output** (SSE @2305 + scalar @2543): confirm the spike mesh uses it, and whether ITS output is sane object-space (→ pure D3D11 ring bug) or already world/spiky (→ hard-fill bug). This closes the "is the source REALLY sane for THIS mesh" gap.
- [ ] **W2. Which ring + offset does the spike slice use?** Log in Direct3d11_DynamicVertexBufferData::lock for the spiking mesh: skin-vs-plain ring, ringUsed/m_offset, discard flag, ringNewFrame. Cross-check the GPU bind byte-offset (StateCache:2292/1239) equals the write pointer.
- [ ] **W3. Read-past check with the REAL draw counts** — compare RenderCommand m_numberOfVertices (StateCache:1537) and the index max for the spiking DrawIndexed vs the written slice length. Re-run cape-index-probe semantics against the actual draw, not the lock.
- [ ] **W4. Diff the D3D9 path live** — boot gl05, same mesh, same probe points; confirm D3D9 writes/binds/draws the identical slice cleanly (per divergence #2/#4 the bound is the difference).
- [ ] **W5. If ring-generation confirmed:** test fix = give the skin/plain ring proper per-frame reset (wire beginFrame) OR bound the D3D11 draw's vertex range / force WRITE_DISCARD per skinned lock.

## Other leaves (for completeness / future bugs)
- Batch: `FullGeometrySkeletalAppearanceBatchRenderer::renderBuffers` — WORLD-space CPU bake (line 230), plain dynamic VB, `setObjectToWorld = identity` (:328). DORMANT this session. If it activates, vertices are world-space and a stale tail looks exactly like our symptom — and its plain ring may not reset (divergence #2).
- Soft fill: SBSSP `fillVertexBuffer` @1832 (dot3 @1848 / non-dot3 @1905) — EXONERATED (instrumented sane).
- Rigid hardpoint attachments: MeshAppearance/ShaderPrimitiveSet — object-space, not vSize=36 skin ring.
- Debug: s_showSkeleton skeleton overlay.

## Sources
- `research/RENDER-PATH-TREE-cursor.md` (510 lines, top-down, leaf decision tree + divergence table)
- `research/RENDER-PATH-TREE-codex.md` (8097 lines, every-method call graph + plugin-boundary tables + selector trace)
