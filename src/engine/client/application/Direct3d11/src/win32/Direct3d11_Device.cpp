// ======================================================================
//
// Direct3d11_Device.cpp
// Phase 11 D3D11 renderer plugin -- device + flip-model swap chain +
// per-frame slot bodies (clear-to-color MVP).
//
// Plan 11-03 (Wave 3). Implements:
//   - D3D_FEATURE_LEVEL_11_0 baseline ID3D11Device + immediate context
//   - DXGI_SWAP_EFFECT_FLIP_DISCARD swap chain (2 buffers, BGRA8)
//   - Back-buffer RTV + DXGI_FORMAT_D24_UNORM_S8_UINT depth-stencil DSV
//   - beginScene (rebinds RTV+DSV unconditionally per RESEARCH Pitfall 3)
//   - endScene (no-op -- D3D11 has no batched-state-flush)
//   - clearViewport (dark blue MVP; argb arg ignored until Wave 4)
//   - present (vsync=1; FATAL on DEVICE_REMOVED per D-13 / SPEC Boundaries)
//   - displayModeChanged (ResizeBuffers + recreate RTV/DSV)
//   - Debug-layer detect-and-fallback per RESEARCH Pitfall 8
//
// Per CONTEXT D-13: NO D3DPOOL_MANAGED, NO OnLostDevice, NO OnResetDevice.
// DXGI flip model has no lost-device concept; DEVICE_REMOVED from Present()
// is a process-restart class event (no recovery attempt).
//
// ======================================================================

#include "FirstDirect3d11.h"
#include "Direct3d11_Device.h"

#include "ConfigDirect3d11.h"
#include "Direct3d11.h"   // for FATAL_DX_HR + hresultString
#include "Direct3d11_StateCache.h"   // Plan 11-09.15 Iter-8: route beginScene viewport setup through setViewport so cb0[9] viewportData lands in slot0 before the first draw

#include "sharedDebug/DebugFlags.h"
#include "sharedFoundation/Os.h"

#include <d3d11.h>
#include <d3d11sdklayers.h>   // ID3D11InfoQueue (Plan 11-08 Iter-1 safety net)
#include <d3dcompiler.h>      // GAMMA-01: compile the brightness/contrast/gamma pass shaders
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <cmath>              // fabsf for GAMMA-01 identity detection
#include <cstring>            // strlen/memcpy for GAMMA-01 shader compile + cbuffer update

#include <cstdio>             // snprintf for InfoQueue category/severity formatting; fopen for Iter-1.7 file sink
#include <ctime>              // time_t / localtime_s / strftime for Iter-1.7 session header
#include <climits>            // INT_MAX sentinel for ms_windowX/Y (Plan 11-09.5)

// ======================================================================

using Microsoft::WRL::ComPtr;

namespace Direct3d11_DeviceNamespace
{
	// ------------------------------------------------------------------
	// Namespace state (ms_-prefixed per PATTERNS §Shared 3).

	bool ms_installed = false;
	HWND ms_window    = nullptr;
	int  ms_width     = 0;
	int  ms_height    = 0;
	bool ms_windowed  = true;

	// Plan 11-09.5: window-position state mirroring Direct3d9Namespace.
	// INT_MAX sentinel triggers monitor-center fallback in updateWindowSettings
	// (matches Direct3d9.cpp:1906-1910 behavior).
	bool ms_engineOwnsWindow = false;
	int  ms_windowX          = INT_MAX;
	int  ms_windowY          = INT_MAX;
	bool ms_borderlessWindow = false;

	ComPtr<ID3D11Device>           ms_device;
	ComPtr<ID3D11DeviceContext>    ms_context;
	ComPtr<IDXGISwapChain1>        ms_swapChain;
	ComPtr<ID3D11RenderTargetView> ms_backBufferRTV;
	ComPtr<ID3D11Texture2D>        ms_depthStencilTex;
	ComPtr<ID3D11DepthStencilView> ms_depthStencilDSV;

	// Plan 11-09.8: 16-byte zero-filled BIND_VERTEX_BUFFER created in
	// Direct3d11_Device::create after the device is alive. Bound at
	// InputSlot=15 by Direct3d11_StateCache::applyPreDrawState with
	// stride=0 / offset=0 -- backs reflection-driven phantom layout
	// elements for VS-declared semantics absent from the VBFormat.
	ComPtr<ID3D11Buffer>           ms_phantomZeroBuffer;

	// Plan 11-08 Iter-1: D3D11 debug-layer InfoQueue. Non-null only when
	// the device was created with D3D11_CREATE_DEVICE_DEBUG (i.e. _DEBUG
	// build AND ConfigDirect3d11::getEnableDebugLayer() AND the Pitfall 8
	// fallback did NOT strip the DEBUG bit). drainInfoQueue() guards on
	// this and is a no-op when null.
	ComPtr<ID3D11InfoQueue>        ms_infoQueue;

	// Phase 19 (19-04 Task 0) DEV-ONLY value-dump frame counter. Monotonic,
	// incremented once per drainInfoQueue() (per-frame boundary). Tags the
	// neutral b0 / setLights dumps so a value's frame-to-frame variation is
	// unambiguous across StaticShaderData.cpp + LightManager.cpp. REVERT after
	// the cross-frame light-feed question is answered (P19_LIGHTDUMP).
	unsigned                       s_diagFrameIndex = 0u;

	// Plan 11-08 Iter-1.7: file sink for InfoQueue messages. Opened in
	// append mode at create() time when ms_infoQueue is acquired so a
	// crash mid-frame doesn't lose any messages drained since the
	// previous launch. NULL when the InfoQueue is not live OR fopen
	// failed; drainInfoQueue() guards on it. Engine-side
	// DEBUG_REPORT_LOG_PRINT still runs in parallel for live debugger
	// observation; this sink exists because that route goes to
	// OutputDebugString + stdout/stderr which are invisible when
	// SwgClient_d.exe is launched from explorer.
	FILE *                         ms_d3d11LogFile = nullptr;

	// ------------------------------------------------------------------
	// GAMMA-01: brightness/contrast/gamma state + lazily-created resources
	// for the pre-Present full-screen curve pass (D3D9 SetGammaRamp parity,
	// Direct3d9.cpp:2198-2216). All null until the first non-identity
	// setBrightnessContrastGamma call; identity keeps ms_bcgActive false
	// and present() skips the pass entirely.

	float ms_bcgBrightness = 1.0f;
	float ms_bcgContrast   = 1.0f;
	float ms_bcgGamma      = 1.0f;
	bool  ms_bcgActive     = false;
	bool  ms_bcgInitFailed = false;   // compile/create failed once -- don't retry per frame

	ComPtr<ID3D11VertexShader>       ms_bcgVS;
	ComPtr<ID3D11PixelShader>        ms_bcgPS;
	ComPtr<ID3D11Buffer>             ms_bcgCB;
	ComPtr<ID3D11SamplerState>       ms_bcgSampler;
	ComPtr<ID3D11Texture2D>          ms_bcgTempTex;   // back-buffer copy (CopyResource source can't be the RTV target)
	ComPtr<ID3D11ShaderResourceView> ms_bcgTempSRV;
	UINT ms_bcgTempWidth  = 0;
	UINT ms_bcgTempHeight = 0;

	// Fullscreen triangle via SV_VertexID (no VB / no input layout). The PS
	// applies the exact D3D9 ramp curve (Direct3d9.cpp:2207); saturate before
	// pow avoids the NaN a negative base would produce (D3D9's int clamp
	// covered the same range).
	char const * const cms_bcgShaderSource =
		"Texture2D srcTex : register(t0);\n"
		"SamplerState srcSamp : register(s0);\n"
		"cbuffer BcgCB : register(b0) { float4 bcg; }\n"   // x=brightness y=contrast z=1/gamma w=unused
		"struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };\n"
		"VSOut vsMain(uint id : SV_VertexID)\n"
		"{\n"
		"	VSOut o;\n"
		"	float2 uv = float2((id << 1) & 2, id & 2);\n"
		"	o.pos = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);\n"
		"	o.uv = uv;\n"
		"	return o;\n"
		"}\n"
		"float4 psMain(VSOut i) : SV_Target\n"
		"{\n"
		"	float3 c = srcTex.Sample(srcSamp, i.uv).rgb;\n"
		"	c = pow(saturate(0.5 + bcg.y * (c * bcg.x - 0.5)), bcg.z);\n"
		"	return float4(c, 1.0);\n"
		"}\n";

