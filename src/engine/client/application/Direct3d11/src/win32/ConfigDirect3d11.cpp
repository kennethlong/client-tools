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
	bool         ms_preventDriverInternalThreading;
	bool         ms_censusLog;
	bool         ms_constantBufferRing;
	bool         ms_shaderCachePreload;
	bool         ms_pointSprites;
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

	// CONSULT-51 NV worker-thread race defeat (D3D11_CREATE_DEVICE_PREVENT_INTERNAL_
	// THREADING_OPTIMIZATIONS). Default TRUE -- turning it off re-exposes the nvwgf2um
	// zone-in crash race; false is for perf A/B ONLY (the flag throttles the driver's
	// internal threading and is the prime suspect for gl11 frame-rate pressure, 2026-07-03).
	KEY_BOOL(preventDriverInternalThreading, true);

	// CONSULT-58 per-frame Map-churn census: writes gl11-census.csv (one row per
	// frame: frame ms, draws, cbuffer updates per stage/slot, VB/IB ring
	// locks/discards) from present(). Default off = zero I/O.
	KEY_BOOL(censusLog, false);

	// CONSULT-67 cbuffer NO_OVERWRITE ring (Direct3d11_ConstantBuffer). Default
	// ON; also requires the D3D11.1 caps probed at install. false = kill switch
	// back to the legacy per-slot Map(WRITE_DISCARD) path.
	KEY_BOOL(constantBufferRing, true);

	// CONSULT-68 shader-cache RAM preload (Direct3d11_ShaderCache). The stall
	// sampler convicted per-hit fopen/fread of .cso files in the draw path
	// (charselect first-draw 1.2s, repeated 100-160ms zone-in stalls). Default
	// ON: a background thread slurps the cache dir at install; tryLoad becomes
	// a memory lookup. false = kill switch back to per-hit disk reads.
	KEY_BOOL(shaderCachePreload, true);

	// Point-sprite GS emulation (Direct3d11_PointSprite): D3D9-parity sized
	// stars (2026-08-15 gl05-vs-gl11 A/B conviction). Default ON; false =
	// kill switch back to 1-pixel hardware points (the pre-fix look).
	KEY_BOOL(pointSprites, true);
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
bool         ConfigDirect3d11::getPreventDriverInternalThreading()    { return ms_preventDriverInternalThreading; }
bool         ConfigDirect3d11::getCensusLog()                         { return ms_censusLog; }
bool         ConfigDirect3d11::getConstantBufferRing()                { return ms_constantBufferRing; }
bool         ConfigDirect3d11::getShaderCachePreload()                { return ms_shaderCachePreload; }
bool         ConfigDirect3d11::getPointSprites()                      { return ms_pointSprites; }

// ======================================================================
