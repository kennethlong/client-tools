// ======================================================================
//
// Direct3d11_CompileIncludeHandler.cpp
// Phase 11 D3D11 renderer plugin -- ID3DInclude handler implementation.
//
// Plan 11-07 Iteration 2 -- fixes the FATAL surfaced by Iter-1 smoke:
//
//   FATAL: Direct3d11_VertexShaderData: D3DCompile vs_5_0
//   'vertex_program/2d.vsh' failed: ...(3,10-61): error X1507:
//   failed to open source file: 'vertex_program/include/vertex_shader_constants.inc'
//
// Root cause: Plan 11-05's D3DCompile calls passed
// D3D_COMPILE_STANDARD_FILE_INCLUDE (which is the sentinel
// (ID3DInclude*)1) for the pInclude parameter. That sentinel selects
// the default Win32 file-system include handler, which only looks at
// the on-disk filesystem -- it cannot see TRE-archived asset content.
// SWG ships its .inc header sources inside the TRE bundle, so D3DCompile
// emits X1507 "failed to open source file".
//
// Fix: implement an ID3DInclude derivative whose Open() routes the
// included filename through TreeFile::open() (engine API; backs both
// search-path overlay directories and TRE archives transparently), then
// reads the file into a heap-allocated buffer that Close() releases.
// This is structurally identical to D3D9's IncludeHandler at
// Direct3d9_VertexShaderData.cpp:54-156, minus the per-engine-owns-window
// caching layer (Plan 11-07 keeps it simple -- shader compile is a one-shot
// install-time event; per-launch caching belongs in the .cso disk cache
// that Direct3d11_ShaderCache already provides at the bytecode level).
//
// Memory: buffer allocated in Open() as `new unsigned char[length]`,
// freed in Close() as `delete[]`. D3DCompile guarantees a matching
// Close() call for every successful Open().
//
// Path normalisation: D3D9 strips a leading "../../" if present (legacy
// of CLI compiler usage); we mirror that defensive behaviour. SWG-era
// include paths are typically simple relative paths
// (`vertex_program/include/vertex_shader_constants.inc`) that map
// directly onto TreeFile search paths.
//
// Plan 11-07 Iteration 4 -- SM4+ reserved-keyword textual rewrite.
//
// Iter-3 swapped the D3DCompile profile from "vs_5_0" to
// "vs_4_0_level_9_3" hoping the legacy-compat profile would relax SM5
// keyword reservations. Smoke surfaced the SAME FATAL signature:
//
//   FATAL: Direct3d11_VertexShaderData: D3DCompile vs_4_0_level_9_3
//   'vertex_program/2d.vsh' failed:
//     vertex_program/include/vertex_shader_constants.inc(81,25-29):
//     error X3000: syntax error: unexpected token 'point'
//
// Root cause analysis: `point` / `line` / `triangle` / `lineadj` /
// `triangleadj` are reserved at the HLSL **lexer** level in every SM4+
// profile (introduced for geometry-shader primitive-type-binding
// syntax). Profile-string downgrades do NOT relax this -- the lexer
// classifies them as keywords BEFORE any profile-specific feature
// gating runs. SWG's D3D9-era HLSL uses these identifiers freely as
// constant / sampler-state / variable names, which is fine under
// vs_2_0 (D3DX9 compiler) but rejected by d3dcompiler_47's lexer in
// every SM4+ target profile (including vs_4_0_level_9_*).
//
// Fix (Iter-4): textual whole-word rewrite in Open() before returning
// the include buffer to D3DCompile. Each of the 5 reserved keywords
// is renamed to `<keyword>_id` (e.g., `point` -> `point_id`,
// `triangle` -> `triangle_id`). Whole-word matching: a match starts
// at a position where the preceding char (if any) is NOT in
// `[A-Za-z0-9_]` and ends at a position where the following char is
// NOT in `[A-Za-z0-9_]`. Case-sensitive (HLSL is case-sensitive).
// This means `endpoint` / `linear` / `pointSize` / `triangleStrip_x`
// do NOT match. The `_id` suffix is short, unambiguous, and unlikely
// to collide with legitimate identifiers in the existing shaders.
//
// Edge cases:
//   * Comments: D3DCompile strips comments before lexing, so
//     rewriting inside comments is harmless.
//   * Preprocessor `#define point 5` would become `#define point_id 5`
//     and any subsequent `point` use would also become `point_id` --
//     consistent.
//   * String literals: HLSL has no real string-literal type; safe to
//     ignore.
//
// Cache invalidation: the rewrite happens AFTER hashSource() computes
// the FNV-1a hash, so cache entries are still keyed by the ORIGINAL
// source bytes. To force a mass cache miss when the rewrite logic
// changes between iterations, the VS/PS compile sites inject a
// `D3D11_REWRITE_VERSION` macro into the defines list (which DOES
// participate in hashSource). Bump the version when you change the
// keyword list. See Direct3d11_VertexShaderData.cpp / PixelShader
// equivalent for the matching D3D_SHADER_MACRO entry.
//
// Scope caveat: this rewrite touches `#include`-resolved content
// only. The MAIN shader file (e.g., `2d.vsh` itself) is NOT routed
// through this handler; the engine reads it via TreeFile and passes
// the content directly to D3DCompile's pSrcData parameter. If a main
// .vsh body uses these identifiers (not just the .inc), Iter-5 would
// need to apply the same rewrite at the
// Direct3d11_VertexShaderData::compileOrLoad source-buffer staging
// point.
//
// ======================================================================