	bool createBcgResources()
	{
		ComPtr<ID3DBlob> blob;
		ComPtr<ID3DBlob> errors;

		HRESULT hr = D3DCompile(cms_bcgShaderSource, strlen(cms_bcgShaderSource), "bcg_pass",
			nullptr, nullptr, "vsMain", "vs_5_0", 0, 0, blob.GetAddressOf(), errors.GetAddressOf());
		if (FAILED(hr))
		{
			DEBUG_REPORT_LOG_PRINT(true, ("Direct3d11_Device: GAMMA-01 vsMain compile failed: %s\n",
				errors ? static_cast<char const *>(errors->GetBufferPointer()) : "<no log>"));
			return false;
		}
		hr = ms_device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &ms_bcgVS);
		if (FAILED(hr))
			return false;

		blob.Reset();
		errors.Reset();
		hr = D3DCompile(cms_bcgShaderSource, strlen(cms_bcgShaderSource), "bcg_pass",
			nullptr, nullptr, "psMain", "ps_5_0", 0, 0, blob.GetAddressOf(), errors.GetAddressOf());
		if (FAILED(hr))
		{
			DEBUG_REPORT_LOG_PRINT(true, ("Direct3d11_Device: GAMMA-01 psMain compile failed: %s\n",
				errors ? static_cast<char const *>(errors->GetBufferPointer()) : "<no log>"));
			return false;
		}
		hr = ms_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &ms_bcgPS);
		if (FAILED(hr))
			return false;

		D3D11_SAMPLER_DESC sd = {};
		sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_POINT;
		sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		hr = ms_device->CreateSamplerState(&sd, &ms_bcgSampler);
		if (FAILED(hr))
			return false;

		D3D11_BUFFER_DESC bd = {};
		bd.ByteWidth      = 16;
		bd.Usage          = D3D11_USAGE_DYNAMIC;
		bd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		hr = ms_device->CreateBuffer(&bd, nullptr, &ms_bcgCB);
		return SUCCEEDED(hr);
	}

	// Runs the curve pass: back buffer -> temp copy -> draw back through the
	// curve PS. Saves and restores every piece of context state it touches so
	// the StateCache's view of bound state stays truthful.
	void applyBcgPass()
	{
		if (!ms_bcgActive || ms_bcgInitFailed || !ms_context || !ms_swapChain || !ms_backBufferRTV)
			return;

		if (!ms_bcgVS)
		{
			if (!createBcgResources())
			{
				ms_bcgInitFailed = true;
				return;
			}
			DEBUG_REPORT_LOG_PRINT(true, ("Direct3d11_Device: GAMMA-01 pass resources created\n"));
		}

		ComPtr<ID3D11Texture2D> backBuffer;
		if (FAILED(ms_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
			return;

		D3D11_TEXTURE2D_DESC bbDesc;
		backBuffer->GetDesc(&bbDesc);

		if (!ms_bcgTempTex || ms_bcgTempWidth != bbDesc.Width || ms_bcgTempHeight != bbDesc.Height)
		{
			ms_bcgTempSRV.Reset();
			ms_bcgTempTex.Reset();

			D3D11_TEXTURE2D_DESC td = bbDesc;
			td.BindFlags      = D3D11_BIND_SHADER_RESOURCE;
			td.MiscFlags      = 0;
			td.CPUAccessFlags = 0;
			td.Usage          = D3D11_USAGE_DEFAULT;
			if (FAILED(ms_device->CreateTexture2D(&td, nullptr, &ms_bcgTempTex))
				|| FAILED(ms_device->CreateShaderResourceView(ms_bcgTempTex.Get(), nullptr, &ms_bcgTempSRV)))
			{
				ms_bcgTempSRV.Reset();
				ms_bcgTempTex.Reset();
				ms_bcgInitFailed = true;
				return;
			}
			ms_bcgTempWidth  = bbDesc.Width;
			ms_bcgTempHeight = bbDesc.Height;
		}

		ms_context->CopyResource(ms_bcgTempTex.Get(), backBuffer.Get());

		D3D11_MAPPED_SUBRESOURCE mapped;
		if (SUCCEEDED(ms_context->Map(ms_bcgCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			float const vals[4] = { ms_bcgBrightness, ms_bcgContrast, 1.0f / ms_bcgGamma, 0.0f };
			memcpy(mapped.pData, vals, sizeof(vals));
			ms_context->Unmap(ms_bcgCB.Get(), 0);
		}

		// -- save the state the pass touches (Get* adds refs; released below)
		ID3D11RenderTargetView   *oldRTV = nullptr;
		ID3D11DepthStencilView   *oldDSV = nullptr;
		ID3D11RasterizerState    *oldRS  = nullptr;
		ID3D11BlendState         *oldBlend = nullptr;
		FLOAT                     oldBlendFactor[4];
		UINT                      oldSampleMask = 0xffffffffu;
		ID3D11DepthStencilState  *oldDSS = nullptr;
		UINT                      oldStencilRef = 0;
		ID3D11InputLayout        *oldLayout = nullptr;
		D3D11_PRIMITIVE_TOPOLOGY  oldTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
		ID3D11VertexShader       *oldVS = nullptr;
		ID3D11PixelShader        *oldPS = nullptr;
		ID3D11ShaderResourceView *oldSRV0 = nullptr;
		ID3D11SamplerState       *oldSamp0 = nullptr;
		ID3D11Buffer             *oldPSCB0 = nullptr;
		D3D11_VIEWPORT            oldViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
		UINT                      oldViewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;

		ms_context->OMGetRenderTargets(1, &oldRTV, &oldDSV);
		ms_context->RSGetState(&oldRS);
		ms_context->OMGetBlendState(&oldBlend, oldBlendFactor, &oldSampleMask);
		ms_context->OMGetDepthStencilState(&oldDSS, &oldStencilRef);
		ms_context->IAGetInputLayout(&oldLayout);
		ms_context->IAGetPrimitiveTopology(&oldTopology);
		ms_context->VSGetShader(&oldVS, nullptr, nullptr);
		ms_context->PSGetShader(&oldPS, nullptr, nullptr);
		ms_context->PSGetShaderResources(0, 1, &oldSRV0);
		ms_context->PSGetSamplers(0, 1, &oldSamp0);
		ms_context->PSGetConstantBuffers(0, 1, &oldPSCB0);
		ms_context->RSGetViewports(&oldViewportCount, oldViewports);

		// -- run the pass
		ID3D11RenderTargetView *rtv = ms_backBufferRTV.Get();
		ms_context->OMSetRenderTargets(1, &rtv, nullptr);

		D3D11_VIEWPORT vp = {};
		vp.Width    = static_cast<FLOAT>(bbDesc.Width);
		vp.Height   = static_cast<FLOAT>(bbDesc.Height);
		vp.MaxDepth = 1.0f;
		ms_context->RSSetViewports(1, &vp);

		FLOAT const blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		ms_context->RSSetState(nullptr);
		ms_context->OMSetBlendState(nullptr, blendFactor, 0xffffffffu);
		ms_context->OMSetDepthStencilState(nullptr, 0);
		ms_context->IASetInputLayout(nullptr);
		ms_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		ms_context->VSSetShader(ms_bcgVS.Get(), nullptr, 0);
		ms_context->PSSetShader(ms_bcgPS.Get(), nullptr, 0);
		ID3D11ShaderResourceView *srv = ms_bcgTempSRV.Get();
		ms_context->PSSetShaderResources(0, 1, &srv);
		ID3D11SamplerState *samp = ms_bcgSampler.Get();
		ms_context->PSSetSamplers(0, 1, &samp);
		ID3D11Buffer *cb = ms_bcgCB.Get();
		ms_context->PSSetConstantBuffers(0, 1, &cb);

		ms_context->Draw(3, 0);

		// -- restore + release the Get* refs
		ms_context->OMSetRenderTargets(1, &oldRTV, oldDSV);
		ms_context->RSSetState(oldRS);
		ms_context->OMSetBlendState(oldBlend, oldBlendFactor, oldSampleMask);
		ms_context->OMSetDepthStencilState(oldDSS, oldStencilRef);
		ms_context->IASetInputLayout(oldLayout);
		ms_context->IASetPrimitiveTopology(oldTopology);
		ms_context->VSSetShader(oldVS, nullptr, 0);
		ms_context->PSSetShader(oldPS, nullptr, 0);
		ms_context->PSSetShaderResources(0, 1, &oldSRV0);
		ms_context->PSSetSamplers(0, 1, &oldSamp0);
		ms_context->PSSetConstantBuffers(0, 1, &oldPSCB0);
		if (oldViewportCount)
			ms_context->RSSetViewports(oldViewportCount, oldViewports);

		if (oldRTV)    oldRTV->Release();
		if (oldDSV)    oldDSV->Release();
		if (oldRS)     oldRS->Release();
		if (oldBlend)  oldBlend->Release();
		if (oldDSS)    oldDSS->Release();
		if (oldLayout) oldLayout->Release();
		if (oldVS)     oldVS->Release();
		if (oldPS)     oldPS->Release();
		if (oldSRV0)   oldSRV0->Release();
		if (oldSamp0)  oldSamp0->Release();
		if (oldPSCB0)  oldPSCB0->Release();
	}

	// ------------------------------------------------------------------
	// Helper: create back-buffer RTV + depth-stencil texture + DSV.
	// Used by create() and displayModeChanged().

	bool createBackBufferViews(int width, int height)
	{
		// Acquire back buffer from swap chain.
		ComPtr<ID3D11Texture2D> backBuffer;
		HRESULT hr = ms_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
		FATAL_DX_HR("IDXGISwapChain1::GetBuffer(0) failed: %s", hr);

		// Back-buffer RTV.
		hr = ms_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &ms_backBufferRTV);
		FATAL_DX_HR("ID3D11Device::CreateRenderTargetView (back buffer) failed: %s", hr);

		// Depth-stencil texture.
		D3D11_TEXTURE2D_DESC dsDesc = {};
		dsDesc.Width      = static_cast<UINT>(width);
		dsDesc.Height     = static_cast<UINT>(height);
		dsDesc.MipLevels  = 1;
		dsDesc.ArraySize  = 1;
		dsDesc.Format     = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsDesc.SampleDesc.Count   = 1;
		dsDesc.SampleDesc.Quality = 0;
		dsDesc.Usage      = D3D11_USAGE_DEFAULT;
		dsDesc.BindFlags  = D3D11_BIND_DEPTH_STENCIL;
		dsDesc.CPUAccessFlags = 0;
		dsDesc.MiscFlags  = 0;
		hr = ms_device->CreateTexture2D(&dsDesc, nullptr, &ms_depthStencilTex);
		FATAL_DX_HR("ID3D11Device::CreateTexture2D (depth-stencil) failed: %s", hr);

		// Depth-stencil view.
		hr = ms_device->CreateDepthStencilView(ms_depthStencilTex.Get(), nullptr, &ms_depthStencilDSV);
		FATAL_DX_HR("ID3D11Device::CreateDepthStencilView failed: %s", hr);

		return true;
	}
}

using namespace Direct3d11_DeviceNamespace;

// ======================================================================
// Direct3d11_Device::create -- device + swap chain + back-buffer + DSV.
//
// Pattern source: RESEARCH §Architecture Patterns Pattern 1 (lines 173-235)
// Pitfall 8: detect-and-fallback if D3D11_CREATE_DEVICE_DEBUG fails with
//            DXGI_ERROR_SDK_COMPONENT_MISSING (Graphics Tools optional
//            feature missing). Log a hint, retry without DEBUG flag, no
//            FATAL.
// SPEC §Constraints: D3D_FEATURE_LEVEL_11_0 minimum -- FATAL if driver
// downgrades.

bool Direct3d11_Device::create(HWND hwnd, int width, int height, bool windowed, bool engineOwnsWindow)
{
	if (ms_installed)
	{
		DEBUG_REPORT_LOG_PRINT(true, ("Direct3d11_Device::create called twice; ignoring second call.\n"));
		return true;
	}

	ms_window           = hwnd;
	ms_width            = width;
	ms_height           = height;
	ms_windowed         = windowed;
	ms_engineOwnsWindow = engineOwnsWindow;

	// ------------------------------------------------------------------
	// Step 1: Create ID3D11Device + immediate context.

	UINT createDeviceFlags = 0;

	// CONSULT-51 (2026-06-27): DEFEAT THE NV DRIVER WORKER-THREAD RACE.
	// The nvwgf2um/nvwgf2umx SetDependencyInfo null-deref at zone-in is a RACE in the NV UMD's
	// internal worker-thread pool: the fault fires on a driver worker thread (NVDEV_Thunk, 0 engine
	// frames) while servicing the rapid Map(WRITE_DISCARD) churn the scene submits. DECISIVE PROOF:
	// enabling the D3D11 debug layer -- which forces synchronous, serialized driver work -- makes the
	// crash vanish on BOTH Win32 AND x64, with ZERO validation errors logged. So it is neither a bad
	// asset nor an API-contract violation; it is the driver racing on its own threads. This flag asks
	// the runtime NOT to create those internal driver worker threads, giving the debug layer's
	// race-immunity in production at a fraction of the cost (no validation, no log). gl05/D3D9 is
	// immune because it has no such async resource-dependency worker model.
	createDeviceFlags |= D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS;

#ifdef _DEBUG
	if (ConfigDirect3d11::getEnableDebugLayer())
		createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;  // Pitfall 8: handled below
#endif

	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_11_0,   // SPEC §Constraints baseline -- fail loudly if hardware below this
	};
	D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;

	HRESULT hr = D3D11CreateDevice(
		nullptr,                                // default adapter (ConfigDirect3d11::getPreferredAdapterIndex>=0 -> EnumAdapters in Wave 4+)
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,                                // no software rasterizer
		createDeviceFlags,
		featureLevels, _countof(featureLevels),
		D3D11_SDK_VERSION,
		&ms_device, &fl, &ms_context);

	// Pitfall 8: retry without DEBUG flag if Graphics Tools optional feature missing.
	if (hr == DXGI_ERROR_SDK_COMPONENT_MISSING && (createDeviceFlags & D3D11_CREATE_DEVICE_DEBUG))
	{
		DEBUG_REPORT_LOG_PRINT(true,
			("Direct3d11: Graphics Tools optional feature missing; retrying D3D11CreateDevice "
			 "without DEBUG flag. Install via Windows Settings > Apps > Optional Features > "
			 "'Graphics Tools' for D3D11 debug-layer messages.\n"));
		createDeviceFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
		hr = D3D11CreateDevice(
			nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
			createDeviceFlags, featureLevels, _countof(featureLevels),
			D3D11_SDK_VERSION, &ms_device, &fl, &ms_context);
	}

	FATAL_DX_HR("D3D11CreateDevice failed: %s", hr);

	FATAL(fl < D3D_FEATURE_LEVEL_11_0,
		("Direct3d11: feature level < 11.0 (got 0x%x); SPEC §Constraints requires "
		 "D3D 11.0 baseline. GPU/driver cannot satisfy Phase 11.", static_cast<unsigned>(fl)));

	DEBUG_REPORT_LOG_PRINT(true,
		("Direct3d11: device created at feature level 0x%x (D3D11_CREATE_DEVICE_DEBUG=%s)\n",
		 static_cast<unsigned>(fl),
		 (createDeviceFlags & D3D11_CREATE_DEVICE_DEBUG) ? "on" : "off"));

	// ------------------------------------------------------------------
	// Plan 11-08 Iter-1: acquire ID3D11InfoQueue when the debug layer is
	// live so every D3D11 debug-layer validation message reaches the
	// runtime log on the same frame it fires. The Iter-18 BSOD established
	// that the next cbuffer-wiring bug MUST surface as a logged warning
	// BEFORE escalating to TDR -- without this drain in place, a bad
	// cbuffer-write would race the rasterizer to BSOD with no diagnostic
	// trace recoverable from disk.

	if (createDeviceFlags & D3D11_CREATE_DEVICE_DEBUG)
	{
		HRESULT const qiHr = ms_device.As(&ms_infoQueue);
		if (SUCCEEDED(qiHr) && ms_infoQueue)
		{
			// Start with no filter -- capture every severity. Later
			// iterations can tighten if the steady-state log is too noisy.
			ms_infoQueue->PushEmptyStorageFilter();

			// Corruption is unrecoverable -- break into debugger when
			// attached, or terminate the process when not. ERROR-severity
			// is downgraded to log-only (NOT break): the Iter-1 initial
			// configuration set ERROR=TRUE, which killed the smoke during
			// ShaderTemplate_Iff load of `lightsaberblade_lava_a.sht`
			// before drainInfoQueue() could log the actual D3D11 message
			// (DebugBreak fires synchronously when the message is added
			// to the queue, not when the queue is drained). The safety
			// net's intent is "early warning BEFORE TDR/BSOD" -- the LOG
			// is the early warning, not the BREAK. ERROR + WARNING + INFO
			// all flow through drainInfoQueue() each frame. CORRUPTION
			// stays TRUE because it's genuinely unrecoverable.
			ms_infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
			ms_infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR,      FALSE);

			DEBUG_REPORT_LOG_PRINT(true,
				("Direct3d11: ID3D11InfoQueue acquired; per-frame drain installed; "
				 "break-on-severity = CORRUPTION (ERROR downgraded to log-only per Iter-1.6).\n"));

			// Plan 11-08 Iter-1.7: open the parallel file sink. Append
			// mode + line-buffered flush after each message (set below in
			// drainInfoQueue) so a crash mid-frame doesn't strand
			// messages in stdio's internal buffer. Path is CWD-relative;
			// SwgClient_d.exe runs with CWD = stage/ in normal launches
			// so the file lands at stage/d3d11-debug.log. fopen failure
			// is non-fatal -- DEBUG_REPORT_LOG_PRINT still runs in
			// parallel; if Kenny is running with DebugView attached he
			// gets the messages even if the file sink can't open.
			ms_d3d11LogFile = std::fopen("d3d11-debug.log", "ab");
			if (ms_d3d11LogFile)
			{
				time_t const now = std::time(nullptr);
				struct tm tmBuf = {};
				localtime_s(&tmBuf, &now);
				char ts[64] = {};
				std::strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", &tmBuf);
				std::fprintf(ms_d3d11LogFile,
					"\n========================================\n"
					"=== D3D11 InfoQueue session begin\n"
					"=== %s\n"
					"=== feature level 0x%x   debug flag = %s\n"
					"========================================\n",
					ts,
					static_cast<unsigned>(fl),
					(createDeviceFlags & D3D11_CREATE_DEVICE_DEBUG) ? "on" : "off");
				std::fflush(ms_d3d11LogFile);
			}
			else
			{
				DEBUG_REPORT_LOG_PRINT(true,
					("Direct3d11: could not open d3d11-debug.log for append; "
					 "InfoQueue messages will route through DEBUG_REPORT_LOG_PRINT only.\n"));
			}
		}
		else
		{
			DEBUG_REPORT_LOG_PRINT(true,
				("Direct3d11: ID3D11InfoQueue QueryInterface failed (hr=0x%08X); "
				 "debug-layer messages WILL NOT route to the runtime log this session. "
				 "Install 'Graphics Tools' optional feature via Windows Settings.\n",
				 static_cast<unsigned>(qiHr)));
		}
	}

	// ------------------------------------------------------------------
	// Step 2: Create DXGI flip-model swap chain.

	ComPtr<IDXGIDevice>   dxgiDevice;
	ComPtr<IDXGIAdapter>  adapter;
	ComPtr<IDXGIFactory2> factory;

	hr = ms_device.As(&dxgiDevice);
	FATAL_DX_HR("ID3D11Device::QueryInterface(IDXGIDevice) failed: %s", hr);

	hr = dxgiDevice->GetAdapter(&adapter);
	FATAL_DX_HR("IDXGIDevice::GetAdapter failed: %s", hr);

	hr = adapter->GetParent(IID_PPV_ARGS(&factory));
	FATAL_DX_HR("IDXGIAdapter::GetParent(IDXGIFactory2) failed: %s", hr);

	DXGI_SWAP_CHAIN_DESC1 desc = {};
	desc.Width              = static_cast<UINT>(width);
	desc.Height             = static_cast<UINT>(height);
	desc.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;   // matches engine PackedArgb conventions
	desc.Stereo             = FALSE;
	desc.SampleDesc.Count   = 1;                            // MSAA not in flip-model swap chain
	desc.SampleDesc.Quality = 0;
	desc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount        = 2;                            // flip model requires >= 2
	desc.Scaling            = DXGI_SCALING_STRETCH;
	desc.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.AlphaMode          = DXGI_ALPHA_MODE_UNSPECIFIED;
	desc.Flags              = 0;

	hr = factory->CreateSwapChainForHwnd(
		ms_device.Get(), hwnd,
		&desc, nullptr, nullptr, &ms_swapChain);
	FATAL_DX_HR("IDXGIFactory2::CreateSwapChainForHwnd failed: %s", hr);

	// Disable Alt-Enter -- engine handles fullscreen toggle via setWindowedMode.
	factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

	// ------------------------------------------------------------------
	// Step 3: Back-buffer RTV + depth-stencil texture + DSV.

	if (!createBackBufferViews(width, height))
		return false;

	DEBUG_REPORT_LOG_PRINT(true,
		("Direct3d11: swap chain %dx%d BGRA8 FLIP_DISCARD (2 buffers); back-buffer RTV + D24S8 DSV created\n",
		 width, height));

	ms_installed = true;

	// ------------------------------------------------------------------
	// Plan 11-09.8: phantom zero buffer. 16-byte zero-filled
	// BIND_VERTEX_BUFFER (immutable). Bound at InputSlot=15 by
	// applyPreDrawState; backs reflected-but-VBFormat-absent VS inputs.
	{
		D3D11_BUFFER_DESC pzDesc = {};
		pzDesc.ByteWidth          = 16;
		pzDesc.Usage              = D3D11_USAGE_IMMUTABLE;
		pzDesc.BindFlags          = D3D11_BIND_VERTEX_BUFFER;
		pzDesc.CPUAccessFlags     = 0;
		pzDesc.MiscFlags          = 0;
		pzDesc.StructureByteStride = 0;

		uint8_t zeros[16] = {};
		D3D11_SUBRESOURCE_DATA pzInit = {};
		pzInit.pSysMem = zeros;

		HRESULT const pzHr = ms_device->CreateBuffer(&pzDesc, &pzInit, ms_phantomZeroBuffer.GetAddressOf());
		FATAL_DX_HR("Direct3d11_Device: phantom zero buffer CreateBuffer failed: %s", pzHr);
		DEBUG_REPORT_LOG_PRINT(true,
			("Direct3d11: phantom zero buffer (slot 15, 16B immutable, stride=0) ready\n"));
	}

	// ------------------------------------------------------------------
	// Initial clear + present (mirrors D3D9 plugin's pattern at
	// Direct3d9.cpp:1363-1367). Puts dark blue on the screen IMMEDIATELY
	// at install-time, before any other engine slot has a chance to fire.
	// Without this, the per-frame loop may FATAL on an unstubbed slot
	// (Wave 4+ territory) before reaching beginScene/clearViewport/present
	// at all, and Plan 11-03's "dark blue window" deliverable would never
	// be visually observable.
	//
	// Direct3d11_Device::beginScene rebinds RTV+DSV (Pitfall 3).

	Direct3d11_Device::beginScene();
	Direct3d11_Device::clearViewport(true, 0, true, 1.0f, true, 0);
	Direct3d11_Device::endScene();
	Direct3d11_Device::present();

	DEBUG_REPORT_LOG_PRINT(true, ("Direct3d11: initial clear-to-color present complete (dark blue 0,0,0.25,1)\n"));

	return true;
}

