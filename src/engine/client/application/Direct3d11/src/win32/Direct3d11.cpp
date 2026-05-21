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
#include "Direct3d11_ConstantBuffer.h"
#include "Direct3d11_Device.h"
#include "Direct3d11_DynamicIndexBufferData.h"
#include "Direct3d11_DynamicVertexBufferData.h"
#include "Direct3d11_LightManager.h"
#include "Direct3d11_Metrics.h"
#include "Direct3d11_PixelShaderProgramData.h"
#include "Direct3d11_RenderTarget.h"
#include "Direct3d11_StateCache.h"   // Plan 11-09 Iter-2.7 Fix C: setVSConstants
#include "Direct3d11_ShaderCache.h"
#include "Direct3d11_ShaderImplementationData.h"
#include "Direct3d11_StateCache.h"
#include "Direct3d11_StaticIndexBufferData.h"
#include "Direct3d11_StaticShaderData.h"
#include "Direct3d11_StaticVertexBufferData.h"
#include "Direct3d11_TextureData.h"
#include "Direct3d11_VertexBufferDescriptorMap.h"
#include "Direct3d11_VertexDeclarationMap.h"
#include "Direct3d11_VertexShaderData.h"

#include "clientGraphics/Gl_dll.def"
#include "clientGraphics/ShaderCapability.h"
#include "clientGraphics/ShaderImplementation.h"
#include "clientGraphics/StaticShader.h"
#include "clientGraphics/StaticVertexBuffer.h"
#include "clientGraphics/DynamicVertexBuffer.h"
#include "clientGraphics/StaticIndexBuffer.h"
#include "sharedDebug/DebugFlags.h"
#include "sharedFoundation/Os.h"
#include "sharedMath/VectorRgba.h"

