// ======================================================================
//
// Direct3d11_ShaderImplementationData.h
// Phase 11 D3D11 renderer plugin -- per-ShaderImplementation graphics
// data wrapper. Plan 11-07 Iteration 1.
//
// Engine side: each loaded ShaderImplementation asset (one per .shi /
// .sht file) gets a backend-owned GraphicsData child via
// Graphics::createShaderImplementationGraphicsData. The D3D9 plugin's
// equivalent (Direct3d9_ShaderImplementationData) stores per-pass FFP
// texture-stage-state lists or VSPS pixel-shader-program pointers; in
// D3D11 the FFP path is descoped (D-04a) and the pixel-shader programs
// are wrapped by Direct3d11_PixelShaderProgramData (Plan 11-05).
//
// For Plan 11-07 Iteration 1 this wrapper is INTENTIONALLY minimal:
//   - construct successfully so the engine's
//     ShaderImplementation::load_NNNN() path completes
//   - retain a back-pointer to the ShaderImplementation for later
//     iterations that need per-pass info (alpha blend, color write,
//     pixel-shader program -> Direct3d11_PixelShaderProgramData lookup)
//   - apply(passNumber) is a recorded no-op -- per-pass D3D11 state
//     application (RS/BS/DSS overrides from the asset) lands in a
//     later iteration as the smoke surfaces the need
//
// This is the canonical "Iteration 1 known stub" -- enough surface for
// the engine to leave the factory slot without FATAL, no per-pass state
// application yet. Subsequent iterations land state-apply work driven
// by actual visual symptoms.
//
// Per D-04a: NO FFP per-pass code paths (Direct3d9_ShaderImplementationData
// has #ifdef FFP regions for TextureStageState arrays; we omit those).
// Per D-13: ComPtr ownership only (no D3DPOOL_MANAGED / OnLostDevice).
//
// ======================================================================

#ifndef INCLUDED_Direct3d11_ShaderImplementationData_H
#define INCLUDED_Direct3d11_ShaderImplementationData_H

// ======================================================================

class MemoryBlockManager;
class ShaderImplementation;

#include "clientGraphics/ShaderImplementation.h"

// ======================================================================

class Direct3d11_ShaderImplementationData : public ShaderImplementationGraphicsData
{
public:

	static void *operator new(size_t size);
	static void  operator delete(void *memory);

	static void install();
	static void remove();

public:

	explicit Direct3d11_ShaderImplementationData(ShaderImplementation const &implementation);
	virtual ~Direct3d11_ShaderImplementationData() override;

	// Apply per-pass D3D11 state (alpha blend / z / color-write overrides
	// from the asset's ShaderImplementation::Pass). Iteration 1 recorded
	// no-op; later iterations populate based on what the smoke surfaces.
	void apply(int passNumber) const;

	// Accessor for later iterations -- StaticShaderData::apply() routes
	// per-pass material / texture work and needs the implementation back
	// to walk the pixel-shader program list.
	ShaderImplementation const * getImplementation() const;

private:

	Direct3d11_ShaderImplementationData();
	Direct3d11_ShaderImplementationData(Direct3d11_ShaderImplementationData const &);
	Direct3d11_ShaderImplementationData &operator =(Direct3d11_ShaderImplementationData const &);

private:

	static MemoryBlockManager *ms_memoryBlockManager;

private:

	ShaderImplementation const * m_implementation;
};

// ======================================================================

inline ShaderImplementation const * Direct3d11_ShaderImplementationData::getImplementation() const
{
	return m_implementation;
}

// ======================================================================

#endif
