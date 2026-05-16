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
};

// ======================================================================

#endif
