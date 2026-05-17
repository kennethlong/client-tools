// ======================================================================
//
// Direct3d11_ShaderImplementationData.cpp
// Phase 11 D3D11 renderer plugin -- per-ShaderImplementation graphics
// data wrapper. Plan 11-07 Iteration 1.
//
// See header preamble for design rationale (D-04a omission of FFP path,
// minimal-Iteration-1 contract).
//
// ======================================================================

#include "FirstDirect3d11.h"
#include "Direct3d11_ShaderImplementationData.h"

#include "Direct3d11.h"

#include "clientGraphics/ShaderImplementation.h"
#include "sharedDebug/DebugFlags.h"
#include "sharedFoundation/MemoryBlockManager.h"

// ======================================================================

MemoryBlockManager *Direct3d11_ShaderImplementationData::ms_memoryBlockManager = nullptr;

// ======================================================================

void Direct3d11_ShaderImplementationData::install()
{
	DEBUG_FATAL(ms_memoryBlockManager, ("Direct3d11_ShaderImplementationData already installed"));
	ms_memoryBlockManager = new MemoryBlockManager(
		"Direct3d11_ShaderImplementationData", true,
		sizeof(Direct3d11_ShaderImplementationData), 0, 0, 0);
}

// ----------------------------------------------------------------------

void Direct3d11_ShaderImplementationData::remove()
{
	delete ms_memoryBlockManager;
	ms_memoryBlockManager = nullptr;
}

// ----------------------------------------------------------------------

void *Direct3d11_ShaderImplementationData::operator new(size_t size)
{
	UNREF(size);
	NOT_NULL(ms_memoryBlockManager);
	DEBUG_FATAL(size != sizeof(Direct3d11_ShaderImplementationData), ("wrong new called"));
	return ms_memoryBlockManager->allocate();
}

// ----------------------------------------------------------------------

void Direct3d11_ShaderImplementationData::operator delete(void *memory)
{
	NOT_NULL(ms_memoryBlockManager);
	ms_memoryBlockManager->free(memory);
}

// ======================================================================

Direct3d11_ShaderImplementationData::Direct3d11_ShaderImplementationData(ShaderImplementation const &implementation)
:	ShaderImplementationGraphicsData(),
	m_implementation(&implementation)
{
	// Iteration 1: construct only. Per-pass D3D11 state (BS/DSS overrides
	// from the asset's ShaderImplementation::Pass fields -- alphaBlend,
	// zEnable/zWrite/zCompare, writeEnable, etc.) is NOT applied here.
	// That work lands when the smoke surfaces a specific visual symptom
	// (e.g. additive particles rendering wrong because blend state isn't
	// being applied per-pass).
	//
	// Currently the Plan 11-06 Direct3d11_StateCache defaults are used
	// for every pass: CULL_BACK, DEPTH_TEST=true, DEPTH_FUNC=LESS_EQUAL,
	// no blend. Most opaque geometry should render correctly with those
	// defaults; transparency / additive / depth-tweak passes will need
	// per-pass overrides in later iterations.
	UNREF(implementation);
}

// ----------------------------------------------------------------------

Direct3d11_ShaderImplementationData::~Direct3d11_ShaderImplementationData()
{
	m_implementation = nullptr;
}

// ======================================================================

void Direct3d11_ShaderImplementationData::apply(int /*passNumber*/) const
{
	// Iteration 1: recorded no-op.
	//
	// The D3D9 sibling (Direct3d9_ShaderImplementationData::apply) calls
	// Direct3d9_StateCache::setRenderState(...) for every per-pass
	// override (alphaBlend, colorWrite, z-compare, stencil) and -- under
	// FFP -- iterates per-stage TextureStageState lists. Under D3D11:
	//
	//   - FFP per-stage texture-op state is descoped (D-04a). Pixel
	//     work happens in HLSL pixel shaders (Plan 11-05); the D3D9
	//     FFP combiner state has no D3D11 analog.
	//
	//   - Per-pass alpha-blend / z / color-write overrides ARE relevant
	//     under D3D11 (they map to ID3D11BlendState +
	//     ID3D11DepthStencilState descriptor mutations against the
	//     Plan 11-06 StateCache caches). The mapping is mechanical:
	//
	//       m_alphaBlendEnable        -> bsDesc.RenderTarget[0].BlendEnable
	//       m_alphaBlendSource / Dest -> bsDesc.SrcBlend / DestBlend
	//       m_alphaBlendOperation     -> bsDesc.BlendOp
	//       m_writeEnable             -> bsDesc.RenderTarget[0].RenderTargetWriteMask
	//       m_zEnable / m_zWrite      -> dssDesc.DepthEnable / DepthWriteMask
	//       m_zCompare                -> dssDesc.DepthFunc
	//
	//     Plan 11-07 Iteration 2+ (driven by smoke) will land these.
	//
	// For now: do nothing. The engine's per-frame draw loop applies
	// StateCache defaults; that's enough to surface what fails next.
}

// ======================================================================
