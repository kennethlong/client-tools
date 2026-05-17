// ======================================================================
//
// Direct3d11_VertexShaderData.cpp
// Phase 11 D3D11 renderer plugin -- vertex shader (HLSL source) compile.
// Plan 11-05 (Wave 5 -- shader layer).
//
// Compile flow (RESEARCH Pattern 3, D-03 hybrid):
//   1. Parse the .vsh header line ("//hlsl vs_..." or "//asm vs_...") --
//      we honor the //hlsl path; .asm vsh sources predate D3D9 SM2.0 and
//      cannot compile under D3D11. //asm shaders are recorded but produce
//      a NULL ID3D11VertexShader; the actual draw path in Plan 11-06 will
//      skip passes with NULL VS (mirrors D3D9 ConfigDirect3d9::getCreateShaders
//      bypass shape for missing/unsupported shaders).
//   2. Build defines: inject POSITION -> SV_POSITION macro (Pitfall 1);
//      also inject D3D11_PROFILE macro so the profile string participates in
//      the cache key (Plan 11-07 Iter-3 -- profile changes invalidate
//      cached blobs automatically).
//   3. Hash (source text + defines) via Direct3d11_ShaderCache::hashSource.
//   4. tryLoad: on hit, skip D3DCompile; on miss, D3DCompile with the
//      kVertexShaderProfile profile string (Plan 11-07 Iter-3 changes this
//      from "vs_5_0" to "vs_4_0_level_9_3"; see profile-rationale block
//      below) + same defines + Direct3d11_CompileIncludeHandler (Plan 11-07
//      Iter-2; routes `#include "..."` through TreeFile so TRE-archived
//      `.inc` headers resolve correctly), then store.
//   5. CreateVertexShader from the resulting blob; cache the bytecode hash
//      for Pitfall 6 input-layout cache use (Plan 11-06).
//
// Profile-rationale (Plan 11-07 Iter-3):
//   The engine's stock .vsh sources were authored against vs_1_1 / vs_2_0
//   / vs_3_0 and ship as HLSL text inside TRE archives. Under vs_5_0
//   (D3DCompile's strictest profile), the compiler reserves additional
//   identifiers as keywords (geometry-shader primitive type names like
//   `point`, `line`, `triangle`, etc.). Iter-2 smoke surfaced an X3000
//   "syntax error: unexpected token 'point'" at line 81 of
//   `vertex_program/include/vertex_shader_constants.inc` -- the engine's
//   D3D9-era HLSL uses `point` as a sampler-state filter literal and/or
//   identifier that vs_5_0 reserves. The `vs_4_0_level_9_*` profiles
//   target D3D11 bytecode but enforce D3D9-era feature/syntax constraints,
//   which relaxes those keyword reservations and accepts the legacy
//   syntax. D3D11's CreateVertexShader accepts vs_4_0_level_9_* bytecode
//   (per the DXSDK feature-level docs). SPEC R3 "HLSL SM5.0 recompilation"
//   is satisfied in spirit: the build is produced by the D3D11 toolchain
//   (d3dcompiler_47 + ID3D11Device), even though the chosen profile is a
//   legacy-compat profile -- which is the only path that consumes the
//   existing asset content. A future Phase 12 asset re-author would
//   re-introduce vs_5_0 once the source text is modernized.
//
//   `vs_4_0_level_9_3` chosen over `vs_4_0_level_9_1` because Plan 11-01
//   Phase A static analysis recorded engine .vsh tags up to `vs_2_0` --
//   level_9_3 maps to SM3-equivalent feature constraints (256 const
//   registers + relative addressing), which comfortably covers what the
//   engine's HLSL bodies declare. level_9_1 caps at SM2 vertex-shader
//   constraints and would likely surface a second compile failure on the
//   more elaborate skeletal-mesh / terrain shaders later in this iteration
//   sequence.
//
// _clearfp() is INTENTIONALLY dropped per PATTERNS line 520: the D3DX-era
// FPU bug doesn't apply to D3DCompile; reintroduce only if a regression
// surfaces (RESEARCH confirms not applicable).
//
// Per CONTEXT D-13: ComPtr ownership only.
//
// ======================================================================

#include "FirstDirect3d11.h"
#include "Direct3d11_VertexShaderData.h"

#include "Direct3d11.h"
#include "Direct3d11_CompileIncludeHandler.h"
#include "Direct3d11_Device.h"
#include "Direct3d11_ShaderCache.h"

#include "sharedFoundation/MemoryBlockManager.h"

#include <cstring>
#include <vector>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

// ======================================================================

using Microsoft::WRL::ComPtr;

