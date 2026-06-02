// ======================================================================
//
// Direct3d11_VertexShaderData.h
// Phase 11 D3D11 renderer plugin -- vertex shader (HLSL source) compile +
// ID3D11VertexShader ownership + input-layout cache. Plan 11-05.
//
// Plan 17-09 (GAP-6 fix): the VS is now compiled PER textureCoordinateSetKey,
// mirroring D3D9 Direct3d9_VertexShaderData::createVertexShader. The asset
// .vsh declares its texcoord inputs through named tags (MAIN/NRML/DOT3/...)
// via leading `#define textureCoordinateSet<TAG> textureCoordinateSet<N>` +
// `DECLARE_textureCoordinateSets`. D3D9 STRIPS those defaults and regenerates
// them per-mesh from a key (each tag -> the mesh texcoord set index that feeds
// it), so a 1-UV+DOT3 mesh aliases MAIN/NRML to set0 and lands DOT3 at set1.
// The pre-17-09 D3D11 path compiled the source defaults verbatim (MAIN=0,
// NRML=1, DOT3=2), producing a 3-input signature the 2-set mesh can't satisfy
// -> the bump VS read its tangent from an unfed TEXCOORD2 -> NaN. We now
// replicate the per-key remap, so each key produces its own compiled variant.
//
// Per RESEARCH Pitfall 1: POSITION -> SV_POSITION macro injected.
// Per RESEARCH Pitfall 6: input-layout cache keyed by (VBFormat,
// VS-bytecode-hash) -- per-key variants have distinct bytecode/hash so the
// layout cache composes with no cache-key change.
//
// ======================================================================

#ifndef INCLUDED_Direct3d11_VertexShaderData_H
#define INCLUDED_Direct3d11_VertexShaderData_H

// ======================================================================

class MemoryBlockManager;

#include "clientGraphics/ShaderImplementation.h"
#include "sharedFoundation/Tag.h"            // Plan 17-09: Tag + ConvertStringToTag for texcoord-set tags

#include <cstdint>
#include <d3d11.h>
#include <d3d11shader.h>     // Plan 11-09.8: ID3D11ShaderReflection + D3D11_SIGNATURE_PARAMETER_DESC
#include <d3dcompiler.h>     // D3DReflect + D3DCompile
#include <unordered_map>     // Plan 17-09: per-key variant cache
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
// System-value outputs (SV_POSITION etc.) are FILTERED OUT before storage
// -- only "normal" semantics that may map to a PS input are recorded.

struct Direct3d11_ReflectedVSOutput
{
	char                         SemanticName[16];   // null-terminated; truncate beyond 15 chars
	UINT                         SemanticIndex;
	UINT                         Register;           // Plan 11-09.13 Iter-4: HW output register the HLSL compiler assigned (o0/o1/o2/...). Load-bearing for D3D11 stage linkage.
	UINT                         ComponentMask;      // .Mask from D3D11_SIGNATURE_PARAMETER_DESC
	UINT                         ReadWriteMask;      // .ReadWriteMask -- which components VS wrote
	D3D_REGISTER_COMPONENT_TYPE  ComponentType;
};

// ======================================================================

class Direct3d11_VertexShaderData : public ShaderImplementationPassVertexShaderGraphicsData
{
public:

	// Plan 17-09: one compiled variant per textureCoordinateSetKey. All the
	// per-compile state that used to be single-instance members now lives here,
	// keyed by the per-pass texcoord-set mapping. The input-layout cache and PS
	// reconstruction key off bytecodeHash / outputSigHash, both per-variant.
	struct Variant
	{
		Microsoft::WRL::ComPtr<ID3D11VertexShader>   d3dVS;
		Microsoft::WRL::ComPtr<ID3DBlob>             bytecode;
		uint64_t                                     bytecodeHash = 0;
		std::vector<Direct3d11_ReflectedVSInput>     reflectedInputs;
		std::vector<Direct3d11_ReflectedVSOutput>    reflectedOutputs;
		uint32_t                                     outputSigHash = 0;   // Plan 17-07 PS-rewrite-cache salt (outputs only)
	};

	static void *operator new(size_t size);
	static void  operator delete(void *memory);

	static void install();
	static void remove();

public:

	explicit Direct3d11_VertexShaderData(ShaderImplementationPassVertexShader const &vertexShader);
	virtual ~Direct3d11_VertexShaderData() override;

	// Plan 17-09: the ordered texcoord-set tags parsed from the .vsh header
	// (MAIN/NRML/DOT3/...). Direct3d11_StaticShaderData uses this list to build
	// the per-pass textureCoordinateSetKey (mirror Direct3d9_StaticShaderData::
	// Pass::construct). Empty for tag-less shaders (key is then irrelevant).
	std::vector<Tag> const & getTextureCoordinateSetTags() const;

	// Plan 17-09: fetch (lazily compiling on first request) the compiled variant
	// for a given texcoord-set key. The caller (StaticShaderData via StateCache)
	// passes the key EXPLICITLY -- never read from ambient state in cached/async
	// layout creation. Assembly / compile-failure VS data returns the empty
	// variant (null d3dVS) and the draw is skipped, matching prior behavior.
	Variant const & getVariant(uint32 textureCoordinateSetKey) const;

	// Plan 11-09.15 Iter-5 diagnostic: engine-side shader access point.
	ShaderImplementationPassVertexShader const *getEngineShader() const { return m_vertexShader; }

	// Phase 19: true when the .vsh is legacy //asm (vs_1_1 / vs_2_0) and the
	// variant therefore carries the GENERIC FALLBACK VS (const-white COLOR0,
	// TEXCOORD0-only, world-transform) rather than the asset's own compiled VS.
	// The draw path uses this to FORCE the reflection-driven fallback PS lane:
	// an asset PS expects the asm-computed varyings, which the fallback VS does
	// not produce, so binding it would render wrong (or magenta). See
	// Direct3d11_StateCache::applyPreDrawState PS-selection.
	bool isAssembly() const { return m_isAssembly; }

private:

	Direct3d11_VertexShaderData();
	Direct3d11_VertexShaderData(Direct3d11_VertexShaderData const &);
	Direct3d11_VertexShaderData &operator =(Direct3d11_VertexShaderData const &);

	// Plan 17-09: compile one variant for `key`, applying the D3D9-equivalent
	// per-tag remap macros + DECLARE_textureCoordinateSets. Returns a reference
	// to the cached variant (inserted into m_variants).
	Variant const & compileVariant(uint32 textureCoordinateSetKey) const;

private:

	static MemoryBlockManager *ms_memoryBlockManager;

private:

	ShaderImplementationPassVertexShader const * m_vertexShader;
	bool                                         m_isAssembly;  // .vsh starts "//asm vs_..."; cannot compile under D3D11

	// Plan 17-09: compile body = source AFTER the stripped leading //hlsl +
	// `#define` lines (mirrors Direct3d9_VertexShaderData m_compileText). The
	// stripped texcoord `#define`s are regenerated per-key in compileVariant.
	char const *                                 m_bodyText;
	size_t                                       m_bodyLen;

	// Plan 17-09: ordered texcoord-set tags (MAIN/NRML/DOT3/...) parsed once.
	std::vector<Tag>                             m_textureCoordinateSetTags;

	// Plan 17-09: per-key compiled variants (lazy). mutable so getVariant() can
	// stay const while compiling on demand.
	mutable std::unordered_map<uint32_t, Variant> m_variants;
};

// ======================================================================

inline std::vector<Tag> const & Direct3d11_VertexShaderData::getTextureCoordinateSetTags() const
{
	return m_textureCoordinateSetTags;
}

// ======================================================================

#endif
