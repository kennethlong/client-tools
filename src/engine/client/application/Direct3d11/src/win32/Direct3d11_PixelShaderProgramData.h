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

#include <cstdint>
#include <d3d11.h>
#include <d3d11shader.h>      // Plan 17-02 HIGH-2: D3D_REGISTER_COMPONENT_TYPE for reflected PS input signature
#include <unordered_map>
#include <vector>             // Plan 17-02 HIGH-2: std::vector for reflected PS cbuffer layouts + input sig
#include <wrl/client.h>

class Direct3d11_VertexShaderData;

// ======================================================================
//
// Plan 17-02 HIGH-2 reflected-PS-cbuffer descriptors. Captured once per
// compile via D3DReflect inside the PS-program ctor's recompile-lane
// branch; consumed by Plan 17-03's Direct3d11_StaticShaderData::apply
// per-draw upload path. D3DReflect is NEVER called per-draw -- the cache
// here is the single canonical source.
//
// Mirrors the Direct3d11_ReflectedVSInput/Output precedent at
// Direct3d11_VertexShaderData.h:53-90. Truncation-WARN (R3-02e) fires
// once if a reflected name doesn't fit; Plan 03 looks up variables by
// exact name match, so silent truncation -> silent zero upload.

struct Direct3d11_ReflectedPSCbufferVar
{
	char    Name[64];        // R3-02e: WARN once on truncation
	UINT    StartOffset;     // D3D11_SHADER_VARIABLE_DESC.StartOffset
	UINT    Size;            // D3D11_SHADER_VARIABLE_DESC.Size
};

struct Direct3d11_ReflectedPSCbufferLayout
{
	char                                            Name[64];        // cbuffer name (e.g. "PerMaterial" or rewriter-emitted name); R3-02e WARN once on truncation
	UINT                                            BindPoint;       // D3D11_SHADER_INPUT_BIND_DESC.BindPoint
	UINT                                            TotalSize;       // D3D11_SHADER_BUFFER_DESC.Size -- needed by Plan 03 Contract A staging-buffer upload
	std::vector<Direct3d11_ReflectedPSCbufferVar>   Vars;
};

struct Direct3d11_ReflectedPSInputSig
{
	char                            SemanticName[32];   // R3-02e: WARN once on truncation
	UINT                            SemanticIndex;
	UINT                            Register;
	UINT                            Mask;
	D3D_REGISTER_COMPONENT_TYPE     ComponentType;
};

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

	// Plan 17-02 HIGH-2 accessors -- Plan 03 consumes these to drive the
	// per-draw constant upload. NEVER call D3DReflect at apply() time; the
	// cache populated in the recompile-lane ctor branch (when m_d3dPS was
	// bound from a //hlsl PSRC) is the single canonical source.
	//
	// All three return empty / null when the recompile lane didn't run
	// (no PSRC retained / PSRC was asm / PSRC compile failed) -- callers
	// must handle the empty case by skipping the upload.
	ID3DBlob *                                                  getPsBytecode() const;
	std::vector<Direct3d11_ReflectedPSCbufferLayout> const &    getReflectedCbufferLayouts() const;
	std::vector<Direct3d11_ReflectedPSInputSig> const &         getReflectedPSInputs() const;

	// Plan 11-09 Iter-2: minimal magenta pass-through PS, compiled in
	// install(). applyPreDrawState binds this when ms_currentPSData has
	// no real PS (per Plan 11-05 PEXE caveat) AND getOrCompilePSForVS
	// returns null (compile-failure tombstone). Output color is debug
	// magenta float4(1,0,1,1) -- a visible "Phase 12 asset re-author
	// owes us a real PS" signal that still surfaces geometry visibly.
	static ID3D11PixelShader *getFallbackPS();

	// Plan 11-09.13 Iter-4: per-VS dynamic PS compile (Bucket 2 override
	// of CODEX's Bucket 1 recommendation; see PLAN.md
	// <iter4_overrides_codex_bucket1>). At first encounter of each VS
	// (keyed by VS bytecode hash), this generates a PS whose input
	// struct mirrors the VS's reflected output struct register-for-
	// register so D3D11 stage linkage is satisfied semantically AND
	// positionally. Compile result is cached; compile failure stores a
	// nullptr tombstone to avoid retry storms. Returns the cached PS,
	// or null on tombstone (caller must fall back to getFallbackPS).
	//
	// Iter-3 used a static Variant T textured-passthrough PS; that path
	// produced 65,034 id=343 register-position mismatch errors per smoke
	// session because Variant T declared TEXCOORD0 at register v0 while
	// most engine VSes output TEXCOORD0 at o1+/o2+. Per-VS dynamic
	// compile eliminates the positional mismatch by construction.
	//
	// Plan 11-09.14 (CODEX Bucket A): the SRV0 binding gap is now closed.
	// Direct3d11_StaticShaderData::apply() binds the slot-0 diffuse SRV +
	// sampler via Direct3d11_StateCache::setPixelShaderResource(0, srv) +
	// setPixelShaderSampler(0, desc). Dynamic PSes sampling register(t0)
	// now read actual texture data for passes whose pixelShader has a
	// TextureSampler with m_textureIndex == 0. Slots 1..7 still deferred
	// to Phase 12 real per-asset PS compile.
	static ID3D11PixelShader *getOrCompilePSForVS(Direct3d11_VertexShaderData const *vsData);

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

	// Plan 11-09.13 Iter-4: per-VS dynamic PS cache. Keyed by VS
	// bytecode hash; populated lazily by getOrCompilePSForVS at first
	// draw using each VS. Entries may be null (compile-failure
	// tombstone). Cleared in remove().
	static std::unordered_map<uint64_t, Microsoft::WRL::ComPtr<ID3D11PixelShader>> ms_perVSCache;

private:

	ShaderImplementationPassPixelShaderProgram const * m_program;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>          m_d3dPS;

	// Plan 17-02 HIGH-2: retained DXBC blob from the recompile-lane
	// success path; empty otherwise. Plan 03 may need GetBufferPointer()
	// for re-reflection or contracts; the cached layouts below are the
	// hot-path consumer.
	Microsoft::WRL::ComPtr<ID3DBlob>                          m_psBytecodeBlob;

	// Plan 17-02 HIGH-2 + HIGH-4: cached reflection of the recompiled PS.
	// Populated ONCE in the ctor's recompile-lane success path; consumed
	// per-draw by Plan 03's apply() without ever calling D3DReflect again.
	// Empty vectors when the recompile lane did not run or reflection
	// failed (defensive non-fatal -- Plan 03 must skip the upload in that
	// case, matching the VS m_reflectedInputs/Outputs empty-vector contract
	// at Direct3d11_VertexShaderData.h:158/:161).
	std::vector<Direct3d11_ReflectedPSCbufferLayout>          m_reflectedCbufferLayouts;
	std::vector<Direct3d11_ReflectedPSInputSig>               m_reflectedPSInputs;
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

// ----------------------------------------------------------------------

inline ID3DBlob *Direct3d11_PixelShaderProgramData::getPsBytecode() const
{
	return m_psBytecodeBlob.Get();
}

// ----------------------------------------------------------------------

inline std::vector<Direct3d11_ReflectedPSCbufferLayout> const &Direct3d11_PixelShaderProgramData::getReflectedCbufferLayouts() const
{
	return m_reflectedCbufferLayouts;
}

// ----------------------------------------------------------------------

inline std::vector<Direct3d11_ReflectedPSInputSig> const &Direct3d11_PixelShaderProgramData::getReflectedPSInputs() const
{
	return m_reflectedPSInputs;
}

// ======================================================================

#endif
