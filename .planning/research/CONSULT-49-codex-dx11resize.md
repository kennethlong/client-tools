# CONSULT-49 (Codex) — D3D11 resize: device-lost/restored fan-out safety + missed screen-sized RTs

Repo: `D:/Code/swg-client-v2`. Read the actual source. Give file:line for every claim.

## Facts — treat as GIVEN (do NOT re-derive)

- The D3D11 renderer plugin (`src/engine/client/application/Direct3d11/`) currently has
  `STUB(resize)` at `Direct3d11.cpp:1026` — its `Gl_api` `resize` slot is a fatal stub
  (`scaffold_fatal_stub`).
- The D3D9 renderer implements `Direct3d9Namespace::resize(int,int)` (`Direct3d9.cpp:2141`) as:
  `lostDevice()` → `ms_device->Reset(newBackbufferSize)` → `restoreDevice()`. `lostDevice` /
  `restoreDevice` fire the registered device-lost / device-restored callbacks.
- `Graphics::resize(w,h)` (`Graphics.cpp:520`) sets `ms_currentRenderTargetWidth/Height` and
  `ms_frameBufferMaxWidth/Height` to `w,h`, then calls `ms_api->resize(w,h)`.
- The ONLY callbacks registered through `Graphics::addDeviceLostCallback` /
  `addDeviceRestoredCallback` (grep-confirmed) are THREE, all engine-side / renderer-agnostic:
  `PostProcessingEffectsManager` (`deviceLost`/`deviceRestored`), `Bloom`, `BinkVideo`.

## The proposed change — verify or REFUTE it (do not assume it is correct)

Implement D3D11 `resize(w,h)` to: (1) fire the registered **device-lost** callbacks; (2) resize the
**swapchain backbuffer** via DXGI `ResizeBuffers` + recreate the backbuffer RTV/DSV — **the D3D11
`ID3D11Device`/`ID3D11DeviceContext` pointers DO NOT change; this is NOT a true device loss**, only
the swapchain backbuffer + any screen-sized offscreen textures need re-fetching at the new
`Graphics::getFrameBufferMaxWidth/Height`; (3) fire the registered **device-restored** callbacks.

## Questions

1. For EACH of the 3 registered callbacks (PostProcessingEffectsManager, Bloom, BinkVideo): trace its
   `deviceLost` and `deviceRestored` bodies (file:line). Report whether firing **lost-then-restored
   while the D3D11 device/context are UNCHANGED** is SAFE and CORRECT, or whether any of them:
   (a) assumes the device pointer was recreated; (b) would double-free / leave a dangling pointer;
   (c) would FAIL to pick up the new size; (d) has any ordering hazard if fired outside a real
   device-loss event. One verdict per callback.

2. Enumerate any OTHER screen-resolution-sized render targets or depth buffers in the **gl11 client
   build** that are created OUTSIDE those 3 callbacks and would therefore stay at the OLD size after
   this change (i.e. silently not resized). Candidates to check: water/planar-reflection RTs, shadow
   / depth maps, `ShaderPrimitiveSorter`, `DynamicRenderTarget`, screenshot / texture-renderer baked
   RTs, any `Graphics::createRenderTarget`/`TextureList::fetch(TCF_renderTarget,...)` call sized from
   `getFrameBufferMax*`/`getCurrentRenderTarget*`. file:line for each + whether it is recreated on
   resize through any path.

3. Does the D3D11 plugin register any of its OWN internal resources through the same
   `addDeviceLostCallback`/`addDeviceRestoredCallback` path (beyond the 3 engine-side ones), such that
   firing them on a resize matters? file:line.

Answer crisply, grouped by question. No preamble.
