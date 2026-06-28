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
#include <set>            // Plan 17-04 Task 1: per-(VS, PS) once-per-pair logging dedupe set; Plan 17-04.X: one-shot-per-fileName PSRC dump dedupe
#include <string>         // Iter-4: HLSL generator builds std::string source
#include <utility>        // Plan 17-04 Task 1: std::pair key for the dedupe set
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
		defines.push_back({ "D3D11_REWRITE_VERSION",  "26" });   // CONSULT-53: 25 -> 26 (additive-glow alphaTest.z premultiply in generated PS). Phase 19: 24 -> 25 (Rule F specular-pow NaN guard for interiors/NPCs). Plan 17-08 GAP-5: 23 -> 24 (wrapper now USES VS COLOR0 with NaN guard, not hard 0). 22->23 was the COLOR-zero beachhead. Prior: Plan 17-07 Round-5 item 5: bump 21 -> 22 (live tree already carried 21 from Plan 17-02; a 20->21 re-bump would be a NO-OP) to invalidate stale .cso caches for the per-VS rewrite lane + VS-output-signature-hash salt. MUST stay in lockstep with the non-fatal helper's define below so the FATAL + non-fatal helpers share one .cso hash. Plan 17-02 (20->21) + Iter-29B notes preserved in version history.
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
		Microsoft::WRL::ComPtr<ID3DBlob>          &outBytecodeBlob,
		uint32_t vsOutputSignatureHashSalt = 0)   // Plan 17-07 MEDIUM: non-zero for the per-VS rewrite lane -> salts the .cso cache key so the same PSRC recompiled against different VS output orderings does not collide. 0 (default) for the native ctor lane -> cache key identical to Plan 17-02.
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
		defines.push_back({ "D3D11_REWRITE_VERSION",  "26" });  // CONSULT-53: 25 -> 26 (additive-glow premultiply); must match the FATAL helper (above) so the .cso cache hash is shared. Phase 19: 24 -> 25 (Rule F specular-pow NaN guard).
		// Plan 17-07 MEDIUM: per-VS rewrite lane salt. saltStr must outlive the
		// D3DCompile call below (D3D_SHADER_MACRO holds char const*), so it is a
		// stack buffer scoped to this function body. Native ctor lane passes 0 ->
		// no salt define -> cache key identical to Plan 17-02.
		char saltStr[16] = { 0 };
		if (vsOutputSignatureHashSalt != 0)
		{
			_snprintf_s(saltStr, sizeof(saltStr), _TRUNCATE, "%u",
				static_cast<unsigned>(vsOutputSignatureHashSalt));
			defines.push_back({ "D3D11_VS_OUTPUT_SIG_HASH", saltStr });
		}
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
	// the VS provides TEXCOORD0 + FLOAT32 + at least 2 components AND the
	// caller allows sampling (allowTextureSample == an SRV is actually
	// bound at slot 0 -- sampling a null SRV returns (0,0,0,0) = black,
	// so untextured passes must take the color-only branch instead);
	// otherwise vertex color or magenta. Returns empty string on
	// validation failure (caller treats as tombstone).
	std::string buildHlslForVSOutputs(std::vector<Direct3d11_ReflectedVSOutput> const &outputsIn, bool allowTextureSample)
	{
		// Defensive: validate semantic names BEFORE we start emitting.
		for (Direct3d11_ReflectedVSOutput const &o : outputsIn)
		{
			if (!isValidHlslIdentifier(o.SemanticName))
				return std::string();
		}

		// Sort by Register, then by FIRST COMPONENT within the register
		// (walls fix 2026-06-11): two semantics can share one register with
		// complementary masks (tfcl interior VS packs FOG at o2.x and
		// TEXCOORD0 at o2.yz). fxc packs PS inputs greedily in DECLARATION
		// order, so within a shared register the lower-component semantic
		// must be declared first or the PS's packing diverges from the VS's
		// (FOG@x/uv@yz vs uv@xy/FOG@z) -> register-position linkage break
		// (id=343) / garbage uv. SemanticIndex remains the final tie-break.
		auto firstComponent = [](UINT mask) -> UINT {
			if (mask & 0x1) return 0;
			if (mask & 0x2) return 1;
			if (mask & 0x4) return 2;
			if (mask & 0x8) return 3;
			return 0;
		};
		std::vector<Direct3d11_ReflectedVSOutput> outputs = outputsIn;
		std::sort(outputs.begin(), outputs.end(),
			[&firstComponent](Direct3d11_ReflectedVSOutput const &a, Direct3d11_ReflectedVSOutput const &b) {
				if (a.Register != b.Register) return a.Register < b.Register;
				UINT const fa = firstComponent(a.ComponentMask);
				UINT const fb = firstComponent(b.ComponentMask);
				if (fa != fb) return fa < fb;
				return a.SemanticIndex < b.SemanticIndex;
			});

		// Find TEXCOORD0 / FLOAT32 / >=2 components declared -- decides
		// whether the body samples or returns vertex color / magenta.
		// ComponentMask (declared signature contract) gates this, not
		// ReadWriteMask (runtime write behavior).
		//
		// Walls fix 2026-06-11: the gate previously required the uv at
		// .xy of its register ((mask & 0x3) == 0x3). The tfcl interior
		// family packs fog at o2.x and uv0 at o2.yz (mask 0x6), which
		// failed that gate and forced the color-only branch -- flat grey
		// untextured walls/floor. The declared struct field's arity equals
		// the mask's popcount and the field's own .xy IS the semantic's
		// first two components wherever they live in the register, so
		// requiring >=2 components is sufficient.
		int texcoord0Index = -1;
		if (allowTextureSample)
		{
			for (size_t i = 0; i < outputs.size(); ++i)
			{
				Direct3d11_ReflectedVSOutput const &o = outputs[i];
				if (_stricmp(o.SemanticName, "TEXCOORD") != 0) continue;
				if (o.SemanticIndex != 0) continue;
				if (o.ComponentType != D3D_REGISTER_COMPONENT_FLOAT32) continue;
				int componentCount = 0;
				if (o.ComponentMask & 0x1) ++componentCount;
				if (o.ComponentMask & 0x2) ++componentCount;
				if (o.ComponentMask & 0x4) ++componentCount;
				if (o.ComponentMask & 0x8) ++componentCount;
				if (componentCount < 2) continue;
				texcoord0Index = static_cast<int>(i);
				break;
			}
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

		// Fog fix 2026-06-11: FOG0 detection. The reauthored //hlsl VSes
		// output a per-vertex EXP2 fog factor (1 = no fog .. 0 = full fog)
		// at semantic FOG; D3D9 hardware consumed it post-PS, D3D11 must
		// lerp in the PS. Build the access expression (field may be wider
		// than 1 component on exotic packings -> take .x).
		std::string fogStmt;
		for (size_t i = 0; i < outputs.size(); ++i)
		{
			Direct3d11_ReflectedVSOutput const &o = outputs[i];
			if (_stricmp(o.SemanticName, "FOG") != 0) continue;
			if (o.SemanticIndex != 0) continue;
			if (o.ComponentType != D3D_REGISTER_COMPONENT_FLOAT32) continue;
			int componentCount = 0;
			if (o.ComponentMask & 0x1) ++componentCount;
			if (o.ComponentMask & 0x2) ++componentCount;
			if (o.ComponentMask & 0x4) ++componentCount;
			if (o.ComponentMask & 0x8) ++componentCount;
			if (componentCount < 1) continue;
			char buf[128];
			snprintf(buf, sizeof(buf),
				"    if (fogColor.w > 0.5) col.rgb = lerp(fogColor.rgb, col.rgb, saturate(input._v%zu%s));\n",
				i, componentCount > 1 ? ".x" : "");
			fogStmt = buf;
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

		// Plan 11-09.15 Iter-44B: per-pass alpha-test parameters.
		// alphaTest.x = enable (0/1), alphaTest.y = reference (0..1).
		// State pushed by Direct3d11_StateCache::setAlphaTest from
		// StaticShaderData::apply per-pass. Test semantics emulate D3D9's
		// dominant GreaterOrEqual: discard pixel when sampled alpha < ref.
		// Other compare functions are not currently supported in this
		// scaffold; if a future asset needs them we extend the cbuffer
		// with a function index.
		//
		// Fog fix 2026-06-11: declared unconditionally now (the color-only
		// branch needs fogColor too); textureFactor/fogColor layout shared
		// with the FFP cascade builder (Direct3d11_PSAlphaTestCB).
		hlsl += "cbuffer PSAlphaTest : register(b1) { float4 alphaTest; float4 textureFactor; float4 fogColor; };\n";

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
			hlsl += ".xy);\n";
			// CONSULT-53 (2026-06-28): premultiply the texture sample by its own alpha for
			// additive (One-dest, SrcColor) passes (alphaTest.z flag), BEFORE the vertex-color
			// modulate. Straight-alpha UI atlases keep full RGB where alpha==0; the additive
			// SrcColor blend ignores alpha and adds that masked RGB -> solid cyan fill on the
			// space-HUD gauge backgrounds (gl11), where gl05 shows only the thin arc. D3D9
			// effectively feeds premultiplied data; mirror it here. Over-blend
			// (SrcAlpha/InvSrcAlpha) passes leave the flag 0 (premultiplying them would
			// double-apply alpha). col.a is preserved for the alpha-test below.
			hlsl += "    if (alphaTest.z > 0.5) col.rgb *= col.a;\n";
			hlsl += "    col = col * input.";
			hlsl += colField;
			hlsl += ";\n";
			hlsl += "    if (alphaTest.x > 0.5) clip(col.a - alphaTest.y);\n";
			hlsl += fogStmt;   // fog fix 2026-06-11: post-alpha-test fog lerp (D3D9 pipeline order)
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
			hlsl += "    if (alphaTest.z > 0.5) col.rgb *= col.a;\n";   // CONSULT-53: additive-glow premultiply parity (no vtx-color branch)
			hlsl += "    if (alphaTest.x > 0.5) clip(col.a - alphaTest.y);\n";
			hlsl += fogStmt;   // fog fix 2026-06-11
			hlsl += "    return col;\n";
		}
		else if (color0Index >= 0)
		{
			// Plan 11-09.15 Iter-28: vertex-color-only (no texture). This
			// path matches `2d.vsh` which the Iter-26 diagnostic logged
			// with SRV0=NULL -- colored quads (panel fills, separators).
			// Previously fell through to magenta because no TEXCOORD0
			// output; now emits the vertex color directly.
			// Fog fix 2026-06-11: routed through `col` so the fog lerp
			// applies (UI VSes output no FOG -> fogStmt empty -> unchanged).
			char colField[32];
			snprintf(colField, sizeof(colField), "_v%d", color0Index);
			hlsl += "    float4 col = input.";
			hlsl += colField;
			hlsl += ";\n";
			hlsl += fogStmt;
			hlsl += "    return col;\n";
		}
		else
		{
			hlsl += "    return float4(1.0f, 0.0f, 1.0f, 1.0f);\n";
		}
		hlsl += "}\n";

		return hlsl;
	}

	// ------------------------------------------------------------------
	// Detail-blend fix 2026-06-11: which combiner args an op consumes.
	// Every supported op reads arg1 except selectArg2, and arg2 except
	// selectArg1; arg0 consumption (lerp/multiplyAdd) is checked at the
	// opExpr site. Used to decide whether an empty (unsupported) argument
	// expression should abort cascade generation.
	bool usesArg1(uint8_t op) { return op != static_cast<uint8_t>(ShaderImplementationPassStage::TO_selectArg2); }
	bool usesArg2(uint8_t op) { return op != static_cast<uint8_t>(ShaderImplementationPassStage::TO_selectArg1); }

	// ------------------------------------------------------------------
	// Detail-blend fix 2026-06-11: FFP texture-stage cascade PS generator.
	//
	// Emulates the D3D9 fixed-function multi-texture cascade (D3DTSS_COLOROP/
	// ALPHAOP + args, reference semantics per the D3D9 translation tables at
	// Direct3d9_ShaderImplementationData.cpp:86-124 and Stage::apply) in a
	// generated PS, so FFP stage-based .shts (interior wall/floor detail
	// blends -- c_2blend_dirt etc.) render their full layer stack instead of
	// the single-texture first cut. Per D3D9 semantics:
	//   * stage 0 CURRENT = DIFFUSE (interpolated vertex color, COLOR0)
	//   * each stage computes color and alpha independently, result is
	//     saturated and becomes CURRENT for the next stage
	//   * cascade terminates at the first TO_disable colorOp (construct
	//     already stops there, so stageCount is the live count)
	//   * TFACTOR = the pass's textureFactor (b1 cbuffer, pushed by apply())
	//
	// Returns empty string when the cascade uses anything unsupported
	// (TA_specular/TA_temp args, result-to-temp, dot3/bumpenv/premodulate
	// ops, texture arg on a textureless stage, texcoord index the VS does
	// not provide) -- the caller then falls back to the single-texture
	// builder above, never magenta.
	std::string buildHlslForVSOutputsFfp(std::vector<Direct3d11_ReflectedVSOutput> const &outputsIn, Direct3d11_FfpCombinerDesc const &desc)
	{
		using SIPStage = ShaderImplementationPassStage;

		if (desc.stageCount <= 0 || desc.stageCount > 8)
			return std::string();

		for (Direct3d11_ReflectedVSOutput const &o : outputsIn)
		{
			if (!isValidHlslIdentifier(o.SemanticName))
				return std::string();
		}

		// Sort identical to buildHlslForVSOutputs (register, then first
		// component, then semantic index) -- the declaration-order packing
		// invariant is shared.
		auto firstComponent = [](UINT mask) -> UINT {
			if (mask & 0x1) return 0;
			if (mask & 0x2) return 1;
			if (mask & 0x4) return 2;
			if (mask & 0x8) return 3;
			return 0;
		};
		std::vector<Direct3d11_ReflectedVSOutput> outputs = outputsIn;
		std::sort(outputs.begin(), outputs.end(),
			[&firstComponent](Direct3d11_ReflectedVSOutput const &a, Direct3d11_ReflectedVSOutput const &b) {
				if (a.Register != b.Register) return a.Register < b.Register;
				UINT const fa = firstComponent(a.ComponentMask);
				UINT const fb = firstComponent(b.ComponentMask);
				if (fa != fb) return fa < fb;
				return a.SemanticIndex < b.SemanticIndex;
			});

		// Locate COLOR0 (diffuse), every TEXCOORD index (>=2 FLOAT32
		// components, same gate as the single-texture builder), and the
		// FOG0 factor (fog fix 2026-06-11).
		int colorField = -1;
		int texcoordField[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
		std::string fogStmt;
		for (size_t i = 0; i < outputs.size(); ++i)
		{
			Direct3d11_ReflectedVSOutput const &o = outputs[i];
			if (o.ComponentType != D3D_REGISTER_COMPONENT_FLOAT32) continue;
			if (_stricmp(o.SemanticName, "COLOR") == 0 && o.SemanticIndex == 0
				&& (o.ComponentMask & 0xF) == 0xF)
			{
				colorField = static_cast<int>(i);
			}
			else if (_stricmp(o.SemanticName, "TEXCOORD") == 0 && o.SemanticIndex < 8)
			{
				int componentCount = 0;
				if (o.ComponentMask & 0x1) ++componentCount;
				if (o.ComponentMask & 0x2) ++componentCount;
				if (o.ComponentMask & 0x4) ++componentCount;
				if (o.ComponentMask & 0x8) ++componentCount;
				if (componentCount >= 2)
					texcoordField[o.SemanticIndex] = static_cast<int>(i);
			}
			else if (_stricmp(o.SemanticName, "FOG") == 0 && o.SemanticIndex == 0 && fogStmt.empty())
			{
				int componentCount = 0;
				if (o.ComponentMask & 0x1) ++componentCount;
				if (o.ComponentMask & 0x2) ++componentCount;
				if (o.ComponentMask & 0x4) ++componentCount;
				if (o.ComponentMask & 0x8) ++componentCount;
				if (componentCount < 1) continue;
				char buf[128];
				snprintf(buf, sizeof(buf),
					"    if (fogColor.w > 0.5) cur.rgb = lerp(fogColor.rgb, cur.rgb, saturate(input._v%zu%s));\n",
					i, componentCount > 1 ? ".x" : "");
				fogStmt = buf;
			}
		}

		// Per-stage argument expression builder. channelColor selects the
		// .rgb (with alphaReplicate -> .aaa) vs .a form. Returns empty on
		// unsupported argument.
		auto argExpr = [&](int stageIdx, uint8_t arg, uint8_t mods, bool channelColor) -> std::string {
			char buf[64];
			std::string e;
			bool const alphaReplicate = channelColor && (mods & 0x2) != 0;
			char const *suffix = channelColor ? (alphaReplicate ? ".aaa" : ".rgb") : ".a";
			switch (arg)
			{
				case SIPStage::TA_current:        e = std::string("cur") + suffix; break;
				case SIPStage::TA_diffuse:        e = std::string("dif") + suffix; break;
				case SIPStage::TA_texture:
					if (!desc.stages[stageIdx].hasTexture)
						return std::string();
					snprintf(buf, sizeof(buf), "tex%d%s", stageIdx, suffix);
					e = buf;
					break;
				case SIPStage::TA_textureFactor:  e = std::string("textureFactor") + suffix; break;
				case SIPStage::TA_specular:       // VS provides no COLOR1 -- unsupported
				case SIPStage::TA_temp:           // result-to-temp routing -- unsupported
				default:
					return std::string();
			}
			if (mods & 0x1)
				e = "(1.0 - " + e + ")";
			return e;
		};

		// Per-stage op expression. blendTex/blendDif/blendCur/blendFac are
		// the scalar blend factors. Returns empty on unsupported op.
		auto opExpr = [&](int stageIdx, uint8_t op, std::string const &a0, std::string const &a1, std::string const &a2,
		                  std::string const &a1Alpha, bool /*channelColor*/) -> std::string {
			char texA[32];
			snprintf(texA, sizeof(texA), "tex%d.a", stageIdx);
			bool const stageHasTexture = desc.stages[stageIdx].hasTexture != 0;
			switch (op)
			{
				case SIPStage::TO_selectArg1:        return a1;
				case SIPStage::TO_selectArg2:        return a2;
				case SIPStage::TO_modulate:          return "(" + a1 + " * " + a2 + ")";
				case SIPStage::TO_modulate2x:        return "((" + a1 + " * " + a2 + ") * 2.0)";
				case SIPStage::TO_modulate4x:        return "((" + a1 + " * " + a2 + ") * 4.0)";
				case SIPStage::TO_add:               return "(" + a1 + " + " + a2 + ")";
				case SIPStage::TO_addSigned:         return "(" + a1 + " + " + a2 + " - 0.5)";
				case SIPStage::TO_addSigned2x:       return "((" + a1 + " + " + a2 + " - 0.5) * 2.0)";
				case SIPStage::TO_subtract:          return "(" + a1 + " - " + a2 + ")";
				case SIPStage::TO_addSmooth:         return "(" + a1 + " + " + a2 + " * (1.0 - " + a1 + "))";
				case SIPStage::TO_blendDiffuseAlpha: return "lerp(" + a2 + ", " + a1 + ", dif.a)";
				case SIPStage::TO_blendTextureAlpha:
					if (!stageHasTexture) return std::string();
					return "lerp(" + a2 + ", " + a1 + ", " + texA + ")";
				case SIPStage::TO_blendFactorAlpha:  return "lerp(" + a2 + ", " + a1 + ", textureFactor.a)";
				case SIPStage::TO_blendTextureAlphapm:
					if (!stageHasTexture) return std::string();
					return "(" + a1 + " + " + a2 + " * (1.0 - " + texA + "))";
				case SIPStage::TO_blendCurrentAlpha: return "lerp(" + a2 + ", " + a1 + ", cur.a)";
				case SIPStage::TO_modulateAlpha_addColor:
					if (a1Alpha.empty()) return std::string();
					return "(" + a1 + " + " + a1Alpha + " * " + a2 + ")";
				case SIPStage::TO_modulateColor_addAlpha:
					if (a1Alpha.empty()) return std::string();
					return "(" + a1 + " * " + a2 + " + " + a1Alpha + ")";
				case SIPStage::TO_modulateInvalpha_addColor:
					if (a1Alpha.empty()) return std::string();
					return "((1.0 - " + a1Alpha + ") * " + a2 + " + " + a1 + ")";
				case SIPStage::TO_modulateInvcolor_addAlpha:
					if (a1Alpha.empty()) return std::string();
					return "((1.0 - " + a1 + ") * " + a2 + " + " + a1Alpha + ")";
				case SIPStage::TO_multiplyAdd:
					if (a0.empty()) return std::string();
					return "(" + a0 + " + " + a1 + " * " + a2 + ")";
				case SIPStage::TO_lerp:
					if (a0.empty()) return std::string();
					return "lerp(" + a2 + ", " + a1 + ", " + a0 + ")";
				default:
					// TO_disable mid-list (construct stops there), dot3,
					// premodulate, bumpEnvMap* -- unsupported.
					return std::string();
			}
		};

		// Validate + build per-stage bodies first so any unsupported feature
		// aborts before we commit to the cascade.
		std::string stageBodies;
		for (int s = 0; s < desc.stageCount; ++s)
		{
			Direct3d11_FfpCombinerStage const &st = desc.stages[s];

			if (st.resultArg != static_cast<uint8_t>(SIPStage::TA_current))
				return std::string();

			char line[160];
			if (st.hasTexture)
			{
				if (st.texcoordIndex >= 8 || texcoordField[st.texcoordIndex] < 0)
					return std::string();
				snprintf(line, sizeof(line), "    float4 tex%d = t%d.Sample(s%d, input._v%d.xy);\n",
					s, s, s, texcoordField[st.texcoordIndex]);
				stageBodies += line;
			}

			// Color channel. Ops referencing only specific args evaluate
			// lazily -- build all arg expressions but tolerate empties for
			// args the op doesn't consume.
			std::string const c0 = argExpr(s, st.colorArg0, st.colorMod0, true);
			std::string const c1 = argExpr(s, st.colorArg1, st.colorMod1, true);
			std::string const c2 = argExpr(s, st.colorArg2, st.colorMod2, true);
			std::string const c1a = argExpr(s, st.colorArg1, st.colorMod1, false);
			bool const colorActive = (st.colorOp != static_cast<uint8_t>(SIPStage::TO_disable));
			std::string colorExpr;
			if (colorActive)
			{
				if (c1.empty() && usesArg1(st.colorOp)) return std::string();
				if (c2.empty() && usesArg2(st.colorOp)) return std::string();
				colorExpr = opExpr(s, st.colorOp, c0, c1, c2, c1a, true);
				if (colorExpr.empty())
					return std::string();
			}
			else
				colorExpr = "cur.rgb";

			// Alpha channel. TO_disable while color is active = passthrough.
			std::string const a0 = argExpr(s, st.alphaArg0, st.alphaMod0, false);
			std::string const a1 = argExpr(s, st.alphaArg1, st.alphaMod1, false);
			std::string const a2 = argExpr(s, st.alphaArg2, st.alphaMod2, false);
			std::string alphaExpr;
			if (st.alphaOp != static_cast<uint8_t>(SIPStage::TO_disable))
			{
				if (a1.empty() && usesArg1(st.alphaOp)) return std::string();
				if (a2.empty() && usesArg2(st.alphaOp)) return std::string();
				alphaExpr = opExpr(s, st.alphaOp, a0, a1, a2, a1, false);
				if (alphaExpr.empty())
					return std::string();
			}
			else
				alphaExpr = "cur.a";

			stageBodies += "    cur = saturate(float4(" + colorExpr + ", " + alphaExpr + "));\n";
		}

		// Cascade validated -- emit the full PS.
		std::string hlsl;
		hlsl.reserve(2048);
		hlsl += "// Detail-blend FFP combiner-cascade PS (auto-generated).\n";
		hlsl += "// Mirrors VS output struct register-for-register; emulates the\n";
		hlsl += "// D3D9 fixed-function texture stage cascade for this pass.\n";

		char decl[96];
		for (int s = 0; s < desc.stageCount; ++s)
		{
			if (!desc.stages[s].hasTexture)
				continue;
			snprintf(decl, sizeof(decl), "Texture2D    t%d : register(t%d);\nSamplerState s%d : register(s%d);\n", s, s, s, s);
			hlsl += decl;
		}
		hlsl += "cbuffer PSAlphaTest : register(b1) { float4 alphaTest; float4 textureFactor; float4 fogColor; };\n";

		hlsl += "struct PSIn\n{\n";
		hlsl += "    float4 _pos : SV_POSITION;\n";
		for (size_t i = 0; i < outputs.size(); ++i)
		{
			Direct3d11_ReflectedVSOutput const &o = outputs[i];
			std::string type = hlslTypeFor(o.ComponentType, o.ComponentMask);
			char field[48];
			snprintf(field, sizeof(field), "    %s _v%zu : %s%u;\n", type.c_str(), i, o.SemanticName, o.SemanticIndex);
			hlsl += field;
		}
		hlsl += "};\n\n";

		hlsl += "float4 main(PSIn input) : SV_TARGET\n{\n";
		if (colorField >= 0)
		{
			char difLine[48];
			snprintf(difLine, sizeof(difLine), "    float4 dif = input._v%d;\n", colorField);
			hlsl += difLine;
		}
		else
			hlsl += "    float4 dif = float4(1.0, 1.0, 1.0, 1.0);\n";
		hlsl += "    float4 cur = dif;\n";
		hlsl += stageBodies;
		hlsl += "    if (alphaTest.x > 0.5) clip(cur.a - alphaTest.y);\n";
		hlsl += fogStmt;   // fog fix 2026-06-11: post-alpha-test fog lerp (D3D9 pipeline order)
		hlsl += "    return cur;\n";
		hlsl += "}\n";

		return hlsl;
	}

	// ------------------------------------------------------------------
	// Plan 17-07 (HIGH-4): PSRC main() VS-signature reconstruction rewriter.
	//
	// The dominant char-select PSRC form (22/22 per
	// evidence/plan-17-04x-psrc-source-dump.txt) is
	//   float4 main(in TYPE NAME : SEMANTIC, ...) : SV_Target
	//
	// FINDING (project_17_07_ps_register_base_offset, from the 2026-05-29 boot
	// rewrite-diag): the VS<->PS mismatch is a register-BASE offset, NOT an
	// ordering problem. The VS reserves output register o0 for SV_Position, so its
	// user varyings start at o1; the asset PS starts its varyings at v0. D3D11
	// matches by register number -> COLOR0@v0 != COLOR0@o1 -> id=343 / INCOMPATIBLE.
	// A parameter-LIST REORDER can never introduce the +1 base shift (the first
	// declared PS param is always v0), and some PSes also SKIP a VS output, which a
	// reorder cannot pad. So this rewriter does NOT reorder -- it RECONSTRUCTS the
	// PS input signature to mirror the VS output signature exactly (SV_Position at
	// register 0, then one field per VS output in register order with matching
	// type/mask), the proven buildHlslForVSOutputs (:487-492) invariant, then wraps
	// the asset's renamed main() so its real shading body is preserved verbatim.
	//
	// Round-5 item 9: returns a {status,text} result rather than string-equality
	// control flow. Failed = parse failure / non-float4 entry / a non-system
	// consumed input with no VS producer (would compile a PS reading garbage) ->
	// caller tries the struct-bound rewriter, then selectFallbackPSForVS. Only
	// SV_Position is a supported system-value input. Input-validation bounds
	// (T-17-07-01): 64 KiB source cap, 16 param cap.

	enum class PsRewriteStatus { Rewritten, Unchanged, Failed };
	struct PsRewriteResult { PsRewriteStatus status; std::string text; };

	inline bool psRewriteIsWs(char c)    { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
	inline bool psRewriteIsAlpha(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'; }
	inline bool psRewriteIsDigit(char c) { return c >= '0' && c <= '9'; }
	inline bool psRewriteIsAlnum(char c) { return psRewriteIsAlpha(c) || psRewriteIsDigit(c); }

	std::string psRewriteTrim(std::string const &s)
	{
		size_t b = 0, e = s.size();
		while (b < e && psRewriteIsWs(s[b])) ++b;
		while (e > b && psRewriteIsWs(s[e - 1])) --e;
		return s.substr(b, e - b);
	}

	// Offset of '(' opening the entry-point main() parameter list, or npos.
	size_t psRewriteFindMainParen(std::string const &s)
	{
		size_t pos = 0;
		while ((pos = s.find("main", pos)) != std::string::npos)
		{
			bool okPrev = (pos == 0) || !psRewriteIsAlnum(s[pos - 1]);
			size_t p = pos + 4;
			while (p < s.size() && psRewriteIsWs(s[p])) ++p;
			if (okPrev && p < s.size() && s[p] == '(')
				return p;
			pos += 4;
		}
		return std::string::npos;
	}

	// Index of the ')' matching the '(' at openIdx (paren-depth scan), or npos.
	size_t psRewriteMatchParen(std::string const &s, size_t openIdx)
	{
		int depth = 0;
		for (size_t i = openIdx; i < s.size(); ++i)
		{
			if (s[i] == '(') ++depth;
			else if (s[i] == ')') { if (--depth == 0) return i; }
		}
		return std::string::npos;
	}

	struct PsParsedParam
	{
		std::string text;          // trimmed original parameter substring (preserved verbatim on emit)
		std::string semanticBase;  // e.g. "COLOR", "TEXCOORD", "SV_Position"
		UINT        semanticIndex; // trailing integer, default 0
		bool        hasSemantic;
		bool        isSystemValue;
	};

	// Parse one parameter substring. Returns false on malformed input (-> Failed).
	bool psRewriteParseParam(std::string const &raw, PsParsedParam &out)
	{
		out.text          = psRewriteTrim(raw);
		out.semanticIndex = 0;
		out.hasSemantic   = false;
		out.isSystemValue = false;
		if (out.text.empty())
			return true;   // tolerate trailing empty slot; caller filters

		size_t const colon = out.text.rfind(':');
		if (colon == std::string::npos)
			return true;   // no semantic binding -> likely struct-bound param

		size_t p = colon + 1;
		while (p < out.text.size() && psRewriteIsWs(out.text[p])) ++p;
		if (p >= out.text.size() || !psRewriteIsAlpha(out.text[p]))
			return false;  // malformed semantic

		std::string sem;
		while (p < out.text.size() && psRewriteIsAlnum(out.text[p]))
			sem += out.text[p++];

		// Split sem into alpha/underscore base + trailing decimal index.
		size_t digitStart = sem.size();
		while (digitStart > 0 && psRewriteIsDigit(sem[digitStart - 1])) --digitStart;
		if (digitStart == 0)
			return false;  // all-digit semantic is malformed

		out.semanticBase = sem.substr(0, digitStart);
		if (digitStart < sem.size())
		{
			UINT idx = 0;
			for (size_t i = digitStart; i < sem.size(); ++i)
				idx = idx * 10u + static_cast<UINT>(sem[i] - '0');
			out.semanticIndex = idx;
		}
		out.hasSemantic = true;

		// SV_ prefix (case-insensitive) => system value.
		if (out.semanticBase.size() >= 3)
		{
			auto up = [](char c) { return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 32) : c; };
			if (up(out.semanticBase[0]) == 'S' && up(out.semanticBase[1]) == 'V' && out.semanticBase[2] == '_')
				out.isSystemValue = true;
		}
		return true;
	}

	std::vector<std::string> psRewriteSplitTopLevelCommas(std::string const &params)
	{
		std::vector<std::string> out;
		int depth = 0;
		size_t start = 0;
		for (size_t i = 0; i < params.size(); ++i)
		{
			char const c = params[i];
			if (c == '(' || c == '[') ++depth;
			else if (c == ')' || c == ']') --depth;
			else if (c == ',' && depth == 0) { out.push_back(params.substr(start, i - start)); start = i + 1; }
		}
		out.push_back(params.substr(start));
		return out;
	}

	PsRewriteResult rewritePsMainParameterListForVSOutputs(
		std::string const &psrcText,
		std::vector<Direct3d11_ReflectedVSOutput> const &vsOutputsIn)
	{
		PsRewriteResult result{ PsRewriteStatus::Failed, psrcText };

		if (psrcText.size() > 64u * 1024u) return result;   // T-17-07-01 size bound
		if (vsOutputsIn.empty())           return result;

		size_t const openIdx = psRewriteFindMainParen(psrcText);
		if (openIdx == std::string::npos) return result;
		size_t const closeIdx = psRewriteMatchParen(psrcText, openIdx);
		if (closeIdx == std::string::npos) return result;

		// Locate the entry-point "main" identifier (immediately before openIdx,
		// across optional whitespace) so we can rename it -> assetMain1707, and
		// require the return type to be `float4` (only the float4-returning entry
		// form is wrapper-safe; void/out-param mains -> Failed -> fallback).
		size_t q = openIdx;
		while (q > 0 && psRewriteIsWs(psrcText[q - 1])) --q;
		if (q < 4) return result;
		size_t const mainStart = q - 4;
		if (psrcText.compare(mainStart, 4, "main") != 0) return result;
		size_t r = mainStart;
		while (r > 0 && psRewriteIsWs(psrcText[r - 1])) --r;
		if (r < 6 || psrcText.compare(r - 6, 6, "float4") != 0) return result;

		// Parse the asset main() parameter list.
		std::string const paramList = (closeIdx > openIdx + 1)
			? psrcText.substr(openIdx + 1, closeIdx - openIdx - 1)
			: std::string();
		std::vector<std::string> const rawParams = psRewriteSplitTopLevelCommas(paramList);
		if (rawParams.size() > 16) return result;           // T-17-07-01 count bound

		std::vector<PsParsedParam> params;
		for (std::string const &rp : rawParams)
		{
			PsParsedParam pp;
			if (!psRewriteParseParam(rp, pp)) return result;   // malformed -> Failed
			if (pp.text.empty()) continue;                     // tolerate trailing empty slot
			params.push_back(pp);
		}
		if (params.empty())
		{
			// PS reads no varyings -> trivially register-compatible; no rewrite.
			result.status = PsRewriteStatus::Unchanged;
			result.text   = psrcText;
			return result;
		}

		// Every param must be semantic-bound (else likely struct-bound -> let the
		// struct-bound rewriter try) and a clean identifier; only SV_Position is a
		// supported system-value input (others -> Failed -> fallback).
		for (PsParsedParam const &pp : params)
		{
			if (!pp.hasSemantic) return result;
			if (!isValidHlslIdentifier(pp.semanticBase.c_str())) return result;
			if (pp.isSystemValue && _stricmp(pp.semanticBase.c_str(), "SV_Position") != 0)
				return result;
		}

		// Sort a copy of VS outputs by Register (ties: first component, then
		// SemanticIndex) -- mirror buildHlslForVSOutputs. The first-component
		// tie-break (fog fix 2026-06-11) keeps declaration order deterministic
		// when two semantics share a register with complementary masks (e.g.
		// FOG at oN.x + TEXCOORD at oN.yz) so fxc re-derives the VS packing.
		// Validate each semantic is a clean identifier.
		std::vector<Direct3d11_ReflectedVSOutput> vsOutputs = vsOutputsIn;
		for (Direct3d11_ReflectedVSOutput const &o : vsOutputs)
			if (!isValidHlslIdentifier(o.SemanticName)) return result;
		auto firstComponent1707 = [](UINT mask) -> UINT {
			if (mask & 0x1) return 0;
			if (mask & 0x2) return 1;
			if (mask & 0x4) return 2;
			if (mask & 0x8) return 3;
			return 0;
		};
		std::sort(vsOutputs.begin(), vsOutputs.end(),
			[&firstComponent1707](Direct3d11_ReflectedVSOutput const &a, Direct3d11_ReflectedVSOutput const &b) {
				if (a.Register != b.Register) return a.Register < b.Register;
				UINT const fa = firstComponent1707(a.ComponentMask);
				UINT const fb = firstComponent1707(b.ComponentMask);
				if (fa != fb) return fa < fb;
				return a.SemanticIndex < b.SemanticIndex;
			});

		// Map each non-SV asset param to its VS output index (by semantic+index).
		// A non-system consumed input with no VS producer -> Failed (would read garbage).
		std::vector<int> paramToVs(params.size(), -1);
		for (size_t k = 0; k < params.size(); ++k)
		{
			if (params[k].isSystemValue) continue;   // SV_Position sourced from f_pos1707
			int found = -1;
			for (size_t j = 0; j < vsOutputs.size(); ++j)
			{
				if (_stricmp(params[k].semanticBase.c_str(), vsOutputs[j].SemanticName) != 0) continue;
				if (params[k].semanticIndex != vsOutputs[j].SemanticIndex) continue;
				found = static_cast<int>(j);
				break;
			}
			if (found < 0) return result;            // consumed input, no producer -> Failed
			paramToVs[k] = found;
		}

		// === VS-signature reconstruction (NOT a reorder) ===
		// The VS<->PS mismatch is a register-BASE offset, not an ordering problem
		// (see project_17_07_ps_register_base_offset): the VS reserves output
		// register o0 for SV_Position so user varyings start at o1, while the asset
		// PS starts its varyings at v0. Reordering the parameter list can never add
		// that base shift. Instead we RECONSTRUCT the PS input signature to mirror
		// the VS output signature EXACTLY -- SV_Position at register 0, then one
		// field per VS output in register order with the matching type/mask -- so
		// the HLSL compiler re-derives the SAME register + component packing as the
		// VS (the buildHlslForVSOutputs invariant), including the +1 base AND any VS
		// outputs the PS doesn't consume (kept as unused padding fields to preserve
		// alignment). The asset main() is renamed assetMain1707 and called from a new
		// wrapper main(PsIn1707), sourcing each asset param from its mirror field --
		// so the asset's real shading body is preserved verbatim.
		// Fog fix 2026-06-11: locate the VS's FOG0 output among the mirrored
		// fields. D3D9 applied EXP2 fog POST-PS in hardware from the VS oFog
		// factor; D3D11 has no fog stage, so the wrapper lerps the asset PS
		// result toward the per-pass fog color (b1, fed by StateCache
		// setFog/setPsFogMode). Field names carry the 1707 suffix so they
		// cannot collide with asset globals (pixel_shader_constants.inc
		// declares `textureFactor` etc.; asset register(cN) globals live in
		// $Globals at b0 -- b1 is the plugin's alphaTest/textureFactor/fog
		// slot, layout = Direct3d11_PSAlphaTestCB).
		int fogVsIndex = -1;
		for (size_t j = 0; j < vsOutputs.size(); ++j)
		{
			Direct3d11_ReflectedVSOutput const &o = vsOutputs[j];
			if (_stricmp(o.SemanticName, "FOG") != 0) continue;
			if (o.SemanticIndex != 0) continue;
			if (o.ComponentType != D3D_REGISTER_COMPONENT_FLOAT32) continue;
			if ((o.ComponentMask & 0xF) == 0) continue;
			fogVsIndex = static_cast<int>(j);
			break;
		}
		bool const fogFieldIsScalar = (fogVsIndex >= 0)
			&& (((vsOutputs[fogVsIndex].ComponentMask & 0xF) & ((vsOutputs[fogVsIndex].ComponentMask & 0xF) - 1)) == 0);   // single bit set

		std::string mirror;
		mirror.reserve(1024);
		mirror += "\n\n// Plan 17-07 VS-mirror wrapper (register-base alignment; project_17_07_ps_register_base_offset).\n";
		if (fogVsIndex >= 0)
			mirror += "cbuffer PSFog1707 : register(b1) { float4 alphaTest1707; float4 textureFactor1707; float4 fogColor1707; };\n";
		mirror += "struct PsIn1707\n{\n";
		mirror += "    float4 f_pos1707 : SV_Position;\n";
		for (size_t j = 0; j < vsOutputs.size(); ++j)
		{
			Direct3d11_ReflectedVSOutput const &o = vsOutputs[j];
			std::string const type = hlslTypeFor(o.ComponentType, o.ComponentMask);
			char idx[16];   _snprintf_s(idx,   sizeof(idx),   _TRUNCATE, "%u",  o.SemanticIndex);
			char fname[24]; _snprintf_s(fname, sizeof(fname), _TRUNCATE, "f_%zu", j);
			mirror += "    "; mirror += type; mirror += " "; mirror += fname;
			mirror += " : "; mirror += o.SemanticName; mirror += idx; mirror += ";\n";
		}
		mirror += "};\n\n";
		mirror += "float4 main(PsIn1707 i1707) : SV_Target\n{\n    float4 c1707 = assetMain1707(";
		for (size_t k = 0; k < params.size(); ++k)
		{
			if (k) mirror += ", ";
			if (params[k].isSystemValue)
			{
				mirror += "i1707.f_pos1707";
			}
			else if (_stricmp(params[k].semanticBase.c_str(), "COLOR") == 0)
			{
				// Plan 17-08 (GAP-5): the char-select VS computes per-vertex diffuse/
				// specular (COLOR0/COLOR1) from its OWN lighting constants. GAP-5
				// (Direct3d11_StateCache::composeSlot0Shadow) now feeds those VS lighting
				// constants (lightData/extendedLightData in VertexSlot0CB), so the VS
				// emits VALID vertex colors -- USE them for full per-vertex ambient
				// parity. NaN GUARD: if a residual NaN slips through (unfed edge case),
				// fall back to 0 -- per functions.inc vertexDiffuse is purely ADDITIVE,
				// so 0 yields clean per-pixel N.L from the GAP-4-fed b0 dot3 constants
				// (lit, never the NaN->black silhouette). HLSL broadcasts the scalar 0.
				// History: GAP-4 (pre-GAP-5) hard-passed 0 here as a beachhead.
				char arg[72]; _snprintf_s(arg, sizeof(arg), _TRUNCATE,
					"(any(isnan(i1707.f_%d)) ? 0 : i1707.f_%d)", paramToVs[k], paramToVs[k]);
				mirror += arg;
			}
			else
			{
				char arg[24]; _snprintf_s(arg, sizeof(arg), _TRUNCATE, "i1707.f_%d", paramToVs[k]);
				mirror += arg;
			}
		}
		mirror += ");\n";
		// Fog fix 2026-06-11: post-PS fog lerp (D3D9 pipeline order -- fog is
		// the last stage before blend). Gated on fogColor1707.w (enable) so
		// fog-disabled scenes pay one branch.
		if (fogVsIndex >= 0)
		{
			char fogLine[160];
			_snprintf_s(fogLine, sizeof(fogLine), _TRUNCATE,
				"    if (fogColor1707.w > 0.5) c1707.rgb = lerp(fogColor1707.rgb, c1707.rgb, saturate(i1707.f_%d%s));\n",
				fogVsIndex, fogFieldIsScalar ? "" : ".x");
			mirror += fogLine;
		}
		mirror += "    return c1707;\n}\n";

		// Rename the asset entry main -> assetMain1707 (so it is a plain function
		// defined before the wrapper), then append the mirror struct + wrapper.
		std::string const renamed =
			psrcText.substr(0, mainStart) + "assetMain1707" + psrcText.substr(mainStart + 4);
		result.text   = renamed + mirror;
		result.status = PsRewriteStatus::Rewritten;
		return result;
	}

	// Plan 17-07 (HIGH-4 rare-asset fallback): struct-bound main() form
	// `struct PSIn { ... }; float4 main(in PSIn input) : SV_Target`. ZERO
	// occurrences in the captured char-select PSRC dump (22/22 use the
	// parameter-list form handled above). A correct struct-member reorderer is
	// the same sort discipline applied to struct fields, but it cannot be
	// exercised or validated on the char-select beachhead (no struct-bound asset
	// boots it). Per Round-5's anti-"mental rewrite" stance we do NOT ship an
	// unvalidated reorderer that would silently run on a legacy asset; we return
	// Failed so the bind cleanly falls through to selectFallbackPSForVS (the
	// documented safety net for the residual 0-1/9 pair). When a real
	// struct-bound char-select asset surfaces, implement the field reorder here
	// mirroring rewritePsMainParameterListForVSOutputs.
	PsRewriteResult rewritePsMainStructBoundParameterForVSOutputs(
		std::string const &psrcText,
		std::vector<Direct3d11_ReflectedVSOutput> const &vsOutputsIn)
	{
		(void)vsOutputsIn;
		return PsRewriteResult{ PsRewriteStatus::Failed, psrcText };
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

ID3D11PixelShader *Direct3d11_PixelShaderProgramData::getOrCompilePSForVS(Direct3d11_VertexShaderData const *vsData, uint32 textureCoordinateSetKey, bool srv0Bound, Direct3d11_FfpCombinerDesc const *ffpDesc)
{
	// Plan 11-09.13 Iter-4: lazy per-VS PS generation + cache. Keyed by
	// VS bytecode hash so identical bytecode shares a PS. Cache miss
	// triggers HLSL generation from reflected outputs + D3DCompile;
	// compile failure tombstones the entry (null ComPtr stored) so
	// subsequent calls hit the cache without retrying. Caller (StateCache
	// selectFallbackPSForVS) falls back to ms_fallbackPS on null return.
	if (!vsData)
		return nullptr;

	// Plan 17-09: resolve the per-key VS variant (cached after the bind path's
	// first getVariant). Outputs are key-invariant, but keying ms_perVSCache by
	// THIS variant's bytecode hash keeps the per-VS PS coherent with the exact
	// variant bound for the draw.
	Direct3d11_VertexShaderData::Variant const &vsVariant = vsData->getVariant(textureCoordinateSetKey);
	uint64_t const hash = vsVariant.bytecodeHash;
	// Walls fix 2026-06-11: a VS can be drawn both with and without a
	// texture at SRV0 (FFP stage-based passes bind per-pass; some passes
	// have no texture). The textured and untextured generated PSes differ
	// (sample-modulate vs vertex-color), so each gets its own cache entry --
	// salt the untextured key. Same VS + same srv0Bound -> same entry.
	//
	// Detail-blend fix 2026-06-11: FFP combiner cascades additionally key on
	// the combiner hash -- the same VS is shared by many .shts with different
	// stage cascades (op/arg/texcoord permutations).
	uint64_t cacheKey = srv0Bound ? hash : (hash ^ 0x5EBAF00D5EBAF00Dull);
	if (ffpDesc)
		cacheKey ^= 0x9E3779B97F4A7C15ull * (static_cast<uint64_t>(ffpDesc->hash) | 1ull);
	auto it = ms_perVSCache.find(cacheKey);
	if (it != ms_perVSCache.end())
		return it->second.Get();   // hit (may be null tombstone)

	std::vector<Direct3d11_ReflectedVSOutput> const &outputs = vsVariant.reflectedOutputs;

	// No outputs (reflection failed or VS is degenerate) -> tombstone.
	// Empty PS generator output (validation failure) -> tombstone.
	// Compile failure -> tombstone. All paths cache-miss-then-tombstone
	// to avoid retry storms across thousands of draws using the same VS.
	//
	// Detail-blend fix 2026-06-11: try the FFP cascade first; on
	// unsupported-feature rejection (empty string) fall back to the
	// single-texture builder so previously-rendering draws never regress
	// to magenta.
	bool builtFfpCascade = false;
	std::string hlsl;
	if (ffpDesc)
	{
		hlsl = buildHlslForVSOutputsFfp(outputs, *ffpDesc);
		builtFfpCascade = !hlsl.empty();
	}
	if (hlsl.empty())
		hlsl = buildHlslForVSOutputs(outputs, srv0Bound);
	if (hlsl.empty())
	{
		ms_perVSCache[cacheKey] = ComPtr<ID3D11PixelShader>();
		return nullptr;
	}

	emitFirstGeneratorLog(cacheKey, hlsl);

	char displayName[64];
	snprintf(displayName, sizeof(displayName), "perVS_dyn_%016llX%s%s.hlsl",
	         static_cast<unsigned long long>(hash),
	         srv0Bound ? "" : "_notex",
	         builtFfpCascade ? "_ffp" : "");

	ComPtr<ID3D11PixelShader> ps;
	bool const compiled = compilePixelShaderFromHlsl(
		hlsl.c_str(), hlsl.size(), displayName, ps);

	if (!compiled || !ps)
	{
		ms_perVSCache[cacheKey] = ComPtr<ID3D11PixelShader>();
		return nullptr;
	}

	ms_perVSCache[cacheKey] = ps;
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

				// Plan 17-04.X DIAGNOSTIC: dump retained HLSL PSRC source text to
				// disk, one-shot per unique fileName. The post-Plan-17-04 boot
				// surfaced that the reflected SwgVertexConstants cbuffer has
				// generic packedRegister0-4 variable names instead of the
				// material[N] / materialDiffuse / materialSpecular / materialEmissive
				// names Plan 17-04 Task 2 assumed. The actual semantic mapping
				// (which packedRegisterN corresponds to which D3D9 c-register
				// content) lives in the asset PSRC source, so we dump it here for
				// inspection. File sink: stage/plan-17-04x-psrc-source-dump.txt
				// (resolves to stage/stage/... per cwd convention). Per-fileName
				// one-shot via static map. Bounded by the 22 unique HLSL PS
				// programs from Plan 17-01's census. REMOVE this block once the
				// mapping is decoded into writeVarByName.
				{
					static std::set<std::string> s_dumpedFileNames;
					char const * const fileName = pixelShaderProgram.getFileName();
					std::string const key = fileName ? fileName : "(null)";
					if (s_dumpedFileNames.find(key) == s_dumpedFileNames.end())
					{
						s_dumpedFileNames.insert(key);
						FILE *fp = nullptr;
						fopen_s(&fp, "stage/plan-17-04x-psrc-source-dump.txt", "ab");
						if (fp)
						{
							char header[512];
							int const headerLen = _snprintf_s(header, sizeof(header), _TRUNCATE,
								"\n=== PSRC SOURCE FOR '%s' (sourceLen=%zu) ===\n",
								key.c_str(), sourceLen);
							if (headerLen > 0)
								fwrite(header, 1, static_cast<size_t>(headerLen), fp);
							fwrite(psrcText, 1, sourceLen, fp);
							static char const footer[] = "\n=== END ===\n";
							fwrite(footer, 1, sizeof(footer) - 1, fp);
							fflush(fp);
							fclose(fp);
						}
					}
				}

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

							// Plan 17-09 (bump-arm texture-swap fix): build the stage->SRV-slot
							// remap from the reflected texture + sampler bindings. The engine
							// binds SRVs by D3D9 stage index, but fxc assigns SM4 texture
							// registers by first-USE order (not the pinned sampler/stage order)
							// when splitting legacy sampler2D -- so for multi-texture PSes the
							// texture register != stage. Pair each texture with its sampler BY
							// NAME (fxc names both after the source `sampler NAME`): the SRV slot
							// for stage S is the bind point of the texture whose name matches the
							// sampler at register S. Identity when a stage has no matching pair.
							{
								char  samplerNameByReg[8][128] = {};
								bool  samplerPresent[8]        = {};
								struct TexBind { char name[128]; UINT bindPoint; };
								std::vector<TexBind> texBinds;
								for (UINT r = 0; r < shaderDesc.BoundResources; ++r)
								{
									D3D11_SHADER_INPUT_BIND_DESC rb = {};
									if (FAILED(reflector->GetResourceBindingDesc(r, &rb)) || !rb.Name)
										continue;
									if (rb.Type == D3D_SIT_SAMPLER)
									{
										if (rb.BindPoint < 8)
										{
											strncpy_s(samplerNameByReg[rb.BindPoint], 128, rb.Name, _TRUNCATE);
											samplerPresent[rb.BindPoint] = true;
										}
									}
									else if (rb.Type == D3D_SIT_TEXTURE)
									{
										TexBind tb = {};
										strncpy_s(tb.name, 128, rb.Name, _TRUNCATE);
										tb.bindPoint = rb.BindPoint;
										texBinds.push_back(tb);
									}
								}
								for (int s = 0; s < 8; ++s)
								{
									if (!samplerPresent[s])
										continue;
									for (TexBind const &tb : texBinds)
									{
										if (std::strcmp(tb.name, samplerNameByReg[s]) == 0)
										{
											m_srvSlotForStage[s] = static_cast<int>(tb.bindPoint);
											break;
										}
									}
									if (m_srvSlotForStage[s] >= 0 && m_srvSlotForStage[s] != s)
									{
										char msg[256];
										_snprintf_s(msg, sizeof(msg), _TRUNCATE,
											"PS-srv-remap shader='%s' stage=%d -> srvSlot=%d (sampler='%s')",
											pixelShaderProgram.getFileName(), s, m_srvSlotForStage[s],
											samplerNameByReg[s]);
										if (iq) iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, msg);
										DEBUG_REPORT_LOG_PRINT(true, ("%s\n", msg));
									}
								}
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
//
// Plan 17-04 Task 1: VS<->PS signature pair compatibility gate.
//
// Walks the PS's reflected input signature (user semantics; Plan 17-02
// already filters SV_* at population time) and asserts each input is
// satisfiable by a VS output at the SAME hardware register slot with
// matching (SemanticName, SemanticIndex), matching component type, and a
// VS write mask that is a SUPERSET of the PS read mask.
//
// Returns false on any mismatch -- the caller (Direct3d11_StateCache::
// applyPreDrawState) then routes through selectFallbackPSForVS, whose
// Iter-3 buildHlslForVSOutputs-built PS mirrors the VS output signature
// register-for-register and is therefore compatible by construction.
//
// Per-(vs, ps) result is logged ONCE via DEBUG_REPORT_LOG_PRINT + the
// ID3D11InfoQueue file sink (R3-02b dual-route precedent). The static
// dedupe set keys on the (VS*, PS*) pair pointers; both pointers outlive
// any draw that consults the validator (m_d3dVS / m_d3dPS live on the
// per-template graphics-data objects), so the keys remain stable.
//
// Defensive-null behavior:
//   * vs == nullptr -> return true (no VS to validate; let asset PS bind).
//   * ps == nullptr -> caller-side assertion failure (deferred to NOT_NULL
//     at the callsite); avoid here to keep the validator a pure observer.
//   * ps has empty reflected inputs (Plan 17-02 reflection failed or PS
//     has no user-varying inputs) -> trivially compatible (no inputs to
//     mismatch).
//   * vs has empty reflected outputs -> incompatible IF ps has any user
//     inputs (PS expects inputs the VS doesn't provide). Empty PS inputs
//     against empty VS outputs is compatible.

bool Direct3d11_PixelShaderProgramData::isCompatibleWithVS(
	Direct3d11_VertexShaderData       const *vs,
	uint32                                   textureCoordinateSetKey,
	Direct3d11_PixelShaderProgramData const *ps)
{
	if (!ps)
		return false;   // defensive; callsite is expected to pre-check, but don't crash here.
	if (!vs)
		return true;    // pre-17-04 default: no VS-side data to compare; let asset PS bind.

	std::vector<Direct3d11_ReflectedPSInputSig>  const &psInputs  = ps->getReflectedPSInputs();
	std::vector<Direct3d11_ReflectedVSOutput>    const &vsOutputs = vs->getVariant(textureCoordinateSetKey).reflectedOutputs;   // Plan 17-09

	// Static dedupe set so we emit at most one log line per (VS, PS) pair
	// across the entire process lifetime. Keys on raw pointers because the
	// reflected program-data objects are template-graphics-data singletons
	// (allocated by MemoryBlockManager + retained until plugin remove).
	static std::set<std::pair<void const *, void const *>> s_loggedPairs;
	std::pair<void const *, void const *> const key(
		static_cast<void const *>(vs),
		static_cast<void const *>(ps));
	bool const firstSeen = (s_loggedPairs.find(key) == s_loggedPairs.end());
	if (firstSeen)
		s_loggedPairs.insert(key);

	// Walk PS inputs; every input must be satisfiable. Trivial-compat early
	// exit: PS declares no user inputs (e.g. magenta fallback PS) -> nothing
	// to mismatch, return true. This path bypasses the empty-VS-outputs
	// branch below because there's no PS demand for VS supply.
	if (psInputs.empty())
	{
		if (firstSeen)
		{
			char msg[256];
			_snprintf_s(msg, sizeof(msg), _TRUNCATE,
				"Plan 17-04 Task 1 VS<->PS pair compatibility: COMPATIBLE (ps has no user-varying inputs) vs=%p ps=%p",
				static_cast<void const *>(vs), static_cast<void const *>(ps));
			if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
				iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, msg);
			DEBUG_REPORT_LOG_PRINT(true, ("%s\n", msg));
		}
		return true;
	}

	// PS has demands and VS supplies nothing -> structurally incompatible.
	// Caught here rather than failing the loop below so the log message is
	// specific.
	if (vsOutputs.empty())
	{
		if (firstSeen)
		{
			char msg[384];
			_snprintf_s(msg, sizeof(msg), _TRUNCATE,
				"Plan 17-04 Task 1 VS<->PS pair compatibility: INCOMPATIBLE vs=%p ps=%p reason='ps has %u user inputs but vs reflection produced 0 outputs (reflection failed at compile time or vs is degenerate)'",
				static_cast<void const *>(vs), static_cast<void const *>(ps),
				static_cast<unsigned>(psInputs.size()));
			if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
				iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, msg);
			DEBUG_REPORT_LOG_PRINT(true, ("%s\n", msg));
		}
		return false;
	}

	for (Direct3d11_ReflectedPSInputSig const &psIn : psInputs)
	{
		// Locate VS output matching THIS PS input by (SemanticName,
		// SemanticIndex). _stricmp matches D3D11's semantic comparison
		// (HLSL semantics are case-insensitive per the HLSL spec).
		Direct3d11_ReflectedVSOutput const *vsOutMatch = nullptr;
		for (Direct3d11_ReflectedVSOutput const &vsOut : vsOutputs)
		{
			if (_stricmp(vsOut.SemanticName, psIn.SemanticName) != 0) continue;
			if (vsOut.SemanticIndex != psIn.SemanticIndex)            continue;
			vsOutMatch = &vsOut;
			break;
		}

		// Per-mismatch reason string; populated by the first failed check
		// so the log line cites a concrete failure rather than "incompatible".
		char const *mismatchReason = nullptr;
		char        reasonBuf[256];

		if (!vsOutMatch)
		{
			_snprintf_s(reasonBuf, sizeof(reasonBuf), _TRUNCATE,
				"ps input '%s%u' has no matching vs output semantic",
				psIn.SemanticName, psIn.SemanticIndex);
			mismatchReason = reasonBuf;
		}
		else if (vsOutMatch->Register != psIn.Register)
		{
			_snprintf_s(reasonBuf, sizeof(reasonBuf), _TRUNCATE,
				"ps input '%s%u' expects register v%u but vs writes semantic at register o%u (D3D11 stage linkage is register-position-strict; id=343)",
				psIn.SemanticName, psIn.SemanticIndex,
				psIn.Register, vsOutMatch->Register);
			mismatchReason = reasonBuf;
		}
		else if (vsOutMatch->ComponentType != psIn.ComponentType)
		{
			_snprintf_s(reasonBuf, sizeof(reasonBuf), _TRUNCATE,
				"ps input '%s%u' component type %d does not match vs output component type %d (e.g. float vs int)",
				psIn.SemanticName, psIn.SemanticIndex,
				static_cast<int>(psIn.ComponentType),
				static_cast<int>(vsOutMatch->ComponentType));
			mismatchReason = reasonBuf;
		}
		else if ((vsOutMatch->ComponentMask & psIn.Mask) != psIn.Mask)
		{
			// PS read mask must be a subset of VS write mask. Use VS
			// ComponentMask (declared signature contract) rather than
			// ReadWriteMask (which components VS actually wrote) -- D3D11
			// validates against the declared signature.
			_snprintf_s(reasonBuf, sizeof(reasonBuf), _TRUNCATE,
				"ps input '%s%u' read mask 0x%X is not a subset of vs write mask 0x%X at register %u",
				psIn.SemanticName, psIn.SemanticIndex,
				psIn.Mask, vsOutMatch->ComponentMask, psIn.Register);
			mismatchReason = reasonBuf;
		}

		if (mismatchReason)
		{
			if (firstSeen)
			{
				char msg[512];
				_snprintf_s(msg, sizeof(msg), _TRUNCATE,
					"Plan 17-04 Task 1 VS<->PS pair compatibility: INCOMPATIBLE vs=%p ps=%p reason='%s'",
					static_cast<void const *>(vs), static_cast<void const *>(ps),
					mismatchReason);
				if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
					iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, msg);
				DEBUG_REPORT_LOG_PRINT(true, ("%s\n", msg));
			}
			return false;
		}
	}

	// Every PS input matched a VS output by (semantic, register, type,
	// mask-subset). Asset PS is safe to bind.
	if (firstSeen)
	{
		char msg[256];
		_snprintf_s(msg, sizeof(msg), _TRUNCATE,
			"Plan 17-04 Task 1 VS<->PS pair compatibility: COMPATIBLE vs=%p ps=%p (matched %u ps inputs against vs outputs)",
			static_cast<void const *>(vs), static_cast<void const *>(ps),
			static_cast<unsigned>(psInputs.size()));
		if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
			iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, msg);
		DEBUG_REPORT_LOG_PRINT(true, ("%s\n", msg));
	}
	return true;
}

// ----------------------------------------------------------------------
//
// Plan 17-07 (HIGH-6 + Round-5 item 1): rewritten-lane validator. Mirrors
// isCompatibleWithVS above but reads an EXPLICIT per-VS reflected-input vector
// (the rewritten PS's signature) instead of the ctor-time m_reflectedPSInputs
// (the native PS's signature). Emits a DISTINCT "Plan 17-07 COMPATIBLE/
// INCOMPATIBLE vs=... ps=... (rewritten)" verdict so Plan 17-05's
// `Plan 17-07 .* COMPATIBLE vs=` grep measures the rewritten verdict, NOT the
// native `Plan 17-04 Task 1 ... COMPATIBLE (...) vs=` token. Deduped per
// (vs, psForLog) pair. The native isCompatibleWithVS above is left UNCHANGED.

bool Direct3d11_PixelShaderProgramData::isCompatibleWithVS_withExplicitPSInputs(
	Direct3d11_VertexShaderData                 const *vs,
	uint32                                             textureCoordinateSetKey,
	std::vector<Direct3d11_ReflectedPSInputSig> const &psInputs,
	void                                        const *psForLog)
{
	if (!vs)
		return true;   // no VS to validate against (mirror native-path default).

	std::vector<Direct3d11_ReflectedVSOutput> const &vsOutputs = vs->getVariant(textureCoordinateSetKey).reflectedOutputs;   // Plan 17-09

	static std::set<std::pair<void const *, void const *>> s_loggedPairs;
	std::pair<void const *, void const *> const key(static_cast<void const *>(vs), psForLog);
	bool const firstSeen = (s_loggedPairs.find(key) == s_loggedPairs.end());
	if (firstSeen)
		s_loggedPairs.insert(key);

	auto emitVerdict = [&](bool compatible, char const *detail)
	{
		if (!firstSeen) return;
		char msg[512];
		// Token shape "Plan 17-07 rewritten-lane (IN)COMPATIBLE vs=" so the
		// Plan 17-05 grep contract `Plan 17-07 .* COMPATIBLE vs=` matches the
		// rewritten verdict (the `.*` needs intervening text; a bare one-space
		// "Plan 17-07 COMPATIBLE vs=" would NOT match that regex).
		if (compatible)
			_snprintf_s(msg, sizeof(msg), _TRUNCATE,
				"Plan 17-07 rewritten-lane COMPATIBLE vs=0x%p ps=0x%p (matched %u ps inputs)",
				static_cast<void const *>(vs), psForLog,
				static_cast<unsigned>(psInputs.size()));
		else
			_snprintf_s(msg, sizeof(msg), _TRUNCATE,
				"Plan 17-07 rewritten-lane INCOMPATIBLE vs=0x%p ps=0x%p reason='%s'",
				static_cast<void const *>(vs), psForLog, detail ? detail : "");
		if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
			iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, msg);
		DEBUG_REPORT_LOG_PRINT(true, ("%s\n", msg));
	};

	if (psInputs.empty())
	{
		emitVerdict(true, nullptr);   // no user inputs to mismatch.
		return true;
	}
	if (vsOutputs.empty())
	{
		emitVerdict(false, "ps has user inputs but vs reflection produced 0 outputs");
		return false;
	}

	for (Direct3d11_ReflectedPSInputSig const &psIn : psInputs)
	{
		Direct3d11_ReflectedVSOutput const *match = nullptr;
		for (Direct3d11_ReflectedVSOutput const &o : vsOutputs)
		{
			if (_stricmp(o.SemanticName, psIn.SemanticName) != 0) continue;
			if (o.SemanticIndex != psIn.SemanticIndex)           continue;
			match = &o;
			break;
		}

		char        reasonBuf[256];
		char const *reason = nullptr;
		if (!match)
		{
			_snprintf_s(reasonBuf, sizeof(reasonBuf), _TRUNCATE,
				"ps input '%s%u' has no matching vs output semantic",
				psIn.SemanticName, psIn.SemanticIndex);
			reason = reasonBuf;
		}
		else if (match->Register != psIn.Register)
		{
			_snprintf_s(reasonBuf, sizeof(reasonBuf), _TRUNCATE,
				"ps input '%s%u' expects register v%u but vs writes at o%u",
				psIn.SemanticName, psIn.SemanticIndex, psIn.Register, match->Register);
			reason = reasonBuf;
		}
		else if (match->ComponentType != psIn.ComponentType)
		{
			_snprintf_s(reasonBuf, sizeof(reasonBuf), _TRUNCATE,
				"ps input '%s%u' component type %d != vs %d",
				psIn.SemanticName, psIn.SemanticIndex,
				static_cast<int>(psIn.ComponentType), static_cast<int>(match->ComponentType));
			reason = reasonBuf;
		}
		else if ((match->ComponentMask & psIn.Mask) != psIn.Mask)
		{
			_snprintf_s(reasonBuf, sizeof(reasonBuf), _TRUNCATE,
				"ps input '%s%u' read mask 0x%X not a subset of vs write mask 0x%X",
				psIn.SemanticName, psIn.SemanticIndex, psIn.Mask, match->ComponentMask);
			reason = reasonBuf;
		}

		if (reason)
		{
			emitVerdict(false, reason);
			return false;
		}
	}

	emitVerdict(true, nullptr);
	return true;
}

