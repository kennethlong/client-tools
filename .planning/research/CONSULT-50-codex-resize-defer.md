# CONSULT-50 (Codex) — find holes in a deferred-resize fix (gl11 / clientGraphics)

Repo `D:/Code/swg-client-v2`. Read the actual source. file:line for every claim. Read-only.

## Problem being fixed (treat as GIVEN)

Under Utinni injection the SWG window is reparented + `SetWindowPos`-resized during startup, delivering
`WM_SIZE` BEFORE the first rendered frame. A prior change made that `WM_SIZE` (gl11 / `rasterMajor==11`)
run a full `Graphics::resize` → device-lost/restored fan-out before the device/RTs were ready → no
render under injection (standalone was fine).

## The fix just applied (verify it; hunt for holes)

In `src/engine/client/library/clientGraphics/src/win32/Graphics.cpp`:
- `GraphicsNamespace::displayModeChanged()` (gl11 branch) NO LONGER resizes synchronously. It records
  `ms_pendingResizeWidth/Height` + `ms_resizePending=true` and returns.
- `Graphics::beginScene()` applies the pending resize (calls `Graphics::resize`) ONLY when
  `ms_resizePending && ms_firstPresentDone`, then clears the flag.
- `Graphics::present()` and `Graphics::present(HWND,int,int)` set `ms_firstPresentDone = true`.
- New statics in `GraphicsNamespace`: `ms_resizePending`, `ms_pendingResizeWidth/Height`,
  `ms_firstPresentDone`.
- `Direct3d11.cpp` `resize_impl` now early-returns if `!Direct3d11_Device::isInstalled()`.

## Questions (answer crisply, grouped)

1. **Coverage:** Enumerate every caller of `Graphics::resize(int,int)` and every site that invokes the
   gl11 `displayModeChanged` hook (`ms_displayModeChangedHookFunction`). Does any path resize gl11
   synchronously during startup that BYPASSES the new deferral (i.e. reaches `Graphics::resize` or
   `ms_api->resize` without going through the `displayModeChanged` defer)? List each with file:line and
   whether it runs pre- or post-first-present.
2. **Liveness / lost-resize:** Is `Graphics::beginScene()` guaranteed to be called every frame from the
   game loop AFTER `Graphics::present()` sets `ms_firstPresentDone`? Could a pending resize be set but
   never applied (e.g. a frame path that presents but never calls `Graphics::beginScene` again, or
   `present` not reached)? Cite the game-loop order (`Game.cpp`).
3. **Reentrancy:** `Graphics::beginScene` now calls `Graphics::resize` (which calls `ms_api->resize` →
   device-lost/restored + swapchain ResizeBuffers) BEFORE `ms_api->beginScene()`. Is that a safe point
   (no half-open scene, no RT bound that ResizeBuffers would conflict with)? Any reentrancy if a
   callback or resize triggers another WM_SIZE/displayModeChanged?
4. **First-present gate correctness:** Does the install-time clear/present in
   `Direct3d11_Device::create()` go through `Graphics::present()` (which would set `ms_firstPresentDone`
   too early), or the plugin's own present? Confirm `ms_firstPresentDone` only flips on a real
   game-loop present, not the install-time one.
5. Any case where this DEFERS forever (firstPresentDone never set) and thus the embed never resizes?

No preamble.
