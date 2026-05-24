// ======================================================================
//
// Direct3d11_StaticShaderData.h
// Phase 11 D3D11 renderer plugin -- per-StaticShader graphics data wrapper.
// Plan 11-07 Iteration 1.
//
// Engine side: each loaded StaticShader (the resolved material+texture+
// shader-template combination held by every renderable instance) gets
// a backend-owned GraphicsData child via
// Graphics::createStaticShaderGraphicsData. This wrapper holds the
// per-instance state the engine resolves at load time, and applies that
// state on each draw via Direct3d11_StateCache::setStaticShader.
//
// Mandatory virtual surface (from clientGraphics/StaticShader.h):
//   - update(StaticShader const &)   -- engine notifies us of mutation
//   - getTextureSortKey() const      -- engine asks for a render-sort key
//
// In addition we provide apply(pass) for use by Plan 11-06's
// Direct3d11_StateCache::setStaticShader slot binder; it currently
// records the bind but does not consume it. Iteration 1 begins
// consuming the bind by:
//
//   1. Resolving the pass's VS via the pass's
//      ShaderImplementationPassVertexShader -> Direct3d11_VertexShaderData
//      lookup; binding via VSSetShader (or skipping if NULL VS).
//   2. Resolving the pass's PS via the pass's
//      ShaderImplementationPassPixelShader::m_pixelShaderProgram ->
//      Direct3d11_PixelShaderProgramData lookup; binding via PSSetShader
//      (per Plan 11-05, the PS is currently always NULL for stock
//      assets because the bytecode is D3D9-era; D3D11 default
//      pass-through pixel pipeline takes over).
//
// Per Iteration 1's "minimum viable" contract we do NOT bind per-pass
// textures, materials, samplers yet. Those land as subsequent iteration
// fixes when smoke surfaces specific missing visuals (e.g. untextured
// geometry, wrong colors).
//
// Per D-04a: FFP construction paths in Direct3d9_StaticShaderData
// (#ifdef FFP regions for D3DTSS_TEXTURETRANSFORMFLAGS, etc.) are
// INTENTIONALLY OMITTED.
// Per D-13: ComPtr ownership only.
//
// ======================================================================

#ifndef INCLUDED_Direct3d11_StaticShaderData_H
#define INCLUDED_Direct3d11_StaticShaderData_H

// ======================================================================

class Direct3d11_PixelShaderProgramData;
class Direct3d11_TextureData;
class Direct3d11_VertexShaderData;
class MemoryBlockManager;
class ShaderImplementation;
class StaticShader;

#include "clientGraphics/StaticShader.h"

#include <d3d11.h>
#include <array>
#include <vector>

// ======================================================================

class Direct3d11_StaticShaderData : public StaticShaderGraphicsData
{
public:

	static void *operator new(size_t size);
	static void  operator delete(void *memory);

	static void install();
	static void remove();
	static void beginFrame();

public:

	explicit Direct3d11_StaticShaderData(StaticShader const &shader);
	virtual ~Direct3d11_StaticShaderData() override;

	// StaticShaderGraphicsData virtuals.
	virtual void update(StaticShader const &shader) override;
	virtual int  getTextureSortKey() const override;

	// Per-draw apply -- called from Direct3d11_StateCache::setStaticShader
	// (Plan 11-06 stub-now real-here). Returns true if a vertex shader
	// was successfully bound; false otherwise (caller may skip draw).
	bool apply(int passNumber) const;

	// Plan 11-09.15 Iter-29B diagnostic: return the active StaticShader's
	// template name (i.e. the .sht asset path) for logging in drawQuadList,
	// or "<none>" when nothing is currently bound. CODEX+Cursor consult
	// recommended this routing diagnostic to localize whether font draws
	// hit shader/uicanvas_filtered.sht (with the current Iter-28 modulate
	// PS) or some other template path. ms_active is a private static, so
	// expose via a public accessor rather than friending StateCache.
	static char const * getActiveStaticShaderName();

private:

	Direct3d11_StaticShaderData();
	Direct3d11_StaticShaderData(Direct3d11_StaticShaderData const &);
	Direct3d11_StaticShaderData &operator =(Direct3d11_StaticShaderData const &);

	void construct(StaticShader const &shader);

private:

	static MemoryBlockManager *ms_memoryBlockManager;

	// Active-binding cache (matches D3D9 sibling pattern; prevents redundant
	// per-draw VS/PS rebinds when consecutive draws share the same shader).
	static Direct3d11_StaticShaderData const * ms_active;
	static int                                 ms_activePass;

private:

	StaticShader const *         m_shader;
	ShaderImplementation const * m_implementation;

	// Plan 11-09 Iter-1: per-pass VS + PS pointer cache. Populated by
	// construct() via the public StaticShader -> StaticShaderTemplate ->
	// ShaderEffect -> ShaderImplementation traversal (CODEX consult locked
	// the friend set at ShaderImplementation.h only; m_pass private access
	// is the only field that needs friend). Non-owning -- engine owns the
	// VertexShader / PixelShaderProgram lifetimes via the ShaderImplementation
	// graph above; we hold raw pointers per CODEX Q4 D3D9-mirror guidance.
	std::vector<Direct3d11_VertexShaderData const *>          m_passVS;
	std::vector<Direct3d11_PixelShaderProgramData const *>    m_passPS;

	// Plan 11-09.14 (CODEX Bucket A): per-pass slot-0 stage cache. Mirrors
	// the D3D9 sibling's Direct3d9_StaticShaderData::Stage but minimal --
	// only the m_textureIndex==0 entry of pass.m_pixelShader->m_textureSamplers,
	// because the Plan 11-09.13 Iter-4 dynamic fallback PS samples ONLY
	// register(t0)/register(s0). Slots 1..7 are intentionally out-of-scope
	// (CODEX Q1 Bucket B rejected as dead bind cost).
	//
	// Lifetime: Direct3d11_TextureData const * const * is a pointer-to-pointer
	// into the engine Texture's m_graphicsData field, so texture reloads
	// (engine swaps *m_graphicsData) automatically reflect at apply time
	// via the double-indirection deref. Mirrors D3D9 sibling Stage::m_texture
	// (Direct3d9_StaticShaderData.cpp:327).
	struct Stage
	{
		bool                                   m_present;       // false if pass has no sampler at this textureIndex -> apply unbinds SRV
		Direct3d11_TextureData const * const * m_texture;       // null when global-texture short-circuit returned null OR textureData.texture was null
		D3D11_SAMPLER_DESC                     m_samplerDesc;   // zero-init + filled by build helper
	};

	// Plan 11-09.15 Iter-44E: multi-stage texture binding. D3D9 had 8 texture
	// stages; D3D11 supports 128 SRV slots but the engine's shader assets all
	// stay within the D3D9 limit. Plan 11-09.14 (Iter-39C era) wired only
	// stage 0; Iter-44E extends to all 8. Kenny's "gray eyes" diagnostic
	// 2026-05-23 surfaced the gap -- character passes whose eye texture is
	// on stage 1+ were getting no SRV bound on that stage, so the PS read
	// whatever was sticky from the previous draw (default gray).
	static constexpr int kMaxStages = 8;
	typedef std::array<Stage, kMaxStages> PerPassStages;
	std::vector<PerPassStages>                                m_passStages;
};

// ======================================================================

#endif