// ======================================================================
// Direct3d11_Device::destroy -- release in REVERSE creation order.
// ComPtr destruction at static-storage cleanup time would also work, but
// explicit ordered Reset() gives clean teardown logging and avoids
// late-destruction warnings from the debug layer.

void Direct3d11_Device::destroy()
{
	if (!ms_installed)
		return;

	DEBUG_REPORT_LOG_PRINT(true, ("Direct3d11_Device: destroy\n"));

	// Drain any final pending validation messages so they reach the log
	// before the InfoQueue itself goes away. Then release the InfoQueue
	// BEFORE the device -- explicit pre-device reset eliminates a
	// debug-layer late-destruction warning even with ComPtr.
	Direct3d11_Device::drainInfoQueue();
	ms_infoQueue.Reset();

	// Plan 11-08 Iter-1.7: close the file sink after the final drain,
	// before ComPtrs Reset(), so the closing newline + EOF land cleanly.
	if (ms_d3d11LogFile)
	{
		std::fprintf(ms_d3d11LogFile, "=== D3D11 InfoQueue session end ===\n");
		std::fclose(ms_d3d11LogFile);
		ms_d3d11LogFile = nullptr;
	}

	// Plan 11-09.8: release phantom zero buffer alongside other VBs.
	ms_phantomZeroBuffer.Reset();

	// GAMMA-01: release curve-pass resources before the device goes away.
	ms_bcgTempSRV.Reset();
	ms_bcgTempTex.Reset();
	ms_bcgCB.Reset();
	ms_bcgSampler.Reset();
	ms_bcgPS.Reset();
	ms_bcgVS.Reset();
	ms_bcgTempWidth  = 0;
	ms_bcgTempHeight = 0;
	ms_bcgInitFailed = false;

	ms_depthStencilDSV.Reset();
	ms_depthStencilTex.Reset();
	ms_backBufferRTV.Reset();
	ms_swapChain.Reset();
	if (ms_context) ms_context->ClearState();
	ms_context.Reset();
	ms_device.Reset();

	ms_window           = nullptr;
	ms_width            = 0;
	ms_height           = 0;
	ms_engineOwnsWindow = false;
	ms_windowX          = INT_MAX;
	ms_windowY          = INT_MAX;
	ms_borderlessWindow = false;
	ms_installed        = false;
}

