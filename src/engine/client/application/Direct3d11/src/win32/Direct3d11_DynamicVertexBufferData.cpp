// ======================================================================
//
// Direct3d11_DynamicVertexBufferData.cpp
// Phase 11 D3D11 renderer plugin -- dynamic ring-buffer VB.
// Plan 11-04.
//
// Ring sized at install() based on DXGI DedicatedVideoMemory (replaces
// D3D9's getVideoMemoryInMegabytes -- always shader path, no FFP branch
// per Plan 11-01 D-04a DESCOPE verdict).
//
// Lock/Unlock per RESEARCH Pitfall 5:
//   wrap            -> Map(D3D11_MAP_WRITE_DISCARD), reset used=0
//   append          -> Map(D3D11_MAP_WRITE_NO_OVERWRITE), advance used
//
// Per D-13: ComPtr ownership; no D3DPOOL_MANAGED / OnLostDevice /
// OnResetDevice.
//
// ======================================================================

#include "FirstDirect3d11.h"
#include "Direct3d11_DynamicVertexBufferData.h"

#include "Direct3d11.h"
#include "Direct3d11_Device.h"

#include "clientGraphics/VertexBuffer.h"
#include "sharedFoundation/MemoryBlockManager.h"

#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

// ======================================================================

using Microsoft::WRL::ComPtr;

bool                              Direct3d11_DynamicVertexBufferData::ms_newFrame;
int                               Direct3d11_DynamicVertexBufferData::ms_size;
int                               Direct3d11_DynamicVertexBufferData::ms_used;
ComPtr<ID3D11Buffer>              Direct3d11_DynamicVertexBufferData::ms_d3dRingBuffer;
MemoryBlockManager *              Direct3d11_DynamicVertexBufferData::ms_memoryBlockManager;
int                               Direct3d11_DynamicVertexBufferData::ms_locksSinceBeginFrame;
int                               Direct3d11_DynamicVertexBufferData::ms_discardsSinceBeginFrame;
int                               Direct3d11_DynamicVertexBufferData::ms_locksSinceResourceCreation;
int                               Direct3d11_DynamicVertexBufferData::ms_discardsSinceResourceCreation;
int                               Direct3d11_DynamicVertexBufferData::ms_locksEver;
int                               Direct3d11_DynamicVertexBufferData::ms_discardsEver;

// Plan 11-09.15 Iter-4 (Test A): stash the slice CPU pointer at lock() so
// unlock() can snapshot the engine's written verts BEFORE Unmap invalidates
// the mapped memory. Used by the fan-shaped-slice CPU snapshot diagnostic
// in unlock(int). File-scope static because the ring buffer is shared; only
// one lock can be active at a time.
static void *s_lastLockedSliceCPUPtr = nullptr;

// ======================================================================

void Direct3d11_DynamicVertexBufferData::install()
{
	DEBUG_FATAL(ms_memoryBlockManager, ("Direct3d11_DynamicVertexBufferData already installed"));
	ms_memoryBlockManager = new MemoryBlockManager(
		"Direct3d11_DynamicVertexBufferData", true,
		sizeof(Direct3d11_DynamicVertexBufferData), 0, 0, 0);

	// Always-shader path (no FFP branch per Plan 11-01 D-04a DESCOPE).
	// Query DXGI for DedicatedVideoMemory and pick a ring size.
	ID3D11Device * const device = Direct3d11_Device::getDevice();
	NOT_NULL(device);

	int videoMemoryMB = 256;  // defensive default if DXGI query fails

	ComPtr<IDXGIDevice>  dxgiDevice;
	ComPtr<IDXGIAdapter> adapter;
	HRESULT hr = device->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
	if (SUCCEEDED(hr) && dxgiDevice)
	{
		hr = dxgiDevice->GetAdapter(&adapter);
		if (SUCCEEDED(hr) && adapter)
		{
			DXGI_ADAPTER_DESC adapterDesc = {};
			if (SUCCEEDED(adapter->GetDesc(&adapterDesc)))
				videoMemoryMB = static_cast<int>(adapterDesc.DedicatedVideoMemory / (1024 * 1024));
		}
	}

	if      (videoMemoryMB <=  16) ms_size =  256 * 1024;
	else if (videoMemoryMB <=  32) ms_size =  512 * 1024;
	else if (videoMemoryMB <=  64) ms_size = 1024 * 1024;
	else                           ms_size = 2048 * 1024;

	D3D11_BUFFER_DESC bd = {};
	bd.ByteWidth      = static_cast<UINT>(ms_size);
	bd.Usage          = D3D11_USAGE_DYNAMIC;
	bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bd.MiscFlags      = 0;

	hr = device->CreateBuffer(&bd, nullptr, &ms_d3dRingBuffer);
	FATAL_DX_HR("Direct3d11_DynamicVertexBufferData::CreateBuffer (dynamic VB ring) failed: %s", hr);

	ms_used     = 0;
	ms_newFrame = true;
	ms_locksSinceResourceCreation    = 0;
	ms_discardsSinceResourceCreation = 0;

	DEBUG_REPORT_LOG_PRINT(true,
		("Direct3d11_DynamicVertexBufferData: ring buffer %d KB (videoMemory ~%d MB)\n",
		 ms_size / 1024, videoMemoryMB));
}

