# CONSULT-49 (Cursor) — D3D11 resize: dim-flow / camera projection / assert correctness

Repo: `D:/Code/swg-client-v2`. Read the actual source. Give file:line for every claim. Read-only.

## Context — treat as GIVEN (do NOT re-derive)

On a window resize for the **D3D11 (gl11)** client I will route:
`Os` `WM_SIZE` → `Graphics::displayModeChanged` → (new) `Graphics::resize(newClientW, newClientH)`.
`Graphics::resize` (`Graphics.cpp:520`) sets `ms_currentRenderTargetWidth/Height` and
`ms_frameBufferMaxWidth/Height` to the new size, then calls the renderer `resize`, which recreates the
swapchain backbuffer AND fires the device-restored callbacks so `PostProcessingEffectsManager`
recreates its primary/secondary/tertiary offscreen scene RTs at the new
`getFrameBufferMaxWidth/Height`.

The headline bug being fixed: embedded in a host panel, the 3D scene renders correct-scale but
CROPPED to the upper-left — because today only the swapchain backbuffer resizes; the scene RT +
viewport + projection stay at the original 1600x900.

## Verify each point with file:line evidence (confirm or REFUTE)

1. **Camera self-correct.** `GroundScene.cpp` (~2289-2302) sets BOTH `Graphics::setViewport(...)` and
   `m_cameras[view]->setViewport(0,0,getCurrentRenderTargetWidth,getCurrentRenderTargetHeight)` EVERY
   frame. Does `Camera::setViewport` (`Camera.cpp` ~297) → `Camera::setup` (~104) FULLY recompute the
   projection matrix + aspect / vertical-FOV from the new width/height, so projection + aspect
   self-correct on the first frame after the dims change? Quote the relevant math lines.

2. **The DEBUG_FATAL.** `PostProcessingEffectsManager::preSceneRender` (~185-200) DEBUG_FATALs if
   `ms_primaryBuffer->getWidth() != Graphics::getCurrentRenderTargetWidth()` (and height). After the
   resize (dims updated AND primaryBuffer recreated at new `getFrameBufferMaxWidth`), will this assert
   HOLD on the next frame? Are `getFrameBufferMaxWidth` and `getCurrentRenderTargetWidth` guaranteed
   equal right after `Graphics::resize`? Could the assert trip in any intermediate window (dims
   updated but buffer not yet recreated, or vice-versa)?

3. **Timing.** `Graphics::setRenderTarget` temporarily changes `ms_currentRenderTargetWidth/Height` to
   the bound RT's size during scene rendering. Confirm `WM_SIZE`/`displayModeChanged` is dispatched
   from the Windows message pump BETWEEN frames (find the `Os::update` / PeekMessage/DispatchMessage
   site), NOT mid-scene — so reading `ms_currentRenderTargetWidth` inside `displayModeChanged` sees the
   backbuffer size, not a transient offscreen-RT size. Cite the pump call site.

4. **Safety of an early/late resize.** Could calling `Graphics::resize` at an arbitrary `WM_SIZE`
   (before the first scene/device is fully up, or during teardown) deref a null `ms_api` or null
   device? Note `Graphics::resize` does `NOT_NULL(ms_api->resize)`. Is there a guard that makes an
   early WM_SIZE safe, or do I need one?

Answer crisply, grouped by question. No preamble.
