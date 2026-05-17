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
// ======================================================================

#include "FirstDirect3d11.h"
#include "Direct3d11_CompileIncludeHandler.h"

#include "fileInterface/AbstractFile.h"
#include "sharedDebug/DebugFlags.h"
#include "sharedFile/TreeFile.h"

#include <cstring>

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

	*ppData = buffer;
	*pBytes = static_cast<UINT>(length);
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