// ----------------------------------------------------------------------

void Direct3d11_DynamicVertexBufferData::remove()
{
	ms_d3dRingBuffer.Reset();

	delete ms_memoryBlockManager;
	ms_memoryBlockManager = nullptr;
}

// ----------------------------------------------------------------------

void Direct3d11_DynamicVertexBufferData::beginFrame()
{
	// Wrap at the start of each frame so per-frame dynamic uploads don't
	// stomp on previous-frame data still in-flight.
	ms_newFrame                = true;
	ms_locksSinceBeginFrame    = 0;
	ms_discardsSinceBeginFrame = 0;
}

// ----------------------------------------------------------------------

ID3D11Buffer *Direct3d11_DynamicVertexBufferData::getSharedRingBuffer()
{
	return ms_d3dRingBuffer.Get();
}

// ----------------------------------------------------------------------

void *Direct3d11_DynamicVertexBufferData::operator new(size_t size)
{
	UNREF(size);
	NOT_NULL(ms_memoryBlockManager);
	DEBUG_FATAL(size != sizeof(Direct3d11_DynamicVertexBufferData), ("wrong new called"));
	DEBUG_FATAL(size != static_cast<size_t>(ms_memoryBlockManager->getElementSize()), ("installed with bad size"));
	return ms_memoryBlockManager->allocate();
}

// ----------------------------------------------------------------------

void Direct3d11_DynamicVertexBufferData::operator delete(void *memory)
{
	NOT_NULL(ms_memoryBlockManager);
	ms_memoryBlockManager->free(memory);
}

// ======================================================================

Direct3d11_DynamicVertexBufferData::Direct3d11_DynamicVertexBufferData(const VertexBuffer &vertexBuffer)
:
	DynamicVertexBufferGraphicsData(),
	m_vertexBufferDescriptor(Direct3d11_VertexBufferDescriptorMap::getDescriptor(vertexBuffer.getFormat())),
	m_numberOfVertices(0),
	m_offset(0)
{
}

// ----------------------------------------------------------------------

Direct3d11_DynamicVertexBufferData::~Direct3d11_DynamicVertexBufferData()
{
}

// ----------------------------------------------------------------------

const VertexBufferDescriptor &Direct3d11_DynamicVertexBufferData::getDescriptor() const
{
	return m_vertexBufferDescriptor;
}

// ----------------------------------------------------------------------

int Direct3d11_DynamicVertexBufferData::getVertexSize() const
{
	return m_vertexBufferDescriptor.vertexSize;
}

// ----------------------------------------------------------------------

void Direct3d11_DynamicVertexBufferData::roundUpUsed() const
{
	int const vertexSize = m_vertexBufferDescriptor.vertexSize;
	ms_used = ((ms_used + vertexSize - 1) / vertexSize) * vertexSize;
}

// ----------------------------------------------------------------------

void *Direct3d11_DynamicVertexBufferData::lock(int numberOfVertices, bool forceDiscard)
{
	roundUpUsed();

	int const vertexSize = m_vertexBufferDescriptor.vertexSize;
	int const length     = numberOfVertices * vertexSize;

	++ms_locksSinceBeginFrame;
	++ms_locksSinceResourceCreation;
	++ms_locksEver;

	D3D11_MAP    mapType  = D3D11_MAP_WRITE_NO_OVERWRITE;
	int          discard  = 0;
	if (ms_newFrame || forceDiscard || ms_used + length > ms_size)
	{
		ms_newFrame = false;
		++ms_discardsSinceBeginFrame;
		++ms_discardsSinceResourceCreation;
		++ms_discardsEver;
		discard = 1;

		DEBUG_FATAL(length > ms_size, ("Too many vertices %d/%d", numberOfVertices, ms_size / vertexSize));

		mapType = D3D11_MAP_WRITE_DISCARD;
		ms_used = 0;
	}

	ID3D11DeviceContext * const context = Direct3d11_Device::getContext();
	NOT_NULL(context);

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	HRESULT const hr = context->Map(ms_d3dRingBuffer.Get(), 0, mapType, 0, &mapped);
	FATAL(FAILED(hr),
		("Could not lock dynamic vb %d=err %d=discard %d=offset %d=length %d/%d/%d=locks %d/%d/%d=discards",
		 HRESULT_CODE(hr), discard, ms_used, length,
		 ms_locksSinceBeginFrame, ms_locksSinceResourceCreation, ms_locksEver,
		 ms_discardsSinceBeginFrame, ms_discardsSinceResourceCreation, ms_discardsEver));
	NOT_NULL(mapped.pData);

	// Track this lock's slice. The engine writes into the byte range
	// [m_offset, m_offset + length); after Unmap the GPU sees the writes.
	m_numberOfVertices = numberOfVertices;
	m_offset           = ms_used / vertexSize;  // VERTEX index (engine sets BaseVertexLocation per draw)

	void * const sliceStart = static_cast<char *>(mapped.pData) + ms_used;
	ms_used += length;

	// Plan 11-09.15 Iter-4 (Test A): stash for unlock-time snapshot.
	s_lastLockedSliceCPUPtr = sliceStart;

	return sliceStart;
}

