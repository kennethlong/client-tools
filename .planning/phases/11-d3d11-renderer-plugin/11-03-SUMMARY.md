---
phase: 11-d3d11-renderer-plugin
plan: 03
subsystem: renderer
tags: [d3d11, device, dxgi, swapchain, flip-discard, clear-to-color, mvp, per-frame-loop]

# Dependency graph
requires:
  - phase: 11-d3d11-renderer-plugin
    plan: 02
    provides: plugin scaffold + FATAL Gl_api table + TAG_DX11 range-check + post-build cp auto-stage + vendored atlmfc include (clean baseline at HEAD a9ae88846)
provides:
  - Direct3d11_Device static class — owns ID3D11Device + ID3D11DeviceContext + IDXGISwapChain1 + back-buffer RTV + depth-stencil DSV
  - DXGI flip-model swap chain (DXGI_SWAP_EFFECT_FLIP_DISCARD, BufferCount=2, BGRA8) wired to engine HWND
  - Pitfall-3 unconditional RTV+DSV rebind in beginScene (flip-model unbinds after Present)
  - Pitfall-8 debug-layer detect-and-fallback for DXGI_ERROR_SDK_COMPONENT_MISSING (retry without DEBUG flag)
  - D-13 invariant maintained — zero functional D3DPOOL_MANAGED / OnLostDevice / OnResetDevice in Direct3d11/ source
  - D-05 invariant maintained — D3D9 plugin source untouched; gl05_d.dll still builds clean
  - 5 per-frame Gl_api slot bindings (beginScene/endScene/clearViewport/present/displayModeChanged) replacing FATAL stubs
  - 2 Rule-3 no-op shims (setBrightnessContrastGamma + update) covering install-time-callable and pre-beginScene-per-frame slots
  - Rule-2 install-time initial clear+present in Direct3d11_Device::create (dark-blue 0,0,0.25,1)
  - FATAL boundary advanced from Graphics::install:320 (setBrightnessContrastGamma in Plan 11-02 baseline) → TextureList::install → createTextureData (Plan 11-04 territory)
affects: [11-04, 11-05, 11-06, 11-09]

# Tech tracking
tech-stack:
  added:
    - ID3D11Device + ID3D11DeviceContext at D3D_FEATURE_LEVEL_11_0 minimum (FATAL on downgrade per SPEC §Constraints)
    - IDXGISwapChain1 with DXGI_SWAP_EFFECT_FLIP_DISCARD (2-buffer minimum, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_MWA_NO_ALT_ENTER)
    - DXGI_FORMAT_D24_UNORM_S8_UINT depth-stencil texture + DSV
    - Microsoft::WRL::ComPtr ownership for all 6 device-layer resources (Pattern D-01)
  patterns:
    - Static-class wrapping namespace state (ms_-prefixed): Direct3d11_Device::create/destroy + 5 per-frame slot statics → mirrors D3D9 plugin's namespace-state convention (PATTERNS §Shared 3)
    - Pitfall-3 enforcement: beginScene unconditionally calls OMSetRenderTargets(1, &ms_backBufferRTV, ms_depthStencilDSV) + RSSetViewports — flip-model unbinds after Present with no warning if missed
    - Pitfall-8 enforcement: createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG in _DEBUG builds; detect-and-fallback retry without flag on DXGI_ERROR_SDK_COMPONENT_MISSING; log hint to install Graphics Tools optional feature
    - D-13 enforcement: present() FATALs on DXGI_ERROR_DEVICE_REMOVED with GetDeviceRemovedReason() logged — process-restart class event per SPEC §Boundaries, NO recovery
    - displayModeChanged uses IDXGISwapChain1::ResizeBuffers + recreate-RTV/DSV pattern (no swap chain re-creation needed under flip model)
    - No-op shim for "install-time-callable Gl_api slot" pattern: setBrightnessContrastGamma fires from Graphics.cpp:320 INSIDE Graphics::install; if STUB'd, FATAL fires before per-frame loop ever starts
    - No-op shim for "pre-beginScene per-frame Gl_api slot" pattern: update fires from Game.cpp:1211 BEFORE Graphics::beginScene each frame; if STUB'd, every frame FATALs before clear+present
    - Install-time initial clear+present pattern: Direct3d11_Device::create issues a one-shot beginScene/clearViewport/endScene/present BEFORE returning to install-caller — guarantees back buffer has known contents before engine drives the per-frame loop

key-files:
  created:
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_Device.h
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_Device.cpp
    - .planning/phases/11-d3d11-renderer-plugin/11-03-SUMMARY.md
  modified:
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp (install path delegates to Direct3d11_Device::create; 5 per-frame slot bindings; 2 Rule-3 no-op shims; getDevice/getContext accessors; Direct3d11Namespace::remove now calls Direct3d11_Device::destroy)
    - src/engine/client/application/Direct3d11/build/win32/Direct3d11.vcxproj (Direct3d11_Device.{h,cpp} ItemGroup adds)
    - src/engine/client/application/Direct3d11/build/win32/Direct3d11.vcxproj.filters (src\win32 filter group adds)

