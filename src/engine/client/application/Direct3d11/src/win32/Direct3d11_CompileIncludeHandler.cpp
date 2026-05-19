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
// Plan 11-07 Iteration 4 -- SM4+ reserved-keyword textual rewrite
// (originally implemented INLINE in this file).
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
// Iter-4 added a whole-word textual rewrite in Open() before returning
// the include buffer to D3DCompile, renaming each of 5 SM4+ reserved
// keywords (`point` / `line` / `triangle` / `lineadj` / `triangleadj`)
// to `<keyword>_id`.
//
// Plan 11-07 Iteration 7 -- rewrite logic extracted to
// Direct3d11_HlslRewrite.{h,cpp} so the same rewrite can run at two
// distinct call sites:
//
//   1. This include-handler (Iter-2/4 site) -- routes `#include`-
//      resolved content through the rewrite. Heap-owning buffer
//      semantics (the include handler allocates the buffer via
//      readEntireFileAndClose; the rewrite either passes it through
//      unchanged or swaps in a fresh allocation and frees the
//      original; Close() delete[]'s whichever pointer Open() returned).
//
//   2. Direct3d11_VertexShaderData::compileOrLoad (Iter-7 NEW site) --
//      routes the MAIN shader source (e.g., `2d.vsh` itself) through
//      the rewrite. Non-owning view semantics; output goes into a
//      stack-scoped std::vector<char>.
//
// Iter-7 also extended the rewrite with two new rules that handle the
// X3202 ("location semantics cannot be specified on members") error
// class surfaced by Iter-5/6 smoke on the MAIN shader body of
// `vertex_program/2d.vsh`:
//
//   Rule B -- `:\s*c\d+\b` (e.g., `: c4`) replaced with whitespace.
//   Rule C -- `:\s*register\s*\(\s*c\d+\s*\)` (e.g., `: register(c4)`)
//             replaced with whitespace.
//
// Both new rules drop the legacy D3D9-era struct-member register-
// binding shortcut metadata. D3DCompile's automatic register-allocator
// then picks registers from the available pool. Caveat: if the engine
// assumes a specific register layout at the C++ level, runtime
// rendering could break in subtle ways (no FATAL, but wrong visuals).
// See Direct3d11_HlslRewrite.{h,cpp} for the full rule set + semantic-
// loss discussion.
//
// Edge cases (unchanged from Iter-4):
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
// participate in hashSource). Bump the version when you change any
// rule in Direct3d11_HlslRewrite.cpp. See Direct3d11_VertexShaderData.cpp /
// PixelShader equivalent for the matching D3D_SHADER_MACRO entry.
//
// ======================================================================

#include "FirstDirect3d11.h"
#include "Direct3d11_CompileIncludeHandler.h"

#include "Direct3d11_HlslRewrite.h"

#include "fileInterface/AbstractFile.h"
#include "sharedDebug/DebugFlags.h"
#include "sharedFile/TreeFile.h"

#include <cstring>
#include <cstddef>

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
	// `triangleadj` -> `<keyword>_id`.
	//
	// Plan 11-07 Iter-7: rewrite extended with two new rules for the
	// X3202 ("location semantics cannot be specified on members")
	// error class. Rule B strips `:\s*c\d+\b`; Rule C strips
	// `:\s*register\s*\(\s*c\d+\s*\)`. Both replace with whitespace
	// to preserve column offsets in subsequent error messages.
	//
	// The rewrite is no-op (returns the original buffer untouched)
	// when no matches are found, so most includes pay zero allocation
	// cost beyond the single-pass scan. When matches are found, the
	// original buffer is freed inside the utility and a fresh heap
	// buffer is returned; either way Close() delete[]'s the result.
	std::size_t rewrittenLength = 0;
	unsigned char *finalBuffer = Direct3d11_HlslRewrite::applyToIncludeBuffer(
		buffer, static_cast<std::size_t>(length), rewrittenLength, resolvedName);

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