// ======================================================================
// Singleton accessors.

ID3D11Device *        Direct3d11_Device::getDevice()  { return ms_device.Get(); }
IDXGISwapChain1 *     Direct3d11_Device::getSwapChain() { return ms_swapChain.Get(); }   // Utinni hook-point advertisement (handoff 2026-06-15): live swapchain, null before create()/after destroy(), borrowed (not owned)
ID3D11RenderTargetView * Direct3d11_Device::getBackBufferRTV() { return ms_backBufferRTV.Get(); }
ID3D11DepthStencilView * Direct3d11_Device::getDepthStencilView() { return ms_depthStencilDSV.Get(); }   // Phase 19: shared screen depth (::46); RenderTarget binds it on the full-screen scene RT so the world depth-tests (D3D9 parity -- depth surface persists across SetRenderTarget for screen-sized RTs)
ID3D11Buffer *        Direct3d11_Device::getPhantomZeroBuffer() { return ms_phantomZeroBuffer.Get(); }

// Plan 11-09 Iter-2.7f (CODEX Round 6): frame-draw-activity flag + pending
// primary clear state. clearViewport's escape hatch defers primary-RT
// clears that happen after a draw within the same frame; beginScene applies
// them at the start of the next frame.
namespace {
	bool                                 s_frameHasDraws = false;
	bool                                 s_pendingPrimaryColorClear   = false;
	uint32                               s_pendingPrimaryColorValue   = 0;
	bool                                 s_pendingPrimaryDepthClear   = false;
	real                                 s_pendingPrimaryDepthValue   = 1.0f;
	bool                                 s_pendingPrimaryStencilClear = false;
	uint32                               s_pendingPrimaryStencilValue = 0;
}

