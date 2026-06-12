# CONSULT-25 — D3D11 soft-skin "texture spikes": intra-frame CPU-write/GPU-read hazard on a shared dynamic VB ring

You are one of four independent reviewers (Codex, Cursor, a fresh Opus agent, a fresh Sonnet agent). Give your BEST independent analysis. Prior 4-way consults on this exact bug CONVERGED ON WRONG ANSWERS TWICE (rebind, read-past). Consensus has been the failure mode. Trust the measurements below over any prior hypothesis. **We want the correct minimal FIX, with reasoning, for the hazard characterized below — and we want you to challenge the characterization if the evidence supports a different one.**

## Engine / setup
- Star Wars Galaxies client, in-house **D3D11 renderer plugin (`gl11`)** reimplementing the legacy `Gl_api`. Reference: the legacy **D3D9 plugin (`gl05`)** renders the same content **clean**.
- Bug: close, **soft-skinned** characters (dewbacks, Jawas/robes, multiple creatures at once) render with vertical "texture spikes" — most of a draw's vertices collapse/fan into thin shards. **DISTANCE-GATED** (far = clean, close = spikes), **flickers** mild↔extreme frame-to-frame. D3D9 renders all of it clean.
- Distance gating = the LOD→skinning-mode switch: far→`SM_noSkinning`, medium→`SM_hardSkinning` (clean), **close→`SM_softSkinning`** = CPU-skin into a shared dynamic vertex buffer every frame (`SoftwareBlendSkeletalShaderPrimitive`). The soft-skin dynamic path is the common factor.