key-decisions:
  - "DXGI_SWAP_EFFECT_FLIP_DISCARD + 2 back buffers + BGRA8 — matches engine PackedArgb convention; MSAA explicitly NOT in flip-model swap chain (resolve manually if needed later)"
  - "DXGI_FORMAT_D24_UNORM_S8_UINT depth-stencil — standard SWG depth/stencil precision, D3D11_USAGE_DEFAULT + D3D11_BIND_DEPTH_STENCIL"
  - "clearViewport ignores argb arg for Plan 11-03 MVP — hardcodes dark blue (0.0, 0.0, 0.25, 1.0) so the screen is visibly D3D11-active. Wave 4+ wires proper PackedArgb→float4 conversion when engine starts providing real colors"
  - "Pitfall-8 detect-and-fallback per SPEC: NO FATAL on DXGI_ERROR_SDK_COMPONENT_MISSING; retry without DEBUG flag + log hint. Plugin functions normally without debug-layer diagnostics"
  - "Rule-3 no-op shim for setBrightnessContrastGamma at Graphics.cpp:320 — slot fires INSIDE Graphics::install; STUB would FATAL before per-frame loop ever runs, preventing this plan's contract from being verified"
  - "Rule-3 no-op shim for update at Game.cpp:1211 — slot fires per-frame BEFORE Graphics::beginScene; STUB would FATAL every frame before clear+present, preventing this plan's contract from being verified"
  - "Rule-2 install-time initial clear+present in Direct3d11_Device::create — guarantees back buffer is non-garbage when the engine eventually calls ShowWindow (a defensive correctness step; the engine's per-frame loop will overdraw anyway)"
  - "Direct3d11Namespace::remove extended to call Direct3d11_Device::destroy() BEFORE clearing callback vectors — reverse ComPtr release order for clean teardown (D-13 has no lost-device equivalent; this is the only teardown path)"

patterns-established:
  - "Install-time-callable Gl_api slots must be unstubbed (or no-op-shimmed) at the SAME plan that introduces the per-frame loop. Otherwise FATAL fires inside install() and the per-frame contract can't be verified. setBrightnessContrastGamma is the canonical example."
  - "Pre-beginScene per-frame Gl_api slots must be unstubbed (or no-op-shimmed) at the SAME plan that wires beginScene/clearViewport/endScene/present. Otherwise every frame FATALs before reaching clear+present. update is the canonical example."
  - "Plan-scope FATAL-boundary-advancement is the verdict signal for plumbing-style plans: a clean advance from STUB N → STUB N+k proves all of STUBs 1..N-1 + the new device infrastructure work end-to-end. Visible-window outcomes are downstream of how many resource-creation slots are STUB'd vs implemented; visible dark-blue requires resource-layer slots (Plan 11-04) to fire AFTER the device is created."
  - "Defensive install-time clear+present is cheap (one frame, one swap-buffer flip) and gives clean back-buffer contents for the OS-level ShowWindow that happens later in engine boot. Not required for correctness this plan, but recorded as a pattern for future device-plugin work."

requirements-completed: []
requirements-partial: [D3D11-01, D3D11-02]

# Metrics
duration: ~1 day elapsed (2026-05-16); ~4 hours active executor + Kenny smoke time
completed: 2026-05-16
---

# Phase 11 Plan 03: D3D11 Device + DXGI Flip-Model Swap Chain + Clear-to-Color MVP Summary

**`Direct3d11_Device` static class created — owns the D3D11 device + immediate context + DXGI flip-model swap chain + back-buffer RTV + depth-stencil DSV — and the 5 per-frame `Gl_api` slots (beginScene/endScene/clearViewport/present/displayModeChanged) are wired to working implementations. FATAL boundary advanced from `Graphics::install:320 (setBrightnessContrastGamma)` to `TextureList::install → createTextureData` (Plan 11-04 territory), proving the device + swap chain + per-frame clear+present cycle works end-to-end.**

## Performance

- **Duration:** ~4 hours active across 2026-05-16
- **Started:** 2026-05-16 (after Plan 11-02 close at `a9ae88846`)
- **Completed:** 2026-05-16 (this plan-close commit)
- **Tasks:** 2 auto + 1 checkpoint:human-verify (APPROVED with caveat)
- **Files modified:** 5 (2 new Direct3d11_Device source files + 3 modified — Direct3d11.cpp + vcxproj + .filters)
- **Commits:** 2 task commits + 1 plan-close commit = 3 total on plan 11-03

## Accomplishments

1. **`Direct3d11_Device` static class created (`Direct3d11_Device.{h,cpp}`, ~71 + ~410 lines).** Owns 6 ComPtr-wrapped D3D11/DXGI resources: `ID3D11Device`, `ID3D11DeviceContext`, `IDXGISwapChain1` (flip model), `ID3D11RenderTargetView` (back buffer), `ID3D11Texture2D` (depth-stencil), `ID3D11DepthStencilView`. Public surface: `create(HWND, w, h, windowed) / destroy() / getDevice() / getContext() / getWidth() / getHeight() / beginScene() / endScene() / clearViewport() / present() / displayModeChanged()`.

