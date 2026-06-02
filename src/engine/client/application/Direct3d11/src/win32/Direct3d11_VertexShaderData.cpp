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
// Profile-rationale (Plan 11-07 Iter-3 + Plan 11-09.6):
//   The engine's stock .vsh sources were authored against vs_1_1 / vs_2_0
//   / vs_3_0 and ship as HLSL text inside TRE archives. Under vs_5_0
//   (D3DCompile's strictest profile), the compiler reserves additional
//   identifiers as keywords (geometry-shader primitive type names like
//   `point`, `line`, `triangle`, etc.). Iter-2 smoke surfaced an X3000
//   "syntax error: unexpected token 'point'" at line 81 of
//   `vertex_program/include/vertex_shader_constants.inc` -- the engine's
//   D3D9-era HLSL uses `point` as a sampler-state filter literal and/or
//   identifier that vs_5_0 reserves. The `vs_4_*` and `vs_4_0_level_9_*`
//   profiles target D3D11 bytecode but relax those keyword reservations
//   and accept the legacy syntax. D3D11's CreateVertexShader accepts
//   either form. SPEC R3 "HLSL SM5.0 recompilation" is satisfied in
//   spirit: the build is produced by the D3D11 toolchain (d3dcompiler_47
//   + ID3D11Device), even though the chosen profile is a non-SM5
//   profile -- which is the only path that consumes the existing asset
//   content. A future Phase 12 asset re-author would re-introduce
//   vs_5_0 once the source text is modernized.
//
//   Plan 11-07 Iter-3 originally chose `vs_4_0_level_9_3` over
//   `vs_4_0_level_9_1` because Plan 11-01 Phase A static analysis recorded
//   engine .vsh tags up to `vs_2_0` -- level_9_3 maps to SM3-equivalent
//   feature constraints (256 const registers + relative addressing),
//   comfortably covering what the engine's HLSL bodies declare.
//
//   Plan 11-09.6 bumps `vs_4_0_level_9_3` -> plain `vs_4_0`. The level_9
//   profile bucket enforces SM2.x-style limits including the
//   256-instruction-slot ceiling per fxc target docs. First instance of
//   the cap busting surfaced on Plan 11-09.5 char-create smoke:
//   `vertex_program/a_specmap_bump_vs20_for_ps20.vsh` compiles to 293
//   instructions, busting the 256 cap by 37 slots with X5615. Plain
//   `vs_4_0` keeps the relaxed legacy-keyword treatment (vs_5_0 issues
//   still avoided) while dropping the level_9 instruction-slot cap
//   (vs_4_0 envelope is ~4096 slots, comfortable headroom). CODEX
//   pre-implementation consult (.planning/phases/11-d3d11-renderer-plugin/
//   11-09.6-CODEX-CONSULT-shader-target-bump.md) endorsed `vs_4_0` over
//   `vs_5_0` and rejected fallback-ladder patterns.
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
#include "Direct3d11_HlslRewrite.h"
#include "Direct3d11_ShaderCache.h"

#include "sharedFoundation/MemoryBlockManager.h"

