// ======================================================================
//
// DpvsProfileInstrumentation.h
// copyright 2026 whitengold
//
// Phase 10 -- DPVS profiling instrumentation (THROWAWAY; removed per
// .planning/phases/10-dpvs-culling-experiment/10-CONTEXT.md D-15).
//
// Per-frame DPVS occlusion-culling cost capture: CSV writer state,
// run-label state (with filename + CSV/formula sanitization), F10
// capture toggle state, ExitChain teardown, and a DebugFlag-driven
// on-screen overlay print routine. Consumed by RenderWorld + the F10/F11
// keybind hooks (plan 10-04). All static surface; no instances exist.
//
// ======================================================================

#ifndef INCLUDED_DpvsProfileInstrumentation_H
#define INCLUDED_DpvsProfileInstrumentation_H

// ======================================================================

#include <string>

// ======================================================================

// Phase 10 -- DPVS profiling instrumentation (THROWAWAY; D-15 cleanup target)
class DpvsProfileInstrumentation
{
public:

	static void install();
	static void remove();

	// Called from Game::run() after Graphics::present() each frame (plan 10-04;
	// per RESEARCH.md A8). totalFrameMs is the just-completed frame duration.
	static void onFrameEnd(float totalFrameMs);

	// Called from CuiIoWin F10 hook (plan 10-04, #ifdef _DEBUG only).
	static void toggleCapture();

	// Called from /setrunlabel console command (plan 10-04). Sanitized
	// per RESEARCH.md Security Domain before stored / used as filename.
	static void setRunLabel(std::string const & label);

	// Called from RenderWorld around resolveVisibility() each frame (plan 10-04).
	// Cached values folded into the CSV row written by onFrameEnd().
	static void recordCpuQpcUs(uint32 us);
	static void recordProfilerUs(uint32 us);
	static void recordVisibleObjectCount(int count);

	// Overlay accessors -- consumed by the DebugFlag report routine.
	static bool getCaptureActive();
	static char const * getRunLabel();
	static int getCapturedFrameCount();

private:

	// Disable -- all-statics class (mirrors Graphics.h pattern).
	DpvsProfileInstrumentation();
};

// ======================================================================

#endif
