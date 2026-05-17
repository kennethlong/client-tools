// ======================================================================
//
// Direct3d11_RenderTarget.cpp
// Phase 11 D3D11 renderer plugin -- offscreen baked-texture render
// target (RTV + paired SRV for sample-back).
//
// Plan 11-04 (Wave 4 -- resource layer). Replaces createRenderTarget /
// setRenderTarget / copyRenderTargetToNonRenderTargetTexture scaffold
// stubs. Per CONTEXT D-13: no D3DPOOL_MANAGED / OnLostDevice /
// OnResetDevice -- D3D11 staging texture replaces D3D9's
// D3DPOOL_SYSTEMMEM bake surface. Per CONTEXT D-01: clean rewrite of the
// "render to baked texture, sample back" flow; Direct3d9_RenderTarget is
// design reference only.
//
// The "primary" RTV/DSV is the back-buffer pair owned by
// Direct3d11_Device. setRenderTargetToPrimary just rebinds those via the
// device's beginScene helper. setRenderTarget(texture) routes draws to a
// secondary offscreen target backed by either a user-supplied Texture
// (already has its own ID3D11Texture2D+SRV via Direct3d11_TextureData) or
// the persistent 512x512 baked-texture pair created at install.
//
// ======================================================================

#include "FirstDirect3d11.h"
#include "Direct3d11_RenderTarget.h"

#include "Direct3d11.h"
#include "Direct3d11_Device.h"
#include "Direct3d11_TextureData.h"

#include "clientGraphics/Texture.h"
#include "sharedDebug/DebugFlags.h"

#include <d3d11.h>
#include <wrl/client.h>

// ======================================================================

using Microsoft::WRL::ComPtr;

namespace Direct3d11_RenderTargetNamespace
{
	int const  cms_bakedTextureMaxDimension = 512;

	bool       ms_installed         = false;
	bool       ms_primaryTargetSet  = true;

	// Persistent 512x512 offscreen RT + paired SRV for the "render to
	// baked texture, sample back" flow (used by skin shaders, certain UI
	// effects). RTV is for the GPU write side; SRV is for the read-back
	// path that fills the user-supplied destination texture on
	// copyRenderTargetToNonRenderTargetTexture.

	ComPtr<ID3D11Texture2D>        ms_renderTargetTexture;
	ComPtr<ID3D11RenderTargetView> ms_renderTargetView;
	ComPtr<ID3D11ShaderResourceView> ms_renderTargetSrv;

	// User-supplied destination -- ms_userTexture is the engine Texture
	// the bake will copy into when copyRenderTargetToNonRenderTargetTexture
	// fires. ms_userMip / ms_userCubeFace track the destination subresource.
	Texture *  ms_userTexture       = nullptr;
	CubeFace   ms_userCubeFace      = CF_none;
	int        ms_userMip           = 0;
	int        ms_copyWidth         = 0;
	int        ms_copyHeight        = 0;

	// User-supplied is a render-target Texture itself? Track which RTV
	// to bind in setRenderTarget.
	ComPtr<ID3D11RenderTargetView> ms_userRTV;
}
using namespace Direct3d11_RenderTargetNamespace;

// ======================================================================