#include <algorithm>      // Plan 17-07: std::sort for output-signature-hash determinism
#include <cstdint>        // Plan 17-07: uint32_t hash
#include <cstdio>         // Plan 17-09: _snprintf_s for generated macro strings
#include <cstring>
#include <set>            // Phase 19: one-shot-per-filename fallback telemetry dedupe
#include <string>         // Plan 17-09: generated per-key macro strings (kept alive across D3DCompile)
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
	// Plan 11-09.6: vs_4_0 profile string. Centralized constant so the
	// D3DCompile call site, the D3D_SHADER_MACRO cache-key injection,
	// and any future audit point all read the same string. See the
	// file-preamble Profile-rationale block for the Plan 11-07 Iter-3 ->
	// Plan 11-09.6 (vs_4_0_level_9_3 -> vs_4_0) bump reasoning and the
	// reason vs_5_0 is still avoided.
	char const * const kVertexShaderProfile = "vs_4_0";

	// Plan 17-09: texcoord-set tag parsing, mirroring
	// Direct3d9_VertexShaderData (getToken / skipRestOfTheLine / TAG_DOT3).
	// The .vsh header carries leading `#define textureCoordinateSet<TAG>
	// textureCoordinateSet<N>` lines that name the shader's texcoord inputs.
	// D3D9 strips those defaults and regenerates them per-key; we mirror that.
	Tag const TAG_DOT3_local = TAG(D,O,T,3);

	// Copy the next whitespace-delimited token into d (mirror D3D9 getToken).
	void getToken(char const *&s, char *d)
	{
		if (s == nullptr || *s == '\0') { *d = '\0'; return; }
		while (*s && (*s == ' ' || *s == '\r' || *s == '\n' || *s == '\t'))
			++s;
		for ( ; *s && *s != ' ' && *s != '\n' && *s != '\r' && *s != '\t'; ++s, ++d)
			*d = *s;
		*d = '\0';
	}

	// Advance s past the rest of the current line, honoring backslash line
	// continuations (mirror D3D9 skipRestOfTheLine).
	void skipRestOfTheLine(char const *&s)
	{
		if (s == nullptr || *s == '\0') return;
		for ( ; *s && *s != '\n' && *s != '\r'; )
		{
			if (*s == '\\')
			{
				++s;
				if (*s == '\r') ++s;
				if (*s == '\n') ++s;
			}
			else
				++s;
		}
	}


	// Plan 17-09: header detection + texcoord-tag parsing + body-strip now lives
	// inline in the constructor (mirroring Direct3d9_VertexShaderData), using the
	// getToken / skipRestOfTheLine helpers above. The old parseHeader (//hlsl vs
	// //asm only, no #define strip) was removed -- the constructor's tokenizer
	// supersedes it.

	// ------------------------------------------------------------------
	// Phase 19: fallback VS for legacy //asm vertex programs.
	//
	// D3DCompile cannot consume D3D9 vertex assembly, so interior/structural/
	// sky geometry that ships as `//asm` `.vsh` produced a NULL d3dVS -> the
	// draw was skipped (skippedNullVS / [P19SKIP]) -> cell walls/floor/ceiling
	// + sky never rendered (handoff facet 5). The PIXEL path already has a
	// fallback (Phase 17: selectFallbackPSForVS -> buildHlslForVSOutputs); the
	// VERTEX path had none. This generic HLSL VS restores the symmetry: it
	// applies the standard object->clip WVP (objectWorldCameraProjectionMatrix
	// @ b0 c0 -- the exact transform3d() the //hlsl VS use, row-vector mul +
	// PACK_MATRIX_ROW_MAJOR to match the transposed upload) and forwards
	// TEXCOORD0, emitting a CONST WHITE COLOR0 so the reflection-driven
	// fallback PS samples t0/s0 * white = the diffuse texture. Design:
	// .planning/research/CONSULT-19-vs-fallback-interior-asm-SYNTHESIS.md
	// (CODEX + Cursor converged, code-grounded).
	//
	// Input is MINIMAL (POSITION0 + TEXCOORD0 only): missing semantics are
	// backed by phantom-zero elements at slot 15 (augmentWithPhantomElements).
	// COLOR0 is EMITTED const-white, never DECLARED as an input -- a mesh
	// without vertex color would otherwise feed COLOR0=0 and the PS would
	// multiply the texture to black.
	//
	// LIMITATIONS (first cut, by design -- the visibility bridge, not parity):
	//   * No lighting / baked vertex color -> flat / over-bright vs D3D9.
	//   * Only TEXCOORD0 forwarded -> lightmaps / detail UVs (TEXCOORD1+) lost.
	//   * Static-world transform ONLY: transformed/RHW geometry (e.g.
	//     gradient_sky, POSITION as XYZRHW) is double-transformed and will look
	//     wrong. A dedicated transformed/sky variant is the documented
	//     follow-up; the long-term fix is authoring //hlsl versions of the
	//     shared //asm `.vsh` set (restores lighting/lightmaps/sky).
	char const * const kAssemblyFallbackVSSource =
		"cbuffer SwgVertexConstants : register(b0)\n"
		"{\n"
		"    float4x4 objectWorldCameraProjectionMatrix : packoffset(c0);\n"
		"};\n"
		"struct VSIn  { float3 position : POSITION0; float2 tc0 : TEXCOORD0; };\n"
		"struct VSOut { float4 position : SV_POSITION; float2 tc0 : TEXCOORD0; float4 color : COLOR0; };\n"
		"VSOut main(VSIn input)\n"
		"{\n"
		"    VSOut o;\n"
		"    o.position = mul(float4(input.position, 1.0f), objectWorldCameraProjectionMatrix);\n"
		"    o.tc0      = input.tc0;\n"
		"    o.color    = float4(1.0f, 1.0f, 1.0f, 1.0f);\n"
		"    return o;\n"
		"}\n";

	// Compile the fallback VS into outBlob (+ outHash for the .cso cache key /
	// input-layout cache). Source + defines are key-INDEPENDENT, so every
	// texcoord-set key shares one cached blob. Returns false on compile failure
	// (caller leaves d3dVS null -> draw skipped, matching prior behavior); we
	// deliberately do NOT FATAL here (our own controlled source failing should
	// not crash a world load).
	bool compileAssemblyFallbackVS(char const *displayName, ComPtr<ID3DBlob> &outBlob, uint64_t &outHash)
	{
		std::vector<D3D_SHADER_MACRO> defines;
		defines.push_back({ "D3D11",             "1" });
		defines.push_back({ "D3D11_PROFILE",     kVertexShaderProfile });
		defines.push_back({ "D3D11_VS_FALLBACK", "1" });   // salts the cache key; distinguishes from any asset VS
		defines.push_back({ nullptr,             nullptr });

		size_t   const srcLen = std::strlen(kAssemblyFallbackVSSource);
		uint64_t const hash   = Direct3d11_ShaderCache::hashSource(
			kAssemblyFallbackVSSource, srcLen, defines.data());
		outHash = hash;

		ComPtr<ID3DBlob> blob;
		if (!Direct3d11_ShaderCache::tryLoad(hash, blob))
		{
			UINT flags = 0;
	#ifdef _DEBUG
			flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
	#else
			flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
	#endif
			flags |= D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY;
			flags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;   // match the transposed WVP upload (transform3d convention)

			char virtName[160];
			_snprintf_s(virtName, sizeof(virtName), _TRUNCATE,
				"vs_fallback(%s)", (displayName && *displayName) ? displayName : "asm");

			ComPtr<ID3DBlob> errors;
			HRESULT const hr = D3DCompile(
				kAssemblyFallbackVSSource, srcLen, virtName,
				defines.data(), nullptr /*no #include*/, "main", kVertexShaderProfile,
				flags, 0, blob.GetAddressOf(), errors.GetAddressOf());
			if (FAILED(hr) || !blob)
			{
				char const *err = errors ? static_cast<char const *>(errors->GetBufferPointer()) : "no error blob";
				DEBUG_REPORT_LOG_PRINT(true,
					("Direct3d11_VertexShaderData: FALLBACK VS compile FAILED for //asm '%s': %s\n",
					(displayName && *displayName) ? displayName : "asm", err));
				return false;
			}
			Direct3d11_ShaderCache::store(hash, blob.Get());
		}
		outBlob = blob;
		return true;
	}

	// One-shot-per-filename telemetry: which //asm .vsh now get the fallback VS
	// (handoff NEXT STEP 2: ".vsh filename telemetry on the asm path"). Dual
	// route (DEBUG_REPORT_LOG + InfoQueue) so it shows alongside [P19SKIP] in
	// the RenderDoc/info log. Census these names -> author //hlsl for the
	// shared set (the long-term correctness fix).
	void logAssemblyFallbackOnce(char const *displayName)
	{
		static std::set<std::string> s_logged;
		std::string const key = displayName ? displayName : "(null)";
		if (!s_logged.insert(key).second)
			return;
		DEBUG_REPORT_LOG_PRINT(true,
			("[P19VSFALLBACK] generic world-transform VS bound for //asm '%s'\n", key.c_str()));
		if (ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue())
		{
			char buf[320];
			_snprintf_s(buf, sizeof(buf), _TRUNCATE,
				"[P19VSFALLBACK] generic world-transform VS bound for //asm '%s'", key.c_str());
			iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, buf);
		}
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
	m_isAssembly(false),
	m_bodyText(nullptr),
	m_bodyLen(0)
{
	// Plan 17-09: parse the .vsh header EXACTLY like Direct3d9_VertexShaderData --
	// detect //hlsl vs //asm, collect the leading texcoord-set `#define` tags, and
	// set the compile body to AFTER the stripped leading //hlsl + #define block.
	// The stripped texcoord macros are regenerated per-key in compileVariant().
	{
		char const *text = vertexShader.m_text;
		char const *bodyStart = text;
		bool hlsl = false;
		for (bool done = false; !done; )
		{
			bodyStart = text;                 // position before the failing token (mirror D3D9 m_compileText)
			char token[128];
			getToken(text, token);
			if (std::strcmp(token, "//hlsl") == 0)
			{
				hlsl = true;
				skipRestOfTheLine(text);
			}
			else if (std::strcmp(token, "//asm") == 0)
			{
				m_isAssembly = true;
				skipRestOfTheLine(text);
			}
			else if (std::strcmp(token, "#define") == 0)
			{
				char defToken[128];
				getToken(text, defToken);
				char const *tag = nullptr;
				if (hlsl && std::strncmp(defToken, "textureCoordinateSet", 20) == 0)
					tag = defToken + 20;
				else if (m_isAssembly && std::strncmp(defToken, "vTextureCoordinateSet", 21) == 0)
					tag = defToken + 21;
				if (tag && *tag)
					m_textureCoordinateSetTags.push_back(ConvertStringToTag(tag));
				skipRestOfTheLine(text);
			}
			else
			{
				done = true;
			}
		}
		m_bodyText = bodyStart;
		m_bodyLen  = (bodyStart && *bodyStart) ? std::strlen(bodyStart) : 0;
	}

	if (m_isAssembly)
	{
		// D3D9-era vertex shader assembly (vs_1_1 / vs_2_0). D3DCompile does
		// not accept assembly; reassembly via D3DAssemble is gone from the
		// modern SDK. Phase 19: rather than leave variant.d3dVS NULL (which
		// made the draw-dispatch SKIP the pass -> interiors/structural/sky
		// invisible -- handoff facet 5), compileVariant() now binds a GENERIC
		// FALLBACK VS (world-transform + TEXCOORD0 + const-white COLOR0) for
		// the //asm case. We DON'T early-return here anymore; compileVariant's
		// m_isAssembly branch produces the fallback lazily per key, and the
		// draw path forces the matching fallback PS lane. The Plan 11-01 D-04a
		// expectation (cantina has zero //asm VS) proved WRONG: the interior
		// cell shell + sky use unconverted //asm `.vsh`.
		DEBUG_REPORT_LOG_PRINT(true,
			("Direct3d11_VertexShaderData: '%s' is //asm vs_*; no native D3D11 path -- Phase 19 generic fallback VS will be bound.\n",
			vertexShader.getFilename()));
		// fall through: variants (incl. the fallback) compile lazily in getVariant().
	}

	// Plan 17-09: variants are compiled lazily, per texcoord-set key, in getVariant().
}