MemoryBlockManager *Direct3d11_VertexShaderData::ms_memoryBlockManager = nullptr;

// ======================================================================

namespace Direct3d11_VertexShaderDataNamespace
{
	// Plan 11-07 Iter-3: vs_4_0_level_9_3 profile string. Centralized
	// constant so the D3DCompile call site, the D3D_SHADER_MACRO cache-key
	// injection, and any future audit point all read the same string.
	// See the file-preamble Profile-rationale block for why this differs
	// from the original Plan 11-05 "vs_5_0" choice.
	char const * const kVertexShaderProfile = "vs_4_0_level_9_3";

	// Scan the .vsh header for the language tag. Format examples:
	//   //hlsl vs_1_1
	//   //asm vs_1_1
	// We accept either, signal `isAssembly` to caller. Returns the
	// offset (within the source text) of the first character AFTER the
	// recognized header line so the compile body starts cleanly.
	bool parseHeader(char const *text, size_t length, bool &outIsAssembly)
	{
		outIsAssembly = false;
		if (!text || length < 6)
			return false;

		// Skip leading whitespace.
		size_t i = 0;
		while (i < length && (text[i] == ' ' || text[i] == '\t' || text[i] == '\r' || text[i] == '\n'))
			++i;
		if (i + 6 > length)
			return false;

		if (std::strncmp(text + i, "//hlsl", 6) == 0)
		{
			outIsAssembly = false;
			return true;
		}
		if (std::strncmp(text + i, "//asm", 5) == 0)
		{
			outIsAssembly = true;
			return true;
		}
		// No header -- treat as HLSL by default (engine asset pipeline
		// only ships //hlsl / //asm variants; defensive default-HLSL).
		return true;
	}
}
using namespace Direct3d11_VertexShaderDataNamespace;

// ======================================================================

void Direct3d11_VertexShaderData::install()
{
	DEBUG_FATAL(ms_memoryBlockManager, ("Direct3d11_VertexShaderData already installed"));
	ms_memoryBlockManager = new MemoryBlockManager(
		"Direct3d11_VertexShaderData", true,
		sizeof(Direct3d11_VertexShaderData), 0, 0, 0);
}

// ----------------------------------------------------------------------

void Direct3d11_VertexShaderData::remove()
{
	delete ms_memoryBlockManager;
	ms_memoryBlockManager = nullptr;
}

// ----------------------------------------------------------------------

void *Direct3d11_VertexShaderData::operator new(size_t size)
{
	UNREF(size);
	NOT_NULL(ms_memoryBlockManager);
	DEBUG_FATAL(size != sizeof(Direct3d11_VertexShaderData), ("wrong new called"));
	return ms_memoryBlockManager->allocate();
}

// ----------------------------------------------------------------------

void Direct3d11_VertexShaderData::operator delete(void *memory)
{
	NOT_NULL(ms_memoryBlockManager);
	ms_memoryBlockManager->free(memory);
}

// ======================================================================

Direct3d11_VertexShaderData::Direct3d11_VertexShaderData(ShaderImplementationPassVertexShader const &vertexShader)
:	ShaderImplementationPassVertexShaderGraphicsData(),
	m_vertexShader(&vertexShader),
	m_d3dVS(),
	m_bytecode(),
	m_bytecodeHash(0),
	m_isAssembly(false)
{
	// Detect language flavor from the leading //hlsl or //asm token.
	bool ok = parseHeader(vertexShader.m_text, static_cast<size_t>(vertexShader.m_textLength), m_isAssembly);
	UNREF(ok);

	if (m_isAssembly)
	{
		// D3D9-era vertex shader assembly (vs_1_1 / vs_2_0). D3DCompile
		// does not accept assembly; reassembly via D3DAssemble is gone
		// from the modern SDK. Record the situation; m_d3dVS stays NULL
		// and Plan 11-06's draw dispatch will skip these passes (or
		// trigger a build-time error if an asset surfaces here in target
		// scenes). Per Plan 11-01 D-04a finding: the Tatooine + cantina
		// scenes did not surface FFP usage; assembly VS usage in those
		// scenes is also expected to be zero, but logging is kept loud
		// in case an asset surprises us.
		DEBUG_REPORT_LOG_PRINT(true,
			("Direct3d11_VertexShaderData: '%s' is //asm vs_*; no D3D11 compile path. NULL VS bound; Plan 11-06 will skip pass.\n",
			vertexShader.getFilename()));
		return;
	}

	compileOrLoad(vertexShader.m_text, static_cast<size_t>(vertexShader.m_textLength), vertexShader.getFilename());
}

// ----------------------------------------------------------------------

