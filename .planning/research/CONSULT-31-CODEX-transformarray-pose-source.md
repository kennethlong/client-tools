# Research: where does transformArray (the per-frame pose) come from, and is every valid index filled? (read-only, no fixes)

Repo: D:\Code\swg-client-v2. Skin file: src/engine/client/library/clientSkeletalAnimation/src/shared/appearance/SoftwareBlendSkeletalShaderPrimitive.cpp

## Established facts (treat as given)
- A skinned creature's last ~27 vertices (of 1656) render at wrong WORLD-space positions ~[-2816,0,-6400] (Y exactly 0); the rest are correct at the creature's world pos ~[3400,5,-4900].
- The vertex buffer is fully written; bone indices used are all IN RANGE [0,transformCount) (a clamp caught zero out-of-range). So valid indices produce wrong positions.
- The soft-skin math (~line 1862/1905): `blendPosition += transformArray[idx].rotateTranslate_l2p(sourcePosition) * weight`. `transformArray` (type `PoseModelTransform *`) and `transformCount` are passed into `skinData(transformCount, transformArray, iterator)`.

## Your task (trace the call chain; cite file:line)
1. Find every caller of `skinData(...)` / `fillDynamicVertexBuffer(...)` on SoftwareBlendSkeletalShaderPrimitive and where `transformArray` + `transformCount` originate. What object owns/builds that PoseModelTransform array each frame (SkeletalAppearance2 / Skeleton / TransformAnimationController / etc.)?
2. Is EVERY entry transformArray[0..transformCount) written/updated each frame, or can a valid index point to a STALE pose (previous frame) or an UNINITIALIZED/default pose (allocated but never filled this frame)? Look for the array's allocation + per-frame fill loop and whether the fill count can be < transformCount.
3. transformCount semantics: is it the skeleton's bone count, the mesh's referenced-bone count, or a max? Could the mesh reference a valid index whose bone is not part of the currently-bound skeleton's animated set (so its pose is default/identity/garbage)?
4. LOD angle: how is the per-shader vertex set + its stored bone indices selected per LOD? Do the high-LOD mesh's stored bone indices correspond to the SAME ordering as the runtime pose array? Could a high-LOD mesh be skinned against a pose array built for a different (lower) detail level or a different skeleton?

Deliverable: the provenance of transformArray, a yes/no on "a valid index can resolve to a stale/unfilled pose," and whether a LOD/skeleton-ordering mismatch is possible — with file:line.
