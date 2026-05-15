// ======================================================================
//
// DpvsProfileInstrumentation.cpp
// copyright 2026 whitengold
//
// Phase 10 -- DPVS profiling instrumentation (THROWAWAY; D-15 cleanup target).
//
// State + CSV writer + on-screen overlay + run-label sanitization. Consumed
// by RenderWorld + Game::run + CuiIoWin keybind hooks in plan 10-04.
// See .planning/phases/10-dpvs-culling-experiment/10-RESEARCH.md §Code
// Examples (lines 442-640) for the canonical pattern this implements, and
// §Security Domain (lines 746-768) for the sanitizer rules.
//
// ======================================================================

#include "clientGraphics/FirstClientGraphics.h"
#include "clientGraphics/DpvsProfileInstrumentation.h"

#include "clientGraphics/Graphics.h"
#include "clientGraphics/RenderWorld.h"
#include "fileInterface/StdioFile.h"
#include "sharedDebug/DebugFlags.h"
#include "sharedFoundation/ConfigFile.h"
#include "sharedFoundation/ExitChain.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

// ======================================================================
// Phase 10 -- DPVS profiling instrumentation (THROWAWAY; D-15)
namespace DpvsProfileInstrumentationNamespace
{
	bool          ms_installed;
	bool          ms_captureActive;
	bool          ms_reportOverlay;        // DebugFlag toggle (registered with DebugFlags)
	std::string   ms_runLabel;
	std::string   ms_csvDir;               // from ConfigFile [Dpvs/Experiment] csvDir
	int           ms_capturedFrames;       // rows written into the current file
	int           ms_firstFrameInFile;     // frame number when current CSV was opened
	AbstractFile *ms_csv;                  // NULL between captures
	uint32        ms_lastCpuQpcUs;
	uint32        ms_lastProfilerUs;
	int           ms_lastVisibleObjects;

	void  reportOverlay();
	void  sanitizeRunLabel(std::string & s);
	void  openCsv();
	void  closeCsv();
	void  writeRow(uint32 gpuUs, bool disjointInvalid, float totalFrameMs);
	std::string currentIsoTimestamp();
}

using namespace DpvsProfileInstrumentationNamespace;

// ======================================================================
// Phase 10 -- DPVS profiling instrumentation (THROWAWAY; D-15)
// Public surface
// ======================================================================

void DpvsProfileInstrumentation::install()
{
	DEBUG_FATAL(ms_installed, ("DpvsProfileInstrumentation::install called twice"));

	ms_installed          = true;
	ms_captureActive      = false;
	ms_runLabel.clear();
	ms_capturedFrames     = 0;
	ms_firstFrameInFile   = 0;
	ms_csv                = NULL;
	ms_lastCpuQpcUs       = 0;
	ms_lastProfilerUs     = 0;
	ms_lastVisibleObjects = 0;

	// Phase 10 -- DPVS profiling instrumentation (THROWAWAY; D-15)
	// Config: CSV output directory (relative to <exe-dir> by default).
	char const * const csvDir = ConfigFile::getKeyString("Dpvs/Experiment", "csvDir", "dpvs-profile/");
	ms_csvDir = (csvDir != NULL) ? csvDir : "dpvs-profile/";

	// Phase 10 -- DPVS profiling instrumentation (THROWAWAY; D-15)
	// Register the DebugFlag-driven overlay print routine. Toggle key is
	// [ClientGraphics/Dpvs] reportInstrumentation; default ON in debug builds.
	ms_reportOverlay = ConfigFile::getKeyBool("ClientGraphics/Dpvs", "reportInstrumentation", true);
	DebugFlags::registerFlag(ms_reportOverlay, "ClientGraphics/Dpvs", "reportInstrumentation", reportOverlay);

	ExitChain::add(&DpvsProfileInstrumentation::remove, "DpvsProfileInstrumentation::Remove");
}

// ----------------------------------------------------------------------

void DpvsProfileInstrumentation::remove()
{
	closeCsv();
	DebugFlags::unregisterFlag(ms_reportOverlay);
	ms_installed = false;
}

// ----------------------------------------------------------------------
// Phase 10 -- DPVS profiling instrumentation (THROWAWAY; D-15)
// Per-frame entrypoint -- called from Game::run() after Graphics::present()
// (plan 10-04). FIRST action gates on ms_installed only (NOT ms_captureActive)
// so the GPU timing pool gets drained every frame per RESEARCH.md Pitfall 4 --
// otherwise the pool stalls. Capture-active gates the CSV row write below.
void DpvsProfileInstrumentation::onFrameEnd(float totalFrameMs)
{
	if (!ms_installed)
		return;

	// Always drain the GPU timing pool, even when capture is off, so it does
	// not stall on the (frame - 2) slot waiting for a Begin/End pair that
	// will never come. Plan 02 GPU primitives consumed here.
	uint32 gpuUs           = 0;
	bool   disjointInvalid = false;
	IGNORE_RETURN(Graphics::dpvsGpuTimingPollResult(&gpuUs, &disjointInvalid));

	if (!ms_captureActive)
		return;

	writeRow(gpuUs, disjointInvalid, totalFrameMs);
}