## The dynamic VB ring (the suspect resource)
`Direct3d11_DynamicVertexBufferData` — a SINGLE `ID3D11Buffer` (`ms_d3dRingBuffer`, 2 MB, `D3D11_USAGE_DYNAMIC`), shared by ALL dynamic VB users (soft-skin meshes AND `CuiLayerRenderer`'s UI quad batcher). Lock logic:
```cpp
void *Direct3d11_DynamicVertexBufferData::lock(int numberOfVertices, bool forceDiscard) {
    const int length = numberOfVertices * vertexSize;
    D3D11_MAP mapType = D3D11_MAP_WRITE_NO_OVERWRITE;
    if (ms_newFrame || forceDiscard || ms_used + length > ms_size) {
        ++ms_discardsEver;
        mapType = D3D11_MAP_WRITE_DISCARD;   // renames backing
        ms_used = 0;
    }
    context->Map(ms_d3dRingBuffer, 0, mapType, 0, &mapped);
    m_offset = ms_used / vertexSize;         // slice vertex offset -> draw BaseVertexLocation
    sliceStart = (char*)mapped.pData + ms_used;
    ms_used += length;                       // (D3D9 charges this in unlock instead)
}
```
- `CuiLayerRenderer` locks `getNumberOfLockableDynamicVertices()` = the ENTIRE remaining ring each UI batch, so `ms_used` maxes out and the ring `WRITE_DISCARD`s **~370 times per frame** in a busy scene.
- `ms_used`/discard bookkeeping is **CPU-side only** — it does NOT track GPU read completion (no fences).

## The symptom in the buffer (RenderDoc, deterministic / capture-VISIBLE)
RenderDoc capture of a violently-spiking dewback (`export_mesh` of the spike draw, 2631 verts indexed):
- **Verts 0–168: real dewback geometry. Verts 169–2630: ALL one identical stale value.** Exactly ONE clean boundary at 169. No NaN/inf. Triangles fan from the 169 real verts to the one stale point = the shards.
- The VS reads `POSITION` directly from the VB (no GPU skinning), so the garbage `SV_Position` IS the stale VB tail.
- Same signature recurs at every scale: a garment draw was 25 real + 98 stale; a Jawa "missing head" is the subtle version. It is **reproduced in RenderDoc's re-rendered intermediate RT** (deterministic, NOT a capture-invisible-only race).

## What is POSITIVELY ELIMINATED (measured, not argued)
1. **CPU skin** — clean. Engine probe replicates the position blend from source verts + bone transforms; 600 sampled draws, ZERO collapse, no degenerate/out-of-range bones.
2. **The D3D11 VB WRITE** — clean. Probe reads back the just-mapped slice before `Unmap`: distinct verts, `longestRun<=2`. The bytes written are correct.
3. **SBSSP index construction** — in-range. Build-time probe over 400 primitives (dewback/ronto/bib_fortuna/all LODs): EVERY index buffer references exactly `[0, vertexCount)`, `maxIndex == vertexCount-1`, `overRead=0`. The index buffer is NOT out of range.
4. **The skeletal batcher** (`FullGeometrySkeletalAppearanceBatchRenderer`, the combine-many-meshes path) — its `renderBuffers` **never executes** while multiple characters tear. Not involved.
5. **Bind offset** — a prior fix re-bound the primitive's own VB/IB at draw; the bound offset became provably correct; **still collapsed**. Not a stale bind.
6. **Per-frame CPU↔GPU race** — full frame-level serialization (an `ID3D11Query` EVENT spin at `Present`) was a NO-OP; spikes persisted.
7. **Intervening discard-rename between write and deferred draw** — JUST TESTED: moved the `systemStream→ring` copy out of `prepareToDraw` and into `draw()`, immediately before this primitive's own `DrawIndexed` (CPU skin still in prepare, into a persistent per-primitive `SystemVertexBuffer`; only the GPU-ring copy relocated). **Spikes persist.** So a discard renaming the slice in the gap between write and draw is NOT the cause.

## The DECISIVE characterization (this run, fixed build)
The draw-time probe does `CopySubresourceRegion(ring→staging) → Map(READ) → scan` right before each sampled `DrawIndexed` — this **drains the GPU per sampled draw**.
- Sampled garment draws: VB **distinct/correct** (`runStart==numVtx`, no position-collapse) AND, in steady state, transform **correct** (`clipW≈1.4` positive). The only bad frames are a 2–8-call STARTUP transient (view/proj not yet settled), which flips to good and stays good.
- So **for the draws this probe samples, both VB and transform are correct in steady state — yet the spikes are persistent and visible.**
- Therefore: **per-draw GPU drain (this probe's `Map(READ)`) MASKS the corruption. Per-frame Present-sync does NOT. Write-immediately-before-draw does NOT.** And the real (large, unsampled) spike draw shows the collapsed VB in RenderDoc.

⇒ Working conclusion: an **intra-frame, per-draw CPU-write / GPU-read hazard on the single shared `WRITE_DISCARD`/`NO_OVERWRITE` ring** that only a per-draw GPU *drain* hides. Most likely a `WRITE_NO_OVERWRITE` append (or a `WRITE_DISCARD` that the in-house ring doesn't treat as a true driver rename) landing on / aliasing a region the GPU is **still reading** from a prior in-flight deferred draw — because `ms_used`/discard bookkeeping has no GPU-completion tracking. Note the documented INVERSION: **more** discards = **fewer** spikes ("frequent renaming is protective"); reducing discards (D3D9-faithful accounting) made it dramatically WORSE.

## Fixes ALREADY TRIED AND FAILED (do not re-propose verbatim)
1. Per-generation ring (N=4 buffers, store buffer on slice, bind per-slice) — worse/deterministic.
2. Per-frame `beginFrame()` generation flip (true once-per-frame hook) — no change / catastrophic when done per-pass (draws defer across passes).
3. **Uniform per-instance dynamic buffers** (one `ID3D11Buffer` per dynamic VB) — **`nvwgf2um` driver crash on world load** from mass `CreateBuffer`/`Release` churn. (Key constraint: do NOT mass-create per-frame/per-instance D3D11 buffers.)
4. Fence-retired N-buffer (N fixed buffers + `ID3D11Query` EVENT, per-frame flip, whole-frame-per-buffer) and a per-slice-identity advance-on-discard variant — same spikes.
5. D3D9-faithful `ms_used` accounting (charge actual at unlock, no lock-time over-charge) — dramatically WORSE (the inversion).
6. Frame-level Present serialization — no-op.
7. Rebind VB/IB at draw — no-op (offset was already correct).
8. Deferred-write (copy into ring in `draw()` right before `DrawIndexed`) — no-op (THIS consult's new result).

## The QUESTION
Given the above — a single `D3D11_USAGE_DYNAMIC` 2 MB ring, `WRITE_DISCARD` on wrap / `WRITE_NO_OVERWRITE` to append, shared across hundreds of **deferred** soft-skin draws AND CuiLayer's whole-ring UI lock; where a **per-draw GPU drain is the ONLY thing that masks the corruption**; where mass per-instance buffers crash the driver and N-buffer/fence/per-frame-flip/accounting fixes have all failed:

1. **What is the correct, minimal fix?** Be concrete (which buffer/lock/Map semantics, where, how to track GPU completion safely without mass allocation).
2. Do you AGREE the evidence shows an intra-frame CPU-write/GPU-read aliasing hazard? If not, what does it show — and what single measurement would falsify your alternative?
3. Why would a **per-draw GPU drain** mask it but **per-frame sync** and **write-immediately-before-draw** NOT — and what does that asymmetry tell you about the true root cause?
4. Is there a way for D3D9 (same single-shared-ring architecture, `NOOVERWRITE`/`DISCARD`, but with `DrawIndexedPrimitive`'s explicit `MinIndex`/`NumVertices` bound) to be immune to the exact thing biting D3D11 (`DrawIndexed`, no vertex-range bound)? If so, that's a strong lead — explain the mechanism and the fix it implies.

Key files: `Direct3d11_DynamicVertexBufferData.cpp` (lock @~225), `SoftwareBlendSkeletalShaderPrimitive.cpp` (prepareToDraw skin+copy, draw()), `CuiLayerRenderer.cpp` (~62-68 whole-ring lock), `Direct3d11_StateCache.cpp` (drawPartialIndexedTriangleList / IASetVertexBuffers).
