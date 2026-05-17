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
#include "Direct3d11_PixelShaderProgramData.h"
#include "Direct3d11_VertexShaderData.h"

#include "clientGraphics/ShaderImplementation.h"
#include "clientGraphics/StaticShader.h"
#include "clientGraphics/StaticShaderTemplate.h"
#include "sharedDebug/DebugFlags.h"
#include "sharedFoundation/MemoryBlockManager.h"

// ======================================================================

MemoryBlockManager *Direct3d11_StaticShaderData::ms_memoryBlockManager = nullptr;

Direct3d11_StaticShaderData const * Direct3d11_StaticShaderData::ms_active     = nullptr;
int                                 Direct3d11_StaticShaderData::ms_activePass = -1;

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
	m_implementation(nullptr)
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
	// We need the ShaderImplementation reference for apply()'s per-pass
	// VS/PS lookup. The engine wires it as
	//   shader.getStaticShaderTemplate().m_effect->m_implementation
	// in the D3D9 sibling (Direct3d9_StaticShaderData ctor at line 988).
	// However m_effect is a PRIVATE field of StaticShaderTemplate and
	// the engine's friend lists for that field include Direct3d8/9 only.
	//
	// For Iteration 1 we DO NOT need it -- we deliberately use only the
	// public StaticShader surface (which doesn't expose the
	// ShaderImplementation either). apply() therefore cannot walk pass
	// state in this iteration; it routes through the Plan 11-06 default
	// state machinery.
	//
	// Subsequent iterations (driven by smoke) will likely need to add a
	// Direct3d11_StaticShaderData friend to ShaderEffect /
	// StaticShaderTemplate / ShaderImplementation alongside the existing
	// Direct3d8/9 friends -- mirrors the Plan 11-04/06 Rule-3 friend
	// pattern. We defer that until the bind work actually needs it.
	UNREF(shader);
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
	// Per-draw shader binding. Iteration 1 contract:
	//
	//   1. Activate-cache check -- if we're already the active shader at
	//      this pass, skip rebind work (matches D3D9 sibling shape and
	//      avoids redundant per-draw bindings within a sorted batch).
	//   2. Record the bind so subsequent iterations have a canonical
	//      entry point to thread per-pass VS/PS lookup through.
	//   3. Return false -- "no vertex shader successfully bound" --
	//      so the Plan 11-06 draw-call dispatch keeps using the engine's
	//      previously-bound VS (or skips the draw via applyPreDrawState's
	//      NULL-VS guard, which is the iteration-1 expected behavior).
	//
	// Per-pass VS / PS binding requires friend access to
	// ShaderImplementation::m_pass + StaticShaderTemplate::m_effect (see
	// construct() rationale). That friend extension lands when smoke
	// surfaces a specific symptom that requires it (e.g. solid-color
	// rendering because the engine's default VS doesn't transform
	// correctly for the asset's vertex format).
	if (ms_active == this && ms_activePass == passNumber)
		return false;

	ms_active     = this;
	ms_activePass = passNumber;

	UNREF(passNumber);
	return false;
}

// ======================================================================