void Direct3d11_Device::setFrameHasDrawActivity()
{
	s_frameHasDraws = true;
}
ID3D11DeviceContext * Direct3d11_Device::getContext() { return ms_context.Get(); }
int                   Direct3d11_Device::getWidth()   { return ms_width; }
int                   Direct3d11_Device::getHeight()  { return ms_height; }
bool                  Direct3d11_Device::isInstalled(){ return ms_installed; }
HWND                  Direct3d11_Device::getWindow()  { return ms_window; }
bool                  Direct3d11_Device::isWindowed() { return ms_windowed; }
bool                  Direct3d11_Device::engineOwnsWindow() { return ms_engineOwnsWindow; }

// ======================================================================
// Plan 11-09.5 -- plugin-side window-show parity with D3D9.
//
// Mirrors Direct3d9Namespace::updateWindowSettings (Direct3d9.cpp:1870-1930)
// with the following DXGI substitutions:
//   - Monitor info: DXGI IDXGIOutput->GetDesc().DesktopCoordinates via
//     ms_swapChain->GetContainingOutput (replaces D3D9's
//     IDirect3D9::GetAdapterMonitor + Win32 GetMonitorInfo). Fallback to
//     MonitorFromWindow + GetMonitorInfo if DXGI returns null/E_FAIL.
//   - State references go through the namespace ms_-prefixed statics
//     directly (we live in the same TU as the state).
// Everything else (style bits, AdjustWindowRect, SetWindowPos flag combo
// including the crucial SWP_SHOWWINDOW) is identical to D3D9.

void Direct3d11_Device::updateWindowSettings()
{
	if (!ms_engineOwnsWindow)
		return;

	DWORD const windowStyleWindowed   = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_BORDER | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
	DWORD const windowStyleFullscreen = WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

	// Resolve the containing monitor's desktop rect. DXGI first; fall back to
	// the Win32 MonitorFromWindow path if DXGI is unavailable (e.g. the swap
	// chain hasn't been bound to an output yet).
	RECT monitorRect = { 0, 0, 0, 0 };
	bool monitorRectValid = false;

	if (ms_swapChain)
	{
		ComPtr<IDXGIOutput> output;
		if (SUCCEEDED(ms_swapChain->GetContainingOutput(&output)) && output)
		{
			DXGI_OUTPUT_DESC outputDesc;
			if (SUCCEEDED(output->GetDesc(&outputDesc)))
			{
				monitorRect = outputDesc.DesktopCoordinates;
				monitorRectValid = true;
			}
		}
	}

	if (!monitorRectValid)
	{
		HMONITOR const monitor = MonitorFromWindow(ms_window, MONITOR_DEFAULTTOPRIMARY);
		MONITORINFO mi;
		ZeroMemory(&mi, sizeof(mi));
		mi.cbSize = sizeof(mi);
		if (GetMonitorInfo(monitor, &mi))
		{
			monitorRect = mi.rcMonitor;
			monitorRectValid = true;
		}
	}

	if (ms_windowed)
	{
		RECT rect;
		rect.left   = 0;
		rect.top    = 0;
		rect.right  = ms_width;
		rect.bottom = ms_height;

		DWORD const windowStyle = ms_borderlessWindow ? windowStyleFullscreen : windowStyleWindowed;
		SetWindowLong(ms_window, GWL_STYLE, windowStyle);

		// Adjust client-area rect to include the WS_CAPTION/border chrome.
		BOOL const result1 = AdjustWindowRect(&rect, windowStyle, FALSE);
		DEBUG_FATAL(!result1, ("AdjustWindowRect failed"));
		UNREF(result1);

		if (monitorRectValid)
		{
			if (ms_borderlessWindow)
			{
				ms_windowX = monitorRect.left;
				ms_windowY = monitorRect.top;
			}
			else if (ms_windowX == INT_MAX || ms_windowY == INT_MAX)
			{
				// Center on monitor (first-launch fallback; matches
				// Direct3d9.cpp:1906-1910).
				ms_windowX = monitorRect.left + (((monitorRect.right - monitorRect.left) - ms_width) / 2);
				ms_windowY = monitorRect.top  + (((monitorRect.bottom - monitorRect.top) - ms_height) / 2);
			}
		}
		else
		{
			// Last-resort fallback if both DXGI and Win32 monitor queries
			// failed. Place at (0,0) so the window is at least visible
			// on the primary display.
			if (ms_windowX == INT_MAX) ms_windowX = 0;
			if (ms_windowY == INT_MAX) ms_windowY = 0;
		}

		BOOL const result2 = SetWindowPos(ms_window, HWND_NOTOPMOST,
		                                  ms_windowX, ms_windowY,
		                                  rect.right - rect.left, rect.bottom - rect.top,
		                                  SWP_NOCOPYBITS | SWP_SHOWWINDOW);
		FATAL(!result2, ("SetWindowPos failed"));
	}
	else
	{
		SetWindowLong(ms_window, GWL_STYLE, windowStyleFullscreen);

		// Use the monitor rect if we have it; otherwise fall back to the
		// current window position (the SetWindowPos call will still apply
		// SWP_SHOWWINDOW which is the bit that actually matters here).
		RECT placement = monitorRect;
		if (!monitorRectValid)
		{
			if (!GetWindowRect(ms_window, &placement))
			{
				placement.left = placement.top = 0;
				placement.right = ms_width;
				placement.bottom = ms_height;
			}
		}

		BOOL const result2 = SetWindowPos(ms_window, HWND_TOPMOST,
		                                  placement.left, placement.top,
		                                  ms_width, ms_height,
		                                  SWP_NOCOPYBITS | SWP_SHOWWINDOW);
		FATAL(!result2, ("SetWindowPos failed"));
	}
}

