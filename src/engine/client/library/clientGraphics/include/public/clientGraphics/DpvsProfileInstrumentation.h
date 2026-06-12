// ======================================================================
//
// DpvsProfileInstrumentation.h
// copyright 2026 whitengold
//
// Phase 23 -- DPVS D3D11 remeasure. Restored from git tag
// phase-10-instrumentation-pre-cleanup (commit 9f2ec3715); the Phase 10
// module was deleted as THROWAWAY per D-15 then brought back to re-run the
// occlusion-culling A/B under the D3D11 renderer (DPVS-01).
//
// Per-frame DPVS occlusion-culling cost capture: CSV writer state,
// run-label state (with filename + CSV/formula sanitization), F10
// capture toggle state, ExitChain teardown, and a DebugFlag-driven
// on-screen overlay print routine. Consumed by RenderWorld + the F10/F11
// keybind hooks (Plan 02). All static surface; no instances exist.
//
// ======================================================================

#ifndef INCLUDED_DpvsProfileInstrumentation_H
#define INCLUDED_DpvsProfileInstrumentation_H

// ======================================================================

#include <string>

// ======================================================================

// Phase 23 -- DPVS D3D11 remeasure (restored from Phase 10 recovery tag)
class DpvsProfileInstrumentation
{
public:

	static void install();
	static void remove();

	// Called from Game::run() after Graphics::present() each frame (Plan 02).
	// totalFrameMs is the just-completed frame duration.
	static void onFrameEnd(float totalFrameMs);

	// Called from CuiIoWin F10 hook (Plan 02, #ifdef _DEBUG only).
	static void toggleCapture();

	// Called from /setrunlabel console command (Plan 02). Sanitized
	// per RESEARCH.md Security Domain before stored / used as filename.
	static void setRunLabel(std::string const & label);

	// Called from RenderWorld around resolveVisibility() each frame (Plan 02).
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
