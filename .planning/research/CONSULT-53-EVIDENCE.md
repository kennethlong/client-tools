# CONSULT-53 — Space HUD "cyan square" — LOCKED EVIDENCE (do not re-derive)

**Symptom (gl11 / D3D11 only):** In space (cockpit + 3rd person), the minimap/radar and the
cockpit reticle gauges are framed by a **semi-transparent cyan/teal rectangle** that should not be
there. Under **gl05 (D3D9)** the same HUD element's frame is **invisible** (correct). Cosmetic only;
all heavy space bugs are fixed. Captures: `stage/Capture47.rdc` (1st-person cockpit),
`stage/Capture48.rdc` (3rd person).

## GROUND TRUTH — measured by RenderDoc, treat as LOCKED axioms

Source: `Capture47.rdc`, draw **EID 8080** (D3D11), RenderDoc PS execution trace at a covered pixel
(800,750) on the cyan reticle. Decoded float values:

1. **Pixel shader PS 2136** (DXBC `ps_4_0`) — full disasm:
   ```
   cb1 = PSAlphaTest (48 bytes, 3 float4 vars); t0 = texture; s0 = sampler
   0: lt  r0.x, l(0.5), alphaTest.x          ; r0.x = (alphaTest.x > 0.5)  -> ALPHA-TEST ENABLE
   1: sample r1, v2.xy, t0, s0                ; r1 = texture sample
   2: mad r0.y, r1.w, v1.w, -alphaTest.y      ; r0.y = texA*vtxA - alphaRef
   3: mul r1, r1, v1                          ; r1 = tex.rgba * vtxColor.rgba
   4: mov o0, r1                              ; OUTPUT = tex * vtxColor
   5: lt  r0.y, r0.y, l(0)                    ; below-ref?
   6: and r0.x, r0.x, r0.y                    ; discard iff (enabled AND below-ref)
   7: discard_nz r0.x
   ```
2. **Alpha-test is OFF at this draw**: `alphaTest.x <= 0.5` → enable bit = 0. Trace confirms the
   pixel is **NOT discarded** (r0.x = 0 at `discard_nz`).
3. **Texture sample** (t0, resource id 3) = RGBA **(0.733, 0.710, 0.733, 0.5725)** — a gray panel
   texel with **alpha ≈ 0.57**. UV sampled = (0.527, 0.496).
4. **Vertex color** v1 = RGBA **(0.263, 0.494, 0.514, 0.40)** — muted teal, **vtx alpha 0.40**.
5. **Output fragment** o0 = RGBA **(0.193, 0.351, 0.377, 0.229)**. Confirmed: o0.a = texA(0.5725) ×
   vtxA(0.40) = **0.229**. RGB = tex.rgb × vtx.rgb (teal).
6. Fragment is **alpha-blended** onto the RT (B8G8R8A8_UNORM 1600×900) → the visible translucent
   teal/cyan square frame. (OM blend factors not yet dumped — assume SrcAlpha/InvSrcAlpha pending
   confirmation.)
7. Pipeline: VS=2134, PS=2136, depth D24S8. The reticle widget batches (CuiLayerRenderer groups by
   shader) the legitimately-visible cyan **ring** AND the bug **frame** under the **same shader** —
   so "fix the frame" must not kill the ring.

## Engine path (verified from source, treat as given)

- All CUI quads go through `CuiLayerRenderer::render(...)` → batched by `s_curShader` →
  `flushRenderQueueQuads()` → `Graphics::setStaticShader(s_curShader->prepareToView())` +
  `Graphics::drawQuadList()`
  (`src/engine/client/library/clientUserInterface/src/shared/core/CuiLayerRenderer.cpp:170,518,530`).
  Blend/alpha-test state comes from the **StaticShader (.sht) pass**, applied per-renderer.
- D3D11 per-pass state apply: `Direct3d11_StaticShaderData.cpp:1660-1764` — mirrors D3D9. Alpha-test
  enable+ref pushed into PS cbuffer slot 1 (`Direct3d11_StateCache::setAlphaTest`,
  StaticShaderData.cpp:1691-1706). Reference resolution claims to mirror
  `Direct3d9_StaticShaderData.cpp:740-760` (TAG_A255/A128/A001/A000 → 255/128/1/0, else
  `getAlphaTestReferenceValue`).
- D3D11 quad draw: `Direct3d11_StateCache::drawQuadList` (quad-list IB, R16_UINT). D3D9 ref:
  `Direct3d9.cpp:4277`.

## THE OPEN QUESTION (split the crew on this)

The D3D11 fragment is a teal texel-panel at 0.229 alpha, alpha-test OFF, alpha-blended → visible.
**Why is the SAME element's frame invisible under gl05 (D3D9)?** RenderDoc CANNOT capture D3D9 —
reason from source + the locked D3D11 ground truth. Three live, non-overlapping hypotheses farmed to
four consultants (see CONSULT-53-*). Convergence-from-divergence is the signal.
