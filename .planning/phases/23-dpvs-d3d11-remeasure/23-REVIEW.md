---
phase: 23-dpvs-d3d11-remeasure
reviewed: 2026-06-12T18:30:00Z
depth: standard
files_reviewed: 10
files_reviewed_list:
  - src/engine/client/library/clientGraphics/include/public/clientGraphics/DpvsProfileInstrumentation.h
  - src/engine/client/library/clientGraphics/src/shared/DpvsProfileInstrumentation.cpp
  - src/engine/client/library/clientGraphics/include/public/clientGraphics/RenderWorld.h
  - src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp
  - src/engine/client/library/clientGraphics/src/win32/SetupClientGraphics.cpp
  - src/engine/client/library/clientGraphics/build/win32/clientGraphics.vcxproj
  - src/engine/client/library/clientGame/src/shared/core/Game.cpp
  - src/engine/client/library/clientUserInterface/src/shared/core/CuiIoWin.cpp
  - src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserDefault.cpp
  - docs/recon/23-dpvs-d3d11-profiling.md
findings:
  critical: 0
  warning: 4
  info: 8
  total: 12
status: issues_found
---

# Phase 23: Code Review Report

**Reviewed:** 2026-06-12T18:30:00Z
**Depth:** standard
**Files Reviewed:** 10
**Status:** issues_found

## Summary

Reviewed the Phase 23 diff (`363750b46^..HEAD`) restoring the Phase-10 DPVS profiling
instrumentation for the D3D11 remeasure: the `DpvsProfileInstrumentation` CSV
writer/overlay module, RenderWorld occlusion A/B re-gate + QPC bracket, install hook,
`Game::run` per-frame driver, F10/F11 debug keybinds, `/setrunlabel` command, vcxproj
entries, and the recon verdict doc.

The core is sound. Verified against the surrounding engine code:

- The run-label sanitizer correctly neutralizes path traversal (`/ \ : ..` all
  replaced before filename use), CSV cell injection (`,` `"` CR/LF stripped), and
  Excel formula injection (leading `= + - @` prefixed) in the right order
  (formula-guard insert happens before the 64-char cap, so the guard survives
  truncation). No bypass found, including via `Unicode::wideToNarrow` high bytes.
- The `_DEBUG` cullingParameters re-gate is bit-correct and the shipped Option-α
  `#else` branch is untouched, as claimed.
- `Graphics::getRenderedVerticesPointsLinesTrianglesCalls` (a `_DEBUG`-only API,
  Graphics.h:92-99) and `RenderWorldCommander::getNumberOfVisibleObjects` are
  correctly `#ifdef _DEBUG`-fenced at their call sites in `writeRow` and
  `RenderWorld::drawScene`.
- `PerformanceTimer` is installed by `SetupSharedDebug` long before first use;
  no divide-by-zero risk. `DebugFlags::config` uses the current variable value as the
  config default, so the pre-read + `registerFlag` sequence in `install()` is
  redundant but correct. ExitChain teardown ordering (module removed before
  DebugFlags) is safe.
- The CSV header string matches `tools/dpvs-profile/analysis.py` `EXPECTED_HEADER`
  exactly (10 columns, same order).
- Recon doc arithmetic checks out (outdoor 34.88−34.12 = 0.76 median delta, indoor
  37.79 ≤ 38.49 AND 44.87 ≤ 45.84 ⇒ `remove`), and the "no RenderWorld.cpp in this
  plan's diff" claim is accurate for plan 23-03 specifically (RenderWorld.cpp was
  touched only by 23-01 commits).

No Critical findings. The Warnings cluster around CSV-writer failure modes (silent
no-op capture) and a data-integrity gap in the per-frame row cache; the Info items
are dead state, doc-accuracy, and debug-only edge cases.

## Warnings

### WR-01: openCsv never creates the output directory — capture silently produces nothing on a fresh checkout

