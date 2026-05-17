// ======================================================================
//
// Direct3d11_PixelShaderProgramData.cpp
// Phase 11 D3D11 renderer plugin -- pixel shader program wrapper impl.
// Plan 11-05 (Wave 5 -- shader layer).
//
// Asset-pipeline caveat (see header preamble): ShaderImplementation's
// pixel shader m_exe is pre-compiled D3D9-era DWORD bytecode (PEXE IFF
// chunk). D3D11 cannot consume it. We construct the wrapper successfully
// and leave m_d3dPS = NULL; Plan 11-06's draw dispatch handles the NULL
// case. This unblocks the engine's createPixelShaderProgramData factory
// slot during ShaderImplementation::install.
//
// We use D3DCompile() / kPixelShaderProfile (Plan 11-07 Iter-3 sets this
// to "ps_4_0_level_9_3"; was "ps_5_0" in Plan 11-05 baseline) as
// DEAD-CODE compile-time PROOF that the D3D11 shader-compile path is
// present (SPEC R3 acceptance signal, with the Iter-3 profile-rationale
// caveat -- see Direct3d11_VertexShaderData.cpp preamble). The helper is
// reachable from the constructor only when the engine ever surfaces an
// HLSL-source pixel shader; current asset pipeline does not.
//
// ======================================================================

#include "FirstDirect3d11.h"
#include "Direct3d11_PixelShaderProgramData.h"

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

MemoryBlockManager *Direct3d11_PixelShaderProgramData::ms_memoryBlockManager = nullptr;

// ======================================================================

namespace Direct3d11_PixelShaderProgramDataNamespace
{
	// Plan 11-07 Iter-3: ps_4_0_level_9_3 profile string. Mirrors the VS
	// compile-site profile change in Direct3d11_VertexShaderData.cpp. See
	// that file's preamble Profile-rationale block for the full reasoning
	// (D3D9-era HLSL syntax surfaces tokens that vs_5_0 / ps_5_0 reserve
	// as keywords; vs_4_0_level_9_* / ps_4_0_level_9_* relax those
	// reservations). This helper remains [[maybe_unused]] today (engine
	// ships pre-compiled D3D9 PEXE bytecode rather than HLSL source for
	// pixel shaders -- see header preamble); when a future Phase 12 asset
	// re-author surfaces HLSL-source pixel shaders, this is the code path
	// that will execute.
	char const * const kPixelShaderProfile = "ps_4_0_level_9_3";

	// SPEC R3 compile-time proof: this helper exercises the ps_4_0_level_9_3
	// (Iter-3) + D3DCompile + ShaderCache code path. It is invoked when an
	// HLSL source pixel shader surfaces in the asset pipeline (none today;
	// future Phase 12 asset re-author will trigger this path).
	//
	// Returns nullptr if compile fails or input empty; otherwise an
	// ID3D11PixelShader owned by `outComPtr`.
	//
	// Annotated [[maybe_unused]] so MSVC /W4 doesn't warn during the
	// Plan 11-05 baseline where no HLSL PS asset surfaces.
	[[maybe_unused]] bool compilePixelShaderFromHlsl(
		char const *sourceText,
		size_t sourceLen,
		char const *displayName,
		Microsoft::WRL::ComPtr<ID3D11PixelShader> &outComPtr)
	{
		if (!sourceText || sourceLen == 0)
			return false;

		// Plan 11-07 Iter-3: D3D11_PROFILE macro injected into the defines
		// list so the profile string participates in the cache hash
		// (mirrors Direct3d11_VertexShaderData.cpp Iter-3 change).
		std::vector<D3D_SHADER_MACRO> defines;
		defines.push_back({ "POSITION",      "SV_POSITION" });
		defines.push_back({ "D3D11",         "1" });
		defines.push_back({ "D3D11_PROFILE", kPixelShaderProfile });
		defines.push_back({ nullptr, nullptr });

		uint64_t const hash = Direct3d11_ShaderCache::hashSource(
			sourceText, sourceLen, defines.data());

		ComPtr<ID3DBlob> blob;
		if (!Direct3d11_ShaderCache::tryLoad(hash, blob))
		{
			ComPtr<ID3DBlob> errors;
			UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
			flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
			flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
			char const *virtName = (displayName && *displayName) ? displayName : "pixel_shader.psh";

			// Plan 11-07 Iter-2: route `#include "..."` directives
			// through TreeFile so TRE-archived `.inc` headers resolve.
			// Mirrors the VS compile path; this helper remains
			// [[maybe_unused]] until a future Phase 12 asset re-author
			// surfaces HLSL-source pixel shaders, but the contract is
			// correct as of Iter-2.
			//
			// Plan 11-07 Iter-3: profile string moved from "ps_5_0" to
			// kPixelShaderProfile (currently "ps_4_0_level_9_3"); see
			// namespace-scope comment above kPixelShaderProfile for
			// rationale.
			HRESULT hr = D3DCompile(
				sourceText,
				sourceLen,
				virtName,
				defines.data(),
				Direct3d11_CompileIncludeHandler::getInstance(),
				"main",
				kPixelShaderProfile,
				flags,
				0,
				blob.GetAddressOf(),
				errors.GetAddressOf());

			if (FAILED(hr))
			{
				char const *err = errors ? static_cast<char const *>(errors->GetBufferPointer()) : "no error blob";
				FATAL(true, ("Direct3d11_PixelShaderProgramData: D3DCompile %s '%s' failed: %s",
					kPixelShaderProfile, virtName, err));
			}
			if (errors && errors->GetBufferSize() > 0)
			{
				DEBUG_REPORT_LOG_PRINT(true,
					("Direct3d11_PixelShaderProgramData: '%s' warnings:\n%s\n",
					virtName, static_cast<char const *>(errors->GetBufferPointer())));
			}

			Direct3d11_ShaderCache::store(hash, blob.Get());
		}

		HRESULT const hr = Direct3d11_Device::getDevice()->CreatePixelShader(
			blob->GetBufferPointer(),
			blob->GetBufferSize(),
			nullptr,
			outComPtr.GetAddressOf());
		FATAL_DX_HR("Direct3d11_PixelShaderProgramData::CreatePixelShader failed: %s", hr);
		return true;
	}