// ----------------------------------------------------------------------

void Direct3d11_DynamicVertexBufferData::unlock()
{
	unlock(m_numberOfVertices);
}

// ----------------------------------------------------------------------

void Direct3d11_DynamicVertexBufferData::unlock(int numberOfVertices)
{
	// Plan 11-09.15 Iter-4 (Test A): CPU-side VB snapshot for radial-pattern
	// investigation. Iter-3's per-fan-draw state diagnostic showed 15/16 fan
	// samples at byte offset 0 + same shared-ring VB pointer; the math on
	// the fan-list IB path is clean (CODEX Bucket A verified) so the next
	// question is whether the engine is writing a real quad into CPU memory
	// or degenerate/collapsed data. Read from s_lastLockedSliceCPUPtr BEFORE
	// Unmap -- after Unmap the mapped memory may be rotated by the driver
	// (D3D11_USAGE_DYNAMIC + WRITE_DISCARD semantics).
	//
	// Filtered to fan-shaped slices (numberOfVertices==4 && vertexSize==24:
	// position(12)+color(4)+UV(8)) to keep log volume low. Sampled like the
	// Iter-3 diagnostic: first 5 + every 500th.
	{
		int const vertexSize = m_vertexBufferDescriptor.vertexSize;
		bool const fanShaped = (numberOfVertices == 4) && (vertexSize == 24);
		if (fanShaped && s_lastLockedSliceCPUPtr)
		{
			static int s_diagFanShapedUnlocks = 0;
			++s_diagFanShapedUnlocks;
			bool const earlyBucket  = (s_diagFanShapedUnlocks <= 5);
			bool const sampleBucket = (s_diagFanShapedUnlocks > 5) && ((s_diagFanShapedUnlocks % 500) == 0);
			if (earlyBucket || sampleBucket)
			{
				uint8_t const *vb = static_cast<uint8_t const *>(s_lastLockedSliceCPUPtr);
				ID3D11InfoQueue *iq = Direct3d11_Device::getInfoQueue();
				for (int v = 0; v < 4; ++v)
				{
					uint8_t const *src = vb + v * vertexSize;
					float const px = *reinterpret_cast<float const *>(src + 0);
					float const py = *reinterpret_cast<float const *>(src + 4);
					float const pz = *reinterpret_cast<float const *>(src + 8);
					uint32_t const c = *reinterpret_cast<uint32_t const *>(src + 12);
					float const u  = *reinterpret_cast<float const *>(src + 16);
					float const tv = *reinterpret_cast<float const *>(src + 20);
					char buf[256];
					_snprintf_s(buf, sizeof(buf), _TRUNCATE,
						"Plan 11-09.15 Iter-4 fan-shaped unlock#%d vert#%d pos=(%.3f,%.3f,%.3f) color=0x%08X uv=(%.3f,%.3f) offsetVerts=%d",
						s_diagFanShapedUnlocks, v, px, py, pz, c, u, tv, m_offset);
					if (iq)
						iq->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO, buf);
				}
			}
		}
		s_lastLockedSliceCPUPtr = nullptr;
	}

	ID3D11DeviceContext * const context = Direct3d11_Device::getContext();
	NOT_NULL(context);

	context->Unmap(ms_d3dRingBuffer.Get(), 0);

	m_numberOfVertices = numberOfVertices;
	// Note: ms_used was already advanced in lock(); we don't re-adjust
	// here. (D3D9 plugin advances on unlock because Lock takes only an
	// upper-bound length and Unlock takes the actual used length; D3D11
	// Map gets a slice pointer and the actual-used count only affects
	// the draw call, not the buffer accounting.)
}

// ----------------------------------------------------------------------

int Direct3d11_DynamicVertexBufferData::getNumberOfLockableDynamicVertices(bool withDiscard)
{
	roundUpUsed();
	return (ms_size - (withDiscard ? 0 : ms_used)) / m_vertexBufferDescriptor.vertexSize;
}

// ----------------------------------------------------------------------

int Direct3d11_DynamicVertexBufferData::getSortKey()
{
	return static_cast<int>(reinterpret_cast<uintptr_t>(ms_d3dRingBuffer.Get()));
}

// ======================================================================
