// ======================================================================
//
// Direct3d11_DynamicIndexBufferData.cpp
// Phase 11 D3D11 renderer plugin -- dynamic ring-buffer IB.
// Plan 11-04.
//
// ======================================================================

#include "FirstDirect3d11.h"
#include "Direct3d11_DynamicIndexBufferData.h"

#include "Direct3d11.h"
#include "Direct3d11_Device.h"
#include "ConfigDirect3d11.h"

#include "sharedFoundation/MemoryBlockManager.h"
#include "sharedFoundation/Os.h"

#include <d3d11.h>
#include <wrl/client.h>

// Phase 19 world-corruption DIAGNOSTIC (CODEX+Cursor ROUND-2 consult). See the
// matching block in Direct3d11_DynamicVertexBufferData.cpp. Smoking-gun test:
// log every dynamic IB ring lock that runs OFF the main/render thread.
// RESULT 2026-05-31: SILENT (CPU-thread race dead). P19_DISCARD_ONLY forces
// WRITE_DISCARD on every lock to test the CPU/GPU intra-frame reuse hazard.
// RESULT 2026-05-31: DISCARD-only ring = NO CHANGE -> dynamic ring EXONERATED.
#define P19_THREAD_AUDIT 0
#define P19_DISCARD_ONLY 0

// ======================================================================

using Microsoft::WRL::ComPtr;

bool                              Direct3d11_DynamicIndexBufferData::ms_newFrame;
MemoryBlockManager               *Direct3d11_DynamicIndexBufferData::ms_memoryBlockManager;
int                               Direct3d11_DynamicIndexBufferData::ms_numberOfIndices;
int                               Direct3d11_DynamicIndexBufferData::ms_usedNumberOfIndices;
ComPtr<ID3D11Buffer>              Direct3d11_DynamicIndexBufferData::ms_d3dRingBuffer;
int                               Direct3d11_DynamicIndexBufferData::ms_locksSinceBeginFrame;
int                               Direct3d11_DynamicIndexBufferData::ms_discardsSinceBeginFrame;
int                               Direct3d11_DynamicIndexBufferData::ms_locksSinceResourceCreation;
int                               Direct3d11_DynamicIndexBufferData::ms_discardsSinceResourceCreation;
int                               Direct3d11_DynamicIndexBufferData::ms_locksEver;
int                               Direct3d11_DynamicIndexBufferData::ms_discardsEver;

namespace
{
	// Default ring size in indices (KB / sizeof(Index)). Mirrors D3D9's
	// ConfigDirect3d9::getDynamicIndexBufferSize default (256 KB) until
	// the engine calls setSize.
	int const cms_defaultDynamicIndexBufferKB = 256;
}

// ======================================================================

void Direct3d11_DynamicIndexBufferData::install()
{
	DEBUG_FATAL(ms_memoryBlockManager, ("Direct3d11_DynamicIndexBufferData already installed"));

	ms_numberOfIndices = (cms_defaultDynamicIndexBufferKB * 1024) / static_cast<int>(sizeof(Index));
	recreateRing();

	ms_memoryBlockManager = new MemoryBlockManager(
		"Direct3d11_DynamicIndexBufferData", true,
		sizeof(Direct3d11_DynamicIndexBufferData), 0, 0, 0);
}

// ----------------------------------------------------------------------

void Direct3d11_DynamicIndexBufferData::remove()
{
	ms_d3dRingBuffer.Reset();
	delete ms_memoryBlockManager;
	ms_memoryBlockManager = nullptr;
}

// ----------------------------------------------------------------------

void Direct3d11_DynamicIndexBufferData::beginFrame()
{
	ms_newFrame                = true;
	ms_locksSinceBeginFrame    = 0;
	ms_discardsSinceBeginFrame = 0;
}

// ----------------------------------------------------------------------

void Direct3d11_DynamicIndexBufferData::setSize(int numberOfIndices)
{
	if (numberOfIndices == ms_numberOfIndices)
		return;

	ms_numberOfIndices = numberOfIndices;
	recreateRing();
}

// ----------------------------------------------------------------------

void Direct3d11_DynamicIndexBufferData::recreateRing()
{
	ms_d3dRingBuffer.Reset();

	ID3D11Device * const device = Direct3d11_Device::getDevice();
	NOT_NULL(device);

	D3D11_BUFFER_DESC bd = {};
	bd.ByteWidth      = static_cast<UINT>(ms_numberOfIndices * static_cast<int>(sizeof(Index)));
	bd.Usage          = D3D11_USAGE_DYNAMIC;
	bd.BindFlags      = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bd.MiscFlags      = 0;

	HRESULT const hr = device->CreateBuffer(&bd, nullptr, &ms_d3dRingBuffer);
	FATAL_DX_HR("Direct3d11_DynamicIndexBufferData::CreateBuffer (dynamic IB ring) failed: %s", hr);

	ms_usedNumberOfIndices             = 0;
	ms_newFrame                        = true;
	ms_locksSinceResourceCreation      = 0;
	ms_discardsSinceResourceCreation   = 0;
}

