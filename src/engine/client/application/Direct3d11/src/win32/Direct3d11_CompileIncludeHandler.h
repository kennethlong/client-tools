// ======================================================================
//
// Direct3d11_CompileIncludeHandler.h
// Phase 11 D3D11 renderer plugin -- ID3DInclude handler that routes
// HLSL `#include "..."` directives through the engine's TreeFile
// abstraction (TRE archives + on-disk overlay).
//
// Plan 11-07 Iteration 2.
//
// Background: D3DCompile resolves `#include "foo"` directives by calling
// back into an ID3DInclude::Open implementation. Plan 11-05 passed
// D3D_COMPILE_STANDARD_FILE_INCLUDE (=(ID3DInclude*)1) which uses the
// default Win32 file-system handler -- this cannot see TRE-archived
// asset content. SWG ships its `.inc` headers (vertex_shader_constants.inc,
// pixel_shader_constants.inc, etc.) inside the TRE bundle pointed at by
// [SharedFile] in client_d.cfg, so D3DCompile fails with X1507
// "failed to open source file".
//
// This handler resolves both D3D_INCLUDE_LOCAL (`#include "..."`) and
// D3D_INCLUDE_SYSTEM (`#include <...>`) through `TreeFile::open()`. The
// engine's TreeFile already handles the search-order semantics (overlay
// directories first, TRE archive content second). Engine HLSH assets
// use LOCAL exclusively today; SYSTEM is routed identically as a safe
// default.
//
// Per CONTEXT D-13: no D3DPOOL_MANAGED / OnLostDevice / OnResetDevice
// (the handler does not own GPU resources at all).
//
// Per Plan 11-04's D-05 invariant: D3D9 plugin source untouched. D3D9
// has its own ID3DXInclude implementation at
// Direct3d9_VertexShaderData.cpp:54-156 with its own include cache; the
// D3D11 handler is independent.
//
// ======================================================================

#ifndef INCLUDED_Direct3d11_CompileIncludeHandler_H
#define INCLUDED_Direct3d11_CompileIncludeHandler_H

// ======================================================================

#include <d3dcompiler.h>

// ======================================================================

class Direct3d11_CompileIncludeHandler : public ID3DInclude
{
public:

	// Singleton accessor -- the handler is stateless (no per-compile
	// member state) so a single instance can service every D3DCompile
	// call site. Returns a non-owning ID3DInclude* suitable to pass as
	// the `pInclude` parameter to D3DCompile.
	static ID3DInclude *getInstance();

public:

	// ID3DInclude vtable.
	virtual HRESULT __stdcall Open(
		D3D_INCLUDE_TYPE IncludeType,
		LPCSTR           pFileName,
		LPCVOID          pParentData,
		LPCVOID *        ppData,
		UINT *           pBytes) override;

	virtual HRESULT __stdcall Close(LPCVOID pData) override;

private:

	Direct3d11_CompileIncludeHandler() = default;
	~Direct3d11_CompileIncludeHandler() = default;

	Direct3d11_CompileIncludeHandler(Direct3d11_CompileIncludeHandler const &) = delete;
	Direct3d11_CompileIncludeHandler &operator=(Direct3d11_CompileIncludeHandler const &) = delete;
};

// ======================================================================

#endif
