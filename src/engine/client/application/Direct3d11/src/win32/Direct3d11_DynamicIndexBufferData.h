// ======================================================================
//
// Direct3d11_DynamicIndexBufferData.h
// Phase 11 D3D11 renderer plugin -- dynamic ring-buffer IB.
// Plan 11-04 (Wave 4). Replaces createDynamicIndexBufferData stub.
//
// Engine signature is parameterless: each call returns a fresh
// DynamicIndexBufferGraphicsData; all instances share one
// USAGE_DYNAMIC ID3D11Buffer ring (D3D11_BIND_INDEX_BUFFER).
//
// Lock/Unlock semantics mirror Direct3d11_DynamicVertexBufferData:
//   wrap   -> Map(D3D11_MAP_WRITE_DISCARD)
//   append -> Map(D3D11_MAP_WRITE_NO_OVERWRITE)
//
// ======================================================================

#ifndef INCLUDED_Direct3d11_DynamicIndexBufferData_H
#define INCLUDED_Direct3d11_DynamicIndexBufferData_H

// ======================================================================

class MemoryBlockManager;

#include "clientGraphics/DynamicIndexBuffer.h"

#include <d3d11.h>
#include <wrl/client.h>

// ======================================================================

class Direct3d11_DynamicIndexBufferData : public DynamicIndexBufferGraphicsData
{
public:

	static void install();
	static void remove();
	static void beginFrame();
	static void setSize(int numberOfIndices);

	static ID3D11Buffer *getSharedRingBuffer();

	static void *operator new(size_t size);
	static void  operator delete(void *memory);

public:

	Direct3d11_DynamicIndexBufferData();
	virtual ~Direct3d11_DynamicIndexBufferData();

	virtual Index *lock(int numberOfIndices);
	virtual void   unlock();

	int getOffset() const;
	int getNumberOfIndices() const;

private:

	Direct3d11_DynamicIndexBufferData(const Direct3d11_DynamicIndexBufferData &);
	Direct3d11_DynamicIndexBufferData &operator =(const Direct3d11_DynamicIndexBufferData &);

private:

	static bool                                  ms_newFrame;
	static MemoryBlockManager                   *ms_memoryBlockManager;
	static int                                   ms_numberOfIndices;       // ring size in INDICES
	static int                                   ms_usedNumberOfIndices;
	static Microsoft::WRL::ComPtr<ID3D11Buffer>  ms_d3dRingBuffer;

	static int                                   ms_locksSinceBeginFrame;
	static int                                   ms_discardsSinceBeginFrame;
	static int                                   ms_locksSinceResourceCreation;
	static int                                   ms_discardsSinceResourceCreation;
	static int                                   ms_locksEver;
	static int                                   ms_discardsEver;

private:

	int m_offset;             // INDEX offset into ring at last lock
	int m_numberOfIndices;    // count locked at last lock

	static void recreateRing();
};

// ======================================================================

inline int Direct3d11_DynamicIndexBufferData::getOffset() const
{
	return m_offset;
}

// ----------------------------------------------------------------------

inline int Direct3d11_DynamicIndexBufferData::getNumberOfIndices() const
{
	return m_numberOfIndices;
}

// ======================================================================

#endif
