// ======================================================================
//
// Direct3d11_ShaderCache.cpp
// Phase 11 D3D11 renderer plugin -- D-03 hybrid shader compile cache impl.
//
// Plan 11-05 (Wave 5 -- shader layer).
//
// Cache path = <cacheDir>/<hash:016x>.cso. Defines participate in the FNV-1a
// 64-bit hash; cache directory is engine-configured via ConfigDirect3d11
// (NEVER user-input -- defense in depth around path traversal, T-11-05-01).
//
// Per RESEARCH §Don't Hand-Roll: the .cso blob is opaque & self-versioning;
// no format game. Just hash + dump + load.
//
// store() failures are NON-FATAL (cache is opportunistic; next launch just
// recompiles). install() creates the directory lazily.
//
// ======================================================================

#include "FirstDirect3d11.h"
#include "Direct3d11_ShaderCache.h"

#include "sharedDebug/DebugFlags.h"
#include "sharedFoundation/Os.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <filesystem>

// ======================================================================

namespace Direct3d11_ShaderCacheNamespace
{
	bool        ms_installed = false;
	std::string ms_cacheDir;
	int         ms_hits      = 0;
	int         ms_misses    = 0;
	int         ms_stores    = 0;
	int         ms_storeFailures = 0;

	// FNV-1a 64-bit constants.
	constexpr uint64_t kFnv1aOffset = 14695981039346656037ULL;
	constexpr uint64_t kFnv1aPrime  = 1099511628211ULL;

	// Mix a single byte into the FNV-1a accumulator.
	inline uint64_t fnv1aStep(uint64_t hash, unsigned char b)
	{
		hash ^= static_cast<uint64_t>(b);
		hash *= kFnv1aPrime;
		return hash;
	}

	// Mix a NUL-terminated string (NOT including its NUL) into the FNV-1a
	// accumulator. Used by hashSource for define names/values.
	inline uint64_t fnv1aString(uint64_t hash, char const *s)
	{
		if (!s)
			return hash;
		while (*s)
		{
			hash = fnv1aStep(hash, static_cast<unsigned char>(*s));
			++s;
		}
		return hash;
	}

	// Build the per-hash cache filename: <cacheDir>/<hash:016x>.cso.
	// Defensive validation: hex digits only (this is our own hex output,
	// but defense-in-depth keeps T-11-05-01 trivially mitigated).
	std::string buildCachePath(uint64_t hash)
	{
		char hexBuf[17];
		std::snprintf(hexBuf, sizeof(hexBuf), "%016llx", static_cast<unsigned long long>(hash));
		// Defensive scan -- T-11-05-01 mitigation.
		for (int i = 0; i < 16; ++i)
		{
			char const c = hexBuf[i];
			DEBUG_FATAL(!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')),
				("Direct3d11_ShaderCache: non-hex char '%c' in cache filename (hash=%016llx)",
				c, static_cast<unsigned long long>(hash)));
		}

		std::string path = ms_cacheDir;
		if (!path.empty() && path.back() != '/' && path.back() != '\\')
			path += '/';
		path += hexBuf;
		path += ".cso";
		return path;
	}
}
using namespace Direct3d11_ShaderCacheNamespace;

// ======================================================================

void Direct3d11_ShaderCache::install(char const *cacheDir)
{
	DEBUG_FATAL(ms_installed, ("Direct3d11_ShaderCache already installed"));
	NOT_NULL(cacheDir);

	ms_cacheDir = cacheDir;
	ms_installed = true;
	ms_hits = 0;
	ms_misses = 0;
	ms_stores = 0;
	ms_storeFailures = 0;

	// Lazy directory creation. Failure here is non-fatal -- subsequent
	// store() calls will quietly log and skip persisting; tryLoad() will
	// always return miss; compile-every-time still works.
	std::error_code ec;
	std::filesystem::create_directories(ms_cacheDir, ec);
	if (ec)
	{
		DEBUG_REPORT_LOG_PRINT(true,
			("Direct3d11_ShaderCache: could not create cache dir '%s' (%s); cache disabled for this run\n",
			ms_cacheDir.c_str(), ec.message().c_str()));
	}
	else
	{
		DEBUG_REPORT_LOG_PRINT(true,
			("Direct3d11_ShaderCache: install at '%s'\n", ms_cacheDir.c_str()));
	}
}

// ----------------------------------------------------------------------

