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
#include "Direct3d11_HlslRewrite.h"
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
Microsoft::WRL::ComPtr<ID3D11PixelShader> Direct3d11_PixelShaderProgramData::ms_fallbackPS;
Microsoft::WRL::ComPtr<ID3D11PixelShader> Direct3d11_PixelShaderProgramData::ms_fallbackPSTextured;

// ======================================================================

namespace Direct3d11_PixelShaderProgramDataNamespace
{
	// Plan 11-09.6: ps_4_0 profile string. Mirrors the VS compile-site
	// profile bump in Direct3d11_VertexShaderData.cpp (vs_4_0_level_9_3 ->
	// vs_4_0). See that file's preamble Profile-rationale block for the
	// full reasoning -- vs_5_0/ps_5_0 reserves D3D9-era HLSL identifiers
	// as keywords (still avoided here); the level_9 profile bucket
	// enforces SM2.x-style limits including the 256-instruction-slot
	// ceiling (busted by vertex_program/a_specmap_bump_vs20_for_ps20.vsh
	// on Plan 11-09.5 char-create smoke; X5615). Plain ps_4_0 drops the
	// cap while keeping legacy-syntax compatibility. CODEX pre-Plan-11-09.6
	// consult endorsed this over ps_5_0. This helper remains [[maybe_unused]]
	// today for stock engine assets (engine ships pre-compiled D3D9 PEXE
	// bytecode rather than HLSL source for pixel shaders -- see header
	// preamble); the fallback magenta PS path DOES execute through here
	// (Plan 11-09 Iter-2), so the profile bump is exercised at install
	// time on every D3D11 session.
	char const * const kPixelShaderProfile = "ps_4_0";

