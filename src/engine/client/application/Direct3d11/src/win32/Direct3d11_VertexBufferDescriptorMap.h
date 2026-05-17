// ======================================================================
//
// Direct3d11_VertexBufferDescriptorMap.h
// Phase 11 D3D11 renderer plugin -- VertexBufferDescriptor cache.
// Plan 11-04 helper -- the engine-side VertexBufferDescriptor shape is
// the same for any backend; this class just caches per-VertexBufferFormat
// descriptors (offset/size table) so identical formats share one entry.
//
// Mirrors Direct3d9_VertexBufferDescriptorMap minus the D3DFVF table
// (D3D11 uses ID3D11InputLayout for vertex declaration, generated lazily
// when shaders bind in Wave 5/6 -- not here).
//
// ======================================================================

#ifndef INCLUDED_Direct3d11_VertexBufferDescriptorMap_H
#define INCLUDED_Direct3d11_VertexBufferDescriptorMap_H

// ======================================================================

#include <d3d11.h>

class  VertexBufferFormat;
struct VertexBufferDescriptor;

// ======================================================================

class Direct3d11_VertexBufferDescriptorMap
{
public:

	static void install();
	static void remove();

	static const VertexBufferDescriptor &getDescriptor(const VertexBufferFormat &vertexFormat);
	static const VertexBufferDescriptor &getDescriptor(uint32 formatFlags);

	// Plan 11-06: build D3D11_INPUT_ELEMENT_DESC[] for the given engine
	// VertexBufferFormat. Writes <= 16 entries (D3D11 hard max for input
	// elements per shader stage) into outDesc; returns the count actually
	// written. The semantic / format / offset table per RESEARCH Pitfall 6
	// (engine VertexBufferFormat -> D3D11 input layout).
	static int buildInputElementDesc(
		VertexBufferFormat const &format,
		D3D11_INPUT_ELEMENT_DESC outDesc[16]);
};

// ======================================================================

#endif
