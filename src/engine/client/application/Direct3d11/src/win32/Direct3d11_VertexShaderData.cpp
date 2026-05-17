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
//   2. Build defines: inject POSITION -> SV_POSITION macro (Pitfall 1).
//   3. Hash (source text + defines) via Direct3d11_ShaderCache::hashSource.
//   4. tryLoad: on hit, skip D3DCompile; on miss, D3DCompile with vs_5_0
//      profile + same defines + D3D_COMPILE_STANDARD_FILE_INCLUDE, then store.
//   5. CreateVertexShader from the resulting blob; cache the bytecode hash
//      for Pitfall 6 input-layout cache use (Plan 11-06).
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
	std::vector<D3D_SHADER_MACRO> defines;
	defines.push_back({ "POSITION", "SV_POSITION" });
	defines.push_back({ "D3D11",    "1" });
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

		HRESULT hr = D3DCompile(
			sourceText,
			sourceLen,
			virtName,
			defines.data(),
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"main",
			"vs_5_0",
			flags,
			0,
			blob.GetAddressOf(),
			errors.GetAddressOf());

		if (FAILED(hr))
		{
			char const *err = errors ? static_cast<char const *>(errors->GetBufferPointer()) : "no error blob";
			FATAL(true, ("Direct3d11_VertexShaderData: D3DCompile vs_5_0 '%s' failed: %s",
				virtName, err));
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