void Direct3d11_RenderTarget::install()
{
	DEBUG_FATAL(ms_installed, ("Direct3d11_RenderTarget already installed"));

	ID3D11Device * const device = Direct3d11_Device::getDevice();
	NOT_NULL(device);

	// Persistent 512x512 BGRA8 baked-texture target.
	// BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE so
	// the same texture is writable as RTV AND samplable as SRV (matches
	// D3D9's "render to texture then sample" flow but without the
	// system-memory intermediate).

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width              = static_cast<UINT>(cms_bakedTextureMaxDimension);
	desc.Height             = static_cast<UINT>(cms_bakedTextureMaxDimension);
	desc.MipLevels          = 1;
	desc.ArraySize          = 1;
	desc.Format             = DXGI_FORMAT_B8G8R8X8_UNORM;
	desc.SampleDesc.Count   = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage              = D3D11_USAGE_DEFAULT;
	desc.BindFlags          = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags     = 0;
	desc.MiscFlags          = 0;

	HRESULT hr = device->CreateTexture2D(&desc, nullptr, &ms_renderTargetTexture);
	FATAL_DX_HR("Direct3d11_RenderTarget: CreateTexture2D (baked) failed: %s", hr);

	hr = device->CreateRenderTargetView(ms_renderTargetTexture.Get(), nullptr, &ms_renderTargetView);
	FATAL_DX_HR("Direct3d11_RenderTarget: CreateRenderTargetView (baked) failed: %s", hr);

	hr = device->CreateShaderResourceView(ms_renderTargetTexture.Get(), nullptr, &ms_renderTargetSrv);
	FATAL_DX_HR("Direct3d11_RenderTarget: CreateShaderResourceView (baked) failed: %s", hr);

	ms_installed        = true;
	ms_primaryTargetSet = true;

	DEBUG_REPORT_LOG_PRINT(true,
		("Direct3d11_RenderTarget: baked-texture target %dx%d BGRA8 RTV+SRV created\n",
		 cms_bakedTextureMaxDimension, cms_bakedTextureMaxDimension));
}

// ----------------------------------------------------------------------

void Direct3d11_RenderTarget::remove()
{
	if (!ms_installed)
		return;

	ms_userRTV.Reset();
	ms_renderTargetSrv.Reset();
	ms_renderTargetView.Reset();
	ms_renderTargetTexture.Reset();

	ms_userTexture  = nullptr;
	ms_installed    = false;
}

// ----------------------------------------------------------------------

void Direct3d11_RenderTarget::setRenderTargetToPrimary()
{
	DEBUG_FATAL(!ms_installed, ("Direct3d11_RenderTarget not installed"));

	if (ms_primaryTargetSet)
		return;

	// Rebind the back-buffer RTV + DSV. Direct3d11_Device::beginScene
	// already encapsulates the rebind logic; reuse it for correctness.
	Direct3d11_Device::beginScene();
	ms_primaryTargetSet = true;

	// Release the user-side resource we held while the secondary target
	// was active (if any).
	ms_userRTV.Reset();
}

// ----------------------------------------------------------------------

void Direct3d11_RenderTarget::setRenderTarget(Texture *texture, CubeFace cubeFace, int mipmapLevel)
{
	NOT_NULL(ms_renderTargetTexture);
	NOT_NULL(texture);

	ID3D11Device *        const device  = Direct3d11_Device::getDevice();
	ID3D11DeviceContext * const context = Direct3d11_Device::getContext();
	NOT_NULL(device);
	NOT_NULL(context);

	// Save engine destination metadata for the copy on
	// copyRenderTargetToNonRenderTargetTexture.
	ms_userTexture      = texture;
	ms_userCubeFace     = cubeFace;
	ms_userMip          = mipmapLevel;
	ms_primaryTargetSet = false;

	if (!texture->isRenderTarget())
	{
		// User passed an ordinary texture; we render into the persistent
		// 512x512 baked target and copy out on the matching unbind. Cap
		// the copy region to the user texture's dimensions at the
		// requested mip level.
		ms_copyWidth  = (std::max)(1, texture->getWidth()  >> mipmapLevel);
		ms_copyHeight = (std::max)(1, texture->getHeight() >> mipmapLevel);

		if (ms_copyWidth > cms_bakedTextureMaxDimension || ms_copyHeight > cms_bakedTextureMaxDimension)
		{
			DEBUG_WARNING(true,
				("Direct3d11_RenderTarget: requested baked-texture region %dx%d exceeds "
				 "%dx%d cap; copy will truncate",
				 ms_copyWidth, ms_copyHeight,
				 cms_bakedTextureMaxDimension, cms_bakedTextureMaxDimension));
			ms_copyWidth  = (std::min)(ms_copyWidth,  cms_bakedTextureMaxDimension);
			ms_copyHeight = (std::min)(ms_copyHeight, cms_bakedTextureMaxDimension);
		}

		// Bind the persistent baked-texture RTV. No depth-stencil (matches
		// the D3D9 plugin's SetDepthStencilSurface(NULL) behaviour).
		ID3D11RenderTargetView * rtvs[1] = { ms_renderTargetView.Get() };
		context->OMSetRenderTargets(1, rtvs, nullptr);
	}
	else
	{
		// Texture is itself a render target -- create (or reuse) an RTV
		// directly on its underlying ID3D11Texture2D / Cube via the
		// Direct3d11_TextureData helper.
		Direct3d11_TextureData const *texData =
			static_cast<Direct3d11_TextureData const *>(texture->getGraphicsData());
		NOT_NULL(texData);

		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		// We don't know the exact format here without re-querying the
		// resource; let the runtime infer with a nullptr desc (works for
		// non-typeless formats which the engine uses exclusively).
		ms_userRTV.Reset();
		HRESULT const hr = device->CreateRenderTargetView(texData->getTexture(), nullptr, &ms_userRTV);
		FATAL_DX_HR("Direct3d11_RenderTarget::setRenderTarget CreateRenderTargetView failed: %s", hr);

		UNREF(rtvDesc);

		ID3D11RenderTargetView * rtvs[1] = { ms_userRTV.Get() };
		context->OMSetRenderTargets(1, rtvs, nullptr);

		ms_copyWidth  = (std::max)(1, texture->getWidth()  >> mipmapLevel);
		ms_copyHeight = (std::max)(1, texture->getHeight() >> mipmapLevel);
	}

	// Update the viewport to match the active render target's dimensions.
	D3D11_VIEWPORT vp = {};
	vp.TopLeftX = 0.0f;
	vp.TopLeftY = 0.0f;
	vp.Width    = static_cast<float>(ms_copyWidth);
	vp.Height   = static_cast<float>(ms_copyHeight);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	context->RSSetViewports(1, &vp);
}

