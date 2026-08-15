// ======================================================================
//
// ConfigDirect3d11.h
// Phase 11 D3D11 renderer plugin -- config-key registration surface
// (Plan 11-02 scaffold; subset of ConfigDirect3d9 plus three new keys
//  for D3D11-specific concerns -- shader cache dir, debug layer, adapter.)
//
// ======================================================================

#ifndef INCLUDED_ConfigDirect3d11_H
#define INCLUDED_ConfigDirect3d11_H

// ======================================================================

class ConfigDirect3d11
{
public:
	static void install();

	// Carried from Direct3d9 (semantics unchanged; per-renderer config sections).
	static int  getAdapter();
	static int  getDynamicVertexBufferSize();
	static int  getDynamicIndexBufferSize();
	static bool getDiscardDynamicBuffersAtBeginningOfFrame();
	static bool getAntiAlias();

	// NEW for D3D11:
	static char const * getShaderCacheDir();         // D-03 disk cache (default: "stage/shader-cache/")
	static bool         getEnableDebugLayer();        // D-08 fallback (default: true in _DEBUG, false in NDEBUG)
	static int          getPreferredAdapterIndex();   // default: -1 (use default adapter)
	static bool         getPreventDriverInternalThreading(); // default: true (NV race defeat); false = perf A/B ONLY
	static bool         getCensusLog();                      // default: false; true = per-frame gl11-census.csv (CONSULT-58)
	static bool         getConstantBufferRing();             // default: true; false = legacy per-slot cbuffer path (CONSULT-67 kill switch)
	static bool         getShaderCachePreload();             // default: true; false = per-hit disk reads (CONSULT-68 kill switch)
	static bool         getPointSprites();                   // default: true; false = 1-pixel points (point-sprite GS kill switch)
};

// ======================================================================

#endif
