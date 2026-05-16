// ======================================================================
//
// ConfigDirect3d11.cpp
// Phase 11 D3D11 renderer plugin -- config-key registration body
// (Plan 11-02 scaffold; mirrors ConfigDirect3d9.cpp's KEY_INT/KEY_BOOL/
//  KEY_STRING pattern but under the [Direct3d11] section.)
//
// ======================================================================

#include "FirstDirect3d11.h"
#include "ConfigDirect3d11.h"

#include "sharedFoundation/ConfigFile.h"

// ======================================================================

namespace ConfigDirect3d11Namespace
{
	int          ms_adapter;
	int          ms_dynamicVertexBufferSize;
	int          ms_dynamicIndexBufferSize;
	bool         ms_discardDynamicBuffersAtBeginningOfFrame;
	bool         ms_antiAlias;

	// D3D11-specific.
	char const * ms_shaderCacheDir;
	bool         ms_enableDebugLayer;
	int          ms_preferredAdapterIndex;
}
using namespace ConfigDirect3d11Namespace;

// ======================================================================

#define KEY_INT(a,b)     (ms_ ## a = ConfigFile::getKeyInt("Direct3d11", #a, b))
#define KEY_BOOL(a,b)    (ms_ ## a = ConfigFile::getKeyBool("Direct3d11", #a, b))

// NOTE: KEY_STRING intentionally NOT supported through this scaffold.
// ConfigFile::getKeyString(section, key, default, bool) is not stubbed
// in DllExport.dll's delay-load surface (Direct3d9 plugin doesn't use it
// either), so the renderer plugin would fail to link against it. Wave 5
// (shader cache) revisits this -- either stubs getKeyString in DllExport
// (engine-side -- requires its own plan) or persists the cache dir via a
// different mechanism. For Plan 11-02 scaffold, ms_shaderCacheDir is a
// hard-coded compile-time default.

// ======================================================================

void ConfigDirect3d11::install()
{
	KEY_INT (adapter, -1);
	KEY_INT (dynamicVertexBufferSize, 256);
	KEY_INT (dynamicIndexBufferSize, 64);
	KEY_BOOL(discardDynamicBuffersAtBeginningOfFrame, false);
	KEY_BOOL(antiAlias, true);

	// D3D11-specific.
	ms_shaderCacheDir = "stage/shader-cache/";    // hard-coded default; see note above
#ifdef _DEBUG
	KEY_BOOL(enableDebugLayer, true);
#else
	KEY_BOOL(enableDebugLayer, false);
#endif
	KEY_INT (preferredAdapterIndex, -1);
}

// ----------------------------------------------------------------------

int  ConfigDirect3d11::getAdapter()                                   { return ms_adapter; }
int  ConfigDirect3d11::getDynamicVertexBufferSize()                   { return ms_dynamicVertexBufferSize; }
int  ConfigDirect3d11::getDynamicIndexBufferSize()                    { return ms_dynamicIndexBufferSize; }
bool ConfigDirect3d11::getDiscardDynamicBuffersAtBeginningOfFrame()   { return ms_discardDynamicBuffersAtBeginningOfFrame; }
bool ConfigDirect3d11::getAntiAlias()                                 { return ms_antiAlias; }

char const * ConfigDirect3d11::getShaderCacheDir()                    { return ms_shaderCacheDir; }
bool         ConfigDirect3d11::getEnableDebugLayer()                  { return ms_enableDebugLayer; }
int          ConfigDirect3d11::getPreferredAdapterIndex()             { return ms_preferredAdapterIndex; }

// ======================================================================
