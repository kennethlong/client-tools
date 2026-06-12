# Research task: trace the object-local "camera-in-object-space" transform flow, D3D9 vs D3D11 (read-only)

## Established fact (verified at the GPU; do not re-litigate)
D3D11 (`gl11`) skeletal **soft-skin** draws map clean object-local vertices to clip-space **W ≈ 0 / negative** (object lands on the near plane → spikes); D3D9 (`gl05`) renders the same draws correctly. The vertex data is verified clean. So the per-object transform fed to the vertex shader is wrong only in D3D11.

This engine renders skeletal meshes **object-local**: rather than world-transforming every skinned vertex, it transforms the **camera into the object's space** and draws the object near its own origin (object world translation effectively identity in that space). A draw-time probe showed the world-translation the D3D11 path was using as `[0,0,0]`.

## Your task (repo: `src/engine/client/application/Direct3d9` vs `Direct3d11`, plus `clientGraphics`/`clientSkeletalAnimation`)
1. Find where the **camera-relative / object-local** view transform is computed for a skeletal draw, and how the object's position/orientation feeds it. Does `SoftwareBlendSkeletalShaderPrimitive::prepareToDraw()` (around the `Graphics::setObjectToWorldTransformAndScale` call) set a transform that the plugin then combines with the camera?
2. Compare how **D3D9** consumes that object-to-world+camera to form the final vertex transform vs how **D3D11** does (`Direct3d11_StateCache` `s_cachedWorld` / `flushSlot0IfDirty` / `objectWorldCameraProjectionMatrix`).
3. Specifically check: is the object's world translation being **dropped, zeroed, or applied in the wrong space/order** in D3D11 such that the object collapses toward the camera origin (near plane)? Is the camera position correctly subtracted/applied in each plugin?
4. Note any place D3D11 assumes a value (identity world, a cached transform, a frame-stale matrix) that D3D9 computes fresh per draw.

Deliverable: the object-local transform flow for each plugin with file:line, and the specific point where D3D11 could lose the correct object depth (yielding clip-W ≈ 0). Do not propose fixes.
