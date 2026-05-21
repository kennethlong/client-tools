# CODEX Consult Round 2 — Plan 11-09 Iter-2 Pipeline-Stats Diagnosis

**Date:** 2026-05-20 (continuing same session as the Round 1 consult)
**Predecessor:** `11-09-CODEX-CONSULT-iter2-magenta-missing.md` (Round 1)
**Status:** Iter-2.2 pipeline statistics + state probe smoke complete. Data narrowly pinpoints the failure region. Asking for prioritized next-diagnostic recommendation given the narrower evidence.

---

## CODEX Round 1 recap

You ranked suspects 1-5 (WVP / cull / depth / topology / OM) for "Iter-2 PS bound + dark-blue preserved + no magenta visible" and recommended `D3D11_QUERY_PIPELINE_STATISTICS` around the first N fallback draws as the definitive bisector. Decision tree:
- VSInvocations > 0, PSInvocations == 0 → "WVP, clip, cull, depth before PS, topology"
- PSInvocations > 0 → "OM/blend/write-mask/render-target/present path"

## Iter-2.2 probe output (single smoke; ≥60 sec, clean exit)

```
[Iter-2.1 probe] Direct3d11_PixelShaderProgramData::install() entered -- gl11_d.dll has Iter-2 code
[Iter-2.1 probe] compilePixelShaderFromHlsl returned: true
[Iter-2.1 probe] ms_fallbackPS pointer: 174B6F84
[Iter-2.2 probe] CreateQuery PIPELINE_STATISTICS: hr=0x00000000, query=1F7E1F24
[Iter-2.1 probe] applyPreDrawState first FALLBACK PS bind: fallback=174B6F84, drawCallCount=0,
                 ms_currentPSData=12E5CE88, ms_currentPSData->getPixelShader()=00000000
  RS desc: CullMode=3, FrontCCW=0, FillMode=3, DepthClipEnable=1
  DSS desc: DepthEnable=1, DepthWriteMask=1, DepthFunc=4, StencilEnable=0
  BS desc: RT0.RenderTargetWriteMask=0x0F, RT0.BlendEnable=0, RT0.SrcBlend=2, RT0.DestBlend=1
[Iter-2.2 probe] PIPELINE_STATISTICS Begin (before draw 1)
[Iter-2.2 probe] PIPELINE_STATISTICS End (after 10 counted draws)
[Iter-2.2 probe] PIPELINE_STATISTICS results (10 draws bracketed):
  IAVertices:    40
  IAPrimitives:  10
  VSInvocations: 30
  GSInvocations: 0
  GSPrimitives:  0
  CInvocations:  10   (sent to rasterizer)
  CPrimitives:   10   (rendered = post-clip + post-cull)
  PSInvocations: 0
  HSInvocations: 0
  DSInvocations: 0
  CSInvocations: 0
  Skip counters at query close: NullVS=0, NullLayout=0, totalDrawCallCount=12
```

## Decoded state values

| Field | Decoded |
|---|---|
| RS.CullMode=3 | `D3D11_CULL_BACK` |
| RS.FrontCCW=0 | FALSE (CW is front-facing → BACK cull rejects CCW) |
| RS.FillMode=3 | `D3D11_FILL_SOLID` |
| RS.DepthClipEnable=1 | TRUE |
| DSS.DepthEnable=1 | TRUE |
| DSS.DepthWriteMask=1 | `D3D11_DEPTH_WRITE_MASK_ALL` |
| DSS.DepthFunc=4 | `D3D11_COMPARISON_LESS_EQUAL` |
| DSS.StencilEnable=0 | FALSE |
| BS.WriteMask=0x0F | RGBA all channels |
| BS.BlendEnable=0 | opaque (no blending) |
| BS.SrcBlend=2 / DestBlend=1 | `ONE` * src + `ZERO` * dst = pure overwrite |

State values look canonical. No obvious anomaly.

## What the pipeline-stats numbers tell us

**Narrowing chain:**
- `VSInvocations: 30` > 0 → VS executed. Eliminates "VS doesn't run."
- `CInvocations: 10` > 0 → primitives reached rasterizer. Eliminates "VS output is invalid clip-space."
- `CPrimitives: 10` > 0, **equal to CInvocations** → **ALL primitives passed clip + cull**. Eliminates clip-space rejection AND back-face culling.
- `PSInvocations: 0` → rasterizer produced **zero fragments** despite 10 valid primitives reaching it.

