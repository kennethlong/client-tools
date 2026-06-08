// ======================================================================
//
// Direct3d11_StaticShaderData.cpp
// Phase 11 D3D11 renderer plugin -- per-StaticShader graphics data wrapper.
// Plan 11-07 Iteration 1.
//
// See header preamble for design rationale (D-04a omission of FFP path,
// minimum-viable iteration-1 contract: construct + record bind, apply
// VS via the pass's vertex-shader graphics-data lookup; per-pass
// blend / material / texture binding lands as later iteration fixes).
//
// ======================================================================

#include "FirstDirect3d11.h"
#include "Direct3d11_StaticShaderData.h"

#include "Direct3d11.h"
#include "Direct3d11_ConstantBuffer.h"   // Plan 17-03 R3-03a: getPerMaterialShadow declaration
#include "Direct3d11_Device.h"   // Plan 11-09.15 Iter-17: getInfoQueue for stage0-resolve diagnostic
#include "Direct3d11_LegacyPSConstants.h"   // Plan 17-08 (GAP-4) Producer B: RMW b0 staging from the shared legacy shadow
#include "Direct3d11_LightManager.h"   // Plan 17-08 (GAP-4) Producer A: per-frame dot3 light snapshot
#include "Direct3d11_PixelShaderProgramData.h"
#include "Direct3d11_StateCache.h"
#include "Direct3d11_TextureData.h"
#include "Direct3d11_VertexShaderData.h"

#include "clientGraphics/Material.h"              // Plan 17-03 SR-1: shader.getMaterial source-data resolution
#include "clientGraphics/ShaderEffect.h"          // m_effect -> ShaderEffect *
#include "clientGraphics/ShaderImplementation.h"
#include "clientGraphics/StaticShader.h"
#include "clientGraphics/StaticShaderTemplate.h"
#include "clientGraphics/Texture.h"
#include "sharedDebug/DebugFlags.h"
#include "sharedFoundation/MemoryBlockManager.h"
#include "sharedMath/Vector.h"   // Plan 17-08 (GAP-4) Producer A: world->object-local light dir

#include <algorithm>
#include <cstring>     // Plan 17-03 R3-03c: std::memcpy + std::strcmp + std::strlen for offset-aware staging buffer
#include <vector>      // Plan 17-03 R3-03c: staging byte buffer
#include <d3d11shader.h>   // Plan 17-06a GAP-2 discovery: ID3D11ShaderReflectionType inner-walk of userConstants
#include <d3dcompiler.h>   // Plan 17-06a GAP-2 discovery: D3DReflect re-reflection at one-shot dump time

// Phase 19 (19-04 Task 0) DEV-ONLY neutral b0 upload value-dump. Compile-guarded
// so it is trivially revertible. Must match the same macro in
// Direct3d11_LightManager.cpp. Set to 0 / delete the guarded block to remove.
// OFF 2026-06-01: b0 clean + color bug fixed; the ~1M-msg [P19FULL] scanner was
// flooding a 1.2GB log + the in-proc D3D11 InfoQueue (32-bit heap pressure).
#define P19_LIGHTDUMP 0

// ======================================================================

MemoryBlockManager *Direct3d11_StaticShaderData::ms_memoryBlockManager = nullptr;

Direct3d11_StaticShaderData const * Direct3d11_StaticShaderData::ms_active     = nullptr;
int                                 Direct3d11_StaticShaderData::ms_activePass = -1;

// ======================================================================
//
// Plan 11-09.14 (CODEX Bucket A): per-pass slot-0 SRV/sampler stage
// extraction lives in construct(); binding lives in apply(). Counters
// surface at remove() shutdown to attribute residual id=353 INFO traffic.
//
// ======================================================================

namespace
{
	// Plan 11-09.15 Iter-44B: alpha-test reference-value tag constants.
	// File-local to mirror the D3D9 sibling at Direct3d9_StaticShaderData.cpp:41-44.
	Tag const TAG_A255 = TAG(A,2,5,5);
	Tag const TAG_A128 = TAG(A,1,2,8);
	Tag const TAG_A001 = TAG(A,0,0,1);
	Tag const TAG_A000 = TAG(A,0,0,0);

	// Plan 11-09.14: lifetime counts of slot-0 stage construction outcomes.
	// Pair with Direct3d11_StateCache::ms_drawsWithSRV0Bound at shutdown to
	// answer "did this run actually feed SRV0 through StaticShaderData::apply?"
	int s_passesTotal                       = 0;
	int s_passesWithSampler0                = 0;  // pass has a TextureSampler with m_textureIndex == 0
	int s_passesWithSampler0TextureResolved = 0;  // ...AND Stage0::m_texture resolved to a non-null pointer-to-pointer

	// Filter combiner per CODEX Q3 rules. Returns one of the eight
	// D3D11_FILTER_MIN_*_MAG_*_MIP_* permutations or D3D11_FILTER_ANISOTROPIC.
	// TF_invalid + cubic variants fall back to linear; TF_none on mip maps to
	// point (D3D11 has no exact "no mip filtering" bit -- CODEX Q3 says point
	// mip is the safe Iter-1 mapping).
	D3D11_FILTER toD3D11Filter(StaticShaderTemplate::TextureFilter minF,
	                           StaticShaderTemplate::TextureFilter magF,
	                           StaticShaderTemplate::TextureFilter mipF)
	{
		using SST = StaticShaderTemplate;
		if (minF == SST::TF_anisotropic || magF == SST::TF_anisotropic)
			return D3D11_FILTER_ANISOTROPIC;

		auto normalize = [](SST::TextureFilter f) -> SST::TextureFilter {
			// linear-fallback for invalid / cubic / unsafe (CODEX Q3)
			if (f == SST::TF_point || f == SST::TF_linear)
				return f;
			return SST::TF_linear;
		};
		SST::TextureFilter const ni = normalize(minF);
		SST::TextureFilter const na = normalize(magF);
		// mip: TF_none -> point; otherwise normalize the same way.
		SST::TextureFilter const np = (mipF == SST::TF_none) ? SST::TF_point : normalize(mipF);

		bool const minLin = (ni == SST::TF_linear);
		bool const magLin = (na == SST::TF_linear);
		bool const mipLin = (np == SST::TF_linear);

		// 8-value table: bit2=min, bit1=mag, bit0=mip (1 = linear, 0 = point)
		int const code = (minLin ? 4 : 0) | (magLin ? 2 : 0) | (mipLin ? 1 : 0);
		switch (code)
		{
		case 0: return D3D11_FILTER_MIN_MAG_MIP_POINT;                            // ppp
		case 1: return D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;                     // ppl
		case 2: return D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;               // plp
		case 3: return D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR;                     // pll
		case 4: return D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;                     // lpp
		case 5: return D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;              // lpl
		case 6: return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;                     // llp
		case 7: return D3D11_FILTER_MIN_MAG_MIP_LINEAR;                           // lll
		default: return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		}
	}

	// Plan 11-09.15 Iter-43: engine ShaderImplementation::Blend / BlendOperation
	// to D3D11_BLEND / D3D11_BLEND_OP. Engine enum order is declarative
	// (ShaderImplementation.h:219-241) and matches the D3D11 enum semantic
	// order one-to-one; no semantic re-shuffle required, just a direct
	// switch translation. Default-arms return the D3D11 "over"-composite
	// default factors so an unrecognized engine value still produces a
	// reasonable blend.
	D3D11_BLEND translateBlend(ShaderImplementationPass::Blend b)
	{
		using SIP = ShaderImplementationPass;
		switch (b)
		{
		case SIP::B_Zero:                    return D3D11_BLEND_ZERO;
		case SIP::B_One:                     return D3D11_BLEND_ONE;
		case SIP::B_SourceColor:             return D3D11_BLEND_SRC_COLOR;
		case SIP::B_InverseSourceColor:      return D3D11_BLEND_INV_SRC_COLOR;
		case SIP::B_SourceAlpha:             return D3D11_BLEND_SRC_ALPHA;
		case SIP::B_InverseSourceAlpha:      return D3D11_BLEND_INV_SRC_ALPHA;
		case SIP::B_DestinationAlpha:        return D3D11_BLEND_DEST_ALPHA;
		case SIP::B_InverseDestinationAlpha: return D3D11_BLEND_INV_DEST_ALPHA;
		case SIP::B_DestinationColor:        return D3D11_BLEND_DEST_COLOR;
		case SIP::B_InverseDestinationColor: return D3D11_BLEND_INV_DEST_COLOR;
		case SIP::B_SourceAlphaSaturate:     return D3D11_BLEND_SRC_ALPHA_SAT;
		default:                             return D3D11_BLEND_SRC_ALPHA;
		}
	}

	D3D11_BLEND_OP translateBlendOp(ShaderImplementationPass::BlendOperation op)
	{
		using SIP = ShaderImplementationPass;
		switch (op)
		{
		case SIP::BO_Add:             return D3D11_BLEND_OP_ADD;
		case SIP::BO_Subtract:        return D3D11_BLEND_OP_SUBTRACT;
		case SIP::BO_ReverseSubtract: return D3D11_BLEND_OP_REV_SUBTRACT;
		case SIP::BO_Min:             return D3D11_BLEND_OP_MIN;
		case SIP::BO_Max:             return D3D11_BLEND_OP_MAX;
		default:                      return D3D11_BLEND_OP_ADD;
		}
	}

	// Plan 11-09.15 Iter-44A: engine ShaderImplementationPass::Compare to
	// D3D11_COMPARISON_FUNC. CODEX caveat preserved: D3D9's Compare[] table
	// at Direct3d9_ShaderImplementationData.cpp:31-40 swaps the mapping for
	// C_GreaterOrEqual and C_NotEqual relative to what the enum comments
	// say (C_GreaterOrEqual -> D3DCMP_NOTEQUAL, C_NotEqual -> D3DCMP_GREATEREQUAL).
	// Assets have been authored against this swap for years; mirroring it
	// in D3D11 keeps parity. The D3D9 table comments themselves note the
	// swap is "wrong" but we preserve it intentionally for asset parity --
	// "fix"-ing it would break content that relies on the historical bug.
	D3D11_COMPARISON_FUNC translateCompare(ShaderImplementationPass::Compare c)
	{
		using SIP = ShaderImplementationPass;
		switch (c)
		{
		case SIP::C_Never:        return D3D11_COMPARISON_NEVER;
		case SIP::C_Less:         return D3D11_COMPARISON_LESS;
		case SIP::C_Equal:        return D3D11_COMPARISON_EQUAL;
		case SIP::C_LessOrEqual:  return D3D11_COMPARISON_LESS_EQUAL;
		case SIP::C_Greater:      return D3D11_COMPARISON_GREATER;
		case SIP::C_GreaterOrEqual: return D3D11_COMPARISON_NOT_EQUAL;       // D3D9 PARITY SWAP
		case SIP::C_NotEqual:     return D3D11_COMPARISON_GREATER_EQUAL;     // D3D9 PARITY SWAP
		case SIP::C_Always:       return D3D11_COMPARISON_ALWAYS;
		default:                  return D3D11_COMPARISON_LESS_EQUAL;
		}
	}

