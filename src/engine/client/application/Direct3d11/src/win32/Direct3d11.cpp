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
#include "Direct3d11_LegacyPSConstants.h"   // Plan 17-08 (GAP-4) Producer C: user constants -> b0 shadow @ c8+
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
#include "Direct3d11_VertexBufferVectorData.h"
#include "Direct3d11_VertexDeclarationMap.h"
#include "Direct3d11_VertexShaderData.h"

#include "clientGraphics/Gl_dll.def"
#include "clientGraphics/Graphics.h"
#include "clientGraphics/ShaderCapability.h"
#include "clientGraphics/ShaderImplementation.h"
#include "clientGraphics/StaticShader.h"
#include "clientGraphics/StaticVertexBuffer.h"
#include "clientGraphics/DynamicVertexBuffer.h"
#include "clientGraphics/StaticIndexBuffer.h"
#include "sharedDebug/DebugFlags.h"
#include "sharedFoundation/Os.h"
#include "sharedMath/VectorRgba.h"

#include "WriteTGA.h"
extern "C"
{
#include <jpeglib.h>
}

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
	// Plan 17-03 R3-03a + HIGH-3 (Option A): file-scope PerMaterialCB
	// shadow. Promoted from the old function-local static at the bottom of
	// setPixelShaderUserConstants_impl so BOTH that path AND
	// Direct3d11_StaticShaderData::apply()'s per-pass material upload can
	// read-modify-write into the SAME struct (most-recent write per field
	// wins; each path flushes the FULL struct via updatePS so partial
	// writes never leave garbage). The getter is DECLARED in
	// Direct3d11_ConstantBuffer.h next to the struct; its DEFINITION lives
	// here (where the shadow lives) and is exported through
	// `using namespace Direct3d11Namespace` at the bottom of this file.

	static Direct3d11_PerMaterialCB s_perMaterialShadow = {};

	Direct3d11_PerMaterialCB & getPerMaterialShadow()
	{
		return s_perMaterialShadow;
	}

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
	// Plan 17-08 (GAP-6 / D-08): enable MULTI-STREAM skinned-mesh path. These
	// two caps gate SoftwareBlendSkeletalShaderPrimitive::ms_useMultiStreamVertexBuffers
	// (set at install: maxStreamCount>1 && supportsStreamOffsets && !disableMultiStream).
	// With them false the engine forced single-stream, which places the skinned DOT3
	// tangent at a texcoord index the recompiled bump VS doesn't read as TEXCOORD2 ->
	// phantom-zero -> normalize(0) -> NaN COLOR0 -> wrong sleeves/hands. The D3D11
	// multi-stream bind path is already implemented (Plan 11-09.7
	// setVertexBufferVectorBindState + getOrCreateMultiStream + 11-09.8 phantom) and
	// the GAP-6 global-TEXCOORD-index fix routes stream-1 DOT3 -> TEXCOORD2. D3D11
	// IASetVertexBuffers natively supports per-stream offsets, so advertising both is
	// honest. getOrCreateMultiStream caps streamCount at 2 (constant + dynamic skinned).
	bool supportsStreamOffsets_impl()        { return true; }
	bool supportsDynamicTextures_impl()      { return false; }
	bool supportsAntialias_impl()            { return false; }

	int  getMaximumVertexBufferStreamCount_impl() { return 2; }

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
	// Plan 11-03: slots that fire BEFORE the per-frame loop
	// (Graphics::install line 320 setBrightnessContrastGamma) or every
	// frame BEFORE beginScene (Game::run line 1211 Graphics::update).
	//   - setBrightnessContrastGamma: GAMMA-01 (2026-06-11) -- real
	//     implementation in Direct3d11_Device: D3D11 has no
	//     IDirect3DDevice9::SetGammaRamp equivalent, so the D3D9 ramp curve
	//     (Direct3d9.cpp:2207) runs as a pre-Present full-screen pass over
	//     the back buffer. Identity (1,1,1) keeps it fully disabled.
	//   - update -> per-frame elapsed-time bookkeeping (DPVS-style timing,
	//     dynamic-vertex-buffer discard, etc.; Wave 4+ when resource layer
	//     and instrumentation arrive)

	void setBrightnessContrastGamma_impl(float brightness, float contrast, float gamma)
	{
		Direct3d11_Device::setBrightnessContrastGamma(brightness, contrast, gamma);
	}

	void update_impl(float /*elapsedTime*/)
	{
		// no-op in scaffold; Wave 4+ wires per-frame timing bookkeeping
	}

	// Plan 11-09.10: realize the design intent of the comment block at the
	// install-time wire-up below -- D3D11 has no fixed-function point-sprite
	// control (the D3DRS_POINTSIZE / D3DRS_POINTSPRITEENABLE / D3DRS_POINTSCALE*
	// render states D3D9 used were dropped in D3D10+). HLSL SV_POINTSIZE
	// could carry per-vertex point size, but only at Feature Level 9.x; we
	// run vs_4_0 / FL10+ since Plan 11-09.6. These slots no-op so the
	// engine's Graphics::setPointSize / setPointSize{Min,Max} /
	// setPointScale{Enable,Factor} / setPointSpriteEnable callers (notably
	// StarAppearance::draw for night-sky stars) return cleanly. POINTLIST
	// topology draws as 1-pixel hardware-default points (Direct3d11_StateCache
	// ::drawPointList is already wired). Visual fidelity for point sprites
	// (engine-requested 2-pixel stars, particle point-sprites) is deferred
	// to Plan 11-11 visual-parity; real emulation would require GS-side
	// quad expansion or CPU expansion to TRIANGLELIST.
	void setPointSize_impl(real /*size*/)                              {}
	void setPointSizeMax_impl(real /*size*/)                           {}
	void setPointSizeMin_impl(real /*size*/)                           {}
	void setPointScaleEnable_impl(bool /*bEnable*/)                    {}
	void setPointScaleFactor_impl(real /*A*/, real /*B*/, real /*C*/)  {}
	void setPointSpriteEnable_impl(bool /*bEnable*/)                   {}

	// Phase 24 (Plan 24-01) / D-07: Bloom is not implemented in the D3D11 plugin.
	// This no-op replaces the scaffold_fatal_stub binding for the setBloomEnabled
	// slot at install, so enabling Bloom no longer FATALs and the [ClientGame/Bloom]
	// enable=0 cfg override key becomes redundant (deleted in plan 24-03). One-shot
	// DEBUG_REPORT_LOG_PRINT noting Bloom is unimplemented; empty no-op body
	// otherwise (mirrors setPointSize_impl).
	void setBloomEnabled_impl(bool /*enabled*/)
	{
#ifdef _DEBUG
		static bool s_loggedBloomUnimplemented = false;
		if (!s_loggedBloomUnimplemented)
		{
			s_loggedBloomUnimplemented = true;
			DEBUG_REPORT_LOG_PRINT(true, ("[Direct3d11] setBloomEnabled: Bloom is unimplemented on D3D11; no-op (D-07).\n"));
		}
#endif
	}

	// Plan 11-09.11: PIX markers / events are pure runtime instrumentation
	// for the D3D PIX-for-Windows debugger. With no attached PIX session
	// they're silent no-ops in BOTH D3D9 and D3D11; the engine guards its
	// call sites with `#if PRODUCTION == 0` (Graphics::pixSetMarker at
	// Graphics.cpp:3409, Graphics::pixBeginEvent at 3417, Graphics::pixEndEvent
	// at 3428) so they only fire in Debug builds anyway. Plan 11-09.10 close
	// smoke surfaced the scaffold_fatal_stub via GroundScene::drawScene's
	// `pixSetMarker(L"CloseLoadingScreen")` (GroundScene.cpp:2217) -- the
	// first exercise of any pix* slot in a typical launch flow. Native
	// D3D11 PIX integration via ID3DUserDefinedAnnotation::SetMarker /
	// BeginEvent / EndEvent is deferred to Plan 11-12+ if PIX-debugging
	// becomes valuable for visual-parity diagnosis or perf work.
	//
	// Engine call-site audit (as of Plan 11-09.11 scaffold): pixSetMarker
	// has 2 callers (Game.cpp:1009 "Quit", GroundScene.cpp:2217
	// "CloseLoadingScreen"); pixBeginEvent / pixEndEvent have 0 callers in
	// the current source tree. We no-op all 3 for symmetry + future-proofing.
	void pixSetMarker_impl(WCHAR const * /*markerName*/)  {}
	void pixBeginEvent_impl(WCHAR const * /*eventName*/)  {}
	void pixEndEvent_impl(WCHAR const * /*eventName*/)    {}

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

	// Plan 11-09.7: multi-stream VertexBufferVector factory. Mirrors
	// Direct3d9Namespace::createVertexBufferVectorData (Direct3d9.cpp:3648).
	VertexBufferVectorGraphicsData * createVertexBufferVectorData_impl(VertexBufferVector const &vbVector)
	{
		return new Direct3d11_VertexBufferVectorData(vbVector);
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

	// Plan 11-09.15 Iter-41: screenShot implementation replacing the Plan 11-02
	// STUB. Mirrors the D3D9 sibling's flow (Direct3d9.cpp:2768) but uses the
	// D3D11 staging-texture readback pattern: walk back from the cached back-
	// buffer RTV to the underlying ID3D11Texture2D, create a USAGE_STAGING
	// copy, CopyResource, Map for CPU read, write the file. Back-buffer
	// format is DXGI_FORMAT_B8G8R8A8_UNORM (Direct3d11_Device.cpp:313) -- BGRA
	// matches WriteTGA's expectation and is one byte-swap away from libjpeg's
	// RGB input.
	bool screenShot_impl(GlScreenShotFormat format, int quality, char const *fileName)
	{
		ID3D11Device *        const device  = Direct3d11_Device::getDevice();
		ID3D11DeviceContext * const context = Direct3d11_Device::getContext();
		ID3D11RenderTargetView * const rtv  = Direct3d11_Device::getBackBufferRTV();
		if (!device || !context || !rtv)
		{
			Graphics::setLastError("engine", "screenshot_failed_unknown");
			return false;
		}

		Microsoft::WRL::ComPtr<ID3D11Resource> backResource;
		rtv->GetResource(backResource.GetAddressOf());
		Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
		if (!backResource || FAILED(backResource.As(&backBuffer)))
		{
			Graphics::setLastError("engine", "screenshot_failed_unknown");
			return false;
		}

		D3D11_TEXTURE2D_DESC desc = {};
		backBuffer->GetDesc(&desc);

		if (desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM)
		{
			// All current D3D11 swap-chain paths use BGRA8 (per Direct3d11_Device.cpp:313).
			// A different format would need per-format channel-shuffle logic.
			Graphics::setLastError("engine", "screenshot_failed_wrong_format");
			return false;
		}

		D3D11_TEXTURE2D_DESC stagingDesc = desc;
		stagingDesc.Usage          = D3D11_USAGE_STAGING;
		stagingDesc.BindFlags      = 0;
		stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		stagingDesc.MiscFlags      = 0;
		stagingDesc.MipLevels      = 1;
		stagingDesc.ArraySize      = 1;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
		HRESULT hr = device->CreateTexture2D(&stagingDesc, nullptr, staging.GetAddressOf());
		if (FAILED(hr) || !staging)
		{
			Graphics::setLastError("engine", "screenshot_failed_unknown");
			return false;
		}

		context->CopyResource(staging.Get(), backBuffer.Get());

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		hr = context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
		if (FAILED(hr) || !mapped.pData)
		{
			Graphics::setLastError("engine", "screenshot_failed_unknown");
			return false;
		}

		int const width  = static_cast<int>(desc.Width);
		int const height = static_cast<int>(desc.Height);
		uint8 const * const src = static_cast<uint8 const *>(mapped.pData);
		UINT const srcPitch = mapped.RowPitch;

		bool ok = false;
		char buffer[Os::MAX_PATH_LENGTH] = {};

		switch (format)
		{
			case GSSF_tga:
			{
				sprintf(buffer, "%s.tga", fileName);
				WriteTGA::write(buffer, width, height, src, true, static_cast<int>(srcPitch));
				ok = true;
				break;
			}

			case GSSF_jpg:
			{
				sprintf(buffer, "%s.jpg", fileName);
				FILE *outputFile = fopen(buffer, "wb");
				if (outputFile)
				{
					jpeg_compress_struct cinfo;
					jpeg_error_mgr jerr;
					cinfo.err = jpeg_std_error(&jerr);
					jpeg_create_compress(&cinfo);
					jpeg_stdio_dest(&cinfo, outputFile);
					cinfo.image_width      = width;
					cinfo.image_height     = height;
					cinfo.input_components = 3;
					cinfo.in_color_space   = JCS_RGB;
					jpeg_set_defaults(&cinfo);
					if (quality > 0)
					{
						// 24 WR-04: the (q < 0) branch was dead -- q == quality is already > 0 here.
						// Keep the upper clamp; quality <= 0 still means "use libjpeg's jpeg_set_defaults value".
						int q = quality;
						if (q > 100) q = 100;
						jpeg_set_quality(&cinfo, q, TRUE);
					}
					jpeg_start_compress(&cinfo, TRUE);

					std::vector<uint8> scanLine(static_cast<size_t>(width) * 3u);
					uint8 const *row = src;
					for (int y = 0; y < height; ++y, row += srcPitch)
					{
						uint32 const *source = reinterpret_cast<uint32 const *>(row);
						for (int x = 0, b = 0; x < width; ++x)
						{
							uint32 const pixel = source[x];  // BGRA in mapped order
							scanLine[b++] = static_cast<uint8>((pixel >> 16) & 0xff);  // R
							scanLine[b++] = static_cast<uint8>((pixel >>  8) & 0xff);  // G
							scanLine[b++] = static_cast<uint8>((pixel >>  0) & 0xff);  // B
						}
						JSAMPROW rowPtr = scanLine.data();
						jpeg_write_scanlines(&cinfo, &rowPtr, 1);
					}

					jpeg_finish_compress(&cinfo);
					jpeg_destroy_compress(&cinfo);
					fclose(outputFile);
					ok = true;
				}
				else
				{
					Graphics::setLastError("engine", "screenshot_failed_write_problem");
				}
				break;
			}

			case GSSF_bmp:
			{
				// Hand-rolled BGR 24bpp uncompressed BMP. The D3D9 sibling uses
				// D3DXSaveSurfaceToFile which is not available in D3D11; WIC
				// would be the modern equivalent but adds a new dependency.
				// Uncompressed BMP is ~30 lines and has no external deps.
				sprintf(buffer, "%s.bmp", fileName);
				FILE *outputFile = fopen(buffer, "wb");
				if (outputFile)
				{
					int const rowBytes      = width * 3;
					int const paddedRowBytes = (rowBytes + 3) & ~3;
					uint32 const pixelBytes  = static_cast<uint32>(paddedRowBytes) * static_cast<uint32>(height);
					uint32 const fileSize    = 14u + 40u + pixelBytes;

					uint8 fileHeader[14] = {};
					fileHeader[0] = 'B';
					fileHeader[1] = 'M';
					fileHeader[2] = static_cast<uint8>(fileSize       & 0xff);
					fileHeader[3] = static_cast<uint8>((fileSize >>  8) & 0xff);
					fileHeader[4] = static_cast<uint8>((fileSize >> 16) & 0xff);
					fileHeader[5] = static_cast<uint8>((fileSize >> 24) & 0xff);
					fileHeader[10] = 54;  // pixel data offset (14 + 40)

					uint8 infoHeader[40] = {};
					infoHeader[0] = 40;   // header size
					uint32 const w = static_cast<uint32>(width);
					uint32 const h = static_cast<uint32>(height);
					infoHeader[4]  = static_cast<uint8>( w        & 0xff);
					infoHeader[5]  = static_cast<uint8>((w >>  8) & 0xff);
					infoHeader[6]  = static_cast<uint8>((w >> 16) & 0xff);
					infoHeader[7]  = static_cast<uint8>((w >> 24) & 0xff);
					infoHeader[8]  = static_cast<uint8>( h        & 0xff);
					infoHeader[9]  = static_cast<uint8>((h >>  8) & 0xff);
					infoHeader[10] = static_cast<uint8>((h >> 16) & 0xff);
					infoHeader[11] = static_cast<uint8>((h >> 24) & 0xff);
					infoHeader[12] = 1;   // planes
					infoHeader[14] = 24;  // bits per pixel
					// compression = 0 (BI_RGB), bytes [16..19] image size (can be 0 for BI_RGB)

					fwrite(fileHeader, 1, sizeof(fileHeader), outputFile);
					fwrite(infoHeader, 1, sizeof(infoHeader), outputFile);

					// BMP is BOTTOM-UP. Write last row first.
					std::vector<uint8> rowBuf(static_cast<size_t>(paddedRowBytes), 0);
					for (int y = height - 1; y >= 0; --y)
					{
						uint8 const *srcRow = src + static_cast<size_t>(y) * static_cast<size_t>(srcPitch);
						uint8 *dstRow = rowBuf.data();
						uint32 const *sp = reinterpret_cast<uint32 const *>(srcRow);
						for (int x = 0; x < width; ++x)
						{
							uint32 const pixel = sp[x];  // BGRA
							*dstRow++ = static_cast<uint8>((pixel >>  0) & 0xff);  // B
							*dstRow++ = static_cast<uint8>((pixel >>  8) & 0xff);  // G
							*dstRow++ = static_cast<uint8>((pixel >> 16) & 0xff);  // R
						}
						// Trailing bytes already zero from rowBuf initial fill.
						fwrite(rowBuf.data(), 1, static_cast<size_t>(paddedRowBytes), outputFile);
					}

					fclose(outputFile);
					ok = true;
				}
				else
				{
					Graphics::setLastError("engine", "screenshot_failed_write_problem");
				}
				break;
			}

			default:
			{
				Graphics::setLastError("engine", "screenshot_failed_unknown_format");
				break;
			}
		}

		context->Unmap(staging.Get(), 0);
		return ok;
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
		//
		// Plan 17-03 R3-03a + HIGH-3 (Option A): the shadow is now the
		// FILE-SCOPE s_perMaterialShadow promoted to namespace scope above,
		// shared with Direct3d11_StaticShaderData::apply()'s per-pass
		// material upload via Direct3d11Namespace::getPerMaterialShadow().
		// Both paths read-modify-write the same struct and flush the FULL
		// struct via updatePS; the most-recent write per field wins for
		// each flush. The previous function-local static here was a
		// cross-clobber hazard waiting to surface (Iter-45 wired the
		// userConstants flush but left the material* fields silently
		// zero on every call — Plan 17-03 fixes both).
		Direct3d11_PerMaterialCB & shadow = getPerMaterialShadow();
		int const slots = sizeof(shadow.userConstants) / sizeof(shadow.userConstants[0]);
		int const clamped = (count > slots) ? slots : count;
		for (int i = 0; i < clamped; ++i)
		{
			shadow.userConstants[i] = DirectX::XMFLOAT4(
				constants[i].r, constants[i].g, constants[i].b, constants[i].a);
		}
		if (count > slots)
		{
			DEBUG_REPORT_LOG_PRINT(true,
				("Direct3d11::setPixelShaderUserConstants: count=%d > userConstants[] slots=%d; truncated\n",
				count, slots));
		}

		// Plan 17-08 (GAP-4) Producer C: ALSO mirror the user constants into the
		// legacy b0 SwgVertexConstants shadow at c8+ (PSCR_userConstant + i) --
		// the slot where the recompiled asset PS actually reads them (the b2
		// PerMaterialCB flush below is retained for any non-asset path). The b0
		// user region holds 17 float4 (c8..c24), independent of the (smaller)
		// PerMaterialCB userConstants[] slot count. byte offset = (8 + i) * 16.
		{
			using namespace Direct3d11_LegacyPSConstantRegisters;
			int const b0UserSlots = Direct3d11_LegacyPSConstants::kRegisterCount - PSCR_userConstant; // 17
			int const b0Clamped   = (count > b0UserSlots) ? b0UserSlots : count;
			for (int i = 0; i < b0Clamped; ++i)
			{
				Direct3d11_LegacyPSConstants::writeFloat4(
					PSCR_userConstant + i,
					DirectX::XMFLOAT4(constants[i].r, constants[i].g, constants[i].b, constants[i].a));
			}
		}

		// Plan 17-03 R3 SUB-STEP 1g (HIGH-3 cross-clobber regression check):
		// one-shot DEBUG_REPORT_LOG_PRINT + InfoQueue dual-route showing
		// non-zero coexistence of userConstants + material* fields on the
		// SAME shadow at the SAME flush. Proves both code paths read-modify-
		// write the SAME struct without zeroing each other's fields.
		// `#if 0`'d-out after the first successful boot is recorded.
#if 1
		{
			static bool s_loggedOnce = false;
			if (!s_loggedOnce)
			{
				s_loggedOnce = true;
				char msg[512];
				_snprintf_s(msg, sizeof(msg), _TRUNCATE,
					"Plan 17-03 R3 SUB-STEP 1g shared-shadow coexistence at setPixelShaderUserConstants flush: "
					"userConstants[0]=(%.3f,%.3f,%.3f,%.3f) "
					"materialDiffuse=(%.3f,%.3f,%.3f,%.3f) "
					"materialSpecular=(%.3f,%.3f,%.3f,%.3f) "
					"materialEmissive=(%.3f,%.3f,%.3f,%.3f)",
					shadow.userConstants[0].x, shadow.userConstants[0].y, shadow.userConstants[0].z, shadow.userConstants[0].w,
					shadow.materialDiffuse.x,  shadow.materialDiffuse.y,  shadow.materialDiffuse.z,  shadow.materialDiffuse.w,
					shadow.materialSpecular.x, shadow.materialSpecular.y, shadow.materialSpecular.z, shadow.materialSpecular.w,
					shadow.materialEmissive.x, shadow.materialEmissive.y, shadow.materialEmissive.z, shadow.materialEmissive.w);
				if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
					iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, msg);
				DEBUG_REPORT_LOG_PRINT(true, ("%s\n", msg));
			}
		}
#endif

		// Plan 11-09.15 Iter-45 + Plan 17-03: flush the PerMaterialCB shadow
		// to PS slot 2. The slot-2 binding is the engine's PerMaterialCB
		// convention (Direct3d11_HlslRewrite.cpp:797; 11-05-SUMMARY); this
		// remains hardcoded as slot 2 because userConstants always belong
		// to that binding by engine convention (it is NOT a reflected
		// lookup — Plan 17-03's reflection-driven path lives in
		// Direct3d11_StaticShaderData::apply at layout.BindPoint).
		// Material diffuse/specular/emissive in `shadow` are populated by
		// Plan 17-03's apply() path when the current draw uses a material
		// AND its reflected cbuffer is bound at slot 2; otherwise they
		// remain at their last value (per pass `apply` zeros them on
		// non-material passes per R3-03b so stale data does not bleed).
		Direct3d11_ConstantBuffer::updatePS(2, &shadow, sizeof(shadow));
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
	// gl_PointSize-equivalent lives in HLSL via SV_PointSize (FL9.x only;
	// we run vs_4_0 / FL10+). Plan 11-09.10 wires these as inline no-ops --
	// the earlier Plan 11-06 STUB(...) routes redirected to scaffold_fatal_stub
	// (FATAL), not no-op, which Plan 11-09.9 close smoke surfaced via
	// StarAppearance::draw's setPointSize(2.f) call for night-sky stars.
	// Stars + particle point sprites now render at 1-pixel hardware default;
	// Plan 11-11 visual-parity decides whether real emulation (GS quad
	// expansion or CPU point-to-quad) is needed for fidelity.
	ms_glApi.setPointSize         = setPointSize_impl;
	ms_glApi.setPointSizeMax      = setPointSizeMax_impl;
	ms_glApi.setPointSizeMin      = setPointSizeMin_impl;
	ms_glApi.setPointScaleEnable  = setPointScaleEnable_impl;
	ms_glApi.setPointScaleFactor  = setPointScaleFactor_impl;
	ms_glApi.setPointSpriteEnable = setPointSpriteEnable_impl;

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

	// Plan 11-09.15 Iter-41: real screenShot binding (Print Screen keybind via
	// DirectInput::KeyboardDevice DIK_SYSRQ -> ScreenShotHelper::screenShot).
	// Writes ./screenshots/screenShotNNNN.{tga|jpg|bmp} per Graphics's
	// ms_screenShotFormat (default JPG -- Graphics.cpp:132).
	ms_glApi.screenShot = screenShot_impl;

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
	// Plan 11-09.7: multi-stream slots wired -- skeletal-animation
	// rendering (SoftwareBlendSkeletalShaderPrimitive) consumes these
	// during char-customization preview + world entry.
	ms_glApi.createStaticVertexBufferData   = createStaticVertexBufferData_impl;
	ms_glApi.createDynamicVertexBufferData  = createDynamicVertexBufferData_impl;
	ms_glApi.createVertexBufferVectorData   = createVertexBufferVectorData_impl;
	ms_glApi.setVertexBuffer                = Direct3d11_StateCache::setVertexBuffer;
	ms_glApi.setVertexBufferVector          = Direct3d11_VertexBufferVectorData::bind;

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

	// Phase 24 (Plan 24-01) / D-07: bind setBloomEnabled to the non-fatal no-op
	// _impl (was the fatal-stub binding) so enabling Bloom no longer FATALs.
	ms_glApi.setBloomEnabled = setBloomEnabled_impl;

	// Plan 11-09.11: PIX instrumentation slots as inline no-ops. See the
	// _impl block above for the design rationale (Graphics.cpp guards calls
	// with `#if PRODUCTION == 0`; PIX-for-Windows is silent without an
	// attached session; native ID3DUserDefinedAnnotation wire-up deferred
	// to Plan 11-12+).
	ms_glApi.pixSetMarker  = pixSetMarker_impl;
	ms_glApi.pixBeginEvent = pixBeginEvent_impl;
	ms_glApi.pixEndEvent   = pixEndEvent_impl;

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