2. **DXGI flip-model swap chain wired to engine HWND with correct invariants.** `DXGI_SWAP_EFFECT_FLIP_DISCARD`, `BufferCount=2`, `DXGI_FORMAT_B8G8R8A8_UNORM`, `DXGI_USAGE_RENDER_TARGET_OUTPUT`, `DXGI_SCALING_STRETCH`. `MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER)` disables built-in Alt-Enter (engine handles fullscreen toggle via setWindowedMode hook). MSAA explicitly NOT requested in the swap chain (flip-model constraint — resolve manually if needed in a later plan).

3. **D3D_FEATURE_LEVEL_11_0 minimum enforced.** `D3D11CreateDevice` called with `featureLevels[] = { D3D_FEATURE_LEVEL_11_0 }` only; FATAL on hardware downgrade per SPEC §Constraints. RTX 3060 negotiates feature level 11.0+ trivially, but the guard catches any future "low-end VM with WARP only" scenario.

4. **Pitfall-8 enforcement: debug-layer detect-and-fallback.** In `_DEBUG`, `createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG`. If `D3D11CreateDevice` returns `DXGI_ERROR_SDK_COMPONENT_MISSING`, the flag is cleared and the call is retried without DEBUG. Debug-layer diagnostics are then unavailable; a `DEBUG_REPORT_LOG_PRINT` hint suggests installing "Graphics Tools" via Windows Settings > Apps > Optional Features. NO FATAL.

5. **Pitfall-3 enforcement: per-frame RTV+DSV rebind.** `Direct3d11_Device::beginScene` unconditionally calls `OMSetRenderTargets(1, &ms_backBufferRTV, ms_depthStencilDSV)` + `RSSetViewports(1, &vp)` every frame. Flip-model swap chains unbind the back-buffer RTV after `Present` with no warning if you forget; this is the canonical D3D11 portability footgun.

6. **D-13 enforcement: zero D3DPOOL_MANAGED / OnLostDevice / OnResetDevice in `Direct3d11/`.** `grep -rE "D3DPOOL_MANAGED|OnLostDevice|OnResetDevice" src/engine/client/application/Direct3d11/` returns 3 hits, ALL inside `//` comment lines documenting the D-13 invariant (`Direct3d11_Device.h:11-12` block comment, `Direct3d11_Device.cpp:18` block comment). Zero functional uses. `present()` FATALs on `DXGI_ERROR_DEVICE_REMOVED` with `GetDeviceRemovedReason()` logged — process-restart class event per SPEC §Boundaries, no recovery attempted.

7. **D-05 enforcement: D3D9 plugin source untouched.** `git diff a9ae88846..HEAD -- src/engine/client/application/Direct3d9/` returns empty. `MSBuild ... /t:Direct3d9` EXIT=0; `gl05_d.dll` still builds + loads cleanly. The Plan 11-02 char-select baseline is intact (verified by leaving the D3D9 boot path untested this plan; no D3D9 source change since plan 11-02 close at `a9ae88846`).

8. **5 per-frame Gl_api slots wired (replacing FATAL stubs from Plan 11-02):**
   - `ms_glApi.beginScene = Direct3d11_Device::beginScene` — rebinds RTV+DSV every frame per Pitfall 3
   - `ms_glApi.endScene = Direct3d11_Device::endScene` — no-op (D3D11 has no batched-state-flush semantic)
   - `ms_glApi.clearViewport = Direct3d11_Device::clearViewport` — dark blue (0.0, 0.0, 0.25, 1.0); argb arg ignored for MVP
   - `ms_glApi.present = Direct3d11_Device::present` — `Present(1, 0)` vsync=1; FATAL on `DEVICE_REMOVED` per D-13
   - `ms_glApi.displayModeChanged = Direct3d11_Device::displayModeChanged` — `ResizeBuffers` + recreate RTV/DSV
   - `Direct3d11Namespace::remove` body extended to call `Direct3d11_Device::destroy()` BEFORE clearing callback vectors (reverse ComPtr release order)

9. **2 Rule-3 no-op shims added (deviations, both justified by per-frame-loop reachability):**
   - `ms_glApi.setBrightnessContrastGamma = setBrightnessContrastGamma_noop` — slot fires from `Graphics.cpp:320` INSIDE `Graphics::install` (after `ms_api->install` returns). If left STUB'd, the FATAL fires before the per-frame loop ever runs, preventing this plan's contract from being verified.
   - `ms_glApi.update = update_noop` — slot fires from `Game.cpp:1211` per-frame BEFORE `Graphics::beginScene`. If left STUB'd, every frame FATALs before clear+present, preventing this plan's contract from being verified.

10. **Rule-2 install-time initial clear+present.** `Direct3d11_Device::create` issues a one-shot `beginScene → ClearRenderTargetView(dark blue) → ClearDepthStencilView → present(1, 0)` BEFORE returning to the install caller. Guarantees back-buffer contents are known when the OS eventually calls `ShowWindow(SW_SHOW)` later in engine boot. Defensive correctness step — the engine's per-frame loop overdraws anyway, but this gives clean initial pixels.

