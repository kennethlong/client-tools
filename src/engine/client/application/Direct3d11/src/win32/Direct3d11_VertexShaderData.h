// ======================================================================
//
// Direct3d11_VertexShaderData.h
// Phase 11 D3D11 renderer plugin -- vertex shader (HLSL source) compile +
// ID3D11VertexShader ownership + input-layout cache. Plan 11-05.
//
// Per RESEARCH §Pattern 3: D3DCompile vs_5_0 + ShaderCache wiring.
// Per RESEARCH Pitfall 1: POSITION -> SV_POSITION macro injected.
// Per RESEARCH Pitfall 6: input-layout cache keyed by (VBFormat,
// VS-bytecode-hash) -- the same shader compiled with different defines
// becomes different bytecode and needs its own ID3D11InputLayout.
//
// Per CONTEXT D-01: clean rewrite; Direct3d9_VertexShaderData is design
// reference only. Per D-13: ComPtr ownership only.
//
// Engine-side: the asset pipeline ships HLSL text in
// ShaderImplementationPassVertexShader::m_text (loaded by
// ShaderImplementation.cpp:2155 from <filename>.vsh under
// TreeFile::PriorityData). The .vsh format includes a "//hlsl vs_X_Y" header
// line + the HLSL body; D3D9 parses the header and routes vs_1_1 / vs_2_0
// through D3DXCompileShader. We always compile vs_5_0 under D3D11 (SPEC R3).
//
// ======================================================================

#ifndef INCLUDED_Direct3d11_VertexShaderData_H
#define INCLUDED_Direct3d11_VertexShaderData_H

// ======================================================================

class MemoryBlockManager;

#include "clientGraphics/ShaderImplementation.h"

#include <cstdint>
#include <d3d11.h>
#include <d3d11shader.h>     // Plan 11-09.8: ID3D11ShaderReflection + D3D11_SIGNATURE_PARAMETER_DESC
#include <d3dcompiler.h>     // D3DReflect + D3DCompile
#include <vector>
#include <wrl/client.h>

// ======================================================================
//
// Plan 11-09.8 compact reflected-VS-input descriptor. Captured once per
// compile via D3DReflect; consumed by Direct3d11_VertexBufferDescriptorMap::
// augmentWithPhantomElements at layout-creation time. SemanticName copied
// (not pointer) because the reflection interface that owns the string
// table is released after capture.
//
// System-value inputs (SV_VertexID etc.) are FILTERED OUT before storage
// -- only "normal" semantics that may need a VBFormat or phantom-slot
// element are recorded.

struct Direct3d11_ReflectedVSInput
{
	char                         SemanticName[16];   // null-terminated; truncate beyond 15 chars
	UINT                         SemanticIndex;
	UINT                         ComponentMask;      // .Mask from D3D11_SIGNATURE_PARAMETER_DESC -- 0x1/0x3/0x7/0xF
	D3D_REGISTER_COMPONENT_TYPE  ComponentType;      // D3D_REGISTER_COMPONENT_FLOAT32 etc.
};

// ======================================================================
//
// Plan 11-09.13 Iter-3 compact reflected-VS-output descriptor. Captured
// once per compile via D3DReflect (parallel block to the input-reflection
// loop). Consumed by Direct3d11_StateCache's per-draw fallback-PS variant
// selection (Variant M / Variant T) so D3D11 stage-linkage compatibility
// can be enforced at bind time -- mirroring Plan 11-09.8's input-side
// pattern but on the VS-output / PS-input boundary.
//
// CODEX-endorsed (consult: 11-09.13-CODEX-CONSULT-reflection-driven-
// fallback-variant-selection.md). ReadWriteMask is stored alongside
// ComponentMask because Microsoft Learn signature docs flag both as
// stage-linkage signals: Mask = which components the signature declares,
// ReadWriteMask = which components the VS actually wrote. For Variant T
// selection we check ComponentMask (the signature contract) plus
// ComponentType (must be FLOAT32 for sampling); ReadWriteMask is
// preserved for future telemetry / diagnostics.
//
// System-value outputs (SV_POSITION etc.) are FILTERED OUT before storage
// -- only "normal" semantics that may map to a PS input are recorded.

