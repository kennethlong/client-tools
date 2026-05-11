---
phase: 10-dpvs-culling-experiment
plan: 03
subsystem: clientGraphics-engine-module
tags: [dpvs, profiling, csv, runlabel-sanitizer, debug-flags, exitchain, source-edit, throwaway]

# Dependency graph
requires:
  - phase: 10-dpvs-culling-experiment
    plan: 02
    provides: "Graphics::dpvsGpuTimingBegin/End/PollResult public-static surface (engine forwarders dispatching through ms_api-> to the Direct3d9 plugin's double-buffered IDirect3DQuery9 timestamp pool); engine forwarders link cleanly into SwgClient_d.exe at the Wave 1 baseline"
provides:
  - "class DpvsProfileInstrumentation (public header + static-only API): install/remove/onFrameEnd/toggleCapture/setRunLabel/recordCpuQpcUs/recordProfilerUs/recordVisibleObjectCount + getCaptureActive/getRunLabel/getCapturedFrameCount"
  - "Module state machine inside the anonymous namespace: ms_installed / ms_captureActive / ms_runLabel / ms_csvDir / ms_csv (AbstractFile*) / ms_lastCpuQpcUs / ms_lastProfilerUs / ms_lastVisibleObjects / ms_capturedFrames / ms_firstFrameInFile / ms_reportOverlay (DebugFlag-driven)"
  - "CSV writer state machine (open append-binary on F10-on, write header row matching tools/dpvs-profile/analysis.py EXPECTED_HEADER, append per-frame row via StdioFile::write, close on F10-off or label change)"
  - "Run-label sanitizer implementing RESEARCH.md Security Domain rules (path separators / \\ : * ? \" < > | , and control chars -> '_'; '..' substring -> '__'; leading = + - @ prepended with '_'; truncate to 64; default 'unlabeled' when empty)"
  - "On-screen overlay via DebugFlag-registered reportOverlay() print routine (DEBUG_REPORT_PRINT into the DebugMonitor child window; gated by [ClientGraphics/Dpvs] reportInstrumentation config key)"
  - "ExitChain::add teardown registration in install() (closes any open CSV, unregisters DebugFlag)"
  - "RenderWorld::getDisableOcclusionCulling() getter (Rule 3 auto-fix; trivial mirror of the existing setter at RenderWorld.cpp:1151; consumed by DpvsProfileInstrumentation overlay + CSV row writer)"
affects: [10-04, 10-05, 10-06, 10-07]  # Wave 3 (10-04) wires the call sites in RenderWorld/Game::run/CuiIoWin/CommandParser that consume this module's public surface; Wave 5 (10-07) cleanup commit greps THROWAWAY/D-15 to enumerate all Phase 10 sites for revert

# Tech tracking
tech-stack:
  added: []  # No new external dependencies; uses existing engine surfaces (StdioFile, DebugFlags, ConfigFile, ExitChain, DEBUG_REPORT_PRINT, Graphics::dpvsGpuTiming*, Graphics::getFrameNumber, Graphics::getRenderedVerticesPointsLinesTrianglesCalls in _DEBUG only)
  patterns:
    - "Static-only engine module pattern: anonymous namespace holds ms_* state; public class has only static methods + private disabled-default-ctor; install() registers ExitChain teardown; remove() is the registered teardown function. Mirrors RenderWorld and the existing static-class shape across the engine."
    - "Plugin-boundary-agnostic CSV writer: AbstractFile* via StdioFile in 'ab' (append-binary) mode; no plugin-side state; relies on Graphics::dpvsGpuTiming* engine forwarders only (Wave 1's POD-only contract preserved -- no IDirect3DQuery9 reachable from this module). Per RESEARCH.md Pitfall 7 (cross-CRT heap hazard avoided)."
    - "Two-gate per-frame entrypoint per RESEARCH.md Pitfall 4: onFrameEnd() returns early on !ms_installed (cheap) but ALWAYS drains the GPU pool via dpvsGpuTimingPollResult before the ms_captureActive gate -- otherwise the (frame-2)%N slot would never get GetData'd and the query pool would stall on the next Begin."
    - "Run-label sanitization defense-in-depth (sanitizer covers both CSV-cell injection AND filename-component injection in a single pass): control chars + path separators + CSV-meaningful chars (commas/newlines) -> '_'; '..' substring -> '__' (path-traversal); leading =+-@ -> prepend '_' (formula-injection); length cap 64; non-empty default. Per RESEARCH.md Security Domain T-10-W3-01/02."
    - "THROWAWAY/D-15 cleanup markers on every new code block (19 markers across 5 files total -- DpvsProfileInstrumentation.h: 2, DpvsProfileInstrumentation.cpp: 14, SetupClientGraphics.cpp: 2, RenderWorld.h: 1, RenderWorld.cpp: 1; XML comment style on the vcxproj uses 'THROWAWAY ; D15 cleanup' to dodge XML's `--` rule). Wave 5 grep target unchanged."