**File:** `src/engine/client/library/clientGraphics/src/shared/DpvsProfileInstrumentation.cpp:296`
**Issue:** `openCsv()` does `new StdioFile(path, "ab")`, which is a bare `fopen` —
it does not create intermediate directories. The default `csvDir` is `dpvs-profile/`
relative to the working directory (`stage/`), and `stage/*` is gitignored
(`.gitignore:92`), so nothing in the repo or build creates the directory. On any
fresh checkout / new machine, the first F10 press fails the open: a `DEBUG_WARNING`
in Debug (easy to miss in the warning stream), completely silent in Release.
`tools/dpvs-profile/test-protocol.md:106` papers over this with a manual checklist
item, but the code-level failure is silent data loss for a measurement session.
**Fix:** Create the directory before opening, e.g. in `install()` or at the top of
`openCsv()`:
```cpp
#include <direct.h>
...
IGNORE_RETURN(_mkdir(ms_csvDir.c_str()));   // ok if it already exists (EEXIST)
```
(or use the engine's `Os` directory-creation helper if one exists for multi-level
paths).

### WR-02: toggleCapture leaves ms_captureActive=true when openCsv fails — overlay shows "REC" while nothing is written

**File:** `src/engine/client/library/clientGraphics/src/shared/DpvsProfileInstrumentation.cpp:132-139`
**Issue:** `toggleCapture()` sets `ms_captureActive = true` and then calls
`openCsv()`. Both `openCsv` failure paths (path snprintf overflow at :289, fopen
failure at :297) return with `ms_csv == NULL` but leave `ms_captureActive` true.
From that point `writeRow()` no-ops every frame (`ms_csv == NULL` guard at :335)
while `getCaptureActive()` returns true, so the on-screen overlay (`reportOverlay`,
:413) shows `REC` — the operator believes a pass is being captured and discovers
the loss only after the session. The same desync exists in the `setRunLabel()`
reopen path (:155-156). This is the failure mode the recon doc's "build-staleness
caveat" nearly hit.
**Fix:** Make open failure observable and consistent:
```cpp
void DpvsProfileInstrumentation::toggleCapture()
{
	ms_captureActive = !ms_captureActive;
	if (ms_captureActive)
	{
		openCsv();
		if (ms_csv == NULL)        // open failed -- don't pretend to record
			ms_captureActive = false;
	}
	else
		closeCsv();
}
```
(Alternatively have `openCsv()` return bool and check it at both call sites.)

### WR-03: csvDir concatenation assumes a trailing separator — misconfigured value silently writes files to the wrong place

**File:** `src/engine/client/library/clientGraphics/src/shared/DpvsProfileInstrumentation.cpp:85-87, 287-288`
**Issue:** The path is built as `snprintf(path, ..., "%s%s-%d.csv", ms_csvDir.c_str(),
label.c_str(), frameStart)` with no separator. The default `"dpvs-profile/"` carries
its own slash, but a user-supplied `[Dpvs/Experiment] csvDir=dpvs-profile` (no
trailing slash — the natural way to write a config value) silently produces files
named `dpvs-profilemosEisley-pass1-on-12345.csv` in the working directory instead of
the intended folder. No warning fires; the capture "succeeds" into the wrong path.
**Fix:** Normalize once in `install()`:
```cpp
ms_csvDir = (csvDir != NULL) ? csvDir : "dpvs-profile/";
if (!ms_csvDir.empty() && ms_csvDir.back() != '/' && ms_csvDir.back() != '\\')
	ms_csvDir += '/';
```

### WR-04: Stale cached metrics are written as fresh rows for frames where resolveVisibility never ran

