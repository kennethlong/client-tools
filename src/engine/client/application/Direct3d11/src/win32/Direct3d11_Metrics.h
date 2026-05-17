// ======================================================================
//
// Direct3d11_Metrics.h
// Phase 11 D3D11 renderer plugin -- per-frame counters + #ifdef _DEBUG
// reporting overlay. Plan 11-06 (Wave 6).
//
// D3D11-specific counters:
//   * mapCount             -- Map / Unmap pairs per frame (lock budget)
//   * cbufferUpdates       -- Direct3d11_ConstantBuffer::updateXX calls
//   * inputLayoutCacheHits / misses  (Direct3d11_VertexDeclarationMap)
//   * shaderCacheHits / misses       (Direct3d11_ShaderCache)
//   * drawCallCount        -- Draw / DrawIndexed invocations
//
// Per-frame reset in beginFrame; publish via DEBUG_REPORT_LOG_PRINT
// on endFrame gated by DebugFlag ClientGraphics/Direct3d11/reportFrameStats.
//
// Per CONTEXT D-13: no fixed-function counters; SM5.0 unified.
//
// ======================================================================

#ifndef INCLUDED_Direct3d11_Metrics_H
#define INCLUDED_Direct3d11_Metrics_H

// ======================================================================

class Direct3d11_Metrics
{
public:

	static void install();
	static void remove();
	static void beginFrame();
	static void endFrame();

#ifdef _DEBUG
	// Per-frame counters (reset in beginFrame).
	static int vertices;
	static int indices;
	static int triangles;
	static int drawCallCount;
	static int mapCount;
	static int cbufferUpdates;

	// Cumulative cache stats (NOT reset per frame).
	static int inputLayoutCacheHits;
	static int inputLayoutCacheMisses;
	static int shaderCacheHits;
	static int shaderCacheMisses;
#endif
};

// ======================================================================

#endif
