// ======================================================================
//
// VertexBufferVector.h
// Copyright 2002, Sony Online Entertainment Inc.
// All Rights Reserved.
//
// ======================================================================

#ifndef INCLUDED_VertexBufferVector_H
#define INCLUDED_VertexBufferVector_H

// ======================================================================

class HardwareVertexBuffer;

// ======================================================================

class VertexBufferVectorGraphicsData
{
public:
	virtual DLLEXPORT ~VertexBufferVectorGraphicsData();
};

class VertexBufferVector
{
	friend class  Graphics;
	friend struct Gl_api;
	friend class  Direct3d8;
	friend class  Direct3d9;
	friend class  Direct3d9_VertexBufferVectorData;
	friend class  Direct3d11_VertexBufferVectorData;   // Plan 11-09.7 (Rule-3 deviation; parallels D3D9 line above; mirrors Plan 11-04 Texture.h + Plan 11-06 HardwareVB/IB + Plan 11-09 ShaderImplementation.h precedent)

public:

	VertexBufferVector(HardwareVertexBuffer const & vertexBuffer1);
	VertexBufferVector(HardwareVertexBuffer const & vertexBuffer1, HardwareVertexBuffer const & vertexBuffer2);
	~VertexBufferVector();

private:

	typedef stdvector<HardwareVertexBuffer const *>::fwd VertexBufferList;

private:

	VertexBufferList *               m_vertexBufferList;
	VertexBufferVectorGraphicsData * m_graphicsData;
};

// ======================================================================

#endif