struct Direct3d11_ReflectedVSOutput
{
	char                         SemanticName[16];   // null-terminated; truncate beyond 15 chars
	UINT                         SemanticIndex;
	UINT                         ComponentMask;      // .Mask from D3D11_SIGNATURE_PARAMETER_DESC
	UINT                         ReadWriteMask;      // .ReadWriteMask -- which components VS wrote
	D3D_REGISTER_COMPONENT_TYPE  ComponentType;
};

// ======================================================================

class Direct3d11_VertexShaderData : public ShaderImplementationPassVertexShaderGraphicsData
{
public:

	static void *operator new(size_t size);
	static void  operator delete(void *memory);

	static void install();
	static void remove();

public:

	explicit Direct3d11_VertexShaderData(ShaderImplementationPassVertexShader const &vertexShader);
	virtual ~Direct3d11_VertexShaderData() override;

	// Accessors used by Plan 11-06 state cache / draw dispatch.
	ID3D11VertexShader *getVertexShader() const;
	ID3DBlob           *getBytecode() const;

	// VS-bytecode hash -- used as the second half of the
	// (VBFormat, VS-bytecode-hash) input-layout cache key (Pitfall 6).
	uint64_t            getBytecodeHash() const;

	// Plan 11-09.8: VS-declared input signature (post-reflection,
	// SV_* filtered). Empty vector if reflection failed (defensive
	// non-fatal); layout creation then proceeds without phantom-
	// element augmentation, matching pre-Plan-11-09.8 behavior.
	std::vector<Direct3d11_ReflectedVSInput> const & getReflectedInputs() const;

	// Plan 11-09.13 Iter-3: VS-declared output signature (post-reflection,
	// SV_* filtered). Empty vector if reflection failed (defensive
	// non-fatal); Direct3d11_StateCache's fallback-PS selection then
	// defaults to Variant M (magenta), matching pre-Iter-3 behavior.
	std::vector<Direct3d11_ReflectedVSOutput> const & getReflectedOutputs() const;

private:

	Direct3d11_VertexShaderData();
	Direct3d11_VertexShaderData(Direct3d11_VertexShaderData const &);
	Direct3d11_VertexShaderData &operator =(Direct3d11_VertexShaderData const &);

	void compileOrLoad(char const *sourceText, size_t sourceLen, char const *displayName);

private:

	static MemoryBlockManager *ms_memoryBlockManager;

private:

	ShaderImplementationPassVertexShader const * m_vertexShader;
	Microsoft::WRL::ComPtr<ID3D11VertexShader>   m_d3dVS;
	Microsoft::WRL::ComPtr<ID3DBlob>             m_bytecode;
	uint64_t                                     m_bytecodeHash;
	bool                                         m_isAssembly;  // .vsh starts "//asm vs_..."; we can't compile that under D3D11

	// Plan 11-09.8: captured post-compile via D3DReflect.
	std::vector<Direct3d11_ReflectedVSInput>     m_reflectedInputs;

	// Plan 11-09.13 Iter-3: VS output signature, parallel capture.
	std::vector<Direct3d11_ReflectedVSOutput>    m_reflectedOutputs;
};

// ======================================================================

inline ID3D11VertexShader *Direct3d11_VertexShaderData::getVertexShader() const
{
	return m_d3dVS.Get();
}

// ----------------------------------------------------------------------

inline ID3DBlob *Direct3d11_VertexShaderData::getBytecode() const
{
	return m_bytecode.Get();
}

// ----------------------------------------------------------------------

inline uint64_t Direct3d11_VertexShaderData::getBytecodeHash() const
{
	return m_bytecodeHash;
}

// ----------------------------------------------------------------------

inline std::vector<Direct3d11_ReflectedVSInput> const & Direct3d11_VertexShaderData::getReflectedInputs() const
{
	return m_reflectedInputs;
}

// ----------------------------------------------------------------------

inline std::vector<Direct3d11_ReflectedVSOutput> const & Direct3d11_VertexShaderData::getReflectedOutputs() const
{
	return m_reflectedOutputs;
}

// ======================================================================

#endif
