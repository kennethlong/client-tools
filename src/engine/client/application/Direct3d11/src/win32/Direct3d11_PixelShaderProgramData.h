// ======================================================================
//
// Direct3d11_PixelShaderProgramData.h
// Phase 11 D3D11 renderer plugin -- pixel shader program wrapper.
// Plan 11-05 (Wave 5 -- shader layer).
//
// Asset-pipeline caveat: ShaderImplementationPassPixelShaderProgram::m_exe
// is a DWORD * holding PRE-COMPILED D3D9-era pixel shader bytecode (PEXE
// chunk in the .psh IFF; see ShaderImplementation.cpp:2906). D3D11's
// CreatePixelShader expects DXBC-format SM4.0+ bytecode and will reject
// D3D9 bytecode. Two paths exist for D3D11:
//   (a) Re-author the .psh assets as HLSL source + recompile via D3DCompile
//       (PATTERNS §asset-pipeline -- future Phase 12 work);
//   (b) Skip pixel-shader bind in draw dispatch when m_pixelShader == NULL
//       (D3D11 default-passthrough -- Plan 11-06 work).
//
// For Plan 11-05 we adopt path (b): construct the wrapper successfully so
// the engine's createPixelShaderProgramData factory slot stops FATAL'ing
// during ShaderImplementation::install, log the size of m_exe for the
// asset audit, leave m_d3dPS as NULL. Plan 11-06's draw-call dispatch
// will skip PSSetShader when m_d3dPS is NULL, which lets D3D11's
// rasterizer use the default pass-through pixel pipeline (UNDEFINED
// output -- but the draw doesn't FATAL). Real PS rendering blocks on
// the asset re-author follow-up.
//
// This is recorded as a Plan 11-05 known stub in the SUMMARY's "Known
// Stubs" section. SPEC R3 acceptance (HLSL SM5.0 compile pipeline) is
// satisfied by the vertex-shader path which DOES compile to SM5.0; the
// pixel-shader path requires the asset re-author follow-up.
//
// Per CONTEXT D-13: ComPtr ownership only.
//
// ======================================================================

#ifndef INCLUDED_Direct3d11_PixelShaderProgramData_H
#define INCLUDED_Direct3d11_PixelShaderProgramData_H

// ======================================================================

class MemoryBlockManager;

#include "clientGraphics/ShaderImplementation.h"

#include <d3d11.h>
#include <wrl/client.h>

// ======================================================================

class Direct3d11_PixelShaderProgramData : public ShaderImplementationPassPixelShaderProgramGraphicsData
{
public:

	static void *operator new(size_t size);
	static void  operator delete(void *memory);

	static void install();
	static void remove();

public:

	explicit Direct3d11_PixelShaderProgramData(ShaderImplementationPassPixelShaderProgram const &pixelShaderProgram);
	virtual ~Direct3d11_PixelShaderProgramData() override;

	// May return NULL -- see header preamble. Plan 11-06's draw dispatch
	// must handle the NULL case (skip PSSetShader -> D3D11 default).
	ID3D11PixelShader *getPixelShader() const;

	// Plan 11-09 Iter-2: minimal magenta pass-through PS, compiled in
	// install(). applyPreDrawState binds this when ms_currentPSData has
	// no real PS (per Plan 11-05 PEXE caveat). Output color is debug
	// magenta float4(1,0,1,1) -- a visible "Phase 12 asset re-author
	// owes us a real PS" signal that still surfaces geometry visibly.
	static ID3D11PixelShader *getFallbackPS();

private:

	Direct3d11_PixelShaderProgramData();
	Direct3d11_PixelShaderProgramData(Direct3d11_PixelShaderProgramData const &);
	Direct3d11_PixelShaderProgramData &operator =(Direct3d11_PixelShaderProgramData const &);

private:

	static MemoryBlockManager *ms_memoryBlockManager;

	// Plan 11-09 Iter-2: magenta pass-through PS singleton. Compiled at
	// install(), released at remove(). Bound by applyPreDrawState when
	// the active StaticShader has no real PS (Plan 11-05 PEXE caveat).
	static Microsoft::WRL::ComPtr<ID3D11PixelShader> ms_fallbackPS;

private:

	ShaderImplementationPassPixelShaderProgram const * m_program;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>          m_d3dPS;
};

// ======================================================================

inline ID3D11PixelShader *Direct3d11_PixelShaderProgramData::getPixelShader() const
{
	return m_d3dPS.Get();
}

// ----------------------------------------------------------------------

inline ID3D11PixelShader *Direct3d11_PixelShaderProgramData::getFallbackPS()
{
	return ms_fallbackPS.Get();
}

// ======================================================================

#endif