	D3D11_TEXTURE_ADDRESS_MODE toD3D11Address(StaticShaderTemplate::TextureAddress a)
	{
		using SST = StaticShaderTemplate;
		switch (a)
		{
		case SST::TA_wrap:       return D3D11_TEXTURE_ADDRESS_WRAP;
		case SST::TA_mirror:     return D3D11_TEXTURE_ADDRESS_MIRROR;
		case SST::TA_clamp:      return D3D11_TEXTURE_ADDRESS_CLAMP;
		case SST::TA_border:     return D3D11_TEXTURE_ADDRESS_BORDER;
		case SST::TA_mirrorOnce: return D3D11_TEXTURE_ADDRESS_MIRROR_ONCE;
		case SST::TA_invalid:    // post-fallback default
		default:                 return D3D11_TEXTURE_ADDRESS_WRAP;
		}
	}

	// Build a fully-populated D3D11_SAMPLER_DESC for the slot-0 stage,
	// mirroring D3D9 sibling Direct3d9_StaticShaderData.cpp:336-354's
	// invalid-fallback semantics (textureData.X != TF_invalid ? textureData.X
	// : textureSampler.X). Zero-init the desc first (cache-hash padding
	// hazard per Plan 11-06 Risk #3).
	D3D11_SAMPLER_DESC buildSamplerDescForStage0(
		StaticShaderTemplate::TextureData const &textureData,
		ShaderImplementation::Pass::PixelShader::TextureSampler const &textureSampler)
	{
		using SST = StaticShaderTemplate;
		auto resolveAddr = [](SST::TextureAddress td, SST::TextureAddress ts) {
			return td != SST::TA_invalid ? td : ts;
		};
		auto resolveFilter = [](SST::TextureFilter td, SST::TextureFilter ts) {
			return td != SST::TF_invalid ? td : ts;
		};

		SST::TextureAddress const addrU = resolveAddr(textureData.addressU,
		    static_cast<SST::TextureAddress>(textureSampler.m_textureAddressU));
		SST::TextureAddress const addrV = resolveAddr(textureData.addressV,
		    static_cast<SST::TextureAddress>(textureSampler.m_textureAddressV));
		SST::TextureAddress const addrW = resolveAddr(textureData.addressW,
		    static_cast<SST::TextureAddress>(textureSampler.m_textureAddressW));

		SST::TextureFilter const fMin = resolveFilter(textureData.minificationFilter,
		    static_cast<SST::TextureFilter>(textureSampler.m_textureMinificationFilter));
		SST::TextureFilter const fMag = resolveFilter(textureData.magnificationFilter,
		    static_cast<SST::TextureFilter>(textureSampler.m_textureMagnificationFilter));
		SST::TextureFilter const fMip = resolveFilter(textureData.mipFilter,
		    static_cast<SST::TextureFilter>(textureSampler.m_textureMipFilter));

		D3D11_SAMPLER_DESC desc = {};
		desc.Filter         = toD3D11Filter(fMin, fMag, fMip);
		desc.AddressU       = toD3D11Address(addrU);
		desc.AddressV       = toD3D11Address(addrV);
		desc.AddressW       = toD3D11Address(addrW);
		desc.MipLODBias     = 0.0f;
		// CODEX Q3a: D3D11 spec range is [1, 16]; no caps query needed.
		int const aniso = std::max(1, std::min(16, textureData.maxAnisotropy));
		desc.MaxAnisotropy  = static_cast<UINT>(aniso);
		desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		desc.MinLOD         = 0.0f;
		desc.MaxLOD         = D3D11_FLOAT32_MAX;
		// Phase 19 DIAGNOSTIC (P19_MAXLOD0): clamp the ASSET-PS sampler to mip 0
		// only -- the path world geometry actually uses. Corruption gone ->
		// low-mip content/upload-transient implicated. REVERT.
// RESULT 2026-05-31: MaxLOD=0 = NO CHANGE -> mip chain EXONERATED. Reverted.
#define P19_MAXLOD0 0
#if P19_MAXLOD0
		desc.MaxLOD         = 0.0f;
#endif
		// BorderColor zeroed (D3D9 sibling doesn't override; D3D11 default
		// matches D3D9 fallback of opaque-black border).
		return desc;
	}
}

// ======================================================================

void Direct3d11_StaticShaderData::install()
{
	DEBUG_FATAL(ms_memoryBlockManager, ("Direct3d11_StaticShaderData already installed"));
	ms_memoryBlockManager = new MemoryBlockManager(
		"Direct3d11_StaticShaderData", true,
		sizeof(Direct3d11_StaticShaderData), 0, 0, 0);
}

// ----------------------------------------------------------------------

void Direct3d11_StaticShaderData::remove()
{
	// Plan 11-09.14: lifetime construction-side counters surface here so the
	// shutdown log answers "of all passes constructed, how many had a slot-0
	// sampler resolvable to a non-null texture?" Paired with
	// Direct3d11_StateCache::ms_drawsWithSRV0Bound to attribute id=353 residue.
	DEBUG_REPORT_LOG_PRINT(true,
		("Direct3d11_StaticShaderData: shutdown stats passesTotal=%d passesWithSampler0=%d passesWithSampler0TextureResolved=%d\n",
		s_passesTotal, s_passesWithSampler0, s_passesWithSampler0TextureResolved));

	delete ms_memoryBlockManager;
	ms_memoryBlockManager = nullptr;
	ms_active     = nullptr;
	ms_activePass = -1;
}

// ----------------------------------------------------------------------

void Direct3d11_StaticShaderData::beginFrame()
{
	ms_active     = nullptr;
	ms_activePass = -1;
}

// ----------------------------------------------------------------------

void *Direct3d11_StaticShaderData::operator new(size_t size)
{
	UNREF(size);
	NOT_NULL(ms_memoryBlockManager);
	DEBUG_FATAL(size != sizeof(Direct3d11_StaticShaderData), ("wrong new called"));
	return ms_memoryBlockManager->allocate();
}

// ----------------------------------------------------------------------

void Direct3d11_StaticShaderData::operator delete(void *memory)
{
	NOT_NULL(ms_memoryBlockManager);
	ms_memoryBlockManager->free(memory);
}

// ======================================================================

Direct3d11_StaticShaderData::Direct3d11_StaticShaderData(StaticShader const &shader)
:	StaticShaderGraphicsData(),
	m_shader(&shader),
	// Plan 11-09 Iter-1: direct field access mirroring D3D9 sibling
	// (Direct3d9_StaticShaderData.cpp:988-992). CODEX consult initially
	// recommended the public getter chain
	// (.getShaderEffect().getActiveShaderImplementation()), but those
	// methods are non-inline and non-DLLEXPORT in clientGraphics, so plugin
	// DLLs can't link them -- LNK2019 fired on the first build attempt.
	// Field-access-via-friend is the established plugin pattern; D3D8 +
	// D3D9 both do this. Friend declarations in StaticShaderTemplate.h +
	// ShaderEffect.h + ShaderImplementation.h match the D3D9 friend set.
	m_implementation(shader.getStaticShaderTemplate().m_effect->m_implementation),
	m_passVS(),
	m_passPS(),
	m_passVSSlot(),  // UAF fix: live-deref slots (see header)
	m_passPSSlot(),  // UAF fix: live-deref slots (see header)
	m_passStages()  // Plan 11-09.14 (slot 0) / Plan 11-09.15 Iter-44E (all 8 stages)
{
	construct(shader);
}

// ----------------------------------------------------------------------

Direct3d11_StaticShaderData::~Direct3d11_StaticShaderData()
{
	if (ms_active == this)
	{
		ms_active     = nullptr;
		ms_activePass = -1;
	}
	m_shader         = nullptr;
	m_implementation = nullptr;
}

// ----------------------------------------------------------------------

