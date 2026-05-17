// ======================================================================
//
// Direct3d11.cpp
// Phase 11 D3D11 renderer plugin -- plugin entry point + Gl_api scaffold.
//
// Plan 11-02 SCAFFOLD-ONLY: every Gl_api slot is populated, but every
// slot that does real rendering work is routed to scaffold_fatal_stub.
// Engine-queried slots (during Graphics::install() startup -- supports*,
// getShaderCapability, requiresVertexAndPixelShaders, lost-device
// callback register/unregister, wasDeviceReset, isGdiVisible) are
// real no-op implementations so the engine reaches the first work-slot
// FATAL as designed.  Wave 3 onward replaces FATAL stubs with real
// D3D11 implementations.
//
// Per CONTEXT D-01: this is a clean rewrite. Direct3d9.cpp is the
// design reference only; not source-copied.
//
// ======================================================================

#include "FirstDirect3d11.h"
#include "Direct3d11.h"

#include "ConfigDirect3d11.h"
#include "Direct3d11_Device.h"

#include "clientGraphics/Gl_dll.def"
#include "clientGraphics/ShaderCapability.h"
#include "sharedDebug/DebugFlags.h"
#include "sharedFoundation/Os.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <stdio.h>
#include <vector>

// ======================================================================

namespace Direct3d11Namespace
{
	// ------------------------------------------------------------------
	// The exported Gl_api table.

	Gl_api ms_glApi;

	// ------------------------------------------------------------------
	// Lost-device callback registration -- D3D11 has no lost-device
	// concept, but the engine still queries these slots during boot
	// (PostProcessingEffectsManager registers callbacks unconditionally).
	// Track registrations but never invoke them.

	typedef void (*CallbackFunction)();
	std::vector<CallbackFunction> ms_deviceLostCallbacks;
	std::vector<CallbackFunction> ms_deviceRestoredCallbacks;

	// ------------------------------------------------------------------
	// Scaffold FATAL stub -- every unimplemented work slot routes here.
	// Wave 3+ plans replace these one-by-one with real D3D11 work.

	[[noreturn]] void scaffold_fatal_stub()
	{
		FATAL(true, ("Direct3d11 plugin: scaffold-only -- unimplemented Gl_api slot called (Plan 11-02 expected; Wave 3+ replaces this)"));
	}

	// ------------------------------------------------------------------
	// HRESULT-to-string helper. Replaces <dxerr9.h>'s DXGetErrorString9
	// (gone in D3D11 SDK). FormatMessageA covers the system HRESULTs;
	// driver-specific D3D11 hresults will print as "HRESULT 0x...".

	char const * hresultString(HRESULT hr)
	{
		static char buf[256];
		DWORD const n = FormatMessageA(
			FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr, static_cast<DWORD>(hr), 0,
			buf, sizeof(buf), nullptr);
		if (n == 0)
			snprintf(buf, sizeof(buf), "HRESULT 0x%08lX (no message)", static_cast<unsigned long>(hr));
		return buf;
	}

	// ------------------------------------------------------------------
	// Engine-queried no-op implementations. These MUST be real (not FATAL)
	// because Graphics::install() and downstream subsystems call them
	// before any real rendering happens. Returning sane defaults keeps the
	// engine on its happy path until it hits the first scaffold_fatal_stub.

	bool verify_impl()
	{
		// Called from Graphics.cpp:245 -- gate on whether the plugin loaded OK.
		// In scaffold, return true (the only failure mode is missing slots,
		// which we've filled below).
		return true;
	}

	bool requiresVertexAndPixelShaders_impl()
	{
		// D3D11 has no fixed-function pipeline; SM5.0 always available.
		return true;
	}

	bool wasDeviceReset_impl()
	{
		// D3D11 has no lost-device. Always false.
		return false;
	}

	bool isGdiVisible_impl()
	{
		// Pre-Win10 GDI-on-DX-surface query; irrelevant on D3D11 targets.
		return false;
	}

	void addDeviceLostCallback_impl(CallbackFunction fn)
	{
		// Track but never invoke -- D3D11 has no lost-device concept.
		ms_deviceLostCallbacks.push_back(fn);
	}

	void removeDeviceLostCallback_impl(CallbackFunction fn)
	{
		for (auto it = ms_deviceLostCallbacks.begin(); it != ms_deviceLostCallbacks.end(); ++it)
			if (*it == fn) { ms_deviceLostCallbacks.erase(it); return; }
	}