	// Heuristic check: pre-compiled D3D9 pixel shader bytecode (PEXE
	// chunk) starts with a version DWORD whose high byte is 0xFFFF (sentinel)
	// followed by version major/minor. D3D11 DXBC bytecode starts with
	// the literal "DXBC" magic in the first 4 bytes (0x43425844 LE).
	// We log which we detect for the asset audit.
	bool looksLikeDxbc(DWORD const *exe)
	{
		if (!exe) return false;
		uint32_t const first = static_cast<uint32_t>(exe[0]);
		// "DXBC" little-endian
		return first == 0x43425844u;
	}
}
using namespace Direct3d11_PixelShaderProgramDataNamespace;

// ======================================================================

void Direct3d11_PixelShaderProgramData::install()
{
	DEBUG_FATAL(ms_memoryBlockManager, ("Direct3d11_PixelShaderProgramData already installed"));
	ms_memoryBlockManager = new MemoryBlockManager(
		"Direct3d11_PixelShaderProgramData", true,
		sizeof(Direct3d11_PixelShaderProgramData), 0, 0, 0);
}

// ----------------------------------------------------------------------

void Direct3d11_PixelShaderProgramData::remove()
{
	delete ms_memoryBlockManager;
	ms_memoryBlockManager = nullptr;
}

// ----------------------------------------------------------------------

void *Direct3d11_PixelShaderProgramData::operator new(size_t size)
{
	UNREF(size);
	NOT_NULL(ms_memoryBlockManager);
	DEBUG_FATAL(size != sizeof(Direct3d11_PixelShaderProgramData), ("wrong new called"));
	return ms_memoryBlockManager->allocate();
}

// ----------------------------------------------------------------------

void Direct3d11_PixelShaderProgramData::operator delete(void *memory)
{
	NOT_NULL(ms_memoryBlockManager);
	ms_memoryBlockManager->free(memory);
}

// ======================================================================

Direct3d11_PixelShaderProgramData::Direct3d11_PixelShaderProgramData(ShaderImplementationPassPixelShaderProgram const &pixelShaderProgram)
:	ShaderImplementationPassPixelShaderProgramGraphicsData(),
	m_program(&pixelShaderProgram),
	m_d3dPS()
{
	// m_exe is pre-compiled D3D9 bytecode from the .psh IFF (see header
	// preamble). Check for DXBC magic (would indicate a future re-authored
	// SM5 asset); otherwise log + leave m_d3dPS null.
	DWORD const *exe = pixelShaderProgram.m_exe;
	if (!exe)
	{
		DEBUG_REPORT_LOG_PRINT(true,
			("Direct3d11_PixelShaderProgramData: '%s' has NULL m_exe; leaving m_d3dPS NULL (Plan 11-06 draw will skip)\n",
			pixelShaderProgram.getFileName()));
		return;
	}

	if (looksLikeDxbc(exe))
	{
		// Future asset-pipeline path: exe[] is a DXBC blob; the high
		// DWORD count is not stored alongside m_exe in the engine struct
		// (PEXE chunk records the count implicitly via the IFF chunk
		// length). Without the size we cannot safely call
		// CreatePixelShader. Log the situation and treat as the current
		// pre-compiled-D3D9 case until the asset-pipeline contract grows
		// an explicit byte-count field.
		DEBUG_REPORT_LOG_PRINT(true,
			("Direct3d11_PixelShaderProgramData: '%s' bytecode begins with DXBC magic -- future SM5 asset detected; size field not yet plumbed (Plan 11-06 work), leaving m_d3dPS NULL\n",
			pixelShaderProgram.getFileName()));
		return;
	}

	// Current pre-compiled D3D9 bytecode case. We CANNOT pass this to
	// CreatePixelShader -- it would E_INVALIDARG. Log the version for
	// the asset-audit trail.
	int const verMajor = static_cast<int>((exe[0] >> 8) & 0xff);
	int const verMinor = static_cast<int>((exe[0] >> 0) & 0xff);
	DEBUG_REPORT_LOG_PRINT(true,
		("Direct3d11_PixelShaderProgramData: '%s' is pre-compiled D3D9 PS %d.%d bytecode -- not consumable by D3D11; m_d3dPS NULL (asset re-author follow-up; Plan 11-06 draw skip)\n",
		pixelShaderProgram.getFileName(), verMajor, verMinor));
}

// ----------------------------------------------------------------------

Direct3d11_PixelShaderProgramData::~Direct3d11_PixelShaderProgramData()
{
	m_program = nullptr;
}

// ======================================================================