void Direct3d11_StaticShaderData::construct(StaticShader const &shader)
{
	// Plan 11-09 Iter-1: walk ShaderImplementation::m_pass[] and extract
	// per-pass Direct3d11_VertexShaderData + Direct3d11_PixelShaderProgramData
	// pointers. ms_currentVSData / ms_currentPSData wiring happens in apply().
	//
	// m_implementation is populated by the ctor init list via direct field
	// access (D3D9 verbatim mirror): plugin DLLs can't link non-inline
	// clientGraphics member functions, so the CODEX-recommended public
	// getter chain doesn't work here -- see consult file for the discovery.
	// Field-access-via-friend covers all four engine headers:
	//   - StaticShader.h          (Direct3d11_StateCache for m_graphicsData)
	//   - StaticShaderTemplate.h  (Direct3d11_StaticShaderData for m_effect)
	//   - ShaderEffect.h          (Direct3d11_StaticShaderData for m_implementation)
	//   - ShaderImplementation.h  (Direct3d11_StaticShaderData for m_pass)
	m_passVS.clear();
	m_passPS.clear();
	m_passVSSlot.clear();   // UAF fix: live-deref slots
	m_passPSSlot.clear();   // UAF fix: live-deref slots
	m_passTexcoordKey.clear();   // Plan 17-09
	m_passStages.clear();
	m_passMaterial.clear();

	if (!m_implementation)
		return;

	ShaderImplementation::Passes const & passes = *m_implementation->m_pass;
	size_t const passCount = passes.size();
	m_passVS.assign(passCount, nullptr);
	m_passPS.assign(passCount, nullptr);
	m_passVSSlot.assign(passCount, nullptr);   // UAF fix: live-deref slots
	m_passPSSlot.assign(passCount, nullptr);   // UAF fix: live-deref slots
	m_passTexcoordKey.assign(passCount, 0u);   // Plan 17-09
	{
		PerPassStages emptyStages;
		for (auto &s : emptyStages)
			s = Stage{ false, nullptr, {} };
		m_passStages.assign(passCount, emptyStages);
	}
	// Plan 17-03 SR-1: per-pass material source-data cache sized 1:1 with
	// the PS cache. Default-construct to zero/false so any pass that fails
	// resolution below stays at the safe "no material" state.
	{
		PerPassMaterial emptyMat = {};
		emptyMat.m_diffuse         = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		emptyMat.m_specular        = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		emptyMat.m_emissive        = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		emptyMat.m_textureFactor   = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		emptyMat.m_textureFactor2  = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		m_passMaterial.assign(passCount, emptyMat);
	}

	size_t passIndex = 0;
	for (ShaderImplementation::Passes::const_iterator i = passes.begin(); i != passes.end(); ++i, ++passIndex)
	{
		ShaderImplementation::Pass const * const pass = *i;
		if (!pass)
			continue;

		// VS: pass->m_vertexShader is public (ShaderImplementation.h:263);
		// m_graphicsData on ShaderImplementationPassVertexShader is public via
		// ShaderImplementationPassVertexShaderGraphicsData base. May be null
		// when the pass has no VS (FFP path -- D-04a omitted in this plugin).
		if (pass->m_vertexShader && pass->m_vertexShader->m_graphicsData)
		{
			m_passVS[passIndex] = static_cast<Direct3d11_VertexShaderData const *>(
				pass->m_vertexShader->m_graphicsData);
			// UAF fix: store the ADDRESS of the engine owner's m_graphicsData slot so
			// apply() can re-read it LIVE (it is freed+recreated on graphics-data
			// restore; the cast snapshot above would dangle). See header.
			m_passVSSlot[passIndex] = &pass->m_vertexShader->m_graphicsData;

			// Plan 17-09: compute this pass's texcoord-set key, mirroring
			// Direct3d9_StaticShaderData::Pass::construct:550-578. Each VS texcoord
			// tag (MAIN/NRML/DOT3/...) maps to the mesh set index the StaticShader
			// assigns it; 3 bits per tag. apply() resolves the matching VS variant
			// so the recompiled VS reads its texcoords from the right registers
			// (e.g. 1-UV+DOT3 mesh -> MAIN/NRML alias set0, DOT3 -> set1).
			uint32 key = 0;
			std::vector<Tag> const &tags = m_passVS[passIndex]->getTextureCoordinateSetTags();
			int const tagCount = static_cast<int>(tags.size());
			DEBUG_FATAL(tagCount > 8, ("Plan 17-09: more than 8 texcoord-set tags (%d)", tagCount));
			for (int t = 0; t < tagCount && t < 8; ++t)
			{
				uint8 setIndex = 0;
				if (!shader.getTextureCoordinateSet(tags[t], setIndex))
					setIndex = 0;   // mirror D3D9 fallback: missing tag -> set 0
				if (setIndex > 7)
					setIndex = 0;   // mirror D3D9 clamp: out-of-range -> 0
				key |= (static_cast<uint32>(setIndex) << (t * 3));
			}
			m_passTexcoordKey[passIndex] = key;
		}

		// PS: pass->m_pixelShader (public, ShaderImplementation.h:265) -->
		// m_program (public, ShaderImplementation.h:611) --> m_graphicsData
		// (public, ShaderImplementation.h:681). May be null per Plan 11-05
		// PEXE caveat -- D3D11 CreatePixelShader rejects D3D9-era bytecode,
		// so Direct3d11_PixelShaderProgramData::m_d3dPS often stays null.
		// Iter-2 wires the magenta fallback PS to handle that case.
		if (pass->m_pixelShader
		    && pass->m_pixelShader->m_program
		    && pass->m_pixelShader->m_program->m_graphicsData)
		{
			m_passPS[passIndex] = static_cast<Direct3d11_PixelShaderProgramData const *>(
				pass->m_pixelShader->m_program->m_graphicsData);
			// UAF fix: store the ADDRESS of the engine owner's m_graphicsData slot so
			// apply() can re-read it LIVE (it is freed+recreated on graphics-data
			// restore; the cast snapshot above would dangle -> the boot crash). See header.
			m_passPSSlot[passIndex] = &pass->m_pixelShader->m_program->m_graphicsData;
		}

		// Plan 17-03 SR-1: per-pass MATERIAL + TEXTUREFACTOR source-data
		// resolution. Mirrors Direct3d9_StaticShaderData.cpp:593-700.
		// Without this source data, the Plan 17-03 reflection-driven upload
		// writes ZERO into the cbuffer for every pass and CHAR-02 fails
		// (gray eyes). The cached PerPassMaterial is consumed by apply()
		// at draw time. Defensive defaults mirror the D3D9 sibling's
		// material-tag-failure-fallback (white diffuse, opaque alpha) so
		// passes whose material tag does not resolve still render visibly
		// rather than silently going black.
		{
			PerPassMaterial pm = {};
			pm.m_diffuse        = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
			pm.m_specular       = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
			pm.m_emissive       = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
			pm.m_textureFactor  = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
			pm.m_textureFactor2 = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

			ShaderImplementation::Pass const * const engPass = pass;   // R3-03e: nested-type alias

			if (engPass->m_materialTag)
			{
				Material material;
				if (shader.getMaterial(engPass->m_materialTag, material))
				{
					pm.m_materialValid = true;
					VectorArgb const &d = material.getDiffuseColor();
					VectorArgb const &s = material.getSpecularColor();
					VectorArgb const &e = material.getEmissiveColor();
					pm.m_diffuse  = DirectX::XMFLOAT4(d.r, d.g, d.b, d.a);
					pm.m_specular = DirectX::XMFLOAT4(s.r, s.g, s.b, s.a);
					pm.m_emissive = DirectX::XMFLOAT4(e.r, e.g, e.b, e.a);
					pm.m_power    = material.getSpecularPower();
				}
				else
				{
					// Defensive defaults mirroring Direct3d9_StaticShaderData.cpp:627-650.
					pm.m_materialValid = true;
					pm.m_diffuse  = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
					pm.m_specular = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
					pm.m_emissive = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
					pm.m_power    = 0.0f;
					DEBUG_WARNING(true,
						("Direct3d11_StaticShaderData: could not find material tag in shader %s",
						shader.getStaticShaderTemplate().getName().getString()));
				}
			}

			if (engPass->m_textureFactorTag)
			{
				uint32 tf = 0;
				if (shader.getTextureFactor(engPass->m_textureFactorTag, tf))
				{
					pm.m_textureFactor.x = static_cast<float>((tf >> 16) & 0xff) / 255.0f;
					pm.m_textureFactor.y = static_cast<float>((tf >>  8) & 0xff) / 255.0f;
					pm.m_textureFactor.z = static_cast<float>((tf >>  0) & 0xff) / 255.0f;
					pm.m_textureFactor.w = static_cast<float>((tf >> 24) & 0xff) / 255.0f;
					pm.m_textureFactorValid = true;
				}
			}

			if (engPass->m_textureFactorTag2)
			{
				uint32 tf2 = 0;
				if (shader.getTextureFactor(engPass->m_textureFactorTag2, tf2))
				{
					pm.m_textureFactor2.x = static_cast<float>((tf2 >> 16) & 0xff) / 255.0f;
					pm.m_textureFactor2.y = static_cast<float>((tf2 >>  8) & 0xff) / 255.0f;
					pm.m_textureFactor2.z = static_cast<float>((tf2 >>  0) & 0xff) / 255.0f;
					pm.m_textureFactor2.w = static_cast<float>((tf2 >> 24) & 0xff) / 255.0f;
					pm.m_textureFactor2Valid = true;
				}
			}

			m_passMaterial[passIndex] = pm;
		}

		// Plan 11-09.14 (CODEX Bucket A): walk pass->m_pixelShader->m_textureSamplers
		// to resolve per-stage textures + samplers. Plan 11-09.15 Iter-44E extends
		// from slot-0-only to all 8 stages: D3D9 character/multi-texture shaders
		// (skeletal eye pass, terrain detail blends, etc.) bind textures at
		// m_textureIndex 1..7 and pre-Iter-44E those bindings were silently
		// dropped, leaving the slot at whatever-the-last-draw-set-it-to or
		// nullptr -> gray/black sample. Kenny's 2026-05-23 "gray eyes" smoke
		// finding pinpointed the gap.
		++s_passesTotal;
		if (pass->m_pixelShader && pass->m_pixelShader->m_textureSamplers)
		{
			using TexSamplers = ShaderImplementation::Pass::PixelShader::TextureSamplers;
			TexSamplers const &samplers = *pass->m_pixelShader->m_textureSamplers;
			for (TexSamplers::const_iterator si = samplers.begin(); si != samplers.end(); ++si)
			{
				ShaderImplementation::Pass::PixelShader::TextureSampler const *ts = *si;
				if (!ts)
					continue;
				int const stageIndex = ts->m_textureIndex;
				if (stageIndex < 0 || stageIndex >= kMaxStages)
					continue;

				Stage &stage = m_passStages[passIndex][stageIndex];
				stage.m_present = true;
				if (stageIndex == 0)
					++s_passesWithSampler0;

				// Resolve the texture via the D3D9 sibling pattern
				// (Direct3d9_StaticShaderData.cpp:297-352): getTextureData
				// fallback to wrap/linear/aniso=1 if the tag isn't in the
				// shader's TextureDataMap; then global-texture short-circuit;
				// then m_graphicsData -> Direct3d11_TextureData const * const *.
				//
				// Always pre-fill textureData with safe defaults so all later
				// code paths (including the invalid-fallback merge in
				// buildSamplerDescForStage0) see a populated struct.
				// getTextureData overrides on success; remains as filled below
				// on failure or when m_textureTag is zero.
				StaticShaderTemplate::TextureData textureData;
				textureData.placeholder         = false;
				textureData.addressU            = StaticShaderTemplate::TA_wrap;
				textureData.addressV            = StaticShaderTemplate::TA_wrap;
				textureData.addressW            = StaticShaderTemplate::TA_wrap;
				textureData.mipFilter           = StaticShaderTemplate::TF_linear;
				textureData.minificationFilter  = StaticShaderTemplate::TF_linear;
				textureData.magnificationFilter = StaticShaderTemplate::TF_linear;
				textureData.maxAnisotropy       = 1;
				textureData.texture             = nullptr;

				if (ts->m_textureTag)
					shader.getTextureData(ts->m_textureTag, textureData);

				bool const tagIsGlobal = ts->m_textureTag && Direct3d11_TextureData::isGlobalTexture(ts->m_textureTag);
				if (tagIsGlobal)
				{
					stage.m_texture = Direct3d11_TextureData::getGlobalTexture(ts->m_textureTag);
				}
				else if (textureData.texture)
				{
					stage.m_texture = reinterpret_cast<Direct3d11_TextureData const * const *>(
						textureData.texture->getGraphicsDataAddress());
				}
				else
				{
					stage.m_texture = nullptr;
				}

				// Plan 11-09.15 Iter-18: log stage-0 texture resolution to a
				// file (not InfoQueue) because construct() may fire before
				// the device-side InfoQueue is wired up. Iter-17 attempted
				// InfoQueue and produced ZERO lines this session, indicating
				// the construct() build path either ran pre-InfoQueue setup
				// or never executed for stage-0 samplers at all. File I/O
				// works regardless. First call per session truncates;
				// subsequent calls append. Capped at 100 entries to bound
				// the file size.
				{
					static int  s_iter18BuildCount  = 0;
					static bool s_iter18FirstWrite  = true;
					if (s_iter18BuildCount < 100)
					{
						++s_iter18BuildCount;
						char const * const mode = s_iter18FirstWrite ? "wb" : "ab";
						s_iter18FirstWrite = false;
						FILE *fp = nullptr;
						fopen_s(&fp, "stage/iter18-stage0-resolve.txt", mode);
						if (fp)
						{
							Tag const t = ts->m_textureTag;
							char tagStr[5] = { static_cast<char>((t >> 24) & 0xFF),
							                   static_cast<char>((t >> 16) & 0xFF),
							                   static_cast<char>((t >>  8) & 0xFF),
							                   static_cast<char>((t      ) & 0xFF),
							                   '\0' };
							for (int k = 0; k < 4; ++k)
								if (tagStr[k] < 0x20 || tagStr[k] > 0x7E) tagStr[k] = '?';

							// Plan 11-09.15 Iter-19: dereference stage.m_texture to
							// get the actual SRV pointer. Cross-reference with the
							// Iter-15 readback's SRV ptr (0x1F6AC9EC = primaryBuffer)
							// to identify WHICH shader's MAIN texture resolves to the
							// 1024x768 RT.
							void const * srvPtr = nullptr;
							if (stage.m_texture && *stage.m_texture)
							{
								Direct3d11_TextureData const * const td = *stage.m_texture;
								srvPtr = static_cast<void const *>(td->getShaderResourceView());
							}
							// Plan 11-09.15 Iter-21: ShaderImplementation::getName()
							// returned null in Iter-20 output; switch to the StaticShader
							// template name (StaticShader -> ShaderTemplate -> PersistentCrcString)
							// which is the actual .sht asset path.
							CrcString const &templateName = shader.getStaticShaderTemplate().getName();
							char const *templateNameStr = templateName.getString();
							if (!templateNameStr || !*templateNameStr)
								templateNameStr = "<empty>";
							fprintf(fp,
								"build#%d sht='%s' pass=%d tag='%s' (0x%08X) isGlobal=%d "
								"textureData.texture=0x%p stage.m_texture=0x%p stage.m_present=%d "
								"derefSRV=0x%p\n",
								s_iter18BuildCount, templateNameStr,
								passIndex,
								tagStr, static_cast<unsigned int>(t), tagIsGlobal ? 1 : 0,
								static_cast<void const *>(textureData.texture),
								static_cast<void const *>(stage.m_texture),
								stage.m_present ? 1 : 0,
								srvPtr);
							fclose(fp);
						}
					}
				}

				if (stageIndex == 0 && stage.m_texture)
					++s_passesWithSampler0TextureResolved;

				stage.m_samplerDesc = buildSamplerDescForStage0(textureData, *ts);

				// Plan 11-09.15 Iter-44E: no break -- iterate every sampler
				// in the pass, not just the m_textureIndex==0 entry. The
				// loop's iteration order is the engine's authoring order,
				// and each entry is stored at its own m_textureIndex slot.
			}
		}
	}

	// Plan 17-03 SR-1: m_passMaterial must be sized 1:1 with m_passPS so
	// apply() can index by pass number without an extra bounds check.
	DEBUG_FATAL(m_passMaterial.size() != m_passPS.size(),
		("Direct3d11_StaticShaderData: m_passMaterial.size()=%zu != m_passPS.size()=%zu (SR-1 invariant)",
		m_passMaterial.size(), m_passPS.size()));
}