	void addDeviceRestoredCallback_impl(CallbackFunction fn)
	{
		ms_deviceRestoredCallbacks.push_back(fn);
	}

	void removeDeviceRestoredCallback_impl(CallbackFunction fn)
	{
		for (auto it = ms_deviceRestoredCallbacks.begin(); it != ms_deviceRestoredCallbacks.end(); ++it)
			if (*it == fn) { ms_deviceRestoredCallbacks.erase(it); return; }
	}

	int getShaderCapability_impl()
	{
		// D3D11 targets HLSL SM5.0 per SPEC R3. The engine's
		// ShaderCapability(major, minor) only accepts a few legacy
		// (D3D9-era) values; for the scaffold we report 2.0 (the
		// max ShaderCapability() admits) -- Wave 3 reworks the
		// shader-capability surface if SM5 needs first-class repr.
		return ShaderCapability(2, 0);
	}

	int getVideoMemoryInMegabytes_impl()
	{
		// Plausible default until DXGI query lands in Wave 3.
		return 256;
	}

	void getOtherAdapterRects_impl(stdvector<RECT>::fwd & /*rects*/)
	{
		// Single-adapter scaffold; report empty list.
	}

	bool supportsHardwareMouseCursor_impl()  { return false; }
	bool supportsMipmappedCubeMaps_impl()    { return false; }
	bool supportsScissorRect_impl()          { return false; }
	bool supportsTwoSidedStencil_impl()      { return false; }
	bool supportsStreamOffsets_impl()        { return false; }
	bool supportsDynamicTextures_impl()      { return false; }
	bool supportsAntialias_impl()            { return false; }

	int  getMaximumVertexBufferStreamCount_impl() { return 1; }

	// Plan 11-03: displayModeChanged delegates directly to Direct3d11_Device.
	// (Old Plan 11-02 displayModeChanged_impl no-op shim removed.)

	void remove_impl()
	{
		// Plugin tear-down. Plan 11-03 wires the real device destruction.
		Direct3d11_Device::destroy();
		ms_deviceLostCallbacks.clear();
		ms_deviceRestoredCallbacks.clear();
	}

	void flushResources_impl(bool /*fullReset*/)
	{
		// No-op until Wave 3 lands the resource layer.
	}

	// ------------------------------------------------------------------
	// Plan 11-03: no-op slots that fire BEFORE the per-frame loop
	// (Graphics::install line 320 setBrightnessContrastGamma) or every
	// frame BEFORE beginScene (Game::run line 1211 Graphics::update).
	// Wave 4+ wires real implementations:
	//   - setBrightnessContrastGamma -> DXGI output color-space / per-frame
	//     post-process LUT (D3D11 has no IDirect3DDevice9::SetGammaRamp
	//     equivalent at the device level)
	//   - update -> per-frame elapsed-time bookkeeping (DPVS-style timing,
	//     dynamic-vertex-buffer discard, etc.; Wave 4+ when resource layer
	//     and instrumentation arrive)

	void setBrightnessContrastGamma_impl(float /*brightness*/, float /*contrast*/, float /*gamma*/)
	{
		// no-op in scaffold; Wave 4+ wires real gamma/contrast/brightness
	}

	void update_impl(float /*elapsedTime*/)
	{
		// no-op in scaffold; Wave 4+ wires per-frame timing bookkeeping
	}

#ifdef _DEBUG
	bool getShowMipmapLevels_impl()                            { return false; }
	void showMipmapLevels_impl(bool /*enabled*/)               { /* no-op */ }
	void setTexturesEnabled_impl(bool /*enabled*/)             { /* no-op */ }
	void setBadVertexBufferVertexShaderCombination_impl(bool * /*flag*/, char const * /*name*/) { /* no-op */ }
	void getRenderedVerticesPointsLinesTrianglesCalls_impl(int &v, int &p, int &l, int &t, int &c)
	{
		v = 0; p = 0; l = 0; t = 0; c = 0;
	}
#endif
}
using namespace Direct3d11Namespace;

// ======================================================================
// Direct3d11 class -- public surface used by the engine.
// In Plan 11-02 the singletons are nullptr; Wave 3 creates the real device.

ID3D11Device *        Direct3d11::getDevice()                  { return Direct3d11_Device::getDevice(); }
ID3D11DeviceContext * Direct3d11::getContext()                 { return Direct3d11_Device::getContext(); }
int                   Direct3d11::getShaderCapability()        { return getShaderCapability_impl(); }
int                   Direct3d11::getVideoMemoryInMegabytes()  { return getVideoMemoryInMegabytes_impl(); }
bool                  Direct3d11::supportsPixelShaders()       { return true; }
bool                  Direct3d11::supportsVertexShaders()      { return true; }

