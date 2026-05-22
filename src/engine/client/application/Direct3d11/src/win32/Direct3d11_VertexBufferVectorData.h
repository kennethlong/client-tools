// ======================================================================
//
// Direct3d11_VertexBufferVectorData.h
// Phase 11 D3D11 renderer plugin -- multi-stream VertexBufferVector
// graphics-data wrapper. Plan 11-09.7.
//
// Mirrors Direct3d9_VertexBufferVectorData shape with one architectural
// substitution: D3D9 caches an IDirect3DVertexDeclaration9 at
// construction time (VS-agnostic). D3D11 ID3D11InputLayout creation
// REQUIRES VS bytecode (Pitfall 6), so this wrapper does NOT cache a
// layout. Instead it captures the per-stream VertexBufferFormat
// pointers needed to compose the input-layout cache key at draw time
// via Direct3d11_VertexDeclarationMap::getOrCreateMultiStream.
//
// Stream-count ceiling: MAX_VERTEX_BUFFERS = 2 (matches D3D9 ceiling
// at Direct3d9_VertexBufferVectorData.cpp:21).
//
// Used by the skeletal-animation rendering path
// (SoftwareBlendSkeletalShaderPrimitive) for split position/normal +
// blend-indices/weights vertex streams.
//
// ======================================================================

#ifndef INCLUDED_Direct3d11_VertexBufferVectorData_H
#define INCLUDED_Direct3d11_VertexBufferVectorData_H

// ======================================================================

#include "clientGraphics/VertexBufferVector.h"

class VertexBufferFormat;

// ======================================================================

class Direct3d11_VertexBufferVectorData : public VertexBufferVectorGraphicsData
{
public:

	enum { MAX_VERTEX_BUFFERS = 2 };

	Direct3d11_VertexBufferVectorData(VertexBufferVector const & vertexBufferVector);
	virtual ~Direct3d11_VertexBufferVectorData();

	// Per-stream format accessors -- consumed by
	// Direct3d11_StateCache::setVertexBufferVector when composing the
	// IASetVertexBuffers call + the input-layout cache key.
	int                          getStreamCount() const;
	VertexBufferFormat const *   getStreamFormat(int stream) const;

	// Plan 11-09.7: Gl_api::setVertexBufferVector slot body. Wired by
	// Direct3d11.cpp at install time. Iterates the engine VBVector (we
	// have friend access to VertexBufferVector::m_vertexBufferList +
	// m_graphicsData via the Rule-3 declaration in
	// clientGraphics/VertexBufferVector.h), extracts per-stream
	// ID3D11Buffer / stride / offset, and routes to
	// Direct3d11_StateCache::setVertexBufferVectorBindState. Static
	// because it's wired into ms_glApi.setVertexBufferVector as a
	// function pointer.
	static void                  bind(VertexBufferVector const & vbVector);

private:

	int                          m_streamCount;
	VertexBufferFormat const *   m_streamFormat[MAX_VERTEX_BUFFERS];
};

// ======================================================================

#endif