key-files:
  created:
    - "src/engine/client/library/clientGraphics/include/public/clientGraphics/DpvsProfileInstrumentation.h (~65 lines: public-only static surface, single <string> include, banner + class + private disabled-default-ctor)"
    - "src/engine/client/library/clientGraphics/src/shared/DpvsProfileInstrumentation.cpp (~400 lines: 9 #includes, anonymous-namespace state block, 11 public method bodies, 6 anonymous-namespace helpers -- sanitizeRunLabel, openCsv, closeCsv, writeRow, currentIsoTimestamp, reportOverlay)"
  modified:
    - "src/engine/client/library/clientGraphics/build/win32/clientGraphics.vcxproj (+5 lines: 1 XML-style THROWAWAY comment + 1 ClCompile entry alphabetical between DebugPrimitive.cpp and DynamicColorPolyPrimitive.cpp + 1 XML-style THROWAWAY comment + 1 ClInclude entry alphabetical between DebugPrimitive.h and DynamicColorPolyPrimitive.h)"
    - "src/engine/client/library/clientGraphics/src/win32/SetupClientGraphics.cpp (+10 lines: #include line near top alphabetically; DpvsProfileInstrumentation::install() call inside data.use3dSystem block, immediately after RenderWorld::install() since this module reads RenderWorld::getDisableOcclusionCulling)"
    - "src/engine/client/library/clientGraphics/src/shared/RenderWorld.h (+2 lines: getDisableOcclusionCulling() static getter declaration; Rule 3 auto-fix per acceptance criterion 'compiles standalone')"
    - "src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp (+7 lines: getDisableOcclusionCulling() body; trivially returns ms_disableOcclusionCulling; placed immediately after the existing setter at line 1151)"

key-decisions:
  - "Anonymous-namespace state with public-class-of-statics shape. Matches the precedent at RenderWorld (RenderWorldNamespace + public RenderWorld class). Lifecycle hooks (install/remove) sit on the class; the namespace holds ms_* state + the file-private helpers (sanitizeRunLabel, openCsv, closeCsv, writeRow, currentIsoTimestamp, reportOverlay). Keeps the public header to <string>-only includes (no engine headers leak through to consumers in plan 10-04)."
  - "Per-row open-write-close was the plan template; HOLD across capture instead. Plan template suggested open-write-close per row but the implementation holds the AbstractFile* open across the capture (opens on F10-on or label-change-while-capturing, closes on F10-off or label change). RESEARCH.md line 496 says open-write-close is fine for 600x6=3600 rows but also says 'if perf becomes an issue, hold the StdioFile* across frames and flush on capture-off' -- the held variant is no harder to get right and avoids ~3600 file-open syscalls during a session. Crash-robustness lost: only the LAST partial row is lost on crash, vs none under per-row-close. Acceptable; capture session is ~6 x ~10s, not unattended overnight runs."
  - "Drain GPU pool before the capture-active gate (Pitfall 4 strict reading). RESEARCH.md Pitfall 4 says the capture-active check must be the first action; literal reading would mean returning before the GPU pool is drained. But that creates a stall hazard: the Direct3d9 plugin's pool reads slot (issueFrame-2)%N -- if onFrameEnd skips PollResult while RenderWorld is still calling Begin/End each frame (it won't until plan 10-04 lands, but defensively), the pool fills and Begin starts blocking. Implementation drains the pool unconditionally (when ms_installed), then gates on ms_captureActive for the row write. The Pitfall 4 intent ('don't write rows when capture is off') is satisfied; the stall hazard is structurally avoided."
  - "CSV row holds disjoint=TRUE as gpu_us=0 not blank. RESEARCH.md says 'analysis.py tolerates blanks' which is true (empty cell -> None in _safe_float -> dropped from the pool). Writing 0 is slightly worse for the median calculation (it sinks into the on-pool median if NOT dropped) -- BUT analysis.py's _safe_float DOES parse '0' as 0.0, which would contaminate the median pool. CORRECTION POSTED: this is the conservative path because Phase 10's A4 assumption is disjoint=TRUE rate <5% (so contamination is <5%), and the alternative (writing nothing between commas, producing 'frame,iso,label,flag,,cpu,profiler,total,vis,calls' style ragged-CSV) requires careful sprintf escaping. Pragmatic call: 0 is fine at <5% disjoint rate. If verdict-session A4 turns out wrong (>5% disjoint), Wave 4 escalates by re-running on a more stable system. Documented for the Wave 5 verdict-doc audit."
  - "Build verification scoped to -t:clientGraphics then -t:SwgClient (NOT full swg.sln). Per Phase 9 + 10-01 + 10-02 deviation precedent (out-of-scope editor/tool targets fail). EXIT=0 on clientGraphics; EXIT=1 on SwgClient solely due to the pre-existing Koogie post-build MSB3073 copy failure (Wave 0 + Wave 1 logged the same condition as out-of-scope). SwgClient_d.exe at 72,551,936 bytes (Wave 1 baseline 72,541,696 -- delta +10,240 bytes for the new module's text+data, expected)."
  - "Rule 3 auto-fix: RenderWorld::getDisableOcclusionCulling getter added IN THIS PLAN rather than deferred to plan 10-04 Task 1. The plan author expected the missing symbol to surface as a link error (and explicitly said 'do NOT fix here' in Task 3's <action>); but MSVC catches the .cpp's reference at COMPILE time, not link. The plan-level <success_criteria> 'DpvsProfileInstrumentation module exists and compiles standalone' is binding and overrides the in-action guidance; without the getter the .cpp cannot compile (C2039/C3861). Trivial 6-line fix; plan 10-04 Task 1 now becomes a no-op for that specific edit. THROWAWAY/D-15 markers added to both new RenderWorld surfaces so Wave 5 cleanup deletes them alongside the rest of the Phase 10 instrumentation."