#include "FirstDirect3d11.h"
#include "Direct3d11_CompileIncludeHandler.h"

#include "fileInterface/AbstractFile.h"
#include "sharedDebug/DebugFlags.h"
#include "sharedFile/TreeFile.h"

#include <cstring>
#include <cstddef>

// ======================================================================

namespace Direct3d11_CompileIncludeHandlerNamespace
{
	// SM4+ HLSL lexer reserves these identifiers as keywords for
	// geometry-shader primitive-type-binding syntax. SWG's D3D9-era
	// HLSL uses them as ordinary identifiers (variable / constant /
	// sampler-state names), which the lexer rejects with X3000
	// "syntax error: unexpected token '<keyword>'" before any
	// profile-specific feature gating runs. Profile-string swaps
	// (Iter-3: vs_5_0 -> vs_4_0_level_9_3) do NOT relax this --
	// the reservation is at the lexer level, not the profile-feature
	// level.
	//
	// Rewrite policy: rename each occurrence to `<keyword>_id`. The
	// `_id` suffix is short, unambiguous, and not used as a suffix
	// by the engine's stock HLSL (verified by grep). Convention is
	// stable across iterations; bump D3D11_REWRITE_VERSION (in the
	// VS/PS compile-site defines) when adding to this list.
	struct ReservedKeyword
	{
		char const *word;
		size_t      length;
	};

	constexpr ReservedKeyword kReservedKeywords[] = {
		{ "point",       5 },  // GS primitive type + sampler filter literal
		{ "line",        4 },  // GS primitive type
		{ "triangle",    8 },  // GS primitive type
		{ "lineadj",     7 },  // GS primitive type (line w/ adjacency)
		{ "triangleadj", 11 }, // GS primitive type (triangle w/ adjacency)
	};

	constexpr size_t kReservedKeywordCount =
		sizeof(kReservedKeywords) / sizeof(kReservedKeywords[0]);

	// Replacement suffix appended after each matched keyword.
	constexpr char const  kReplacementSuffix[] = "_id";
	constexpr size_t      kReplacementSuffixLength = 3;

	// HLSL identifier character class (same as C): [A-Za-z0-9_].
	// A whole-word match requires the previous char (if any) to be
	// OUT of this class AND the following char (if any) to be OUT
	// of this class.
	inline bool isIdentChar(unsigned char c)
	{
		return (c >= 'a' && c <= 'z')
			|| (c >= 'A' && c <= 'Z')
			|| (c >= '0' && c <= '9')
			|| (c == '_');
	}

	// Compute, in a single pass, the number of bytes the rewritten
	// buffer requires. Whole-word, case-sensitive match. Original
	// length is returned if no rewrites apply (caller can skip the
	// allocation in that case).
	size_t computeRewrittenLength(unsigned char const *src, size_t length)
	{
		if (!src || length == 0)
			return 0;

		size_t out = 0;
		size_t i   = 0;
		while (i < length)
		{
			bool matched = false;

			// A whole-word match can only start at the beginning of
			// the buffer OR after a non-identifier character.
			bool const leftBoundaryOk =
				(i == 0) || !isIdentChar(src[i - 1]);

			if (leftBoundaryOk)
			{
				for (size_t k = 0; k < kReservedKeywordCount; ++k)
				{
					ReservedKeyword const &kw = kReservedKeywords[k];
					if (i + kw.length > length)
						continue;
					if (std::memcmp(src + i, kw.word, kw.length) != 0)
						continue;

					// Right boundary: next char must be end-of-buffer
					// or non-identifier.
					size_t const tail = i + kw.length;
					bool const rightBoundaryOk =
						(tail == length) || !isIdentChar(src[tail]);
					if (!rightBoundaryOk)
						continue;

					// Whole-word match. Output = keyword + suffix.
					out += kw.length + kReplacementSuffixLength;
					i = tail;
					matched = true;
					break;
				}
			}

			if (!matched)
			{
				++out;
				++i;
			}
		}

		return out;
	}

