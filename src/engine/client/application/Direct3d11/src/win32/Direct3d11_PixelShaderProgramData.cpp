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
#include "Direct3d11_VertexShaderData.h"   // Iter-4: getBytecodeHash / getReflectedOutputs

#include "sharedFoundation/MemoryBlockManager.h"

#include <algorithm>      // Iter-4: std::sort
#include <cstdio>         // Iter-4: snprintf
#include <cstring>
#include <string>         // Iter-4: HLSL generator builds std::string source
#include <vector>
#include <d3d11.h>
#include <d3d11sdklayers.h>   // Iter-4: ID3D11InfoQueue::AddApplicationMessage
#include <d3dcompiler.h>
#include <wrl/client.h>

// ======================================================================

using Microsoft::WRL::ComPtr;

MemoryBlockManager *Direct3d11_PixelShaderProgramData::ms_memoryBlockManager = nullptr;
Microsoft::WRL::ComPtr<ID3D11PixelShader> Direct3d11_PixelShaderProgramData::ms_fallbackPS;
std::unordered_map<uint64_t, Microsoft::WRL::ComPtr<ID3D11PixelShader>> Direct3d11_PixelShaderProgramData::ms_perVSCache;

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

	// ------------------------------------------------------------------
	// Plan 11-09.13 Iter-4 helpers: per-VS PS HLSL generator + first-
	// emit diagnostic.

	// Map (D3D_REGISTER_COMPONENT_TYPE, ComponentMask) -> HLSL scalar/
	// vector type. Mask bit count determines vector arity (1..4). Defaults
	// to float4 for unrecognized component types.
	std::string hlslTypeFor(D3D_REGISTER_COMPONENT_TYPE ct, UINT mask)
	{
		int components = 0;
		if (mask & 0x1) ++components;
		if (mask & 0x2) ++components;
		if (mask & 0x4) ++components;
		if (mask & 0x8) ++components;
		if (components == 0) components = 4;   // defensive: unknown -> widest

		char const *prefix = "float";
		switch (ct)
		{
			case D3D_REGISTER_COMPONENT_FLOAT32: prefix = "float"; break;
			case D3D_REGISTER_COMPONENT_SINT32:  prefix = "int";   break;
			case D3D_REGISTER_COMPONENT_UINT32:  prefix = "uint";  break;
			default:                             prefix = "float"; break;
		}

		std::string s = prefix;
		if (components > 1)
			s += static_cast<char>('0' + components);   // "float2", "float3", "float4"
		return s;
	}

	// Validate that a reflected SemanticName is a strict HLSL identifier
	// (letter / underscore prefix; alphanumeric / underscore body). Any
	// failure aborts generator output -> getOrCompilePSForVS tombstones
	// the VS and the caller falls back to Variant M magenta.
	bool isValidHlslIdentifier(char const *name)
	{
		if (!name || !*name)
			return false;
		auto isAlpha = [](char c) {
			return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
		};
		auto isAlnum = [&](char c) {
			return isAlpha(c) || (c >= '0' && c <= '9');
		};
		if (!isAlpha(name[0]))
			return false;
		for (char const *p = name + 1; *p; ++p)
		{
			if (!isAlnum(*p))
				return false;
		}
		return true;
	}

	// Build per-VS PS HLSL source from reflected outputs. Walks outputs
	// sorted by Register so PS-input declaration order matches the VS
	// output HW register layout exactly (HLSL compiler assigns PS input
	// registers in declaration order -> v0==o0, v1==o1, ...). Body
	// samples SRV0/sampler0 via the matching TEXCOORD0 declaration when
	// the VS provides TEXCOORD0 + FLOAT32 + at least xy; otherwise
	// returns magenta. Returns empty string on validation failure
	// (caller treats as tombstone).
	std::string buildHlslForVSOutputs(std::vector<Direct3d11_ReflectedVSOutput> const &outputsIn)
	{
		// Defensive: validate semantic names BEFORE we start emitting.
		for (Direct3d11_ReflectedVSOutput const &o : outputsIn)
		{
			if (!isValidHlslIdentifier(o.SemanticName))
				return std::string();
		}

		// Sort by Register; ties broken by SemanticIndex for determinism.
		std::vector<Direct3d11_ReflectedVSOutput> outputs = outputsIn;
		std::sort(outputs.begin(), outputs.end(),
			[](Direct3d11_ReflectedVSOutput const &a, Direct3d11_ReflectedVSOutput const &b) {
				if (a.Register != b.Register) return a.Register < b.Register;
				return a.SemanticIndex < b.SemanticIndex;
			});

		// Find TEXCOORD0 / FLOAT32 / xy declared -- decides whether the
		// body samples or returns magenta. ComponentMask (declared
		// signature contract) gates this, not ReadWriteMask (runtime
		// write behavior).
		int texcoord0Index = -1;
		for (size_t i = 0; i < outputs.size(); ++i)
		{
			Direct3d11_ReflectedVSOutput const &o = outputs[i];
			if (_stricmp(o.SemanticName, "TEXCOORD") != 0) continue;
			if (o.SemanticIndex != 0) continue;
			if (o.ComponentType != D3D_REGISTER_COMPONENT_FLOAT32) continue;
			if ((o.ComponentMask & 0x3) != 0x3) continue;
			texcoord0Index = static_cast<int>(i);
			break;
		}

		std::string hlsl;
		hlsl.reserve(1024);

		hlsl += "// Plan 11-09.13 Iter-4 per-VS dynamic PS (Bucket 2).\n";
		hlsl += "// Auto-generated to mirror VS output struct register-for-register.\n";

		if (texcoord0Index >= 0)
		{
			hlsl += "Texture2D    t : register(t0);\n";
			hlsl += "SamplerState s : register(s0);\n";
		}

		hlsl += "struct PSIn\n{\n";
		hlsl += "    float4 _pos : SV_POSITION;\n";

		for (size_t i = 0; i < outputs.size(); ++i)
		{
			Direct3d11_ReflectedVSOutput const &o = outputs[i];
			std::string type = hlslTypeFor(o.ComponentType, o.ComponentMask);
			char field[32];
			snprintf(field, sizeof(field), "_v%zu", i);

			char idxStr[16];
			snprintf(idxStr, sizeof(idxStr), "%u", o.SemanticIndex);

			hlsl += "    ";
			hlsl += type;
			hlsl += " ";
			hlsl += field;
			hlsl += " : ";
			hlsl += o.SemanticName;
			hlsl += idxStr;
			hlsl += ";\n";
		}
		hlsl += "};\n\n";

		hlsl += "float4 main(PSIn input) : SV_TARGET\n{\n";
		if (texcoord0Index >= 0)
		{
			// Plan 11-09.15 Iter-14 DIAGNOSTIC (Step A from CODEX + Cursor
			// converged consult on PS-texture-sample-returns-zero):
			// visualize the input TEXCOORD0 as color (R=u, G=v, B=0, A=1).
			// CODEX + Cursor both rule out H6 (PS register linkage drift)
			// based on FXC analysis of the generated HLSL -- the prepended
			// SV_POSITION at PSIn register 0 forces the auto-allocator to
			// place TEXCOORD0 at v1, matching the VS output at o1. Both
			// pivot to H3/H5/H7 (texture GPU content is zero / wrong / not
			// uploaded). This UV viz pass either:
			//   * Shows a red-to-yellow-to-green gradient across the splash
			//     -> UVs reach the PS correctly -> texture content really
			//     is zero -> Iter-15 adds GPU staging readback to confirm
			//     and traces Direct3d11_TextureData::lock/unlock for the
			//     splash format (likely TF_RGB_888 lock-bridge path).
			//   * Shows solid black -> UVs are zeroed at PS input despite
			//     clean linkage -> input layout / phantom element path
			//     (lower probability per both consults).
			// REVERT once root cause is fixed.
			char field[32];
			snprintf(field, sizeof(field), "_v%d", texcoord0Index);
			hlsl += "    return float4(input.";
			hlsl += field;
			hlsl += ".xy, 0.0f, 1.0f);\n";
		}
		else
		{
			hlsl += "    return float4(1.0f, 0.0f, 1.0f, 1.0f);\n";
		}
		hlsl += "}\n";

		return hlsl;
	}

	// First-time-the-generator-runs diagnostic. Routes via the
	// ID3D11InfoQueue file sink (d3d11-debug.log) so the message is
	// observable from explorer-launched smokes; the
	// DEBUG_REPORT_LOG_PRINT side-channel goes to OutputDebugString +
	// stdout (invisible under explorer launch per Direct3d11_Device.cpp:91
	// preamble). One-shot per process. Iter-3 dropped the equivalent
	// SRV0 diagnostic into a sink that was invisible during smoke;
	// Iter-4 fixes that.
	void emitFirstGeneratorLog(uint64_t bytecodeHash, std::string const &hlsl)
	{
		static bool s_emitted = false;
		if (s_emitted) return;
		s_emitted = true;

		char header[256];
		snprintf(header, sizeof(header),
			"Plan 11-09.13 Iter-4 first dynamic-PS generated HLSL (VS bytecode hash=0x%016llX, %zu bytes):",
			static_cast<unsigned long long>(bytecodeHash),
			hlsl.size());

		if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
		{
			iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, header);
			iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, hlsl.c_str());
		}
		DEBUG_REPORT_LOG_PRINT(true, ("%s\n%s\n", header, hlsl.c_str()));
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
	// Plan 11-09.13 Iter-3 added a static Variant T (textured-passthrough
	// PS) compiled at install time, selected by reflection when the
	// bound VS's output signature contained TEXCOORD0 + FLOAT32 + xy
	// mask. Smoke result: 0 id=342 (hard bar passed) but 65,034 id=343
	// "Semantic 'TEXCOORD' is defined for mismatched hardware registers
	// between the output stage and input stage." D3D11 stage linkage
	// matches by semantic name AND hardware register position; Variant T
	// declared TEXCOORD0 at register v0 but most engine VSes output
	// TEXCOORD0 at register o1+/o2+ (after BLEND/COLOR/NORMAL semantics
	// in their output struct). Bucket 1 (Compile-N-pick-1) was therefore
	// register-position-limited for SWG's VS population.
	//
	// Plan 11-09.13 Iter-4 (this state): override CODEX's "Bucket 2 is
	// too much for this phase" guidance with Bucket 2 (per-VS dynamic
	// PS compile). At first encounter of each VS, generate a PS whose
	// input struct mirrors the VS's reflected output struct register-
	// for-register; compile via D3DCompile; cache by VS bytecode hash.
	// See getOrCompilePSForVS + buildHlslForVSOutputs. Variant T (Iter-3
	// static) is gone -- the register-position mismatch is structurally
	// resolved by construction.
	//
	// CODEX-flagged stale premise persists: Direct3d11_StaticShaderData::
	// apply() (cpp:208-248) still does NOT bind per-pass diffuse
	// textures. Linkage-correct dynamic PSes that sample SRV0 will read
	// garbage / zero until per-pass texture binding lands (Plan 11-09.14
	// or Phase 12). StateCache's first-dynamic-PS-bind SRV0/sampler0
	// diagnostic surfaces the gap; Iter-3's id=353 INFO lines next to
	// each id=343 confirmed SRV0 = null when textured PS was bound.
	char const kFallbackMagentaHlsl[] =
		"// Plan 11-09 Iter-2 / 11-09.13 Iter-4 magenta safety-net PS.\n"
		"// Universal -- only requires SV_POSITION (always provided by\n"
		"// the rasterizer). Bound by applyPreDrawState only when the\n"
		"// per-VS dynamic compile path returns null (compile-failure\n"
		"// tombstone or empty reflection data).\n"
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

	// Plan 11-09.13 Iter-4: per-VS dynamic PS variants are NOT compiled
	// at install. They are generated lazily per VS bytecode hash by
	// getOrCompilePSForVS at first encounter (first draw using each VS).
	// Variant T (Iter-3 static textured-passthrough) is gone -- the
	// register-position mismatch problem it caused (65K id=343 errors
	// per session) is structurally resolved by the per-VS approach,
	// which generates a PS whose input struct mirrors the VS's reflected
	// output struct register-for-register.
}

