// ======================================================================
//
// Direct3d11_VertexDeclarationMap.cpp
// Phase 11 D3D11 renderer plugin -- input-layout cache implementation.
// Plan 11-06.
//
// ======================================================================

#include "FirstDirect3d11.h"
#include "Direct3d11_VertexDeclarationMap.h"

#include "Direct3d11.h"               // FATAL_DX_HR
#include "Direct3d11_Device.h"
#include "Direct3d11_VertexBufferDescriptorMap.h"
#include "Direct3d11_VertexShaderData.h"

#include "clientGraphics/VertexBufferFormat.h"

#include <map>
#include <utility>

// ======================================================================

using Microsoft::WRL::ComPtr;

namespace Direct3d11_VertexDeclarationMapNamespace
{
	// Cache key: (engine VertexBufferFormat flags, VS bytecode hash).
	// Pitfall 6: cannot use VBFormat alone -- the input layout is validated
	// against the VS bytecode signature at CreateInputLayout time.
	typedef std::pair<uint32, uint64_t> Key;
	typedef std::map<Key, ComPtr<ID3D11InputLayout>> Cache;

	Cache *ms_cache = nullptr;

	// Diagnostics (also incremented by Direct3d11_Metrics when present).
	int ms_cacheHits   = 0;
	int ms_cacheMisses = 0;
}
using namespace Direct3d11_VertexDeclarationMapNamespace;

// ======================================================================

void Direct3d11_VertexDeclarationMap::install()
{
	DEBUG_FATAL(ms_cache, ("Direct3d11_VertexDeclarationMap already installed"));
	ms_cache = new Cache;
	ms_cacheHits = 0;
	ms_cacheMisses = 0;
}

// ----------------------------------------------------------------------

void Direct3d11_VertexDeclarationMap::remove()
{
	DEBUG_REPORT_LOG_PRINT(true,
		("Direct3d11_VertexDeclarationMap: shutdown stats hits=%d misses=%d distinct=%d\n",
		ms_cacheHits, ms_cacheMisses, ms_cache ? static_cast<int>(ms_cache->size()) : 0));
	delete ms_cache;
	ms_cache = nullptr;
}

// ----------------------------------------------------------------------

int Direct3d11_VertexDeclarationMap::getCachedLayoutCount()
{
	return ms_cache ? static_cast<int>(ms_cache->size()) : 0;
}

// ----------------------------------------------------------------------

ID3D11InputLayout *Direct3d11_VertexDeclarationMap::getOrCreate(
	VertexBufferFormat const &format,
	Direct3d11_VertexShaderData const *vsData)
{
	NOT_NULL(ms_cache);

	if (!vsData)
		return nullptr;

	ID3DBlob *bytecode = vsData->getBytecode();
	if (!bytecode)
		return nullptr;     // //asm VS or compile failure path -- draw must skip

	Key const key(format.getFlags(), vsData->getBytecodeHash());

	auto it = ms_cache->find(key);
	if (it != ms_cache->end())
	{
		++ms_cacheHits;
		return it->second.Get();
	}

	++ms_cacheMisses;

	D3D11_INPUT_ELEMENT_DESC elements[16] = {};
	int const elementCount = Direct3d11_VertexBufferDescriptorMap::buildInputElementDesc(format, elements);
	if (elementCount <= 0)
		return nullptr;

	ComPtr<ID3D11InputLayout> layout;
	HRESULT const hr = Direct3d11_Device::getDevice()->CreateInputLayout(
		elements,
		static_cast<UINT>(elementCount),
		bytecode->GetBufferPointer(),
		bytecode->GetBufferSize(),
		layout.GetAddressOf());

	if (FAILED(hr))
	{
		// Mismatch between VBFormat and the VS input signature. This is
		// not fatal here -- the engine may try a fallback shader. Log and
		// return nullptr; the draw-dispatch will skip the call.
		DEBUG_REPORT_LOG_PRINT(true,
			("Direct3d11_VertexDeclarationMap::CreateInputLayout failed (format=0x%08x, vsHash=0x%016llx): %s\n",
			format.getFlags(),
			static_cast<unsigned long long>(vsData->getBytecodeHash()),
			Direct3d11Namespace::hresultString(hr)));
		return nullptr;
	}

	auto inserted = ms_cache->emplace(key, layout);
	DEBUG_FATAL(!inserted.second, ("Direct3d11_VertexDeclarationMap: cache insert collision"));
	return inserted.first->second.Get();
}

// ======================================================================