// ======================================================================

void Direct3d11_StaticShaderData::update(StaticShader const &shader)
{
	// Engine notifies us that the StaticShader's per-instance state (texture
	// override / material override / etc.) has been mutated. Iteration 1:
	// rebuild from scratch.
	construct(shader);

	// If the active shader is us, drop the cache so the next draw rebinds.
	if (ms_active == this)
	{
		ms_active     = nullptr;
		ms_activePass = -1;
	}
}

// ----------------------------------------------------------------------

int Direct3d11_StaticShaderData::getTextureSortKey() const
{
	// Iteration 1: 0 is the universal "no sort key" answer; engine treats
	// it as a tie. Later iterations may use a texture-pointer hash or
	// the first-stage texture pointer (mirrors D3D9 sibling).
	return 0;
}

// ----------------------------------------------------------------------

char const * Direct3d11_StaticShaderData::getActiveStaticShaderName()
{
	// Plan 11-09.15 Iter-29B diagnostic accessor. Returns the .sht
	// template path of whatever StaticShader was most recently bound via
	// setStaticShader -> apply(). Used by Direct3d11_StateCache::drawQuadList
	// to log which shader template each UI quad batch routes through, so
	// we can determine whether font draws hit shader/uicanvas_filtered.sht
	// (current Iter-28 modulate branch) or some other path.
	if (!ms_active || !ms_active->m_shader)
		return "<none>";
	CrcString const &name = ms_active->m_shader->getStaticShaderTemplate().getName();
	char const * const s = name.getString();
	return (s && *s) ? s : "<empty>";
}

// ======================================================================

