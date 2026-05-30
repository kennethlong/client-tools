// ======================================================================
//
// Direct3d11_VertexBufferDescriptorMap.cpp
// Phase 11 D3D11 renderer plugin -- VertexBufferDescriptor cache.
// Plan 11-04. See header for rationale.
//
// ======================================================================

#include "FirstDirect3d11.h"
#include "Direct3d11_VertexBufferDescriptorMap.h"

#include "Direct3d11_VertexShaderData.h"   // Plan 11-09.8: Direct3d11_ReflectedVSInput full definition

#include "clientGraphics/VertexBufferFormat.h"
#include "clientGraphics/VertexBufferDescriptor.h"

#include <cstring>                          // Plan 11-09.8: _stricmp (Windows MSVC CRT extension)
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
	UINT inputSlot,
	UINT firstTexCoordSemanticIndex)
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
		// Plan 17-08 (GAP-6): GLOBAL TEXCOORD usage index across streams (mirror
		// D3D9 Direct3d9_VertexDeclarationMap.cpp:119-224). Per-stream-local `t`
		// would relabel stream 1's skinned DOT3 tangent as TEXCOORD0; the bump VS
		// declares it as TEXCOORD2 -> uncovered -> phantom-zeroed -> normalize(0)
		// -> NaN COLOR0. Offsetting by firstTexCoordSemanticIndex makes the
		// semantic index match what the VS (and D3D9) expect.
		add("TEXCOORD", firstTexCoordSemanticIndex + static_cast<UINT>(t), dxgi, bytes);
	}

	DEBUG_FATAL(n == 0, ("Direct3d11_VertexBufferDescriptorMap::buildInputElementDesc: empty layout"));
	return n;
}

// ======================================================================
// Plan 11-09.8: phantom-element augmentation.
//
// CreateInputLayout under vs_4_0 validates the layout against the bytecode
// input signature strictly. Engine D3D9-era HLSL declares TEXCOORD inputs
// even when unused; vs_4_0 preserves them in the signature. When the
// VBFormat-derived elements (above) don't cover every declared input, the
// caller MUST add phantom elements that match -- otherwise CreateInputLayout
// returns FAIL.
//
// Per CODEX 11-09.8 consult Q4: phantom elements live at InputSlot=15
// (engine uses low slots for real streams; D3D11 exposes 16 IA slots).
// The phantom buffer (Direct3d11_Device::getPhantomZeroBuffer) is a 16-byte
// zero-filled VB bound with stride=0 -- every vertex reads zero, so the
// VS sees zero for any phantom-backed input.
//
// CODEX Q3: filter system-value inputs (already done by VertexShaderData
// when populating reflectedInputs; defensive double-check here).

namespace
{
	enum { kPhantomInputSlot = 15 };

	// Map reflected (ComponentType, ComponentMask) to a DXGI_FORMAT.
	// CODEX Q4: phantom elements read zero from a stride-0 buffer; format
	// type-equivalence with the source matters for SM4 signature validation
	// (the layout must match the signature's per-element component type).
	// Mask width 1/2/3/4 -> R/RG/RGB/RGBA channels.
	DXGI_FORMAT reflectedToDxgi(D3D_REGISTER_COMPONENT_TYPE type, UINT mask)
	{
		// Determine channel count from the mask. The reflected mask packs
		// the active component lanes; we take the highest set bit + 1.
		int channels = 0;
		if (mask & 0x1) channels = 1;
		if (mask & 0x2) channels = 2;
		if (mask & 0x4) channels = 3;
		if (mask & 0x8) channels = 4;
		if (channels == 0) channels = 4;   // defensive

		switch (type)
		{
			case D3D_REGISTER_COMPONENT_UINT32:
				switch (channels)
				{
					case 1: return DXGI_FORMAT_R32_UINT;
					case 2: return DXGI_FORMAT_R32G32_UINT;
					case 3: return DXGI_FORMAT_R32G32B32_UINT;
					default: return DXGI_FORMAT_R32G32B32A32_UINT;
				}
			case D3D_REGISTER_COMPONENT_SINT32:
				switch (channels)
				{
					case 1: return DXGI_FORMAT_R32_SINT;
					case 2: return DXGI_FORMAT_R32G32_SINT;
					case 3: return DXGI_FORMAT_R32G32B32_SINT;
					default: return DXGI_FORMAT_R32G32B32A32_SINT;
				}
			case D3D_REGISTER_COMPONENT_FLOAT32:
			default:
				switch (channels)
				{
					case 1: return DXGI_FORMAT_R32_FLOAT;
					case 2: return DXGI_FORMAT_R32G32_FLOAT;
					case 3: return DXGI_FORMAT_R32G32B32_FLOAT;
					default: return DXGI_FORMAT_R32G32B32A32_FLOAT;
				}
		}
	}
}

int Direct3d11_VertexBufferDescriptorMap::augmentWithPhantomElements(
	std::vector<Direct3d11_ReflectedVSInput> const &reflectedInputs,
	D3D11_INPUT_ELEMENT_DESC *outDesc,
	int currentCount,
	int maxElements)
{
	NOT_NULL(outDesc);

	int n = currentCount;
	for (auto const & ri : reflectedInputs)
	{
		// Skip if format-derived elements already cover this semantic.
		bool covered = false;
		for (int i = 0; i < n; ++i)
		{
			D3D11_INPUT_ELEMENT_DESC const &existing = outDesc[i];
			if (existing.SemanticName == nullptr)
				continue;
			if (existing.SemanticIndex != ri.SemanticIndex)
				continue;
			if (_stricmp(existing.SemanticName, ri.SemanticName) != 0)
				continue;
			covered = true;
			break;
		}
		if (covered)
			continue;

		// Append phantom element at InputSlot=15.
		if (n >= maxElements)
		{
			DEBUG_REPORT_LOG_PRINT(true,
				("Direct3d11_VertexBufferDescriptorMap::augmentWithPhantomElements: "
				 "max element capacity %d exceeded -- VS signature has more inputs than the layout can hold\n",
				 maxElements));
			return -1;
		}

		// The SemanticName pointer in D3D11_INPUT_ELEMENT_DESC must remain
		// valid for the lifetime of the layout. Direct3d11_ReflectedVSInput
		// owns its SemanticName storage (char array, not pointer) and lives
		// in the VS data's vector, which the cache key (vsBytecodeHash)
		// implicitly keeps alive for the input-layout lifetime. So storing
		// the pointer to the ReflectedInput's char array is safe.
		D3D11_INPUT_ELEMENT_DESC &d = outDesc[n++];
		d.SemanticName         = ri.SemanticName;
		d.SemanticIndex        = ri.SemanticIndex;
		d.Format               = reflectedToDxgi(ri.ComponentType, ri.ComponentMask);
		d.InputSlot            = kPhantomInputSlot;
		d.AlignedByteOffset    = 0;
		d.InputSlotClass       = D3D11_INPUT_PER_VERTEX_DATA;
		d.InstanceDataStepRate = 0;
	}

	return n;
}

// ======================================================================
