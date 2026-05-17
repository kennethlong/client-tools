// ======================================================================
//
// Direct3d11_ShaderCache.h
// Phase 11 D3D11 renderer plugin -- D-03 hybrid shader compile cache.
//
// API: install(dir) creates/uses a per-launch on-disk cache; tryLoad+store
// implement the (FNV-1a 64-bit source-hash) -> .cso blob round-trip; hashSource
// participates the D3D_SHADER_MACRO defines array so that {POSITION, SV_POSITION}
// vs {POSITION, POSITION} produce distinct hashes.
//
// Per CONTEXT D-03: hybrid compile-on-first-use + persistent .cso disk cache.
// Cache directory defaults to ConfigDirect3d11::getShaderCacheDir() (currently
// "stage/shader-cache/") and survives across launches; .gitignored per Plan 02.
//
// Per RESEARCH §Don't Hand-Roll: the .cso blob is opaque & self-versioning to
// the runtime; just hash + dump + load. No format game.
//
// Per RESEARCH §Security Domain V12: hash is internally-generated hex; we
// validate hex-only before path concat (defense-in-depth) since the cache
// directory is engine-configured (NOT user input).
//
// ======================================================================

#ifndef INCLUDED_Direct3d11_ShaderCache_H
#define INCLUDED_Direct3d11_ShaderCache_H

// ======================================================================

#include <cstdint>
#include <d3dcompiler.h>
#include <wrl/client.h>

// ======================================================================

class Direct3d11_ShaderCache
{
public:

	static void install(char const *cacheDir);   // creates cacheDir if missing
	static void remove();

	// Returns true on cache hit (outBlob populated via D3DCreateBlob); false
	// on miss (caller invokes D3DCompile then passes the result to store).
	static bool tryLoad(uint64_t sourceHash, Microsoft::WRL::ComPtr<ID3DBlob> &outBlob);

	// Persist bytecode blob for next-launch warm start. Failure (e.g. disk
	// full / permission error) logs a warning -- NOT FATAL. Cache is opportunistic.
	static void store(uint64_t sourceHash, ID3DBlob *blob);

	// Compute a stable 64-bit hash over (source text, defines). Defines
	// participate so that {POSITION,SV_POSITION} vs {POSITION,POSITION} hash
	// distinctly. FNV-1a 64-bit -- non-cryptographic, fast, deterministic.
	static uint64_t hashSource(char const *text, size_t length, D3D_SHADER_MACRO const *defines);
};

// ======================================================================

#endif