bool Direct3d11_StaticShaderData::apply(int passNumber) const
{
	// Plan 11-09 Iter-1: per-draw VS + PS binding via the per-pass extraction
	// cache populated in construct(). Matches D3D9 sibling shape
	// (Direct3d9_StaticShaderData.cpp:1045).
	//
	// Plan 11-09.14 (CODEX Bucket A): per-pass slot-0 SRV + sampler binding
	// added below the VS/PS pointer set. CODEX correction #1 -- select the
	// TextureSampler whose m_textureIndex == 0, not the first iterator (the
	// D3D9 sibling indexes m_stage by destination texture index). CODEX
	// correction #2 -- previous Iter-4 sampler0=NULL diagnostic was reading
	// stale device state (PSSetSamplers runs late in applyPreDrawState);
	// replaced by lifetime counters in remove() shutdown log.
	//
	// Still deferred to Phase 12 (real per-asset PS compile): material /
	// textureFactor / textureScroll / alpha-test / stencil / fog / multi-slot
	// SRV/sampler binding (slots 1..7). CODEX Q5 -- the dynamic fallback PS
	// reads only t.Sample(s, input._v0.xy) from register(t0)/register(s0),
	// so those features have no visible effect under Plan 11-09.13 anyway.
	bool const cacheHit = (ms_active == this && ms_activePass == passNumber);

	if (!cacheHit)
	{
		ms_active     = this;
		ms_activePass = passNumber;

		if (passNumber >= 0 && static_cast<size_t>(passNumber) < m_passVS.size())
		{
			size_t const idx = static_cast<size_t>(passNumber);

			// UAF FIX (release boot-crash 2026-06-02): re-derive the per-pass VS/PS
			// graphics-data pointers LIVE from the stable engine-owner slots, instead
			// of trusting the construct-time snapshot (m_passVS/m_passPS), which dangles
			// after a device graphics-data remove/restore frees+recreates *m_graphicsData.
			// D3D9 reads m_graphicsData live at apply for exactly this reason
			// (Direct3d9_StaticShaderData.cpp:829). Use liveVS/livePS for every deref
			// below; the snapshot vectors are kept only for construct-time work + the
			// usesVertexShader() presence check (a stale-but-non-null entry still
			// correctly answers "this pass has a VS/PS").
			Direct3d11_VertexShaderData const * const liveVS =
				(idx < m_passVSSlot.size() && m_passVSSlot[idx] && *m_passVSSlot[idx])
					? static_cast<Direct3d11_VertexShaderData const *>(*m_passVSSlot[idx])
					: nullptr;
			Direct3d11_PixelShaderProgramData const * const livePS =
				(idx < m_passPSSlot.size() && m_passPSSlot[idx] && *m_passPSSlot[idx])
					? static_cast<Direct3d11_PixelShaderProgramData const *>(*m_passPSSlot[idx])
					: nullptr;

			// Plan 17-09: hand the StateCache BOTH the VS data and this pass's
			// resolved texcoord-set key, so it binds the correct per-key variant
			// (and passes the key explicitly into layout creation -- never ambient).
			Direct3d11_StateCache::setCurrentVSData(liveVS, m_passTexcoordKey[idx]);
			Direct3d11_StateCache::setCurrentPSData(livePS);

			// Plan 17-09 (GAP-5): feed materialSpecularPower into the VS b0 shadow
			// (lightData[25].w). Mirrors D3D9 applyLights_vertexShader:577 reading
			// Direct3d9_StateCache::getSpecularPower(). m_power was captured from
			// material.getSpecularPower() in construct(). Without this the VS object-
			// space specular does pow(N.H, 0) = NaN on back/perp vertices -> the
			// purple/green bump arms. setSpecularPower clamps 0/NaN to the D3D9 default.
			if (idx < m_passMaterial.size())
			{
				PerPassMaterial const &pmPower = m_passMaterial[idx];
				Direct3d11_StateCache::setSpecularPower(pmPower.m_materialValid ? pmPower.m_power : 32.0f);

				// Plan 17-09 (GAP-5 bump-arm audit): also feed the VS material COLOR
				// struct (c11/c13/c14) -- mirrors D3D9 StaticShaderData:862
				// setVertexShaderConstants(VSCR_material, &m_material, 5). The asset VS
				// seeds its diffuse from cb0[14]=emissive and tints specular by
				// cb0[13]=specular; without this they were 0 (black VS specular, no
				// material emissive in the diffuse seed). When the pass has no valid
				// material, feed zero (matches D3D9 Zero(m_material) default + the
				// R3-03b zero-material convention used for the PS path below).
				if (pmPower.m_materialValid)
					Direct3d11_StateCache::setVertexMaterialColors(pmPower.m_diffuse, pmPower.m_specular, pmPower.m_emissive);
				else
					Direct3d11_StateCache::setVertexMaterialColors(DirectX::XMFLOAT4(0,0,0,0), DirectX::XMFLOAT4(0,0,0,0), DirectX::XMFLOAT4(0,0,0,0));
			}

			// Plan 17-03 R3 (D-04 + HIGH-2 + HIGH-3 + R3-03a + R3-03b + R3-03c + SR-1 + SR-2):
			// OFFSET-AWARE reflection-driven material + textureFactor upload into the
			// shared PerMaterial shadow. The upload uses the cached layout's actual
			// NAME / BIND POINT / variable START OFFSETS -- NEVER hardcoded
			// "PerMaterial" or slot 2 or sizeof(Direct3d11_PerMaterialCB). Every
			// pass uploads a current cbuffer (mirrors D3D9
			// Direct3d9_StaticShaderData.cpp:835-897 per-pass apply semantics).
			// R3-03b: !pm.m_materialValid passes upload ZERO material so stale
			// data does NOT bleed into a subsequent material pass.
			if (livePS && idx < m_passMaterial.size())
			{
				PerPassMaterial const &pm = m_passMaterial[idx];
				Direct3d11_PerMaterialCB & shadow = Direct3d11Namespace::getPerMaterialShadow();
				std::vector<Direct3d11_ReflectedPSCbufferLayout> const &layouts =
					livePS->getReflectedCbufferLayouts();

				// R3-03b: read source XMFLOAT4s for THIS pass. If !m_materialValid,
				// source values are ZERO (so the eventual updatePS call uploads zero
				// material -- never the prior pass's data).
				DirectX::XMFLOAT4 const passDiffuse  = pm.m_materialValid       ? pm.m_diffuse        : DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
				DirectX::XMFLOAT4 const passSpecular = pm.m_materialValid       ? pm.m_specular       : DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
				DirectX::XMFLOAT4 const passEmissive = pm.m_materialValid       ? pm.m_emissive       : DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
				DirectX::XMFLOAT4 const passTexFac   = pm.m_textureFactorValid  ? pm.m_textureFactor  : DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
				DirectX::XMFLOAT4 const passTexFac2  = pm.m_textureFactor2Valid ? pm.m_textureFactor2 : DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

				// For each reflected cbuffer layout, build a staging byte buffer sized
				// to the cbuffer's reflected TotalSize, copy our XMFLOAT4 source data
				// at each named variable's StartOffset (bounds-checked against var.Size),
				// then upload via updatePS(layout.BindPoint, staging, layout.TotalSize).
				// Variables not present in the cached layout are silently skipped
				// (Plan 03 must handle name-mismatches per R3-03f -- documented in
				// 17-03-SUMMARY.md if the reflected names don't match the candidate
				// set {materialDiffuse/Specular/Emissive, textureFactor, textureFactor2}).
				for (auto const &layout : layouts)
				{
					if (layout.TotalSize == 0)
						continue;   // Defensive: zero-size means Plan 02 reflection failed for this entry.

					// Plan 17-03 INVESTIGATION (2026-05-28, RESOLVED): an earlier
					// build of this code path (commit 6777c3328 + post-build link
					// produced 2026-05-28 20:36) FATAL'd with a ~1 GB allocation
					// at PPEM init. A diagnostic fopen/fwrite/fclose trace
					// inserted here masked the crash (Heisenbug — code-layout /
					// timing sensitive) and captured 4407 apply() invocations
					// across all char-select shaders, ALL showing layout.TotalSize
					// == 400 (SwgVertexConstants @ b0). Trace preserved at
					// .planning/phases/17-psrc-census-char-select-beachhead/
					// evidence/plan-17-03-apply-trace.txt. Conclusion: the FATAL
					// was NOT in this staging alloc — somewhere else in the
					// setStaticShader call chain has an uninitialized-memory or
					// timing-dependent issue that this build's code layout
					// doesn't expose. Adding a defensive TotalSize cap here would
					// be cargo-culting. Re-add the trace if the crash recurs.
					// Defensive upper bound below is a CHEAP belt-and-braces
					// guard against the std::vector ctor diverging if a future
					// reflected layout returns a corrupt TotalSize.
					static constexpr unsigned int kMaxReasonableCbufferTotalSize = 64 * 1024;   // 64 KiB; SwgVertexConstants is 400 B.
					if (layout.TotalSize > kMaxReasonableCbufferTotalSize)
					{
						DEBUG_WARNING(true,
							("Direct3d11_StaticShaderData::apply: skipping suspicious layout.TotalSize=%u (cap=%u) for shader '%s' layoutName='%s' bindPoint=%u",
							 layout.TotalSize, kMaxReasonableCbufferTotalSize,
							 m_shader ? m_shader->getStaticShaderTemplate().getName().getString() : "(null-m_shader)",
							 (layout.Name && layout.Name[0]) ? layout.Name : "(empty)",
							 layout.BindPoint));
						continue;
					}

					// Staging buffer sized to the reflected cbuffer total size. Char-select
					// PS cbuffers are 400 bytes (per Plan 02's PS-cbuffer dump for
					// SwgVertexConstants @ b0); std::vector is safe for any size up to
					// Direct3d11_ConstantBuffer::kMaxCBufferBytes (1152).
					//
					// Plan 17-08 (GAP-4) Producer B: for the asset PS's SwgVertexConstants
					// cbuffer @ b0 (BindPoint==0, TotalSize==400), RMW-initialise the
					// staging from the SHARED legacy b0 shadow instead of zeroing it, so
					// the dot3 light constants (c0..c4, Producer A) AND user constants
					// (c8+, Producer C) SURVIVE -- the material/textureFactor name patches
					// below then layer on top (c5..c7). The previous zero-init was the
					// clobber that wiped the lights and rendered the body BLACK. All other
					// cbuffers keep the zero-init (they have no shared shadow).
					bool const isLegacyB0 =
						(layout.BindPoint == 0
						 && layout.TotalSize == Direct3d11_LegacyPSConstants::kShadowBytes);
					std::vector<unsigned char> staging(layout.TotalSize, 0u);
					if (isLegacyB0)
					{
						std::memcpy(staging.data(),
							Direct3d11_LegacyPSConstants::getShadow(),
							Direct3d11_LegacyPSConstants::kShadowBytes);

						// Plan 17-08 (GAP-4) Producer A (write site): compute the dot3
						// light constants in OBJECT-LOCAL space (the asset PS lights
						// against the object-space normal_o) from the per-frame snapshot
						// + the current object transform, and write c0..c4 into the
						// staging by reflected byte offset (c-index * 16; the reflected
						// vars are generic packedRegister0..4 so we map by OFFSET, not
						// name -- consult-validated). Mirrors
						// Direct3d9_LightManager::applyLights_vertexShader_dot3 (:777-830):
						// localDirection = -rotate_p2l(worldDir), .w = materialSpecularPower.
						Direct3d11_PixelDot3State const &d3 = Direct3d11_LightManager::getPixelDot3State();
							// Plan 17-08 (GAP-4) VISIBLE diagnostic via InfoQueue (DEBUG_REPORT_LOG_PRINT
							// is NOT captured in d3d11-debug.log). First few b0 draws report whether
							// Producer A fed lights. d3valid=0 => no T_parallel light at char-select
							// -> c0..c4 zero -> BLACK (prime suspect). d3valid=1 + black => value/space bug.
							{
								static int s_gap4DiagCount = 0;
								if (s_gap4DiagCount < 6)
								{
									++s_gap4DiagCount;
									char const *sn = (m_shader ? m_shader->getStaticShaderTemplate().getName().getString() : "(null)");
									char gap4[400];
									_snprintf_s(gap4, sizeof(gap4), _TRUNCATE,
										"Plan 17-08 GAP4 b0 #%d shader='%s' d3valid=%d worldDir=(%.3f,%.3f,%.3f) "
										"diffuse=(%.3f,%.3f,%.3f) specPow=%.3f",
										s_gap4DiagCount, (sn && *sn) ? sn : "(empty)",
										d3.valid ? 1 : 0,
										d3.localDirectionWorld.x, d3.localDirectionWorld.y, d3.localDirectionWorld.z,
										d3.diffuseColor.x, d3.diffuseColor.y, d3.diffuseColor.z, pm.m_power);
									if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
										iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, gap4);
								}
							}
						if (d3.valid)
						{
							using namespace Direct3d11_LegacyPSConstantRegisters;
							Vector const worldDir(
								d3.localDirectionWorld.x, d3.localDirectionWorld.y, d3.localDirectionWorld.z);
							Vector const localDir = Direct3d11_StateCache::worldDirectionToObjectLocal(worldDir);

							DirectX::XMFLOAT4 const c0(-localDir.x, -localDir.y, -localDir.z, pm.m_power);
							std::memcpy(staging.data() + PSCR_dot3LightDirection               * 16, &c0,                          sizeof(DirectX::XMFLOAT4));
							std::memcpy(staging.data() + PSCR_dot3LightDiffuseColor             * 16, &d3.diffuseColor,             sizeof(DirectX::XMFLOAT4));
							std::memcpy(staging.data() + PSCR_dot3LightSpecularColor            * 16, &d3.specularColor,            sizeof(DirectX::XMFLOAT4));
							std::memcpy(staging.data() + PSCR_dot3LightTangentMinusDiffuseColor * 16, &d3.tangentMinusDiffuseColor, sizeof(DirectX::XMFLOAT4));
							std::memcpy(staging.data() + PSCR_dot3LightTangentMinusBackColor    * 16, &d3.tangentMinusBackColor,    sizeof(DirectX::XMFLOAT4));
						}
#if P19_LIGHTDUMP
						// Phase 19 (19-04 Task 0) DEV-ONLY neutral b0 upload dump: the ACTUAL
						// dot3 bytes about to be uploaded for THIS draw (what the GPU sees),
						// read back from staging. Logs the first b0 draws each frame. NEUTRAL:
						// reads the uploaded values regardless of d3.valid, so a value STABLE
						// across frames while pixels still cycle REFUTES the light-feed story,
						// and a value that JUMPS while the camera is still implicates it (and
						// the setLights dump says why). REVERT after the question is answered.
						{
							static unsigned s_lastFrame = 0xFFFFFFFFu;
							static int      s_b0ThisFrame = 0;
							unsigned const frame = Direct3d11_Device::getDiagFrameIndex();
							if (frame != s_lastFrame) { s_lastFrame = frame; s_b0ThisFrame = 0; }
							int const b0Idx = s_b0ThisFrame++;
							if (b0Idx < 48)
							{
								using namespace Direct3d11_LegacyPSConstantRegisters;
								DirectX::XMFLOAT4 upDir, upDiff, upSpec;
								std::memcpy(&upDir,  staging.data() + PSCR_dot3LightDirection    * 16, sizeof(upDir));
								std::memcpy(&upDiff, staging.data() + PSCR_dot3LightDiffuseColor  * 16, sizeof(upDiff));
								std::memcpy(&upSpec, staging.data() + PSCR_dot3LightSpecularColor * 16, sizeof(upSpec));
								char const *sn = (m_shader ? m_shader->getStaticShaderTemplate().getName().getString() : "(null)");
								if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
								{
									// Phase 19 (19-04 Task 0b): also log the per-material color the PS
									// reads (matDiff + textureFactor). If THESE jump frame-to-frame for
									// the same surface while the light feed is stable, the material/asset
									// path is the cycling source, not lighting.
									char line[512];
									_snprintf_s(line, sizeof(line), _TRUNCATE,
										"[P19DUMP] f=%u b0#%d shader='%s' d3valid=%d upDir=(%.3f,%.3f,%.3f,%.3f) "
										"upDiff=(%.3f,%.3f,%.3f) upSpec=(%.3f,%.3f,%.3f) "
										"matV=%d matDiff=(%.3f,%.3f,%.3f,%.3f) tfV=%d texFac=(%.3f,%.3f,%.3f,%.3f)",
										frame, b0Idx, (sn && *sn) ? sn : "(empty)", d3.valid ? 1 : 0,
										upDir.x, upDir.y, upDir.z, upDir.w,
										upDiff.x, upDiff.y, upDiff.z,
										upSpec.x, upSpec.y, upSpec.z,
										pm.m_materialValid ? 1 : 0,
										pm.m_diffuse.x, pm.m_diffuse.y, pm.m_diffuse.z, pm.m_diffuse.w,
										pm.m_textureFactorValid ? 1 : 0,
										pm.m_textureFactor.x, pm.m_textureFactor.y, pm.m_textureFactor.z, pm.m_textureFactor.w);
									iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, line);
								}
							}
						}
#endif
					}

					// Lookup + write helper. Returns true on a successful write so the
					// shadow mirror below can mirror only the writes that landed.
					auto writeVarByName = [&](char const *varName, DirectX::XMFLOAT4 const &value) -> bool
					{
						for (auto const &var : layout.Vars)
						{
							if (std::strcmp(var.Name, varName) != 0) continue;
							// R3-03c bounds: a reflected var smaller than 16 bytes means
							// the asset declared something narrower (e.g. float3) -- skip
							// rather than overrun. A larger var is unexpected for a
							// material/textureFactor variable; we still only write the
							// first 16 bytes.
							if (var.Size < sizeof(DirectX::XMFLOAT4))
								return false;
							if (var.StartOffset + sizeof(DirectX::XMFLOAT4) > layout.TotalSize)
								return false;
							std::memcpy(staging.data() + var.StartOffset, &value, sizeof(DirectX::XMFLOAT4));
							return true;
						}
						return false;
					};

					// Plan 17-06b (GAP-2 MAPPING, Round-4 HIGH-1): companion helper for
					// nested-member / array-element offset writes. The writeVarByName
					// lambda above iterates top-level layout.Vars and string-compares
					// whole names -- it CANNOT reach into userConstants[N] or a nested
					// member. This helper writes an XMFLOAT4 at an EXPLICIT absolute byte
					// offset with the same bounds-check posture (offset + 16 <= TotalSize).
					//
					// Plan 17-06b OUTCOME = CASE C (DEFERRED). Plan 17-06a's discovery boot
					// (2026-05-29) showed userConstants reflects as a FLAT float4[17] array
					// with NO member names (typeClass=3 D3D_SVC_VECTOR; elements at
					// 128 + N*16). D3D11 reflection alone cannot say which index is diffuse
					// vs emissive. The secondary source of truth -- setPixelShaderUserConstants_impl
					// (Direct3d11.cpp:679-704) -- writes the userConstants[] slots POSITIONALLY
					// from a generic VectorRgba const* array; there is NO engine line that maps
					// a specific slot index to materialDiffuse / materialEmissive (the per-slot
					// semantics are asset-defined in each shader's pixel_shader_constants.inc,
					// which lives in the TRE archives, not in deterministic engine code).
					// Per Round-5 review item 3, writing to a GUESSED slot would set
					// wroteDiffuse=1 and FALSELY close GAP-2 -- forbidden. So the candidate
					// chains below are intentionally LEFT UNCHANGED (no speculative write);
					// this helper ships as latent infrastructure for a FUTURE evidence-grounded
					// mapping (e.g. an open-world shader whose .inc names the slots, or a
					// concrete engine upload-order source line). Per Round-4 HIGH-3,
					// char-select PSes consume textureFactor.rgb + COLOR0, NOT cbuffer
					// material diffuse/emissive -- so DEFERRING does not block CHAR-01/02/03
					// visual delivery (that is GAP-3 / Plan 17-07).
					auto writeVarFloat4AtOffset = [&](unsigned int absoluteOffset, DirectX::XMFLOAT4 const &value) -> bool
					{
						if (absoluteOffset + sizeof(DirectX::XMFLOAT4) > layout.TotalSize)
							return false;
						std::memcpy(staging.data() + absoluteOffset, &value, sizeof(DirectX::XMFLOAT4));
						return true;
					};
					(void)writeVarFloat4AtOffset;   // Case C: latent infrastructure, not yet wired (no evidence-backed offset)

					// Plan 17-04 Task 2: writeVarByName lookups are now SCHEMA-AWARE.
					//
					// Plan 17-03's R3-03g flush log proved wroteDiffuse / wroteSpecular /
					// wroteEmissive were all ZERO across every char-select shader despite
					// valid source data — because the reflected cbuffer at b0 is the
					// rewriter-emitted SwgVertexConstants (Direct3d11_HlslRewrite.cpp:761-812
					// wraps vertex_shader_constants.inc's globals — see HlslRewrite.cpp
					// for the source rationale) which declares its material members as
					// elements of a `material[5]` ARRAY, NOT as 5 named scalars
					// (materialDiffuse / materialSpecular / materialEmissive).
					//
					// D3DReflect surfaces array elements as `material[0]`, `material[1]`,
					// ... so we extend each lookup to try BOTH the array form AND the
					// original hardcoded name (so a future shader that DOES declare a
					// plain `materialDiffuse` — e.g. a PS that doesn't `#include` the VS
					// rewrite header — still works).
					//
					// Array index mapping is best-effort against the documented likely
					// layout (Plan 17-04 PLAN lines 108-114; Plan 17-03 SUMMARY line 252-258
					// caveat). The .inc is in TRE archives so the executor can't read it
					// directly; we write to BOTH candidate slots that are plausible per
					// the two cited slot guesses. Worst case the wrong-slot writes are
					// overwritten by the right-slot writes from a subsequent pass / shader,
					// or land in unused cbuffer space — they cannot corrupt the WVP
					// matrix at material[0] or the lightData array which lives at higher
					// offsets per the rewriter's section ordering.
					//
					// The diagnostic R3-03g flush log below + the new Task-2 discovery
					// dump (one-shot per shader-name pattern) surface the actual
					// reflected variable names + offsets so we can pin the exact mapping
					// from boot evidence and tighten this in a follow-up if needed.

					// Try the array-element form (SwgVertexConstants exposes
					// material[N] elements per HlslRewrite Rule D wrap), then fall
					// back to the original hardcoded scalar names so a PS that
					// declares plain `materialDiffuse` (i.e. doesn't `#include` the
					// VS rewrite header) still has its values land.
					//
					// Slot mapping: per Plan 17-04 PLAN's "likely mapping" (lines
					// 108-114) — material[0]=diffuse, material[1]=specular,
					// material[2]=emissive. This is one of two candidate orderings
					// surveyed by the planner; the other (per Plan 17-03 SUMMARY
					// caveat line 258 — material[1]=diffuse, material[3]=specular,
					// material[2]=emissive) corresponds to the D3D9 FFP
					// ambient/diffuse/specular/emissive/power ordering. The
					// discovery dump below pins which is correct from boot
					// evidence; if the FFP ordering proves right, the index
					// constants here flip in a tightening commit.
					//
					// Deliberately ONE candidate per channel (no fallthrough
					// across array indices) — otherwise a successful write to a
					// wrong slot would prevent the right slot from getting hit on
					// the next attempt and could stomp another channel's data.
					// Plan 17-04.X (post-boot evidence refinement, 2026-05-28):
					// The Plan 17-04 cbuffer-vars-discovery dump captured real
					// names from D3DReflect on SwgVertexConstants @ b0
					// (.planning/phases/17-.../evidence/...psrc-source-dump.txt):
					//   var[0..4]: packedRegister0..4  (generic, packed per the
					//              rewriter — each PS's pixel_shader_constants.inc
					//              defines what semantically lives in each slot)
					//   var[5]:    textureFactor                 ← named
					//   var[6]:    textureFactor2                ← named
					//   var[7]:    materialSpecularColor         ← named (NOT
					//              "materialSpecular" — Plan 17-04 Task 2 missed
					//              the "Color" suffix by one word)
					//   var[8]:    userConstants[17]   (272 bytes — engine
					//              setPixelShaderConstants destination)
					//
					// Confirmed by PSRC source inspection:
					//   - h_color2_specmap_cbmp_ps20.psh body uses
					//     `materialSpecularColor` directly — landing a write to
					//     it should make specular highlight tint correct on the
					//     character head.
					//   - h_simple_pp_ps20.psh body uses textureFactor.rgb as the
					//     diffuse tint — landing a write to it should color the
					//     character clothing.
					//   - The diffuse/emissive material values are NOT directly
					//     consumed by these PS programs (diffuse comes from the
					//     texture sampler; emissive lives in packedRegisterN +
					//     userConstants engine state). So material[N] / material*
					//     lookups will silently miss for char-select shaders —
					//     kept as fallback for any future PS that DOES declare
					//     plain materialDiffuse/etc.
					bool const wroteDiffuse  =
						writeVarByName("material[0]",   passDiffuse)
						|| writeVarByName("materialDiffuse",  passDiffuse);
					bool const wroteSpecular =
						writeVarByName("materialSpecularColor", passSpecular)
						|| writeVarByName("material[1]",   passSpecular)
						|| writeVarByName("materialSpecular", passSpecular);
					bool const wroteEmissive =
						writeVarByName("material[2]",   passEmissive)
						|| writeVarByName("materialEmissive", passEmissive);
					bool const wroteTextureFactor  = writeVarByName("textureFactor",  passTexFac);
					bool const wroteTextureFactor2 = writeVarByName("textureFactor2", passTexFac2);
					(void)wroteTextureFactor;
					(void)wroteTextureFactor2;

					// Plan 17-04 Task 2 discovery dump: when at least one of the
					// wrote* flags lands as zero across the head + eye anchor
					// passes, the R3-03g log captures the FAILED flags but NOT
					// the actual reflected variable names available in this
					// shader's cbuffer. Emit a one-shot per-anchor enumeration
					// of (var.Name, var.StartOffset, var.Size) so the boot
					// evidence pins the EXACT names + offsets for the
					// follow-up cbuffer-mapping refinement.
					//
					// Gated on the same head / eye shader-name substring
					// heuristic as R3-03g — exactly two lines land in the
					// log per boot anchor (one per head pass, one per eye pass,
					// per layout when wrote* is incomplete).
					{
						static bool s_dumpedHeadVars = false;
						static bool s_dumpedEyeVars  = false;
						char const *shaderNameDbg = "?";
						if (m_shader)
						{
							CrcString const &templateNameDbg = m_shader->getStaticShaderTemplate().getName();
							char const * const snDbg = templateNameDbg.getString();
							if (snDbg && *snDbg) shaderNameDbg = snDbg;
						}
						bool const isHeadDbg = std::strstr(shaderNameDbg, "head") != nullptr;
						bool const isEyeDbg  = std::strstr(shaderNameDbg, "eye")  != nullptr;
						bool const incomplete = !(wroteDiffuse && wroteSpecular && wroteEmissive);
						bool const fire = incomplete
							&& ((isHeadDbg && !s_dumpedHeadVars) || (isEyeDbg && !s_dumpedEyeVars));
						if (fire)
						{
							if (isHeadDbg) s_dumpedHeadVars = true;
							if (isEyeDbg)  s_dumpedEyeVars  = true;

							// Header line.
							{
								char headerMsg[512];
								_snprintf_s(headerMsg, sizeof(headerMsg), _TRUNCATE,
									"Plan 17-04 Task 2 cbuffer-vars-discovery shader='%s' layoutName='%s' bindPoint=%u totalSize=%u varsCount=%u wrote(D,S,E)=(%d,%d,%d) — enumerating reflected variables:",
									shaderNameDbg,
									(layout.Name && layout.Name[0]) ? layout.Name : "(empty)",
									layout.BindPoint, layout.TotalSize,
									static_cast<unsigned>(layout.Vars.size()),
									wroteDiffuse  ? 1 : 0,
									wroteSpecular ? 1 : 0,
									wroteEmissive ? 1 : 0);
								if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
									iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, headerMsg);
								DEBUG_REPORT_LOG_PRINT(true, ("%s\n", headerMsg));
							}

							// Per-variable enumeration. One line per variable
							// so long lists don't truncate; the InfoQueue file
							// sink accepts each as a separate record.
							for (size_t vi = 0; vi < layout.Vars.size(); ++vi)
							{
								auto const &v = layout.Vars[vi];
								char varMsg[320];
								_snprintf_s(varMsg, sizeof(varMsg), _TRUNCATE,
									"Plan 17-04 Task 2 cbuffer-vars-discovery shader='%s' var[%u]: name='%s' startOffset=%u size=%u",
									shaderNameDbg,
									static_cast<unsigned>(vi),
									v.Name, v.StartOffset, v.Size);
								if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
									iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, varMsg);
								DEBUG_REPORT_LOG_PRINT(true, ("%s\n", varMsg));
							}

							// Plan 17-06a (GAP-2 DISCOVERY): walk the INNER reflection of
							// any non-trivial top-level var (struct or array) -- notably
							// userConstants[17] (272 B opaque region at offset 128). The
							// cached layout.Vars carries ONLY {Name,StartOffset,Size} with
							// NO type descriptors (Direct3d11_PixelShaderProgramData.h:66-70),
							// so D3DReflect MUST re-run here to obtain inner member/element
							// names + offsets. This is ONE-SHOT per anchor (same s_dumped*
							// gate as above) -- at most two D3DReflect calls per boot (head
							// + eye), NEVER per draw. Recursion capped at depth 2. Output
							// gives Plan 17-06b unambiguous mapping evidence for where
							// diffuse + emissive material colors actually live.
							{
								auto emit06a = [&](char const *fieldPath, unsigned absOffset, unsigned byteSize, int typeClass, char const *typeName)
								{
									char m06a[384];
									_snprintf_s(m06a, sizeof(m06a), _TRUNCATE,
										"Plan 17-06a cbuffer-vars-discovery shader='%s' %s offset=%u size=%u typeClass=%d typeName='%s'",
										shaderNameDbg, fieldPath, absOffset, byteSize, typeClass,
										(typeName && typeName[0]) ? typeName : "?");
									if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
										iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, m06a);
									DEBUG_REPORT_LOG_PRINT(true, ("%s\n", m06a));
								};

								ID3DBlob *const psBlob06a = (livePS ? livePS->getPsBytecode() : nullptr);
								if (psBlob06a)
								{
									ID3D11ShaderReflection *refl06a = nullptr;
									HRESULT const hr06a = D3DReflect(
										psBlob06a->GetBufferPointer(), psBlob06a->GetBufferSize(),
										IID_ID3D11ShaderReflection, reinterpret_cast<void **>(&refl06a));
									if (SUCCEEDED(hr06a) && refl06a)
									{
										ID3D11ShaderReflectionConstantBuffer *cb06a =
											(layout.Name && layout.Name[0]) ? refl06a->GetConstantBufferByName(layout.Name) : nullptr;
										D3D11_SHADER_BUFFER_DESC cbd06a = {};
										if (cb06a && SUCCEEDED(cb06a->GetDesc(&cbd06a)))
										{
											for (UINT cvi = 0; cvi < cbd06a.Variables; ++cvi)
											{
												ID3D11ShaderReflectionVariable *cv = cb06a->GetVariableByIndex(cvi);
												if (!cv) continue;
												D3D11_SHADER_VARIABLE_DESC cvd = {};
												if (FAILED(cv->GetDesc(&cvd))) continue;
												ID3D11ShaderReflectionType *ct = cv->GetType();
												if (!ct) continue;
												D3D11_SHADER_TYPE_DESC ctd = {};
												if (FAILED(ct->GetDesc(&ctd))) continue;

												bool const isStruct06a = (ctd.Members  > 0);
												bool const isArray06a  = (ctd.Elements > 0);
												if (!isStruct06a && !isArray06a)
													continue;   // only walk non-trivial vars (userConstants, nested structs)

												unsigned const baseOff = cvd.StartOffset;   // absolute within cbuffer

												if (isStruct06a)
												{
													for (UINT mi = 0; mi < ctd.Members; ++mi)
													{
														ID3D11ShaderReflectionType *mt = ct->GetMemberTypeByIndex(mi);
														char const *mn = ct->GetMemberTypeName(mi);
														if (!mt) continue;
														D3D11_SHADER_TYPE_DESC mtd = {};
														if (FAILED(mt->GetDesc(&mtd))) continue;
														unsigned const mOff = baseOff + mtd.Offset;
														unsigned const mSz  = (mtd.Rows ? mtd.Rows : 1u) * (mtd.Columns ? mtd.Columns : 1u) * 4u
														                      * (mtd.Elements ? mtd.Elements : 1u);
														char path1[160];
														_snprintf_s(path1, sizeof(path1), _TRUNCATE, "%s.%s",
															cvd.Name, (mn && mn[0]) ? mn : "?");
														emit06a(path1, mOff, mSz, static_cast<int>(mtd.Class), mtd.Name);

														// depth-2: one nested level only (recursion cap)
														if (mtd.Members > 0)
														{
															for (UINT mj = 0; mj < mtd.Members; ++mj)
															{
																ID3D11ShaderReflectionType *m2 = mt->GetMemberTypeByIndex(mj);
																char const *m2n = mt->GetMemberTypeName(mj);
																if (!m2) continue;
																D3D11_SHADER_TYPE_DESC m2d = {};
																if (FAILED(m2->GetDesc(&m2d))) continue;
																unsigned const m2Off = mOff + m2d.Offset;
																unsigned const m2Sz  = (m2d.Rows ? m2d.Rows : 1u) * (m2d.Columns ? m2d.Columns : 1u) * 4u
																                       * (m2d.Elements ? m2d.Elements : 1u);
																char path2[224];
																_snprintf_s(path2, sizeof(path2), _TRUNCATE, "%s.%s.%s",
																	cvd.Name, (mn && mn[0]) ? mn : "?", (m2n && m2n[0]) ? m2n : "?");
																emit06a(path2, m2Off, m2Sz, static_cast<int>(m2d.Class), m2d.Name);
															}
														}
													}
												}
												else   // flat array (no member names) -- e.g. userConstants[17]
												{
													unsigned const stride = (ctd.Elements > 0 && cvd.Size > 0)
														? (cvd.Size / ctd.Elements) : 16u;
													for (UINT ei = 0; ei < ctd.Elements; ++ei)
													{
														unsigned const eOff = baseOff + ei * stride;
														char pathA[160];
														_snprintf_s(pathA, sizeof(pathA), _TRUNCATE, "%s[%u]", cvd.Name, ei);
														emit06a(pathA, eOff, stride, static_cast<int>(ctd.Type), ctd.Name);
													}
												}
											}
										}
										refl06a->Release();
										refl06a = nullptr;
									}
								}
							}
						}
					}

					// Mirror writes into the shared shadow so setPixelShaderUserConstants_impl's
					// slot-2 flush sees the SAME material values when the bound layout binds
					// at slot 2 (R3-03a + HIGH-3 shared shadow contract). When the layout's
					// bind point is NOT 2 the shadow mirror is a harmless overwrite -- the
					// staging buffer is what reaches THIS shader's reflected cbuffer.
					if (wroteDiffuse)  shadow.materialDiffuse  = passDiffuse;
					if (wroteSpecular) shadow.materialSpecular = passSpecular;
					if (wroteEmissive) shadow.materialEmissive = passEmissive;
					// textureFactor mirror is conditional on the optional sub-step-1d
					// PerMaterialCB extension; Plan 17-03 char-select census did NOT
					// surface textureFactor in any reflected PS cbuffer (all are
					// SwgVertexConstants @ b0, 9 vars, no textureFactor present), so
					// the extension is SKIPPED and the staging buffer alone holds the
					// asset's offsets if the asset ever declares textureFactor.

					// R3-03d StateCache bind-point coverage: char-select census shows
					// ALL reflected cbuffers at layout.BindPoint == 0; slot 0 is already
					// bound at Direct3d11_StateCache.cpp:1138. No StateCache edit needed.
					// If a future asset surfaces a non-{0,2} bind point, extend the
					// pre-draw bind loop there.
