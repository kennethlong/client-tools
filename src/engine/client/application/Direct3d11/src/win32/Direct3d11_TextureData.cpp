// ======================================================================
//
// Direct3d11_TextureData.cpp
// Phase 11 D3D11 renderer plugin -- texture resource implementation.
//
// Plan 11-04 (Wave 4 -- resource layer). Replaces createTextureData
// scaffold_fatal_stub. Owns ID3D11Texture2D / Cube / Volume + paired
// ID3D11ShaderResourceView via Microsoft::WRL::ComPtr (CONTEXT D-01).
// Per CONTEXT D-13 / SPEC R2 D3D11-02: NO D3DPOOL_MANAGED, NO
// OnLostDevice, NO OnResetDevice -- resource lifetime is per-instance
// ComPtr Release on destructor.
//
// DXGI_FORMAT translation table mirrors D3D9 plugin's D3DFORMAT table
// shape (17 functional entries + 2 sentinels), per PATTERNS
// §Direct3d11_TextureData.{h,cpp} lines 343-360 + engine TextureFormat
// enum order (Texture.def lines 14-37).
//
// Per RESEARCH Pitfall 4: SRV is independent from sampler -- this file
// creates the SRV; sampler bind lives in the state-cache plan (Wave 6).
//
// ======================================================================

#include "FirstDirect3d11.h"
#include "Direct3d11_TextureData.h"

#include "Direct3d11.h"
#include "Direct3d11_Device.h"

#include "clientGraphics/TextureFormatInfo.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedFoundation/MemoryBlockManager.h"
#include "sharedFoundation/Os.h"

#include <d3d11.h>
#include <wrl/client.h>

#include <map>

// ======================================================================

using Microsoft::WRL::ComPtr;

const Tag TAG_ENVM = TAG(E,N,V,M);

// ======================================================================

MemoryBlockManager                       *Direct3d11_TextureData::ms_memoryBlockManager;
Direct3d11_TextureData::GlobalTextureList *Direct3d11_TextureData::ms_globalTextureList;

// DXGI_FORMAT translation table -- one entry per TextureFormat enum value
// (Texture.def TF_ARGB_8888..TF_ABGR_32F + TF_Count + TF_Native sentinels).
// Mirrors Direct3d9_TextureData.cpp:33-54 layout. Unsupported / unmappable
// formats use DXGI_FORMAT_UNKNOWN; the install-time CheckFeatureSupport
// pass below marks those entries unsupported and the engine falls through
// to the next runtimeFormat candidate.
static const DXGI_FORMAT translationTable[] =
{
	DXGI_FORMAT_B8G8R8A8_UNORM,     // TF_ARGB_8888
	DXGI_FORMAT_B4G4R4A4_UNORM,     // TF_ARGB_4444 (Win8+ only; install probe marks unsupported on older)
	DXGI_FORMAT_B5G5R5A1_UNORM,     // TF_ARGB_1555
	DXGI_FORMAT_B8G8R8X8_UNORM,     // TF_XRGB_8888
	DXGI_FORMAT_UNKNOWN,            // TF_RGB_888  -- no 24bpp packed in DXGI; engine promotes via runtimeFormats
	DXGI_FORMAT_B5G6R5_UNORM,       // TF_RGB_565
	DXGI_FORMAT_B5G5R5A1_UNORM,     // TF_RGB_555  -- closest DXGI; alpha bit unused by engine
	DXGI_FORMAT_BC1_UNORM,          // TF_DXT1
	DXGI_FORMAT_UNKNOWN,            // TF_DXT2     -- premultiplied BC2; no native DXGI variant; engine promotes
	DXGI_FORMAT_BC2_UNORM,          // TF_DXT3
	DXGI_FORMAT_UNKNOWN,            // TF_DXT4     -- premultiplied BC3; no native DXGI variant; engine promotes
	DXGI_FORMAT_BC3_UNORM,          // TF_DXT5
	DXGI_FORMAT_A8_UNORM,           // TF_A_8
	DXGI_FORMAT_R8_UNORM,           // TF_L_8      -- sample with .rrr swizzle in HLSL (Plan 11-05 generators)
	DXGI_FORMAT_UNKNOWN,            // TF_P_8      -- palettized normal-map; D3D11 has no P_8; engine promotes
	DXGI_FORMAT_R16G16B16A16_FLOAT, // TF_ABGR_16F
	DXGI_FORMAT_R32G32B32A32_FLOAT, // TF_ABGR_32F
	DXGI_FORMAT_UNKNOWN,            // TF_Count (sentinel)
	DXGI_FORMAT_UNKNOWN             // TF_Native (sentinel)
};

