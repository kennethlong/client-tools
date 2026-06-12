# Research task: does the soft-skin VS input layout read vertex bytes the engine never writes? (read-only)

## Established (treat as given)
D3D11-only skinned-character "spikes." New evidence reframes it: the spike is **live-only** — RenderDoc *replay* renders it nearly clean (uninitialized memory zero-inits on replay), and a CPU staging read-back of the dynamic VB shows the **vertex POSITION (offset 0) clean** at draw time. Yet live, the geometry shreds. Signature = **the GPU reads UNDEFINED bytes that are NOT the position-at-offset-0 the probe checks** — bytes that are zero on replay and stale/garbage live.

Leading idea to verify/refute: the D3D11 **input layout / vertex declaration** for the skeletal VS reads a vertex component at an offset/stride the CPU skin path does **not** write — so those bytes are uninitialized in the dynamic slice.

## Trace precisely (file:line; no fixes)
1. The dynamic vertex **format** for `SoftwareBlendSkeletalShaderPrimitive::m_dynamicStream` / `m_systemStream` (`dynamicFormat`): exactly which components it declares (position, normal, color, dot3/tangent/binormal, how many texcoord sets, blend data) and the byte offset + stride of each.
2. What `skinData(...)` actually writes into the stream each frame, and what `fillConstantVertexBufferData(...)` writes once. **Compare against #1** — is every declared component written, or is any component (e.g. a tangent/binormal/dot3 set, a 2nd texcoord, a normal) left UNWRITTEN in the dynamic slice?
3. The D3D11 **input layout** built for the active skeletal vertex shader (in the Direct3d11 plugin): does it bind/read components at offsets that #2 leaves unwritten? Does the input-layout stride match the lock stride the engine writes? Any element with a different `AlignedByteOffset` than where the engine writes it?
4. Does the VS actually CONSUME any unwritten component in computing `SV_Position` (directly or via a multiply)? A normal/tangent normally doesn't move position — but check for any skinning-weight / position-delta / morph term that would.

Deliverable: a byte-map of the dynamic vertex (offset | component | written-by-engine? | read-by-input-layout?) and a clear yes/no on "the VS reads vertex bytes the skin never wrote." If yes, name the component + offset — that's the undefined memory.