**Remaining viable causes (from your Round 1 list):**
1. **Early-Z depth rejection** — depth test is enabled (DSS.DepthEnable=1, LESS_EQUAL), and you flagged in Round 1 that depth clears are conditional in `Direct3d11_Device.cpp:443`. If the depth buffer isn't cleared (or is cleared to 0.0), `LESS_EQUAL` rejects all incoming fragments at z≈0..1; modern GPUs early-Z this BEFORE PS, so PSInvocations stays 0 while CPrimitives counts the primitives.
2. **WVP collapse to degenerate / sub-pixel screen-space triangles** — primitives pass clip + cull (their clip-space SV_POSITION is in range) but viewport-transformed coords collapse to zero pixel coverage. Plan 11-08's matrix composition was never validated by visible geometry.
3. **Topology mismatch** — you flagged `drawQuadList` is TODO and triangle-fan-as-list-without-expansion. **The IAVertices=40 / IAPrimitives=10 / VSInvocations=30 ratio is suspicious**: 4 verts per IAPrimitive matches the signature of "quad list (4 verts/quad) submitted as TRIANGLELIST with only the first 3 verts producing a triangle, 4th vert dropped." If true, the engine is calling drawQuadList on UI text glyphs / sprites; each quad produces 1 valid sub-triangle (verts 0,1,2) and loses the 2nd triangle (verts 0,2,3). The remaining triangle might be degenerate depending on quad vertex order.

**Ruled out by data:**
- Cull mode (CPrimitives > 0)
- Clip rejection (CPrimitives = CInvocations, none clip-rejected)
- VS execution / IL signature (VSInvocations > 0)
- Compile failure (Iter-2.1 probe data: ms_fallbackPS valid pointer)
- Bind failure (probe shows fallback bound on first draw)
- PS signature linkage (debug layer ERROR count = 0 across session)

## Tighter question for Round 2

> Given **CPrimitives = 10 (all 10 primitives passed clip + cull) but PSInvocations = 0 (zero fragments)**, what's the prioritized next diagnostic?
>
> **Sub-questions:**
>
> **A. Early-Z depth rejection.** Is the standard test "force `DepthFunc = D3D11_COMPARISON_ALWAYS` for one smoke and see if magenta surfaces" the right cheapest test here, or is there a more decisive D3D11-idiomatic check? Could `OMSetDepthStencilState(NULL, 0)` (NULL DSS → device-default = depth disabled) be cleaner than rewriting our DSS desc?
>
> **B. Degenerate / sub-pixel screen-space triangles.** Is there a D3D11 query / counter that would tell us if primitives are passing rasterizer-stage primitive-setup but failing pixel-coverage-test? I don't see one in `D3D11_QUERY_DATA_PIPELINE_STATISTICS` beyond what we have. The cleanest test I see: dump the actual clip-space SV_POSITION output of the first few triangles via stream-out (D3D11 stream-output stage). Heavy, but definitive.
>
> **C. Topology / drawQuadList mismatch.** The IAVertices=40 / IAPrimitives=10 / 4-verts-per-primitive ratio strongly suggests quad-list-as-triangle-list (drawQuadList TODO). What's the canonical D3D11 idiom for drawQuadList → split-into-2-triangles? Expand-to-indexed-tris on the CPU side at draw time, OR maintain a parallel index buffer with quad-to-tri index mapping (0,1,2, 0,2,3, ...)? D3D9 had QUADLIST natively; D3D11 doesn't.
>
> **D. The 4-verts / 1-prim ratio + VSInvocations=30 oddity.** With 10 IA primitives and VSInvocations=30, are we losing a vertex per "quad" to VS-side cache hit, or is D3D11 actually dropping the 4th vertex when running TRIANGLELIST on a 4-vert input batch? Trying to verify topology interpretation.
>
> **E. What single iteration is highest-bang-for-buck next?** My read: Iter-2.3 = force `DepthFunc=ALWAYS` (1 line, deterministic; if magenta surfaces, depth bug; if not, escalate to Iter-2.4 = topology fix for drawQuadList). Alternative: Iter-2.3 = wire drawQuadList properly (heavier but addresses the most suspicious ratio). Which is your call?

## Files cited in Round 1 (for re-reference)

| File | Line(s) | Function |
|---|---|---|
| `Direct3d11_StateCache.cpp` | 690 | RS desc setup (cull mapping) — confirmed sane in probe |
| `Direct3d11_StateCache.cpp` | 224 | composeAndUploadSlot0 (WVP cbuffer upload) |
| `Direct3d11_StateCache.cpp` | 279 | DSS default LESS_EQUAL enable |
| `Direct3d11_StateCache.cpp` | 263 | BS default write-mask = all |
| `Direct3d11_Device.cpp` | 443 | conditional depth clears |
| `Direct3d11_Device.cpp` | 402 | beginScene RTV + viewport rebind |

## What I'm doing while you consult

Holding all code edits until you respond. The data is unambiguous enough that one more iteration probably wraps the diagnosis; I want to point the iteration at the right suspect.

---