	// SPEC R3 compile-time proof: this helper exercises the ps_4_0
	// (Plan 11-09.6) + D3DCompile + ShaderCache code path. It is invoked
	// at install time for the magenta fallback PS (Plan 11-09 Iter-2) and
	// when an HLSL source pixel shader surfaces in the asset pipeline (none
	// today; future Phase 12 asset re-author will trigger that path).
	//
	// Returns nullptr if compile fails or input empty; otherwise an
	// ID3D11PixelShader owned by `outComPtr`.
	//
	// Plan 11-09 Iter-2 first real caller: install-time fallback PS compile.
	// [[maybe_unused]] dropped now that ms_fallbackPS routes through here.
	bool compilePixelShaderFromHlsl(
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
		//
		// Plan 11-07 Iter-4: D3D11_REWRITE_VERSION macro added (mirrors
		// the VS compile path). Rides along with hashSource so changes
		// to the rewrite rules in Direct3d11_HlslRewrite.cpp auto-
		// invalidate stale .cso cache entries when the version bumps.
		//
		// Plan 11-07 Iter-7: version bumped 3 -> 4 to ride the VS-side
		// rule extension (Rules B/C for `: c<N>` / `: register(c<N>)`)
		// and the main-source rewrite call site added below. See
		// Direct3d11_VertexShaderData.cpp's Iter-7 block for the full
		// rationale; the PS helper mirrors all VS-side changes for
		// symmetry + future Phase 12 HLSL-PS asset compatibility.
		//
		// Plan 11-07 Iter-8: version bumped 4 -> 5 to ride the VS-side
		// Rules B/C register-type-letter set expansion from `c` only to
		// the full `[bcstu]` set (b/cbuffer, c/constant, s/sampler,
		// t/texture, u/UAV). See Direct3d11_VertexShaderData.cpp's
		// Iter-8 block for the full rationale.
		//
		// Plan 11-07 Iter-9: version bumped 5 -> 6 to ride the VS-side
		// Rules B/C register-type-letter set expansion `[bcstu]` ->
		// `[bcstuv]`. The Iter-8 THROWAWAY diagnostic dump (reverted in
		// Iter-9's first commit) revealed `2d.vsh:8` uses `register(v0)`
		// -- vertex input stream register. See
		// Direct3d11_VertexShaderData.cpp's Iter-9 block for the full
		// rationale.
		//
		// Plan 11-07 Iter-10: version bumped 6 -> 7 to ride the VS-side
		// THROWAWAY diagnostic dump pair (reverted in Iter-11's first
		// commit; see Direct3d11_VertexShaderData.cpp for the trimmed
		// past-tense reference).
		//
		// Plan 11-07 Iter-11: version bumped 8 -> 9 to ride the VS-side
		// context-aware Rules B/C revision. See
		// Direct3d11_VertexShaderData.cpp's Iter-11 block for the full
		// rationale (over-strip diagnosis + `sawColonThisLine` fix).
		//
		// Plan 11-07 Iter-12: version bumped 9 -> 10 to ride the VS-side
		// PURE DIAGNOSTIC THROWAWAY iteration (reverted in Iter-13's
		// first commit; see Direct3d11_VertexShaderData.cpp for the
		// trimmed past-tense reference).
		//
		// Plan 11-07 Iter-13: version bumped 10 -> 11 (revert commit) then
		// 11 -> 12 (this commit -- Rule D cbuffer-wrap). See
		// Direct3d11_VertexShaderData.cpp's Iter-13 block for the full
		// rationale; the PS helper rides the version bump in lockstep
		// even though Rule D only fires on include content seen during
		// VS compiles (the PS compile path uses the same rewrite utility
		// + cache key, so the bump must match).
		std::vector<D3D_SHADER_MACRO> defines;
		defines.push_back({ "POSITION",               "SV_POSITION" });
		defines.push_back({ "D3D11",                  "1" });
		defines.push_back({ "D3D11_PROFILE",          kPixelShaderProfile });
		defines.push_back({ "D3D11_REWRITE_VERSION",  "15" });   // Plan 11-09.6 Iter-1: ps_4_0_level_9_3 -> ps_4_0 target bump (VS-site mirror).
		defines.push_back({ nullptr,                  nullptr });

		uint64_t const hash = Direct3d11_ShaderCache::hashSource(
			sourceText, sourceLen, defines.data());

		ComPtr<ID3DBlob> blob;
		if (!Direct3d11_ShaderCache::tryLoad(hash, blob))
		{
			// Plan 11-07 Iter-7: apply the HLSL textual rewrite to the
			// MAIN shader source (mirrors the VS-side Iter-7 addition in
			// Direct3d11_VertexShaderData.cpp::compileOrLoad). The
			// engine ships pre-compiled D3D9 PEXE bytecode for pixel
			// shaders today (Plan 11-05 finding), so this helper
			// remains [[maybe_unused]]; the rewrite call is added for
			// symmetry with the VS path so when a future Phase 12 asset
			// re-author surfaces HLSL-source pixel shaders, the rule
			// coverage is identical.
			std::vector<char> rewrittenSource;
			Direct3d11_HlslRewrite::applyToMainSource(
				sourceText, sourceLen, rewrittenSource, displayName);

			ComPtr<ID3DBlob> errors;
			// Plan 11-07 Iter-6: D3DCOMPILE_ENABLE_STRICTNESS removed.
			// Mirrors the VS-compile-site Iter-6 fix in
			// Direct3d11_VertexShaderData.cpp. The Iter-5 addition of
			// ENABLE_BACKWARDS_COMPATIBILITY here was mutually exclusive
			// with the Plan 11-05 baseline ENABLE_STRICTNESS, producing
			// X3116. Drop STRICTNESS (compat mode is by-design the opposite
			// stance). See VS compile site for the full meta-lesson.
			UINT flags = 0;
#ifdef _DEBUG
			flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
			flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
			// Plan 11-07 Iter-5: mirror the VS-compile-site addition of
			// D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY (0x1000). The flag
			// enables D3D9-era HLSL syntax (e.g. legacy struct-member
			// register-binding shortcut semantics) to compile under SM4+
			// targets. See Direct3d11_VertexShaderData.cpp's matching
			// block for the full Iter-5 rationale + the X3202
			// crash-dump-driven motivation. This PS helper remains
			// [[maybe_unused]] until a future Phase 12 asset re-author
			// surfaces HLSL-source pixel shaders; the flag is added
			// for symmetry + correctness when that code path activates.
			flags |= D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY;

			// Plan 11-08 Iter-1.5 retrofit (CODEX Q2 mandate; PS-site
			// mirror of VS site): force row-major matrix packing in
			// compiled HLSL cbuffer storage. See Direct3d11_VertexShaderData.cpp
			// matching block for the full Iter-1.5 narrative. Flag value
			// (1 << 3 = 0x8) does NOT bit-collide with the previously-
			// dropped STRICTNESS (1 << 11 = 0x800). Pairs with the
			// D3D11_REWRITE_VERSION 13 -> 14 bump above for shader-cache
			// invalidation. The PS helper remains [[maybe_unused]] today
			// (engine ships pre-compiled D3D9 PEXE bytecode); the retrofit
			// lands here for symmetry so a future Phase 12 asset
			// re-author starts on a row-major-correct foundation.
			flags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

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
				rewrittenSource.data(),
				rewrittenSource.size(),
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

	// Plan 11-09 Iter-2: compile the magenta fallback PS so applyPreDrawState
	// has something to bind when an asset's PS is null (Plan 11-05 PEXE
	// caveat -- D3D11 CreatePixelShader rejects D3D9-era bytecode, so
	// stock assets ship with m_d3dPS = null). Output color float4(1,0,1,1)
	// is intentional debug magenta -- a visible "Phase 12 asset re-author
	// owes us a real PS" signal that still surfaces geometry rather than
	// rasterizing to no fragment writes.
	//
	// Variant M (magenta) PS signature is the absolute minimum: float4
	// pos : SV_POSITION input (D3D11 spec mandates SV_POSITION is always
	// available from the rasterizer; PS input MUST be a subset of VS
	// output, so declaring only SV_POSITION is safe regardless of what
	// the engine's VS outputs). Output is a single SV_TARGET float4.
	//
	// Plan 11-09.13 Iter-1 attempted a single textured-passthrough body
	// adding `float2 uv : TEXCOORD0` PS input + `t.Sample(s, uv)`, hoping
	// to lift the world from solid magenta to recognizable textures
	// without requiring Phase 12 asset re-author. Result: 483,451 D3D11
	// debug-layer id=342 ERRORs ("VS-PS linkage error: PS input requires
	// Semantic/Index (TEXCOORD,0) but VS output stage doesn't provide
	// it") in one smoke session = every draw rejected by D3D11.
	// Iter-2 reverted to magenta as the safe known-good.
	//
	// Plan 11-09.13 Iter-3 (this state): keep Variant M as the universal
	// fallback and compile a Variant T (textured-passthrough) alongside.
	// applyPreDrawState selects between them by reflecting the bound VS's
	// output signature (Direct3d11_VertexShaderData::m_reflectedOutputs)
	// and picking Variant T only when the VS provides TEXCOORD0 with
	// FLOAT32 component type at xy mask coverage -- the D3D11 stage-
	// linkage compatibility floor for `float2 uv : TEXCOORD0` PS input.
	// CODEX-endorsed Bucket-1 architecture; full consult at
	// 11-09.13-CODEX-CONSULT-reflection-driven-fallback-variant-selection.md.
	//
	// CODEX-flagged stale premise: Direct3d11_StaticShaderData::apply()
	// does NOT yet bind per-pass diffuse textures (cpp:208-248 explicitly
	// defers material/texture/sampler binding to Iter-3+). Variant T
	// samples whatever SRV slot 0 holds from earlier engine state --
	// possibly null, possibly stale. StateCache's selection path emits
	// a one-shot SRV0/sampler0 diagnostic the first time Variant T is
	// selected per session, surfacing this gap as telemetry rather than
	// hiding it behind a magenta world. SRV0 binding fix itself is
	// scoped to Iter-4 / Plan 11-09.14.
	char const kFallbackMagentaHlsl[] =
		"// Plan 11-09 Iter-2 / 11-09.13 Iter-3 Variant M magenta fallback PS.\n"
		"// Universal -- only requires SV_POSITION (always provided by\n"
		"// the rasterizer). Bound when the active VS does not output a\n"
		"// TEXCOORD0 float xy signature compatible with Variant T.\n"
		"float4 main(float4 pos : SV_POSITION) : SV_TARGET\n"
		"{\n"
		"    return float4(1.0f, 0.0f, 1.0f, 1.0f);\n"
		"}\n";

	bool const compiledMagenta = compilePixelShaderFromHlsl(
		kFallbackMagentaHlsl,
		sizeof(kFallbackMagentaHlsl) - 1,   // exclude trailing null
		"fallback_magenta.hlsl",
		ms_fallbackPS);

	FATAL(!compiledMagenta, ("Direct3d11_PixelShaderProgramData::install: Variant M magenta fallback PS compile returned false"));
	FATAL(!ms_fallbackPS,   ("Direct3d11_PixelShaderProgramData::install: Variant M compiled but ComPtr is empty"));

	// Plan 11-09.13 Iter-3 Variant T (textured-passthrough). Selected per-
	// draw only when reflection proves the bound VS outputs TEXCOORD0 with
	// FLOAT32 component type at xy mask coverage (see Direct3d11_StateCache
	// selectFallbackPSForVS). PS-input declaration matches Microsoft Learn
	// stage-linkage rules: SV_POSITION input + non-SV TEXCOORD0 input at
	// float2. SRV slot 0 + sampler slot 0 are read via t.Sample(s, uv);
	// pre-Iter-4 the SRV may not be bound for this draw (CODEX stale-
	// premise) -- StateCache emits a one-shot diagnostic on first
	// selection so the gap is observable.
	char const kFallbackTexturedHlsl[] =
		"// Plan 11-09.13 Iter-3 Variant T textured-passthrough fallback PS.\n"
		"// Selected when reflection proves bound VS outputs TEXCOORD0\n"
		"// (FLOAT32, xy mask). Samples whatever the engine bound at\n"
		"// SRV slot 0 / sampler slot 0; per-pass diffuse-texture binding\n"
		"// in Direct3d11_StaticShaderData::apply() is Iter-4 work.\n"
		"Texture2D    t : register(t0);\n"
		"SamplerState s : register(s0);\n"
		"float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET\n"
		"{\n"
		"    return t.Sample(s, uv);\n"
		"}\n";

	bool const compiledTextured = compilePixelShaderFromHlsl(
		kFallbackTexturedHlsl,
		sizeof(kFallbackTexturedHlsl) - 1,
		"fallback_textured.hlsl",
		ms_fallbackPSTextured);

	FATAL(!compiledTextured,    ("Direct3d11_PixelShaderProgramData::install: Variant T textured fallback PS compile returned false"));
	FATAL(!ms_fallbackPSTextured, ("Direct3d11_PixelShaderProgramData::install: Variant T compiled but ComPtr is empty"));
}

// ----------------------------------------------------------------------

void Direct3d11_PixelShaderProgramData::remove()
{
	ms_fallbackPS.Reset();
	ms_fallbackPSTextured.Reset();
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
