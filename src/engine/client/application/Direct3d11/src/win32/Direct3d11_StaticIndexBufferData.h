// ======================================================================
//
// Direct3d11_StaticIndexBufferData.h
// Phase 11 D3D11 renderer plugin -- static index buffer.
// Plan 11-04 (Wave 4). Replaces createStaticIndexBufferData stub.
//
// USAGE = D3D11_USAGE_DEFAULT + UpdateSubresource on lock/unlock
// (mirrors Direct3d11_StaticVertexBufferData rationale -- engine fills
// via lock() after CreateBuffer).
// Index format = R16_UINT (engine uses 16-bit indices; Index typedef in
// Graphics.def is unsigned short).
//
// ======================================================================

#ifndef INCLUDED_Direct3d11_StaticIndexBufferData_H
#define INCLUDED_Direct3d11_StaticIndexBufferData_H

// ======================================================================

class MemoryBlockManager;

#include "clientGraphics/StaticIndexBuffer.h"

#include <d3d11.h>
#include <wrl/client.h>

// ======================================================================

class Direct3d11_StaticIndexBufferData : public StaticIndexBufferGraphicsData
{
public:

	static void *operator new(size_t size);
	static void  operator delete(void *memory);

	static void install();
	static void remove();

public:

	explicit Direct3d11_StaticIndexBufferData(const StaticIndexBuffer &indexBuffer);
	virtual ~Direct3d11_StaticIndexBufferData();

	virtual Index *lock(bool readOnly);
	virtual void   unlock();

	ID3D11Buffer *getIndexBuffer() const;

private:

	Direct3d11_StaticIndexBufferData();
	Direct3d11_StaticIndexBufferData(const Direct3d11_StaticIndexBufferData &);
	Direct3d11_StaticIndexBufferData &operator =(const Direct3d11_StaticIndexBufferData &);

private:

	static MemoryBlockManager  *ms_memoryBlockManager;

private:

	const StaticIndexBuffer                  &m_indexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer>      m_d3dIndexBuffer;
	Index                                    *m_lockBuffer;
	bool                                      m_lockedReadOnly;
};

// ======================================================================

inline ID3D11Buffer *Direct3d11_StaticIndexBufferData::getIndexBuffer() const
{
	return m_d3dIndexBuffer.Get();
}

// ======================================================================

#endif
