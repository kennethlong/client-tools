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
#include <vector>

class  VertexBufferFormat;
struct VertexBufferDescriptor;
struct Direct3d11_ReflectedVSInput;   // Plan 11-09.8 (defined in Direct3d11_VertexShaderData.h)

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
	// (engine VertexBufferFormat -> D3D11 input layout). All elements
	// receive InputSlot=0 (single-stream caller).
	static int buildInputElementDesc(
		VertexBufferFormat const &format,
		D3D11_INPUT_ELEMENT_DESC outDesc[16]);

	// Plan 11-09.7: multi-stream variant. Writes elements with
	// InputSlot=inputSlot at outDesc[0..return-1]. Caller manages the
	// cumulative write position across streams (typically writing
	// stream 0 starting at index 0, then stream 1 starting at the
	// previous return value, etc.). Returns -1 if maxElements is
	// exceeded (caller should treat as failure -- ID3D11InputLayout
	// has a hard 16-element cap).
	// Plan 17-08 (GAP-6): firstTexCoordSemanticIndex threads a GLOBAL running
	// TEXCOORD usage index across streams, mirroring D3D9
	// Direct3d9_VertexDeclarationMap.cpp:119-224 (which uses a single
	// `textureCoordinate` counter spanning all streams). Without this, stream 1's
	// TEXCOORD sets reset to index 0 and the bump VS's `TEXCOORD2` tangent (DOT3
	// on stream 1) is never matched -> phantom-zeroed -> normalize(0) -> NaN.
	// Single-stream callers pass 0. Caller advances by the stream's
	// getNumberOfTextureCoordinateSets() before the next stream.
	static int buildInputElementDescForStream(
		VertexBufferFormat const &format,
		D3D11_INPUT_ELEMENT_DESC *outDesc,
		int maxElements,
		UINT inputSlot,
		UINT firstTexCoordSemanticIndex = 0);

	// Plan 11-09.8: append phantom elements for VS-declared inputs not
	// covered by the format-derived elements. Phantom elements use
	// InputSlot=15 with AlignedByteOffset=0 (zero-stride read from the
	// global phantom zero buffer). Mutates outDesc in place starting at
	// outDesc[currentCount]. Returns the new total element count, OR -1
	// on capacity overflow (caller treats as layout-creation failure).
	//
	// CODEX-endorsed (Plan 11-09.8 consult Q2 C-lite/A2 hybrid): per-VS
	// reflection captured by Direct3d11_VertexShaderData drives this
	// augmentation; format-driven elements remain authoritative for the
	// real geometry streams, phantom elements fill the gap.
	static int augmentWithPhantomElements(
		std::vector<Direct3d11_ReflectedVSInput> const &reflectedInputs,
		D3D11_INPUT_ELEMENT_DESC *outDesc,
		int currentCount,
		int maxElements);
};

// ======================================================================

#endif