// Compile-time check: translation table length must cover TF_Native sentinel
// (TF_Native enum value = TF_Count + 1, so table length = TF_Count + 2).
static_assert(sizeof(translationTable) / sizeof(translationTable[0]) == (TF_Count + 2),
              "translationTable must have one entry per TextureFormat enum value (TF_ARGB_8888..TF_Native)");

// Identifies the block-compressed DXGI families (BC1..BC7). All BC formats
// use a 4x4 pixel block as the indivisible storage unit; D3D11 strict-
// rejects USAGE_STAGING CreateTexture2D/CreateTexture3D when Width or
// Height is not a multiple of 4 for these formats (D3D9 silently tolerated
// the sub-block bottom-of-chain mip levels). Used in lock() to pad the
// staging-texture descriptor up to the block boundary; the BC block grid
// covers the same data regardless of whether the locked mip's logical
// dimensions are sub-block, so RowPitch / engine payload / unlock
// CopySubresourceRegion all behave identically.
//
// Range coverage:
//   BC1_TYPELESS (70) .. BC5_SNORM (84)       -- BC1/BC2/BC3/BC4/BC5
//   BC6H_TYPELESS (94) .. BC7_UNORM_SRGB (99) -- BC6H/BC7
// Values 85..93 are non-BC formats (B5G6R5, BGRA8, etc.) interleaved
// between the two BC ranges, so a single >= && <= check is incorrect.
static bool isBlockCompressedFormat(DXGI_FORMAT fmt)
{
	return (fmt >= DXGI_FORMAT_BC1_TYPELESS && fmt <= DXGI_FORMAT_BC5_SNORM)
	    || (fmt >= DXGI_FORMAT_BC6H_TYPELESS && fmt <= DXGI_FORMAT_BC7_UNORM_SRGB);
}

// ======================================================================
// install -- create MemoryBlockManager + probe DXGI_FORMAT support.
// ======================================================================

void Direct3d11_TextureData::install()
{
	DEBUG_FATAL(ms_memoryBlockManager, ("Direct3d11_TextureData already installed"));
	ms_memoryBlockManager = new MemoryBlockManager(
		"Direct3d11_TextureData::memoryBlockManager", true,
		sizeof(Direct3d11_TextureData), 0, 0, 0);

	// Probe DXGI format support against the live device. CheckFormatSupport
	// returns the set of usage flags supported for the format; we treat
	// "texture-2D bindable" as supported for the engine-side TextureFormatInfo
	// table (used by Texture::loadSurface to choose a runtime format).
	ID3D11Device * const device = Direct3d11_Device::getDevice();
	NOT_NULL(device);

	for (int i = 0; i < TF_Count; ++i)
	{
		bool supported = false;
		DXGI_FORMAT const native = translationTable[i];
		if (native != DXGI_FORMAT_UNKNOWN)
		{
			UINT support = 0;
			HRESULT const hr = device->CheckFormatSupport(native, &support);
			if (SUCCEEDED(hr) && (support & D3D11_FORMAT_SUPPORT_TEXTURE2D) != 0)
				supported = true;
		}
		TextureFormatInfo::setSupported(static_cast<TextureFormat>(i), supported);
	}

	ms_globalTextureList = new GlobalTextureList;
}

// ----------------------------------------------------------------------

void Direct3d11_TextureData::remove()
{
	if (!ExitChain::isFataling())
	{
		if (ms_memoryBlockManager)
		{
			delete ms_memoryBlockManager;
			ms_memoryBlockManager = nullptr;
		}

		if (ms_globalTextureList)
		{
			while (!ms_globalTextureList->empty())
			{
				ms_globalTextureList->begin()->second.engineTexture->release();
				ms_globalTextureList->erase(ms_globalTextureList->begin());
			}
			delete ms_globalTextureList;
			ms_globalTextureList = nullptr;
		}
	}
}

// ----------------------------------------------------------------------

