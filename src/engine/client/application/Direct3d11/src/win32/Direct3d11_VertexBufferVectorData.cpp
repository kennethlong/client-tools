// ======================================================================
//
// Direct3d11_VertexBufferVectorData.cpp
// Phase 11 D3D11 renderer plugin -- multi-stream VertexBufferVector
// graphics-data wrapper. Plan 11-09.7.
//
// Constructor captures per-stream VertexBufferFormat pointers from
// the engine's VertexBufferVector. No D3D11-specific resource is
// allocated here (unlike Direct3d9_VertexBufferVectorData which
// fetches an IDirect3DVertexDeclaration9 at construction time):
// input-layout creation in D3D11 requires VS bytecode (Pitfall 6),
// so the actual ID3D11InputLayout is deferred to apply-time via
// Direct3d11_VertexDeclarationMap::getOrCreateMultiStream.
//
// Engine-side Rule-3 deviation: VertexBufferVector::m_vertexBufferList
// is private; access requires the engine-side friend declaration
// added in this plan to clientGraphics/VertexBufferVector.h (mirrors
// the existing Direct3d9_VertexBufferVectorData friend at line 30).
//
// ======================================================================

#include "FirstDirect3d11.h"
#include "Direct3d11_VertexBufferVectorData.h"

#include "Direct3d11_DynamicVertexBufferData.h"
#include "Direct3d11_StateCache.h"
#include "Direct3d11_StaticVertexBufferData.h"

#include "clientGraphics/HardwareVertexBuffer.h"
#include "clientGraphics/VertexBufferFormat.h"

// ======================================================================

Direct3d11_VertexBufferVectorData::Direct3d11_VertexBufferVectorData(VertexBufferVector const & vertexBufferVector)
:	VertexBufferVectorGraphicsData(),
	m_streamCount(0)
{
	for (int i = 0; i < MAX_VERTEX_BUFFERS; ++i)
		m_streamFormat[i] = nullptr;

	VertexBufferVector::VertexBufferList const & list = *vertexBufferVector.m_vertexBufferList;

	int const count = static_cast<int>(list.size());
	DEBUG_FATAL(count > MAX_VERTEX_BUFFERS,
		("Direct3d11_VertexBufferVectorData: VBVector has %d streams; MAX_VERTEX_BUFFERS=%d (matches D3D9 ceiling)",
		count, MAX_VERTEX_BUFFERS));

	int j = 0;
	for (auto i = list.begin(); i != list.end() && j < MAX_VERTEX_BUFFERS; ++i, ++j)
		m_streamFormat[j] = &((*i)->getFormat());

	m_streamCount = j;
}

// ----------------------------------------------------------------------

Direct3d11_VertexBufferVectorData::~Direct3d11_VertexBufferVectorData()
{
	// No D3D11 resource owned here; the input-layout cache owns
	// ID3D11InputLayout ownership via Direct3d11_VertexDeclarationMap.
}

// ----------------------------------------------------------------------

int Direct3d11_VertexBufferVectorData::getStreamCount() const
{
	return m_streamCount;
}

// ----------------------------------------------------------------------

VertexBufferFormat const * Direct3d11_VertexBufferVectorData::getStreamFormat(int stream) const
{
	DEBUG_FATAL(stream < 0 || stream >= m_streamCount,
		("Direct3d11_VertexBufferVectorData::getStreamFormat: stream %d out of range [0, %d)",
		stream, m_streamCount));
	return m_streamFormat[stream];
}

// ----------------------------------------------------------------------
// Plan 11-09.7: Gl_api::setVertexBufferVector slot body.
//
// Iterates the engine VBVector (we have friend access via the Rule-3
// declaration in clientGraphics/VertexBufferVector.h:31), extracts per-
// stream ID3D11Buffer + stride + offset + format, and routes to
// Direct3d11_StateCache::setVertexBufferVectorBindState.
//
// Mirrors Direct3d9.cpp:3764 setVertexBufferVector shape, with D3D9 ->
// D3D11 substitutions: SetStreamSource-per-slot loop replaced by a
// single IASetVertexBuffers batch issued from applyPreDrawState. We
// track static/dynamic slice-count parity for the engine's draw-call
// vertex-count argument (matches D3D9's ms_sliceFirstVertex /
// ms_sliceNumberOfVertices tracking but routed through StateCache).
//
// Mixed static + dynamic VBs across streams: supported (D3D9
// precedent). Stream-offset semantics differ between static (offset=0)
// and dynamic (offset=data->getOffset() in vertices); we encode the
// dynamic offset into IASetVertexBuffers's per-slot offset[] arg in
// bytes (Plan 11-04 dynamic-ring pattern).

void Direct3d11_VertexBufferVectorData::bind(VertexBufferVector const & vbVector)
{
	enum { kMaxStreams = MAX_VERTEX_BUFFERS };
	ID3D11Buffer *             buffers[kMaxStreams] = {};
	UINT                       strides[kMaxStreams] = {};
	UINT                       offsets[kMaxStreams] = {};
	VertexBufferFormat const * formats[kMaxStreams] = {};

	int sliceFirstVertex = 0;
	int sliceVertexCount = 0;

	VertexBufferVector::VertexBufferList const & list = *vbVector.m_vertexBufferList;
	int stream = 0;
	for (auto i = list.begin(); i != list.end() && stream < kMaxStreams; ++i, ++stream)
	{
		HardwareVertexBuffer const &vb = **i;
		formats[stream] = &vb.getFormat();

		switch (vb.getType())
		{
			case HardwareVertexBuffer::T_static:
				{
					StaticVertexBuffer const *svb = safe_cast<StaticVertexBuffer const *>(&vb);
					Direct3d11_StaticVertexBufferData const *data =
						safe_cast<Direct3d11_StaticVertexBufferData const *>(svb->m_graphicsData);
					if (!data)
						break;
					buffers[stream] = data->getVertexBuffer();
					strides[stream] = static_cast<UINT>(data->getVertexSize());
					offsets[stream] = 0;
					sliceVertexCount = svb->getNumberOfVertices();
					sliceFirstVertex = 0;
				}
				break;

			case HardwareVertexBuffer::T_dynamic:
				{
					DynamicVertexBuffer const *dvb = safe_cast<DynamicVertexBuffer const *>(&vb);
					Direct3d11_DynamicVertexBufferData const *data =
						safe_cast<Direct3d11_DynamicVertexBufferData const *>(dvb->m_graphicsData);
					if (!data)
						break;
					buffers[stream] = Direct3d11_DynamicVertexBufferData::getSharedRingBuffer();
					strides[stream] = static_cast<UINT>(data->getVertexSize());
					offsets[stream] = static_cast<UINT>(data->getOffset()) * strides[stream];
					sliceVertexCount = data->getNumberOfVertices();
				}
				break;

			default:
				DEBUG_FATAL(true, ("Direct3d11_VertexBufferVectorData::bind: unknown VB type %d", static_cast<int>(vb.getType())));
				break;
		}
	}

	Direct3d11_StateCache::setVertexBufferVectorBindState(
		stream,
		buffers,
		strides,
		offsets,
		formats,
		sliceFirstVertex,
		sliceVertexCount);
}

// ======================================================================
