// ======================================================================
//
// Direct3d11_Device.h
// Phase 11 D3D11 renderer plugin -- device + flip-model swap chain +
// back-buffer RTV + depth-stencil DSV ownership + per-frame slot bodies.
//
// Plan 11-03 (Wave 3 -- clear-to-color MVP). Replaces the FATAL-stub
// per-frame slots populated in Plan 11-02 with real D3D11 implementations.
//
// Per CONTEXT D-01: clean rewrite; D3D9 design reference only. Owns
// resources via Microsoft::WRL::ComPtr. Per D-13: NO D3DPOOL_MANAGED /
// OnLostDevice / OnResetDevice (DXGI flip model has no lost-device
// concept; DEVICE_REMOVED is process-restart class per SPEC §Boundaries).
//
// ======================================================================

#ifndef INCLUDED_Direct3d11_Device_H
#define INCLUDED_Direct3d11_Device_H

// ======================================================================

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

class PackedArgb;
struct ID3D11InfoQueue;   // Plan 11-09.13 Iter-4: forward-decl to keep <d3d11sdklayers.h> out of this header

// ======================================================================

class Direct3d11_Device
{
public:
	// Lifecycle -- called from Direct3d11::install.
	static bool create(HWND hwnd, int width, int height, bool windowed, bool engineOwnsWindow);
	static void destroy();

	// Singleton accessors -- nullptr before create() and after destroy().
	static ID3D11Device *        getDevice();
	static ID3D11DeviceContext * getContext();
	static int                   getWidth();
	static int                   getHeight();
	static HWND                  getWindow();
	static bool                  isWindowed();
	static bool                  engineOwnsWindow();

	// Plan 11-09.5: plugin-side window-show parity with D3D9. Mirrors
	// Direct3d9Namespace::updateWindowSettings (Direct3d9.cpp:1870-1930):
	// sets WS_CAPTION-class style (windowed) or WS_POPUP (fullscreen),
	// adjusts client-rect for chrome, centers on the DXGI containing-output
	// monitor on first call, and ends in SetWindowPos(... SWP_SHOWWINDOW)
	// which is what reveals the engine-created hidden window
	// (Os::install creates with WS_POPUP and no WS_VISIBLE by design).
	// Called from Direct3d11::install after create() succeeds.
	static void updateWindowSettings();

	// Plan 11-09.5: real binding for Gl_api::setWindowedMode (replaces
	// the Plan 11-02 scaffold_fatal_stub STUB). For Iter-1 the DXGI
	// fullscreen toggle is intentionally deferred (windowed-mode only)
	// per 11-09.5-PLAN.md Task 1d scope guardrail; the function logs and
	// re-applies updateWindowSettings rather than FATAL'ing on toggle.
	static void setWindowedMode(bool windowed);

	// Plan 11-09 Iter-2.7c (CODEX Round 5): expose backbuffer RTV for
	// post-PS-rejection diagnostic. applyPreDrawState's first-fallback probe
	// compares OMGetRenderTargets vs this pointer to determine if UI draws
	// hit an offscreen RT instead of the swapchain backbuffer.
	static ID3D11RenderTargetView * getBackBufferRTV();

	// Phase 19: the shared screen depth-stencil view (full-screen D24S8).
	// Direct3d11_RenderTarget binds this on the engine's full-screen scene RT
	// (the post-FX baked target the whole world renders into) so world geometry
	// depth-tests. D3D9 parity: SetRenderTarget for a screen-sized RT does NOT
	// clear the depth surface, so the primary depth persists (Direct3d9_
	// RenderTarget::setRenderTarget only nulls depth for non-render-target user
	// textures). Caller does NOT own the returned pointer.
	static ID3D11DepthStencilView * getDepthStencilView();

	// Plan 11-09.8: phantom zero buffer at InputSlot=15. A 16-byte
	// zero-filled BIND_VERTEX_BUFFER (USAGE_IMMUTABLE) created at install
	// time. Bound by Direct3d11_StateCache::applyPreDrawState with
	// stride=0 + offset=0 so every vertex reads zero. Backs reflected-but-
	// VBFormat-absent VS inputs (CreateInputLayout signature validation
	// under vs_4_0 requires the layout cover the bytecode signature).
	// Caller does NOT own the returned pointer.
	static ID3D11Buffer *           getPhantomZeroBuffer();

	// Plan 11-09 Iter-2.7f (CODEX Round 6): signal that a draw was submitted
	// in the current frame. Used by clearViewport's primary-backbuffer escape
	// hatch -- if clearViewport targets the backbuffer AFTER draw activity,
	// it stashes a pending clear that beginScene applies at the start of the
	// next frame. (D3D9 engine pattern is clear-after-draw; D3D11 flip-model
	// would wipe the to-be-presented surface if we cleared immediately.)
	// Idempotent; cheap; safe to call per-draw.
	static void setFrameHasDrawActivity();

	// ------------------------------------------------------------------
	// Gl_api per-frame slot bodies (replace FATAL stubs from Plan 11-02).
	// Signatures match Gl_api struct field types in Gl_dll.def exactly.

	// Gl_api::beginScene  -- void (*beginScene)()
	static void beginScene();

	// Gl_api::endScene  -- void (*endScene)()
	static void endScene();

	// Gl_api::clearViewport  --
	//   void (*clearViewport)(bool clearColor, uint32 colorValue,
	//                         bool clearDepth, real depthValue,
	//                         bool clearStencil, uint32 stencilValue)
	static void clearViewport(bool clearColor, uint32 colorValue,
	                          bool clearDepth, real depthValue,
	                          bool clearStencil, uint32 stencilValue);

	// Gl_api::present  -- bool (*present)()
	static bool present();

	// Gl_api::displayModeChanged  -- void (*displayModeChanged)()
	// Recreates back-buffer RTV + DSV at the new HWND client-rect size.
	static void displayModeChanged();

	// ------------------------------------------------------------------
	// Plan 11-08 Iter-1: ID3D11InfoQueue frame-drain. Drains every D3D11
	// debug-layer message accumulated since the previous call and routes
	// each via DEBUG_REPORT_LOG_PRINT. Called from present() before the
	// swap-chain Present so a frame's draw-call validation messages land
	// in the runtime log on the same frame they fire. No-op when the
	// debug layer is not live (ConfigDirect3d11::getEnableDebugLayer()
	// returned false, OR Pitfall 8 fallback stripped the DEBUG flag at
	// device creation time). The Iter-18 BSOD established this drain is
	// the prerequisite safety net for any cbuffer-write change.
	static void drainInfoQueue();

	// Plan 11-09.13 Iter-4: raw ID3D11InfoQueue accessor for application
	// diagnostics. AddApplicationMessage messages drain through the file
	// sink at stage/d3d11-debug.log alongside debug-layer messages -- the
	// route DEBUG_REPORT_LOG_PRINT does NOT take (it goes to
	// OutputDebugString + stdout, invisible from explorer-launched
	// smokes per Direct3d11_Device.cpp:91 preamble). Returns null when
	// the debug layer is not live. Caller must NOT Release; the device
	// holds the ComPtr.
	static ID3D11InfoQueue *getInfoQueue();

	// Phase 19 (19-04 Task 0) DEV-ONLY: monotonic per-frame index for the
	// neutral b0 / setLights value-dump. Incremented in drainInfoQueue().
	// REVERT after the cross-frame light-feed question is answered.
	static unsigned getDiagFrameIndex();
};

// ======================================================================

#endif
