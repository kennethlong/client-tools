// ======================================================================
//
// Direct3d11_TextureData.h
// Phase 11 D3D11 renderer plugin -- texture resource (ID3D11Texture2D +
// paired ID3D11ShaderResourceView) implementing TextureGraphicsData.
//
// Plan 11-04 (Wave 4 -- resource layer). Replaces createTextureData
// scaffold_fatal_stub. Per CONTEXT D-01: clean rewrite; Direct3d9_TextureData
// is design reference only. Per D-13: NO D3DPOOL_MANAGED, NO OnLostDevice,
// NO OnResetDevice -- ComPtr ownership only. Per Pitfall 4: SRV is
// independent from sampler -- sampler binding lands in Plan 11-06 state
// cache.
//
// ======================================================================

#ifndef INCLUDED_Direct3d11_TextureData_H
#define INCLUDED_Direct3d11_TextureData_H

// ======================================================================

class MemoryBlockManager;

#include "clientGraphics/Texture.h"
#include "sharedFoundation/Tag.h"

#include <cstdint>
#include <d3d11.h>
#include <vector>
#include <wrl/client.h>

// ======================================================================

class Direct3d11_TextureData : public TextureGraphicsData
{
public:

	static void *operator new(size_t size);
	static void  operator delete(void *pointer);

	static void install();
	static void remove();

	static void releaseAllGlobalTextures();

	static DXGI_FORMAT getDxgiFormat(TextureFormat textureFormat);

	static bool                                  isGlobalTexture(Tag textureTag);
	static Direct3d11_TextureData const * const *getGlobalTexture(Tag textureTag);
	static void                                  setGlobalTexture(Tag textureTag, const Texture &engineTexture);

public:

	Direct3d11_TextureData(const Texture &newEngineTexture, const TextureFormat *runtimeFormats, int numberOfRuntimeFormats);
	virtual ~Direct3d11_TextureData();

	virtual TextureFormat getNativeFormat() const;
	virtual void          lock(LockData &lockData);
	virtual void          unlock(LockData &lockData);

	virtual void          copyFrom(int surfaceLevel,
	                               TextureGraphicsData const & rhs,
	                               int srcX, int srcY, int srcWidth, int srcHeight,
	                               int dstX, int dstY, int dstWidth, int dstHeight);

	ID3D11Resource *           getTexture() const;       // base storage
	ID3D11ShaderResourceView * getShaderResourceView() const;
	TextureFormat              getTextureFormat() const;

	// Plan 11-08 Iter-3a.1: lazy-create and cache an RTV for render-target
	// textures so Direct3d11_RenderTarget::setRenderTarget reuses it across
	// bind cycles instead of creating a fresh one every call. Pre-fix the
	// per-frame churn was ~20,975 CreateRenderTargetView + matching destroy
	// per ~5-minute session (one Create+Destroy pair per RT-bind cycle); the
	// D3D11 debug layer was reporting the create/destroy as INFO traffic in
	// stage/d3d11-debug.log. Single create per RT-texture lifetime after this
	// fix. Pattern mirrors the engine's D3D9 design where each render-target
	// texture owns its IDirect3DSurface9. Caller passes the device so we
	// don't need to circular-include Direct3d11_Device.h.
	//
	// Returns nullptr if the texture is not a render-target OR the RTV
	// creation fails (rare; format-incompat surfaces here).
	ID3D11RenderTargetView *   getOrCreateRenderTargetView(ID3D11Device *device) const;

private:

	// disabled
	Direct3d11_TextureData();
	Direct3d11_TextureData(const Direct3d11_TextureData &);
	Direct3d11_TextureData &operator =(const Direct3d11_TextureData &);

private:

	struct GlobalTextureInfo
	{
		Texture const *                        engineTexture;
		Direct3d11_TextureData const *         d3dTexture;
	};

	typedef stdmap<Tag, GlobalTextureInfo>::fwd  GlobalTextureList;

	static MemoryBlockManager  *ms_memoryBlockManager;
	static GlobalTextureList   *ms_globalTextureList;

private:

	const Texture                                    &m_engineTexture;
	Microsoft::WRL::ComPtr<ID3D11Resource>            m_texture;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  m_srv;

	// Plan 11-08 Iter-3a.1: lazy-cached RTV for render-target textures.
	// Mutable so getOrCreateRenderTargetView() can stay const (matches the
	// engine's "logical const = no observable state change" model -- the
	// cache is an implementation detail). Null when m_engineTexture is
	// not a render target OR the lazy creator hasn't fired yet.
	mutable Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv;

	TextureFormat                                     m_destFormat;
	bool                                              m_isCubeMap;
	bool                                              m_isVolumeMap;

	// Plan 11-09.9: CPU shadow buffer for 24bpp-engine-fill -> 32bpp-DXGI-
	// GPU-surface bridging. D3D11 / DXGI has no 24bpp packed surface format
	// (D3DFMT_R8G8B8 has no DXGI counterpart), so TF_RGB_888 surfaces get
	// allocated under a wider promoted format (TF_XRGB_8888 -> DXGI BGRX or
	// TF_ARGB_8888 -> DXGI BGRA) at create time via the runtimeFormats walk.
	// At lock time we detect the format-width mismatch, hand the engine a
	// CPU shadow at its 24bpp expected stride, then on unlock expand row-by-
	// row into a 32bpp staging texture before the normal CopySubresourceRegion
	// upload path. Plan 11-09.9 covers TF_RGB_888 only; other UNKNOWN-mapped
	// engine formats (TF_DXT2/4, TF_P_8) still FATAL until extended.
	std::vector<uint8_t>                              m_lockBridgeBuffer;
	TextureFormat                                     m_lockBridgeFormat;
	UINT                                              m_lockBridgePitch;
	int                                               m_lockBridgeWidth;
	int                                               m_lockBridgeHeight;
	bool                                              m_lockBridgeActive;

	//lint -esym(1737, Direct3d11_TextureData::operator new)
};

// ======================================================================

inline ID3D11Resource *Direct3d11_TextureData::getTexture() const
{
	return m_texture.Get();
}

// ----------------------------------------------------------------------

inline ID3D11ShaderResourceView *Direct3d11_TextureData::getShaderResourceView() const
{
	return m_srv.Get();
}

// ----------------------------------------------------------------------

inline TextureFormat Direct3d11_TextureData::getTextureFormat() const
{
	return m_destFormat;
}

// ======================================================================

#endif
