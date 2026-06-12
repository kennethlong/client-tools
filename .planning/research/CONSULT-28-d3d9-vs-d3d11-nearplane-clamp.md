# Research task: does D3D9 clamp/clip degenerate near-plane geometry where D3D11 explodes? (read-only)

## Context (established by GPU-level probes; treat as given)
A skinned-mesh renderer works under the **D3D9** plugin (`gl05`, `src/engine/client/application/Direct3d9`) but produces **radiating vertex "spikes"** under the **D3D11** plugin (`gl11`, `src/engine/client/application/Direct3d11`) for some on-screen characters. The vertex buffer is verified clean/identical at draw time in both. The shading is a vertex-shader WVP transform: `clip = pos_objectLocal × WVP`. Some draws produce vertices with **clip-space W ≈ 0 or negative** (on/behind the near plane). In D3D11 those vertices perspective-divide to ±infinity and shoot across the screen as shards; the same geometry in D3D9 evidently does not.

## The question
Why would identical geometry that lands on/behind the near plane (clip.w ≤ 0) render **bounded/clean in D3D9** but **explode in D3D11**? Specifically:

1. **Near-plane / w≤0 clipping.** How does the D3D9 fixed-function + programmable pipeline clip primitives with clip.w ≤ 0 (near-plane clipping, w-clipping)? How does D3D11 clip them? Is there a documented difference where D3D9 clamps/clips more aggressively and D3D11 relies on guard-band + lets near-w triangles escape?
2. **Render-state / pipeline config divergence in THIS repo.** Compare how `gl05` vs `gl11` configure clipping-relevant state: `D3DRS_CLIPPING`, `D3DRS_CLIPPLANEENABLE`, guard-band, viewport min/max Z, scissor, `D3DPMISCCAPS_CLIPTLVERTS`, any "clip planes" or `D3DRS_LASTPIXEL`. Does `gl05` enable clipping that `gl11` omits? Find the exact setup sites in both plugins.
3. **Guard band.** D3D11 has a fixed guard band; primitives wholly outside the view but inside the guard band are rasterized, those outside are clipped. A near-w vertex projects to a huge coordinate — does it land inside the D3D11 guard band (rasterized as a shard) whereas D3D9 would have clipped at the near plane first? Cite the rasterization-rules / guard-band docs.
4. **Is there a per-vertex w-clamp or near-plane guard in the D3D9 path** (engine or driver) that the D3D11 port dropped?

## Deliverable
A focused answer to "does D3D9 clamp/clip this and D3D11 not", with: (a) the spec-level near-plane/guard-band difference, and (b) the exact file:line in `gl05` vs `gl11` where clipping/guard/viewport state is configured, highlighting any state D3D9 sets that D3D11 does not. Cite MS docs for the spec parts. Do not propose a fix; locate the divergence.
