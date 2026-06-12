# Find the code path that writes THIS dynamic vertex buffer (read-only, exhaustive search)

Repo: D:\Code\swg-client-v2 (D3D11 SWG client). We have a D3D11-only rendering "spike" and have PROVEN it is written by an unknown code path. Find that path.

## Exact fingerprint of the buffer (treat as hard facts — measured at runtime in the D3D11 plugin)
A **DynamicVertexBuffer** is locked/filled with:
- **vertexSize = 36 bytes** = position(float3,12) + normal(float3,12) + color0(BGRA,4) + ONE 2D texture-coordinate set(float2,8). (i.e. `VertexBufferFormat`: setPosition + setNormal + setColor0 + 1 tcSet dim 2.)
- **~1647 and ~2664 vertices** per lock (two different draws).
- **WORLD-space positions**: the vertex bulk centroid is at an NPC's world location ~[3307, 4.9, -5002] (the creature stands there). So the object→world transform is **baked into the vertices on the CPU** (these are NOT object/model-space verts).
- The **last ~10-30 vertices (a contiguous TAIL)** are garbage at a stale world position ~[-2816, 0, -6400] (Y≈0). The bulk is correct.
- Routed to the D3D11 "skin ring" (the plugin sends any vSize≠28 and ≠24 to a dedicated dynamic ring).

## What it is NOT (ruled out by instrumentation — do not re-propose these)
- NOT `SoftwareBlendSkeletalShaderPrimitive` (clientSkeletalAnimation). Its prepareToDraw was instrumented (path log + systemStream read); the 1647/2664 vertex counts NEVER appear there, and its output is OBJECT-space (~±3 units), not world.
- NOT `FullGeometrySkeletalAppearanceBatchRenderer`. Its batch VB format is position+normal+color = **28 bytes**, not 36, and its renderBuffers did not run this session (probe absent).

## Your task
Enumerate EVERY code path in the client that LOCKS/FILLS a `DynamicVertexBuffer` (clientGraphics `DynamicVertexBuffer::lock`/`begin`, or `Graphics::setVertexBuffer` of a dynamic VB) whose format is **position+normal+color0+1×2D-texcoord (vertexSize 36)** AND whose vertices are written in **WORLD space** (object-to-world baked on the CPU, e.g. via `transform->rotateTranslate` / `getTransform_w` / `getObjectToWorld`). 

Search the WHOLE repo, not just clientSkeletalAnimation: clientGraphics, clientGame, clientObject, particle/swoosh/ribbon systems, shadow volumes, MeshAppearance / ShaderPrimitiveSet, DetailAppearance, any Appearance that pre-bakes world transforms, ShaderPrimitiveSorter, Light/decal/projection systems, anything that batches or world-bakes skinned/animated geometry.

For each candidate: the class + lock site (file:line), the VB format it uses (confirm vertexSize), whether it writes WORLD-space, the per-vertex write loop, and HOW the tail could end up stale (count mismatch between lock count and write count, a dangling/garbage transform pointer for the last sub-batch, an unwritten remainder).

## Deliverable
Ranked list of candidate writers that match the 36-byte WORLD-space fingerprint, with file:line for the lock + the write loop, and for the #1 candidate, the exact mechanism by which the LAST ~27 vertices would get a stale world-space position. The bug is D3D11-only, tail-only, on an NPC-centered world-space buffer.
