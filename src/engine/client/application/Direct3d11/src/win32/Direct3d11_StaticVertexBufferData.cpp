// ======================================================================
//
// Direct3d11_StaticVertexBufferData.cpp
// Phase 11 D3D11 renderer plugin -- static vertex buffer.
// Plan 11-04 (Wave 4 -- resource layer). Replaces createStaticVertexBufferData
// scaffold_fatal_stub.
//
// USAGE = D3D11_USAGE_DEFAULT + UpdateSubresource on lock/unlock. The
// engine fills the buffer via lock() AFTER CreateBuffer (Texture / mesh
// load path), so USAGE_IMMUTABLE (which requires initial data on create)
// is not workable. Engine lock() returns a CPU staging block; unlock()
// blits it into the GPU buffer via UpdateSubresource.
//
// Per D-13: no D3DPOOL_MANAGED / OnLostDevice / OnResetDevice -- ComPtr
// ownership; resource destroyed in destructor.
//
// ======================================================================

#include "FirstDirect3d11.h"
#include "Direct3d11_StaticVertexBufferData.h"

#include "Direct3d11.h"
#include "Direct3d11_Device.h"

#include "sharedFoundation/MemoryBlockManager.h"

#include <d3d11.h>
#include <wrl/client.h>

// ======================================================================

using Microsoft::WRL::ComPtr;

MemoryBlockManager *Direct3d11_StaticVertexBufferData::ms_memoryBlockManager;

// ======================================================================

void Direct3d11_StaticVertexBufferData::install()
{
	DEBUG_FATAL(ms_memoryBlockManager, ("Direct3d11_StaticVertexBufferData already installed"));
	ms_memoryBlockManager = new MemoryBlockManager(
		"Direct3d11_StaticVertexBufferData", true,
		sizeof(Direct3d11_StaticVertexBufferData), 0, 0, 0);
}

// ----------------------------------------------------------------------

void Direct3d11_StaticVertexBufferData::remove()
{
	delete ms_memoryBlockManager;
	ms_memoryBlockManager = nullptr;
}

// ----------------------------------------------------------------------

void *Direct3d11_StaticVertexBufferData::operator new(size_t size)
{
	UNREF(size);
	NOT_NULL(ms_memoryBlockManager);
	DEBUG_FATAL(size != sizeof(Direct3d11_StaticVertexBufferData), ("wrong new called"));
	DEBUG_FATAL(size != static_cast<size_t>(ms_memoryBlockManager->getElementSize()), ("installed with bad size"));
	return ms_memoryBlockManager->allocate();
}

// ----------------------------------------------------------------------

void Direct3d11_StaticVertexBufferData::operator delete(void *memory)
{
	NOT_NULL(ms_memoryBlockManager);
	ms_memoryBlockManager->free(memory);
}

// ======================================================================

Direct3d11_StaticVertexBufferData::Direct3d11_StaticVertexBufferData(const StaticVertexBuffer &vertexBuffer)
:
	StaticVertexBufferGraphicsData(),
	m_vertexBuffer(vertexBuffer),
	m_descriptor(Direct3d11_VertexBufferDescriptorMap::getDescriptor(vertexBuffer.getFormat())),
	m_d3dBuffer(),
	m_lockBuffer(nullptr),
	m_lockedReadOnly(false)
{
	ID3D11Device * const device = Direct3d11_Device::getDevice();
	NOT_NULL(device);

	UINT const byteWidth = static_cast<UINT>(m_descriptor.vertexSize * m_vertexBuffer.getNumberOfVertices());

	D3D11_BUFFER_DESC bd = {};
	bd.ByteWidth      = byteWidth;
	bd.Usage          = D3D11_USAGE_DEFAULT;
	bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;
	bd.MiscFlags      = 0;

	// No initial-data pointer -- engine fills via lock/unlock after
	// CreateBuffer. UpdateSubresource on unlock writes the data.
	HRESULT const hr = device->CreateBuffer(&bd, nullptr, &m_d3dBuffer);
	FATAL_DX_HR("Direct3d11_StaticVertexBufferData::CreateBuffer (static VB) failed: %s", hr);
}

// ----------------------------------------------------------------------

Direct3d11_StaticVertexBufferData::~Direct3d11_StaticVertexBufferData()
{
	if (m_lockBuffer)
	{
		operator delete[](m_lockBuffer);
		m_lockBuffer = nullptr;
	}
	m_d3dBuffer.Reset();
}

// ----------------------------------------------------------------------

const VertexBufferDescriptor &Direct3d11_StaticVertexBufferData::getDescriptor() const
{
	return m_descriptor;
}

// ----------------------------------------------------------------------

void *Direct3d11_StaticVertexBufferData::lock(bool readOnly)
{
	NOT_NULL(m_d3dBuffer);
	DEBUG_FATAL(m_lockBuffer, ("static VB already locked"));

	UINT const byteWidth = static_cast<UINT>(m_descriptor.vertexSize * m_vertexBuffer.getNumberOfVertices());
	m_lockBuffer = operator new[](byteWidth);
	m_lockedReadOnly = readOnly;

	// Engine writes vertex data into the returned pointer; on unlock we
	// UpdateSubresource into the GPU buffer. For readOnly locks the data
	// is undefined (D3D11 USAGE_DEFAULT buffers cannot be read back from
	// the GPU directly -- the codebase only locks for write).
	if (readOnly)
	{
		// Zero so callers don't read stale memory.
		memset(m_lockBuffer, 0, byteWidth);
	}

	return m_lockBuffer;
}

// ----------------------------------------------------------------------

void Direct3d11_StaticVertexBufferData::unlock()
{
	NOT_NULL(m_d3dBuffer);
	NOT_NULL(m_lockBuffer);

	if (!m_lockedReadOnly)
	{
		ID3D11DeviceContext * const context = Direct3d11_Device::getContext();
		NOT_NULL(context);
		context->UpdateSubresource(m_d3dBuffer.Get(), 0, nullptr, m_lockBuffer, 0, 0);
	}

	operator delete[](m_lockBuffer);
	m_lockBuffer = nullptr;
}

// ----------------------------------------------------------------------

int Direct3d11_StaticVertexBufferData::getSortKey()
{
	return static_cast<int>(reinterpret_cast<uintptr_t>(m_d3dBuffer.Get()));
}

// ======================================================================
