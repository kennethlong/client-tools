# CODEX Consult Round 6 — Architectural Fix for clearViewport-after-draw + Half-Quad Mystery

**Date:** 2026-05-21
**Predecessors:** Rounds 1-5 (full diagnostic arc that led to visible magenta on screen)

---

## We have magenta. Plan 11-09 EXIT MILESTONE essentially reached.

After your Round 5 prescription:

| Iter-2.7c | OMGetRenderTargets probe at first fallback bind | Proved boundRTV == backbufferRTV (SAME=1). RTV mismatch eliminated. |
| Iter-2.7d | Event-order probes on beginScene / clearViewport / endScene / applyPreDrawState | **PROVED: engine pattern is `beginScene → applyPreDrawState → clearViewport → endScene → Present` — clearViewport fires AFTER UI draws in same frame.** |
| Iter-2.7e | EXPERIMENTAL disable of color-clear branch in clearViewport | **Magenta visible on screen.** Diagnosis confirmed. |

Kenny's visible-state report: *"one giant magenta on half the window and black on the other half"*. Magenta = the working triangle from each UI quad. Black = uninitialized backbuffer (color-clear is disabled, so the never-drawn half stays at default). Diagnosis is fully consistent.

## The two remaining issues

### Issue 1: clearViewport architecture (this consult's primary)

`Direct3d11_Device::clearViewport` (lines 441-463) does the wrong thing twice:
- Always clears `ms_backBufferRTV` directly, ignoring the currently-bound RTV (CODEX Round 5 flagged)
- Hardcodes dark-blue (0,0,0.25,1), ignoring the engine's `colorValue` (CODEX Round 5 flagged)

But the FUNDAMENTAL bug is that the engine's frame order pushes clearViewport AFTER the UI render within the same frame — a D3D9-era pattern where Present buffered/composed. Under D3D11 flip-model, ClearRenderTargetView immediately wipes the to-be-presented surface.

**My proposed fix architecture (Iter-2.7f):** defer color/depth/stencil clears from clearViewport to the NEXT beginScene.

```cpp
// namespace-scope (Direct3d11_Device.cpp):
struct PendingClear {
    bool colorRequested   = false;   uint32 colorValue   = 0;
    bool depthRequested   = false;   real   depthValue   = 1.0f;
    bool stencilRequested = false;   uint32 stencilValue = 0;
};
PendingClear s_pendingClear;

// In clearViewport: just stash the request, don't call ClearRenderTargetView/DSV yet.
void Direct3d11_Device::clearViewport(...) {
    if (clearColor)   { s_pendingClear.colorRequested   = true; s_pendingClear.colorValue   = colorValue;   }
    if (clearDepth)   { s_pendingClear.depthRequested   = true; s_pendingClear.depthValue   = depthValue;   }
    if (clearStencil) { s_pendingClear.stencilRequested = true; s_pendingClear.stencilValue = stencilValue; }
}

// In beginScene: rebind RTV/DSV (existing), then apply pending clears, then reset state.
void Direct3d11_Device::beginScene() {
    ID3D11RenderTargetView * rtvs[1] = { ms_backBufferRTV.Get() };
    ms_context->OMSetRenderTargets(1, rtvs, ms_depthStencilDSV.Get());
    // ... viewport rebind ...

    if (s_pendingClear.colorRequested && ms_backBufferRTV) {
        // ARGB -> float4 conversion (high-byte alpha, low-byte blue)
        float const a = ((s_pendingClear.colorValue >> 24) & 0xFF) / 255.0f;
        float const r = ((s_pendingClear.colorValue >> 16) & 0xFF) / 255.0f;
        float const g = ((s_pendingClear.colorValue >>  8) & 0xFF) / 255.0f;
        float const b = ((s_pendingClear.colorValue >>  0) & 0xFF) / 255.0f;
        float const c[4] = { r, g, b, a };
        ms_context->ClearRenderTargetView(ms_backBufferRTV.Get(), c);
    }
    UINT clearFlags = 0;
    if (s_pendingClear.depthRequested)   clearFlags |= D3D11_CLEAR_DEPTH;
    if (s_pendingClear.stencilRequested) clearFlags |= D3D11_CLEAR_STENCIL;
    if (clearFlags && ms_depthStencilDSV) {
        ms_context->ClearDepthStencilView(ms_depthStencilDSV.Get(), clearFlags,
            static_cast<FLOAT>(s_pendingClear.depthValue),
            static_cast<UINT8>(s_pendingClear.stencilValue & 0xFF));
    }
    s_pendingClear = {};   // reset for next frame
}
```

### Issue 2: Half-quad rendering (the magenta is half-screen per draw)

Each UI "draw" is non-indexed `Draw(4 verts, 0)` with TRIANGLELIST topology → `floor(4/3) = 1 triangle` (verts 0,1,2; vert 3 dropped). The first triangle covers half the intended quad; the OTHER half (verts 0,2,3) is missing.

