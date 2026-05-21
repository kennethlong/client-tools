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

// ======================================================================

class Direct3d11_Device
{
public:
	// Lifecycle -- called from Direct3d11::install.
	static bool create(HWND hwnd, int width, int height, bool windowed);
	static void destroy();

	// Singleton accessors -- nullptr before create() and after destroy().
	static ID3D11Device *        getDevice();
	static ID3D11DeviceContext * getContext();
	static int                   getWidth();
	static int                   getHeight();

	// Plan 11-09 Iter-2.7c (CODEX Round 5): expose backbuffer RTV for
	// post-PS-rejection diagnostic. applyPreDrawState's first-fallback probe
	// compares OMGetRenderTargets vs this pointer to determine if UI draws
	// hit an offscreen RT instead of the swapchain backbuffer.
	static ID3D11RenderTargetView * getBackBufferRTV();

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
};

// ======================================================================

#endif
