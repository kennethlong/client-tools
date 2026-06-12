# Research: trace per-vertex bone bookkeeping, construction vs consumption (read-only, no fixes)

Repo: D:\Code\swg-client-v2. File: src/engine/client/library/clientSkeletalAnimation/src/shared/appearance/SoftwareBlendSkeletalShaderPrimitive.cpp

## Established facts (treat as given — do not re-litigate)
- A skinned creature mesh (1656 verts) renders with ~27 vertices at wrong WORLD-space positions; the other 1629 are correct on the creature.
- The bad vertices are the CONTIGUOUS TAIL: vertex indices 1629..1655. Their positions cluster near [-2816, 0, -6400] (Y exactly 0); the creature's correct bulk is at world ~[3400, 5, -4900].
- The destination vertex buffer is FULLY written (no truncation/under-write — proven by a sentinel fill: 0 untouched verts across 600+ draws).
- Every bone index used by the skin loop is IN RANGE [0, transformCount) (a runtime clamp caught zero out-of-range). So the wrong positions are produced from VALID indices.
- Only high-LOD (close-up) mesh variants exhibit this; low-LOD variants are clean.

## Your task (meticulous trace; cite file:line)
Map the per-vertex bone bookkeeping CONSTRUCTION against its CONSUMPTION and find any way they can disagree for the tail vertices:
1. CONSTRUCTION (~lines 1297-1384): how `numberOfExtraTransformWeightPairs` is counted (note the `localVertexTransformCount>1` condition at ~1304), how `m_transformData` is sized/filled, and how each `m_sourceVectors[i].m_firstTransformData` / `m_extraTransformDataCount` is set — including the `localVertexTransformCount<1` branch (~1359) and the `==1` case. Note the block is `memset(...,0,...)` at ~1316.
2. CONSUMPTION (~lines 1846-1938, the soft-skin loop): the SHARED pointer `const TransformData *transformData = m_transformData;` advanced by `m_extraTransformDataCount` per vertex inside the loop. 
3. The invariant: state exactly what must hold for the shared `transformData` pointer to stay in sync across all m_vertexCount vertices. Can any vertex's `m_extraTransformDataCount` (as consumed) differ from what construction accounted for — desyncing the pointer so that LATER (tail) vertices read the wrong `TransformData` (valid index, wrong bone/weight)? Check the `==1` and `<1` cases especially.
4. Does `m_systemStream` vs `m_vertexCount` vs the index buffer's max index agree on the vertex count for this mesh/LOD?

Deliverable: the construction-vs-consumption invariant, and a yes/no on "can the shared transformData pointer desync for tail verts," with the exact line(s) where it would break and which vertex categories trigger it.