#if P19_LIGHTDUMP
					// Phase 19 (19-05) DEV: full b0 register dump of the FINAL staging (all
					// non-zero c5..c24) per isLegacyB0 draw, capped per frame. Goal: find the
					// register that VARIES for the SAME shader frame-to-frame -- the shared-shadow
					// RMW stale-inheritance leak (writeShadowBack at :1331 persists every reg, so a
					// draw that doesn't overwrite c8+ inherits the prior draw's bright value). Logs
					// every draw in-process, so it catches the leak whenever it occurs regardless of
					// camera/timing (unlike RenderDoc capture). REVERT after the reg is identified.
					if (isLegacyB0)
					{
						static unsigned s_p19fLastFrame = 0xFFFFFFFFu;
						static int      s_p19fThisFrame  = 0;
						unsigned const p19fFrame = Direct3d11_Device::getDiagFrameIndex();
						if (p19fFrame != s_p19fLastFrame) { s_p19fLastFrame = p19fFrame; s_p19fThisFrame = 0; }
						if (s_p19fThisFrame++ < 250)
						{
							char const *p19sn = (m_shader ? m_shader->getStaticShaderTemplate().getName().getString() : "(null)");
							if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
							{
								char p19line[700];
								bool const p19d3v = Direct3d11_LightManager::getPixelDot3State().valid;
								int p19o = _snprintf_s(p19line, sizeof(p19line), _TRUNCATE,
									"[P19FULL] f=%u d3v=%d sh='%s'", p19fFrame, p19d3v ? 1 : 0, (p19sn && *p19sn) ? p19sn : "(empty)");
								for (int p19c = 0; p19c < 25 && p19o > 0 && p19o < 620; ++p19c)
								{
									DirectX::XMFLOAT4 p19r;
									std::memcpy(&p19r, staging.data() + p19c * 16, sizeof(p19r));
									if (p19r.x != 0.0f || p19r.y != 0.0f || p19r.z != 0.0f || p19r.w != 0.0f)
									{
										int const p19w = _snprintf_s(p19line + p19o, sizeof(p19line) - p19o, _TRUNCATE,
											" c%d=(%.2f,%.2f,%.2f,%.2f)", p19c, p19r.x, p19r.y, p19r.z, p19r.w);
										if (p19w > 0) p19o += p19w;
									}
								}
								iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, p19line);
							}
						}
					}