// ======================================================================
// GetApi -- exported plugin entry point.

extern "C" __declspec(dllexport) Gl_api const * GetApi();

Gl_api const * GetApi()
{
	// Slots the engine touches BEFORE Direct3d11::install runs.
	ms_glApi.verify  = verify_impl;
	ms_glApi.install = Direct3d11::install;
	return &ms_glApi;
}

// ======================================================================
// Direct3d11::install -- populate the rest of the Gl_api table.

bool Direct3d11::install(Gl_install * gl_install)
{
	NOT_NULL(gl_install);

	ConfigDirect3d11::install();

	// ------------------------------------------------------------------
	// Plan 11-03 (Wave 3): create the D3D11 device + DXGI flip-model
	// swap chain + back-buffer RTV + depth-stencil DSV BEFORE wiring
	// the per-frame slots. If this fails, return false; the engine
	// reports plugin-install failure (Graphics::install fail path).

	if (!Direct3d11_Device::create(gl_install->window,
	                               gl_install->width,
	                               gl_install->height,
	                               gl_install->windowed))
	{
		return false;
	}

	// ------------------------------------------------------------------
	// Real implementations -- engine queries these during startup.

	ms_glApi.remove                            = remove_impl;
	ms_glApi.displayModeChanged                = Direct3d11_Device::displayModeChanged;

	ms_glApi.getShaderCapability               = getShaderCapability_impl;
	ms_glApi.requiresVertexAndPixelShaders     = requiresVertexAndPixelShaders_impl;
	ms_glApi.getOtherAdapterRects              = getOtherAdapterRects_impl;
	ms_glApi.getVideoMemoryInMegabytes         = getVideoMemoryInMegabytes_impl;
	ms_glApi.isGdiVisible                      = isGdiVisible_impl;
	ms_glApi.wasDeviceReset                    = wasDeviceReset_impl;

	ms_glApi.addDeviceLostCallback             = addDeviceLostCallback_impl;
	ms_glApi.removeDeviceLostCallback          = removeDeviceLostCallback_impl;
	ms_glApi.addDeviceRestoredCallback         = addDeviceRestoredCallback_impl;
	ms_glApi.removeDeviceRestoredCallback      = removeDeviceRestoredCallback_impl;

	ms_glApi.flushResources                    = flushResources_impl;

#ifdef _DEBUG
	ms_glApi.setTexturesEnabled                            = setTexturesEnabled_impl;
	ms_glApi.showMipmapLevels                              = showMipmapLevels_impl;
	ms_glApi.getShowMipmapLevels                           = getShowMipmapLevels_impl;
	ms_glApi.setBadVertexBufferVertexShaderCombination     = setBadVertexBufferVertexShaderCombination_impl;
	ms_glApi.getRenderedVerticesPointsLinesTrianglesCalls  = getRenderedVerticesPointsLinesTrianglesCalls_impl;
#endif

	ms_glApi.supportsMipmappedCubeMaps         = supportsMipmappedCubeMaps_impl;
	ms_glApi.supportsScissorRect               = supportsScissorRect_impl;
	ms_glApi.supportsHardwareMouseCursor       = supportsHardwareMouseCursor_impl;
	ms_glApi.supportsTwoSidedStencil           = supportsTwoSidedStencil_impl;
	ms_glApi.supportsStreamOffsets             = supportsStreamOffsets_impl;
	ms_glApi.supportsDynamicTextures           = supportsDynamicTextures_impl;
	ms_glApi.supportsAntialias                 = supportsAntialias_impl;

	ms_glApi.getMaximumVertexBufferStreamCount = getMaximumVertexBufferStreamCount_impl;

	// ------------------------------------------------------------------
	// Work slots -- ALL route to scaffold_fatal_stub via type cast.
	// The engine reaches the FIRST of these (probably setBrightnessContrastGamma,
	// called from Graphics::install at line 312) and FATALs as designed,
	// proving end-to-end plumbing reached the plugin's install path.

	#define STUB(slot) ms_glApi.slot = reinterpret_cast<decltype(ms_glApi.slot)>(scaffold_fatal_stub)

	// Plan 11-03: setBrightnessContrastGamma fires from inside Graphics::install
	// at Graphics.cpp:320 -- MUST be a no-op for install to complete and the
	// per-frame loop to start. Wave 4+ wires real gamma/contrast/brightness.
	ms_glApi.setBrightnessContrastGamma = setBrightnessContrastGamma_impl;

	STUB(resize);
	STUB(setWindowedMode);

	STUB(setFillMode);
	STUB(setCullMode);

	STUB(setPointSize);
	STUB(setPointSizeMax);
	STUB(setPointSizeMin);
	STUB(setPointScaleEnable);
	STUB(setPointScaleFactor);
	STUB(setPointSpriteEnable);

	STUB(setAntialiasEnabled);

	// Plan 11-03 (Wave 3): per-frame clear-to-color slot bodies wired to
	// Direct3d11_Device. update is a no-op shim (engine calls every frame
	// at Game::run:1211 BEFORE beginScene; Wave 4+ wires real timing).
	// presentToWindow remains STUB'd (swap-chain-to-arbitrary-HWND used by
	// the launcher overlay -- Wave 4+ territory).
	ms_glApi.beginScene    = Direct3d11_Device::beginScene;
	ms_glApi.endScene      = Direct3d11_Device::endScene;
	ms_glApi.clearViewport = Direct3d11_Device::clearViewport;
	ms_glApi.present       = Direct3d11_Device::present;
	ms_glApi.update        = update_impl;

	STUB(lockBackBuffer);
	STUB(unlockBackBuffer);

	STUB(presentToWindow);
	STUB(setRenderTarget);
	STUB(copyRenderTargetToNonRenderTargetTexture);

	STUB(screenShot);

	STUB(createShaderImplementationGraphicsData);
	STUB(createStaticShaderGraphicsData);
	STUB(setBadVertexShaderStaticShader);
	STUB(setStaticShader);

	STUB(setMouseCursor);
	STUB(showMouseCursor);

	STUB(setViewport);
	STUB(setScissorRect);
	STUB(setWorldToCameraTransform);
	STUB(setProjectionMatrix);
	STUB(setFog);
	STUB(setObjectToWorldTransformAndScale);
	STUB(setGlobalTexture);
	STUB(releaseAllGlobalTextures);
	STUB(setTextureTransform);
	STUB(setVertexShaderUserConstants);
	STUB(setPixelShaderUserConstants);

	STUB(setAlphaFadeOpacity);

	STUB(setLights);

	STUB(createStaticVertexBufferData);
	STUB(createDynamicVertexBufferData);
	STUB(createVertexBufferVectorData);
	STUB(setVertexBuffer);
	STUB(setVertexBufferVector);

	STUB(createStaticIndexBufferData);
	STUB(createDynamicIndexBufferData);
	STUB(setIndexBuffer);
	STUB(setDynamicIndexBufferSize);

	STUB(getOneToOneUVMapping);
	STUB(createTextureData);

	STUB(createVertexShaderData);
	STUB(createPixelShaderProgramData);

	STUB(drawPointList);
	STUB(drawLineList);
	STUB(drawLineStrip);
	STUB(drawTriangleList);
	STUB(drawTriangleStrip);
	STUB(drawTriangleFan);
	STUB(drawQuadList);

	STUB(drawIndexedPointList);
	STUB(drawIndexedLineList);
	STUB(drawIndexedLineStrip);
	STUB(drawIndexedTriangleList);
	STUB(drawIndexedTriangleStrip);
	STUB(drawIndexedTriangleFan);

	STUB(drawPartialPointList);
	STUB(drawPartialLineList);
	STUB(drawPartialLineStrip);
	STUB(drawPartialTriangleList);
	STUB(drawPartialTriangleStrip);
	STUB(drawPartialTriangleFan);

	STUB(drawPartialIndexedPointList);
	STUB(drawPartialIndexedLineList);
	STUB(drawPartialIndexedLineStrip);
	STUB(drawPartialIndexedTriangleList);
	STUB(drawPartialIndexedTriangleStrip);
	STUB(drawPartialIndexedTriangleFan);

	STUB(optimizeIndexBuffer);

	STUB(setBloomEnabled);

	STUB(pixSetMarker);
	STUB(pixBeginEvent);
	STUB(pixEndEvent);

	STUB(writeImage);

#if PRODUCTION == 0
	STUB(createVideoBuffers);
	STUB(fillVideoBuffers);
	STUB(getVideoBufferData);
	STUB(releaseVideoBuffers);
#endif

	#undef STUB

	DEBUG_REPORT_LOG_PRINT(true, ("Direct3d11 plugin: install path complete (scaffold -- all work slots route to FATAL; engine queries are real no-ops)\n"));
	return true;
}

// ======================================================================