Per probe data (Iter-2.4): VB stride=24, vertexCount=4, non-indexed, TRIANGLELIST. Engine is calling `Direct3d11_StateCache::drawTriangleList` directly with 4 verts.

`drawQuadList` is a STUB (returns without drawing) in our impl. If engine called drawQuadList, we'd see 0 draws. So engine is NOT routing through drawQuadList.

**Question: is the engine intentionally submitting 4 verts via drawTriangleList expecting 2 triangles?** That would imply a D3D9 quad-emulation pattern: engine builds a 4-vert VB, calls "drawTriangleList" but expects implicit quad expansion. D3D9 didn't have native QUADLIST in SM2+, but had legacy paths. Maybe the SOE engine has a `Graphics::drawQuadList` that builds a 4-vert VB + calls our drawTriangleList expecting 2 triangles?

## Questions for CODEX

**Q1.** Is the deferred-clear architecture (above) the right fix for clearViewport? Any D3D11 gotchas — e.g., bind-state lifetime around the deferred clear, multiple clearViewport calls per frame, clearViewport during a render-target switch?

**Q2.** Per Round 5 you flagged that clearViewport should target the BOUND RTV, not unconditionally ms_backBufferRTV. With the deferred-clear architecture, the clear lands in beginScene which always targets ms_backBufferRTV. Should the pending clear capture the BOUND-RTV-AT-CLEARVIEWPORT-TIME and apply to that target in beginScene? Or is "clear backbuffer at frame start" semantically correct for the engine's intent?

**Q3.** ARGB → float4 conversion: D3D9's clear uses ARGB packed in a DWORD as `(a<<24) | (r<<16) | (g<<8) | b`. My conversion above assumes that. Verify.

**Q4.** Engine pattern for the half-quad submission. The engine calls drawTriangleList with `ms_currentVBVertexCount = 4` (non-indexed). For 1 triangle this is correct. For a 2-triangle quad, engine should call drawTriangleStrip (4 verts → 2 tris natively) OR submit 6 verts via drawTriangleList. Why 4-vert-drawTriangleList? Is the SOE engine submitting via `Graphics::drawTriangleFan(4 verts)` which we route to TRIANGLELIST (also 1 triangle)? Or `Graphics::drawTriangleStrip(4 verts)` which we'd correctly route to TRIANGLESTRIP (2 tris)?

Looking at our impl quickly:
- `drawTriangleList`: `Draw(ms_currentVBVertexCount, 0)` on TRIANGLELIST. 4 verts → 1 tri.
- `drawTriangleStrip`: `Draw(ms_currentVBVertexCount, 0)` on TRIANGLESTRIP. 4 verts → 2 tris. **THIS WOULD BE CORRECT.**
- `drawTriangleFan`: `Draw(ms_currentVBVertexCount, 0)` on TRIANGLELIST (we don't have native FAN). 4 verts → 1 tri.

The probe shows topology=4 = TRIANGLELIST, so engine is hitting drawTriangleList (or drawTriangleFan). NOT drawTriangleStrip.

**Q5.** Plan 11-09 close criteria: Plan 11-09's exit milestone is "visible geometry on top of the clear in at least one scene." We HAVE that (half-magenta quads). Does Plan 11-09 close here, with the half-quad as a Plan 11-10 starting point? Or should Plan 11-09 also fix the half-quad (your Round 2 prescription was reusable quad index buffer)?

**Q6.** With Iter-2.7e disabling color-clear, the "black" half of the screen is uninitialized backbuffer content. After implementing the deferred-clear fix, engine asks for `colorValue=0x00000000` → ARGB(0,0,0,0) → clear to (0,0,0,0). The black-half wouldn't change visually. **Is the proper visible state after Iter-2.7f "half magenta on black" (engine's intended clear color is black)? Or am I misreading the engine's intent?** The Plan 11-03 hardcoded dark-blue was a placeholder; the engine's true intent appears to be black.

## What I'm doing while you consult

Holding implementation. The current uncommitted working tree has:
- Iter-2 base (fallback PS install + applyPreDrawState bind change)
- Iter-2.1 install/compile/bind first-fire probes
- Iter-2.2 pipeline statistics query
- Iter-2.4 state probe (viewport/scissor/IL/topology/VBFormat)
- Iter-2.5 matrix dump probe
- Iter-2.6 setter first-fire probes (revealed engine doesn't call our matrix setters)
- Iter-2.7a Fix C (namespace shadow + dirty flag + lazy flush + setVSConstants)
- Iter-2.7b c9 viewport-data fix (THE breakthrough)
- Iter-2.7c OMGetRenderTargets probe
- Iter-2.7d event-order probes
- Iter-2.7e color-clear DIAGNOSTIC DISABLE (Magenta visible)

Plan: implement Iter-2.7f per your guidance, strip all Iter-2.X probes per your Round 3 Q7, commit Plan 11-09 close.

---
