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
#include <tuple>
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

	// Plan 11-09.7: multi-stream cache. Key composed from per-stream
	// VertexBufferFormat flags (stream 0 and stream 1; up to MAX_VERTEX_BUFFERS=2
	// per Direct3d11_VertexBufferVectorData) plus VS bytecode hash. Held
	// separately from ms_cache to keep the existing single-stream Key type
	// untouched and avoid (theoretical) collisions where a multi-stream
	// composite-flag pattern might match a single-stream flag pattern.
	typedef std::tuple<uint32, uint32, uint64_t> MultiStreamKey;
	typedef std::map<MultiStreamKey, ComPtr<ID3D11InputLayout>> MultiStreamCache;
	MultiStreamCache *ms_multiStreamCache = nullptr;
	int ms_multiStreamCacheHits   = 0;
	int ms_multiStreamCacheMisses = 0;
}
using namespace Direct3d11_VertexDeclarationMapNamespace;

// ======================================================================

void Direct3d11_VertexDeclarationMap::install()
{
	DEBUG_FATAL(ms_cache, ("Direct3d11_VertexDeclarationMap already installed"));
	ms_cache = new Cache;
	ms_cacheHits = 0;
	ms_cacheMisses = 0;

	// Plan 11-09.7: multi-stream cache.
	DEBUG_FATAL(ms_multiStreamCache, ("Direct3d11_VertexDeclarationMap multi-stream cache already installed"));
	ms_multiStreamCache = new MultiStreamCache;
	ms_multiStreamCacheHits = 0;
	ms_multiStreamCacheMisses = 0;
}

// ----------------------------------------------------------------------

void Direct3d11_VertexDeclarationMap::remove()
{
	DEBUG_REPORT_LOG_PRINT(true,
		("Direct3d11_VertexDeclarationMap: shutdown stats hits=%d misses=%d distinct=%d  "
		 "multi-stream hits=%d misses=%d distinct=%d\n",
		ms_cacheHits, ms_cacheMisses, ms_cache ? static_cast<int>(ms_cache->size()) : 0,
		ms_multiStreamCacheHits, ms_multiStreamCacheMisses,
		ms_multiStreamCache ? static_cast<int>(ms_multiStreamCache->size()) : 0));
	delete ms_cache;
	ms_cache = nullptr;
	delete ms_multiStreamCache;
	ms_multiStreamCache = nullptr;
}

// ----------------------------------------------------------------------