// ----------------------------------------------------------------------
// Plan 11-09.5 -- Gl_api::setWindowedMode real binding.
//
// Replaces the Plan 11-02 STUB(setWindowedMode) scaffold_fatal_stub
// reinterpret_cast. Iter-1 scope: real DXGI fullscreen toggle deferred --
// flip-discard swap chains have stricter SetFullscreenState semantics than
// D3D9 blit-model. For now: if the engine attempts a windowed<->fullscreen
// toggle, log a warning and re-apply existing settings (so the engine's
// Options->Display menu doesn't FATAL). Real toggle is a future plan.

void Direct3d11_Device::setWindowedMode(bool windowed)
{
	if (!ms_engineOwnsWindow)
		return;

	if (ms_windowed == windowed)
	{
		// Boot-time case: engine asks for the mode it's already in. Re-apply
		// to keep behavior aligned with D3D9 (which always calls SetWindowPos
		// regardless of the toggle direction).
		updateWindowSettings();
		return;
	}

	DEBUG_REPORT_LOG_PRINT(true, ("Direct3d11_Device::setWindowedMode toggle (%d -> %d) deferred to a future plan; D3D11 fullscreen state change not yet implemented (Plan 11-09.5 Iter-1 scope is window-show only). Leaving windowed-mode unchanged.\n",
	                              static_cast<int>(ms_windowed), static_cast<int>(windowed)));
	updateWindowSettings();
}

// ======================================================================
// Per-frame slot bodies.
// ======================================================================

// beginScene -- Pitfall 3 (flip-model unbinds RTV after Present; rebind
// every frame unconditionally; viewport also gets rebound here).

void Direct3d11_Device::beginScene()
{
	// Plan 11-09.15 Iter-11: clear PS SRV slots before OMSetRenderTargets
	// to avoid the D3D11 "Resource being set to OM RenderTarget slot 0 is
	// still bound on input!" warning class. Iter-9/10 diagnostics
	// confirmed viewportData @ c9 is correct yet splash/login render
	// black; Iter-10 shutdown stats surfaced 1966 instances per session
	// of this exact warning pair:
	//   "OMSetRenderTargets: Resource being set to OM RenderTarget slot 0
	//      is still bound on input!"
	//   "OMSetRenderTargets[AndUnorderedAccessViews]: Forcing PS shader
	//      resource slot 0 to NULL."
	// When D3D11 detects an RT and SRV pointing at the same resource, it
	// FORCE-NULLS the conflicting SRV slot. For 2D UI draws that follow
	// the RT bind, the PS samples a NULL slot-0 SRV and returns
	// (0,0,0,0) -> black output, exactly matching the splash/login
	// black-screen + char-select-magenta-lines visual (magenta = Variant
	// M fallback for the draws that did pick up an alternate slot;
	// most draws drop to zero color). Clearing SRVs explicitly before
	// the RT swap decouples the binding ordering and lets D3D11 do its
	// validation cleanly.
	{
		ID3D11ShaderResourceView * const kNullSRVs[16] = {};
		ms_context->PSSetShaderResources(0, 16, kNullSRVs);
	}

	ID3D11RenderTargetView * rtvs[1] = { ms_backBufferRTV.Get() };
	ms_context->OMSetRenderTargets(1, rtvs, ms_depthStencilDSV.Get());

	// Plan 11-09.15 Iter-8: route per-frame viewport setup through
	// Direct3d11_StateCache::setViewport instead of calling RSSetViewports
	// directly. setViewport ALSO writes the VSCR_viewportData float4 into
	// slot-0 cbuffer c9 (Plan 11-09 Iter-2.7b CODEX Round 4) -- the
	// constant that SWG's stock 2D vertex shaders (2d.vsh, 2d_texture.vsh,
	// ui_radar.vsh, ui.vsh, ...) read for the pixel-to-NDC conversion
	// applied via transform2d() from vertex_program/include/functions.inc:
	//     ndc.x = pixel.x * viewportData.x + viewportData.z
	//     ndc.y = pixel.y * viewportData.y + viewportData.w
	//
	// Pre-Iter-8, beginScene called RSSetViewports directly with a
	// locally-constructed D3D11_VIEWPORT, bypassing the StateCache. c9
	// stayed at install-time zero until SOMETHING in the engine called
	// Graphics::setViewport (typically Camera, PostProcessingEffectsManager,
	// or ShaderPrimitiveSorter -- none of which run during splash/login).
	// Result: splash and login UI XYZRHW quads were submitted with
	// viewportData=(0,0,0,0), the VS multiplied screen-space x/y by zero,
	// all four corners collapsed to clip-space origin, and the splash
	// rendered as nothing visible (black). The same mechanism with
	// stale-but-nonzero viewportData (Iter-4's longer session, after
	// PostFX/Sorter had transiently set c9 to render-target dimensions)
	// produced the radial-from-world-anchored-point stretched-triangle
	// signature instead of black.
	//
	// Plan 11-09 Iter-2.7b's fix wired setViewport to write c9, but the
	// engine doesn't call Graphics::setViewport during early-frame UI
	// renders -- that gap is the actual hole. Iter-8 closes it by giving
	// every frame a guaranteed valid viewportData write before the first
	// draw, mirroring the spirit of D3D9's path where transformed-vertex
	// callers (FFP D3DDECLUSAGE_POSITIONT or VSPS with c9 reads) had a
	// non-zero c9 from frame zero.
	Direct3d11_StateCache::setViewport(0, 0, ms_width, ms_height, 0.0f, 1.0f);

	// Plan 11-09.15 Iter-9 diagnostic: confirm the Iter-8 setViewport
	// call actually executed and produced sane values. Sampled (first 3
	// frames + every 500th) to limit log noise.
	{
		static int s_beginSceneCount = 0;
		++s_beginSceneCount;
		bool const early  = (s_beginSceneCount <= 3);
		bool const sample = (s_beginSceneCount > 3) && ((s_beginSceneCount % 500) == 0);
		if (early || sample)
		{
			float const vx = (ms_width  > 0) ?  2.0f / static_cast<float>(ms_width)  : 0.0f;
			float const vy = (ms_height > 0) ? -2.0f / static_cast<float>(ms_height) : 0.0f;
			char buf[256];
			_snprintf_s(buf, sizeof(buf), _TRUNCATE,
				"Plan 11-09.15 Iter-9 beginScene#%d width=%d height=%d "
				"computed viewportData=(%.6f, %.6f, -1.0, +1.0)",
				s_beginSceneCount, ms_width, ms_height, vx, vy);
			if (ID3D11InfoQueue *iq = getInfoQueue())
				iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, buf);
		}
	}

	// Plan 11-09 Iter-2.7f (CODEX Round 6): apply pending primary-RT clear
	// from previous frame's clearViewport-after-draw. Engine's D3D9-era
	// pattern issues clearViewport AFTER UI render thinking it'll affect
	// the next frame's start; under D3D11 flip-model that would wipe the
	// to-be-presented surface, so we deferred the clear. Now we apply it
	// against this frame's freshly-bound backbuffer.
	if (s_pendingPrimaryColorClear && ms_backBufferRTV)
	{
		// D3DCOLOR is 0xAARRGGBB (CODEX Q3 verified).
		float const a = static_cast<float>((s_pendingPrimaryColorValue >> 24) & 0xFFu) / 255.0f;
		float const r = static_cast<float>((s_pendingPrimaryColorValue >> 16) & 0xFFu) / 255.0f;
		float const g = static_cast<float>((s_pendingPrimaryColorValue >>  8) & 0xFFu) / 255.0f;
		float const b = static_cast<float>((s_pendingPrimaryColorValue >>  0) & 0xFFu) / 255.0f;
		float const color[4] = { r, g, b, a };
		ms_context->ClearRenderTargetView(ms_backBufferRTV.Get(), color);
	}
	UINT clearFlags = 0;
	if (s_pendingPrimaryDepthClear)   clearFlags |= D3D11_CLEAR_DEPTH;
	if (s_pendingPrimaryStencilClear) clearFlags |= D3D11_CLEAR_STENCIL;
	if (clearFlags && ms_depthStencilDSV)
	{
		ms_context->ClearDepthStencilView(
			ms_depthStencilDSV.Get(),
			clearFlags,
			static_cast<FLOAT>(s_pendingPrimaryDepthValue),
			static_cast<UINT8>(s_pendingPrimaryStencilValue & 0xFFu));
	}
	// Reset pending state + frame-draw flag for this new frame.
	s_pendingPrimaryColorClear   = false;
	s_pendingPrimaryDepthClear   = false;
	s_pendingPrimaryStencilClear = false;
	s_frameHasDraws              = false;
}