// ----------------------------------------------------------------------

void Direct3d11_PixelShaderProgramData::remove()
{
	ms_fallbackPS.Reset();
	ms_perVSCache.clear();              // Iter-4: drop all per-VS dynamic PSes
	delete ms_memoryBlockManager;
	ms_memoryBlockManager = nullptr;
}

// ----------------------------------------------------------------------

ID3D11PixelShader *Direct3d11_PixelShaderProgramData::getOrCompilePSForVS(Direct3d11_VertexShaderData const *vsData)
{
	// Plan 11-09.13 Iter-4: lazy per-VS PS generation + cache. Keyed by
	// VS bytecode hash so identical bytecode shares a PS. Cache miss
	// triggers HLSL generation from reflected outputs + D3DCompile;
	// compile failure tombstones the entry (null ComPtr stored) so
	// subsequent calls hit the cache without retrying. Caller (StateCache
	// selectFallbackPSForVS) falls back to ms_fallbackPS on null return.
	if (!vsData)
		return nullptr;

	uint64_t const hash = vsData->getBytecodeHash();
	auto it = ms_perVSCache.find(hash);
	if (it != ms_perVSCache.end())
		return it->second.Get();   // hit (may be null tombstone)

	std::vector<Direct3d11_ReflectedVSOutput> const &outputs = vsData->getReflectedOutputs();

	// No outputs (reflection failed or VS is degenerate) -> tombstone.
	// Empty PS generator output (validation failure) -> tombstone.
	// Compile failure -> tombstone. All paths cache-miss-then-tombstone
	// to avoid retry storms across thousands of draws using the same VS.
	std::string const hlsl = buildHlslForVSOutputs(outputs);
	if (hlsl.empty())
	{
		ms_perVSCache[hash] = ComPtr<ID3D11PixelShader>();
		return nullptr;
	}

	emitFirstGeneratorLog(hash, hlsl);

	char displayName[64];
	snprintf(displayName, sizeof(displayName), "perVS_dyn_%016llX.hlsl",
	         static_cast<unsigned long long>(hash));

	ComPtr<ID3D11PixelShader> ps;
	bool const compiled = compilePixelShaderFromHlsl(
		hlsl.c_str(), hlsl.size(), displayName, ps);

	if (!compiled || !ps)
	{
		ms_perVSCache[hash] = ComPtr<ID3D11PixelShader>();
		return nullptr;
	}

	ms_perVSCache[hash] = ps;
	return ps.Get();
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
