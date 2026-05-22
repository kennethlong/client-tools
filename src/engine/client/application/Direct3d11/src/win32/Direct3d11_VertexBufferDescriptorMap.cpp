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
//
// Plan 11-06: build D3D11_INPUT_ELEMENT_DESC[] for the given engine
// VertexBufferFormat. Mirrors the layout that getDescriptor() computes for
// vertexSize / offsetXxx -- same component order, same byte offsets. This
// is the second half of the input-layout cache (Pitfall 6); the first half
// (per-format descriptor cache) is shared with Plan 11-04.
//
// Engine vertex layout (matches getDescriptor offset table above):
//   POSITION (float3)        if hasPosition
//   RHW      (float)         if hasPosition AND isTransformed
//   NORMAL   (float3)        if hasNormal
//   PSIZE    (float)         if hasPointSize
//   COLOR0   (uint32 BGRA)   if hasColor0
//   COLOR1   (uint32 BGRA)   if hasColor1
//   TEXCOORDn (float<dim>)   for n in [0, numberOfTextureCoordinateSets)
//
// D3D9 D3DCOLOR is ARGB packed; engine PackedArgb mirrors that. DXGI
// equivalent is DXGI_FORMAT_B8G8R8A8_UNORM which the shader receives in
// .bgra channel order (HLSL sees .rgba => engine's a, r, g, b -- which is
// the same swizzle the D3D9 plugin relies on by convention).
// ======================================================================

int Direct3d11_VertexBufferDescriptorMap::buildInputElementDesc(
	VertexBufferFormat const &format,
	D3D11_INPUT_ELEMENT_DESC outDesc[16])
{
	// Existing single-stream caller path -- delegates to the
	// multi-stream variant with inputSlot=0 + full 16-element capacity.
	int const n = buildInputElementDescForStream(format, outDesc, 16, 0);
	DEBUG_FATAL(n <= 0, ("Direct3d11_VertexBufferDescriptorMap::buildInputElementDesc: empty layout"));
	return n;
}

// ----------------------------------------------------------------------

int Direct3d11_VertexBufferDescriptorMap::buildInputElementDescForStream(
	VertexBufferFormat const &format,
	D3D11_INPUT_ELEMENT_DESC *outDesc,
	int maxElements,
	UINT inputSlot)
{
	NOT_NULL(outDesc);

	int n = 0;          // running element count (caps at maxElements)
	UINT offset = 0;    // running byte offset within one vertex (per-stream)

	auto add = [&](char const *semantic, UINT semanticIndex, DXGI_FORMAT fmt, UINT bytes) -> bool
	{
		if (n >= maxElements)
			return false;
		D3D11_INPUT_ELEMENT_DESC &d = outDesc[n++];
		d.SemanticName         = semantic;
		d.SemanticIndex        = semanticIndex;
		d.Format               = fmt;
		d.InputSlot            = inputSlot;
		d.AlignedByteOffset    = offset;
		d.InputSlotClass       = D3D11_INPUT_PER_VERTEX_DATA;
		d.InstanceDataStepRate = 0;
		offset += bytes;
		return true;
	};

	if (format.hasPosition())
	{
		if (format.isTransformed())
		{
			// Transformed vertices use the SV_POSITION-equivalent xyzrhw
			// layout; pack as float4 covering xyz + rhw together. We emit a
			// single POSITION float4 instead of POSITION float3 + a separate
			// RHW float, because the layout that getDescriptor lays out keeps
			// them adjacent (12 + 4 = 16 bytes). Same byte cost; cleaner HLSL.
			add("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, sizeof(float) * 4);
		}
		else
		{
			add("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, sizeof(float) * 3);
		}
	}

	if (format.hasNormal())
		add("NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, sizeof(float) * 3);

	if (format.hasPointSize())
		add("PSIZE", 0, DXGI_FORMAT_R32_FLOAT, sizeof(float));

	if (format.hasColor0())
		add("COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, sizeof(uint32));

	if (format.hasColor1())
		add("COLOR", 1, DXGI_FORMAT_B8G8R8A8_UNORM, sizeof(uint32));

	const int numberOfTexCoordSets = format.getNumberOfTextureCoordinateSets();
	for (int t = 0; t < numberOfTexCoordSets; ++t)
	{
		const int dim = format.getTextureCoordinateSetDimension(t);
		DXGI_FORMAT dxgi = DXGI_FORMAT_UNKNOWN;
		UINT bytes = 0;
		switch (dim)
		{
			case 1: dxgi = DXGI_FORMAT_R32_FLOAT;           bytes = sizeof(float) * 1; break;
			case 2: dxgi = DXGI_FORMAT_R32G32_FLOAT;        bytes = sizeof(float) * 2; break;
			case 3: dxgi = DXGI_FORMAT_R32G32B32_FLOAT;     bytes = sizeof(float) * 3; break;
			case 4: dxgi = DXGI_FORMAT_R32G32B32A32_FLOAT;  bytes = sizeof(float) * 4; break;
			default:
				DEBUG_FATAL(true, ("Direct3d11_VertexBufferDescriptorMap::buildInputElementDesc: invalid texcoord dim=%d", dim));
				break;
		}
		add("TEXCOORD", static_cast<UINT>(t), dxgi, bytes);
	}

	DEBUG_FATAL(n == 0, ("Direct3d11_VertexBufferDescriptorMap::buildInputElementDesc: empty layout"));
	return n;
}

// ======================================================================
