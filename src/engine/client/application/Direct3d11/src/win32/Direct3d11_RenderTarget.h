// ======================================================================
//
// Direct3d11_RenderTarget.h
// Phase 11 D3D11 renderer plugin -- offscreen render target + paired
// SRV for sample-back paths. Plan 11-04 (Wave 4).
//
// Per CONTEXT D-01 clean-rewrite + D-13 (no lost-device): no
// IDirect3DSurface9 system-memory pair; staging texture is created lazily
// on read-back. ComPtr ownership throughout.
//
// ======================================================================

#ifndef INCLUDED_Direct3d11_RenderTarget_H
#define INCLUDED_Direct3d11_RenderTarget_H

// ======================================================================

class Texture;
#include "clientGraphics/Texture.def"

// ======================================================================

class Direct3d11_RenderTarget
{
public:

	static void install();
	static void remove();

	static void setRenderTargetToPrimary();
	static void setRenderTarget(Texture *texture, CubeFace cubeFace, int mipmapLevel);
	static bool copyRenderTargetToNonRenderTargetTexture();
};

// ======================================================================

#endif