// ----------------------------------------------------------------------

Direct3d11_VertexShaderData::~Direct3d11_VertexShaderData()
{
	m_vertexShader = nullptr;
	// ComPtr members release automatically.
}

// ----------------------------------------------------------------------
//
// Plan 17-07 (Round-5 review item 7): cache-key salt + raw-VS-pointer-reuse
// staleness guard for Direct3d11_PixelShaderProgramData's per-VS rewrite
// cache. FNV-1a over the sorted (SemanticName, SemanticIndex, Register,
// ComponentMask) tuple list of reflected outputs. Sorted first so the hash
// is order-independent over the reflected-output vector (reflection order is
// not guaranteed stable across runs). Returns 0 on no reflected outputs.

// Plan 17-09: file-scope helper (was a member); computes per-variant at compile.
static uint32_t computeOutputSignatureHashFor(std::vector<Direct3d11_ReflectedVSOutput> const &outputs)
{
	if (outputs.empty())
		return 0u;

	// Sort a local copy by (Register, SemanticIndex, SemanticName) for a
	// deterministic, order-independent hash input. Mirrors the sort discipline
	// in buildHlslForVSOutputs (Register-major).
	std::vector<Direct3d11_ReflectedVSOutput> sorted = outputs;
	std::sort(sorted.begin(), sorted.end(),
		[](Direct3d11_ReflectedVSOutput const &a, Direct3d11_ReflectedVSOutput const &b) {
			if (a.Register != b.Register)           return a.Register < b.Register;
			if (a.SemanticIndex != b.SemanticIndex) return a.SemanticIndex < b.SemanticIndex;
			return std::strcmp(a.SemanticName, b.SemanticName) < 0;
		});

	uint32_t hash = 2166136261u;                  // FNV-1a 32-bit offset basis
	auto fold = [&hash](unsigned char byte) {
		hash ^= byte;
		hash *= 16777619u;                        // FNV-1a 32-bit prime
	};
	auto foldUint = [&fold](UINT v) {
		fold(static_cast<unsigned char>( v        & 0xff));
		fold(static_cast<unsigned char>((v >> 8)  & 0xff));
		fold(static_cast<unsigned char>((v >> 16) & 0xff));
		fold(static_cast<unsigned char>((v >> 24) & 0xff));
	};

	for (Direct3d11_ReflectedVSOutput const &o : sorted)
	{
		for (char const *p = o.SemanticName; *p; ++p)
			fold(static_cast<unsigned char>(*p));
		fold(0);                                  // name terminator so "AB"+"C" != "A"+"BC"
		foldUint(o.SemanticIndex);
		foldUint(o.Register);
		foldUint(o.ComponentMask);
	}
	return hash;
}

