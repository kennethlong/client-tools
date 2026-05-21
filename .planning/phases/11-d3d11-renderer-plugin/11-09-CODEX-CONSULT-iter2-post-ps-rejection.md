# CODEX Consult Round 5 — c9 Fix Surfaces Fragments, But Still No Visible Magenta

**Date:** 2026-05-21
**Predecessors:** Round 1-4 (full diagnostic arc that landed Fix C + c9 viewport-data fix)

---

## Round 4 fix WORKED. Mostly.

Iter-2.7b implemented your c9 fix exactly:

```cpp
// In Direct3d11_StateCache::setViewport, post-RSSetViewports:
float const xOffset = (float(x) * 2.0f) / float(width);
float const yOffset = (float(y) * 2.0f) / float(height);
float const viewportData[4] = {
     2.0f / float(width),
    -2.0f / float(height),
    -1.0f - xOffset,
     1.0f + yOffset
};
constexpr int kVSCR_viewportData = 9;
setVSConstants(kVSCR_viewportData, viewportData, 1);
```

Smoke proves your diagnosis was correct on multiple fronts:

| Probe | Result | Diagnosis |
|---|---|---|
| `setViewport` first-fire probe | Fires 3× during startup; `c9 = (0.0020 -0.0026 -1.0 1.0)` (verbatim D3D9 formula on 1024×768) | Engine IS calling setViewport; our c9 write lands |
| `flushSlot0IfDirty` | Fires every frame (#1 ... #61+ over 60 sec); slot 0 dirty path active | Fix C + dirty-flag wiring functional |
| `VBFormat: isTransformed=1` | UI verts ARE screen-space, exactly as you predicted | Confirms UI path |
| `PSInvocations: 3,924,480` | Was 0 pre-fix | **3.9M fragments are now running through the fallback PS** |

The 10 primitives × ~390K pixels each ≈ 3.9M total — matches expected coverage of full-window UI quads.

## But: still no visible magenta on the login screen

The fallback PS outputs `float4(1.0f, 0.0f, 1.0f, 1.0f)` — solid debug magenta. With 3.9M PSInvocations writing magenta to SV_TARGET, the screen should be dominantly magenta. Instead: dark-blue clear remains visible, UI is invisible (same as before c9 fix).

**The problem moved from pre-PS rejection to post-PS rejection.**

## Current state values (from latest smoke probe)

```
RS desc: CullMode=BACK (3), FrontCCW=FALSE, FillMode=SOLID, DepthClipEnable=1
DSS desc: DepthEnable=1, DepthWriteMask=ALL, DepthFunc=LESS_EQUAL, StencilEnable=0
BS desc: RT0.RenderTargetWriteMask=0x0F (RGBA), RT0.BlendEnable=0, RT0.SrcBlend=ONE, RT0.DestBlend=ZERO
Viewport[0]: TL=(0,0) 1024x768 Z=[0, 1]
Scissor: disabled
```

Pipeline-stats query results:
```
IAVertices:    40    IAPrimitives:  10    VSInvocations: 30
CInvocations:  10    CPrimitives:   10
PSInvocations: 3,924,480   (was 0 pre-c9-fix)
```

Skip counters: NullVS=0, NullLayout=0, totalDrawCallCount=11.

## Candidate failure modes (post-PS rejection)

**A. Wrong RTV bound for UI draws.** Engine binds an offscreen RT for UI rendering that's never composited to the backbuffer. The 3.9M magenta fragments land on a never-presented RT. Plan 11-07's dark-blue clear works because it targets the backbuffer directly via Direct3d11_Device::beginScene.

**B. Late depth test rejection.** Early-Z passes (PSInvocations counts the invocation) but late depth-test rejects after PS. Would require depth buffer in a state that conservatively passes early-Z but explicitly rejects late-Z — typically NaN depth values or some driver-specific corner case. Unlikely but possible.

**C. Engine clears backbuffer AFTER UI draws.** Frame flow has clear-RTV happening AFTER the UI draws, wiping the magenta. Then dark-blue clear presents. Would be unusual order.

**D. OMSetDepthStencilState never called → default DSS in effect (depth enabled LESS, write all). UI fragments at z=0 vs depth buffer cleared to 0 → equal but our DepthFunc is LESS_EQUAL = passes**. So that's fine. But if depth buffer wasn't cleared this frame, contains 0.0 from previous frame → still passes LESS_EQUAL at z=0. So this isn't the issue.

**E. Render target's RTV.WriteMask is somehow off.** D3D11 doesn't have per-RTV write mask separate from BlendState; BS.RenderTargetWriteMask covers it = 0x0F = all enabled. Not the issue.

**F. The OM stage's bound RT count is 0** (no RT bound at all). Possible if our setRenderTarget slot is buggy or if `OMSetRenderTargets(0, nullptr, nullptr)` got called somewhere.

## Verified-NOT-the-issue (eliminated earlier)

- Cull mode (CPrimitives = CInvocations = 10)
- Clip (same)
- Viewport (full 1024×768 sane)
- Scissor (disabled)
- VBFormat (isTransformed=1 as expected for UI)
- WriteMask (0x0F from BS desc)
- Stencil (StencilEnable=0)

## Questions for CODEX

**Q1.** Given PSInvocations=3.9M and no visible magenta, what's the rank-ordered list of likely post-PS rejection mechanisms? My prior: F (wrong/no RTV bound) > A (UI targets offscreen RT) > B (late-Z corner case) > C (backbuffer-cleared-after-UI). Agree/disagree?

**Q2.** What's the cheapest D3D11-idiomatic diagnostic to disambiguate? Options I'm considering:
- (a) `ctx->OMGetRenderTargets(1, &rtv, &dsv)` probe at first fallback bind — captures the actual bound RTV pointer, can compare against Direct3d11_Device's backbuffer RTV pointer
- (b) `D3D11_QUERY_OCCLUSION` around the first 10 draws — counts pixels that survived all OM-stage tests, separates PSInvocations (PS ran) from actual writes
- (c) Force `OMSetBlendState(NULL)` (default blend = opaque write-through, all RTs) just before drawing to rule out blend-stage issues
- (d) Force DSS disable for full one-shot test (depth-off didn't help pre-c9-fix, but PS wasn't running then — now that PS runs, depth-off post-PS would reveal late-Z)

**Q3.** D3D11 has multiple cbuffer rebind paths and `bindVS(0)` in our applyPreDrawState may collide with something. Does the engine's UI path possibly call SetRenderTarget(NULL) or OMSetBlendState with zero write-mask somewhere we haven't found?

**Q4.** In the D3D9 plugin's UI rendering flow, are there per-pass DSS/BS overrides (e.g., depth disabled for UI) that our Direct3d11_ShaderImplementationData::apply() is supposed to apply but doesn't? Plan 11-07 Iter-1 minimum-viable apply() is still a no-op for shader-pass DSS/BS overrides.

**Q5.** What's your strongest hypothesis given the data?

---

## What I'd write for Iter-2.7c

Based on my read: extend the first-fallback probe to capture `OMGetRenderTargets` (RTV + DSV pointers) + Direct3d11_Device's backbuffer RTV pointer for comparison. Plus add a `D3D11_QUERY_OCCLUSION` parallel to the existing pipeline stats query. If RTV != backbuffer → answer A/F. If RTV = backbuffer but occlusion count = 0 → answer B (late-Z) or some other post-OM rejection.

---