void Direct3d11_TextureData::releaseAllGlobalTextures()
{
	while (ms_globalTextureList && !ms_globalTextureList->empty())
	{
		ms_globalTextureList->begin()->second.engineTexture->release();
		ms_globalTextureList->erase(ms_globalTextureList->begin());
	}
}

// ----------------------------------------------------------------------

void *Direct3d11_TextureData::operator new(size_t size)
{
	UNREF(size);
	NOT_NULL(ms_memoryBlockManager);
	DEBUG_FATAL(size != sizeof(Direct3d11_TextureData), ("bad size"));
	DEBUG_FATAL(size != static_cast<size_t>(ms_memoryBlockManager->getElementSize()), ("installed with bad size"));
	return ms_memoryBlockManager->allocate();
}

// ----------------------------------------------------------------------

void Direct3d11_TextureData::operator delete(void *pointer)
{
	NOT_NULL(ms_memoryBlockManager);
	ms_memoryBlockManager->free(pointer);
}

// ----------------------------------------------------------------------

bool Direct3d11_TextureData::isGlobalTexture(Tag tag)
{
	return (tag == TAG_ENVM) || ((tag >> 24) & 0xff) == '_';
}

// ----------------------------------------------------------------------

Direct3d11_TextureData const * const *Direct3d11_TextureData::getGlobalTexture(Tag tag)
{
	DEBUG_FATAL(!ms_globalTextureList, ("Not installed"));
	DEBUG_FATAL(!isGlobalTexture(tag), ("Tag is not correct for a global texture"));
	GlobalTextureList::const_iterator i = ms_globalTextureList->find(tag);
	DEBUG_FATAL(i == ms_globalTextureList->end(), ("Could not find requested global texture"));
	return & i->second.d3dTexture;
}

// ----------------------------------------------------------------------

void Direct3d11_TextureData::setGlobalTexture(Tag tag, const Texture &texture)
{
	DEBUG_FATAL(!isGlobalTexture(tag), ("Tag is not correct for a global texture"));
	NOT_NULL(ms_globalTextureList);

	GlobalTextureList::iterator i = ms_globalTextureList->find(tag);
	if (i != ms_globalTextureList->end())
	{
		if (i->second.engineTexture != &texture)
		{
			texture.fetch();
			i->second.engineTexture->release();
			i->second.engineTexture = &texture;
			i->second.d3dTexture    = static_cast<Direct3d11_TextureData const *>(texture.getGraphicsData());
		}
	}
	else
	{
		GlobalTextureInfo gti;
		gti.engineTexture = &texture;
		gti.d3dTexture    = static_cast<Direct3d11_TextureData const *>(texture.getGraphicsData());
		const bool inserted = ms_globalTextureList->insert(GlobalTextureList::value_type(tag, gti)).second;
		UNREF(inserted);
		DEBUG_FATAL(!inserted, ("item was already present in map"));
		texture.fetch();
	}
}

// ----------------------------------------------------------------------

DXGI_FORMAT Direct3d11_TextureData::getDxgiFormat(TextureFormat textureFormat)
{
	return translationTable[textureFormat];
}

// ======================================================================
// Construction -- create ID3D11Texture2D / Cube / Volume + SRV.
// ======================================================================

Direct3d11_TextureData::Direct3d11_TextureData(const Texture &newEngineTexture,
                                               const TextureFormat *runtimeFormats,
                                               int numberOfRuntimeFormats)