// ----------------------------------------------------------------------
// endScene -- D3D11 has no batched-state-flush equivalent (vs D3D9's
// EndScene which closed the implicit BeginScene state batch). No-op.

void Direct3d11_Device::endScene()
{
	// intentional no-op
}

// ----------------------------------------------------------------------
// clearViewport -- engine passes (clearColor, colorValue argb,
// clearDepth, depthValue, clearStencil, stencilValue).
//
// Plan 11-03 MVP: use dark blue (0.0, 0.0, 0.25, 1.0) regardless of
// the argb parameter so the screen is clearly D3D11-active visually.
// Wave 4+ converts the engine-supplied PackedArgb (uint32, ARGB layout)
// to float4 for ClearRenderTargetView.
//
// Depth/stencil components honor the engine's clear flags + values
// (depth in [0,1], stencil in [0,255]).

void Direct3d11_Device::clearViewport(bool clearColor, uint32 colorValue,
                                      bool clearDepth, real depthValue,
                                      bool clearStencil, uint32 stencilValue)
{
	// Plan 11-09 Iter-2.7f (CODEX Round 6 final): targeted fix for the D3D9->
	// D3D11 clear-after-draw hazard.
	//
	// Architecture:
	//   - Clear the BOUND RTV/DSV (D3D9 Clear semantics), not always backbuffer.
	//   - Use engine's colorValue (ARGB -> float4), not Plan 11-03's hardcoded
	//     placeholder.
	//   - Primary-backbuffer escape hatch: if bound RTV is the backbuffer AND
	//     a draw has already submitted this frame, stash the clear and let
	//     beginScene apply it next frame. This is the only case where the
	//     D3D11 flip-model would wipe the to-be-presented surface.
	//   - Offscreen / user RTs: clear immediately. Those are typically
	//     intentional pass-local clears.

	ID3D11RenderTargetView * boundRTV = nullptr;
	ID3D11DepthStencilView * boundDSV = nullptr;
	ms_context->OMGetRenderTargets(1, &boundRTV, &boundDSV);

	bool const boundIsPrimary = (boundRTV == ms_backBufferRTV.Get());
	bool const deferThisClear = boundIsPrimary && s_frameHasDraws;

	if (deferThisClear)
	{
		// Stash for next beginScene. Coalesce: last value wins per aspect.
		if (clearColor)   { s_pendingPrimaryColorClear   = true; s_pendingPrimaryColorValue   = colorValue;   }
		if (clearDepth)   { s_pendingPrimaryDepthClear   = true; s_pendingPrimaryDepthValue   = depthValue;   }
		if (clearStencil) { s_pendingPrimaryStencilClear = true; s_pendingPrimaryStencilValue = stencilValue; }
	}
	else
	{
		// Immediate clear of the bound target (D3D9 semantics).
		if (clearColor && boundRTV)
		{
			float const a = static_cast<float>((colorValue >> 24) & 0xFFu) / 255.0f;
			float const r = static_cast<float>((colorValue >> 16) & 0xFFu) / 255.0f;
			float const g = static_cast<float>((colorValue >>  8) & 0xFFu) / 255.0f;
			float const b = static_cast<float>((colorValue >>  0) & 0xFFu) / 255.0f;
			float const color[4] = { r, g, b, a };
			ms_context->ClearRenderTargetView(boundRTV, color);
		}

		UINT clearFlags = 0;
		if (clearDepth)   clearFlags |= D3D11_CLEAR_DEPTH;
		if (clearStencil) clearFlags |= D3D11_CLEAR_STENCIL;
		if (clearFlags && boundDSV)
		{
			ms_context->ClearDepthStencilView(
				boundDSV,
				clearFlags,
				static_cast<FLOAT>(depthValue),
				static_cast<UINT8>(stencilValue & 0xFFu));
		}
	}

	// OMGetRenderTargets added refs; release them.
	if (boundRTV) boundRTV->Release();
	if (boundDSV) boundDSV->Release();
}

// ----------------------------------------------------------------------
// GAMMA-01: store the curve parameters; the pass itself runs in present().
// Mirrors the D3D9 semantics where each setBrightnessContrastGamma call
// replaces the whole ramp. Identity (1,1,1) disables the pass.

void Direct3d11_Device::setBrightnessContrastGamma(float brightness, float contrast, float gamma)
{
	ms_bcgBrightness = brightness;
	ms_bcgContrast   = contrast;
	ms_bcgGamma      = (gamma > 0.001f) ? gamma : 0.001f;   // guard the 1/gamma in the PS constants

	float const epsilon = 0.001f;
	ms_bcgActive = std::fabs(ms_bcgBrightness - 1.0f) > epsilon
		|| std::fabs(ms_bcgContrast - 1.0f) > epsilon
		|| std::fabs(ms_bcgGamma - 1.0f) > epsilon;

	DEBUG_REPORT_LOG_PRINT(true, ("Direct3d11_Device: setBrightnessContrastGamma(%.3f, %.3f, %.3f) -> pass %s\n",
		ms_bcgBrightness, ms_bcgContrast, ms_bcgGamma, ms_bcgActive ? "ACTIVE" : "identity (off)"));
}

// ----------------------------------------------------------------------
// present -- flip-model present. Per D-13 / SPEC §Boundaries:
// DEVICE_REMOVED is a process-restart class event (no recovery attempt).

