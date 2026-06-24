# gl11 embed-resize SCALING — answers + fix (UI re-layout + poll-resize + flushed trace)

**Date:** 2026-06-23
**Status:** HANDBACK — fix staged; awaiting maintainer injected smoke.
**Audience:** maintainer + Utinni dev. Answers the questions in `2026-06-22-utinni-dx11-advertised-client-gap.md` (572cb480f).
**Source of truth:** this repo. No write to `D:/Code/Utinni`. No contract change.

## Answers to your two questions

**(a) Where do the `displayModeChanged(gl11) … DEFERRED` WARNINGs log, and are they firing?**
The `WARNING()` macro is NOT debug-gated (`Fatal.h:47` `#define WARNING(a,b) ((a)?Warning b:NOP)`), but
`Warning()` routes through the engine REPORT system — `REPORT(RF_log | RF_print | RF_warning)`
(`Fatal.cpp:218`): `RF_log` → the buffered `TailFileLogObserver` log, `RF_print` → `OutputDebugString`.
**That log is not flushed per line; on a clean (non-crashing) run it isn't on disk while the client
runs** — which is exactly why "the WARNINGs never appeared in any stage log." Their absence does NOT
mean the path didn't fire. **Fix: I added a crash-/buffer-proof FLUSHED trace** —
`swgclient-resize-trace.log` (cwd = staging dir), one immediately-flushed line per event:
```
[gl11-resize] displayModeChanged CALLED rM/fpd   11 x 1     (rasterMajor, firstPresentDone)
[gl11-resize] REQUEST (apply next frame)        735 x 460
[gl11-resize] APPLY (Graphics::resize)          735 x 460
[gl11-resize] Graphics::resize ENTER (dims+RTs+proj) 735 x 460
[gl11-resize] POLL window != backbuffer         1455 x 1040
```
This conclusively shows, next smoke, whether `displayModeChanged` fires, whether the deferred apply
runs, whether the poll fallback fires, and the sizes.

**(b) Does `Graphics::resize` (scene RTs + projection) run, not just `ResizeBuffers`?**
**Yes.** `Direct3d11_Device::resizeBackBuffer` is the ONLY place our code calls `IDXGISwapChain::ResizeBuffers`,
and it `Reset()`s our back-buffer RTV first. Under FLIP_DISCARD, `ResizeBuffers` fails while any
back-buffer reference is alive — so Utinni cannot call it while the client holds its RTV. Therefore the
`hkResizeBuffers(735×460)` your DIAG sees **is our path** (`displayModeChanged → deferred apply →
Graphics::resize → resize_impl → resizeBackBuffer → ResizeBuffers`, with the one-frame lag you noted).
`Graphics::resize` updates the dim globals AND fires the device-restored fan-out (PostProc/Bloom RTs
recreated at the new size). The flushed trace's `Graphics::resize ENTER` line will confirm it live.

## The real gap (Codex-confirmed, CONSULT-51): the UI root page never re-lays-out

`Graphics::resize` correctly updates the screen dims + recreates the device-restored RTs, and
`GroundScene`'s viewport/camera re-read the live dims every frame — there is **no fixed-size 3D scene
RT** missed by resize (Codex checked every `TCF_renderTarget`: only PostProc + Bloom, both on the
device-restored path). **But the UI root page is sized once at install (`CuiManager.cpp:448`) and
NOTHING in the game client re-lays it out on resize** — `Graphics::resize` (clientGraphics) can't call
up into the UI layer, and only the Qt editor widgets ever called `CuiManager::setSize`. So the login
screen + in-game HUD stay at the install-time size → "doesn't scale on startup/maximize."

## Fix (2 self-detecting, provider-side, no consumer change, no layering violation)

1. **`CuiManager::render()`** — re-layout the UI when the engine render size changes: compare
   `Graphics::getCurrentRenderTargetWidth/Height()` to the last applied size; on change call
   `CuiManager::setSize()` (root `SetSize` + `ForcePackChildren` + reset dead zone). No-op when
   unchanged (normal play / standalone). This reflows login + HUD on resize.
2. **`Graphics::beginScene()` poll fallback** — if the SWG window client rect ≠ the engine back-buffer
   size and no WM_SIZE-driven resize was queued, drive `Graphics::resize` from here. Under injection
   the embed `WM_SIZE` may never reach the client's WndProc (so `displayModeChanged` wouldn't fire);
   this poll makes the scene resize fire **regardless of message delivery**. Post-first-present +
   size-changed + gl11 only → no-op in normal play and standalone.

Plus the guard fix: the deferred-apply now compares `ms_frameBufferMaxWidth/Height` (stable back-buffer
dims) instead of `ms_currentRenderTargetWidth/Height` (which fluctuates within a frame as
`setRenderTarget` switches to offscreen RTs and could spuriously skip a resize).

## What to verify (maintainer — live inject)

- **Login + in-game HUD rescale** on startup embed + on maximize (the UI gap fix).
- **3D world fills the panel** (no top-left crop). With `Graphics::resize` confirmed running, the world
  should already track; if it still crops, send `swgclient-resize-trace.log` + a RenderDoc post-resize
  frame (compare bound viewport + scene RT vs back-buffer).
- **Read `stage\swgclient-resize-trace.log`** — it now shows definitively whether the event path
  (`displayModeChanged`/`REQUEST`/`APPLY`) or the `POLL` fallback drives each resize, and the sizes.
- Standalone gl11 + SWGEmu/D3D9 unchanged.

## Build

- Release/Win32, `/t:Direct3d11;SwgClient`, `/nodeReuse:false`: 0 `unresolved external symbol`, Build
  succeeded; `SwgClient_r.exe` restaged with matching PDB. gl11_r.dll unchanged this round (no gl11
  source change). Files: `Graphics.cpp` (clientGraphics), `CuiManager.cpp` (clientUserInterface) —
  exe-side; no Gl_api/shared-header change.
- Cross-checked by Codex (CONSULT-51): UI root-page is the confirmed stale consumer; no fixed-size 3D
  scene RT.