:
	TextureGraphicsData(),
	m_engineTexture(newEngineTexture),
	m_texture(),
	m_srv(),
	m_destFormat(TF_RGB_555),
	m_isCubeMap(newEngineTexture.isCubeMap()),
	m_isVolumeMap(newEngineTexture.isVolumeMap())
{
	DEBUG_FATAL(!Os::isMainThread(), ("Creating texture from alternate thread"));

	ID3D11Device * const device = Direct3d11_Device::getDevice();
	NOT_NULL(device);

	const bool isDynamic      = newEngineTexture.isDynamic();
	const bool isRenderTarget = newEngineTexture.isRenderTarget();

	// USAGE / BindFlags matrix (per PATTERNS §interfaces resource matrix):
	//   render-target -> USAGE_DEFAULT + BIND_RENDER_TARGET|BIND_SHADER_RESOURCE
	//   dynamic       -> USAGE_DYNAMIC + BIND_SHADER_RESOURCE + CPU_ACCESS_WRITE
	//   static        -> USAGE_DEFAULT + BIND_SHADER_RESOURCE
	//
	// USAGE_IMMUTABLE would be ideal for static textures but requires
	// initial data at create-time. The engine creates the resource here
	// then writes via lock()/unlock() (Texture::loadSurface), so we cannot
	// use IMMUTABLE without restructuring the load path. USAGE_DEFAULT +
	// UpdateSubresource path is the correct mapping; UpdateSubresource is
	// invoked via the lock/unlock virtuals below using a staging buffer.

	D3D11_USAGE usage         = D3D11_USAGE_DEFAULT;
	UINT        bindFlags     = D3D11_BIND_SHADER_RESOURCE;
	UINT        cpuAccessFlags = 0;
	UINT        miscFlags     = 0;

	if (isRenderTarget)
	{
		usage     = D3D11_USAGE_DEFAULT;
		bindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	}
	else if (isDynamic)
	{
		usage         = D3D11_USAGE_DYNAMIC;
		bindFlags     = D3D11_BIND_SHADER_RESOURCE;
		cpuAccessFlags = D3D11_CPU_ACCESS_WRITE;
	}

	if (m_isCubeMap)
		miscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;

	// Walk the engine-supplied runtime-format list in order; first format
	// whose DXGI mapping is non-UNKNOWN AND CheckFormatSupport accepts wins.

	HRESULT lastResult = E_FAIL;
	for (int i = 0; !m_texture && i < numberOfRuntimeFormats; ++i)
	{
		TextureFormat const candidate = runtimeFormats[i];
		DXGI_FORMAT     const native    = translationTable[candidate];
		if (native == DXGI_FORMAT_UNKNOWN)
			continue;

		// Verify the device actually supports this format for the target
		// resource type. CheckFormatSupport is the runtime equivalent of
		// D3D9's CheckDeviceFormat used in install() above.
		UINT support = 0;
		if (FAILED(device->CheckFormatSupport(native, &support)))
			continue;
		if ((support & D3D11_FORMAT_SUPPORT_TEXTURE2D) == 0 && !m_isVolumeMap)
			continue;
		if (m_isVolumeMap && (support & D3D11_FORMAT_SUPPORT_TEXTURE3D) == 0)
			continue;
		if (isRenderTarget && (support & D3D11_FORMAT_SUPPORT_RENDER_TARGET) == 0)
			continue;

		m_destFormat = candidate;

		if (m_isVolumeMap)
		{
			D3D11_TEXTURE3D_DESC desc = {};
			desc.Width            = static_cast<UINT>(newEngineTexture.getWidth());
			desc.Height           = static_cast<UINT>(newEngineTexture.getHeight());
			desc.Depth            = static_cast<UINT>(newEngineTexture.getDepth());
			desc.MipLevels        = static_cast<UINT>(newEngineTexture.getMipmapLevelCount());
			desc.Format           = native;
			desc.Usage            = usage;
			desc.BindFlags        = bindFlags;
			desc.CPUAccessFlags   = cpuAccessFlags;
			desc.MiscFlags        = 0;  // no cube on 3D

			ComPtr<ID3D11Texture3D> tex3d;
			lastResult = device->CreateTexture3D(&desc, nullptr, &tex3d);
			if (SUCCEEDED(lastResult))
				m_texture = tex3d;
		}
		else
		{
			D3D11_TEXTURE2D_DESC desc = {};
			desc.Width              = static_cast<UINT>(newEngineTexture.getWidth());
			desc.Height             = static_cast<UINT>(newEngineTexture.getHeight());
			desc.MipLevels          = static_cast<UINT>(newEngineTexture.getMipmapLevelCount());
			desc.ArraySize          = m_isCubeMap ? 6 : 1;
			desc.Format             = native;
			desc.SampleDesc.Count   = 1;
			desc.SampleDesc.Quality = 0;
			desc.Usage              = usage;
			desc.BindFlags          = bindFlags;
			desc.CPUAccessFlags     = cpuAccessFlags;
			desc.MiscFlags          = miscFlags;

			ComPtr<ID3D11Texture2D> tex2d;
			lastResult = device->CreateTexture2D(&desc, nullptr, &tex2d);
			if (SUCCEEDED(lastResult))
				m_texture = tex2d;
		}
	}

	if (!m_texture)
	{
		char buffer[1024] = "";
		for (int i = 0; i < numberOfRuntimeFormats; ++i)
		{
			strcat(buffer, TextureFormatInfo::getInfo(runtimeFormats[i]).name);
			strcat(buffer, " ");
		}
		FATAL(true, ("Direct3d11_TextureData: failed to create texture for any runtime format (%s); last HRESULT=%s",
		             buffer, Direct3d11Namespace::hresultString(lastResult)));
	}

	// Create the shader resource view paired with the texture. View
	// dimension matches resource shape (Texture2D / Cube / Texture3D).

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = translationTable[m_destFormat];

	if (m_isVolumeMap)
	{
		srvDesc.ViewDimension              = D3D11_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Texture3D.MostDetailedMip  = 0;
		srvDesc.Texture3D.MipLevels        = static_cast<UINT>(newEngineTexture.getMipmapLevelCount());
	}
	else if (m_isCubeMap)
	{
		srvDesc.ViewDimension              = D3D11_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels      = static_cast<UINT>(newEngineTexture.getMipmapLevelCount());
	}
	else
	{
		srvDesc.ViewDimension              = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip  = 0;
		srvDesc.Texture2D.MipLevels        = static_cast<UINT>(newEngineTexture.getMipmapLevelCount());
	}

	HRESULT const hr = device->CreateShaderResourceView(m_texture.Get(), &srvDesc, &m_srv);
	FATAL_DX_HR("Direct3d11_TextureData: CreateShaderResourceView failed: %s", hr);
}

