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

class MemoryBlockManager;
class ShaderImplementation;
class StaticShader;

#include "clientGraphics/StaticShader.h"

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
};

// ======================================================================

#endif
