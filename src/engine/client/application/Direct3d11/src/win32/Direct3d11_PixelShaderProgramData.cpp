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
		defines.push_back({ "D3D11_REWRITE_VERSION",  "21" });   // Plan 17-02 Task 1 SUB-STEP 1d: bump 20 -> 21 to invalidate stale .cso caches for the new non-fatal PSRC-recompile lane (D3DCompile sourceLen now strlen-derived per R3-02g instead of m_psrcLen-with-trailing-NUL; a stale .cso entry produced under the prior cache contract could otherwise mask a regression). Prior Iter-29B note (Plan 11-09.15) preserved in version history.
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

	// ------------------------------------------------------------------
	// Plan 17-02 HIGH-1 + HIGH-2 sibling helper: NON-FATAL recompile for
	// asset PSRC source. Mirrors compilePixelShaderFromHlsl byte-for-byte
	// EXCEPT:
	//   (i)  D3DCompile FAILED -> log + return false (no FATAL); outBlob
	//        left empty; outComPtr left empty.
	//   (ii) CreatePixelShader FAILED -> log + return false (no FATAL_DX_HR).
	//   (iii) On success, outBytecodeBlob is assigned the compiled DXBC blob
	//        so the caller can retain it for downstream reflection (Plan 03
	//        consumes the cache populated by the ctor reflect-once block).
	//
	// The original compilePixelShaderFromHlsl is PRESERVED unchanged for
	// the install-time magenta fallback PS path (Direct3d11_PixelShaderProgramData::
	// install at :616) where a compile failure IS a developer bug worth
	// crashing on (HIGH-1 rationale). Only the asset-PS ctor uses this
	// non-fatal sibling.
	//
	// Inherits the same define list + flag set + cache contract -- the
	// only behavioral diffs are the two FATAL replacements and the blob
	// retention. Cache key parity is preserved (same hashSource inputs).
	bool tryCompilePixelShaderFromHlslNoFatal(
		char const *sourceText,
		size_t sourceLen,
		char const *displayName,
		Microsoft::WRL::ComPtr<ID3D11PixelShader> &outComPtr,
		Microsoft::WRL::ComPtr<ID3DBlob>          &outBytecodeBlob)
	{
		if (!sourceText || sourceLen == 0)
			return false;

		// Mirror the FATAL helper's define list verbatim so cache keys agree
		// for identical (source, define) inputs. The D3D11_REWRITE_VERSION
		// bump landed in the FATAL helper above (sub-step 1d); we propagate
		// the same value here for cache coherence.
		std::vector<D3D_SHADER_MACRO> defines;
		defines.push_back({ "POSITION",               "SV_POSITION" });
		defines.push_back({ "D3D11",                  "1" });
		defines.push_back({ "D3D11_PROFILE",          kPixelShaderProfile });
		defines.push_back({ "D3D11_REWRITE_VERSION",  "21" });  // Plan 17-02: must match the FATAL helper (above) so the .cso cache hash is shared
		defines.push_back({ nullptr,                  nullptr });

		uint64_t const hash = Direct3d11_ShaderCache::hashSource(
			sourceText, sourceLen, defines.data());

		ComPtr<ID3DBlob> blob;
		if (!Direct3d11_ShaderCache::tryLoad(hash, blob))
		{
			std::vector<char> rewrittenSource;
			Direct3d11_HlslRewrite::applyToMainSource(
				sourceText, sourceLen, rewrittenSource, displayName);

			ComPtr<ID3DBlob> errors;
			UINT flags = 0;
#ifdef _DEBUG
			flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
			flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
			flags |= D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY;
			flags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

			char const *virtName = (displayName && *displayName) ? displayName : "pixel_shader.psh";

			HRESULT const compileHr = D3DCompile(
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

			if (FAILED(compileHr))
			{
				// HIGH-1: NON-FATAL. Log the error blob so the magenta tombstone
				// for this shader's draws has a diagnostic trail and continue.
				char const *err = errors ? static_cast<char const *>(errors->GetBufferPointer()) : "no error blob";
				DEBUG_REPORT_LOG_PRINT(true,
					("tryCompilePixelShaderFromHlslNoFatal: D3DCompile %s '%s' failed (hr=0x%08lX): %s\n",
					kPixelShaderProfile, virtName, static_cast<unsigned long>(compileHr), err));
				return false;
			}
			if (errors && errors->GetBufferSize() > 0)
			{
				DEBUG_REPORT_LOG_PRINT(true,
					("tryCompilePixelShaderFromHlslNoFatal: '%s' warnings:\n%s\n",
					virtName, static_cast<char const *>(errors->GetBufferPointer())));
			}

			Direct3d11_ShaderCache::store(hash, blob.Get());
		}

		HRESULT const createHr = Direct3d11_Device::getDevice()->CreatePixelShader(
			blob->GetBufferPointer(),
			blob->GetBufferSize(),
			nullptr,
			outComPtr.GetAddressOf());
		if (FAILED(createHr))
		{
			// HIGH-1: NON-FATAL. Magenta tombstone owns the draw.
			DEBUG_REPORT_LOG_PRINT(true,
				("tryCompilePixelShaderFromHlslNoFatal: CreatePixelShader '%s' failed hr=0x%08lX\n",
				(displayName && *displayName) ? displayName : "pixel_shader.psh",
				static_cast<unsigned long>(createHr)));
			return false;
		}

		// HIGH-2: hand the compiled DXBC blob to the caller so the program-data
		// object can retain it for downstream reflection (Plan 03 consumes the
		// cache populated by the ctor's reflect-once block).
		outBytecodeBlob = blob;
		return true;
	}

	// ------------------------------------------------------------------
	// Plan 17-02 R3-02e: one-shot WARN when strncpy_s with _TRUNCATE
	// actually truncated a reflected POD-field name. Plan 03 looks up
	// variables by EXACT name match -- a silent truncation produces a
	// silent zero upload. One shot per process keeps the log from
	// flooding when many shaders share the same long-named cbuffer.
	void warnOnceOnTruncation(char const *field, char const *originalName)
	{
		static bool s_warned = false;
		if (s_warned) return;
		s_warned = true;
		DEBUG_REPORT_LOG_PRINT(true,
			("Direct3d11_PixelShaderProgramData: reflected %s truncated (original: '%s'); "
			 "Plan 03 looks up variables by EXACT name match -- truncation observed.\n",
			field        ? field        : "?",
			originalName ? originalName : "?"));
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

		// Plan 11-09.15 Iter-28: COLOR0 detection. Iter-27 surfaced UI
		// quads (drawQuadList implemented) but fonts rendered invisible
		// and panels showed no tint -- because the per-VS PS body did
		// only `t.Sample(s, uv)` and IGNORED the COLOR0 VS output that
		// carries vertex color (font text color, panel tint, fade alpha).
		// ui.vsh outputs `COLOR0 xyzw FLOAT32` at register o1; the PS
		// generator must consume it. Backdrop VS `2d_texture.vsh` has no
		// COLOR0 output -- unaffected.
		//
		// Match: SemanticName=="COLOR", SemanticIndex==0, FLOAT32,
		// ComponentMask has all four xyzw bits (matches ui.vsh exactly).
		// We require xyzw rather than xyz-with-alpha-defaulted because
		// the engine's vertex-color path always supplies a 4-channel
		// PackedArgb -- partial masks would point to an unusual asset
		// we should investigate separately.
		int color0Index = -1;
		for (size_t i = 0; i < outputs.size(); ++i)
		{
			Direct3d11_ReflectedVSOutput const &o = outputs[i];
			if (_stricmp(o.SemanticName, "COLOR") != 0) continue;
			if (o.SemanticIndex != 0) continue;
			if (o.ComponentType != D3D_REGISTER_COMPONENT_FLOAT32) continue;
			if ((o.ComponentMask & 0xF) != 0xF) continue;
			color0Index = static_cast<int>(i);
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

			// Plan 11-09.15 Iter-44B: per-pass alpha-test parameters.
			// alphaTest.x = enable (0/1), alphaTest.y = reference (0..1).
			// State pushed by Direct3d11_StateCache::setAlphaTest from
			// StaticShaderData::apply per-pass. Only matters when sampling
			// a texture, so we only declare the cbuffer in branches that
			// have texcoord0. Test semantics emulate D3D9's dominant
			// GreaterOrEqual: discard pixel when sampled alpha < ref.
			// Other compare functions are not currently supported in this
			// scaffold; if a future asset needs them we extend the cbuffer
			// with a function index.
			hlsl += "cbuffer PSAlphaTest : register(b1) { float4 alphaTest; };\n";
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
		if (texcoord0Index >= 0 && color0Index >= 0)
		{
			// Plan 11-09.15 Iter-29B: restored to Iter-28 `tex*color`
			// modulate while we collect drawQuadList routing diagnostics
			// (added in Direct3d11_StateCache.cpp this iter). Iter-29A1
			// (`.a`-mask), 29A2 (`.r`-mask), and 29A3 (raw `tex`) all
			// failed to surface letter shapes — which rules out the
			// PS-shape hypothesis space and points to either a wrong-
			// SRV-bound bug for font draws, or font draws taking a
			// different VS path than ui.vsh entirely. The diagnostic
			// landing in this iter logs template name + VS hash + SRV0
			// + vertCount per drawQuadList call for the first 50 calls
			// so we can SEE which routing path glyph batches take.
			char texField[32], colField[32];
			snprintf(texField, sizeof(texField), "_v%d", texcoord0Index);
			snprintf(colField, sizeof(colField), "_v%d", color0Index);
			// Plan 11-09.15 Iter-44B: split the return into compute + clip + return so the
			// per-pass alpha-test (state from PSAlphaTest cbuffer) can discard sub-ref pixels.
			hlsl += "    float4 col = t.Sample(s, input.";
			hlsl += texField;
			hlsl += ".xy) * input.";
			hlsl += colField;
			hlsl += ";\n";
			hlsl += "    if (alphaTest.x > 0.5) clip(col.a - alphaTest.y);\n";
			hlsl += "    return col;\n";
		}
		else if (texcoord0Index >= 0)
		{
			// Plan 11-09.15 Iter-27: REVERTED Iter-12/13/14 diagnostic PS hacks.
			// Iter-26 plus CODEX+Cursor consult identified the real bug:
			// Direct3d11_StateCache::drawQuadList was an unimplemented stub
			// that silently dropped all UI quad draws. With drawQuadList
			// implemented (Iter-27) the per-VS PS texture-sample path is
			// now the right thing to do. Backdrop VS `2d_texture.vsh`
			// hits this branch (no COLOR0 output).
			char field[32];
			snprintf(field, sizeof(field), "_v%d", texcoord0Index);
			// Plan 11-09.15 Iter-44B: same alpha-test compute+clip+return shape as the
			// texcoord+color branch above. PSAlphaTest cbuffer declared at the top.
			hlsl += "    float4 col = t.Sample(s, input.";
			hlsl += field;
			hlsl += ".xy);\n";
			hlsl += "    if (alphaTest.x > 0.5) clip(col.a - alphaTest.y);\n";
			hlsl += "    return col;\n";
		}
		else if (color0Index >= 0)
		{
			// Plan 11-09.15 Iter-28: vertex-color-only (no texture). This
			// path matches `2d.vsh` which the Iter-26 diagnostic logged
			// with SRV0=NULL -- colored quads (panel fills, separators).
			// Previously fell through to magenta because no TEXCOORD0
			// output; now emits the vertex color directly.
			char colField[32];
			snprintf(colField, sizeof(colField), "_v%d", color0Index);
			hlsl += "    return input.";
			hlsl += colField;
			hlsl += ";\n";
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

	// ------------------------------------------------------------------
	// Plan 17-02 (D-09 recompile-first): if Plan 17-01's PSRC retain landed
	// the original source text on the pixel-shader program AND it's a
	// //hlsl shader (R3-02c classification: BOM/whitespace-aware first-line
	// lowercase-first-7 compare), recompile via the NON-FATAL sibling helper
	// (HIGH-1). On success, m_d3dPS holds a real ID3D11PixelShader,
	// m_psBytecodeBlob retains the DXBC for downstream reflection, and the
	// reflected cbuffer layout + PS input signature are cached on this
	// program-data object for Plan 03 to consume (HIGH-2). Failures fall
	// through to the existing PEXE-reject log so the magenta tombstone
	// (Plan 11-09 Iter-2 ms_fallbackPS / Iter-4 per-VS dynamic PS) owns
	// THIS shader's draws.
	{
		char const * const psrcText = pixelShaderProgram.m_psrcText;
		int          const psrcLen  = pixelShaderProgram.m_psrcLen;
		if (psrcText && psrcLen >= 7)
		{
			// R3-02c (parity with Plan 17-01 R3-01c): skip leading whitespace +
			// a possible UTF-8 BOM before the lowercase-first-7 compare.
			// ShaderBuilder PixelShaderProgramView.cpp:301-308 classifies the
			// FIRST LINE, not byte 0; BOM/newline-prefixed PSRC must NOT
			// misclassify as asm.
			int hlslStart = 0;
			if (psrcLen >= 3
				&& static_cast<unsigned char>(psrcText[0]) == 0xEF
				&& static_cast<unsigned char>(psrcText[1]) == 0xBB
				&& static_cast<unsigned char>(psrcText[2]) == 0xBF)
			{
				hlslStart = 3;
			}
			while (hlslStart < psrcLen)
			{
				char const c = psrcText[hlslStart];
				if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
				++hlslStart;
			}

			bool isHlsl = false;
			if (psrcLen - hlslStart >= 7)
			{
				char prefix[8] = {0};
				for (int i = 0; i < 7; ++i)
				{
					char const c = psrcText[hlslStart + i];
					prefix[i] = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
				}
				if (std::memcmp(prefix, "//hlsl ", 7) == 0)
					isHlsl = true;
			}

			if (isHlsl)
			{
				// R3-02g: D3DCompile sourceLen is the SOURCE-TEXT length via
				// strlen, NOT pixelShaderProgram.m_psrcLen (which per Plan 17-01
				// R3-01b is the IFF chunk-bytes-consumed length INCLUDING the
				// trailing NUL). Feeding the chunk length to D3DCompile would
				// bake an extra NUL byte into the cache key + compile input.
				size_t const sourceLen = std::strlen(psrcText);

				ComPtr<ID3DBlob> bytecodeBlob;
				bool const ok = tryCompilePixelShaderFromHlslNoFatal(
					psrcText, sourceLen,
					pixelShaderProgram.getFileName(),
					m_d3dPS, bytecodeBlob);
				if (ok && bytecodeBlob)
				{
					// HIGH-2: retain the DXBC and reflect ONCE here so Plan 03's
					// per-draw apply() does NOT call D3DReflect.
					m_psBytecodeBlob = bytecodeBlob;

					ComPtr<ID3D11ShaderReflection> reflector;
					HRESULT const reflectHr = D3DReflect(
						bytecodeBlob->GetBufferPointer(),
						bytecodeBlob->GetBufferSize(),
						IID_PPV_ARGS(reflector.GetAddressOf()));
					if (SUCCEEDED(reflectHr) && reflector)
					{
						D3D11_SHADER_DESC shaderDesc = {};
						if (SUCCEEDED(reflector->GetDesc(&shaderDesc)))
						{
							ID3D11InfoQueue * const iq = Direct3d11_Device::getInfoQueue();

							// HIGH-2: cache cbuffer layouts (Plan 03 looks up by name).
							for (UINT i = 0; i < shaderDesc.ConstantBuffers; ++i)
							{
								ID3D11ShaderReflectionConstantBuffer *cb = reflector->GetConstantBufferByIndex(i);
								if (!cb) continue;
								D3D11_SHADER_BUFFER_DESC cbDesc = {};
								if (FAILED(cb->GetDesc(&cbDesc))) continue;

								Direct3d11_ReflectedPSCbufferLayout layout = {};
								// R3-02e: WARN once on truncation.
								if (cbDesc.Name && std::strlen(cbDesc.Name) >= sizeof(layout.Name))
									warnOnceOnTruncation("PS-cbuffer.Name", cbDesc.Name);
								strncpy_s(layout.Name, sizeof(layout.Name),
								          cbDesc.Name ? cbDesc.Name : "", _TRUNCATE);
								layout.TotalSize = cbDesc.Size;
								D3D11_SHADER_INPUT_BIND_DESC bindDesc = {};
								if (cbDesc.Name
									&& SUCCEEDED(reflector->GetResourceBindingDescByName(cbDesc.Name, &bindDesc)))
								{
									layout.BindPoint = bindDesc.BindPoint;
								}
								for (UINT v = 0; v < cbDesc.Variables; ++v)
								{
									ID3D11ShaderReflectionVariable *var = cb->GetVariableByIndex(v);
									if (!var) continue;
									D3D11_SHADER_VARIABLE_DESC vd = {};
									if (FAILED(var->GetDesc(&vd))) continue;
									Direct3d11_ReflectedPSCbufferVar pv = {};
									if (vd.Name && std::strlen(vd.Name) >= sizeof(pv.Name))
										warnOnceOnTruncation("PS-cbuffer-var.Name", vd.Name);
									strncpy_s(pv.Name, sizeof(pv.Name),
									          vd.Name ? vd.Name : "", _TRUNCATE);
									pv.StartOffset = vd.StartOffset;
									pv.Size        = vd.Size;
									layout.Vars.push_back(pv);
								}
								m_reflectedCbufferLayouts.push_back(layout);

								// R3-02b DUAL-ROUTE diagnostic for HIGH-4 + Plan 03 SR-2:
								// AddApplicationMessage routes via the InfoQueue file sink
								// (stage/d3d11-debug.log) -- visible under explorer launch
								// (precedent: emitFirstGeneratorLog :535-540).
								// DEBUG_REPORT_LOG_PRINT is the live-debug side-channel.
								char msg[512];
								_snprintf_s(msg, sizeof(msg), _TRUNCATE,
									"PS-cbuffer shader='%s' name='%s' bindPoint=%u totalSize=%u vars=%u",
									pixelShaderProgram.getFileName(),
									layout.Name, layout.BindPoint, layout.TotalSize,
									static_cast<unsigned>(layout.Vars.size()));
								if (iq) iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, msg);
								DEBUG_REPORT_LOG_PRINT(true, ("%s\n", msg));
							}

							// HIGH-4: PS input signature dump (cache + R3-02b DUAL-ROUTE log).
							for (UINT i = 0; i < shaderDesc.InputParameters; ++i)
							{
								D3D11_SIGNATURE_PARAMETER_DESC desc = {};
								if (FAILED(reflector->GetInputParameterDesc(i, &desc))) continue;
								// Filter SV_* system-value inputs -- they're auto-generated
								// by the rasterizer, not a VS-output linkage concern.
								if (desc.SystemValueType != D3D_NAME_UNDEFINED) continue;

								Direct3d11_ReflectedPSInputSig sig = {};
								if (desc.SemanticName)
								{
									if (std::strlen(desc.SemanticName) >= sizeof(sig.SemanticName))
										warnOnceOnTruncation("PS-input.SemanticName", desc.SemanticName);
									strncpy_s(sig.SemanticName, sizeof(sig.SemanticName),
									          desc.SemanticName, _TRUNCATE);
								}
								sig.SemanticIndex = desc.SemanticIndex;
								sig.Register      = desc.Register;
								sig.Mask          = desc.Mask;
								sig.ComponentType = desc.ComponentType;
								m_reflectedPSInputs.push_back(sig);

								// R3-02b DUAL-ROUTE diagnostic for HIGH-4.
								char msg[256];
								_snprintf_s(msg, sizeof(msg), _TRUNCATE,
									"PS-input-sig shader='%s' param='%s%u' register=%u mask=0x%X componentType=%d",
									pixelShaderProgram.getFileName(),
									sig.SemanticName, sig.SemanticIndex,
									sig.Register, sig.Mask, static_cast<int>(sig.ComponentType));
								if (iq) iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, msg);
								DEBUG_REPORT_LOG_PRINT(true, ("%s\n", msg));
							}
						}
					}
					// Reflection failure is NON-FATAL (mirror the VS precedent at
					// Direct3d11_VertexShaderData.cpp:611-665). Caches stay empty;
					// Plan 03 must handle the "empty layouts -> skip upload" path.
					return;   // Recompile success path -- m_d3dPS is bound, caches populated as available.
				}
				// HIGH-1: compile failed non-fatally. Log + fall through to the
				// existing PEXE-reject log so the magenta tombstone path takes
				// over for THIS shader's draws.
				DEBUG_REPORT_LOG_PRINT(true,
					("Direct3d11_PixelShaderProgramData: '%s' //hlsl PSRC recompile FAILED; m_d3dPS left null -> magenta tombstone for this shader's draws\n",
					pixelShaderProgram.getFileName()));
			}
			// else: PSRC is asm or some other prefix. Per SR-3 (handled by
			// Plan 17-02 Task 0 census-gate decision), asm port is OUT OF SCOPE
			// for this plan -- 17-02B (placeholder) is the future asm lane.
			// Fall through to the existing PEXE-reject log so the magenta
			// tombstone applies for THIS shader.
		}
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