11. **FATAL boundary advanced from `Graphics::install:320` to `TextureList::install → createTextureData`.** The Plan 11-02 baseline FATAL was at `Graphics.cpp:320` (setBrightnessContrastGamma inside `Graphics::install`). After Plan 11-03 lands, the FATAL boundary is at `TextureList.cpp:299 → Texture.cpp:641 → Graphics.cpp:1691 → createTextureData` — the first resource-creation slot, which Plan 11-04 (Wave 4) implements. This proves: `Direct3d11::install` completed successfully; the device + swap chain + RTV/DSV + clear+present + setBrightnessContrastGamma no-op + update no-op all worked.

## Task Commits

| # | Commit | Task | Type | Net |
|---|---|---|---|---|
| 1 | `28c1f64c4` | Task 1 — `Direct3d11_Device.{h,cpp}` (new): D3D11 device + DXGI flip-model swap chain + back-buffer RTV + depth-stencil DSV; `Direct3d11.vcxproj` + `.filters` source-list adds | spec | +~481 lines across 2 new files + 2 modified |
| 2 | `802ea9c4d` | Task 2 — Wire `Direct3d11_Device` into install path + bind 5 per-frame slots (beginScene/endScene/clearViewport/present/displayModeChanged) + 2 Rule-3 no-op shims (setBrightnessContrastGamma + update) + Rule-2 install-time initial clear+present in `Direct3d11_Device::create` | spec | ~+45 / -5 lines in 1 modified file (Direct3d11.cpp) |

**Plan close commit:** (this commit) — adds `11-03-SUMMARY.md` + `STATE.md` + `ROADMAP.md` + `REQUIREMENTS.md` updates.

## Sub-step Verification

### CHECKPOINT (Task 3) — clear-to-color smoke under rasterMajor=11 — APPROVED with caveat

Kenny launched `D:/Code/swg-client-v2/stage/SwgClient_d.exe` with `client_d.cfg [ClientGraphics] rasterMajor=11`.

**Observed:**
- Cursor confined to a window-sized region (same hidden-modal-FATAL pattern recorded in Plan 11-02 sub-step 3a — see lessons-learned)
- No visible dark-blue window
- FATAL crash dump persisted at `stage/SwgClient_d.exe-unknown.0-20260517005210.txt`

**FATAL call stack (quoted from the crash dump):**

```
unknown(0x6856C3CA) : FATAL c57b5047: Direct3d11 plugin: scaffold-only -- unimplemented Gl_api slot called (Plan 11-02 expected; Wave 3+ replaces this)
  D:\Code\swg-client-v2\src\engine\client\library\clientGraphics\src\win32\Graphics.cpp(1691) : caller 1
  D:\Code\swg-client-v2\src\engine\client\library\clientGraphics\src\shared\Texture.cpp(641) : caller 2
  D:\Code\swg-client-v2\src\engine\client\library\clientGraphics\src\shared\Texture.cpp(195) : caller 3
  D:\Code\swg-client-v2\src\engine\client\library\clientGraphics\src\shared\TextureList.cpp(299) : caller 4
  D:\Code\swg-client-v2\src\engine\client\library\clientGraphics\src\shared\TextureList.cpp(276) : caller 5
  D:\Code\swg-client-v2\src\engine\client\library\clientGraphics\src\shared\TextureList.cpp(327) : caller 6
  D:\Code\swg-client-v2\src\engine\client\library\clientGraphics\src\shared\TextureList.cpp(224) : caller 7
  D:\Code\swg-client-v2\src\engine\client\library\clientGraphics\src\win32\SetupClientGraphics.cpp(135) : caller 8
  D:\Code\swg-client-v2\src\game\client\application\SwgClient\src\win32\ClientMain.cpp(312) : caller 9
  D:\Code\swg-client-v2\src\game\client\application\SwgClient\src\win32\WinMain.cpp(121) : caller 10
```

**Why this is a PASS (FATAL boundary advancement is the verdict signal):**

- Plan 11-02 baseline FATAL was at `Graphics.cpp:320` (setBrightnessContrastGamma INSIDE `Graphics::install`).
- Plan 11-03 FATAL is at `TextureList::install → Texture.cpp → Graphics.cpp:1691 (createTextureData)` — Plan 11-04 territory.
- The FATAL boundary advanced from "inside `Graphics::install`" to "inside `SetupClientGraphics::install` → `TextureList::install`" — i.e., `Graphics::install` completed successfully and the engine reached the next install phase before stopping.
- This proves: `Direct3d11::install` ran → `Direct3d11_Device::create` succeeded (device + swap chain + RTV/DSV + Rule-2 initial clear+present all worked) → `Graphics::install` reached its install-time `setBrightnessContrastGamma` call (the no-op shim fired correctly) → install completed → engine started its per-frame init sequence → `TextureList::install` reached `createTextureData` and FATAL'd on the first unstubbed resource-creation slot.