int Direct3d11_VertexDeclarationMap::getCachedLayoutCount()
{
	int const single = ms_cache ? static_cast<int>(ms_cache->size()) : 0;
	int const multi  = ms_multiStreamCache ? static_cast<int>(ms_multiStreamCache->size()) : 0;
	return single + multi;
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
	int elementCount = Direct3d11_VertexBufferDescriptorMap::buildInputElementDesc(format, elements);
	if (elementCount <= 0)
		return nullptr;

	// Plan 11-09.8: augment with phantom elements (InputSlot=15) for any
	// VS-declared inputs the VBFormat doesn't cover. vs_4_0 preserves
	// unused declared inputs in the bytecode signature; CreateInputLayout
	// rejects layouts that don't match the signature. CODEX-reviewed.
	elementCount = Direct3d11_VertexBufferDescriptorMap::augmentWithPhantomElements(
		vsData->getReflectedInputs(), elements, elementCount, 16);
	if (elementCount < 0)
		return nullptr;   // overflow -- VS signature exceeds 16-element layout cap

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

// ----------------------------------------------------------------------
// Plan 11-09.7: multi-stream input-layout creation + cache.
//
// Mirrors the single-stream path with these differences:
//   1. Cache is ms_multiStreamCache (separate from single-stream cache).
//   2. Key composed from per-stream VBFormat flags + VS bytecode hash.
//   3. D3D11_INPUT_ELEMENT_DESC array assembled via per-stream calls to
//      buildInputElementDescForStream with InputSlot=streamIndex.
//
// streamCount in [1, 2]; values outside trip a DEBUG_FATAL. The lower
// bound being 1 means single-stream callers can technically use this
// API too -- but for clarity they should call the single-stream
// getOrCreate above; this method is intended for VertexBufferVector
// callers from Direct3d11_StateCache::applyPreDrawState.

ID3D11InputLayout *Direct3d11_VertexDeclarationMap::getOrCreateMultiStream(
	VertexBufferFormat const * const *streamFormats,
	int streamCount,
	Direct3d11_VertexShaderData const *vsData)
{
	NOT_NULL(ms_multiStreamCache);
	NOT_NULL(streamFormats);

	DEBUG_FATAL(streamCount < 1 || streamCount > 2,
		("Direct3d11_VertexDeclarationMap::getOrCreateMultiStream: streamCount=%d outside [1,2]",
		streamCount));

	if (!vsData)
		return nullptr;

	ID3DBlob *bytecode = vsData->getBytecode();
	if (!bytecode)
		return nullptr;

	uint32 const flags0 = streamFormats[0] ? streamFormats[0]->getFlags() : 0u;
	uint32 const flags1 = (streamCount >= 2 && streamFormats[1]) ? streamFormats[1]->getFlags() : 0u;
	MultiStreamKey const key(flags0, flags1, vsData->getBytecodeHash());

	auto it = ms_multiStreamCache->find(key);
	if (it != ms_multiStreamCache->end())
	{
		++ms_multiStreamCacheHits;
		return it->second.Get();
	}

	++ms_multiStreamCacheMisses;

	D3D11_INPUT_ELEMENT_DESC elements[16] = {};
	int total = 0;
	// Plan 17-08 (GAP-6): GLOBAL running TEXCOORD usage index across streams,
	// mirroring D3D9 Direct3d9_VertexDeclarationMap.cpp:119-224. Stream 1's
	// skinned DOT3 tangent must surface as TEXCOORD2 (not a per-stream-local
	// TEXCOORD0) so the bump VS's TEXCOORD2 input binds to real data instead of
	// being phantom-zeroed (-> normalize(0) -> NaN COLOR0 -> wrong sleeves/hands).
	UINT runningTexCoord = 0;
	for (int s = 0; s < streamCount; ++s)
	{
		if (!streamFormats[s])
			continue;
		int const remaining = 16 - total;
		int const n = Direct3d11_VertexBufferDescriptorMap::buildInputElementDescForStream(
			*streamFormats[s],
			elements + total,
			remaining,
			static_cast<UINT>(s),
			runningTexCoord);
		if (n <= 0)
		{
			// buildInputElementDescForStream returns 0 if format is empty OR
			// negative if capacity exceeded. Either way, the layout is
			// unsalvageable; skip the draw call (caller contract).
			DEBUG_REPORT_LOG_PRINT(true,
				("Direct3d11_VertexDeclarationMap::getOrCreateMultiStream: stream %d build returned %d (remaining cap=%d)\n",
				s, n, remaining));
			return nullptr;
		}
		total += n;
		runningTexCoord += static_cast<UINT>(streamFormats[s]->getNumberOfTextureCoordinateSets());
	}

	if (total == 0)
		return nullptr;

	// Plan 11-09.8: augment with phantom elements (InputSlot=15) for any
	// VS-declared inputs the multi-stream VBFormats don't cover. Same
	// reasoning as single-stream getOrCreate above.
	total = Direct3d11_VertexBufferDescriptorMap::augmentWithPhantomElements(
		vsData->getReflectedInputs(), elements, total, 16);
	if (total < 0)
		return nullptr;   // overflow

	ComPtr<ID3D11InputLayout> layout;
	HRESULT const hr = Direct3d11_Device::getDevice()->CreateInputLayout(
		elements,
		static_cast<UINT>(total),
		bytecode->GetBufferPointer(),
		bytecode->GetBufferSize(),
		layout.GetAddressOf());

	if (FAILED(hr))
	{
		DEBUG_REPORT_LOG_PRINT(true,
			("Direct3d11_VertexDeclarationMap::CreateInputLayout (multi-stream) failed "
			 "(stream0Flags=0x%08x, stream1Flags=0x%08x, vsHash=0x%016llx): %s\n",
			flags0, flags1,
			static_cast<unsigned long long>(vsData->getBytecodeHash()),
			Direct3d11Namespace::hresultString(hr)));
		return nullptr;
	}

	auto inserted = ms_multiStreamCache->emplace(key, layout);
	DEBUG_FATAL(!inserted.second, ("Direct3d11_VertexDeclarationMap: multi-stream cache insert collision"));
	return inserted.first->second.Get();
}

// ======================================================================
