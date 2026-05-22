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
#include "Direct3d11_Device.h"   // Plan 11-09.15 Iter-17: getInfoQueue for stage0-resolve diagnostic
#include "Direct3d11_PixelShaderProgramData.h"
#include "Direct3d11_StateCache.h"
#include "Direct3d11_TextureData.h"
#include "Direct3d11_VertexShaderData.h"

#include "clientGraphics/ShaderEffect.h"          // m_effect -> ShaderEffect *
#include "clientGraphics/ShaderImplementation.h"
#include "clientGraphics/StaticShader.h"
#include "clientGraphics/StaticShaderTemplate.h"
#include "clientGraphics/Texture.h"
#include "sharedDebug/DebugFlags.h"
#include "sharedFoundation/MemoryBlockManager.h"

#include <algorithm>

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
	m_passStage0()  // Plan 11-09.14
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
	m_passStage0.clear();

	if (!m_implementation)
		return;

	ShaderImplementation::Passes const & passes = *m_implementation->m_pass;
	size_t const passCount = passes.size();
	m_passVS.assign(passCount, nullptr);
	m_passPS.assign(passCount, nullptr);
	m_passStage0.assign(passCount, Stage0{ false, nullptr, {} });

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
		}

		// Plan 11-09.14 (CODEX Bucket A): walk pass->m_pixelShader->m_textureSamplers,
		// find the entry whose m_textureIndex == 0 (D3D9 indexes m_stage by
		// destination texture index, not by iteration order -- CODEX correction #1).
		// Break after first match because Bucket A scope is slot 0 only.
		++s_passesTotal;
		if (pass->m_pixelShader && pass->m_pixelShader->m_textureSamplers)
		{
			using TexSamplers = ShaderImplementation::Pass::PixelShader::TextureSamplers;
			TexSamplers const &samplers = *pass->m_pixelShader->m_textureSamplers;
			for (TexSamplers::const_iterator si = samplers.begin(); si != samplers.end(); ++si)
			{
				ShaderImplementation::Pass::PixelShader::TextureSampler const *ts = *si;
				if (!ts || ts->m_textureIndex != 0)
					continue;

				Stage0 &stage = m_passStage0[passIndex];
				stage.m_present = true;
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

				// Plan 11-09.15 Iter-17: log stage-0 texture resolution for
				// the first 30 shader builds. Iter-16 confirmed all splash
				// draws bind the same wrong 1024x768 BGRA RT to SRV0 -- this
				// instruments the build path so we can see WHICH tag the
				// splash shader's stage 0 uses, whether it hits the global-
				// texture short-circuit (tag starts with '_' OR == TAG_ENVM),
				// AND what the per-shader textureData.texture resolves to.
				{
					static int s_iter17BuildCount = 0;
					if (s_iter17BuildCount < 30)
					{
						++s_iter17BuildCount;
						if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
						{
							Tag const t = ts->m_textureTag;
							char tagStr[5] = { static_cast<char>((t >> 24) & 0xFF),
							                   static_cast<char>((t >> 16) & 0xFF),
							                   static_cast<char>((t >>  8) & 0xFF),
							                   static_cast<char>((t      ) & 0xFF),
							                   '\0' };
							for (int k = 0; k < 4; ++k)
								if (tagStr[k] < 0x20 || tagStr[k] > 0x7E) tagStr[k] = '?';
							char buf[384];
							_snprintf_s(buf, sizeof(buf), _TRUNCATE,
								"Plan 11-09.15 Iter-17 stage0-resolve#%d pass=%d "
								"tag='%s' (0x%08X) isGlobal=%d "
								"textureData.texture=0x%p stage.m_texture=0x%p",
								s_iter17BuildCount, passIndex,
								tagStr, static_cast<unsigned int>(t), tagIsGlobal ? 1 : 0,
								static_cast<void const *>(textureData.texture),
								static_cast<void const *>(stage.m_texture));
							iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, buf);
						}
					}
				}

				if (stage.m_texture)
					++s_passesWithSampler0TextureResolved;

				stage.m_samplerDesc = buildSamplerDescForStage0(textureData, *ts);

				break;  // Bucket A: slot 0 only
			}
		}
	}
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
			Direct3d11_StateCache::setCurrentVSData(m_passVS[idx]);
			Direct3d11_StateCache::setCurrentPSData(m_passPS[idx]);

			// Plan 11-09.14: slot-0 SRV/sampler binding via the public
			// StateCache setters (CODEX Q1 -- never call PSSet* directly
			// from StaticShaderData; preserve the lazy-bind model). The
			// shadow write here is flushed at the next applyPreDrawState's
			// PSSetShaderResources/PSSetSamplers full-array flush.
			Stage0 const &stage = m_passStage0[idx];
			if (!stage.m_present)
			{
				// Pass has no m_textureIndex==0 sampler. CODEX Q4 says
				// unbind SRV0 to avoid sticky cross-shader bleed; sampler0
				// can keep the default (no functional issue per spec).
				Direct3d11_StateCache::setPixelShaderResource(0, nullptr);
			}
			else if (!stage.m_texture || !*stage.m_texture)
			{
				// Sampler exists in the pass but texture didn't resolve
				// (tag missing from TextureDataMap, or m_graphicsData null,
				// or global-texture lookup returned null). Apply the
				// resolved sampler so the slot still has defined state, but
				// unbind SRV0. Fallback-PS will read 0 -> magenta/black.
				Direct3d11_StateCache::setPixelShaderResource(0, nullptr);
				Direct3d11_StateCache::setPixelShaderSampler(0, stage.m_samplerDesc);
			}
			else
			{
				Direct3d11_StateCache::setPixelShaderResource(0,
					(*stage.m_texture)->getShaderResourceView());
				Direct3d11_StateCache::setPixelShaderSampler(0, stage.m_samplerDesc);
			}
		}
		else
		{
			// Out-of-bounds pass index. Clear the slot so
			// applyPreDrawState's null-VS guard skips the draw rather than
			// re-using stale state from another shader.
			Direct3d11_StateCache::setCurrentVSData(nullptr);
			Direct3d11_StateCache::setCurrentPSData(nullptr);
			Direct3d11_StateCache::setPixelShaderResource(0, nullptr);
		}
	}

	if (passNumber < 0 || static_cast<size_t>(passNumber) >= m_passVS.size())
		return false;
	return m_passVS[static_cast<size_t>(passNumber)] != nullptr;
}

// ======================================================================