#endif
					Direct3d11_ConstantBuffer::updatePS(layout.BindPoint, staging.data(), layout.TotalSize);

					// Plan 17-08 (GAP-4) Producer B: persist the merged staging back into
					// the shared legacy shadow so this pass's material/textureFactor
					// patches (c5..c7) are retained for the next pass alongside the
					// lights (Producer A) + user constants (Producer C). One-shot GAP4 b0
					// instrumentation: prove c0 lights reached b0 (c0.xyz != 0 => not black).
					if (isLegacyB0)
					{
						Direct3d11_LegacyPSConstants::writeShadowBack(staging.data());
#if 1
						static bool s_loggedGap4B0 = false;
						if (!s_loggedGap4B0)
						{
							s_loggedGap4B0 = true;
							Direct3d11_LegacyPSConstants::logShadow("StaticShaderData::apply b0 flush");
						}
#endif
					}

					// R3-03g (HIGH-3 + R3-03b proof): one-shot dual-routed log of the
					// material values at THIS flush for the HEAD pass and the EYE pass.
					// Heuristic: shader template name (.sht path) contains 'head' or
					// 'eye'. Static flags ensure only TWO lines land in the log per boot.
					// `#if 0` this block after the first successful boot is recorded.
#if 1
					{
						static bool s_loggedHeadFlush = false;
						static bool s_loggedEyeFlush  = false;
						char const *shaderName = "?";
						if (m_shader)
						{
							CrcString const &templateName = m_shader->getStaticShaderTemplate().getName();
							char const * const sn = templateName.getString();
							if (sn && *sn) shaderName = sn;
						}
						bool isHead = false;
						bool isEye  = false;
						if (std::strstr(shaderName, "head")) isHead = true;
						if (std::strstr(shaderName, "eye"))  isEye  = true;
						if ((isHead && !s_loggedHeadFlush) || (isEye && !s_loggedEyeFlush))
						{
							if (isHead) s_loggedHeadFlush = true;
							if (isEye)  s_loggedEyeFlush  = true;
							char msg[512];
							_snprintf_s(msg, sizeof(msg), _TRUNCATE,
								"Plan 17-03 R3-03g perMaterialShadow at flush shader='%s' bindPoint=%u totalSize=%u "
								"wroteDiffuse=%d wroteSpecular=%d wroteEmissive=%d "
								"diffuse=(%.3f,%.3f,%.3f,%.3f) specular=(%.3f,%.3f,%.3f,%.3f) "
								"emissive=(%.3f,%.3f,%.3f,%.3f) materialValid=%d",
								shaderName, layout.BindPoint, layout.TotalSize,
								wroteDiffuse  ? 1 : 0,
								wroteSpecular ? 1 : 0,
								wroteEmissive ? 1 : 0,
								passDiffuse.x,  passDiffuse.y,  passDiffuse.z,  passDiffuse.w,
								passSpecular.x, passSpecular.y, passSpecular.z, passSpecular.w,
								passEmissive.x, passEmissive.y, passEmissive.z, passEmissive.w,
								pm.m_materialValid ? 1 : 0);
							if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
								iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, msg);
							DEBUG_REPORT_LOG_PRINT(true, ("%s\n", msg));
						}
					}
