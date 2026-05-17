// ======================================================================
//
// Direct3d11_VertexBufferDescriptorMap.cpp
// Phase 11 D3D11 renderer plugin -- VertexBufferDescriptor cache.
// Plan 11-04. See header for rationale.
//
// ======================================================================

#include "FirstDirect3d11.h"
#include "Direct3d11_VertexBufferDescriptorMap.h"

#include "clientGraphics/VertexBufferFormat.h"
#include "clientGraphics/VertexBufferDescriptor.h"

#include <map>

// ======================================================================

namespace Direct3d11_VertexBufferDescriptorMapNamespace
{
	typedef std::map<uint32, VertexBufferDescriptor> DescriptorMap;
	DescriptorMap  *ms_descriptorMap = nullptr;
}
using namespace Direct3d11_VertexBufferDescriptorMapNamespace;

// ======================================================================

void Direct3d11_VertexBufferDescriptorMap::install()
{
	DEBUG_FATAL(ms_descriptorMap, ("Direct3d11_VertexBufferDescriptorMap already installed"));
	ms_descriptorMap = new DescriptorMap;
}

// ----------------------------------------------------------------------

void Direct3d11_VertexBufferDescriptorMap::remove()
{
	delete ms_descriptorMap;
	ms_descriptorMap = nullptr;
}

// ----------------------------------------------------------------------

const VertexBufferDescriptor &Direct3d11_VertexBufferDescriptorMap::getDescriptor(const VertexBufferFormat &vertexFormat)
{
	NOT_NULL(ms_descriptorMap);

	DescriptorMap::iterator i = ms_descriptorMap->find(vertexFormat.getFlags());
	if (i != ms_descriptorMap->end())
		return i->second;

	VertexBufferDescriptor descriptor;

	// position
	if (vertexFormat.hasPosition())
	{
		descriptor.offsetPosition = descriptor.vertexSize;
		descriptor.vertexSize += sizeof(float) * 3;

		// rhw / 1-over-z slot follows position
		if (vertexFormat.isTransformed())
		{
			descriptor.offsetOoz = descriptor.vertexSize;
			descriptor.vertexSize += sizeof(float);
		}
		else
		{
			descriptor.offsetOoz = -1;
		}
	}
	else
	{
		DEBUG_FATAL(vertexFormat.isTransformed(), ("Transformed data requires XYZ as well"));
		descriptor.offsetPosition = -1;
	}

	// normal
	if (vertexFormat.hasNormal())
	{
		descriptor.offsetNormal = descriptor.vertexSize;
		descriptor.vertexSize += sizeof(float) * 3;
	}
	else
	{
		descriptor.offsetNormal = -1;
	}

	// point size
	if (vertexFormat.hasPointSize())
	{
		descriptor.offsetPointSize = descriptor.vertexSize;
		descriptor.vertexSize += sizeof(float);
	}
	else
	{
		descriptor.offsetPointSize = -1;
	}

	// color0
	if (vertexFormat.hasColor0())
	{
		descriptor.offsetColor0 = descriptor.vertexSize;
		descriptor.vertexSize += sizeof(uint32);
	}
	else
	{
		descriptor.offsetColor0 = -1;
	}

	// color1
	if (vertexFormat.hasColor1())
	{
		descriptor.offsetColor1 = descriptor.vertexSize;
		descriptor.vertexSize += sizeof(uint32);
	}
	else
	{
		descriptor.offsetColor1 = -1;
	}

	// texture coordinate sets
	const int numberOfTextureCoordinateSets = vertexFormat.getNumberOfTextureCoordinateSets();
	for (int t = 0; t < numberOfTextureCoordinateSets; ++t)
	{
		const int dimension = vertexFormat.getTextureCoordinateSetDimension(t);
		descriptor.offsetTextureCoordinateSet[t] = descriptor.vertexSize;
		descriptor.vertexSize = static_cast<int8>(descriptor.vertexSize + (sizeof(float) * dimension));
	}
	for (int t = numberOfTextureCoordinateSets; t < VertexBufferFormat::MAX_TEXTURE_COORDINATE_SETS; ++t)
		descriptor.offsetTextureCoordinateSet[t] = -1;

	DEBUG_FATAL(descriptor.vertexSize == 0, ("Vertex has no data"));

	DescriptorMap::value_type entry(vertexFormat.getFlags(), descriptor);
	std::pair<DescriptorMap::iterator, bool> result = ms_descriptorMap->insert(entry);
	DEBUG_FATAL(!result.second, ("insert() said entry was already there, but find() didn't locate it"));

	return result.first->second;
}

// ----------------------------------------------------------------------

const VertexBufferDescriptor &Direct3d11_VertexBufferDescriptorMap::getDescriptor(uint32 formatFlags)
{
	NOT_NULL(ms_descriptorMap);
	DescriptorMap::iterator i = ms_descriptorMap->find(formatFlags);
	DEBUG_FATAL(i == ms_descriptorMap->end(), ("Descriptor not found"));
	return i->second;
}

// ======================================================================
