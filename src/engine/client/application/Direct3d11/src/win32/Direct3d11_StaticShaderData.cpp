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
#include "Direct3d11_StateCache.h"
#include "Direct3d11_VertexShaderData.h"

#include "clientGraphics/ShaderEffect.h"          // m_effect -> ShaderEffect *
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
	m_passPS()
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
	UNREF(shader);

	m_passVS.clear();
	m_passPS.clear();

	if (!m_implementation)
		return;

	ShaderImplementation::Passes const & passes = *m_implementation->m_pass;
	size_t const passCount = passes.size();
	m_passVS.assign(passCount, nullptr);
	m_passPS.assign(passCount, nullptr);

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
	// (Direct3d9_StaticShaderData.cpp:1045) minus the material/texFactor/
	// alpha-test/fog/stencil/texture-stage wiring (CODEX scoped Iter-1 to
	// VS+PS pointers only; those features land Iter-3+ as visual-correctness
	// symptoms surface).
	//
	// CODEX-reframed Iter-1 acceptance bar: "draw activity observable, not
	// geometry visible." A returned true here trips
	// applyPreDrawState::resolveShaders into reaching VertexDeclarationMap::
	// getOrCreate -- producing the CreateInputLayout + IASet*/Draw* InfoQueue
	// traffic that is the Iter-1 success signal.
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
		}
		else
		{
			// Out-of-bounds pass index. Clear the slot so
			// applyPreDrawState's null-VS guard skips the draw rather than
			// re-using stale state from another shader.
			Direct3d11_StateCache::setCurrentVSData(nullptr);
			Direct3d11_StateCache::setCurrentPSData(nullptr);
		}
	}

	if (passNumber < 0 || static_cast<size_t>(passNumber) >= m_passVS.size())
		return false;
	return m_passVS[static_cast<size_t>(passNumber)] != nullptr;
}

// ======================================================================
