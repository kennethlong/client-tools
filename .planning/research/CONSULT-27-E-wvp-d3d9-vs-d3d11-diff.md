# Research task: DIFF the per-object WVP composition + upload, D3D9 vs D3D11 (read-only, no fixes)

## Established fact (verified at the GPU — do NOT re-investigate whether it's true)
In the D3D11 renderer plugin (`gl11`, `src/engine/client/application/Direct3d11`), skeletal **soft-skin** draws transform **clean, correct object-local vertices** (e.g. `(0.2, 1.07, 0.06)`, normal ±1 range — confirmed distinct/uncorrupted by a draw-time staging read-back) to **clip-space W ≈ 0, frequently NEGATIVE**, instead of the correct ~`+cameraDistance`. The same geometry under the D3D9 plugin (`gl05`, `src/engine/client/application/Direct3d9`) renders correctly. So the **per-object world×view×projection matrix uploaded to the vertex shader for these draws is wrong in D3D11** — it lands the object on/behind the near plane → perspective blow-up → vertex "spikes". The vertex buffer is NOT the cause.

The draws come from `SoftwareBlendSkeletalShaderPrimitive` (`src/engine/client/library/clientSkeletalAnimation`). This engine renders skeletal meshes **object-local** (the camera is transformed into object space; the object's own transform contributes the world part). The D3D11 slot-0 cbuffer matrix is `objectWorldCameraProjectionMatrix` (see `Direct3d11_StateCache.cpp`: `s_slot0Shadow`, `s_cachedWorld`, `flushSlot0IfDirty`); the engine uses **column-vector** matrices while the HLSL bytecode expects **row-vector** (matrices are transposed at upload).

## Your task (DIFF, with file:line for BOTH plugins)
Trace and directly compare, between `Direct3d9` and `Direct3d11`, the FULL path by which a per-object WVP reaches the vertex shader for a skeletal/object-local draw:

1. **Where the object-to-world transform enters.** `Graphics::setObjectToWorldTransformAndScale` (or equivalent) → each plugin's handler. How does D3D9 store/use it vs how D3D11 builds `s_cachedWorld`?
2. **Where world is combined with camera (view) and projection.** Find the exact composition site in each plugin. Is the camera-in-object-space transform applied identically? Where does the projection matrix come from in each?
3. **Where the final matrix is uploaded.** D3D9: `SetVertexShaderConstantF` / fixed-function `SetTransform` / `setVertexShaderConstants`. D3D11: the slot-0 cbuffer write in `flushSlot0IfDirty`. Compare the transpose/handedness/row-order handling.
4. **Enumerate every difference** between the two plugins in steps 1–3 that could cause D3D11's matrix to map an object-local vertex to clip-W ≈ 0 / negative while D3D9 maps it correctly. Rank the candidate divergences by likelihood.

Deliverable: a side-by-side trace (D3D9 path | D3D11 path) with file:line, then a ranked list of concrete divergences. Do not propose code changes; just locate the divergence.