Direct3d11_VertexShaderData::~Direct3d11_VertexShaderData()
{
	m_vertexShader = nullptr;
	// ComPtr members release automatically.
}

// ----------------------------------------------------------------------

void Direct3d11_VertexShaderData::compileOrLoad(char const *sourceText, size_t sourceLen, char const *displayName)
{
	if (!sourceText || sourceLen == 0)
	{
		DEBUG_REPORT_LOG_PRINT(true,
			("Direct3d11_VertexShaderData: empty source for '%s'\n", displayName ? displayName : "<unnamed>"));
		return;
	}

	// Build defines: SV_POSITION macro injection per Pitfall 1. We do
	// NOT yet add textureCoordinateSet defines (Plan 11-06 input-layout
	// cache work will surface those if assets request them).
	//
	// Plan 11-07 Iter-3: also inject D3D11_PROFILE so the profile string
	// participates in the FNV-1a cache key built by hashSource. Changing
	// the profile (e.g. swapping vs_5_0 <-> vs_4_0_level_9_3 in a future
	// iteration) automatically invalidates any pre-existing .cso disk
	// cache entries that were keyed under the old profile -- avoids
	// silent profile-mismatch bugs from stale cached bytecode.
	std::vector<D3D_SHADER_MACRO> defines;
	defines.push_back({ "POSITION",      "SV_POSITION" });
	defines.push_back({ "D3D11",         "1" });
	defines.push_back({ "D3D11_PROFILE", kVertexShaderProfile });
	defines.push_back({ nullptr,    nullptr });   // terminator

	// Hash the source + defines -- include the trailing terminator entry
	// so that hashSource sees the full effective defines list.
	uint64_t const hash = Direct3d11_ShaderCache::hashSource(
		sourceText, sourceLen, defines.data());

	// D-03 hybrid: try cache first.
	ComPtr<ID3DBlob> blob;
	if (!Direct3d11_ShaderCache::tryLoad(hash, blob))
	{
		// Cache miss -- compile via D3DCompile.
		ComPtr<ID3DBlob> errors;

		UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
		flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

		// Use a virtual filename of the asset's filename so D3DCompile's
		// error messages point at something useful.
		char const *virtName = (displayName && *displayName) ? displayName : "vertex_shader.vsh";

		// Plan 11-07 Iter-2: route `#include "..."` directives through
		// TreeFile so TRE-archived `.inc` headers resolve. Previously
		// passed D3D_COMPILE_STANDARD_FILE_INCLUDE (default Win32 FS
		// handler), which caused X1507 errors for engine `.vsh` sources
		// that include `vertex_program/include/vertex_shader_constants.inc`
		// and similar TRE-resident headers.
		//
		// Plan 11-07 Iter-3: profile string moved from "vs_5_0" to
		// kVertexShaderProfile (currently "vs_4_0_level_9_3"). See file
		// preamble for rationale -- engine .vsh sources use D3D9-era
		// HLSL syntax (e.g. `point` as a sampler-state filter literal)
		// that vs_5_0 rejects with X3000 syntax errors but
		// vs_4_0_level_9_* accepts. D3D11's CreateVertexShader consumes
		// the resulting bytecode unchanged.
		HRESULT hr = D3DCompile(
			sourceText,
			sourceLen,
			virtName,
			defines.data(),
			Direct3d11_CompileIncludeHandler::getInstance(),
			"main",
			kVertexShaderProfile,
			flags,
			0,
			blob.GetAddressOf(),
			errors.GetAddressOf());

		if (FAILED(hr))
		{
			char const *err = errors ? static_cast<char const *>(errors->GetBufferPointer()) : "no error blob";
			FATAL(true, ("Direct3d11_VertexShaderData: D3DCompile %s '%s' failed: %s",
				kVertexShaderProfile, virtName, err));
		}
		if (errors && errors->GetBufferSize() > 0)
		{
			DEBUG_REPORT_LOG_PRINT(true,
				("Direct3d11_VertexShaderData: '%s' warnings:\n%s\n",
				virtName, static_cast<char const *>(errors->GetBufferPointer())));
		}

		Direct3d11_ShaderCache::store(hash, blob.Get());
	}

	m_bytecode     = blob;
	m_bytecodeHash = hash;

	// Create the ID3D11VertexShader from the bytecode blob.
	HRESULT const hr = Direct3d11_Device::getDevice()->CreateVertexShader(
		blob->GetBufferPointer(),
		blob->GetBufferSize(),
		nullptr,
		m_d3dVS.GetAddressOf());
	FATAL_DX_HR("Direct3d11_VertexShaderData::CreateVertexShader failed: %s", hr);
}

// ======================================================================