// ----------------------------------------------------------------------
//
// Shared post-compile tail for BOTH the normal //hlsl path and the Phase 19
// //asm fallback path: disassembly dump (diagnostic), CreateVertexShader, and
// D3DReflect-driven input/output signature capture + output-sig hash. The
// caller must have already set variant.bytecode + variant.bytecodeHash. Reads
// the blob through variant.bytecode so the body below is identical to the
// historical inline tail (the local `blob` alias preserves it verbatim).

static void finalizeVariantFromBytecode(Direct3d11_VertexShaderData::Variant &variant, char const *displayName)
{
	ID3DBlob * const blob = variant.bytecode.Get();
	if (!blob)
		return;

	// Plan 11-09.15 Iter-7: dump disassembled VS bytecode to a single
	// append-only file for offline analysis. First compile per session
	// truncates the file; subsequent compiles append. Output: stage/vs-disasm-all.txt
	{
		static bool s_iter7FirstWrite = true;
		Microsoft::WRL::ComPtr<ID3DBlob> disasmBlob;
		HRESULT const disasmHr = D3DDisassemble(
			blob->GetBufferPointer(),
			blob->GetBufferSize(),
			0, nullptr, disasmBlob.GetAddressOf());
		if (SUCCEEDED(disasmHr) && disasmBlob)
		{
			char const * const mode = s_iter7FirstWrite ? "wb" : "ab";
			s_iter7FirstWrite = false;
			FILE *fp = nullptr;
			fopen_s(&fp, "stage/vs-disasm-all.txt", mode);
			if (fp)
			{
				char header[512];
				int const headerLen = _snprintf_s(header, sizeof(header), _TRUNCATE,
					"\n=== Plan 11-09.15 Iter-7 VS disassembly: %s hash=0x%016llX bytecode_size=%zu ===\n",
					displayName ? displayName : "<unknown>",
					static_cast<unsigned long long>(variant.bytecodeHash),
					blob->GetBufferSize());
				if (headerLen > 0)
					fwrite(header, 1, static_cast<size_t>(headerLen), fp);
				size_t const disasmSize = disasmBlob->GetBufferSize();
				size_t writeSize = disasmSize;
				if (writeSize > 0)
				{
					char const * const disasmText = static_cast<char const *>(disasmBlob->GetBufferPointer());
					if (disasmText[writeSize - 1] == '\0')
						--writeSize;
				}
				if (writeSize > 0)
					fwrite(disasmBlob->GetBufferPointer(), 1, writeSize, fp);
				fclose(fp);
			}
		}
	}

	// Create the ID3D11VertexShader from the bytecode blob.
	HRESULT const hr = Direct3d11_Device::getDevice()->CreateVertexShader(
		blob->GetBufferPointer(),
		blob->GetBufferSize(),
		nullptr,
		variant.d3dVS.GetAddressOf());
	FATAL_DX_HR("Direct3d11_VertexShaderData::CreateVertexShader failed: %s", hr);

	// Plan 11-09.8 / 11-09.13 Iter-3: capture VS input + output signatures via
	// D3DReflect. Reflection failure is treated as defensive non-fatal; both
	// reflectedInputs and reflectedOutputs stay empty.
	variant.reflectedInputs.clear();
	variant.reflectedOutputs.clear();
	ComPtr<ID3D11ShaderReflection> reflector;
	HRESULT const reflectHr = D3DReflect(
		blob->GetBufferPointer(),
		blob->GetBufferSize(),
		IID_PPV_ARGS(reflector.GetAddressOf()));
	if (SUCCEEDED(reflectHr) && reflector)
	{
		D3D11_SHADER_DESC shaderDesc = {};
		if (SUCCEEDED(reflector->GetDesc(&shaderDesc)))
		{
			variant.reflectedInputs.reserve(shaderDesc.InputParameters);
			for (UINT i = 0; i < shaderDesc.InputParameters; ++i)
			{
				D3D11_SIGNATURE_PARAMETER_DESC desc = {};
				if (FAILED(reflector->GetInputParameterDesc(i, &desc)))
					continue;
				// Filter SV_* system-value inputs -- auto-generated by the IA.
				if (desc.SystemValueType != D3D_NAME_UNDEFINED)
					continue;

				Direct3d11_ReflectedVSInput ri = {};
				if (desc.SemanticName)
				{
					strncpy_s(ri.SemanticName, sizeof(ri.SemanticName),
					          desc.SemanticName, _TRUNCATE);
				}
				ri.SemanticIndex = desc.SemanticIndex;
				ri.ComponentMask = desc.Mask;
				ri.ComponentType = desc.ComponentType;
				variant.reflectedInputs.push_back(ri);
			}

			variant.reflectedOutputs.reserve(shaderDesc.OutputParameters);
			for (UINT i = 0; i < shaderDesc.OutputParameters; ++i)
			{
				D3D11_SIGNATURE_PARAMETER_DESC desc = {};
				if (FAILED(reflector->GetOutputParameterDesc(i, &desc)))
					continue;
				if (desc.SystemValueType != D3D_NAME_UNDEFINED)
					continue;

				Direct3d11_ReflectedVSOutput ro = {};
				if (desc.SemanticName)
				{
					strncpy_s(ro.SemanticName, sizeof(ro.SemanticName),
					          desc.SemanticName, _TRUNCATE);
				}
				ro.SemanticIndex  = desc.SemanticIndex;
				ro.Register       = desc.Register;
				ro.ComponentMask  = desc.Mask;
				ro.ReadWriteMask  = desc.ReadWriteMask;
				ro.ComponentType  = desc.ComponentType;
				variant.reflectedOutputs.push_back(ro);
			}
		}
	}

	// Plan 17-09: per-variant output-signature hash (outputs only).
	variant.outputSigHash = computeOutputSignatureHashFor(variant.reflectedOutputs);
}