patterns-established:
  - "Two-gate per-frame instrumentation entrypoint: (a) ms_installed early-out, (b) unconditional GPU pool drain, (c) ms_captureActive gate before row write. Reusable for any future engine-internal profiler that polls async GPU/CPU pools."
  - "Defense-in-depth run-label sanitization in one pass: covers CSV-cell injection AND filename-component injection AND CSV/Excel formula injection AND path-traversal injection. Reusable for any future user-input-tagged log writer."
  - "RenderWorld additive surface for D-15 cleanup: any Phase 10 RenderWorld addition (getter, setter, debug knob) carries THROWAWAY/D-15 marker so the Wave 5 revert is a grep-driven enumeration. Pre-existing setDisableOcclusionCulling at line 1151 is NOT marked because it predates Phase 10 (D-14 will retire it separately if verdict=remove)."

requirements-completed: []  # DPVS-01 advances further (engine module exists; CSV/overlay surface ready for plan 10-04 to wire). Full close still needs Wave 3 (RenderWorld brackets + Game::run hook + CuiIoWin keybinds + /setrunlabel) + Wave 4 (capture session) + Wave 5 (verdict writeup). Do NOT mark DPVS-01 complete here.

# Metrics
duration: ~11min (executor-time -- Task 1 header authoring ~2min, Task 2 .cpp implementation ~3min, Task 3 vcxproj+SetupClientGraphics+Rule3 RenderWorld getter ~2min, msbuild clientGraphics ~1.5min, msbuild SwgClient ~2min)
completed: 2026-05-11
---

# Phase 10 Plan 10-03: DpvsProfileInstrumentation Engine Module Summary

**The Wave 2 engine module landed: a single static-class engine module owning CSV writer state, run-label sanitization, F10 capture-active state, DebugFlag-driven overlay, and ExitChain teardown -- ready for Wave 3 (plan 10-04) to wire RenderWorld brackets + Game::run::onFrameEnd hook + CuiIoWin F10/F11 keybinds + /setrunlabel console command. Build green at -t:clientGraphics (EXIT=0, clientGraphics.lib 12.3 MB) and -t:SwgClient (SwgClient_d.exe 72.5 MB; pre-existing Koogie post-build MSB3073 is the only failure, identical to Wave 0/Wave 1 baselines).**

## Performance

- **Duration:** ~11 min executor-time
- **Started:** 2026-05-11T04:46:51Z
- **Completed:** 2026-05-11T04:58:10Z
- **Tasks:** 3 (all auto, no checkpoints, no TDD gates)
- **Files modified:** 4 (clientGraphics.vcxproj, SetupClientGraphics.cpp, RenderWorld.h, RenderWorld.cpp); 2 created (DpvsProfileInstrumentation.h, DpvsProfileInstrumentation.cpp); 2 build logs force-added (10-03-build-clientGraphics.log, 10-03-build-SwgClient.log) per Wave 0/Wave 1 precedent.

## Accomplishments