// ----------------------------------------------------------------------

ID3D11Buffer *Direct3d11_DynamicIndexBufferData::getSharedRingBuffer()
{
	return ms_d3dRingBuffer.Get();
}

// ----------------------------------------------------------------------

void *Direct3d11_DynamicIndexBufferData::operator new(size_t size)
{
	UNREF(size);
	NOT_NULL(ms_memoryBlockManager);
	DEBUG_FATAL(size != sizeof(Direct3d11_DynamicIndexBufferData), ("bad size"));
	DEBUG_FATAL(size != static_cast<size_t>(ms_memoryBlockManager->getElementSize()), ("installed with bad size"));
	return ms_memoryBlockManager->allocate();
}

// ----------------------------------------------------------------------

void Direct3d11_DynamicIndexBufferData::operator delete(void *memory)
{
	NOT_NULL(ms_memoryBlockManager);
	ms_memoryBlockManager->free(memory);
}

// ======================================================================

Direct3d11_DynamicIndexBufferData::Direct3d11_DynamicIndexBufferData()
:
	DynamicIndexBufferGraphicsData(),
	m_offset(0),
	m_numberOfIndices(0)
{
}

// ----------------------------------------------------------------------

Direct3d11_DynamicIndexBufferData::~Direct3d11_DynamicIndexBufferData()
{
}

// ----------------------------------------------------------------------

Index *Direct3d11_DynamicIndexBufferData::lock(int numberOfIndices)
{
	m_numberOfIndices = numberOfIndices;
	int const lengthBytes = numberOfIndices * static_cast<int>(sizeof(Index));

#if P19_THREAD_AUDIT
	if (!Os::isMainThread())
	{
		static int s_offThreadIBLocks = 0;
		++s_offThreadIBLocks;
		char tbuf[160];
		_snprintf_s(tbuf, sizeof(tbuf), _TRUNCATE,
			"[P19THREAD] dynamic IB ring lock OFF MAIN THREAD tid=%lu count=%d numIndices=%d",
			GetCurrentThreadId(), s_offThreadIBLocks, numberOfIndices);
		ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue();
		if (iq) iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_WARNING, tbuf);
	}
#endif

	++ms_locksSinceBeginFrame;
	++ms_locksSinceResourceCreation;
	++ms_locksEver;

	D3D11_MAP    mapType = D3D11_MAP_WRITE_NO_OVERWRITE;
	int          discard = 0;
#if P19_DISCARD_ONLY
	bool const p19ForceDiscard = true;  // diagnostic: rename every lock
#else
	bool const p19ForceDiscard = false;
#endif
	if (p19ForceDiscard || ms_newFrame || ms_usedNumberOfIndices + numberOfIndices > ms_numberOfIndices)
	{
		ms_newFrame = false;
		++ms_discardsSinceBeginFrame;
		++ms_discardsSinceResourceCreation;
		++ms_discardsEver;
		discard = 1;

		DEBUG_FATAL(numberOfIndices > ms_numberOfIndices, ("Too many indices %d/%d", numberOfIndices, ms_numberOfIndices));

		mapType = D3D11_MAP_WRITE_DISCARD;
		ms_usedNumberOfIndices = 0;
	}

	ID3D11DeviceContext * const context = Direct3d11_Device::getContext();
	NOT_NULL(context);

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	HRESULT const hr = context->Map(ms_d3dRingBuffer.Get(), 0, mapType, 0, &mapped);
	FATAL(FAILED(hr),
		("Could not lock dynamic ib %d=err %d=discard %d=offset(indices) %d=count %d/%d/%d=locks %d/%d/%d=discards",
		 HRESULT_CODE(hr), discard, ms_usedNumberOfIndices, numberOfIndices,
		 ms_locksSinceBeginFrame, ms_locksSinceResourceCreation, ms_locksEver,
		 ms_discardsSinceBeginFrame, ms_discardsSinceResourceCreation, ms_discardsEver));
	NOT_NULL(mapped.pData);

	m_offset = ms_usedNumberOfIndices;
	ms_usedNumberOfIndices += numberOfIndices;

	UNREF(lengthBytes);

	return reinterpret_cast<Index *>(static_cast<char *>(mapped.pData) + m_offset * static_cast<int>(sizeof(Index)));
}

// ----------------------------------------------------------------------

void Direct3d11_DynamicIndexBufferData::unlock()
{
	ID3D11DeviceContext * const context = Direct3d11_Device::getContext();
	NOT_NULL(context);
	context->Unmap(ms_d3dRingBuffer.Get(), 0);
}

// ======================================================================