// ----------------------------------------------------------------------
//
// Plan 17-07 (HIGH-2 + HIGH-4 + HIGH-6): per-VS rewritten-PS lookup. Lazily
// rewrites the retained //hlsl PSRC main() parameter list into the bound VS's
// output-register order, recompiles (salted .cso key), reflects the new input
// signature, and caches the result keyed on (VS pointer + output-sig hash).
// const + mutable cache: lazy memoization, logically const (ms_currentPSData
// is a const* at the StateCache call site).

std::pair<ID3D11PixelShader *, std::vector<Direct3d11_ReflectedPSInputSig> const *>
Direct3d11_PixelShaderProgramData::tryGetOrBuildRewrittenPSForVS(Direct3d11_VertexShaderData const *vsData, uint32 textureCoordinateSetKey) const
{
	std::pair<ID3D11PixelShader *, std::vector<Direct3d11_ReflectedPSInputSig> const *> const kNone(nullptr, nullptr);
	if (!vsData)
		return kNone;

	Direct3d11_VertexShaderData::Variant const &vsVariant = vsData->getVariant(textureCoordinateSetKey);   // Plan 17-09 (outputs key-invariant)
	uint32_t const vsOutputSignatureHash = vsVariant.outputSigHash;

	// Cache lookup with raw-pointer-reuse staleness guard (MEDIUM): a stored
	// entry whose hash != the current VS hash is a MISS (rebuild), not a serve.
	{
		std::map<Direct3d11_VertexShaderData const *, PerVsRewriteEntry>::const_iterator const it =
			m_perVsRewrittenCache.find(vsData);
		if (it != m_perVsRewrittenCache.end() && it->second.vsOutputSignatureHash == vsOutputSignatureHash)
		{
			if (it->second.ps)
				return std::make_pair(it->second.ps.Get(), &it->second.inputs);
			return kNone;   // tombstone: rewrite previously infeasible for this VS.
		}
	}

	// Tombstone helper: records a null entry (with the current hash) so repeated
	// binds of an infeasible pair don't re-run the rewrite+compile every draw.
	// operator[] overwrites any stale (hash-mismatched) entry found above.
	auto tombstone = [&](char const *why) -> std::pair<ID3D11PixelShader *, std::vector<Direct3d11_ReflectedPSInputSig> const *>
	{
		PerVsRewriteEntry &slot = m_perVsRewrittenCache[vsData];
		slot.ps.Reset();
		slot.inputs.clear();
		slot.vsOutputSignatureHash = vsOutputSignatureHash;
		// One-shot diagnostic (the result is cached, so this fires ~once per VS):
		// surfaces WHICH guard tripped for pairs that produce no rewritten verdict.
		char dm[256];
		_snprintf_s(dm, sizeof(dm), _TRUNCATE,
			"Plan 17-07 rewrite TOMBSTONE vs=0x%p shader='%s' reason='%s'",
			static_cast<void const *>(vsData),
			(m_program ? m_program->getFileName() : "?"), why ? why : "?");
		if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
			iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, dm);
		DEBUG_REPORT_LOG_PRINT(true, ("%s\n", dm));
		return kNone;
	};

	// Need the retained PSRC //hlsl source (Plan 17-01 m_psrcText on the engine
	// program struct).
	char const *psrcText = (m_program ? m_program->m_psrcText : nullptr);
	int  const  psrcLen  = (m_program ? m_program->m_psrcLen  : 0);
	if (!psrcText || psrcLen < 7)
		return tombstone("retained PSRC text missing or too short (Plan 17-01 m_psrcText)");

	// Only the //hlsl PSRC lane is rewritable (mirror the ctor classification:
	// first non-whitespace, BOM-tolerant, must be "//hlsl"). asm PSRC is out of
	// scope (re-assembling asm reproduces the rejected D3D9 bytecode).
	{
		int start = 0;
		if (psrcLen >= 3
			&& static_cast<unsigned char>(psrcText[0]) == 0xEF
			&& static_cast<unsigned char>(psrcText[1]) == 0xBB
			&& static_cast<unsigned char>(psrcText[2]) == 0xBF)
			start = 3;
		while (start < psrcLen)
		{
			char const c = psrcText[start];
			if (c == ' ' || c == '\t' || c == '\r' || c == '\n') ++start; else break;
		}
		if (psrcLen - start < 7)
			return tombstone("PSRC after-whitespace remainder too short for //hlsl");
		if (!(psrcText[start + 0] == '/' && psrcText[start + 1] == '/'
			&& psrcText[start + 2] == 'h' && psrcText[start + 3] == 'l'
			&& psrcText[start + 4] == 's' && psrcText[start + 5] == 'l'))
			return tombstone("PSRC first non-whitespace token is not //hlsl (asm or other prefix)");
	}

	std::string const psrcStr(psrcText);   // strlen-derived (ctor uses the same)

	PsRewriteResult rr = rewritePsMainParameterListForVSOutputs(psrcStr, vsVariant.reflectedOutputs);   // Plan 17-09
	if (rr.status == PsRewriteStatus::Failed)
	{
		PsRewriteResult sb = rewritePsMainStructBoundParameterForVSOutputs(psrcStr, vsVariant.reflectedOutputs);
		if (sb.status == PsRewriteStatus::Failed)
			return tombstone("param-list rewriter Failed AND struct-bound rewriter Failed (no usable main() reorder)");
		rr = sb;
	}
	// rr is now Rewritten or Unchanged -> rr.text is compilable.

	ComPtr<ID3D11PixelShader> ps;
	ComPtr<ID3DBlob>          blob;
	bool const ok = tryCompilePixelShaderFromHlslNoFatal(
		rr.text.c_str(), rr.text.size(),
		(m_program ? m_program->getFileName() : "pixel_shader.psh"),
		ps, blob, vsOutputSignatureHash);
	if (!ok || !ps || !blob)
		return tombstone("rewritten PSRC D3DCompile/CreatePixelShader failed (non-fatal)");

	// Reflect the rewritten PS's input signature (mirror the ctor reflect-once
	// loop at :1061-1092; SV_* filtered). Reflection failure is non-fatal ->
	// empty inputs -> validator treats as trivially compatible.
	PerVsRewriteEntry entry;
	entry.ps                    = ps;
	entry.vsOutputSignatureHash = vsOutputSignatureHash;

	ComPtr<ID3D11ShaderReflection> reflector;
	HRESULT const reflectHr = D3DReflect(
		blob->GetBufferPointer(), blob->GetBufferSize(),
		IID_PPV_ARGS(reflector.GetAddressOf()));
	if (SUCCEEDED(reflectHr) && reflector)
	{
		D3D11_SHADER_DESC shaderDesc = {};
		if (SUCCEEDED(reflector->GetDesc(&shaderDesc)))
		{
			for (UINT i = 0; i < shaderDesc.InputParameters; ++i)
			{
				D3D11_SIGNATURE_PARAMETER_DESC desc = {};
				if (FAILED(reflector->GetInputParameterDesc(i, &desc))) continue;
				if (desc.SystemValueType != D3D_NAME_UNDEFINED) continue;   // SV_* filtered
				Direct3d11_ReflectedPSInputSig sig = {};
				if (desc.SemanticName)
					strncpy_s(sig.SemanticName, sizeof(sig.SemanticName), desc.SemanticName, _TRUNCATE);
				sig.SemanticIndex = desc.SemanticIndex;
				sig.Register      = desc.Register;
				sig.Mask          = desc.Mask;
				sig.ComponentType = desc.ComponentType;
				entry.inputs.push_back(sig);
			}
		}
	}

	// DECISIVE DIAGNOSTIC (Plan 17-07 Task-0-spike-equivalent, deferred to boot):
	// log the rewrite status + the rewritten PS's reflected input registers vs
	// the VS output registers. If a reordered COLOR0 still reflects v0 while the
	// VS writes it at o1, the parameter-list reorder did NOT move the input
	// register (legacy semantic->register pinning under the current compile
	// flags), proving axis-(b) cannot fix the linkage as-built. One-shot per VS
	// (the result is cached below, so the build path runs ~once per VS).
	{
		char const *statusStr =
			(rr.status == PsRewriteStatus::Rewritten) ? "Rewritten" :
			(rr.status == PsRewriteStatus::Unchanged) ? "Unchanged" : "Failed";
		std::string psInStr;
		for (Direct3d11_ReflectedPSInputSig const &s : entry.inputs)
		{
			char b[48];
			_snprintf_s(b, sizeof(b), _TRUNCATE, "%s%u=v%u ", s.SemanticName, s.SemanticIndex, s.Register);
			psInStr += b;
		}
		std::string vsOutStr;
		for (Direct3d11_ReflectedVSOutput const &o : vsVariant.reflectedOutputs)   // Plan 17-09
		{
			char b[48];
			_snprintf_s(b, sizeof(b), _TRUNCATE, "%s%u=o%u ", o.SemanticName, o.SemanticIndex, o.Register);
			vsOutStr += b;
		}
		char head[192];
		_snprintf_s(head, sizeof(head), _TRUNCATE,
			"Plan 17-07 rewrite-diag vs=0x%p shader='%s' status=%s",
			static_cast<void const *>(vsData), (m_program ? m_program->getFileName() : "?"), statusStr);
		if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
		{
			iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, head);
			std::string const detail = std::string("Plan 17-07 rewrite-diag   psIn: ") + psInStr + "| vsOut: " + vsOutStr;
			iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, detail.c_str());
		}
		DEBUG_REPORT_LOG_PRINT(true, ("%s\n  psIn: %s\n  vsOut: %s\n", head, psInStr.c_str(), vsOutStr.c_str()));
	}

	// Defensive (Round-4 HIGH-6): only cache a rewrite that actually VALIDATES
	// against the bound VS via its per-VS reflected inputs. A rewrite that
	// compiled but still mismatches (e.g. the HLSL compiler packed inputs
	// differently than declaration order assumed) is tombstoned so the bind
	// falls through to selectFallbackPSForVS rather than binding a PS that reads
	// garbage. This emits the rewritten-lane verdict log; the StateCache re-check
	// shares the (vs, ps) dedupe key so the log fires exactly once per pair.
	if (!isCompatibleWithVS_withExplicitPSInputs(vsData, textureCoordinateSetKey, entry.inputs, static_cast<void const *>(ps.Get())))
		return tombstone("rewritten PS recompiled+reflected but still validated INCOMPATIBLE (see rewrite-diag registers)");

	// Store (overwriting any stale hash-mismatched entry). std::map element
	// addresses are stable across later inserts, so returning &slot.inputs is safe.
	PerVsRewriteEntry &slot = m_perVsRewrittenCache[vsData];
	slot = std::move(entry);
	if (slot.ps)
		return std::make_pair(slot.ps.Get(), &slot.inputs);
	return kNone;
}

// ----------------------------------------------------------------------

Direct3d11_PixelShaderProgramData::~Direct3d11_PixelShaderProgramData()
{
	m_program = nullptr;
}

// ======================================================================
