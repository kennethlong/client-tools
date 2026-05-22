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

// ======================================================================

#endif