// ----------------------------------------------------------------------

bool Direct3d11_RenderTarget::copyRenderTargetToNonRenderTargetTexture()
{
	// End-of-baking handler. Two paths mirroring setRenderTarget above:
	//
	//   1. User texture was a render target -> nothing to copy; just
	//      release the transient RTV and restore primary.
	//
	//   2. User texture was an ordinary texture -> copy the baked region
	//      out of ms_renderTargetTexture into the engine texture's
	//      underlying ID3D11Texture2D. Use CopySubresourceRegion (same-size
	//      blit, no format conversion -- mirrors D3D9
	//      GetRenderTargetData + D3DXLoadSurfaceFromSurface but cheaper
	//      because no system-memory intermediate is needed).

	if (!ms_userTexture)
	{
		// No active bake -- nothing to do.
		setRenderTargetToPrimary();
		return true;
	}

	if (!ms_userTexture->isRenderTarget())
	{
		ID3D11DeviceContext * const context = Direct3d11_Device::getContext();
		NOT_NULL(context);

		Direct3d11_TextureData const *texData =
			static_cast<Direct3d11_TextureData const *>(ms_userTexture->getGraphicsData());
		NOT_NULL(texData);

		UINT const dstSubresource =
			(ms_userCubeFace != CF_none)
				? D3D11CalcSubresource(
					static_cast<UINT>(ms_userMip),
					static_cast<UINT>(ms_userCubeFace),
					static_cast<UINT>(ms_userTexture->getMipmapLevelCount()))
				: static_cast<UINT>(ms_userMip);

		D3D11_BOX srcBox = {};
		srcBox.left   = 0;
		srcBox.top    = 0;
		srcBox.front  = 0;
		srcBox.right  = static_cast<UINT>(ms_copyWidth);
		srcBox.bottom = static_cast<UINT>(ms_copyHeight);
		srcBox.back   = 1;

		context->CopySubresourceRegion(
			texData->getTexture(),
			dstSubresource,
			0, 0, 0,
			ms_renderTargetTexture.Get(),
			0,
			&srcBox);
	}

	// Release transient state and restore the primary target.
	ms_userRTV.Reset();
	ms_userTexture = nullptr;

	setRenderTargetToPrimary();
	return true;
}

// ======================================================================
