// ======================================================================
//
// Direct3d11_Metrics.cpp
// Phase 11 D3D11 renderer plugin -- per-frame counters + reporting.
// Plan 11-06 (Wave 6).
//
// ======================================================================

#include "FirstDirect3d11.h"
#include "Direct3d11_Metrics.h"

#include "sharedDebug/DebugFlags.h"

// ======================================================================

#ifdef _DEBUG
int Direct3d11_Metrics::vertices               = 0;
int Direct3d11_Metrics::indices                = 0;
int Direct3d11_Metrics::triangles              = 0;
int Direct3d11_Metrics::drawCallCount          = 0;
int Direct3d11_Metrics::mapCount               = 0;
int Direct3d11_Metrics::cbufferUpdates         = 0;
int Direct3d11_Metrics::inputLayoutCacheHits   = 0;
int Direct3d11_Metrics::inputLayoutCacheMisses = 0;
int Direct3d11_Metrics::shaderCacheHits        = 0;
int Direct3d11_Metrics::shaderCacheMisses      = 0;
#endif

namespace Direct3d11_MetricsNamespace
{
#ifdef _DEBUG
	bool ms_reportFrameStats = false;
#endif
}
using namespace Direct3d11_MetricsNamespace;

// ======================================================================

void Direct3d11_Metrics::install()
{
#ifdef _DEBUG
	DebugFlags::registerFlag(ms_reportFrameStats,
		"ClientGraphics/Direct3d11", "reportFrameStats");
#endif
}

// ----------------------------------------------------------------------

void Direct3d11_Metrics::remove()
{
#ifdef _DEBUG
	DebugFlags::unregisterFlag(ms_reportFrameStats);

	DEBUG_REPORT_LOG_PRINT(true,
		("Direct3d11_Metrics: cumulative inputLayoutCache hits=%d misses=%d  shaderCache hits=%d misses=%d\n",
		inputLayoutCacheHits, inputLayoutCacheMisses,
		shaderCacheHits,      shaderCacheMisses));
#endif
}

// ----------------------------------------------------------------------

void Direct3d11_Metrics::beginFrame()
{
#ifdef _DEBUG
	vertices       = 0;
	indices        = 0;
	triangles      = 0;
	drawCallCount  = 0;
	mapCount       = 0;
	cbufferUpdates = 0;
#endif
}

// ----------------------------------------------------------------------

void Direct3d11_Metrics::endFrame()
{
#ifdef _DEBUG
	if (ms_reportFrameStats)
	{
		DEBUG_REPORT_LOG_PRINT(true,
			("Direct3d11 frame: draws=%d tri=%d v=%d i=%d map=%d cbufUpd=%d  ilCache(h/m)=%d/%d  shCache(h/m)=%d/%d\n",
			drawCallCount, triangles, vertices, indices,
			mapCount, cbufferUpdates,
			inputLayoutCacheHits, inputLayoutCacheMisses,
			shaderCacheHits,      shaderCacheMisses));
	}
#endif
}

// ======================================================================