**File:** `src/engine/client/library/clientGraphics/src/shared/DpvsProfileInstrumentation.cpp:161-178, 333-376`; `src/engine/client/library/clientGame/src/shared/core/Game.cpp:1236`
**Issue:** `Game::runGameLoopOnce` calls `onFrameEnd()` unconditionally every loop
iteration — including frames where `RenderWorld::drawScene` never executed (loading
screens, `isOver()` shutdown frame, the `_DEBUG` `ms_disableResolveVisibility` flag
at RenderWorld.cpp:1044-1046, scenes that don't drive RenderWorld). On those frames
`writeRow()` emits the *previous* cull's `cpu_qpc_us` / `visible_object_count`
verbatim, paired with a fresh `frame_no` / `total_frame_ms` — indistinguishable from
real samples in the CSV. The documented 1-frame skew (Game.cpp:1230-1235) covers
`total_frame_ms` only, not this repetition. If capture is ever left running across a
zone load or scene transition, the A/B medians are polluted with duplicated stale
cull timings. (Related: if multiple `drawScene` calls occur in one frame, only the
last call's bracket survives — last-writer-wins on the cache.)
**Fix:** Track freshness — e.g. add a `bool ms_cullSampleFresh` set by
`recordCpuQpcUs()` and cleared by `onFrameEnd()` after the row is written; when the
sample is not fresh, write 0 (or skip the row) so dead frames are filterable:
```cpp
// in writeRow():
uint32 const cpuQpc = ms_cullSampleFresh ? ms_lastCpuQpcUs : 0u;
...
// at end of onFrameEnd():
ms_cullSampleFresh = false;
```

## Info

### IN-01: Dead state and vestigial parameters in the CSV writer

**File:** `src/engine/client/library/clientGraphics/src/shared/DpvsProfileInstrumentation.cpp:49, 78, 121-122, 284, 333`
**Issue:** `ms_firstFrameInFile` is write-only (assigned at :78 and :284, never
read — the frame number is baked into the filename directly from the local
`frameStart`). `onFrameEnd`'s `gpuUs`/`disjointInvalid` locals and `writeRow`'s
matching parameters are hardwired constants (`0`, `false`) after the GPU-strip;
`gpuUsCell = disjointInvalid ? 0u : gpuUs` at :355 is dead logic that always yields 0.
**Fix:** Delete `ms_firstFrameInFile`; either drop the two `writeRow` parameters and
write a literal `0` for the `gpu_us` column, or keep them but remove the dead
ternary. (Low priority for THROWAWAY code, but it shrinks the D-15 cleanup diff.)

### IN-02: /setrunlabel is exposed in ALL builds (including Release) and gives the user no feedback

**File:** `src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserDefault.cpp:134, 202-203, 1778-1795`
**Issue:** Unlike the F10/F11 hooks, the command registration and handler are not
`#ifdef _DEBUG`-gated, so a Release client ships a player-visible `/setrunlabel`
command whose help text reads "THROWAWAY; D-15." It is functionally harmless in
Release (capture can never activate — `toggleCapture` is only reachable from the
`_DEBUG` F10 hook), but it widens the D-15 cleanup surface and leaks internal
phase jargon to users. Also, the handler returns `true` without appending anything
to `result`, so even in Debug the operator gets no confirmation that the label took
(other branches in this parser echo their effect).
**Fix:** Wrap the `MAKE_COMMAND`, table entry, and handler in `#ifdef _DEBUG`, and
append a confirmation, e.g.
`result += Unicode::narrowToWide(std::string("run label set: ") + DpvsProfileInstrumentation::getRunLabel());`

### IN-03: dpvs_occlusion_flag would misreport in a non-_DEBUG capture

**File:** `src/engine/client/library/clientGraphics/src/shared/DpvsProfileInstrumentation.cpp:338-342`
**Issue:** In non-`_DEBUG` builds `dpvsOff` is hardwired to `0` ("occlusion ON"),
but the Option-α `#else` branch in `RenderWorld::drawScene` (:914-916) permanently
omits the `OCCLUSION_CULLING` bit — occlusion is actually OFF in those builds. The
column would state the opposite of reality. Unreachable today (capture can only be
toggled via the `_DEBUG` F10 hook), but it is a loaded trap if anyone adds a
Release-build capture path for the deferred Release re-measure.
**Fix:** In the `#else` branch write `1` (occlusion structurally off per Option α),
or add a comment at the `#else` stating the value is intentionally wrong-but-unreachable.

### IN-04: toggleCapture/setRunLabel lack the ms_installed guard that onFrameEnd has

**File:** `src/engine/client/library/clientGraphics/src/shared/DpvsProfileInstrumentation.cpp:132, 145`
**Issue:** `onFrameEnd()` early-outs on `!ms_installed` (:118), but `toggleCapture()`
and `setRunLabel()` do not. If either were invoked before `install()` (or after
ExitChain teardown), `openCsv()` would run with an empty `ms_csvDir` (file lands in
cwd) and call `Graphics::getFrameNumber()` on a possibly-uninstalled Graphics layer.
Practically unreachable through current callers (UI exists only after
SetupClientGraphics), but the guard is one line.
**Fix:** Add `if (!ms_installed) return;` at the top of both functions.

### IN-05: CSV append mode + frame-number filenames can produce a mid-file duplicate header

**File:** `src/engine/client/library/clientGraphics/src/shared/DpvsProfileInstrumentation.cpp:296, 306-309`
**Issue:** Files are opened `"ab"` and named `<label>-<frameNumber>.csv`. Frame
numbers reset every client session, so re-running the same pass label and toggling
capture at the same frame number appends a second header row mid-file.
`analysis.py` row parsing (`len(raw) == 10` passes the header row through at
analysis.py:101-104) would then feed the string `"total_frame_ms"` into
`float()` during aggregation — crashing or aborting the analysis with no pointer to
the offending file. Low probability, confusing when it hits.
**Fix:** Open with `"wb"` (each open is a new logical run; collision then truncates
rather than corrupts), or include the ISO timestamp in the filename.

### IN-06: F10/F11 intercept skips the s_discardNextKeyDown reset

**File:** `src/engine/client/library/clientUserInterface/src/shared/core/CuiIoWin.cpp:970-981, 1014`
**Issue:** The early `break` for DIK_F10/DIK_F11 exits the `IOET_KeyDown` case
before `s_discardNextKeyDown = false;` (:1014). If the discard flag was armed by a
prior UI flow, a F10/F11 press leaves it armed and the *next* legitimate keystroke
gets swallowed instead. Debug-only and minor, but it is a behavioral difference
versus every other key.
**Fix:** Clear the flag inside the intercept (`s_discardNextKeyDown = false;`)
before `break`, or place the intercept after the reset.

### IN-07: Overlay shows the previous file's row count after capture stops

**File:** `src/engine/client/library/clientGraphics/src/shared/DpvsProfileInstrumentation.cpp:312, 317-324, 410-415`
**Issue:** `ms_capturedFrames` is reset only in `openCsv()`; `closeCsv()` leaves it.
After F10-off the overlay prints `... capturedRows=N` from the closed file while
showing `...` (idle), which reads as "N rows pending." Cosmetic, but the overlay is
the operator's only live feedback during a measurement pass.
**Fix:** `ms_capturedFrames = 0;` in `closeCsv()` (or display `capturedRows` only
while `ms_captureActive`).

### IN-08: Recon doc wording implies profiler_dpvs_us could populate; it is structurally hardwired to 0

**File:** `docs/recon/23-dpvs-d3d11-profiling.md:48`; `src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp:1074`
**Issue:** The doc (under a heading claiming "verified in code, not data problems")
says "the profiler counter was not fed **in this session**" — but
`RenderWorld.cpp:1074` passes a constant `recordProfilerUs(0u)`, so the column is 0
in *every* session by construction, exactly like the GPU-strip the doc explains in
detail one paragraph later. A future re-measure could waste time expecting it to
populate. Minor related inconsistency: the module reads config from two different
sections (`[Dpvs/Experiment] csvDir` at DpvsProfileInstrumentation.cpp:85 vs
`[ClientGraphics/Dpvs]` for the flags), which the protocol doc must keep in sync.
**Fix:** Reword line 48 to "structurally 0 — the QPC bracket replaced the Profiler
read; `recordProfilerUs(0u)` is hardwired (RenderWorld.cpp:1074)", and/or add an
inline comment at the `recordProfilerUs(0u)` call mirroring the gpu_us GPU-strip
comment.

---

_Reviewed: 2026-06-12T18:30:00Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
