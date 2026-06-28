# CONSULT-51 (Codex) — Graphics::resize runs but the 3D scene + UI don't rescale to the new size

Repo `D:/Code/swg-client-v2`. Read actual source. file:line for every claim. Read-only.

## Established facts (treat as GIVEN — do NOT re-derive)

- The gl11 (D3D11) client, embedded + resized, now resizes correctly at the swapchain level:
  `Graphics::resize(w,h)` (Graphics.cpp:520) RUNS on each embed resize. It sets
  `ms_currentRenderTargetWidth/Height`, `ms_currentRenderTargetMaxWidth/Height`,
  `ms_frameBufferMaxWidth/Height`, `ms_viewportWidth/Height` = w,h, then calls `ms_api->resize` which
  (gl11) fires the device-lost + device-restored callbacks and `ResizeBuffers` the backbuffer to w,h.
- The 3 device-restored callbacks (`PostProcessingEffectsManager`, `Bloom`, `BinkVideo`) recreate their
  render targets from `Graphics::getFrameBufferMaxWidth/Height()` — at the NEW size.
- Backbuffer + RTV/DSV are recreated at w,h.

## Symptom (live, under injection)

After an embed resize (e.g. 1600x900 -> 735x460 -> maximize 1455x1040), the backbuffer tracks the
window, BUT the **3D world scene renders cropped to the upper-left** (scene content does NOT rescale to
the new size), and the **login / character-select UI does not rescale** either. So `Graphics::resize`
updates the dimension globals, but something downstream renders at the OLD size.

## Questions (find the stale consumer; file:line each)

1. **3D world scene render target:** Does the world render into `PostProcessingEffectsManager`'s
   primary buffer (recreated by the fan-out) or into a DIFFERENT render target? Search `RenderWorld`,
   `Graphics`/`Gl_api` "screen"/"scene"/"primary" RT, `DynamicRenderTarget`, any
   `TextureList::fetch(TCF_renderTarget, ...)` or `Graphics::createRenderTarget(...)` sized from screen
   dims AT INSTALL TIME and NOT recreated on resize / device-restored. Is there a scene RT whose size
   is captured once and never updated?

2. **Viewport:** Where is the 3D scene viewport set each frame, and does it read the LIVE
   `Graphics::getCurrentRenderTargetWidth/Height()` (so it tracks resize) — or a cached size? Check
   `GroundScene` (~2289), `Camera::setViewport`/`setup`, and the gl11 plugin's per-frame
   `setViewport(0,0,ms_width,ms_height)` (Direct3d11_Device beginScene).

3. **UI layout:** Does the UI re-layout when the screen dims change? Trace `CuiManager` / `CuiWorkspace`
   / `CuiIoWin` / `UICanvasGenerator` / the root `UIPage` size. Is the UI canvas sized ONCE at install
   from screen dims (so it never reflows on resize), or is there a resize/`setSize` notification driven
   by `Graphics::resize` or a per-frame dim read? Is there any existing "resolution changed" path
   (Options menu / `CutScene::resize(640,480)`) that re-lays-out the UI that the embed resize is NOT
   triggering?

4. **The gap:** Given `Graphics::resize` updates the globals + recreates the device-restored RTs, name
   the SPECIFIC downstream artifact(s) that remain at the old size (a fixed-size scene RT not on the
   device-restored path, and/or a UI canvas sized once at install). That is the fix target.

No preamble. Group by question.