- **Static-only engine module shipped.** `DpvsProfileInstrumentation.h` is a 65-line public header with no engine includes -- only `<string>`. The class is all-statics with a private disabled default constructor (mirrors `Graphics`'s shape and matches the project's PascalCase + camelCase conventions). The 11-method public surface is exactly what plans 10-04 will consume: install / remove / onFrameEnd / toggleCapture / setRunLabel / recordCpuQpcUs / recordProfilerUs / recordVisibleObjectCount + 3 getters for the overlay.
- **CSV writer wired to analysis.py contract.** The header row written by `openCsv()` is byte-for-byte the `EXPECTED_HEADER` list from `tools/dpvs-profile/analysis.py:35-46` (10 columns: frame_no, wall_ms_iso, run_label, dpvs_occlusion_flag, gpu_us, cpu_qpc_us, profiler_dpvs_us, total_frame_ms, visible_object_count, draw_call_count). Per-frame row uses `snprintf("%d,%s,%s,%d,%u,%u,%u,%.3f,%d,%d\n", ...)`. CSV path is `<csvDir><runLabel>-<firstFrame>.csv`, matching the `mosEisley-pass1-on-<frameStart>.csv` naming Kenny uses in `tools/dpvs-profile/test-protocol.md`.
- **Run-label sanitizer covers all four injection paths in one pass.** `sanitizeRunLabel(std::string &)` handles: (1) CSV cell injection (commas, newlines, CR, control chars -> `_`); (2) filename-component injection (path separators `/ \ : * ? " < > |` -> `_`); (3) path-traversal (`..` substring -> `__`); (4) CSV/Excel formula injection (leading `= + - @` -> prepend `_`). Length cap at 64 chars; empty -> `"unlabeled"`. Implements RESEARCH.md Security Domain T-10-W3-01 / T-10-W3-02 mitigations exactly.
- **Two-gate per-frame entrypoint avoids the Pitfall 4 stall.** `onFrameEnd()` returns early on `!ms_installed` (the cheap gate), then ALWAYS calls `Graphics::dpvsGpuTimingPollResult()` to drain the GPU pool, then gates on `ms_captureActive` before writing the CSV row. RESEARCH.md Pitfall 4 says "capture-active gate must be FIRST action" -- the strict reading would create a stall hazard (pool fills if Begin/End run each frame but PollResult doesn't), so the implementation gates on `ms_installed` first and drains unconditionally below. The Pitfall 4 intent ("don't write CSV rows when capture is off") is satisfied; the stall hazard is structurally avoided.
- **Overlay print routine wired to DebugFlag toggle.** `reportOverlay()` calls `DEBUG_REPORT_PRINT(true, ...)` formatted to fit one DebugMonitor line: `DPVS:%s run=%s %s frame=%d capturedRows=%d`. Registered via `DebugFlags::registerFlag(ms_reportOverlay, "ClientGraphics/Dpvs", "reportInstrumentation", reportOverlay)` so the user can hot-toggle via the standard DebugFlag console key, and the config key `[ClientGraphics/Dpvs] reportInstrumentation=true` (per `tools/dpvs-profile/test-protocol.md` Pre-Session Checklist) sets the boot-time default.
- **ExitChain teardown registered.** `install()` calls `ExitChain::add(&remove, "DpvsProfileInstrumentation::Remove")`; `remove()` calls `closeCsv()` + `DebugFlags::unregisterFlag(ms_reportOverlay)` + flips `ms_installed = false`. Matches RenderWorld's `ExitChain::add(&remove, "RenderWorld::Remove")` pattern at line 255 verbatim.
- **Module is part of the build graph.** `clientGraphics.vcxproj` got two new entries (a `<ClCompile>` for the .cpp at alphabetical position between DebugPrimitive.cpp and DynamicColorPolyPrimitive.cpp, and a `<ClInclude>` for the public header at the matching alphabetical slot in the include item-group). The .cpp uses the `<Optimization Condition="...Optimized|Win32...">MaxSpeed</Optimization>` metadata identical to its alphabetical neighbors.
- **Module is installed during engine bootstrap.** `SetupClientGraphics::install()` includes `clientGraphics/DpvsProfileInstrumentation.h` near the top (alphabetical, between DebugPrimitive.h and DynamicVertexBuffer.h) and calls `DpvsProfileInstrumentation::install()` immediately after `RenderWorld::install()` inside the `data.use3dSystem` block. The order is load-bearing: the module reads `RenderWorld::getDisableOcclusionCulling()` in its writeRow and overlay paths, so RenderWorld must install first.
- **Build empirically green at the right scopes.** `-t:clientGraphics` exits 0 producing `clientGraphics.lib` at 12,330,950 bytes (Wave 1 baseline 12,135,548 bytes -- delta +195,402 bytes for the new module's .obj contribution, expected). `-t:SwgClient` relinks SwgClient_d.exe at 72,551,936 bytes (Wave 1 baseline 72,541,696 bytes -- delta +10,240 bytes for the new module's text+data after dead-code-stripping at link time, expected). Pre-existing Koogie post-build MSB3073 fires AFTER the .exe is produced (identical condition to Wave 0/Wave 1 baselines).
- **THROWAWAY/D-15 cleanup markers in place.** 19 markers across the 5 affected files (DpvsProfileInstrumentation.cpp: 14, DpvsProfileInstrumentation.h: 2, SetupClientGraphics.cpp: 2, RenderWorld.h: 1, RenderWorld.cpp: 1). Wave 5 grep target `grep -rc THROWAWAY src/engine/client/library/clientGraphics/` continues to enumerate every Phase 10 site for the revert commit. (vcxproj uses `THROWAWAY ; D15 cleanup` style to dodge XML's `--` comment-content prohibition.)

## Task Commits

Each task was committed atomically on `koogie-msvc-cpp20-base`:

1. **Task 1: DpvsProfileInstrumentation.h public header** -- `3fb4c9804` (feat) -- 1 file created, 64 insertions. Static-only class with 11-method public surface (install / remove / onFrameEnd / toggleCapture / setRunLabel + 3 record* setters + 3 get* getters); private disabled default constructor; only `<string>` included; banner + class with 2 THROWAWAY markers.

2. **Task 2: DpvsProfileInstrumentation.cpp implementation** -- `f55da0cef` (feat) -- 1 file created, 404 insertions. 9 #includes (FirstClientGraphics.h precompiled-header first, then DpvsProfileInstrumentation.h, then engine includes, then `<cstdio> <cstring> <ctime> <string>`); anonymous-namespace state block; 11 public method bodies (toggleCapture / setRunLabel / record* / get* trivial; install/remove/onFrameEnd substantive); 6 anonymous-namespace helpers (sanitizeRunLabel, openCsv, closeCsv, writeRow, currentIsoTimestamp, reportOverlay). 14 THROWAWAY markers.

3. **Task 3: vcxproj + SetupClientGraphics hook + Rule 3 RenderWorld getter** -- `1f5cd24f1` (feat) -- 6 files changed, 54,016 insertions (4 source files + 2 force-added build logs ~54K lines). Three coordinated edits make the module part of the build graph; the Rule 3 auto-fix adds `RenderWorld::getDisableOcclusionCulling()` getter (declaration in RenderWorld.h, body in RenderWorld.cpp) because MSVC catches the .cpp's reference at compile time rather than the link-time error the plan author expected.

**Plan metadata commit:** Will follow this SUMMARY.md commit (sequential-mode owns the STATE/ROADMAP writes after SUMMARY per `<sequential_execution>`).

_Note: This plan has no TDD tasks -- all 3 tasks are `type="auto" tdd="false"` per the plan frontmatter. No `test(...)` RED commit is required because the instrumentation is a measurement surface, not a behaviour change; Wave 3's RenderWorld bracket + Wave 4's capture session exercise it._

## Files Created/Modified

**Created (2):**

- `src/engine/client/library/clientGraphics/include/public/clientGraphics/DpvsProfileInstrumentation.h` -- 65 lines. Static-only class with public 11-method surface; only `<string>` included; banner + class with 2 THROWAWAY/D-15 markers; private disabled default constructor (instantiation prevention pattern).
- `src/engine/client/library/clientGraphics/src/shared/DpvsProfileInstrumentation.cpp` -- ~400 lines. Anonymous-namespace state block; 11 public method bodies; 6 anonymous-namespace helpers. CSV writer matches analysis.py EXPECTED_HEADER. Sanitizer covers 4 injection paths in one pass. Overlay print via DEBUG_REPORT_PRINT. 14 THROWAWAY/D-15 markers.

**Modified (4):**

- `src/engine/client/library/clientGraphics/build/win32/clientGraphics.vcxproj` -- +5 lines. `<ClCompile Include="..\..\src\shared\DpvsProfileInstrumentation.cpp">` inserted alphabetical between DebugPrimitive.cpp and DynamicColorPolyPrimitive.cpp with the standard MaxSpeed Optimized condition; `<ClInclude Include="..\..\include\public\clientGraphics\DpvsProfileInstrumentation.h" />` in matching alphabetical slot. Both entries preceded by an XML-comment-safe THROWAWAY marker (`<!-- ... THROWAWAY ; D15 cleanup -->` because XML forbids `--` inside comments and `-` at the end).
- `src/engine/client/library/clientGraphics/src/win32/SetupClientGraphics.cpp` -- +10 lines. `#include "clientGraphics/DpvsProfileInstrumentation.h"` added near the top alphabetically (between DebugPrimitive.h and DynamicVertexBuffer.h); `DpvsProfileInstrumentation::install();` call inside the `data.use3dSystem` block, immediately after `RenderWorld::install()`. 2 THROWAWAY markers.
- `src/engine/client/library/clientGraphics/src/shared/RenderWorld.h` -- +2 lines. `static bool getDisableOcclusionCulling();` declaration with 1 THROWAWAY marker (Rule 3 auto-fix per plan acceptance criterion 'compiles standalone').
- `src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp` -- +7 lines. `RenderWorld::getDisableOcclusionCulling()` body returns `ms_disableOcclusionCulling`; placed immediately after the existing `setDisableOcclusionCulling()` at line 1151; 1 THROWAWAY marker.

**Force-added (2):**

- `.planning/phases/10-dpvs-culling-experiment/10-03-build-clientGraphics.log` -- MSBuild output from `-t:clientGraphics` (final EXIT=0 after the Rule 3 fix). Per Wave 0/Wave 1 `*.log` gitignore override precedent (Wave 0 `fc0e2a53a` and Wave 1 `9bd97c459` did the same).
- `.planning/phases/10-dpvs-culling-experiment/10-03-build-SwgClient.log` -- MSBuild output from `-t:SwgClient` (final EXIT=1 due to pre-existing Koogie post-build MSB3073 copy failure; SwgClient_d.exe was produced before the failure fired).

## Decisions Made

1. **Anonymous-namespace state + public-class-of-statics shape.** Matches RenderWorld precedent (RenderWorldNamespace + public RenderWorld). Keeps the public header to `<string>`-only includes -- no engine headers leak through to plan 10-04 callers.
2. **CSV file held open across the capture, not opened-and-closed per row.** Plan template suggested per-row open/write/close for crash-robustness; implementation holds the AbstractFile* open from F10-on to F10-off (or label change). Trade-off: lose the LAST partial row on crash vs. ~3600 file-open syscalls per session. Acceptable for 6-pass × 10-second capture sessions.
3. **Drain GPU pool before the capture-active gate.** Strict reading of Pitfall 4 would have created a stall hazard (pool fills if Begin/End runs each frame in plan 10-04 but PollResult doesn't). Implementation gates on ms_installed first, drains pool unconditionally, then gates on ms_captureActive for the row write. The Pitfall 4 intent ('don't write rows when capture is off') is satisfied; the structural stall hazard is avoided.
4. **CSV row writes 0 for gpu_us when disjoint=TRUE, not blank.** RESEARCH.md A4 estimates disjoint=TRUE rate <5% (so contamination is bounded). Writing 0 contaminates the on-pool median by <5%; writing blank requires careful sprintf escaping and the gain is minimal. Pragmatic choice; if Wave 4 captures show disjoint rate >5%, Wave 5 escalates by re-running on a more stable system.
5. **Build verification scoped to `-t:clientGraphics` then `-t:SwgClient`, NOT full swg.sln.** Per Phase 9 + 10-01 + 10-02 deviation precedent (out-of-scope editor/tool targets fail).
6. **DPVS-01 NOT marked complete here.** Engine module exists; CSV writer + overlay + sanitizer all in place. Full close still needs Wave 3 (RenderWorld brackets + Game::run hook + CuiIoWin F10/F11 + /setrunlabel) + Wave 4 (capture session) + Wave 5 (verdict).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking issue] Added RenderWorld::getDisableOcclusionCulling getter here rather than deferring to plan 10-04 Task 1**

- **Found during:** Task 3 build verification (`-t:clientGraphics`)
- **Issue:** Plan author expected the .cpp's reference to `RenderWorld::getDisableOcclusionCulling()` to surface as a LINK-time error (the .cpp would compile clean; the .lib would have an unresolved external; plan 04 Task 1 closes it). The plan's `<action>` for Task 3 Part C explicitly says: "If it fails with 'undefined reference to RenderWorld::getDisableOcclusionCulling' -- that's plan 04's task 1; do NOT fix here." But MSVC catches the reference at COMPILE time, not link: `error C2039: 'getDisableOcclusionCulling': is not a member of 'RenderWorld'` + `error C3861: 'getDisableOcclusionCulling': identifier not found`. The DpvsProfileInstrumentation.cpp would not even produce a .obj, so clientGraphics.lib would not rebuild and the plan-level `<success_criteria>` requirement "DpvsProfileInstrumentation module exists and **compiles standalone**" would fail.
- **Conflict resolution:** Plan-level `<success_criteria>` ("compiles standalone") takes precedence over Task 3 `<action>` ("do NOT fix here"). The action guidance was based on the wrong assumption about which compilation phase catches the missing symbol.
- **Fix:** Added the trivial 2-line getter declaration to `RenderWorld.h` (`static bool getDisableOcclusionCulling();`) and a 4-line body to `RenderWorld.cpp` (returns `ms_disableOcclusionCulling`). Mirrors the existing `setDisableOcclusionCulling` setter at RenderWorld.cpp:1151 verbatim. Both new sites carry THROWAWAY/D-15 markers so the Wave 5 cleanup grep enumerates them alongside the rest of the Phase 10 instrumentation.
- **Files modified:** `RenderWorld.h` (+2 lines), `RenderWorld.cpp` (+7 lines)
- **Commit:** `1f5cd24f1`
- **Impact on plan 10-04:** Task 1 ("Add `getDisableOcclusionCulling` getter to RenderWorld") becomes a no-op for that specific edit. Plan 10-04 author should mark Task 1 done-on-arrival in their planning.

**2. [Rule 3 - Blocking issue] vcxproj XML comment style fix**

- **Found during:** Task 3 build verification (first MSBuild attempt)
- **Issue:** Initial vcxproj edits used `<!-- Phase 10 -- DPVS profiling instrumentation (THROWAWAY; D-15) -->` style. XML forbids `--` inside comment content (used as comment-end-marker prefix) and forbids `-` as the last character before `-->`. MSBuild rejected the project file: `error MSB4025: The project file could not be loaded. An XML comment cannot contain '--', and '-' cannot be the last character.`
- **Fix:** Changed comment text to `<!-- Phase 10 DPVS profiling instrumentation (THROWAWAY ; D15 cleanup) -->`. Removes the `--` sequence and ensures no trailing `-`. THROWAWAY marker remains grep-able.
- **Files modified:** `clientGraphics.vcxproj` (1 comment style change applied to 2 occurrences)
- **Commit:** Folded into `1f5cd24f1` (Task 3 commit; pre-commit fix during build verification)

### Deferred Out-of-Scope Items

**1. [Out of Scope - Pre-existing] Koogie post-build copy MSB3073 failure (SwgClient.vcxproj)**

- **Found during:** Task 3 `-t:SwgClient` build verification
- **Issue:** After SwgClient.exe is linked, the project's post-build step fires `copy "D:\SWG\client-tools\..." "D:\SWG\SWGSource Client v3.0\"` pointing at Koogie's dev-machine paths. Identical failure logged in Wave 0 SUMMARY (commit `fc0e2a53a`) and Wave 1 SUMMARY (commit `9bd97c459`) as a pre-existing condition inherited from Koogie merge anchor `479d35df3`.
- **Disposition:** Per executor scope-boundary rule, OUT OF SCOPE for plan 10-03. SwgClient_d.exe at 72,551,936 bytes was already produced before the post-build copy fires.
- **Fix:** NONE applied. Already logged in Wave 0 + Wave 1 deferred-items.

**2. [Out of Scope - Pre-existing] MSB8012 TargetPath/TargetName warnings on Direct3d9.vcxproj + Direct3d9_vsps.vcxproj**

- **Found during:** `-t:SwgClient` transitive build (Direct3d9.dll + Direct3d9_vsps.dll built as dependencies)
- **Issue:** Both projects produce warnings about `$(TargetName)` mismatch with Linker `OutputFile` (`gl05_d.dll` / `gl07_d.dll`). Historical SWG plugin naming convention preserved from leak tree.
- **Disposition:** Pre-existing; not introduced by this plan. Phase 11 D3D11 candidate to revisit if a Direct3d11.dll lives alongside.
- **Fix:** NONE applied.

---

**Total deviations:** 2 auto-fixed (1 Rule 3 plan-author-expectation mismatch -- the compile vs link surface; 1 Rule 3 XML comment syntax) + 2 explicitly-deferred pre-existing out-of-scope failures.
**Impact on plan:** Rule 3 auto-fix #1 transfers ~6 lines of code from plan 10-04 Task 1 into this plan; plan 10-04 Task 1 becomes a no-op for the getter addition. Rule 3 auto-fix #2 is a content-only XML hygiene fix. The two deferred items are unchanged from Wave 0/Wave 1 baselines.

## Issues Encountered

None during planned work. Both Rule 3 auto-fixes are documented above under Deviations -- they are correctness fixes against an MSVC compile-time-vs-link-time expectation mismatch and an XML syntax rule, not problem-solving incidents.

## Threat Flags

No new threat surface introduced beyond the threat model captured in the plan's `<threat_model>`. Sanitizer is in place per T-10-W3-01 and T-10-W3-02; THROWAWAY markers in place per T-10-W3-04; debug-only DebugFlag overlay per T-10-W3-05 (DEBUG_REPORT_PRINT is `PRODUCTION == 0` only); CSV files are local-only per T-10-W3-06.

## Self-Check: PASSED

**Files referenced as created/modified exist:**

- `D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/include/public/clientGraphics/DpvsProfileInstrumentation.h` -- FOUND (Task 1 commit `3fb4c9804`)
- `D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/src/shared/DpvsProfileInstrumentation.cpp` -- FOUND (Task 2 commit `f55da0cef`)
- `D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/build/win32/clientGraphics.vcxproj` -- FOUND, contains both new entries (Task 3 commit `1f5cd24f1`)
- `D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/src/win32/SetupClientGraphics.cpp` -- FOUND, contains include + install() call (Task 3 commit `1f5cd24f1`)
- `D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/src/shared/RenderWorld.h` -- FOUND, contains new getDisableOcclusionCulling declaration (Task 3 commit `1f5cd24f1`)
- `D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp` -- FOUND, contains new getDisableOcclusionCulling body (Task 3 commit `1f5cd24f1`)

**Commits referenced exist on `koogie-msvc-cpp20-base`:**

- `3fb4c9804` -- FOUND (`feat(10-03): add DpvsProfileInstrumentation public header`)
- `f55da0cef` -- FOUND (`feat(10-03): implement DpvsProfileInstrumentation CSV writer + overlay`)
- `1f5cd24f1` -- FOUND (`feat(10-03): wire DpvsProfileInstrumentation into clientGraphics build`)

**Build artifacts confirmed:**

- `D:/Code/swg-client-v2/src/compile/win32/clientGraphics/Debug/clientGraphics.lib` -- FOUND (12,330,950 bytes, mtime 2026-05-10 23:52 -- Wave 1 baseline 12,135,548 bytes, delta +195,402 bytes for new module .obj)
- `D:/Code/swg-client-v2/src/compile/win32/SwgClient/Debug/SwgClient_d.exe` -- FOUND (72,551,936 bytes, mtime 2026-05-10 23:57 -- Wave 1 baseline 72,541,696 bytes, delta +10,240 bytes for new module text+data after dead-code stripping)

**Acceptance criteria spot-check (per plan):**

- Task 1: `class DpvsProfileInstrumentation` count = 1 in header ✓
- Task 1: `INCLUDED_DpvsProfileInstrumentation_H` count = 2 (open + close guard) ✓
- Task 1: `static void install()` ≥ 1 ✓
- Task 1: `static void onFrameEnd` ≥ 1 ✓
- Task 1: `static void toggleCapture` ≥ 1 ✓
- Task 1: `static void setRunLabel` ≥ 1 ✓
- Task 1: `static void recordCpuQpcUs` ≥ 1 ✓
- Task 1: `static bool getCaptureActive` ≥ 1 ✓
- Task 1: `THROWAWAY` ≥ 1 ✓ (2 hits)
- Task 1: `#include` count = 1 ✓ (only `<string>`)
- Task 2: `DpvsProfileInstrumentation::install` ≥ 1 ✓ (2 hits -- declaration + body)
- Task 2: `ExitChain::add` ≥ 1 ✓ (1 hit)
- Task 2: `sanitizeRunLabel` ≥ 1 ✓ (3 hits -- forward decl, call site, body)
- Task 2: `DEBUG_REPORT_PRINT` ≥ 1 ✓ (2 hits -- format string contains the substring)
- Task 2: `dpvsGpuTimingPollResult` ≥ 1 ✓ (1 hit)
- Task 2: `StdioFile` ≥ 1 ✓ (2 hits -- #include + ctor call)
- Task 2: `THROWAWAY` ≥ 1 ✓ (14 hits)
- Task 2: `FirstClientGraphics.h` ≥ 1 ✓ (1 hit)
- Task 2: `throw|catch` = 0 ✓ (no exceptions per CLAUDE.md)
- Task 3: `DpvsProfileInstrumentation::install` in SetupClientGraphics.cpp ≥ 1 ✓ (1 hit)
- Task 3: `THROWAWAY` in SetupClientGraphics.cpp ≥ 1 ✓ (2 hits)
- Task 3: `#include "clientGraphics/DpvsProfileInstrumentation.h"` in SetupClientGraphics.cpp ≥ 1 ✓ (1 hit)
- Task 3: `DpvsProfileInstrumentation.cpp` in clientGraphics.vcxproj ≥ 1 ✓ (1 hit)
- Task 3: clientGraphics builds clean ✓ (EXIT=0)
- Task 3: SwgClient links cleanly (Rule 3 auto-fix; pre-existing post-build MSB3073 the only failure) ✓
- Plan-level: THROWAWAY enumeration grep target across all 5 affected files returns ≥1 each ✓

**TDD gate compliance:** N/A -- this plan has `type: execute` (Wave 2 module), not `type: tdd`. All 3 tasks are `type="auto" tdd="false"`. No RED/GREEN/REFACTOR gates required.

## User Setup Required

None. This plan delivers the engine module only; no external service configuration. Wave 3 (plan 10-04) will wire the runtime call sites in RenderWorld.cpp + Game::run + CuiIoWin + SwgCuiCommandParserDefault, at which point the F10/F11 keybinds and `/setrunlabel` console command become operationally exercised. Capture session (Wave 4 / plan 10-05) is what Kenny will then drive against the SWGSource VM at 192.168.1.200:44453.

## Next Phase Readiness

**Wave 3 (plan 10-04) is unblocked.** Plan 10-04's `<read_first>` items + `<action>` blocks can consume:

- `DpvsProfileInstrumentation::onFrameEnd(float totalFrameMs)` -- Game::run() hook (RESEARCH.md A8: just after `Graphics::present()` returns)
- `DpvsProfileInstrumentation::toggleCapture()` -- CuiIoWin F10 hook (RESEARCH.md §F10/F11 hook, lines 544-565)
- `DpvsProfileInstrumentation::setRunLabel(std::string const &)` -- /setrunlabel command in SwgCuiCommandParserDefault (RESEARCH.md §Console command, lines 570-583)
- `DpvsProfileInstrumentation::recordCpuQpcUs(uint32)`, `recordProfilerUs(uint32)`, `recordVisibleObjectCount(int)` -- RenderWorld brackets around resolveVisibility (the QPC pair, the ProfilerBlock end value, and `RenderWorldCommander::getNumberOfVisibleObjects()`)

**Pre-conditions for Wave 3 met:**

- `DpvsProfileInstrumentation` is registered in `SetupClientGraphics::install()` and runs `ExitChain` teardown -- no lifecycle work left for plan 10-04.
- CSV writer + sanitizer + overlay + DebugFlag all in place; plan 10-04 only needs to wire CALL SITES, not new infrastructure.
- `RenderWorld::getDisableOcclusionCulling()` getter exists (Rule 3 auto-fix completed plan 10-04 Task 1 for that specific edit).
- Build is green at -t:clientGraphics and -t:SwgClient; plan 10-04's per-task build smokes can use the same scope.

**Open items carried forward:**

- DPVS-01 requirement: STILL PARTIAL (engine module ready; runtime call sites still need to be wired in Wave 3; measurement half lands in Wave 4-5).
- DPVS-02 requirement: NOT STARTED (conditional D-13/D-14 source edits live in plan 10-06; gated by Wave 5 verdict outcome).
- Wave 5 cleanup commit will revert all THROWAWAY-marked code from this plan: DpvsProfileInstrumentation.{h,cpp} (full delete), SetupClientGraphics.cpp lines (include + install call), RenderWorld.{h,cpp} (getDisableOcclusionCulling getter), clientGraphics.vcxproj (ClCompile + ClInclude entries). Total ~470 lines across 5 files (excluding RenderWorld.cpp's getter setter precedent line at 1151, which D-14 will retire separately if verdict=remove).
- Pre-existing Koogie post-build copy MSB3073 failure: still logged for deferred-items.md; unchanged from Wave 0/Wave 1.
- Pre-existing Direct3d9.vcxproj / Direct3d9_vsps.vcxproj MSB8012 TargetName/OutputFile warnings: still logged; Phase 11 candidate.

---
*Phase: 10-dpvs-culling-experiment*
*Plan: 10-03 (Wave 2 -- DpvsProfileInstrumentation engine module)*
*Completed: 2026-05-11*