// ----------------------------------------------------------------------

void DpvsProfileInstrumentation::toggleCapture()
{
	ms_captureActive = !ms_captureActive;
	if (ms_captureActive)
		openCsv();
	else
		closeCsv();
}

// ----------------------------------------------------------------------
// Phase 10 -- DPVS profiling instrumentation (THROWAWAY; D-15)
// Label change rolls the CSV file (per RESEARCH.md): close current, swap
// label, reopen with new label as the filename component (if still capturing).
void DpvsProfileInstrumentation::setRunLabel(std::string const & label)
{
	std::string sanitized = label;
	sanitizeRunLabel(sanitized);

	if (ms_csv != NULL)
		closeCsv();

	ms_runLabel = sanitized;

	if (ms_captureActive)
		openCsv();
}

// ----------------------------------------------------------------------

void DpvsProfileInstrumentation::recordCpuQpcUs(uint32 us)
{
	ms_lastCpuQpcUs = us;
}

// ----------------------------------------------------------------------

void DpvsProfileInstrumentation::recordProfilerUs(uint32 us)
{
	ms_lastProfilerUs = us;
}

// ----------------------------------------------------------------------

void DpvsProfileInstrumentation::recordVisibleObjectCount(int count)
{
	ms_lastVisibleObjects = count;
}

// ----------------------------------------------------------------------

bool DpvsProfileInstrumentation::getCaptureActive()
{
	return ms_captureActive;
}

// ----------------------------------------------------------------------

char const * DpvsProfileInstrumentation::getRunLabel()
{
	return ms_runLabel.c_str();
}

// ----------------------------------------------------------------------

int DpvsProfileInstrumentation::getCapturedFrameCount()
{
	return ms_capturedFrames;
}

// ======================================================================
// Phase 10 -- DPVS profiling instrumentation (THROWAWAY; D-15)
// Anonymous-namespace helpers
// ======================================================================

