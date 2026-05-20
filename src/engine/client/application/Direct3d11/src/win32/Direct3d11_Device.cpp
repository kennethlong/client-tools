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

#include "sharedDebug/DebugFlags.h"
#include "sharedFoundation/Os.h"

#include <d3d11.h>
#include <d3d11sdklayers.h>   // ID3D11InfoQueue (Plan 11-08 Iter-1 safety net)
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <cstdio>             // snprintf for InfoQueue category/severity formatting

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

	ComPtr<ID3D11Device>           ms_device;
	ComPtr<ID3D11DeviceContext>    ms_context;
	ComPtr<IDXGISwapChain1>        ms_swapChain;
	ComPtr<ID3D11RenderTargetView> ms_backBufferRTV;
	ComPtr<ID3D11Texture2D>        ms_depthStencilTex;
	ComPtr<ID3D11DepthStencilView> ms_depthStencilDSV;

	// Plan 11-08 Iter-1: D3D11 debug-layer InfoQueue. Non-null only when
	// the device was created with D3D11_CREATE_DEVICE_DEBUG (i.e. _DEBUG
	// build AND ConfigDirect3d11::getEnableDebugLayer() AND the Pitfall 8
	// fallback did NOT strip the DEBUG bit). drainInfoQueue() guards on
	// this and is a no-op when null.
	ComPtr<ID3D11InfoQueue>        ms_infoQueue;

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

bool Direct3d11_Device::create(HWND hwnd, int width, int height, bool windowed)
{
	if (ms_installed)
	{
		DEBUG_REPORT_LOG_PRINT(true, ("Direct3d11_Device::create called twice; ignoring second call.\n"));
		return true;
	}

	ms_window   = hwnd;
	ms_width    = width;
	ms_height   = height;
	ms_windowed = windowed;

	// ------------------------------------------------------------------
	// Step 1: Create ID3D11Device + immediate context.

	UINT createDeviceFlags = 0;
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
			// in debug builds is treated the same; warnings/info flow
			// through the drain to the runtime log.
			ms_infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
			ms_infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR,      TRUE);

			DEBUG_REPORT_LOG_PRINT(true,
				("Direct3d11: ID3D11InfoQueue acquired; per-frame drain installed; "
				 "break-on-severity = CORRUPTION + ERROR.\n"));
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

	ms_depthStencilDSV.Reset();
	ms_depthStencilTex.Reset();
	ms_backBufferRTV.Reset();
	ms_swapChain.Reset();
	if (ms_context) ms_context->ClearState();
	ms_context.Reset();
	ms_device.Reset();

	ms_window    = nullptr;
	ms_width     = 0;
	ms_height    = 0;
	ms_installed = false;
}

// ======================================================================
// Singleton accessors.

ID3D11Device *        Direct3d11_Device::getDevice()  { return ms_device.Get(); }
ID3D11DeviceContext * Direct3d11_Device::getContext() { return ms_context.Get(); }
int                   Direct3d11_Device::getWidth()   { return ms_width; }
int                   Direct3d11_Device::getHeight()  { return ms_height; }

// ======================================================================
// Per-frame slot bodies.
// ======================================================================

// beginScene -- Pitfall 3 (flip-model unbinds RTV after Present; rebind
// every frame unconditionally; viewport also gets rebound here).

void Direct3d11_Device::beginScene()
{
	ID3D11RenderTargetView * rtvs[1] = { ms_backBufferRTV.Get() };
	ms_context->OMSetRenderTargets(1, rtvs, ms_depthStencilDSV.Get());

	D3D11_VIEWPORT vp = {};
	vp.TopLeftX = 0.0f;
	vp.TopLeftY = 0.0f;
	vp.Width    = static_cast<float>(ms_width);
	vp.Height   = static_cast<float>(ms_height);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	ms_context->RSSetViewports(1, &vp);
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

void Direct3d11_Device::clearViewport(bool clearColor, uint32 /*colorValue*/,
                                      bool clearDepth, real depthValue,
                                      bool clearStencil, uint32 stencilValue)
{
	if (clearColor && ms_backBufferRTV)
	{
		// Plan 11-03 MVP dark-blue clear (Wave 4+ wires real argb).
		float const mvpClearColor[4] = { 0.0f, 0.0f, 0.25f, 1.0f };
		ms_context->ClearRenderTargetView(ms_backBufferRTV.Get(), mvpClearColor);
	}

	UINT clearFlags = 0;
	if (clearDepth)   clearFlags |= D3D11_CLEAR_DEPTH;
	if (clearStencil) clearFlags |= D3D11_CLEAR_STENCIL;
	if (clearFlags && ms_depthStencilDSV)
	{
		ms_context->ClearDepthStencilView(
			ms_depthStencilDSV.Get(),
			clearFlags,
			static_cast<FLOAT>(depthValue),
			static_cast<UINT8>(stencilValue & 0xFF));
	}
}

// ----------------------------------------------------------------------
// present -- flip-model present. Per D-13 / SPEC §Boundaries:
// DEVICE_REMOVED is a process-restart class event (no recovery attempt).

bool Direct3d11_Device::present()
{
	if (!ms_swapChain)
		return false;

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
	int const newWidth  = rc.right - rc.left;
	int const newHeight = rc.bottom - rc.top;
	if (newWidth <= 0 || newHeight <= 0)
		return;
	if (newWidth == ms_width && newHeight == ms_height)
		return;

	DEBUG_REPORT_LOG_PRINT(true,
		("Direct3d11_Device::displayModeChanged %dx%d -> %dx%d\n",
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
		}

		if (buf != stackBuf)
			::operator delete(buf);
	}

	ms_infoQueue->ClearStoredMessages();
}

// ======================================================================