#include <DirectXMath.h>

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
		// Plugin tear-down. Plan 11-06 extends Plan 11-05 with state-cache /
		// light-manager / metrics / input-layout-cache teardown in REVERSE
		// install order (so device is the last thing released; ComPtrs see
		// a live device). Plan 11-07 Iteration 1 adds the StaticShaderData
		// + ShaderImplementationData teardown FIRST (last installed, first
		// removed).
		Direct3d11_StaticShaderData::remove();
		Direct3d11_ShaderImplementationData::remove();
		Direct3d11_Metrics::remove();
		Direct3d11_LightManager::remove();
		Direct3d11_StateCache::remove();
		Direct3d11_VertexDeclarationMap::remove();
		Direct3d11_ConstantBuffer::remove();
		Direct3d11_PixelShaderProgramData::remove();
		Direct3d11_VertexShaderData::remove();
		Direct3d11_ShaderCache::remove();
		Direct3d11_RenderTarget::remove();
		Direct3d11_TextureData::remove();
		Direct3d11_DynamicIndexBufferData::remove();
		Direct3d11_StaticIndexBufferData::remove();
		Direct3d11_DynamicVertexBufferData::remove();
		Direct3d11_StaticVertexBufferData::remove();
		Direct3d11_VertexBufferDescriptorMap::remove();
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

	// ------------------------------------------------------------------
	// Plan 11-04 (Wave 4 -- resource layer): Gl_api factory slot bodies
	// for textures, static + dynamic vertex / index buffers, and the
	// render-target re-target / read-back slots. These replace the
	// scaffold_fatal_stub bindings from Plan 11-02 and let the engine
	// hold the geometry / texture resources it pushes in. The plugin
	// still FATALs further downstream on draw-call / shader slots that
	// remain STUB'd until Plan 11-05 / 11-06.

	TextureGraphicsData * createTextureData_impl(const Texture &texture,
	                                             const TextureFormat *runtimeFormats,
	                                             int numberOfRuntimeFormats)
	{
		return new Direct3d11_TextureData(texture, runtimeFormats, numberOfRuntimeFormats);
	}

	StaticVertexBufferGraphicsData * createStaticVertexBufferData_impl(const StaticVertexBuffer &vb)
	{
		return new Direct3d11_StaticVertexBufferData(vb);
	}

	DynamicVertexBufferGraphicsData * createDynamicVertexBufferData_impl(const DynamicVertexBuffer &vb)
	{
		return new Direct3d11_DynamicVertexBufferData(vb);
	}

	StaticIndexBufferGraphicsData * createStaticIndexBufferData_impl(const StaticIndexBuffer &ib)
	{
		return new Direct3d11_StaticIndexBufferData(ib);
	}

	DynamicIndexBufferGraphicsData * createDynamicIndexBufferData_impl()
	{
		return new Direct3d11_DynamicIndexBufferData();
	}

	void setDynamicIndexBufferSize_impl(int numberOfIndices)
	{
		Direct3d11_DynamicIndexBufferData::setSize(numberOfIndices);
	}

	void setRenderTarget_impl(Texture *texture, CubeFace cubeFace, int mipmapLevel)
	{
		if (texture == nullptr)
		{
			// Engine passes NULL to mean "restore the back buffer as the
			// render target." This idiom is used by PostProcessingEffectsManager
			// (PostProcessingEffectsManager.cpp:247), Bloom restore paths, and
			// any post-FX path that needs to rebind the primary swap-chain RT
			// after a baked / secondary RT pass. cubeFace and mipmapLevel are
			// meaningless in this case (the back buffer is a flat 2D target
			// with no mip chain).
			//
			// Direct3d11_RenderTarget::setRenderTargetToPrimary handles this
			// via Direct3d11_Device::beginScene's back-buffer RTV+DSV rebind;
			// it's idempotent (early-out on ms_primaryTargetSet) so spurious
			// repeated calls are safe.
			Direct3d11_RenderTarget::setRenderTargetToPrimary();
			return;
		}

		Direct3d11_RenderTarget::setRenderTarget(texture, cubeFace, mipmapLevel);
	}

	bool copyRenderTargetToNonRenderTargetTexture_impl()
	{
		return Direct3d11_RenderTarget::copyRenderTargetToNonRenderTargetTexture();
	}

	// ------------------------------------------------------------------
	// Plan 11-05 (Wave 5 -- shader layer): Gl_api factory slot bodies
	// for the shader compile pipeline (D3DCompile vs_5_0/ps_5_0 with
	// D-03 hybrid .cso cache) and the cbuffer user-constant setters
	// (Pitfall 2 -- D3D11_USAGE_DYNAMIC + Map/Unmap).
	//
	// These replace 4 scaffold_fatal_stub bindings:
	//   - createVertexShaderData
	//   - createPixelShaderProgramData
	//   - setVertexShaderUserConstants
	//   - setPixelShaderUserConstants
	//
	// createShaderImplementationGraphicsData / createStaticShaderGraphicsData /
	// setBadVertexShaderStaticShader / setStaticShader remain STUB() until
	// Plan 11-06 (state cache + draw dispatch). The engine will FATAL on
	// one of those (probably createShaderImplementationGraphicsData -- the
	// first one called during ShaderTemplate::install) -- that's the new
	// FATAL boundary advanced by Plan 11-05.

	ShaderImplementationPassVertexShaderGraphicsData *createVertexShaderData_impl(
		ShaderImplementationPassVertexShader const &vertexShader)
	{
		return new Direct3d11_VertexShaderData(vertexShader);
	}

	ShaderImplementationPassPixelShaderProgramGraphicsData *createPixelShaderProgramData_impl(
		ShaderImplementationPassPixelShaderProgram const &pixelShaderProgram)
	{
		return new Direct3d11_PixelShaderProgramData(pixelShaderProgram);
	}

	void setVertexShaderUserConstants_impl(int index, float c0, float c1, float c2, float c3)
	{
		// Plan 11-09 Iter-2.7 Fix C: route engine user-constants directly into
		// the VS slot 0 shadow at D3D9 register VCSR_userConstant0 + index
		// (= 52 + index). Verified against
		// Direct3d9_VertexShaderConstantRegisters.h:37 and
		// Direct3d9.cpp:3497. Pre-Fix-C this stored to a function-local-static
		// shadow that nothing read (Iter-2.6 smoke proved Plan 11-05 user-
		// constants impl was a dead-write). Now the shadow lives in
		// Direct3d11_StateCacheNamespace and is flushed once-per-draw via
		// flushSlot0IfDirty at applyPreDrawState top.
		if (index < 0 || index >= 8)
			return;

		constexpr int kVCSR_userConstant0 = 52;
		float const data[4] = { c0, c1, c2, c3 };
		Direct3d11_StateCache::setVSConstants(kVCSR_userConstant0 + index, data, 1);
	}

	void setPixelShaderUserConstants_impl(VectorRgba const *constants, int count)
	{
		// D3D9 analog: Direct3d9_StateCache::setPixelShaderConstants(
		//   PSCR_userConstant, constants, count);
		//
		// D3D11 cbuffer migration: shadow into Direct3d11_PerMaterialCB
		// userConstants[]. Same flush pattern as VS (Plan 11-06 territory).
		static Direct3d11_PerMaterialCB s_perMaterialShadow = {};
		int const slots = sizeof(s_perMaterialShadow.userConstants) / sizeof(s_perMaterialShadow.userConstants[0]);
		int const clamped = (count > slots) ? slots : count;
		for (int i = 0; i < clamped; ++i)
		{
			s_perMaterialShadow.userConstants[i] = DirectX::XMFLOAT4(
				constants[i].r, constants[i].g, constants[i].b, constants[i].a);
		}
		if (count > slots)
		{
			DEBUG_REPORT_LOG_PRINT(true,
				("Direct3d11::setPixelShaderUserConstants: count=%d > userConstants[] slots=%d; truncated\n",
				count, slots));
		}
	}

	// ------------------------------------------------------------------
	// Plan 11-07 Iteration 1: shader-implementation + static-shader factory
	// slot bodies. These replace 2 STUB() bindings inherited from Plan 11-06.
	// Iteration 1 contract: construct successfully so the engine's
	// ShaderImplementation::load_NNNN() + StaticShader::construct() complete
	// past the FATAL boundary predicted by Plan 11-06 SUMMARY.
	//
	// Per-pass state apply (BS/DSS overrides) + per-pass VS/PS lookup are
	// recorded no-ops in Iteration 1; subsequent iterations populate them
	// driven by specific visual symptoms.

	ShaderImplementationGraphicsData * createShaderImplementationGraphicsData_impl(
		ShaderImplementation const &shaderImplementation)
	{
		return new Direct3d11_ShaderImplementationData(shaderImplementation);
	}

	StaticShaderGraphicsData * createStaticShaderGraphicsData_impl(
		StaticShader const &shader)
	{
		return new Direct3d11_StaticShaderData(shader);
	}
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
	                               gl_install->windowed,
	                               gl_install->engineOwnsWindow))
	{
		return false;
	}

	// Plan 11-09.5: plugin-side window-show parity with D3D9. Os::install
	// creates the engine window hidden (WS_POPUP without WS_VISIBLE, no
	// ShowWindow); the plugin owns reveal responsibility. This call ends
	// in SetWindowPos(..., SWP_SHOWWINDOW) which makes the window visible.
	// Mirrors Direct3d9.cpp:1535 placement (right after device-create
	// succeeds, before resource installs). Eliminates the
	// tools/d3d11-smoke/show-window.ps1 external workaround carried forward
	// since Plan 11-07 Iter-17.
	Direct3d11_Device::updateWindowSettings();

	// ------------------------------------------------------------------
	// Plan 11-04: resource-class install in dependency order. Each class
	// allocates its MemoryBlockManager and any process-wide ring buffers.
	// VertexBufferDescriptorMap first (consumed by VB/IB ctors), then
	// the GPU-resource classes, finally the render-target manager.

	Direct3d11_VertexBufferDescriptorMap::install();
	Direct3d11_TextureData::install();
	Direct3d11_StaticVertexBufferData::install();
	Direct3d11_DynamicVertexBufferData::install();
	Direct3d11_StaticIndexBufferData::install();
	Direct3d11_DynamicIndexBufferData::install();
	Direct3d11_RenderTarget::install();

	// ------------------------------------------------------------------
	// Plan 11-05: shader-class install. ShaderCache must come first so that
	// VertexShaderData / PixelShaderProgramData constructors find a valid
	// cache directory during ShaderImplementation::install (which loads the
	// asset shader templates).

	Direct3d11_ShaderCache::install(ConfigDirect3d11::getShaderCacheDir());
	Direct3d11_VertexShaderData::install();
	Direct3d11_PixelShaderProgramData::install();
	Direct3d11_ConstantBuffer::install();

	// ------------------------------------------------------------------
	// Plan 11-06: state cache + draw dispatch + input-layout cache + light
	// manager + metrics. Order matters:
	//   - VertexDeclarationMap depends on ShaderCache + VertexShaderData
	//     (it caches input layouts keyed by VS bytecode hash).
	//   - StateCache depends on ConstantBuffer + all resource layers
	//     above (it dispatches draws against them).
	//   - LightManager depends on ConstantBuffer (LightingCB output).
	//   - Metrics is leaf -- depends on DebugFlags only.

	Direct3d11_VertexDeclarationMap::install();
	Direct3d11_StateCache::install();
	Direct3d11_LightManager::install();
	Direct3d11_Metrics::install();

	// ------------------------------------------------------------------
	// Plan 11-07 Iteration 1: shader-template wrapper classes. The
	// ShaderImplementationData + StaticShaderData wrappers are the
	// engine's per-template / per-instance handles used during
	// ShaderImplementation::load_NNNN and StaticShader::construct.
	// They must be installed BEFORE the factory slots are wired below
	// (which the engine calls during asset load).

	Direct3d11_ShaderImplementationData::install();
	Direct3d11_StaticShaderData::install();

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

	// Plan 11-09.5: real binding replaces the Plan 11-02 STUB(setWindowedMode)
	// scaffold_fatal_stub reinterpret_cast. Iter-1 scope: windowed-mode only;
	// real DXGI SetFullscreenState toggle deferred to a future plan
	// (flip-discard swap chains have stricter semantics than D3D9 blit-model).
	ms_glApi.setWindowedMode = Direct3d11_Device::setWindowedMode;

	// Plan 11-06: rasterizer-state setters land on Direct3d11_StateCache.
	ms_glApi.setFillMode = Direct3d11_StateCache::setFillMode;
	ms_glApi.setCullMode = Direct3d11_StateCache::setCullMode;

	// Point-sprite state: D3D11 has no fixed-function point-sprite control;
	// gl_PointSize-equivalent lives in HLSL via SV_PointSize. Plan 11-06
	// stubs out the setters as no-ops; Plan 11-07 smoke surfaces whether
	// engine particle subsystems rely on engine-side point-sprite emit.
	STUB(setPointSize);
	STUB(setPointSizeMax);
	STUB(setPointSizeMin);
	STUB(setPointScaleEnable);
	STUB(setPointScaleFactor);
	STUB(setPointSpriteEnable);

	ms_glApi.setAntialiasEnabled = Direct3d11_StateCache::setAntialiasEnabled;

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
	// Plan 11-04: render-target re-target + read-back wired to
	// Direct3d11_RenderTarget. The "create" half is folded into
	// createTextureData via Texture::isRenderTarget() (D3D11 has no
	// dedicated createRenderTarget Gl_api slot -- Texture+RTV pair lives
	// inside Direct3d11_TextureData).
	ms_glApi.setRenderTarget                       = setRenderTarget_impl;
	ms_glApi.copyRenderTargetToNonRenderTargetTexture = copyRenderTargetToNonRenderTargetTexture_impl;

	STUB(screenShot);

	// Plan 11-07 Iteration 1: shader-implementation + static-shader factory
	// slots wired. The wrappers (Direct3d11_ShaderImplementationData +
	// Direct3d11_StaticShaderData) construct successfully so the engine's
	// ShaderImplementation::load_NNNN + StaticShader::construct complete
	// past the Plan 11-06 FATAL boundary. Per-pass state apply (BS/DSS
	// overrides) + per-pass VS/PS lookup are recorded no-ops; subsequent
	// iterations populate them driven by specific visual symptoms.
	ms_glApi.createShaderImplementationGraphicsData = createShaderImplementationGraphicsData_impl;
	ms_glApi.createStaticShaderGraphicsData         = createStaticShaderGraphicsData_impl;
	ms_glApi.setBadVertexShaderStaticShader = Direct3d11_StateCache::setBadVertexShaderStaticShader;
	ms_glApi.setStaticShader                = Direct3d11_StateCache::setStaticShader;

	// Windowing utilities -- Plan 11-09 / Wave 7+ territory.
	STUB(setMouseCursor);
	STUB(showMouseCursor);

	// Plan 11-06: per-frame transform + viewport + scissor + global-texture
	// + per-stage texture-transform + alpha-fade slot bodies.
	ms_glApi.setViewport                       = Direct3d11_StateCache::setViewport;
	ms_glApi.setScissorRect                    = Direct3d11_StateCache::setScissorRect;
	ms_glApi.setWorldToCameraTransform         = Direct3d11_StateCache::setWorldToCameraTransform;
	ms_glApi.setProjectionMatrix               = Direct3d11_StateCache::setProjectionMatrix;
	ms_glApi.setFog                            = Direct3d11_StateCache::setFog;
	ms_glApi.setObjectToWorldTransformAndScale = Direct3d11_StateCache::setObjectToWorldTransformAndScale;
	ms_glApi.setGlobalTexture                  = Direct3d11_StateCache::setGlobalTexture;
	ms_glApi.releaseAllGlobalTextures          = Direct3d11_StateCache::releaseAllGlobalTextures;
	ms_glApi.setTextureTransform               = Direct3d11_StateCache::setTextureTransform;
	// Plan 11-05: cbuffer user-constant setters. Plan 11-06's draw dispatch
	// flushes via Direct3d11_ConstantBuffer::bindVS(1) + bindPS(2) in
	// applyPreDrawState.
	ms_glApi.setVertexShaderUserConstants  = setVertexShaderUserConstants_impl;
	ms_glApi.setPixelShaderUserConstants   = setPixelShaderUserConstants_impl;

	ms_glApi.setAlphaFadeOpacity           = Direct3d11_StateCache::setAlphaFadeOpacity;

	// Plan 11-06: lighting flows through Direct3d11_LightManager -> cbuffer.
	ms_glApi.setLights                     = Direct3d11_LightManager::setLights;

	// Plan 11-04: VB + IB factory slots wired.
	// Plan 11-06: setVertexBuffer / setIndexBuffer slots now route through
	// the state cache (geometry tracking + draw-time binding).
	// setVertexBufferVector + createVertexBufferVectorData remain STUB() --
	// multi-stream is deferred (Phase 11 SPEC §Boundaries: single-stream).
	ms_glApi.createStaticVertexBufferData  = createStaticVertexBufferData_impl;
	ms_glApi.createDynamicVertexBufferData = createDynamicVertexBufferData_impl;
	STUB(createVertexBufferVectorData);
	ms_glApi.setVertexBuffer               = Direct3d11_StateCache::setVertexBuffer;
	STUB(setVertexBufferVector);

	ms_glApi.createStaticIndexBufferData   = createStaticIndexBufferData_impl;
	ms_glApi.createDynamicIndexBufferData  = createDynamicIndexBufferData_impl;
	ms_glApi.setIndexBuffer                = Direct3d11_StateCache::setIndexBuffer;
	ms_glApi.setDynamicIndexBufferSize     = setDynamicIndexBufferSize_impl;

	STUB(getOneToOneUVMapping);
	// Plan 11-04: createTextureData -- the slot that was the Plan 11-03
	// FATAL boundary. Now serves real ID3D11Texture2D + SRV pairs.
	ms_glApi.createTextureData             = createTextureData_impl;

	// Plan 11-05: VS + PS shader-program factories wired. The VS path
	// compiles HLSL source to vs_5_0 via D3DCompile + ShaderCache (D-03
	// hybrid). The PS path constructs the wrapper but currently leaves
	// the ID3D11PixelShader NULL because the asset pipeline ships
	// pre-compiled D3D9 bytecode (Plan 11-06 draw dispatch handles NULL).
	// See Direct3d11_PixelShaderProgramData.cpp header for the asset
	// re-author follow-up note.
	ms_glApi.createVertexShaderData       = createVertexShaderData_impl;
	ms_glApi.createPixelShaderProgramData = createPixelShaderProgramData_impl;

	// Plan 11-06: draw-call dispatch all wired to Direct3d11_StateCache.
	ms_glApi.drawPointList               = Direct3d11_StateCache::drawPointList;
	ms_glApi.drawLineList                = Direct3d11_StateCache::drawLineList;
	ms_glApi.drawLineStrip               = Direct3d11_StateCache::drawLineStrip;
	ms_glApi.drawTriangleList            = Direct3d11_StateCache::drawTriangleList;
	ms_glApi.drawTriangleStrip           = Direct3d11_StateCache::drawTriangleStrip;
	ms_glApi.drawTriangleFan             = Direct3d11_StateCache::drawTriangleFan;
	ms_glApi.drawQuadList                = Direct3d11_StateCache::drawQuadList;

	ms_glApi.drawIndexedPointList        = Direct3d11_StateCache::drawIndexedPointList;
	ms_glApi.drawIndexedLineList         = Direct3d11_StateCache::drawIndexedLineList;
	ms_glApi.drawIndexedLineStrip        = Direct3d11_StateCache::drawIndexedLineStrip;
	ms_glApi.drawIndexedTriangleList     = Direct3d11_StateCache::drawIndexedTriangleList;
	ms_glApi.drawIndexedTriangleStrip    = Direct3d11_StateCache::drawIndexedTriangleStrip;
	ms_glApi.drawIndexedTriangleFan      = Direct3d11_StateCache::drawIndexedTriangleFan;

	ms_glApi.drawPartialPointList        = Direct3d11_StateCache::drawPartialPointList;
	ms_glApi.drawPartialLineList         = Direct3d11_StateCache::drawPartialLineList;
	ms_glApi.drawPartialLineStrip        = Direct3d11_StateCache::drawPartialLineStrip;
	ms_glApi.drawPartialTriangleList     = Direct3d11_StateCache::drawPartialTriangleList;
	ms_glApi.drawPartialTriangleStrip    = Direct3d11_StateCache::drawPartialTriangleStrip;
	ms_glApi.drawPartialTriangleFan      = Direct3d11_StateCache::drawPartialTriangleFan;

	ms_glApi.drawPartialIndexedPointList     = Direct3d11_StateCache::drawPartialIndexedPointList;
	ms_glApi.drawPartialIndexedLineList      = Direct3d11_StateCache::drawPartialIndexedLineList;
	ms_glApi.drawPartialIndexedLineStrip     = Direct3d11_StateCache::drawPartialIndexedLineStrip;
	ms_glApi.drawPartialIndexedTriangleList  = Direct3d11_StateCache::drawPartialIndexedTriangleList;
	ms_glApi.drawPartialIndexedTriangleStrip = Direct3d11_StateCache::drawPartialIndexedTriangleStrip;
	ms_glApi.drawPartialIndexedTriangleFan   = Direct3d11_StateCache::drawPartialIndexedTriangleFan;

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