bool Direct3d11_Device::present()
{
	if (!ms_swapChain)
		return false;

	// GAMMA-01: apply the brightness/contrast/gamma curve over the finished
	// frame (no-op when identity). Runs BEFORE drainInfoQueue so any
	// validation messages the pass fires drain on the same frame.
	applyBcgPass();

	// Plan 11-08 Iter-1: drain D3D11 debug-layer validation messages
	// accumulated by this frame's draw calls BEFORE Present so they reach
	// the runtime log on the same frame they fire. No-op when ms_infoQueue
	// is null (debug layer not live).
	drainInfoQueue();

	HRESULT hr = ms_swapChain->Present(1, 0);  // vsync=1 for Wave 3 MVP (allowTearing override is Wave 4+)
	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
	{
		HRESULT const reason = ms_device ? ms_device->GetDeviceRemovedReason() : E_FAIL;
		FATAL(true, ("Direct3d11: DXGI_ERROR_DEVICE_REMOVED on Present (reason: 0x%08X). "
		             "Per SPEC §Boundaries, no recovery -- restart client.",
		             static_cast<unsigned>(reason)));
	}
	FATAL_DX_HR("IDXGISwapChain1::Present failed: %s", hr);

#if 0   // Phase 19 DIAGNOSTIC (REVERT -- set to 0): force full CPU<->GPU serialization
		// each frame to test the "RenderDoc replay is clean -> non-deterministic resource
		// content" hypothesis. Spins the CPU until the GPU finishes this frame's work. If the
		// bright flicker STOPS with this on, the bug is a CPU-upload / GPU-read RACE (a buffer
		// or texture read before its update completes -> fix = proper resource versioning/sync).
		// If it PERSISTS, it's UNINITIALIZED resource content (-> fix = init-on-create).
		// Intentionally kills pipelining (slow) -- diagnostic only, DO NOT SHIP.
	if (ms_context && ms_device)
	{
		ms_context->Flush();
		D3D11_QUERY_DESC qd;
		qd.Query = D3D11_QUERY_EVENT;
		qd.MiscFlags = 0;
		ID3D11Query *q = nullptr;
		if (SUCCEEDED(ms_device->CreateQuery(&qd, &q)) && q)
		{
			ms_context->End(q);
			BOOL done = FALSE;
			while (ms_context->GetData(q, &done, sizeof(done), 0) != S_OK)
			{ /* spin until the GPU reaches this point */ }
			q->Release();
		}
	}
#endif
	return true;
}

// ----------------------------------------------------------------------
// displayModeChanged -- recreate swap chain back-buffer at new HWND
// client-rect size. For the Plan 11-03 MVP this is a minimal
// implementation; Wave 4+ may extend with allow-tearing / format
// recreation paths.

void Direct3d11_Device::displayModeChanged()
{
	if (!ms_installed)
		return;

	RECT rc;
	GetClientRect(ms_window, &rc);
	resizeBackBuffer(rc.right - rc.left, rc.bottom - rc.top);
}

// ----------------------------------------------------------------------
// resizeBackBuffer -- recreate the swap-chain back-buffer (+ RTV/DSV) at an
// explicit client-rect size. Self-guards on uninstalled / zero / unchanged
// size. displayModeChanged() calls this with the live HWND client rect; the
// Gl_api resize slot (Direct3d11.cpp resize_impl) wraps it in the
// device-lost/restored callback cycle so the offscreen scene render target(s)
// + the per-frame camera viewport/projection track the new size (this is the
// embedded-window resize fix: without the wrap only the back-buffer resized
// while the scene viewport/RT/projection stayed at the creation size, so the
// scene rendered cropped to the top-left of a resized back-buffer).

void Direct3d11_Device::resizeBackBuffer(int newWidth, int newHeight)
{
	if (!ms_installed)
		return;

	if (newWidth <= 0 || newHeight <= 0)
		return;
	if (newWidth == ms_width && newHeight == ms_height)
		return;

	DEBUG_REPORT_LOG_PRINT(true,
		("Direct3d11_Device::resizeBackBuffer %dx%d -> %dx%d\n",
		 ms_width, ms_height, newWidth, newHeight));

	// Release back-buffer references (required by ResizeBuffers).
	ms_depthStencilDSV.Reset();
	ms_depthStencilTex.Reset();
	ms_backBufferRTV.Reset();
	if (ms_context) ms_context->ClearState();

	HRESULT hr = ms_swapChain->ResizeBuffers(
		0,                              // BufferCount = preserve existing
		static_cast<UINT>(newWidth),
		static_cast<UINT>(newHeight),
		DXGI_FORMAT_UNKNOWN,            // preserve existing format
		0);
	FATAL_DX_HR("IDXGISwapChain1::ResizeBuffers failed: %s", hr);

	ms_width  = newWidth;
	ms_height = newHeight;

	createBackBufferViews(newWidth, newHeight);
}

// ======================================================================
// Plan 11-08 Iter-1: drainInfoQueue -- pump every D3D11 debug-layer
// validation message accumulated since the previous call through
// DEBUG_REPORT_LOG_PRINT. Hot-loop guarded on ms_infoQueue so callers
// don't need to gate. SetBreakOnSeverity(CORRUPTION/ERROR, TRUE) means
// fatal severities trap before reaching this drain when a debugger is
// attached -- warnings + info routes here in the steady state.

void Direct3d11_Device::drainInfoQueue()
{
	if (!ms_infoQueue)
		return;

	UINT64 const count = ms_infoQueue->GetNumStoredMessages();
	for (UINT64 i = 0; i < count; ++i)
	{
		SIZE_T messageBytes = 0;
		HRESULT hr = ms_infoQueue->GetMessage(i, nullptr, &messageBytes);
		if (FAILED(hr) || messageBytes == 0)
			continue;

		// Stack-bounded scratch for typical messages; heap-fallback for
		// long descriptions. Most validation strings are under 512 bytes.
		char stackBuf[1024];
		void * buf = (messageBytes <= sizeof(stackBuf)) ? static_cast<void *>(stackBuf)
		                                                : ::operator new(messageBytes);

		D3D11_MESSAGE * msg = static_cast<D3D11_MESSAGE *>(buf);
		hr = ms_infoQueue->GetMessage(i, msg, &messageBytes);
		if (SUCCEEDED(hr))
		{
			char const * severity = "INFO";
			switch (msg->Severity)
			{
				case D3D11_MESSAGE_SEVERITY_CORRUPTION: severity = "CORRUPTION"; break;
				case D3D11_MESSAGE_SEVERITY_ERROR:      severity = "ERROR";      break;
				case D3D11_MESSAGE_SEVERITY_WARNING:    severity = "WARNING";    break;
				case D3D11_MESSAGE_SEVERITY_INFO:       severity = "INFO";       break;
				case D3D11_MESSAGE_SEVERITY_MESSAGE:    severity = "MESSAGE";    break;
			}

			DEBUG_REPORT_LOG_PRINT(true,
				("D3D11 [%s] category=%d id=%d: %.*s\n",
				 severity,
				 static_cast<int>(msg->Category),
				 static_cast<int>(msg->ID),
				 static_cast<int>(msg->DescriptionByteLength),
				 msg->pDescription));

			// Plan 11-08 Iter-1.7: mirror to file sink. Same format as
			// the DEBUG_REPORT_LOG_PRINT route. Flush each message so a
			// later crash preserves everything drained so far.
			if (ms_d3d11LogFile)
			{
				std::fprintf(ms_d3d11LogFile,
					"D3D11 [%s] category=%d id=%d: %.*s\n",
					severity,
					static_cast<int>(msg->Category),
					static_cast<int>(msg->ID),
					static_cast<int>(msg->DescriptionByteLength),
					msg->pDescription);
				std::fflush(ms_d3d11LogFile);
			}
		}

		if (buf != stackBuf)
			::operator delete(buf);
	}

	ms_infoQueue->ClearStoredMessages();

	// Phase 19 (19-04 Task 0) DEV-ONLY: advance the value-dump frame index once
	// per drained frame. REVERT with the P19_LIGHTDUMP dumps.
	++s_diagFrameIndex;
}

// ----------------------------------------------------------------------
// Phase 19 (19-04 Task 0) DEV-ONLY value-dump frame accessor. REVERT after use.

unsigned Direct3d11_Device::getDiagFrameIndex()
{
	return s_diagFrameIndex;
}

// ----------------------------------------------------------------------
// Plan 11-09.13 Iter-4: raw InfoQueue accessor for application
// diagnostics via AddApplicationMessage. Iter-3's SRV0 diagnostic via
// DEBUG_REPORT_LOG_PRINT didn't surface in stage/d3d11-debug.log because
// DEBUG_REPORT_LOG_PRINT routes to OutputDebugString + stdout (invisible
// from explorer-launched smokes). The InfoQueue path IS the file sink
// path; AddApplicationMessage entries land in d3d11-debug.log alongside
// debug-layer messages on the next drainInfoQueue cycle.

ID3D11InfoQueue *Direct3d11_Device::getInfoQueue()
{
	return ms_infoQueue.Get();
}

// ======================================================================