#endif
				}
				// If layouts is empty (reflection failed at compile time per Plan 02
				// HIGH-2 non-fatal path), there is no upload this draw. Acceptable for
				// a shader that has no cbuffers; if a shader's reflection went missing
				// due to a compile failure, Plan 03 cannot recover -- documented
				// per-shader in 17-03-SUMMARY.md.
			}

			// Plan 11-09.15 Iter-39C: per-pass alpha-blend enable.
			// Iter-39B wired this to Direct3d11_ShaderImplementationData::apply
			// which turned out to be dead code (no virtual base, nothing in the
			// engine ever calls it). The right site is here -- this method is
			// called per draw from StateCache::draw*. Reads pass.m_alphaBlendEnable
			// from the engine's ShaderImplementationPass and toggles the
			// BlendState descriptor's BlendEnable. Mirrors D3D9 sibling
			// Direct3d9::setAlphaBlendEnable. UI canvas / particles / glow get
			// blend ON; opaque world content gets blend OFF.
			//
			// Still-deferred (small follow-on): m_alphaBlendSource/Dest, m_alphaBlendOperation,
			// m_writeEnable, m_zEnable/Write/Compare. Existing install() defaults
			// (SrcAlpha/InvSrcAlpha "over", DepthEnable=TRUE, LESSEQUAL, write-all)
			// match what typical UI/particle/opaque shaders need.
			if (m_implementation && m_implementation->m_pass
				&& idx < m_implementation->m_pass->size())
			{
				ShaderImplementationPass const * const engPass = (*m_implementation->m_pass)[idx];
				if (engPass)
				{
					Direct3d11_StateCache::setAlphaBlendEnable(engPass->m_alphaBlendEnable);

					// Plan 11-09.15 Iter-44B: per-pass ALPHA-TEST. D3D11 has
					// no FFP alpha-test; we push enable + reference (0..1) into
					// PS cbuffer slot 1 and the dynamic-generated PS reads it
					// and conditionally clip()s. Reference resolution mirrors
					// the D3D9 sibling at Direct3d9_StaticShaderData.cpp:740-760:
					// hardcoded TAG_A255/A128/A001/A000 -> 255/128/1/0, otherwise
					// per-StaticShader getAlphaTestReferenceValue(tag) lookup.
					{
						uint8 refUint = 128;  // safe default (50% threshold)
						if (engPass->m_alphaTestEnable)
						{
							Tag const t = engPass->m_alphaTestReferenceValueTag;
							if      (t == TAG_A255) refUint = 255;
							else if (t == TAG_A128) refUint = 128;
							else if (t == TAG_A001) refUint = 1;
							else if (t == TAG_A000) refUint = 0;
							else if (!m_shader || !m_shader->getAlphaTestReferenceValue(t, refUint))
								refUint = 1;  // matches D3D9's fallback
						}
						Direct3d11_StateCache::setAlphaTest(
							engPass->m_alphaTestEnable,
							static_cast<float>(refUint) / 255.0f);
					}

					// Plan 11-09.15 Iter-44A: per-pass DEPTH state. Mirrors
					// D3D9's RSB/RSM writes for ZENABLE/ZWRITEENABLE/ZFUNC
					// (Direct3d9_ShaderImplementationData.cpp:255-257). Pre-
					// Iter-44A every D3D11 draw used the install() defaults
					// (DepthEnable=TRUE, WriteMask=ALL, Func=LESS_EQUAL),
					// causing the "eyes through back-of-head" symptom: the
					// eye/decal pass on the skeletal character expected
					// either Z-disabled or Always-compare to render last over
					// the back-of-head opaque pass, but got LESS_EQUAL with
					// write-all so it depth-failed in the wrong direction.
					Direct3d11_StateCache::setDepthEnable(engPass->m_zEnable);
					Direct3d11_StateCache::setDepthWriteEnable(engPass->m_zWrite);
					Direct3d11_StateCache::setDepthCompareFunc(translateCompare(engPass->m_zCompare));

					// Plan 11-09.15 Iter-44A: per-pass COLOR-WRITE mask.
					// Mirrors D3D9's RSB-equivalent push of COLORWRITEENABLE.
					// Engine bits are D3D9 D3DCOLORWRITEENABLE_* which are
					// bitwise-identical to D3D11_COLOR_WRITE_ENABLE_* so the
					// value passes through unchanged.
					Direct3d11_StateCache::setColorWriteEnable(engPass->m_writeEnable);

					// Plan 11-09.15 Iter-44C REVERTED 2026-05-23: re-attempt
					// of per-pass blend factors with 44A+B+E prerequisites in
					// place. Smoke STILL regressed (screenShot0007.jpg vs
					// 0006.jpg) -- snow patches on mountain, larger white
					// square particle, brighter washed-out scene. The
					// consult-44 deep-dive (both CODEX and Cursor) confirmed
					// the diagnosis: blend factors are technically correct
					// but they AMPLIFY the underlying PS-gen problem (multi-
					// stage textures bound to slots 1+ that the dynamic PS
					// at Direct3d11_PixelShaderProgramData.cpp:407-472 does
					// not sample). State-only iters have exhausted their
					// useful work; Phase 12 needs the asset PS pipeline +
					// Pass::apply constant uploads. Keeping the
					// setAlphaBlendFactors infrastructure in StateCache for
					// the eventual re-land in Phase 12 once PS gen is
					// correct.
				}
			}

			// Plan 11-09.14: slot-N SRV/sampler binding via the public
			// StateCache setters (CODEX Q1 -- never call PSSet* directly
			// from StaticShaderData; preserve the lazy-bind model). The
			// shadow writes here are flushed at the next applyPreDrawState's
			// PSSetShaderResources/PSSetSamplers full-array flush.
			//
			// Plan 11-09.15 Iter-44E: extended from slot-0-only to all 8
			// stages. Always unbind absent stages to avoid sticky cross-
			// shader SRV bleed (CODEX Q4 model). Per-shader logging on
			// first apply lets us cross-reference multi-stage hits with
			// the visual symptoms (gray-eye / head-clipping diagnostic
			// from 2026-05-23).
			PerPassStages const &stages = m_passStages[idx];

			// Plan 17-09 (bump-arm texture-swap fix): the engine resolves textures by
			// D3D9 stage index, but the recompiled PS may assign SRV registers in a
			// different order (fxc first-use ordering when splitting sampler2D). Map
			// each present stage's SRV to the slot the PS actually reads (identity for
			// single-texture / unreflected PSes). SAMPLERS stay at the stage index --
			// the PSRC pins sampler registers to stage order (s0=diffuse, s1=normal),
			// confirmed correct in the RenderDoc capture; only the texture diverged.
			// Build the SRV slot array FIRST so an absent stage cannot clear a slot a
			// present stage remapped its SRV into (the slots are a permutation).
			Direct3d11_PixelShaderProgramData const * const psData = livePS;   // UAF fix: live, not the stale snapshot
			ID3D11ShaderResourceView * srvForSlot[kMaxStages] = {};
			for (int slotIdx = 0; slotIdx < kMaxStages; ++slotIdx)
			{
				Stage const &stage = stages[slotIdx];
				if (stage.m_present && stage.m_texture && *stage.m_texture)
				{
					int const srvSlot = psData ? psData->getSrvSlotForStage(slotIdx) : slotIdx;
					if (srvSlot >= 0 && srvSlot < kMaxStages)
						srvForSlot[srvSlot] = (*stage.m_texture)->getShaderResourceView();
				}
			}
			for (int slotIdx = 0; slotIdx < kMaxStages; ++slotIdx)
			{
				Stage const &stage = stages[slotIdx];
				// SRV: bind the (possibly remapped) resource for this slot; a null
				// clears it -- covers both absent stages and present-but-no-texture.
				Direct3d11_StateCache::setPixelShaderResource(slotIdx, srvForSlot[slotIdx]);
				// Sampler: unchanged from Iter-44E -- bound at the stage/sampler
				// register for every present stage (regardless of texture presence).
				if (stage.m_present)
					Direct3d11_StateCache::setPixelShaderSampler(slotIdx, stage.m_samplerDesc);
			}

#if P19_LIGHTDUMP
			// Phase 19 (19-04 Task 0b) DEV-ONLY: the ACTUAL SRVs bound for THIS draw,
			// per slot, after the stage->SRV remap. Correlate by shader+frame with the
			// b0 line. If a surface's bound SRV pointers change frame-to-frame while it
			// still cycles, an SRV-slot/texture instability (Phase-17 precedent) is the
			// cycling source; if they are stable, textures are not the cause. REVERT after.
			{
				static unsigned s_lastFrameSrv = 0xFFFFFFFFu;
				static int      s_srvThisFrame = 0;
				unsigned const frameSrv = Direct3d11_Device::getDiagFrameIndex();
				if (frameSrv != s_lastFrameSrv) { s_lastFrameSrv = frameSrv; s_srvThisFrame = 0; }
				int const srvIdx = s_srvThisFrame++;
				if (srvIdx < 48)
				{
					char const *snv = (m_shader ? m_shader->getStaticShaderTemplate().getName().getString() : "(null)");
					if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
					{
						char line[512];
						_snprintf_s(line, sizeof(line), _TRUNCATE,
							"[P19DUMP] f=%u srv#%d shader='%s' pass=%d srv=[%p %p %p %p %p %p %p %p]",
							frameSrv, srvIdx, (snv && *snv) ? snv : "(empty)", passNumber,
							(void*)srvForSlot[0], (void*)srvForSlot[1], (void*)srvForSlot[2], (void*)srvForSlot[3],
							(void*)srvForSlot[4], (void*)srvForSlot[5], (void*)srvForSlot[6], (void*)srvForSlot[7]);
						iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, line);
					}
				}
			}
#endif
			// Iter-44E one-shot log: first 50 apply() calls that have at
			// least one present stage with index > 0 (the new territory
			// this iter opens up). Tells us which assets actually exercise
			// multi-stage binding so we can correlate against the visual
			// symptoms.
			{
				static int s_iter44eLogged = 0;
				if (s_iter44eLogged < 50)
				{
					int maxSlot = -1;
					for (int s = 0; s < kMaxStages; ++s)
						if (stages[s].m_present) maxSlot = s;
					if (maxSlot > 0)
					{
						++s_iter44eLogged;
						if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
						{
							char templateName[256] = "<unknown>";
							if (m_shader)
							{
								CrcString const &name = m_shader->getStaticShaderTemplate().getName();
								char const *fn = name.getString();
								if (fn && *fn)
								{
									_snprintf_s(templateName, sizeof(templateName), _TRUNCATE, "%s", fn);
								}
							}
							char stageMap[64] = {};
							for (int s = 0; s < kMaxStages; ++s)
							{
								char piece[8];
								_snprintf_s(piece, sizeof(piece), _TRUNCATE,
									"%s%d=%c", (s == 0 ? "" : ","), s,
									stages[s].m_present
										? ((stages[s].m_texture && *stages[s].m_texture) ? 'T' : '_')
										: '.');
								strncat_s(stageMap, sizeof(stageMap), piece,
									sizeof(stageMap) - strlen(stageMap) - 1);
							}
							char buf[512];
							_snprintf_s(buf, sizeof(buf), _TRUNCATE,
								"Plan 11-09.15 Iter-44E multi-stage#%d sht='%s' pass=%d maxSlot=%d stages=[%s] "
								"(T=tex bound, _=present but unresolved, .=absent)",
								s_iter44eLogged, templateName, passNumber, maxSlot, stageMap);
							iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, buf);
						}
					}
				}
			}
		}
		else
		{
			// Out-of-bounds pass index. Clear the slot so
			// applyPreDrawState's null-VS guard skips the draw rather than
			// re-using stale state from another shader. Plan 11-09.15
			// Iter-44E: clear all 8 PS SRV slots (multi-stage extension)
			// rather than only slot 0.
			Direct3d11_StateCache::setCurrentVSData(nullptr, 0);   // Plan 17-09: clear (key irrelevant)
			Direct3d11_StateCache::setCurrentPSData(nullptr);
			for (int slotIdx = 0; slotIdx < kMaxStages; ++slotIdx)
				Direct3d11_StateCache::setPixelShaderResource(slotIdx, nullptr);
		}
	}

	if (passNumber < 0 || static_cast<size_t>(passNumber) >= m_passVS.size())
		return false;
	return m_passVS[static_cast<size_t>(passNumber)] != nullptr;
}

// ======================================================================