void Direct3d11_ShaderCache::remove()
{
	DEBUG_REPORT_LOG_PRINT(true,
		("Direct3d11_ShaderCache: remove -- hits=%d misses=%d stores=%d storeFailures=%d\n",
		ms_hits, ms_misses, ms_stores, ms_storeFailures));

	ms_cacheDir.clear();
	ms_installed = false;
	// Cache files on disk are intentionally NOT deleted (D-03 -- cache
	// survives across launches).
}

// ----------------------------------------------------------------------

uint64_t Direct3d11_ShaderCache::hashSource(char const *text, size_t length, D3D_SHADER_MACRO const *defines)
{
	uint64_t hash = kFnv1aOffset;

	// Hash the source body byte-wise.
	if (text)
	{
		for (size_t i = 0; i < length; ++i)
			hash = fnv1aStep(hash, static_cast<unsigned char>(text[i]));
	}

	// Mix defines: Name + '\0' + Definition + '\0' for each entry.
	// Trailing nulls disambiguate {ABC, DEF} from {ABCDEF, ""}.
	if (defines)
	{
		for (D3D_SHADER_MACRO const *p = defines; p->Name != nullptr; ++p)
		{
			hash = fnv1aString(hash, p->Name);
			hash = fnv1aStep(hash, 0);
			hash = fnv1aString(hash, p->Definition);
			hash = fnv1aStep(hash, 0);
		}
	}

	return hash;
}

// ----------------------------------------------------------------------

bool Direct3d11_ShaderCache::tryLoad(uint64_t sourceHash, Microsoft::WRL::ComPtr<ID3DBlob> &outBlob)
{
	if (!ms_installed || ms_cacheDir.empty())
	{
		++ms_misses;
		return false;
	}

	std::string const path = buildCachePath(sourceHash);

	FILE *f = std::fopen(path.c_str(), "rb");
	if (!f)
	{
		++ms_misses;
		return false;
	}

	if (std::fseek(f, 0, SEEK_END) != 0)
	{
		std::fclose(f);
		++ms_misses;
		return false;
	}
	long const sizeLong = std::ftell(f);
	if (sizeLong <= 0 || sizeLong > (64 * 1024 * 1024))
	{
		// Empty / oversized cache files are treated as misses; we will
		// overwrite them on store(). 64 MB is sanity-ceiling for any
		// single shader; a SWG-era HLSL shader compiles to a few KB.
		std::fclose(f);
		++ms_misses;
		return false;
	}
	std::fseek(f, 0, SEEK_SET);

	ID3DBlob *blob = nullptr;
	HRESULT hr = D3DCreateBlob(static_cast<SIZE_T>(sizeLong), &blob);
	if (FAILED(hr) || !blob)
	{
		std::fclose(f);
		++ms_misses;
		return false;
	}

	size_t const got = std::fread(blob->GetBufferPointer(), 1, static_cast<size_t>(sizeLong), f);
	std::fclose(f);
	if (got != static_cast<size_t>(sizeLong))
	{
		blob->Release();
		++ms_misses;
		return false;
	}

	outBlob.Attach(blob);  // takes ownership; ComPtr Release on dtor
	++ms_hits;
	return true;
}

// ----------------------------------------------------------------------

void Direct3d11_ShaderCache::store(uint64_t sourceHash, ID3DBlob *blob)
{
	if (!ms_installed || ms_cacheDir.empty() || !blob)
		return;

	std::string const path = buildCachePath(sourceHash);

	FILE *f = std::fopen(path.c_str(), "wb");
	if (!f)
	{
		++ms_storeFailures;
		DEBUG_REPORT_LOG_PRINT(true,
			("Direct3d11_ShaderCache: store FAILED to open '%s' for write (errno=%d) -- cache miss next launch (non-fatal)\n",
			path.c_str(), errno));
		return;
	}

	size_t const bytes = static_cast<size_t>(blob->GetBufferSize());
	size_t const wrote = std::fwrite(blob->GetBufferPointer(), 1, bytes, f);
	std::fclose(f);

	if (wrote != bytes)
	{
		++ms_storeFailures;
		DEBUG_REPORT_LOG_PRINT(true,
			("Direct3d11_ShaderCache: store SHORT WRITE '%s' wanted=%zu got=%zu -- cache miss next launch (non-fatal)\n",
			path.c_str(), bytes, wrote));
		// Best-effort: try to remove the truncated file so it doesn't
		// poison future tryLoad attempts.
		std::error_code ec;
		std::filesystem::remove(path, ec);
		return;
	}

	++ms_stores;
}

// ======================================================================