namespace DpvsProfileInstrumentationNamespace
{

// Phase 10 -- DPVS profiling instrumentation (THROWAWAY; D-15)
// Sanitize run-label per RESEARCH.md §Security Domain:
//   - Replace path separators / \ : * ? " < > | , and newline/CR with _
//   - Replace any byte < 0x20 with _
//   - Replace ".." substring with "__" (path-traversal guard)
//   - If first char is = + - @, prepend _ (CSV/Excel formula-injection guard)
//   - Truncate to 64 chars
//   - If empty after sanitization, assign "unlabeled"
void sanitizeRunLabel(std::string & s)
{
	for (size_t i = 0; i < s.size(); ++i)
	{
		unsigned char const c = static_cast<unsigned char>(s[i]);
		bool replace = false;
		switch (c)
		{
			case '/': case '\\': case ':': case '*': case '?':
			case '"': case '<':  case '>': case '|': case ',':
			case '\r': case '\n': case '\t':
				replace = true;
				break;
			default:
				if (c < 0x20)
					replace = true;
				break;
		}
		if (replace)
			s[i] = '_';
	}

	// Path-traversal: replace any ".." with "__".
	{
		size_t pos = 0;
		while ((pos = s.find("..", pos)) != std::string::npos)
		{
			s[pos]     = '_';
			s[pos + 1] = '_';
			pos       += 2;
		}
	}

	// CSV/Excel formula-injection guard on the FIRST character.
	if (!s.empty())
	{
		char const c0 = s[0];
		if (c0 == '=' || c0 == '+' || c0 == '-' || c0 == '@')
			s.insert(s.begin(), '_');
	}

	// Length cap.
	if (s.size() > 64)
		s.resize(64);

	if (s.empty())
		s = "unlabeled";
}

// ----------------------------------------------------------------------
// Phase 10 -- DPVS profiling instrumentation (THROWAWAY; D-15)
// Open <csvDir><runLabel>-<firstFrame>.csv in append-binary mode and write
// the header row. CSV column schema MUST match
// tools/dpvs-profile/analysis.py EXPECTED_HEADER -- see that file for the
// columns: frame_no, wall_ms_iso, run_label, dpvs_occlusion_flag, gpu_us,
// cpu_qpc_us, profiler_dpvs_us, total_frame_ms, visible_object_count,
// draw_call_count.
void openCsv()
{
	if (ms_csv != NULL)
		return;

	std::string label = ms_runLabel;
	if (label.empty())
		label = "unlabeled";

	int const frameStart = Graphics::getFrameNumber();
	ms_firstFrameInFile  = frameStart;

	char path[512];
	int const pathLen = snprintf(path, sizeof(path), "%s%s-%d.csv",
		ms_csvDir.c_str(), label.c_str(), frameStart);
	if (pathLen <= 0 || pathLen >= static_cast<int>(sizeof(path)))
	{
		DEBUG_WARNING(true, ("DpvsProfileInstrumentation: csv path overflow (dir=%s label=%s)",
			ms_csvDir.c_str(), label.c_str()));
		return;
	}

	AbstractFile * fl = new StdioFile(path, "ab");
	if (!fl->isOpen())
	{
		DEBUG_WARNING(true, ("DpvsProfileInstrumentation: failed to open CSV at %s", path));
		delete fl;
		ms_csv = NULL;
		return;
	}

	// Header row matches tools/dpvs-profile/analysis.py EXPECTED_HEADER.
	char const * const header =
		"frame_no,wall_ms_iso,run_label,dpvs_occlusion_flag,gpu_us,cpu_qpc_us,"
		"profiler_dpvs_us,total_frame_ms,visible_object_count,draw_call_count\n";
	IGNORE_RETURN(fl->write(static_cast<int>(strlen(header)), header));

	ms_csv            = fl;
	ms_capturedFrames = 0;
}

// ----------------------------------------------------------------------
// Phase 10 -- DPVS profiling instrumentation (THROWAWAY; D-15)
void closeCsv()
{
	if (ms_csv != NULL)
	{
		delete ms_csv;
		ms_csv = NULL;
	}
}

// ----------------------------------------------------------------------
// Phase 10 -- DPVS profiling instrumentation (THROWAWAY; D-15)
// Append one CSV row. When disjointInvalid is true, gpu_us is written as 0
// (analysis.py treats blank / non-numeric as missing; 0 is acceptable here
// because Pitfall 2 already gated -- callers see 0 only when disjoint).
// dpvs_occlusion_flag column: 0 = occlusion ON (normal), 1 = OFF (F11 toggle).
void writeRow(uint32 gpuUs, bool disjointInvalid, float totalFrameMs)
{
	if (ms_csv == NULL)
		return;

	// Phase 10 D-14: getter deleted; DPVS occlusion is permanently off per Option alpha.
	// Wave 7 deletes this file entirely.
	int const dpvsOff = 1;

	int drawCalls = 0;
#ifdef _DEBUG
	{
		int verts = 0, points = 0, lines = 0, tris = 0, calls = 0;
		Graphics::getRenderedVerticesPointsLinesTrianglesCalls(verts, points, lines, tris, calls);
		drawCalls = calls;
	}
#endif

	std::string const iso       = currentIsoTimestamp();
	int const         frameNo   = Graphics::getFrameNumber();
	uint32 const      gpuUsCell = disjointInvalid ? 0u : gpuUs;
	char const *      label     = ms_runLabel.empty() ? "unlabeled" : ms_runLabel.c_str();

	char buf[512];
	int const len = snprintf(buf, sizeof(buf),
		"%d,%s,%s,%d,%u,%u,%u,%.3f,%d,%d\n",
		frameNo,
		iso.c_str(),
		label,
		dpvsOff,
		gpuUsCell,
		ms_lastCpuQpcUs,
		ms_lastProfilerUs,
		totalFrameMs,
		ms_lastVisibleObjects,
		drawCalls);
	if (len <= 0 || len >= static_cast<int>(sizeof(buf)))
		return;

	IGNORE_RETURN(ms_csv->write(len, buf));
	++ms_capturedFrames;
}

// ----------------------------------------------------------------------
// Phase 10 -- DPVS profiling instrumentation (THROWAWAY; D-15)
// Local-time ISO-8601 timestamp (seconds resolution). Pattern matches
// CalendarTime.cpp's ::localtime + ::strftime usage in sharedFoundation.
std::string currentIsoTimestamp()
{
	time_t const epoch = ::time(NULL);
	struct tm * timeinfo = ::localtime(&epoch);
	char buf[32];
	if (timeinfo == NULL)
	{
		buf[0] = '\0';
		return std::string(buf);
	}
	size_t const n = ::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", timeinfo);
	if (n == 0)
		buf[0] = '\0';
	return std::string(buf);
}

// ----------------------------------------------------------------------
// Phase 10 -- DPVS profiling instrumentation (THROWAWAY; D-15)
// Per-frame DEBUG_REPORT_PRINT line driven by the DebugFlag toggle. Same
// surface as RenderWorld::reportMetrics (DebugMonitor child window in
// PRODUCTION==0 builds). Adapted from RESEARCH.md §Code Examples
// (Overlay print routine, lines 521-541).
void reportOverlay()
{
	// Phase 10 D-14: DPVS toggle removed; overlay shows "removed". Wave 7 deletes this file.
	DEBUG_REPORT_PRINT(true, ("DPVS:%s run=%s %s frame=%d capturedRows=%d\n",
		"removed",
		ms_runLabel.empty() ? "(unlabeled)" : ms_runLabel.c_str(),
		ms_captureActive ? "REC" : "...",
		Graphics::getFrameNumber(),
		ms_capturedFrames));
}

} // namespace DpvsProfileInstrumentationNamespace

// ======================================================================
