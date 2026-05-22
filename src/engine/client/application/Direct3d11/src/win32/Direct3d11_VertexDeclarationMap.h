// ======================================================================
//
// Direct3d11_VertexDeclarationMap.h
// Phase 11 D3D11 renderer plugin -- ID3D11InputLayout cache keyed by
// (VertexBufferFormat-flags, VS bytecode hash). Plan 11-06 (Wave 6).
//
// Per RESEARCH Pitfall 6: D3D11's CreateInputLayout validates the input
// layout against the VS bytecode signature. A given engine
// VertexBufferFormat compiled against two different VS bytecodes produces
// two distinct ID3D11InputLayout objects (D3D9's IDirect3DVertexDeclaration9
// only depended on the format; D3D11 cannot do that). The cache key is
// the pair (format flags, VS bytecode hash) which is sufficient because
// D3DCompile is deterministic -- identical (source, defines) yields
// identical bytecode (Plan 11-05 m_bytecodeHash documentation).
//
// Per D-13: ComPtr ownership only.
//
// ======================================================================

#ifndef INCLUDED_Direct3d11_VertexDeclarationMap_H
#define INCLUDED_Direct3d11_VertexDeclarationMap_H

// ======================================================================

#include <cstdint>
#include <d3d11.h>
#include <wrl/client.h>

class VertexBufferFormat;
class Direct3d11_VertexShaderData;

// ======================================================================

class Direct3d11_VertexDeclarationMap
{
public:

	static void install();
	static void remove();

	// Returns a cached or newly-created ID3D11InputLayout for the given
	// engine vertex format + VS. The layout is owned by the cache; do NOT
	// release the returned pointer. Returns nullptr if either argument is
	// degenerate (empty format, NULL VS bytecode) -- caller should skip
	// the draw call (Plan 11-06 draw-dispatch contract).
	static ID3D11InputLayout *getOrCreate(
		VertexBufferFormat const &format,
		Direct3d11_VertexShaderData const *vsData);

	// Plan 11-09.7: multi-stream variant. Builds an input layout where
	// element entries for each stream's format are tagged with
	// InputSlot=streamIndex. Used by Direct3d11_StateCache::applyPreDrawState
	// when a VertexBufferVector is the active geometry source (skeletal-
	// animation rendering path). Cache is separate from the single-stream
	// cache to avoid any (theoretical) Key collisions. streamCount must
	// be in [1, 2] (matches Direct3d9_VertexBufferVectorData ceiling).
	static ID3D11InputLayout *getOrCreateMultiStream(
		VertexBufferFormat const * const *streamFormats,
		int streamCount,
		Direct3d11_VertexShaderData const *vsData);

	// Diagnostic: count of distinct input layouts cached (sum of both
	// single-stream + multi-stream caches).
	static int getCachedLayoutCount();
};

// ======================================================================

#endif