// ----------------------------------------------------------------------

Direct3d11_TextureData::~Direct3d11_TextureData()
{
	// ComPtr handles Release().
	m_srv.Reset();
	m_texture.Reset();
}

// ----------------------------------------------------------------------

TextureFormat Direct3d11_TextureData::getNativeFormat() const
{
	return m_destFormat;
}

// ----------------------------------------------------------------------
// lock -- map staging-style write. D3D11 USAGE_DEFAULT textures cannot be
// Mapped directly; we use a CPU-side staging texture, fill it via Map, and
// CopySubresourceRegion on unlock. The LockData::m_reserved slot carries
// the staging texture between lock/unlock (mirrors the D3D9 plugin's
// "temporary offscreen surface" pattern).

void Direct3d11_TextureData::lock(LockData &lockData)
{
	DEBUG_FATAL(lockData.getWidth() == 0, ("Locking 0 width area"));
	DEBUG_FATAL(lockData.getHeight() == 0, ("Locking 0 height area"));
	DEBUG_FATAL(lockData.getDepth() == 0, ("Locking 0 depth area"));

	if (lockData.getFormat() == TF_Native)
		lockData.m_format = m_destFormat;

	ID3D11Device *        const device  = Direct3d11_Device::getDevice();
	ID3D11DeviceContext * const context = Direct3d11_Device::getContext();
	NOT_NULL(device);
	NOT_NULL(context);

	// For dynamic textures (USAGE_DYNAMIC), Map(WRITE_DISCARD) is legal
	// directly on the resource. For everything else, allocate a staging
	// texture matching the locked region and map that.
	const bool isDynamic = m_engineTexture.isDynamic();

	if (isDynamic && !m_isVolumeMap && !m_isCubeMap)
	{
		D3D11_MAPPED_SUBRESOURCE mapped = {};
		UINT const subResource = static_cast<UINT>(lockData.getLevel());
		HRESULT const hr = context->Map(
			m_texture.Get(),
			subResource,
			lockData.shouldDiscardContents() ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE,
			0, &mapped);
		FATAL_DX_HR("Direct3d11_TextureData::lock (dynamic Map) failed: %s", hr);

		lockData.m_pixelData  = mapped.pData;
		lockData.m_pitch      = static_cast<int>(mapped.RowPitch);
		lockData.m_slicePitch = static_cast<int>(mapped.DepthPitch);
		lockData.m_reserved   = nullptr;  // no staging needed
		return;
	}

	// Staging path: create a USAGE_STAGING texture matching the locked
	// region, Map it, hand the pointer to the engine, then CopySubresource
	// on unlock.

	DXGI_FORMAT const native = translationTable[lockData.getFormat()];
	if (native == DXGI_FORMAT_UNKNOWN)
	{
		FATAL(true, ("Direct3d11_TextureData::lock: TextureFormat %d has no DXGI mapping",
		             static_cast<int>(lockData.getFormat())));
	}

	if (m_isVolumeMap)
	{
		// Pad BC-format staging dims up to the 4-pixel block boundary; D3D11
		// strict-rejects USAGE_STAGING with sub-block W or H. Depth has no
		// block-alignment rule (BC blocks are 4x4 in the X/Y plane only).
		// See isBlockCompressedFormat() comment for the full rationale.
		UINT stagingWidth  = static_cast<UINT>(lockData.getWidth());
		UINT stagingHeight = static_cast<UINT>(lockData.getHeight());
		if (isBlockCompressedFormat(native))
		{
			stagingWidth  = (stagingWidth  + 3u) & ~3u;
			stagingHeight = (stagingHeight + 3u) & ~3u;
		}

		D3D11_TEXTURE3D_DESC desc = {};
		desc.Width          = stagingWidth;
		desc.Height         = stagingHeight;
		desc.Depth          = static_cast<UINT>(lockData.getDepth());
		desc.MipLevels      = 1;
		desc.Format         = native;
		desc.Usage          = D3D11_USAGE_STAGING;
		desc.BindFlags      = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | (lockData.isReadOnly() ? D3D11_CPU_ACCESS_READ : 0);
		desc.MiscFlags      = 0;

		ID3D11Texture3D *staging = nullptr;
		HRESULT hr = device->CreateTexture3D(&desc, nullptr, &staging);
		FATAL_DX_HR("Direct3d11_TextureData::lock CreateTexture3D (staging) failed: %s", hr);

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		hr = context->Map(staging, 0,
			lockData.isReadOnly() ? D3D11_MAP_READ : D3D11_MAP_WRITE, 0, &mapped);
		FATAL_DX_HR("Direct3d11_TextureData::lock Map (staging 3D) failed: %s", hr);

		lockData.m_pixelData  = mapped.pData;
		lockData.m_pitch      = static_cast<int>(mapped.RowPitch);
		lockData.m_slicePitch = static_cast<int>(mapped.DepthPitch);
		lockData.m_reserved   = staging;  // released in unlock
	}
	else
	{
		// Pad BC-format staging dims up to the 4-pixel block boundary; D3D11
		// strict-rejects USAGE_STAGING CreateTexture2D when W or H is < 4 for
		// BC formats. Iter-15 diagnostic confirmed this fires at the bottom-
		// of-chain mips of BC3 textures: a 256x64 BC3 (DXT5) texture's mip 5
		// is 8x2, and CreateTexture2D returns E_INVALIDARG. D3D9 silently
		// tolerated sub-block dims. The BC block grid covers the same data
		// regardless of whether the logical mip dims are sub-block, so the
		// engine's RowPitch read and payload write are identical, and the
		// unlock CopySubresourceRegion (with pSrcBox = nullptr) copies the
		// whole staging into the destination mip subresource without any
		// changes -- both source and destination resolve to the same BC
		// block grid. See isBlockCompressedFormat() comment for the full
		// DXGI range coverage rationale.
		UINT stagingWidth  = static_cast<UINT>(lockData.getWidth());
		UINT stagingHeight = static_cast<UINT>(lockData.getHeight());
		if (isBlockCompressedFormat(native))
		{
			stagingWidth  = (stagingWidth  + 3u) & ~3u;
			stagingHeight = (stagingHeight + 3u) & ~3u;
		}

		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width            = stagingWidth;
		desc.Height           = stagingHeight;
		desc.MipLevels        = 1;
		desc.ArraySize        = 1;
		desc.Format           = native;
		desc.SampleDesc.Count = 1;
		desc.Usage            = D3D11_USAGE_STAGING;
		desc.BindFlags        = 0;
		desc.CPUAccessFlags   = D3D11_CPU_ACCESS_WRITE | (lockData.isReadOnly() ? D3D11_CPU_ACCESS_READ : 0);
		desc.MiscFlags        = 0;

		ID3D11Texture2D *staging = nullptr;
		HRESULT hr = device->CreateTexture2D(&desc, nullptr, &staging);
		FATAL_DX_HR("Direct3d11_TextureData::lock CreateTexture2D (staging) failed: %s", hr);

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		hr = context->Map(staging, 0,
			lockData.isReadOnly() ? D3D11_MAP_READ : D3D11_MAP_WRITE, 0, &mapped);
		FATAL_DX_HR("Direct3d11_TextureData::lock Map (staging 2D) failed: %s", hr);

		lockData.m_pixelData  = mapped.pData;
		lockData.m_pitch      = static_cast<int>(mapped.RowPitch);
		lockData.m_slicePitch = static_cast<int>(mapped.RowPitch * lockData.getHeight());
		lockData.m_reserved   = staging;  // released in unlock
	}
}

