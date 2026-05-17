// ======================================================================
//
// Direct3d11_DynamicVertexBufferData.h
// Phase 11 D3D11 renderer plugin -- dynamic ring-buffer VB.
// Plan 11-04 (Wave 4). Replaces createDynamicVertexBufferData stub.
//
// One process-wide ring buffer (D3D11_USAGE_DYNAMIC), allocated at
// install() with videomem-tiered size. Each per-mesh data instance
// reserves a slice of the ring via lock() (Map WRITE_DISCARD on wrap;
// WRITE_NO_OVERWRITE on append per RESEARCH Pitfall 5).
//
// Per D-13: no D3DPOOL_MANAGED / OnLostDevice / OnResetDevice.
//
// ======================================================================

#ifndef INCLUDED_Direct3d11_DynamicVertexBufferData_H
#define INCLUDED_Direct3d11_DynamicVertexBufferData_H

// ======================================================================

class MemoryBlockManager;

#include "clientGraphics/DynamicVertexBuffer.h"
#include "Direct3d11_VertexBufferDescriptorMap.h"

#include <d3d11.h>
#include <wrl/client.h>

// ======================================================================

class Direct3d11_DynamicVertexBufferData : public DynamicVertexBufferGraphicsData
{
public:

	static void install();
	static void remove();
	static void beginFrame();

	static void *operator new(size_t size);
	static void  operator delete(void *memory);

	static ID3D11Buffer *getSharedRingBuffer();

public:

	explicit Direct3d11_DynamicVertexBufferData(const VertexBuffer &vertexBuffer);
	virtual ~Direct3d11_DynamicVertexBufferData();

	virtual void                            *lock(int numberOfVertices, bool forceDiscard);
	virtual void                             unlock();
	virtual void                             unlock(int numberOfVertices);
	virtual const VertexBufferDescriptor    &getDescriptor() const;
	virtual int                              getNumberOfLockableDynamicVertices(bool withDiscard);
	virtual int                              getSortKey();

	int getNumberOfVertices() const;
	int getVertexSize() const;
	int getOffset() const;

private:

	void roundUpUsed() const;

private:

	Direct3d11_DynamicVertexBufferData();
	Direct3d11_DynamicVertexBufferData(const Direct3d11_DynamicVertexBufferData &);
	Direct3d11_DynamicVertexBufferData &operator =(const Direct3d11_DynamicVertexBufferData &);

private:

	// Ring-buffer state (process-wide; static).
	static bool                                  ms_newFrame;
	static int                                   ms_size;
	static int                                   ms_used;
	static Microsoft::WRL::ComPtr<ID3D11Buffer>  ms_d3dRingBuffer;
	static MemoryBlockManager                   *ms_memoryBlockManager;

	static int                                   ms_locksSinceBeginFrame;
	static int                                   ms_discardsSinceBeginFrame;
	static int                                   ms_locksSinceResourceCreation;
	static int                                   ms_discardsSinceResourceCreation;
	static int                                   ms_locksEver;
	static int                                   ms_discardsEver;

	// Per-mesh state.
	const VertexBufferDescriptor    &m_vertexBufferDescriptor;
	int                              m_numberOfVertices;
	int                              m_offset;     // byte offset into ring at last lock
};

// ======================================================================

inline int Direct3d11_DynamicVertexBufferData::getNumberOfVertices() const
{
	return m_numberOfVertices;
}

// ----------------------------------------------------------------------

inline int Direct3d11_DynamicVertexBufferData::getOffset() const
{
	return m_offset;
}

// ======================================================================

#endif