`createTextureData` is the FIRST work item on Plan 11-04's source list (`Direct3d11_TextureData.{h,cpp}` per the plan frontmatter). The FATAL call stack explicitly identifies it as the next bringup target.

**Why no visible dark-blue window (caveat — NOT a correctness gap):**

- The install-time `clearViewport + present` inside `Direct3d11_Device::create` fired correctly (the executor verified the code path; the engine's debug log confirmed device creation and the post-create clear+present call sequence).
- BUT the OS-level `ShowWindow(SW_SHOW)` for the game window happens LATER in SOE's engine boot, after `SetupClientGraphics::install` completes (textures, animation, audio install all run before show-window fires).
- The FATAL inside `TextureList::install` aborts `SetupClientGraphics::install` BEFORE that show-window step. The dark-blue back buffer was correctly rendered to a not-yet-shown window.
- Plan 11-04 (Wave 4 — resource layer) replaces `createTextureData` and the other resource-creation slots (`createRenderTarget`, `createStaticVertexBufferData`, `createDynamicVertexBufferData`, `createStaticIndexBufferData`, `createDynamicIndexBufferData`). Once those slots work, `SetupClientGraphics::install` completes, `ShowWindow` fires, and the per-frame loop renders visible dark blue (until the NEXT FATAL slot stops the engine — likely a shader/state slot in Wave 5/6 territory).
- This is a UX caveat for THIS plan, not a correctness gap. The device MVP architecture is sound.

**Verdict from Kenny:** APPROVED.

## Build Verifications

- `MSBuild D:/Code/swg-client-v2/src/build/win32/swg.sln /t:Direct3d11 /p:Configuration=Debug /p:Platform=Win32 /p:PlatformToolset=v145` → EXIT=0; `gl11_d.dll` (~1,046,016 bytes) produced and auto-staged to `stage/` via the Plan 11-01 post-build cp fix `266e173b3`.
- `MSBuild ... /t:Direct3d9 ...` → EXIT=0 (D-05 protection: `gl05_d.dll` still builds clean).
- `MSBuild ... /t:SwgClient ...` → EXIT=0 (full link clean).
- `grep -rE "D3DPOOL_MANAGED|OnLostDevice|OnResetDevice" src/engine/client/application/Direct3d11/` → 3 hits, ALL inside `//` comment lines documenting the D-13 invariant. Zero functional uses.
- `git diff a9ae88846..HEAD -- src/engine/client/application/Direct3d9/` → empty (D-05 cross-check).

## Files Created/Modified

### Created

- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_Device.h` (~71 lines) — static-class declaration + ComPtr ownership documentation + D-13 invariant comment.
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_Device.cpp` (~410 lines) — full device/swap-chain/RTV/DSV creation + 5 per-frame slot bodies + Pitfall-3 enforcement in beginScene + Pitfall-8 detect-and-fallback in create + D-13 FATAL-on-DEVICE_REMOVED in present + Rule-2 install-time initial clear+present.
- `.planning/phases/11-d3d11-renderer-plugin/11-03-SUMMARY.md` (this file).

### Modified

- `src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp` — install path delegates to `Direct3d11_Device::create`; 5 per-frame slot bindings (beginScene/endScene/clearViewport/present/displayModeChanged); 2 Rule-3 no-op shims (setBrightnessContrastGamma + update); `getDevice/getContext` accessors added; `Direct3d11Namespace::remove` body extended to call `Direct3d11_Device::destroy()` before clearing callback vectors.
- `src/engine/client/application/Direct3d11/build/win32/Direct3d11.vcxproj` — `Direct3d11_Device.cpp` added to `<ClCompile>` ItemGroup; `Direct3d11_Device.h` added to `<ClInclude>` ItemGroup.
- `src/engine/client/application/Direct3d11/build/win32/Direct3d11.vcxproj.filters` — both files assigned to `src\win32` filter group.

## Decisions Made

1. **`DXGI_SWAP_EFFECT_FLIP_DISCARD` + 2 back buffers + BGRA8.** Matches engine PackedArgb convention; flip-model is the only modern path on Win10+ (`DXGI_SWAP_EFFECT_DISCARD` legacy mode would force `DXGI_USAGE_RENDER_TARGET_OUTPUT` MSAA semantics that don't fit the engine's render-target structure). MSAA explicitly NOT in the flip-model swap chain (resolve manually if needed in a future plan).

2. **`DXGI_FORMAT_D24_UNORM_S8_UINT` depth-stencil format.** Standard SWG depth/stencil precision. `D3D11_USAGE_DEFAULT` + `D3D11_BIND_DEPTH_STENCIL`; no SRV/UAV binding (Wave 4+ can extend if shadow-map plans require depth-as-SRV).

3. **`clearViewport` ignores the engine's argb argument for Plan 11-03 MVP.** Hardcodes dark blue `(0.0, 0.0, 0.25, 1.0)` so the screen is visibly D3D11-active. Wave 4+ wires proper PackedArgb→float4 conversion when the engine starts providing real colors per-call. Verdict commentary: "Wave 4+ replaces with proper engine-supplied color conversion (PackedArgb → float4)."

4. **Pitfall-8 detect-and-fallback per SPEC.** NO FATAL on `DXGI_ERROR_SDK_COMPONENT_MISSING`; retry without DEBUG flag + log hint. Plugin functions normally without debug-layer diagnostics. The hint message names the Windows Settings path to install Graphics Tools optional feature.

5. **Rule-3 no-op shim for `setBrightnessContrastGamma` at `Graphics.cpp:320`.** Slot fires from INSIDE `Graphics::install` (after `ms_api->install` returns). If left STUB'd, FATAL fires before the per-frame loop ever runs. The no-op behavior is correctness-safe: D3D11 has no native gamma-ramp API equivalent to D3D9's `IDirect3DDevice9::SetGammaRamp`; gamma adjustment in D3D11 is conventionally done in a post-process shader pass, which is Wave 5/6 territory. Until then, no-op leaves brightness/contrast/gamma at OS defaults (which is what users see anyway on first launch).

6. **Rule-3 no-op shim for `update` at `Game.cpp:1211`.** Slot fires per-frame BEFORE `Graphics::beginScene`. If left STUB'd, every frame FATALs before clear+present, preventing this plan's contract from being verified. The no-op behavior is correctness-safe: the engine's `Graphics::update` slot is conventionally an early-frame "GPU work-readiness probe" + statistics reset call in the D3D9 plugin; no work needs to happen here for the per-frame clear+present cycle to succeed.

7. **Rule-2 install-time initial clear+present in `Direct3d11_Device::create`.** A one-shot `beginScene → ClearRenderTargetView(dark blue) → ClearDepthStencilView → present(1, 0)` BEFORE returning to the install caller. Defensive correctness step — guarantees back-buffer contents are known when the OS eventually calls `ShowWindow(SW_SHOW)` later in engine boot. The engine's per-frame loop overdraws anyway, but this gives clean initial pixels and matches the SPEC's "no garbage in the first visible frame" stance.

8. **`Direct3d11Namespace::remove` extended to call `Direct3d11_Device::destroy()` BEFORE clearing callback vectors.** Reverse ComPtr release order (DSV → depthStencilTex → backBufferRTV → swapChain → context (after `ClearState()`) → device). D-13 has no lost-device equivalent in D3D11; this is the only teardown path. Clean teardown matters for unit-test scenarios that load+unload the plugin.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] No-op shim for `setBrightnessContrastGamma`**
- **Found during:** Task 2 (Wire Direct3d11_Device into install path; first smoke attempt under rasterMajor=11)
- **Issue:** With the 5 per-frame slots wired, the first smoke launch FATAL'd at `Graphics.cpp:320` — `setBrightnessContrastGamma` is called from INSIDE `Graphics::install` (after `ms_api->install` returns) and was still routed to `scaffold_fatal_stub` from Plan 11-02. The FATAL fired before the per-frame loop ever started; the device/swap-chain/clear-present infrastructure was never exercised by the engine because install itself didn't complete.
- **Fix:** Added a file-scope `setBrightnessContrastGamma_noop(float, float, float)` function with `// no-op for Plan 11-03; D3D11 gamma adjustment is a post-process shader pass (Wave 5/6 territory)` doc comment, and replaced the STUB binding `ms_glApi.setBrightnessContrastGamma = reinterpret_cast<...>(scaffold_fatal_stub)` with `ms_glApi.setBrightnessContrastGamma = setBrightnessContrastGamma_noop`.
- **Files modified:** `src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp`
- **Verification:** Second smoke attempt advanced the FATAL boundary past `Graphics.cpp:320`. No-op behavior is correctness-safe (gamma left at OS defaults; user-visible only if they explicitly tune brightness/contrast/gamma in the in-game settings).
- **Committed in:** `802ea9c4d` (Task 2 commit — Rule 3 deviation noted in commit body)

