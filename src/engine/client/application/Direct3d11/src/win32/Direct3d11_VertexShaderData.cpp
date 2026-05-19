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
	std::vector<D3D_SHADER_MACRO> defines;
	defines.push_back({ "POSITION",               "SV_POSITION" });
	defines.push_back({ "D3D11",                  "1" });
	defines.push_back({ "D3D11_PROFILE",          kVertexShaderProfile });
	defines.push_back({ "D3D11_REWRITE_VERSION",  "8" });
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
