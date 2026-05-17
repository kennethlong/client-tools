// ======================================================================
//
// Direct3d11_StaticVertexBufferData.h
// Phase 11 D3D11 renderer plugin -- static (immutable-shaped) vertex
// buffer implementing StaticVertexBufferGraphicsData. Plan 11-04.
//
// USAGE = D3D11_USAGE_DEFAULT + UpdateSubresource on lock/unlock (the
// engine fills the buffer via lock() AFTER CreateBuffer, so true
// IMMUTABLE is not workable; mirrors the texture-data lock path).
// ComPtr ownership; no D3DPOOL_MANAGED per D-13.
//
// ======================================================================

#ifndef INCLUDED_Direct3d11_StaticVertexBufferData_H
#define INCLUDED_Direct3d11_StaticVertexBufferData_H

// ======================================================================

class MemoryBlockManager;

#include "clientGraphics/StaticVertexBuffer.h"
#include "Direct3d11_VertexBufferDescriptorMap.h"

#include <d3d11.h>
#include <wrl/client.h>

// ======================================================================

class Direct3d11_StaticVertexBufferData : public StaticVertexBufferGraphicsData
{
public:

	static void *operator new(size_t size);
	static void  operator delete(void *memory);

	static void install();
	static void remove();

public:

	explicit Direct3d11_StaticVertexBufferData(const StaticVertexBuffer &vertexBuffer);
	virtual ~Direct3d11_StaticVertexBufferData();

	virtual void                            *lock(bool readOnly);
	virtual void                             unlock();
	virtual const VertexBufferDescriptor    &getDescriptor() const;
	virtual int                              getSortKey();

	ID3D11Buffer *getVertexBuffer() const;
	int           getVertexSize() const;

private:

	Direct3d11_StaticVertexBufferData();
	Direct3d11_StaticVertexBufferData(const Direct3d11_StaticVertexBufferData &);
	Direct3d11_StaticVertexBufferData &operator =(const Direct3d11_StaticVertexBufferData &);

private:

	static MemoryBlockManager *ms_memoryBlockManager;

private:

	const StaticVertexBuffer                  &m_vertexBuffer;
	const VertexBufferDescriptor              &m_descriptor;
	Microsoft::WRL::ComPtr<ID3D11Buffer>       m_d3dBuffer;
	void                                      *m_lockBuffer;  // CPU staging for lock/unlock
	bool                                       m_lockedReadOnly;
};

// ======================================================================

inline ID3D11Buffer *Direct3d11_StaticVertexBufferData::getVertexBuffer() const
{
	return m_d3dBuffer.Get();
}

// ----------------------------------------------------------------------

inline int Direct3d11_StaticVertexBufferData::getVertexSize() const
{
	return m_descriptor.vertexSize;
}

// ======================================================================

#endif