	// Second pass: emit the rewritten bytes into `dst`. Caller
	// guarantees dst has at least `dstCapacity` bytes (the result of
	// computeRewrittenLength on the same src/length).
	void emitRewritten(
		unsigned char const *src, size_t length,
		unsigned char *dst, size_t dstCapacity)
	{
		size_t outPos = 0;
		size_t i      = 0;
		while (i < length)
		{
			bool matched = false;

			bool const leftBoundaryOk =
				(i == 0) || !isIdentChar(src[i - 1]);

			if (leftBoundaryOk)
			{
				for (size_t k = 0; k < kReservedKeywordCount; ++k)
				{
					ReservedKeyword const &kw = kReservedKeywords[k];
					if (i + kw.length > length)
						continue;
					if (std::memcmp(src + i, kw.word, kw.length) != 0)
						continue;

					size_t const tail = i + kw.length;
					bool const rightBoundaryOk =
						(tail == length) || !isIdentChar(src[tail]);
					if (!rightBoundaryOk)
						continue;

					// Copy keyword.
					DEBUG_FATAL(outPos + kw.length > dstCapacity,
						("Direct3d11_CompileIncludeHandler: emit overflow"
						 " (keyword '%s' at src[%zu], outPos=%zu/%zu)",
						 kw.word, i, outPos, dstCapacity));
					std::memcpy(dst + outPos, kw.word, kw.length);
					outPos += kw.length;

					// Append the suffix.
					DEBUG_FATAL(outPos + kReplacementSuffixLength > dstCapacity,
						("Direct3d11_CompileIncludeHandler: emit suffix overflow"
						 " (keyword '%s' at src[%zu], outPos=%zu/%zu)",
						 kw.word, i, outPos, dstCapacity));
					std::memcpy(dst + outPos, kReplacementSuffix, kReplacementSuffixLength);
					outPos += kReplacementSuffixLength;

					i = tail;
					matched = true;
					break;
				}
			}

			if (!matched)
			{
				DEBUG_FATAL(outPos + 1 > dstCapacity,
					("Direct3d11_CompileIncludeHandler: emit byte overflow"
					 " (src[%zu]=0x%02x, outPos=%zu/%zu)",
					 i, src[i], outPos, dstCapacity));
				dst[outPos] = src[i];
				++outPos;
				++i;
			}
		}

		DEBUG_FATAL(outPos != dstCapacity,
			("Direct3d11_CompileIncludeHandler: emit length mismatch"
			 " (outPos=%zu expected=%zu)", outPos, dstCapacity));
	}

	// Apply the SM4+ keyword rewrite to `inBuffer` (length `inLength`).
	// Returns a heap-allocated `new unsigned char[outLength]` buffer
	// (caller-owned); `outLength` receives the rewritten size. The
	// original `inBuffer` is freed by this routine if a rewrite was
	// applied; otherwise `inBuffer` is returned as-is (no-copy fast
	// path) and outLength == inLength.
	//
	// The Close() callback delete[]'s whatever pointer Open() returned,
	// so the lifetime story is: Open() either passes through the
	// original heap buffer (no rewrites needed) or swaps in a new heap
	// buffer (rewrites applied). Close() delete[]'s the resulting
	// pointer either way.
	unsigned char *applyKeywordRewrite(
		unsigned char *inBuffer, size_t inLength, size_t &outLength,
		char const *diagName)
	{
		if (!inBuffer || inLength == 0)
		{
			outLength = inLength;
			return inBuffer;
		}

		size_t const rewrittenLen = computeRewrittenLength(inBuffer, inLength);

		if (rewrittenLen == inLength)
		{
			// Fast path: no occurrences. Pass the buffer through.
			outLength = inLength;
			return inBuffer;
		}

		// Allocate a fresh buffer + emit.
		unsigned char *outBuffer = new unsigned char[rewrittenLen];
		emitRewritten(inBuffer, inLength, outBuffer, rewrittenLen);

		// Discard the original (we own it via readEntireFileAndClose).
		delete[] inBuffer;

		DEBUG_REPORT_LOG_PRINT(true,
			("Direct3d11_CompileIncludeHandler: rewrote SM4+ reserved keywords"
			 " in '%s' (input=%zu bytes, output=%zu bytes; +%zu)\n",
			 diagName ? diagName : "<unnamed>",
			 inLength, rewrittenLen, rewrittenLen - inLength));

		outLength = rewrittenLen;
		return outBuffer;
	}
}
using namespace Direct3d11_CompileIncludeHandlerNamespace;