// ----------------------------------------------------------------------
// unlock -- finalize the staging copy and release intermediate resource.

void Direct3d11_TextureData::unlock(LockData &lockData)
{
	ID3D11DeviceContext * const context = Direct3d11_Device::getContext();
	NOT_NULL(context);

	if (!lockData.m_reserved)
	{
		// Dynamic texture path -- Unmap the resource directly.
		context->Unmap(m_texture.Get(), static_cast<UINT>(lockData.getLevel()));
	}
	else
	{
		// Staging path: Unmap the staging texture, then copy its contents
		// into the destination subresource.
		ID3D11Resource * const staging = static_cast<ID3D11Resource *>(lockData.m_reserved);
		context->Unmap(staging, 0);

		// Skip copy for read-only locks (engine wanted to inspect, not modify).
		if (!lockData.isReadOnly())
		{
			UINT const dstSubresource =
				m_isCubeMap
					? D3D11CalcSubresource(
						static_cast<UINT>(lockData.getLevel()),
						static_cast<UINT>(lockData.m_cubeFace),
						static_cast<UINT>(m_engineTexture.getMipmapLevelCount()))
					: static_cast<UINT>(lockData.getLevel());

			D3D11_BOX srcBox = {};
			srcBox.left   = 0;
			srcBox.top    = 0;
			srcBox.front  = 0;
			srcBox.right  = static_cast<UINT>(lockData.getWidth());
			srcBox.bottom = static_cast<UINT>(lockData.getHeight());
			srcBox.back   = static_cast<UINT>(lockData.getDepth());

			context->CopySubresourceRegion(
				m_texture.Get(),
				dstSubresource,
				static_cast<UINT>(lockData.getX()),
				static_cast<UINT>(lockData.getY()),
				static_cast<UINT>(lockData.getZ()),
				staging,
				0,
				&srcBox);
		}

		staging->Release();
	}

	lockData.m_pixelData  = nullptr;
	lockData.m_pitch      = 0;
	lockData.m_slicePitch = 0;
	lockData.m_reserved   = nullptr;
}