// ----------------------------------------------------------------------

Direct3d11_VertexShaderData::Variant const & Direct3d11_VertexShaderData::getVariant(uint32 textureCoordinateSetKey) const
{
	auto it = m_variants.find(textureCoordinateSetKey);
	if (it != m_variants.end())
		return it->second;
	return compileVariant(textureCoordinateSetKey);
}

// ----------------------------------------------------------------------

Direct3d11_VertexShaderData::Variant const & Direct3d11_VertexShaderData::compileVariant(uint32 textureCoordinateSetKey) const
{
	// Insert the (empty) variant first so even early-returns (assembly / empty
	// body) yield a STABLE cached reference with a null d3dVS -- the draw is then
	// skipped, matching pre-17-09 behavior. No further m_variants insertions happen
	// in this call, so the reference stays valid (no rehash).
	Variant &variant = m_variants[textureCoordinateSetKey];

	char const * const sourceText  = m_bodyText;
	size_t       const sourceLen   = m_bodyLen;
	char const * const displayName = m_vertexShader ? m_vertexShader->getFilename() : nullptr;

	// Phase 19: legacy //asm vertex programs can't compile under D3D11. Instead
	// of leaving d3dVS null (which skipped the draw -> interiors/sky invisible),
	// bind a generic world-transform fallback VS so the geometry is at least
	// drawn textured. See compileAssemblyFallbackVS preamble + CONSULT-19
	// synthesis. The fallback is key-independent; we still cache per key (cheap,
	// shares one .cso). On fallback compile failure we keep the old behavior
	// (null d3dVS -> draw skipped).
	if (m_isAssembly)
	{
		logAssemblyFallbackOnce(displayName);
		ComPtr<ID3DBlob> fbBlob;
		uint64_t         fbHash = 0;
		if (!compileAssemblyFallbackVS(displayName, fbBlob, fbHash) || !fbBlob)
			return variant;
		variant.bytecode     = fbBlob;
		variant.bytecodeHash = fbHash;
		finalizeVariantFromBytecode(variant, displayName);
		return variant;
	}

	if (!sourceText || sourceLen == 0)
	{
		DEBUG_REPORT_LOG_PRINT(true,
			("Direct3d11_VertexShaderData: no compile path for '%s' (emptyBody=1)\n",
			displayName ? displayName : "<unnamed>"));
		return variant;
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
	//
	// Plan 11-07 Iter-4: also inject D3D11_REWRITE_VERSION. This macro
	// rides along with the FNV-1a cache key (hashSource walks the
	// defines list and mixes Name + Definition for each entry). The
	// include-handler's SM4+ keyword rewrite (Direct3d11_HlslRewrite,
	// invoked from both Direct3d11_CompileIncludeHandler::Open and --
	// since Iter-7 -- this compileOrLoad function below) is applied
	// AFTER hashSource computes the key, so without this macro a
	// change to the rewrite rules would not invalidate stale .cso
	// entries that pre-date the change. Bump the version when you
	// change ANY rule in Direct3d11_HlslRewrite.cpp.
	//
	// Plan 11-07 Iter-7: version bumped 3 -> 4. Iter-7 changes:
	//   * extracted the rewrite logic from Direct3d11_CompileIncludeHandler
	//     into Direct3d11_HlslRewrite so it can apply to both `#include`
	//     content AND the main shader source.
	//   * added Rule B (`:\s*c\d+\b` -> whitespace) and Rule C
	//     (`:\s*register\s*\(\s*c\d+\s*\)` -> whitespace) targeting the
	//     X3202 "location semantics cannot be specified on members"
	//     class surfaced by Iter-5/6 smoke on `2d.vsh(8,44-45)`.
	//   * applied the rewrite to the MAIN shader source below (new
	//     call site).
	//
	// Plan 11-07 Iter-8: version bumped 4 -> 5. Iter-7 smoke produced
	// the IDENTICAL X3202 at the identical `2d.vsh(8,44-45)` site,
	// meaning the actual register-type letter on line 8 col 44-45 is
	// NOT `c` (which was Iter-7's speculative choice). Iter-8 expanded
	// Rules B/C to match the (then-canonical) 5 D3D11 register-type
	// letters `[bcstu]`: `b#` (cbuffer), `t#` (texture/SRV), `s#`
	// (sampler), `u#` (UAV), `c#` (constant). Iter-8 also landed a
	// THROWAWAY diagnostic dump in
	// Direct3d11_HlslRewrite::applyToMainSource that wrote the first
	// applyToMainSource input bytes to stage/shader-debug-first-
	// source.txt; the dump was reverted in Iter-9's first commit.
	//
	// Plan 11-07 Iter-9: version bumped 5 -> 6. Iter-8 smoke ALSO
	// produced the IDENTICAL X3202 at `2d.vsh(8,44-45)` despite the
	// `[bcstu]` expansion. The Iter-8 THROWAWAY dump revealed the
	// actual line-8 content:
	//   `float4  position  : POSITION0  : register(v0);`
	// The register-type letter is `v` -- vertex input stream register
	// (D3D9-era vertex declaration syntax). Neither `c` (Iter-7) nor
	// `[bcstu]` (Iter-8) covered it. Iter-9 extends Rules B/C to
	// `[bcstuv]` -- all 6 canonical D3D HLSL register-type letters
	// per MSDN. The `v` case is structurally safer than the others:
	// D3D11's input-layout binding happens at the IA stage via
	// Direct3d11_VertexDeclarationMap (Plan 11-06) using
	// D3D11_INPUT_ELEMENT_DESC, not at the shader level -- so
	// stripping the `register(vN)` hint drops a binding the SM4+
	// compiler doesn't consume anyway.
	//
	// Plan 11-07 Iter-10: version bumped 6 -> 7 alongside a PURE DIAGNOSTIC
	// THROWAWAY iteration that added one-shot rewrite I/O dumps at both
	// Direct3d11_HlslRewrite entry points (main-source + first 5 includes,
	// inline-extended late in Iter-10 / early Iter-11). The dumps captured
	// to stage/shader-rewrite-{main,inc-N}-{input,output}.txt and were
	// reverted in Iter-11's first commit per the prearranged THROWAWAY
	// cleanup plan; their purpose -- giving Iter-11 the ground-truth bytes
	// to design the actual X4016 fix -- was served. See the Iter-11
	// log entry in 11-07-iteration-log.md for the diagnosis the dumps
	// produced.
	//
	// Plan 11-07 Iter-11: version bumped 8 -> 9 to ride the context-aware
	// Rules B/C revision. Iter-10/11 dumps showed vertex_shader_constants
	// .inc was being OVER-STRIPPED on its ~15 GLOBAL `: register(cN)`
	// declarations (including the multi-register `LightData lightData :
	// register(c16)` struct), defeating D3DCompile's auto-allocator and
	// producing X4016 "overlapping register semantics not yet implemented
	// 'c0'". Iter-11 added a `sawColonThisLine` line-scoped flag to Rules
	// B/C: strip only on SECOND-or-later `:` on the same line (struct-
	// member stacked-semantic shape, SM4+ rejects); preserve on FIRST `:`
	// of a line (global-declaration register binding, SM4+ accepts). See
	// the Iter-11 entry in 11-07-iteration-log.md for the diagnosis
	// narrative + the Iter-11 fix description.
	//
	// Plan 11-07 Iter-12: version bumped 9 -> 10 alongside a PURE
	// DIAGNOSTIC THROWAWAY iteration that re-added the Iter-10/11 rewrite
	// I/O dump pair (counter-extended to 5 includes; reverted in Iter-13's
	// first commit). The dumps confirmed Iter-11's context-aware rule
	// preserves global `: register(cN)` bindings correctly; X4016 is a
	// D3DCompile-level rejection of D3D9-era explicit register bindings
	// under the implicit `$Globals` cbuffer that SM4+ creates. See the
	// Iter-12 entry in 11-07-iteration-log.md for the diagnostic findings.
	//
	// Plan 11-07 Iter-13: version bumped 10 -> 11 (revert commit, Rules
	// unchanged from Iter-11) then 11 -> 12 (this commit -- Rule D
	// cbuffer-wrap of c-register-bound globals in
	// vertex_shader_constants.inc). Rule D runs as a pre-pass in
	// Direct3d11_HlslRewrite::applyToIncludeBuffer, detecting the
	// contiguous run of `float<...> name : register(cN);` declarations
	// and wrapping them in `cbuffer SwgVertexConstants : register(b0)
	// { ... };` with each `register(cN)` converted to `packoffset(cN)`.
	// This sidesteps the FXC X4016 "overlapping register semantics not
	// yet implemented 'c0'" rejection that fires when D3D9-era explicit
	// register bindings sit at file scope and get auto-promoted to the
	// implicit $Globals cbuffer under SM4+ / vs_4_0_level_9_3 even with
	// BACKWARDS_COMPATIBILITY enabled. See Direct3d11_HlslRewrite.cpp's
	// Rule D block for the full diagnostic narrative + Iter-12 dump
	// evidence that confirmed hypothesis #2 (D3DCompile-level rejection
	// of preserved bindings) as the X4016 root cause.
	std::vector<D3D_SHADER_MACRO> defines;
	defines.push_back({ "POSITION",               "SV_POSITION" });
	defines.push_back({ "D3D11",                  "1" });
	defines.push_back({ "D3D11_PROFILE",          kVertexShaderProfile });
	defines.push_back({ "D3D11_REWRITE_VERSION",  "17" });   // Phase 19: 16 -> 17 (shared rewriter gained Rule F specular-pow guard; bump keeps VS .cso cache in lockstep with the rewriter logic). Plan 17-09: per-key texcoord-set remap macros; invalidates Plan 11-09.6 cache.

	// Plan 17-09: regenerate the texcoord-set remap macros from the key, mirroring
	// Direct3d9_VertexShaderData::createVertexShader:356-421. Tag i -> mesh set
	// (key >> (i*3)) & 7; DOT3 is dim-4, others dim-2. Emit one
	// `#define textureCoordinateSet<TAG> textureCoordinateSet<set>` per tag, then a
	// single DECLARE_textureCoordinateSets declaring each UNIQUE set ONCE (aliased
	// tags -- e.g. MAIN+NRML both -> set0 -- collapse to one declaration). No
	// `: register(vN)` on the members: vs_4_0 rejects struct-member register binds
	// (the X3202 class) and D3D11 binds inputs by semantic via the input layout.
	// macroStorage backs the char* pointers and MUST outlive the D3DCompile call.
	std::vector<std::string> macroStorage;
	if (!m_textureCoordinateSetTags.empty())
	{
		int setDim[8] = { 0,0,0,0,0,0,0,0 };
		int maxSet = -1;
		size_t const tagCount = m_textureCoordinateSetTags.size();
		macroStorage.reserve(tagCount * 2 + 2);
		for (size_t i = 0; i < tagCount; ++i)
		{
			int const set = static_cast<int>((textureCoordinateSetKey >> (i * 3)) & 7u);
			int const dim = (m_textureCoordinateSetTags[i] == TAG_DOT3_local) ? 4 : 2;
			setDim[set] = dim;
			if (set > maxSet) maxSet = set;

			char tagStr[16] = {};
			ConvertTagToString(m_textureCoordinateSetTags[i], tagStr);              // 4 chars + NUL, e.g. "MAIN"
			macroStorage.push_back(std::string("textureCoordinateSet") + tagStr);   // Name:       textureCoordinateSetMAIN
			macroStorage.push_back(std::string("textureCoordinateSet") + std::to_string(set)); // Definition: textureCoordinateSet0
		}

		std::string decl;
		for (int s = 0; s <= maxSet; ++s)
		{
			if (setDim[s] == 0) continue;
			decl += "float";  decl += std::to_string(setDim[s]);
			decl += " textureCoordinateSet"; decl += std::to_string(s);
			decl += " : TEXCOORD"; decl += std::to_string(s);
			decl += ";";
		}
		macroStorage.push_back(std::string("DECLARE_textureCoordinateSets"));
		macroStorage.push_back(decl);

		// macroStorage is fully populated now -- c_str() pointers are stable.
		size_t const perTag = tagCount * 2;
		for (size_t i = 0; i < perTag; i += 2)
			defines.push_back({ macroStorage[i].c_str(), macroStorage[i + 1].c_str() });
		defines.push_back({ macroStorage[perTag].c_str(), macroStorage[perTag + 1].c_str() });
	}

	defines.push_back({ nullptr,                  nullptr });   // terminator

	// Hash the source + defines -- include the trailing terminator entry
	// so that hashSource sees the full effective defines list.
	uint64_t const hash = Direct3d11_ShaderCache::hashSource(
		sourceText, sourceLen, defines.data());

	// D-03 hybrid: try cache first.
	ComPtr<ID3DBlob> blob;
	if (!Direct3d11_ShaderCache::tryLoad(hash, blob))
	{
		// Plan 11-07 Iter-7: apply the HLSL textual rewrite to the
		// MAIN shader source before handing it to D3DCompile. The Iter-4
		// rewrite ran in the include-handler only -- covering `#include`-
		// resolved `.inc` content -- which left the main `.vsh` body
		// (e.g., `2d.vsh` itself) unrewritten. Iter-5/6 smoke surfaced
		// the X3202 error class in the main body, not the includes:
		//
		//   D:\Code\swg-client-v2\stage\vertex_program\2d.vsh(8,44-45):
		//     error X3202: location semantics cannot be specified on members
		//
		// The new Direct3d11_HlslRewrite::applyToMainSource copies the
		// engine-owned source view into a stack-scoped std::vector<char>
		// with the same rules applied (Rule A: SM4+ keyword rename;
		// Rule B: `:\s*c\d+\b` -> whitespace; Rule C:
		// `:\s*register\s*\(\s*c\d+\s*\)` -> whitespace). D3DCompile
		// is invoked synchronously against the vector's storage; the
		// vector goes out of scope cleanly after the call.
		//
		// The engine's source buffer (sourceText / sourceLen) is read-
		// only here -- ownership stays with ShaderImplementationPassVertexShader::
		// m_text, which is unchanged.
		//
		// Caveat (recorded in Direct3d11_HlslRewrite.h preamble +
		// Plan 11-07 Iter-7 iteration log): Rules B/C DROP the register-
		// binding metadata. D3DCompile's automatic register-allocator
		// picks registers from the available pool. If the engine
		// assumes a specific register layout at the C++ level
		// (SetVertexShaderConstantF(N, ...) expecting the shader to
		// read constant cN), runtime rendering could break in subtle
		// ways. Plan 11-07 Task 3 smoke would surface any such issue
		// as visual artifacts; the rewrite's semantic-info loss is on
		// the suspect list if visuals look wrong.
		std::vector<char> rewrittenSource;
		Direct3d11_HlslRewrite::applyToMainSource(
			sourceText, sourceLen, rewrittenSource, displayName);

		// Cache miss -- compile via D3DCompile.
		ComPtr<ID3DBlob> errors;

		// Plan 11-07 Iter-6: D3DCOMPILE_ENABLE_STRICTNESS removed.
		// Plan 11-05 baseline set Flags1 = ENABLE_STRICTNESS, then Iter-5
		// added ENABLE_BACKWARDS_COMPATIBILITY without noticing the two are
		// mutually exclusive at the d3dcompiler_47 entry validation layer.
		// Smoke under Iter-5 surfaced:
		//   error X3116: Flags specified both compatibility and strict mode.
		//   These are mutually exclusive
		// (crash dump SwgClient_d.exe-unknown.0-20260519010725.txt).
		// Iter-6 drops STRICTNESS because backwards-compatibility mode is BY
		// DESIGN the opposite stance: we WANT the compiler to accept SOE's
		// D3D9-era HLSL leniently, so the strict-mode flag is incompatible
		// with our chosen strategy.
		//
		// Meta-lesson recorded in Iter-6 iteration log entry: when modifying
		// D3DCompile Flags1, always read the existing value first and audit
		// for conflicts. Iter-5 surfaced this lesson by failing to inspect
		// the baseline flag set before adding BACKWARDS_COMPATIBILITY.
		UINT flags = 0;
#ifdef _DEBUG
		flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
		// Plan 11-07 Iter-5: enable backwards-compatibility mode in the
		// HLSL compiler. Microsoft documents this flag (0x1000) as
		// "Directs the compiler to enable older shaders to compile to
		// 5_0 targets." -- it relaxes a number of legacy-syntax
		// restrictions that vs_5_0 / ps_5_0 / vs_4_0_level_9_* otherwise
		// reject.
		//
		// Smoke under Iter-4 surfaced:
		//   D:\...\stage\vertex_program\2d.vsh(8,44-45):
		//   error X3202: location semantics cannot be specified on members
		//
		// X3202 = D3D9-era HLSL allowed struct members to carry
		// register-binding shortcut semantics like `: c0`, `: c4`,
		// `: register(c8)` directly on the member declaration. SM4+
		// permits register bindings on (a) global variables, (b) cbuffer
		// members (with packoffset), (c) samplers/textures -- NOT on
		// struct members. The flag (per MS docs) is the single canonical
		// path that accepts the legacy-struct-member-register form along
		// with many other D3D9-era constructs.
		//
		// Strategy: try the flag first (high-value, low-effort). If it
		// resolves the X3202 case it likely also resolves any further
		// legacy-syntax surface area we haven't surfaced yet -- could
		// short-circuit several future iterations. If it does NOT
		// resolve X3202, Iter-6 lands a textual rewrite of `: c<N>` /
		// `: register(c<N>)` from struct-member contexts (mirrors the
		// Iter-4 include-handler approach, but also routes the MAIN
		// shader source through the rewrite -- the X3202 is in 2d.vsh
		// itself, not a `.inc` file, so the Iter-4 handler does not
		// reach it).
		//
		// Caveat (forward-looking): the flag changes COMPILE output, not
		// just lexing. If subtle runtime rendering issues surface in a
		// later iteration (Plan 11-07 Task 3 smoke), the
		// BACKWARDS_COMPATIBILITY flag becomes a suspect alongside the
		// other compile-flag / rewrite-version changes.
		flags |= D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY;

		// Plan 11-08 Iter-1.5 retrofit (CODEX Q2 mandate): force row-major
		// matrix packing in compiled HLSL cbuffer storage. Without this
		// flag, HLSL `float4x4` defaults to COLUMN-MAJOR; XMFLOAT4X4
		// (row-major bytes) uploaded to a cbuffer slot would be read
		// TRANSPOSED by the shader -- Iter-18 BSOD Hypothesis 1 redux,
		// structurally guaranteed by the layout mismatch. The flag value
		// (1 << 3 = 0x8) does NOT bit-collide with the previously-dropped
		// STRICTNESS (1 << 11 = 0x800) that Iter-5/6 resolved; confirmed
		// by CODEX peer review. Pairs with D3D11_REWRITE_VERSION bump
		// 13 -> 14 above so the FNV-1a shader-cache hash invalidates
		// every cached .cso on next launch.
		flags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

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
			rewrittenSource.data(),
			rewrittenSource.size(),
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

	variant.bytecode = blob;
	variant.bytecodeHash = hash;

	// Plan 11-09.15 Iter-7 disasm dump + CreateVertexShader + D3DReflect
	// input/output signature capture + output-sig hash. Extracted to a shared
	// helper (Phase 19) so the //asm fallback path above reuses it verbatim.
	finalizeVariantFromBytecode(variant, displayName);
	return variant;
}

// ======================================================================
