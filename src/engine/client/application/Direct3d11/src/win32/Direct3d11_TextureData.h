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

#include <d3d11.h>
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
	TextureFormat                                     m_destFormat;
	bool                                              m_isCubeMap;
	bool                                              m_isVolumeMap;

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
