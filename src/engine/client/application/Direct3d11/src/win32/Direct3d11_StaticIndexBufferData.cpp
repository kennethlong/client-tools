// ======================================================================
//
// Direct3d11_StaticIndexBufferData.cpp
// Phase 11 D3D11 renderer plugin -- static index buffer.
// Plan 11-04. See header for rationale.
//
// ======================================================================

#include "FirstDirect3d11.h"
#include "Direct3d11_StaticIndexBufferData.h"

#include "Direct3d11.h"
#include "Direct3d11_Device.h"

#include "sharedFoundation/MemoryBlockManager.h"

#include <d3d11.h>
#include <wrl/client.h>

// ======================================================================

using Microsoft::WRL::ComPtr;

MemoryBlockManager *Direct3d11_StaticIndexBufferData::ms_memoryBlockManager;

// ======================================================================

void Direct3d11_StaticIndexBufferData::install()
{
	DEBUG_FATAL(ms_memoryBlockManager, ("Direct3d11_StaticIndexBufferData already installed"));
	ms_memoryBlockManager = new MemoryBlockManager(
		"Direct3d11_StaticIndexBufferData", true,
		sizeof(Direct3d11_StaticIndexBufferData), 0, 0, 0);
}

// ----------------------------------------------------------------------

void Direct3d11_StaticIndexBufferData::remove()
{
	delete ms_memoryBlockManager;
	ms_memoryBlockManager = nullptr;
}

// ----------------------------------------------------------------------

void *Direct3d11_StaticIndexBufferData::operator new(size_t size)
{
	UNREF(size);
	NOT_NULL(ms_memoryBlockManager);
	DEBUG_FATAL(size != sizeof(Direct3d11_StaticIndexBufferData), ("bad size"));
	DEBUG_FATAL(size != static_cast<size_t>(ms_memoryBlockManager->getElementSize()), ("installed with bad size"));
	return ms_memoryBlockManager->allocate();
}

// ----------------------------------------------------------------------

void Direct3d11_StaticIndexBufferData::operator delete(void *memory)
{
	NOT_NULL(ms_memoryBlockManager);
	ms_memoryBlockManager->free(memory);
}

// ======================================================================

Direct3d11_StaticIndexBufferData::Direct3d11_StaticIndexBufferData(const StaticIndexBuffer &indexBuffer)
:
	StaticIndexBufferGraphicsData(),
	m_indexBuffer(indexBuffer),
	m_d3dIndexBuffer(),
	m_lockBuffer(nullptr),
	m_lockedReadOnly(false)
{
	ID3D11Device * const device = Direct3d11_Device::getDevice();
	NOT_NULL(device);

	UINT const byteWidth = static_cast<UINT>(indexBuffer.getNumberOfIndices() * sizeof(Index));

	D3D11_BUFFER_DESC bd = {};
	bd.ByteWidth      = byteWidth;
	bd.Usage          = D3D11_USAGE_DEFAULT;
	bd.BindFlags      = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;
	bd.MiscFlags      = 0;

	HRESULT const hr = device->CreateBuffer(&bd, nullptr, &m_d3dIndexBuffer);
	FATAL_DX_HR("Direct3d11_StaticIndexBufferData::CreateBuffer (static IB) failed: %s", hr);
}

// ----------------------------------------------------------------------

Direct3d11_StaticIndexBufferData::~Direct3d11_StaticIndexBufferData()
{
	if (m_lockBuffer)
	{
		operator delete[](m_lockBuffer);
		m_lockBuffer = nullptr;
	}
	m_d3dIndexBuffer.Reset();
}

// ----------------------------------------------------------------------

Index *Direct3d11_StaticIndexBufferData::lock(bool readOnly)
{
	DEBUG_FATAL(m_lockBuffer, ("static IB already locked"));

	int const length = m_indexBuffer.getNumberOfIndices() * static_cast<int>(sizeof(Index));
	m_lockBuffer = static_cast<Index *>(operator new[](static_cast<size_t>(length)));
	m_lockedReadOnly = readOnly;

	if (readOnly)
		memset(m_lockBuffer, 0, static_cast<size_t>(length));

	return m_lockBuffer;
}

// ----------------------------------------------------------------------

void Direct3d11_StaticIndexBufferData::unlock()
{
	NOT_NULL(m_d3dIndexBuffer);
	NOT_NULL(m_lockBuffer);

	if (!m_lockedReadOnly)
	{
		ID3D11DeviceContext * const context = Direct3d11_Device::getContext();
		NOT_NULL(context);
		context->UpdateSubresource(m_d3dIndexBuffer.Get(), 0, nullptr, m_lockBuffer, 0, 0);
	}

	operator delete[](m_lockBuffer);
	m_lockBuffer = nullptr;
}

// ======================================================================