// ----------------------------------------------------------------------

void Direct3d11_TextureData::copyFrom(int surfaceLevel,
                                      TextureGraphicsData const &rhs,
                                      int srcX, int srcY, int srcWidth, int srcHeight,
                                      int dstX, int dstY, int dstWidth, int dstHeight)
{
	Direct3d11_TextureData const *src = static_cast<Direct3d11_TextureData const *>(&rhs);
	FATAL(this == src, ("Direct3d11_TextureData::copyFrom() src & dst may not be the same texture"));

	// D3D11 CopySubresourceRegion requires src and dst rect dimensions to
	// match. Stretch-blit (different sizes) would require a render-target +
	// shader pass; defer until Wave 5/6 when that infrastructure exists.
	FATAL(srcWidth != dstWidth || srcHeight != dstHeight,
	      ("Direct3d11_TextureData::copyFrom: src/dst size mismatch not supported (yet); %dx%d -> %dx%d",
	       srcWidth, srcHeight, dstWidth, dstHeight));

	ID3D11DeviceContext * const context = Direct3d11_Device::getContext();
	NOT_NULL(context);

	D3D11_BOX srcBox = {};
	srcBox.left   = static_cast<UINT>(srcX);
	srcBox.top    = static_cast<UINT>(srcY);
	srcBox.front  = 0;
	srcBox.right  = static_cast<UINT>(srcX + srcWidth);
	srcBox.bottom = static_cast<UINT>(srcY + srcHeight);
	srcBox.back   = 1;

	context->CopySubresourceRegion(
		m_texture.Get(), static_cast<UINT>(surfaceLevel),
		static_cast<UINT>(dstX), static_cast<UINT>(dstY), 0,
		src->m_texture.Get(), static_cast<UINT>(surfaceLevel),
		&srcBox);
}

// ======================================================================