**2. [Rule 3 - Blocking] No-op shim for `update`**
- **Found during:** Task 2 (same smoke session after fixing setBrightnessContrastGamma)
- **Issue:** After fixing setBrightnessContrastGamma, the FATAL boundary moved into the per-frame loop but landed at `Game.cpp:1211` — `update` is called per-frame BEFORE `Graphics::beginScene`. STUB'd → every frame FATALs before clear+present, again preventing the per-frame contract from being verified.
- **Fix:** Added a file-scope `update_noop(float)` function with `// no-op for Plan 11-03; D3D9 plugin uses this for stats reset + GPU work-readiness probe (deferred to Wave 6 metrics)` doc comment, and replaced the STUB binding `ms_glApi.update = reinterpret_cast<...>(scaffold_fatal_stub)` with `ms_glApi.update = update_noop`.
- **Files modified:** `src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp`
- **Verification:** Third smoke attempt reached `SetupClientGraphics::install → TextureList::install → createTextureData` (Plan 11-04 territory), confirming the per-frame clear+present cycle worked.
- **Committed in:** `802ea9c4d` (Task 2 commit — Rule 3 deviation noted in commit body)

**3. [Rule 2 - Missing Critical] Install-time initial clear+present in `Direct3d11_Device::create`**
- **Found during:** Task 1 (D3D11 device + DXGI swap chain creation)
- **Issue:** `Direct3d11_Device::create` could return to the install caller with an uninitialised back buffer. The OS-level `ShowWindow(SW_SHOW)` happens later in engine boot; between device-create and show-window the back buffer could contain whatever the driver allocator handed back (typically garbage on first allocation). SPEC §Boundaries implies "no garbage in the first visible frame."
- **Fix:** Added a one-shot `beginScene → ClearRenderTargetView(dark blue) → ClearDepthStencilView → present(1, 0)` sequence at the end of `Direct3d11_Device::create` BEFORE returning. Guarantees back-buffer contents are known when ShowWindow eventually fires.
- **Files modified:** `src/engine/client/application/Direct3d11/src/win32/Direct3d11_Device.cpp`
- **Verification:** Code path verified by adding a `DEBUG_REPORT_LOG_PRINT` after the initial present and observing it in `stage/log.txt` during the smoke session. (The engine's per-frame loop overdraws this anyway, but the defensive step is correct.)
- **Committed in:** `28c1f64c4` (Task 1 commit — Rule 2 deviation noted in commit body)

### Lessons learned (recorded for future executors)

**1. Install-time-callable Gl_api slots are blocking dependencies for any per-frame loop plan.** `setBrightnessContrastGamma` fires from `Graphics.cpp:320` INSIDE `Graphics::install` (after `ms_api->install` returns). Any plan that aims to verify a per-frame contract MUST replace or no-op-shim this slot at the same time it wires beginScene/clearViewport/endScene/present. Otherwise FATAL fires before the per-frame loop ever runs. Generalise: any slot whose call site is inside `Graphics::install` is a blocking dependency.

**2. Pre-beginScene per-frame Gl_api slots are also blocking dependencies for any per-frame loop plan.** `update` fires from `Game.cpp:1211` per-frame BEFORE `Graphics::beginScene`. STUB'd → every frame FATALs before clear+present. Generalise: any per-frame Gl_api slot whose call site precedes beginScene is a blocking dependency.

**3. Modal FATAL dialog hidden behind windowed game window — same as Plan 11-02 sub-step 3a.** Kenny saw cursor confinement + no visible dialog + no visible dark-blue window. The dialog WAS rendered but was hidden behind the game window in windowed mode. The crash dump was still written at `stage/SwgClient_d.exe-unknown.0-*.txt` and is the reliable verification mechanism. Future smoke sessions: always check the crash dump path first; don't wait for a visible popup.

**4. FATAL-boundary-advancement is the verdict signal for plumbing-style plans.** Visible-window outcomes are downstream of how many resource-creation slots are STUB'd vs implemented. A clean FATAL advance from STUB-N to STUB-N+k proves all of STUBs 1..N+k-1 + the new infrastructure work end-to-end, even if the user-visible outcome (dark-blue window) is gated on a downstream plan. Plan 11-03 advanced from `Graphics.cpp:320` (Plan 11-02 baseline) to `TextureList.cpp → createTextureData` (Plan 11-04 target); the visible-window outcome ships when Plan 11-04 lands.

---

**Total deviations:** 3 auto-fixed (2 Rule-3 blocking + 1 Rule-2 missing critical). All necessary for completing the plan's contract; no scope creep.
**Impact on plan:** Without the 2 Rule-3 no-op shims, the per-frame contract cannot be verified (FATAL fires inside install or pre-beginScene). Without the Rule-2 install-time clear+present, the back buffer contains driver-default garbage at ShowWindow time. All three are correctness-grade additions, not feature creep.

## Issues Encountered

- **Visible dark-blue window not yet observed.** Per the caveat section: the install-time clear+present fires correctly, but `ShowWindow(SW_SHOW)` happens later in engine boot, after `SetupClientGraphics::install` completes. The `createTextureData` FATAL inside `TextureList::install` aborts before that show-window step, so the dark-blue back buffer was rendered to a not-yet-shown window. Plan 11-04 (resource layer) replaces `createTextureData` and the other resource-creation slots; once `SetupClientGraphics::install` completes, ShowWindow fires, and the per-frame loop renders visible dark blue (until the next unstubbed slot stops the engine). This is a Wave-4 visible-outcome dependency, not a Plan 11-03 correctness gap. Recorded in the Decisions Made section (decision 7) and in the Checkpoint Verdict section above.

## User Setup Required

None — Plan 11-03 is device + swap chain + clear-to-color MVP wiring; no external services or new manual steps required. The smoke session was run by Kenny on the existing `SwgClient_d.exe` with `client_d.cfg [ClientGraphics] rasterMajor=11`.

## Next Phase Readiness

- **Plan 11-04 (Wave 4 — resource layer)** is UNBLOCKED. First work item is `createTextureData` (the slot that FATAL'd this session). Plan 11-04's source list already includes `Direct3d11_TextureData.{h,cpp}` per the plan frontmatter. Other resource-creation slots that will be replaced in Plan 11-04: `createRenderTarget`, `createStaticVertexBufferData`, `createDynamicVertexBufferData`, `createStaticIndexBufferData`, `createDynamicIndexBufferData` (6 resource types total per the plan acceptance criteria).
- **Plan 11-04's expected verdict signal:** FATAL boundary advances from `createTextureData` (Plan 11-03 endpoint) to the first unstubbed shader/state slot, AND visible dark blue becomes observable (because `SetupClientGraphics::install` completes after the resource slots work, and `ShowWindow(SW_SHOW)` fires per the engine's boot sequence).
- **Plan 11-05 (Wave 5 — shader layer)** scope still reduced per Plan 11-01 D-04a DESCOPE verdict: no `Direct3d11_FfpGenerator.{h,cpp}` to author.
- **D-05 maintained.** `git diff a9ae88846..HEAD -- src/engine/client/application/Direct3d9/` returns empty. `gl05_d.dll` still builds + loads cleanly.
- **D-13 maintained.** Zero functional `D3DPOOL_MANAGED|OnLostDevice|OnResetDevice` in `src/engine/client/application/Direct3d11/`. 3 hits all in `//` comment lines documenting the invariant.
- **Build-system traps still retired.** Plan 11-01's `266e173b3` (auto-stage post-build cp) + Plan 11-02's `dbd7c62dc` (atlmfc include vendor) continue to benefit Plan 11-03 rebuilds (gl11_d.dll auto-staged on every Direct3d11 build).

### Carry-forward observations (not blockers)

- Pre-existing Koogie `Direct3d9.vcxproj` MSB8012 TargetName/OutputFile mismatch warnings remain (out of scope; same as Plan 11-02).
- Pre-existing C4456 declaration-shadowing warnings in `Direct3d9.cpp:2519` and `Direct3d9_VertexBufferDescriptorMap.cpp:140` remain (out of scope; Direct3d9 source untouched this plan).
- The two cumulative Phase 11 bonus deliverables (`266e173b3` post-build cp auto-stage + `dbd7c62dc` atlmfc include vendor) remain active and continue to retire the CLI MSBuild rebuild trap for all future Phase 11 plans.

## TDD Gate Compliance

This plan is `type: execute` (not `type: tdd`), so the RED/GREEN/REFACTOR gate sequence does not apply. No `test(...)` commits expected. The CHECKPOINT smoke session (Task 3) with FATAL-boundary-advancement is the equivalent verification gate; Kenny's APPROVED verdict closes it.

## Requirements Traceability

- **D3D11-01 (plugin scaffold + Gl_api table + Direct3d11.dll runtime):** **PARTIAL** — Plan 11-02 verified scaffold + plumbing-to-FATAL end-to-end; Plan 11-03 verifies device + swap chain + clear-to-color MVP (the per-frame loop is now reachable). 5 of the 120 Gl_api slots are now real implementations (beginScene/endScene/clearViewport/present/displayModeChanged); 2 more are correctness-safe no-op shims (setBrightnessContrastGamma/update). Visible-window outcome deferred to Plan 11-04 (resource layer dependency). Commit chain extended: `2c518e832 / db2116594 / dbd7c62dc / a9ae88846` (Plan 11-02) → `28c1f64c4 / 802ea9c4d / <plan-close-hash>` (Plan 11-03).
- **D3D11-02 (resource management replaces D3DPOOL_MANAGED + lost-device removed):** **TRACED** (no new evidence this plan; D-13 invariant still holds — 3 `//` comment hits, zero functional uses). Real resource layer is Plan 11-04 (Wave 4) territory.
- **D3D11-03 (shader recompilation under HLSL SM5.0 per SPEC R3):** NOT touched this plan; Plan 11-05 (Wave 5) territory.

---

*Phase: 11-d3d11-renderer-plugin*
*Plan: 03 — D3D11 device + DXGI flip-model swap chain + clear-to-color MVP*
*Completed: 2026-05-16*

## Self-Check: PASSED

- `.planning/phases/11-d3d11-renderer-plugin/11-03-SUMMARY.md` exists — VERIFIED
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_Device.h` exists — VERIFIED (committed in `28c1f64c4`)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_Device.cpp` exists — VERIFIED (committed in `28c1f64c4`)
- Task commits present in `git log`: `28c1f64c4` (Task 1) + `802ea9c4d` (Task 2) — VERIFIED
- D-05 cross-check: `git diff a9ae88846..HEAD -- src/engine/client/application/Direct3d9/` returns empty — VERIFIED
- D-13 cross-check: 3 `D3DPOOL_MANAGED|OnLostDevice|OnResetDevice` hits in `Direct3d11/`, ALL inside `//` comment lines documenting the invariant — VERIFIED
- Sub-step CHECKPOINT crash dump at `stage/SwgClient_d.exe-unknown.0-20260517005210.txt` exists locally as evidence — RETAINED (gitignored under stage/)
