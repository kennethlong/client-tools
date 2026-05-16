// ======================================================================
//
// Direct3d11.h
// Phase 11 D3D11 renderer plugin -- public surface
// (Plan 11-02 scaffold; design reference is Direct3d9.h, but this is a
//  clean rewrite per CONTEXT D-01.)
//
// ======================================================================

#ifndef INCLUDED_Direct3d11_H
#define INCLUDED_Direct3d11_H

// ======================================================================

struct Gl_install;

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

// ----------------------------------------------------------------------
// FATAL_DX_HR replacement helper.
// <dxerr9.h> (D3D9 DXGetErrorString9) is gone in the D3D11 SDK; we route
// HRESULTs through FormatMessageA via Direct3d11Namespace::hresultString.
namespace Direct3d11Namespace { char const * hresultString(HRESULT hr); }
#define FATAL_DX_HR(msg, hr) FATAL(FAILED(hr), (msg, Direct3d11Namespace::hresultString(hr)))

// ----------------------------------------------------------------------
// Plan 11-02 chose TAG_DX11 = TAG3(D,1,1) -- preprocessor concatenation
// of TAG_DIGIT_1 (defined as hex literal 31) is well-formed in Tag.h.
// Fallback spelling (kept here for grep-discoverability if any future
// refactor of Tag.h breaks digit-position macros): TAG3(D,X,B).

// ======================================================================

class Direct3d11
{
public:
	// Lifecycle -- called from Graphics.cpp through Gl_api.install.
	static bool                 install(Gl_install * gl_install);

	// Singleton accessors (will resolve to real ID3D11Device/Context once
	// Wave 3 lands the resource layer; nullptr through the scaffold plan).
	static ID3D11Device *        getDevice();
	static ID3D11DeviceContext * getContext();

	// Capability queries -- fixed values through the scaffold plan.
	static int                  getShaderCapability();          // ShaderCapability(2,0) scaffold; Wave 3 reports SM5.0
	static int                  getVideoMemoryInMegabytes();
	static bool                 supportsPixelShaders();          // always true under D3D11
	static bool                 supportsVertexShaders();         // always true under D3D11
};

// ======================================================================

#endif