// ======================================================================

ID3DInclude *Direct3d11_CompileIncludeHandler::getInstance()
{
	// Magma C++11 guarantees thread-safe initialization of function-scope
	// statics. The handler is stateless, so a single shared instance is
	// safe for concurrent use (though D3DCompile is invoked single-threaded
	// during install today).
	static Direct3d11_CompileIncludeHandler s_instance;
	return &s_instance;
}

// ----------------------------------------------------------------------

HRESULT __stdcall Direct3d11_CompileIncludeHandler::Open(
	D3D_INCLUDE_TYPE /*IncludeType*/,
	LPCSTR           pFileName,
	LPCVOID          /*pParentData*/,
	LPCVOID *        ppData,
	UINT *           pBytes)
{
	if (!pFileName || !ppData || !pBytes)
		return E_INVALIDARG;

	*ppData = nullptr;
	*pBytes = 0;

	// Defensive: strip leading "../../" (legacy CLI compiler artifact;
	// mirrors D3D9 IncludeHandler::Open behaviour at
	// Direct3d9_VertexShaderData.cpp:104-106).
	char const *resolvedName = pFileName;
	if (std::strncmp(resolvedName, "../../", 6) == 0)
		resolvedName += 6;

	// Route through TreeFile -- handles overlay directories + TRE
	// archives transparently. canFail=true so a missing include reports
	// STG_E_FILENOTFOUND back to D3DCompile (which produces a clean
	// X1507 error message) rather than triggering an engine-side FATAL.
	AbstractFile *file = TreeFile::open(resolvedName, AbstractFile::PriorityData, true);
	if (!file)
	{
		DEBUG_REPORT_LOG_PRINT(true,
			("Direct3d11_CompileIncludeHandler: could not open '%s' via TreeFile (X1507 path)\n",
			resolvedName));
		return STG_E_FILENOTFOUND;
	}

	if (!file->isOpen())
	{
		delete file;
		DEBUG_REPORT_LOG_PRINT(true,
			("Direct3d11_CompileIncludeHandler: TreeFile::open returned non-open file for '%s'\n",
			resolvedName));
		return STG_E_FILENOTFOUND;
	}

	int const length = file->length();
	if (length <= 0)
	{
		delete file;
		DEBUG_REPORT_LOG_PRINT(true,
			("Direct3d11_CompileIncludeHandler: zero-length include '%s'\n", resolvedName));
		return E_FAIL;
	}

	// readEntireFileAndClose() returns a `new unsigned char[length]`
	// buffer and closes the file (we still own + delete the file
	// wrapper). D3D9 sibling uses the same API at
	// Direct3d9_VertexShaderData.cpp:89,140.
	unsigned char *buffer = file->readEntireFileAndClose();
	delete file;
	if (!buffer)
	{
		DEBUG_REPORT_LOG_PRINT(true,
			("Direct3d11_CompileIncludeHandler: read failed for '%s'\n", resolvedName));
		return E_FAIL;
	}

	// Plan 11-07 Iter-4: apply the SM4+ reserved-keyword whole-word
	// rewrite. Renames `point` / `line` / `triangle` / `lineadj` /
	// `triangleadj` -> `<keyword>_id`. The rewrite is no-op (returns
	// the original buffer untouched) when no matches are found, so
	// most includes pay zero allocation cost beyond the single-pass
	// scan. When matches are found, the original buffer is freed
	// inside applyKeywordRewrite and a fresh heap buffer is returned;
	// either way Close() delete[]'s the result.
	size_t rewrittenLength = 0;
	unsigned char *finalBuffer = applyKeywordRewrite(
		buffer, static_cast<size_t>(length), rewrittenLength, resolvedName);

	*ppData = finalBuffer;
	*pBytes = static_cast<UINT>(rewrittenLength);
	return S_OK;
}

// ----------------------------------------------------------------------

HRESULT __stdcall Direct3d11_CompileIncludeHandler::Close(LPCVOID pData)
{
	// readEntireFileAndClose() in Open() returned a `new unsigned char[]`
	// buffer; release with delete[]. Const-cast is required because the
	// ID3DInclude::Close signature takes LPCVOID; the underlying memory
	// is owned by us and was non-const at allocation.
	if (pData)
	{
		unsigned char *buffer = const_cast<unsigned char *>(static_cast<unsigned char const *>(pData));
		delete[] buffer;
	}
	return S_OK;
}

// ======================================================================
