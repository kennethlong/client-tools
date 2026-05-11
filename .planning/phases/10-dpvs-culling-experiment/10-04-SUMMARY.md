---
phase: 10-dpvs-culling-experiment
plan: 04
subsystem: clientGraphics-engine-wiring + clientGame-runloop + clientUserInterface-input + swgClientUserInterface-parser
tags: [dpvs, profiling, gpu-timing, cpu-timing, keybinds, console-command, hook-wiring, source-edit, throwaway, checkpoint-pending]

# Dependency graph
requires:
  - phase: 10-dpvs-culling-experiment
    plan: 03
    provides: "DpvsProfileInstrumentation engine module (public 11-method static surface: install/remove/onFrameEnd/toggleCapture/setRunLabel/recordCpuQpcUs/recordProfilerUs/recordVisibleObjectCount + 3 getters); RenderWorld::getDisableOcclusionCulling getter pre-landed via Rule 3 auto-fix in 1f5cd24f1; Graphics::dpvsGpuTimingBegin/End/PollResult engine forwarders from Wave 1 (plan 10-02)"
provides:
  - "RenderWorld.cpp: Graphics::dpvsGpuTimingBegin/End bracket around the resolveVisibility() call site plus PerformanceTimer CPU timer; CPU microseconds + visible-object count pushed into DpvsProfileInstrumentation::record* each frame"
  - "Game.cpp: DpvsProfileInstrumentation::onFrameEnd(elapsedTime * 1000.0f) call immediately after Graphics::present() returns, once per Game::run() iteration"
  - "CuiIoWin.cpp: _DEBUG-only F10/F11 intercept in IOET_KeyDown immediately after AwayFromKeyBoardManager::touch() and BEFORE the m_keyboardInputActive InputMap dispatch -- F10 routes to DpvsProfileInstrumentation::toggleCapture; F11 reads-and-flips RenderWorld::setDisableOcclusionCulling"
  - "SwgCuiCommandParserDefault.cpp: /setrunlabel console command registered (MAKE_COMMAND + cmds[] entry + performParsing else-if branch); joins argv[1..] with underscores via Unicode::wideToNarrow and hands the sanitized result to DpvsProfileInstrumentation::setRunLabel"
  - "SwgClient_d.exe rebuilt at 72,552,448 bytes (delta +512 bytes vs Wave 3 baseline 72,551,936 -- new code text from the four hook sites after dead-code stripping at link time, expected)"
affects: [10-05, 10-06, 10-07]  # Wave 5 (10-05) capture session consumes F10/F11/setrunlabel/onFrameEnd; Wave 6 (10-06) conditional D-13 source-edit; Wave 7 (10-07) THROWAWAY cleanup commit greps these sites for revert

# Tech tracking
tech-stack:
  added: []  # No new external dependencies; uses existing engine surfaces (PerformanceTimer from sharedDebug, Graphics::dpvsGpuTiming* from Wave 1, DpvsProfileInstrumentation:: from Wave 2, DIK_F10/F11 from existing <dinput.h>, Unicode::wideToNarrow already used 32x in SwgCuiCommandParserDefault.cpp)
  patterns:
    - "GPU>=CPU enclosing bracket pattern: Graphics::dpvsGpuTimingBegin() opens the GPU disjoint scope BEFORE PerformanceTimer::start(); Graphics::dpvsGpuTimingEnd() closes the GPU scope AFTER PerformanceTimer::stop(). The GPU window encloses the CPU window so any visibility query that touches the GPU has GPU >= CPU as a sanity check in the verdict data."
    - "_DEBUG-only direct input intercept BEFORE InputMap dispatch: F10/F11 are unbound in source today (RESEARCH.md A2); the #ifdef _DEBUG hook runs first in IOET_KeyDown and consumes the event with retval=true; break; This preempts any TRE-archived input-map binding -- desired per RESEARCH.md A2. T-10-W4-01 threat mitigated by the production compile-out fence."
    - "Console command afkmessage template mirror: MAKE_COMMAND in the Commands namespace block + a cmds[] table entry between alphabetized siblings + a performParsing else-if branch after the template's branch. Argv joining via Unicode::wideToNarrow with underscore separator. THROWAWAY markers on every new line so Wave 5 cleanup grep enumerates all sites."
    - "Pre-landed Rule 3 fix detected: RenderWorld::getDisableOcclusionCulling getter (declaration in RenderWorld.h, body in RenderWorld.cpp) was already committed in Wave 3 (1f5cd24f1) as the Rule 3 auto-fix Wave 3 executor applied when the linker-vs-compiler surface mismatch surfaced. Detected via git log on RenderWorld.h/.cpp before starting; Task 1 Part A reduced to no-op for that edit. Documented in deviations."

key-files:
  created: []  # No new source files; all hooks land in pre-existing files
  modified:
    - "src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp (+28 lines, -4 lines: 2 includes [DpvsProfileInstrumentation.h, sharedDebug/PerformanceTimer.h] + GPU/CPU bracket around the existing resolveVisibility() block + 3 record* push calls)"
    - "src/engine/client/library/clientGame/src/shared/core/Game.cpp (+8 lines: 1 include + 1 onFrameEnd call after Graphics::present())"
    - "src/engine/client/library/clientUserInterface/src/shared/core/CuiIoWin.cpp (+22 lines: 5-line _DEBUG include block + 17-line _DEBUG F10/F11 intercept in IOET_KeyDown)"
    - "src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserDefault.cpp (+25 lines: 2 include lines + 1 MAKE_COMMAND line + 2 cmds[] entry lines + 1 performParsing else-if branch ~16 lines)"
  force-added:
    - ".planning/phases/10-dpvs-culling-experiment/10-04-build.log (~53k lines: SwgClient build output post-Task-3 -- SwgClient_d.exe rebuilt at 72,552,448 bytes; exit code 1 solely due to pre-existing Koogie post-build MSB3073 identical to Wave 0/1/2/3)"

key-decisions:
  - "Detected pre-landed Rule 3 fix from Wave 3 BEFORE starting Task 1. `git log --oneline --all -- RenderWorld.h RenderWorld.cpp` showed commit 1f5cd24f1 as the last touch -- the Wave 3 SUMMARY explicitly documented this as a Rule 3 auto-fix that 'transfers ~6 lines of code from plan 10-04 Task 1 into this plan; plan 10-04 Task 1 becomes a no-op for the getter addition.' Skipped Task 1 Part A as a no-op; only added the GPU/CPU bracket + record* pushes in Task 1 Part B."
  - "Per-task incremental builds scoped to the target whose library the task modified (-t:clientGraphics for Task 1; -t:clientGame -t:clientUserInterface for Task 2; -t:swgClientUserInterface for Task 3). Faster than -t:SwgClient at each step; the full SwgClient link is exercised once at Task 4. Matches the Wave 1/2/3 per-task scoping precedent."
  - "Build verification scoped to `-t:SwgClient` at Task 4 (NOT full swg.sln) per Phase 9 + 10-01/02/03 deviation precedent. The relevant test is 'does SwgClient still link with the four new hook sites' -- verified by SwgClient_d.exe rebuild at 72,552,448 bytes (+512 bytes vs Wave 3 baseline). Pre-existing Koogie post-build MSB3073 fires AFTER the .exe is produced (identical to Wave 0/1/2/3 baselines, logged in 10-04-build.log)."
  - "Task 5 (checkpoint:human-verify smoke session) NOT executed by this executor. Plan is `autonomous: false`; the prompt's `<key_context>` explicitly notes 'F10/F11 keybinds compile (functional verification is Wave 5's capture session -- not your job here).' The smoke session is the Wave 5 capture session itself -- Kenny launches the rebuilt EXE, drives the protocol in tools/dpvs-profile/test-protocol.md, and produces the 6 CSV files for the verdict. This SUMMARY documents that Tasks 1-4 are complete and the EXE is ready for that session."
  - "DPVS-01 advances further but NOT marked complete here. Engine + plumbing + wiring all in place; only the CAPTURE half (Wave 5 = 10-05) and the VERDICT half (Wave 5 = same plan + 10-07 cleanup) remain. STATE.md and ROADMAP.md updates reflect plan 10-04 complete + DPVS-01 still partial."

patterns-established:
  - "Pre-landed-task detection protocol: before starting any plan Task, `git log --oneline --all -- <files-listed-in-task>` to surface prior commits touching the same files. If a prior wave's SUMMARY claims Rule 3 auto-fix pre-landing the work, verify the diff and skip rather than redo. Documented in deviations as 'pre-landed; no edit'."
  - "GPU-encloses-CPU bracket idiom for cross-domain timing: GPU Begin → CPU start → work → CPU stop → GPU End → record both. Reusable for any other profiling site that needs synchronized GPU/CPU traces with the GPU as the strict outer bound."
  - "_DEBUG-only direct-input intercept in IOET_KeyDown is the right seam for build-time-only debug keybinds. Compiled out entirely in production via #ifdef _DEBUG (T-10-W4-01 mitigation). Wave 5 cleanup commit just removes the #ifdef block."

requirements-completed: []  # DPVS-01 advances; full close still needs Wave 5 capture session + Wave 7 verdict writeup + cleanup. Wave 6 conditional D-13 source-edit gated by verdict outcome.

# Metrics
duration: ~5min (executor-time -- Task 1 RenderWorld edit + clientGraphics build ~1.5min; Task 2 Game.cpp + CuiIoWin.cpp edits + clientGame + clientUserInterface builds ~1.5min; Task 3 SwgCuiCommandParserDefault edits + swgClientUserInterface build ~1min; Task 4 SwgClient build smoke + log force-add ~1min)
completed: 2026-05-11
---

# Phase 10 Plan 10-04: Wave 3 Hook Wiring -- DpvsProfileInstrumentation Live in the Engine

**Four atomic commits wire DpvsProfileInstrumentation (built in Wave 2) into the running engine: GPU/CPU bracket around resolveVisibility() with CPU + visible-object metrics pushed into the module each frame; onFrameEnd hook in Game::run() immediately after Graphics::present(); _DEBUG-only F10/F11 intercept in CuiIoWin::IOET_KeyDown; /setrunlabel console command in SwgCuiCommandParserDefault. SwgClient_d.exe rebuilt at 72,552,448 bytes (+512 bytes vs Wave 3 baseline). Pre-existing Koogie post-build MSB3073 is the only failure -- identical to all prior Phase 10 wave baselines. Task 5 (checkpoint:human-verify smoke session) deferred to Wave 5's capture session per `autonomous: false` plan policy and the prompt's explicit out-of-scope note for this executor.**

## Performance

- **Duration:** ~5 min executor-time
- **Started:** 2026-05-11T05:06:19Z
- **Completed:** 2026-05-11T05:11:16Z
- **Tasks:** 4 atomic commits landed (Tasks 1-4); Task 5 checkpoint deferred to Wave 5
- **Files modified:** 4 source files (RenderWorld.cpp, Game.cpp, CuiIoWin.cpp, SwgCuiCommandParserDefault.cpp); 1 build log force-added (10-04-build.log) per Wave 0/1/2/3 *.log gitignore override precedent

## Accomplishments

- **Resolved-visibility GPU/CPU bracket landed.** RenderWorld.cpp now wraps the existing `ms_dpvsCamera->resolveVisibility(...)` block with `Graphics::dpvsGpuTimingBegin()` / `Graphics::dpvsGpuTimingEnd()` outside a `PerformanceTimer dpvsCpuTimer.start()/.stop()` pair. After the close, the QPC microseconds value and `RenderWorldCommander::getNumberOfVisibleObjects()` are pushed into the instrumentation module via `DpvsProfileInstrumentation::recordCpuQpcUs()` and `::recordVisibleObjectCount()`. Profiler column gets `recordProfilerUs(0u)` per RESEARCH.md Pitfall 3 (the ProfilerBlock end fires inside `RenderWorldCommander::command()` `QUERY_BEGIN`, unreachable from a mid-frame caller; the QPC value is the authoritative CPU number per CONTEXT D-09).
- **Per-frame onFrameEnd hook landed.** `Game::run()` now calls `DpvsProfileInstrumentation::onFrameEnd(elapsedTime * 1000.0f)` immediately after `Graphics::present()` returns. `elapsedTime` was already read at line ~1097 from `Clock::frameTime()` as the previous frame's duration in seconds; converted to milliseconds for the CSV row. Pitfall 6 1-frame skew between frame_no and total_frame_ms is documented in the inline comment and is insensitive to median/p95 over ~600-row samples.
- **F10/F11 debug intercept landed.** `CuiIoWin::processEvent`'s `IOET_KeyDown` branch now has a `#ifdef _DEBUG` block immediately after `AwayFromKeyBoardManager::touch()` and BEFORE the `m_keyboardInputActive` InputMap dispatch. F10 routes to `DpvsProfileInstrumentation::toggleCapture()`; F11 reads-and-flips `RenderWorld::setDisableOcclusionCulling(!RenderWorld::getDisableOcclusionCulling())`. Both consume the event with `retval = true; break;` so the InputMap dispatch is preempted (acceptable per RESEARCH.md A2 -- F10/F11 are not in source today; TRE-archived bindings would have been preempted regardless). Production builds compile the entire block out via the `#ifdef _DEBUG` fence (T-10-W4-01 mitigation).
- **/setrunlabel console command landed.** Three coordinated edits in `SwgCuiCommandParserDefault.cpp` mirroring the afkmessage template byte-for-byte: `MAKE_COMMAND(setrunlabel)` in the Commands namespace block; a new `cmds[]` entry with usage `<label>` and a Phase-10-tagged description; a new `performParsing` else-if branch immediately after the afkmessage branch. The branch joins `argv[1..]` with underscores via `Unicode::wideToNarrow` (the existing helper used 32x in this file) and hands the result to `DpvsProfileInstrumentation::setRunLabel`, which runs the RESEARCH.md Security Domain sanitizer (path-traversal, formula-injection, CSV-cell-injection, filename-component-injection all normalized in a single pass per Wave 2's T-10-W3-01/02 mitigations).
- **The link closed.** `-t:SwgClient` build relinks `SwgClient_d.exe` at 72,552,448 bytes -- delta +512 bytes from the Wave 3 baseline 72,551,936, expected for the four new hook sites' text after dead-code stripping. All `DpvsProfileInstrumentation::*` symbols and `Graphics::dpvsGpuTiming*` symbols resolve cleanly. The two-plan handoff (Wave 2's `DpvsProfileInstrumentation` static class + Wave 3's hook sites) is now operational end-to-end at the binary level.
- **Pre-landed Rule 3 fix detected and respected.** Before starting Task 1, `git log --oneline --all -- RenderWorld.h RenderWorld.cpp` revealed that commit `1f5cd24f1` (Wave 3, plan 10-03 Task 3) had already added the `RenderWorld::getDisableOcclusionCulling` getter declaration (RenderWorld.h:105-106) and body (RenderWorld.cpp:1157-1163). The Wave 3 SUMMARY explicitly documented this as a Rule 3 auto-fix that "transfers ~6 lines of code from plan 10-04 Task 1 into this plan; plan 10-04 Task 1 becomes a no-op for the getter addition." Honored that contract: Task 1 Part A was skipped as a no-op; only the GPU/CPU bracket + record* pushes (Task 1 Part B) were applied in this plan.
- **THROWAWAY/D-15 cleanup markers everywhere.** The plan's success criterion #6 ("All edits carry THROWAWAY/D-15 markers for Wave 5+ cleanup grep") is met across all 4 modified source files: RenderWorld.cpp (+4 new markers since Wave 3 already had 1 on the getter body), Game.cpp (2), CuiIoWin.cpp (2), SwgCuiCommandParserDefault.cpp (5 new since Wave 0 had none here). Wave 5 cleanup grep target `grep -rc THROWAWAY src/engine/client src/game/client/library/swgClientUserInterface` continues to enumerate every Phase 10 site for the revert commit.

## Task Commits

Each task was committed atomically on `koogie-msvc-cpp20-base`:

1. **Task 1: RenderWorld GPU/CPU bracket around resolveVisibility() (Part A pre-landed)** -- `cca3dcebc` (feat) -- 1 file changed, 28 insertions(+), 4 deletions(-). Adds `DpvsProfileInstrumentation.h` + `sharedDebug/PerformanceTimer.h` includes; wraps the existing `ms_dpvsCamera->resolveVisibility(...)` block with `Graphics::dpvsGpuTimingBegin/End` + `PerformanceTimer dpvsCpuTimer`; pushes CPU microseconds + visible-object count + 0 profiler microseconds into the module each frame. Task 1 Part A (RenderWorld.h getter declaration) was pre-landed in Wave 3 commit `1f5cd24f1` and skipped as a no-op.

2. **Task 2: Game::run onFrameEnd hook + CuiIoWin F10/F11 _DEBUG intercept** -- `2688a0bdd` (feat) -- 2 files changed, 37 insertions(+), 1 deletion(-). Adds `DpvsProfileInstrumentation.h` include + the per-frame `onFrameEnd(elapsedTime * 1000.0f)` call immediately after `Graphics::present()` in Game.cpp. In CuiIoWin.cpp, adds a 5-line `_DEBUG`-only include block and a 17-line `_DEBUG`-only F10/F11 intercept in `IOET_KeyDown` immediately after `AwayFromKeyBoardManager::touch()` and BEFORE `m_keyboardInputActive` InputMap dispatch.

3. **Task 3: /setrunlabel console command in SwgCuiCommandParserDefault** -- `bf464e3ee` (feat) -- 1 file changed, 25 insertions(+). Three coordinated edits mirroring the afkmessage template: `MAKE_COMMAND(setrunlabel)` in the Commands namespace block; new `cmds[]` table entry; new `performParsing` else-if branch joining `argv[1..]` with underscores via `Unicode::wideToNarrow` and handing the result to `DpvsProfileInstrumentation::setRunLabel`. `DpvsProfileInstrumentation.h` include added near the other `clientGraphics/` headers.

4. **Task 4: SwgClient build log + close-the-link verification** -- `d8dbd4076` (chore) -- 1 file changed, 53,022 insertions(+). Force-added `10-04-build.log` per Wave 0/1/2/3 *.log gitignore-override precedent. SwgClient_d.exe rebuilt at 72,552,448 bytes (+512 bytes vs Wave 3 baseline 72,551,936); exit code 1 solely due to the pre-existing Koogie post-build MSB3073 (identical condition to all prior Phase 10 wave baselines, logged in 10-04-build.log).

5. **Task 5: Smoke session checkpoint** -- NOT EXECUTED. See "Checkpoint Deferred" section below.

**Plan metadata commit:** Will follow this SUMMARY.md commit (sequential-mode owns the STATE/ROADMAP writes after SUMMARY per `<sequential_execution>`).

_Note: This plan has no TDD tasks -- all 5 tasks are `type="auto" tdd="false"` or `type="checkpoint:human-verify"` per the plan frontmatter. No `test(...)` RED commit is required because the hook sites are measurement surfaces, not behaviour changes; Wave 5's capture session exercises them functionally._

## Files Modified

**Modified (4 source files):**

- `src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp` -- +28 lines, -4 lines. 2 new `#include` lines (DpvsProfileInstrumentation.h + sharedDebug/PerformanceTimer.h, both with THROWAWAY/D-15 markers); 18 new lines wrapping `ms_dpvsCamera->resolveVisibility(...)` with `Graphics::dpvsGpuTimingBegin()` + `PerformanceTimer dpvsCpuTimer.start()` outside / `dpvsCpuTimer.stop()` + `Graphics::dpvsGpuTimingEnd()` close + 3 `DpvsProfileInstrumentation::record*` push calls. 4 new THROWAWAY/D-15 markers in this plan (Wave 3 had 1 on the getter body for 5 total in this file).
- `src/engine/client/library/clientGame/src/shared/core/Game.cpp` -- +8 lines. 1 new `#include` line (DpvsProfileInstrumentation.h with THROWAWAY/D-15 marker, alphabetically before `clientGraphics/Graphics.h`); 7 lines for the per-frame `onFrameEnd(elapsedTime * 1000.0f)` call immediately after the `Graphics::present()` block, with a 4-line comment explaining the elapsedTime unit conversion + Pitfall 6 1-frame skew. 2 new THROWAWAY/D-15 markers.
- `src/engine/client/library/clientUserInterface/src/shared/core/CuiIoWin.cpp` -- +22 lines. 5-line `_DEBUG`-only include block (DpvsProfileInstrumentation.h + RenderWorld.h, before `clientGraphics/Graphics.h`); 17-line `_DEBUG`-only F10/F11 intercept inside `IOET_KeyDown` immediately after `AwayFromKeyBoardManager::touch()` line. 2 new THROWAWAY/D-15 markers.
- `src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserDefault.cpp` -- +25 lines. 2 new `#include` lines (DpvsProfileInstrumentation.h alphabetically before `clientGraphics/Graphics.h`, with the THROWAWAY/D-15 marker comment immediately above); 1 new `MAKE_COMMAND(setrunlabel)` line in the Commands namespace block; 2 new lines in the `cmds[]` table (1-line comment + 1-line entry); 19 lines for the new `else if (isCommand(argv[0], Commands::setrunlabel))` performParsing branch with a 5-line explanatory comment. 5 new THROWAWAY/D-15 markers.

**Force-added (1 build log):**

- `.planning/phases/10-dpvs-culling-experiment/10-04-build.log` -- ~53k lines of MSBuild stdout/stderr from `-t:SwgClient`. SwgClient.exe lands successfully; Koogie post-build MSB3073 fires AFTER. Per Wave 0/1/2/3 *.log gitignore override precedent.

**Created (0):** No new source files; all 4 hook sites land in pre-existing files.

## Decisions Made

1. **Pre-landed-task detection executed before starting.** `git log --oneline --all -- RenderWorld.h RenderWorld.cpp` surfaced commit `1f5cd24f1` (Wave 3) as the last touch, with the Wave 3 SUMMARY explicitly noting the Rule 3 auto-fix pre-landed the `getDisableOcclusionCulling` getter. Task 1 Part A skipped as a no-op; only Task 1 Part B applied in this plan. Saved ~6 lines of redundant edit and avoided a "double commit" of the same logical change.
2. **Per-task incremental builds at the most-specific scope.** Task 1 → `-t:clientGraphics`; Task 2 → `-t:clientGame -t:clientUserInterface`; Task 3 → `-t:swgClientUserInterface`. The full SwgClient link is exercised at Task 4 once. Faster than rebuilding -t:SwgClient at each step and gives a faster fail-signal localized to the edited target.
3. **Task 4 build scoped to `-t:SwgClient`.** Full swg.sln has known out-of-scope editor/tool target failures per PROJECT.md "Out of Scope" + Phase 9 closeout + Wave 0/1/2/3 baselines. The relevant test for plan 10-04 is "does SwgClient still link after the four new hook sites" -- verified by SwgClient_d.exe rebuild.
4. **Task 5 (checkpoint:human-verify smoke session) deferred to Wave 5.** Plan is `autonomous: false`; the prompt explicitly notes "F10/F11 keybinds compile (functional verification is Wave 5's capture session -- not your job here)." The smoke session content (launch client, zone to Mos Eisley plaza, drive F11/F10/setrunlabel, verify CSV) IS the Wave 5 capture session. Surfacing it as a separate checkpoint here would just duplicate the Wave 5 plan's `<task>` content. Documented in "Checkpoint Deferred" below for the orchestrator.
5. **DPVS-01 NOT marked complete here.** Engine module exists; CPU+GPU timing brackets wired; F10/F11/setrunlabel call sites wired. But the CAPTURE half (Wave 5 = 10-05) hasn't run yet, and the VERDICT half (Wave 5 + 10-07 cleanup) hasn't been written. STATE.md / ROADMAP.md updates reflect plan 10-04 complete + DPVS-01 still partial.

## Deviations from Plan

### Auto-fixed Issues

**None.** Both critical surfaces (the Rule 3 RenderWorld getter pre-landing detection in Task 1; the Koogie post-build MSB3073 in Task 4) were known going in: the prompt's `<key_context>` explicitly told the executor to detect the pre-landed work via git log and skip it, and Wave 0/1/2/3 SUMMARIES all logged MSB3073 as a pre-existing out-of-scope condition. Both were applied as expected without requiring on-the-fly deviation work.

### Pre-existing Work Detected and Skipped

**1. [Detected pre-landed] RenderWorld::getDisableOcclusionCulling getter (Task 1 Part A)**

- **Found during:** Pre-Task-1 `git log --oneline --all -- RenderWorld.h RenderWorld.cpp`
- **Detection:** Commit `1f5cd24f1` (Wave 3, plan 10-03 Task 3) was the last touch on both files. The Wave 3 SUMMARY explicitly documented this as a Rule 3 auto-fix pre-landing the getter declaration + body, with the note "plan 10-04 Task 1 becomes a no-op for the getter addition."
- **Verification:** RenderWorld.h lines 105-106 show `// Phase 10 -- DPVS profiling instrumentation (THROWAWAY; D-15 cleanup target)` + `static bool getDisableOcclusionCulling();`. RenderWorld.cpp lines 1156-1163 show the corresponding body returning `ms_disableOcclusionCulling`.
- **Action:** Skipped Task 1 Part A. Applied only Task 1 Part B (the GPU/CPU bracket + record* pushes around resolveVisibility()).
- **Impact:** Saved ~6 lines of redundant edit; avoided a "double commit" of the same logical change. The plan's `<acceptance_criteria>` `grep -c 'getDisableOcclusionCulling' RenderWorld.h >= 1` and `grep -c 'bool RenderWorld::getDisableOcclusionCulling' RenderWorld.cpp >= 1` both pass against the pre-landed Wave 3 state.

### Deferred Out-of-Scope Items

**1. [Out of Scope - Pre-existing] Koogie post-build copy MSB3073 failure (SwgClient.vcxproj)**

- **Found during:** Task 4 `-t:SwgClient` build verification.
- **Issue:** After SwgClient.exe is linked, the project's post-build step fires `copy "D:\SWG\client-tools\..." "D:\SWG\SWGSource Client v3.0\"` pointing at Koogie's dev-machine paths. Identical failure logged in Wave 0 SUMMARY (commit `fc0e2a53a`), Wave 1 SUMMARY (commit `9bd97c459`), Wave 2 SUMMARY (commit `c3aea6750`), Wave 3 SUMMARY (commit `eea9d11f1`) as a pre-existing condition inherited from Koogie merge anchor `479d35df3`.
- **Disposition:** Per executor scope-boundary rule, OUT OF SCOPE for plan 10-04. SwgClient_d.exe at 72,552,448 bytes was already produced before the post-build copy fires.
- **Fix:** NONE applied. Already logged in Wave 0/1/2/3 deferred-items.

**2. [Out of Scope - Pre-existing] MSB8012 TargetPath/TargetName warnings on Direct3d9.vcxproj + Direct3d9_vsps.vcxproj**

- **Found during:** `-t:SwgClient` transitive build.
- **Issue:** Both projects produce warnings about `$(TargetName)` mismatch with Linker `OutputFile` (`gl05_d.dll` / `gl07_d.dll`). Historical SWG plugin naming convention preserved from leak tree.
- **Disposition:** Pre-existing; not introduced by this plan. Phase 11 D3D11 candidate to revisit.
- **Fix:** NONE applied.

---

**Total deviations:** 0 auto-fixed (both pre-existing surfaces were anticipated and pre-documented). 1 pre-landed-work detection (Task 1 Part A skipped). 2 explicitly-deferred pre-existing out-of-scope failures.

## Issues Encountered

None during planned work. All four mechanical Tasks (1-4) landed cleanly. Pre-landed Rule 3 detection executed as a normal pre-check, not as a problem-solving incident. The two deferred items are unchanged from Wave 0/1/2/3 baselines.

## Checkpoint Deferred

**Task 5 = `type="checkpoint:human-verify"` -- smoke session deferred to Wave 5 (plan 10-05).**

The plan's Task 5 specifies a manual verification protocol Kenny drives:
- Launch `D:/Code/swg-client-v2/stage/SwgClient_d.exe`; log in; pick a Tatooine character.
- Zone into Mos Eisley plaza.
- Verify F11 toggles `DPVS:ON/OFF` in the overlay.
- Verify `/setrunlabel smoketest1` updates `run=` in the overlay.
- Verify F10 toggles `... / REC`; verify a CSV file appears in `D:/Code/swg-client-v2/stage/dpvs-profile/`.
- Verify path-traversal and formula-injection smoke tests against the sanitizer.

Per the executor prompt's `<key_context>`:
> "F10/F11 keybinds compile (functional verification is Wave 5's capture session -- not your job here)."

And per the `<sequential_execution>` block:
> "You are running as a SEQUENTIAL executor agent ... If you hit a checkpoint mid-plan, return structured payload to orchestrator (do NOT auto-approve)."

This SUMMARY records Tasks 1-4 complete + the build closing the link + the EXE ready for Wave 5's capture session. The Wave 5 plan (10-05) will own the actual capture session including all 6 CSV files, the overlay verification, and the sanitizer smoke tests -- those are not extra steps appended to Wave 4; they are Wave 5's reason-to-exist.

**The checkpoint payload returned to the orchestrator (after this SUMMARY commits) is the structured "Checkpoint Reached" notice describing Wave 4 done + the manual verification work now waiting for Kenny in Wave 5.**

## Threat Flags

No new threat surface introduced beyond the threat model captured in the plan's `<threat_model>`. T-10-W4-01 (E -- F10/F11 in production) mitigated by the `#ifdef _DEBUG` fence wrapping the CuiIoWin intercept block. T-10-W4-02 (T -- TRE keybind preemption) accepted per RESEARCH.md A2 (F10/F11 not in source today; preemption is desired behavior). T-10-W4-03 (T -- stale instrumentation) mitigated by THROWAWAY/D-15 markers across all 4 modified files. T-10-W4-04 (I -- getter exposure) accepted; getter is part of the THROWAWAY surface anyway. T-10-W4-05 (D -- bracket overhead) accepted at <50 µs per frame.

## Self-Check: PASSED

**Files referenced as modified exist with the expected content:**

- `D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp` -- FOUND. Contains `Graphics::dpvsGpuTimingBegin` (Task 1 commit `cca3dcebc`); `Graphics::dpvsGpuTimingEnd`; `PerformanceTimer dpvsCpuTimer`; `DpvsProfileInstrumentation::recordCpuQpcUs`; `DpvsProfileInstrumentation::recordVisibleObjectCount`; `DpvsProfileInstrumentation::recordProfilerUs`; 4 new THROWAWAY markers (5 total in file including Wave 3's getter-body marker).
- `D:/Code/swg-client-v2/src/engine/client/library/clientGame/src/shared/core/Game.cpp` -- FOUND. Contains `DpvsProfileInstrumentation::onFrameEnd` (Task 2 commit `2688a0bdd`); `#include "clientGraphics/DpvsProfileInstrumentation.h"`; 2 THROWAWAY markers; existing `Graphics::present` count preserved at 3 occurrences (line 1230 windowed + line 1232 unwindowed + line 1236 comment).
- `D:/Code/swg-client-v2/src/engine/client/library/clientUserInterface/src/shared/core/CuiIoWin.cpp` -- FOUND. Contains `DIK_F10` (Task 2 commit `2688a0bdd`); `DIK_F11`; `DpvsProfileInstrumentation::toggleCapture`; `RenderWorld::setDisableOcclusionCulling`; `RenderWorld::getDisableOcclusionCulling`; 2 THROWAWAY markers; new `#ifdef _DEBUG` block-count increased by 2 (1 for the include, 1 for the intercept).
- `D:/Code/swg-client-v2/src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserDefault.cpp` -- FOUND. Contains `MAKE_COMMAND (setrunlabel)` (Task 3 commit `bf464e3ee`); `Commands::setrunlabel` 2x (cmds[] entry + performParsing branch); `DpvsProfileInstrumentation::setRunLabel` 1x; `#include "clientGraphics/DpvsProfileInstrumentation.h"`; `Unicode::wideToNarrow` 33x (32 pre-existing + 1 new); 5 new THROWAWAY markers.

**Commits referenced exist on `koogie-msvc-cpp20-base`:**

- `cca3dcebc` -- FOUND (`feat(10-04): bracket resolveVisibility with GPU/CPU timers in RenderWorld`)
- `2688a0bdd` -- FOUND (`feat(10-04): hook onFrameEnd in Game::run + F10/F11 debug intercept in CuiIoWin`)
- `bf464e3ee` -- FOUND (`feat(10-04): register /setrunlabel console command for DPVS profiling`)
- `d8dbd4076` -- FOUND (`chore(10-04): record SwgClient build log after Wave 4 hook wiring`)

**Build artifacts confirmed:**

- `D:/Code/swg-client-v2/src/compile/win32/SwgClient/Debug/SwgClient_d.exe` -- FOUND (72,552,448 bytes, mtime 2026-05-11 00:10 -- Wave 3 baseline 72,551,936 bytes, delta +512 bytes for the four new hook sites' text after dead-code stripping).

**Acceptance criteria spot-check (per plan):**

- Task 1: `getDisableOcclusionCulling` in RenderWorld.h ≥ 1 ✓ (1 hit, pre-landed Wave 3)
- Task 1: `bool RenderWorld::getDisableOcclusionCulling` in RenderWorld.cpp ≥ 1 ✓ (1 hit, pre-landed Wave 3)
- Task 1: `Graphics::dpvsGpuTimingBegin` in RenderWorld.cpp ≥ 1 ✓ (1 hit, this plan)
- Task 1: `Graphics::dpvsGpuTimingEnd` in RenderWorld.cpp ≥ 1 ✓ (1 hit)
- Task 1: `PerformanceTimer dpvsCpuTimer` in RenderWorld.cpp ≥ 1 ✓ (1 hit)
- Task 1: `DpvsProfileInstrumentation::recordCpuQpcUs` in RenderWorld.cpp ≥ 1 ✓ (1 hit)
- Task 1: `DpvsProfileInstrumentation::recordVisibleObjectCount` in RenderWorld.cpp ≥ 1 ✓ (1 hit)
- Task 1: `DPVS::Camera::OCCLUSION_CULLING` in RenderWorld.cpp ≥ 1 ✓ (2 hits -- line 906 unchanged, exactly as required; this plan is instrumentation only, D-13 source-edit is Wave 6)
- Task 1: `THROWAWAY` in RenderWorld.cpp ≥ 3 ✓ (5 hits total: 4 new from this plan + 1 pre-landed Wave 3 on getter body)
- Task 2: `DpvsProfileInstrumentation::onFrameEnd` in Game.cpp ≥ 1 ✓ (1 hit)
- Task 2: `Graphics::present` in Game.cpp ≥ 1 ✓ (3 hits, all existing preserved)
- Task 2: `#include "clientGraphics/DpvsProfileInstrumentation.h"` in Game.cpp ≥ 1 ✓ (1 hit)
- Task 2: `DIK_F10` in CuiIoWin.cpp ≥ 1 ✓ (1 hit)
- Task 2: `DIK_F11` in CuiIoWin.cpp ≥ 1 ✓ (1 hit)
- Task 2: `DpvsProfileInstrumentation::toggleCapture` in CuiIoWin.cpp ≥ 1 ✓ (1 hit)
- Task 2: `RenderWorld::setDisableOcclusionCulling` in CuiIoWin.cpp ≥ 1 ✓ (1 hit)
- Task 2: `#ifdef _DEBUG` count in CuiIoWin.cpp increased by at least 1 ✓ (2 new occurrences: include block + intercept block)
- Task 2: `THROWAWAY` in Game.cpp ≥ 1 ✓ (2 hits)
- Task 2: `THROWAWAY` in CuiIoWin.cpp ≥ 1 ✓ (2 hits)
- Task 3: `MAKE_COMMAND (setrunlabel)` in SwgCuiCommandParserDefault.cpp ≥ 1 ✓ (1 hit)
- Task 3: `Commands::setrunlabel` in SwgCuiCommandParserDefault.cpp ≥ 2 ✓ (2 hits: cmds[] entry + performParsing branch)
- Task 3: `DpvsProfileInstrumentation::setRunLabel` in SwgCuiCommandParserDefault.cpp ≥ 1 ✓ (1 hit)
- Task 3: `#include "clientGraphics/DpvsProfileInstrumentation.h"` in SwgCuiCommandParserDefault.cpp ≥ 1 ✓ (1 hit)
- Task 3: `Unicode::wideToNarrow` in SwgCuiCommandParserDefault.cpp ≥ 1 ✓ (33 hits)
- Task 3: `THROWAWAY` in SwgCuiCommandParserDefault.cpp ≥ 2 ✓ (5 hits)
- Task 4: `10-04-build.log` exists ✓ (force-added in commit d8dbd4076)
- Task 4: SwgClient_d.exe rebuilt newer than this task's start time ✓ (mtime 00:10 > start 05:06 UTC)

**TDD gate compliance:** N/A -- this plan has `type: execute` (Wave 3 hook wiring), not `type: tdd`. All 4 source-edit tasks are `type="auto" tdd="false"`; Task 5 is `type="checkpoint:human-verify"`. No RED/GREEN/REFACTOR gates required.

## User Setup Required

**Yes, for Wave 5 (plan 10-05) capture session.** The infrastructure is now operational, but Kenny needs to drive the smoke + capture protocol:

1. Confirm SWGSource VM at 192.168.1.200 is up (per memory `project_vm_server_setup.md`).
2. Create CSV output directory: `mkdir D:/Code/swg-client-v2/stage/dpvs-profile`.
3. Confirm `D:/Code/swg-client-v2/stage/client_d.cfg` has `[ClientGraphics/Dpvs] reportInstrumentation=true` (add the section + key if missing -- this is what triggers the overlay).
4. Run the smoke + capture protocol per `tools/dpvs-profile/test-protocol.md`.

This work is owned by Wave 5 (plan 10-05). No setup required for Wave 4 itself -- the SwgClient.exe is rebuilt and ready.

## Next Phase Readiness

**Wave 5 (plan 10-05) is unblocked.** The full DPVS profiling instrumentation chain is now operational at the binary level:

- GPU timing pool (Wave 1, plan 10-02): IDirect3DQuery9 double-buffered timestamp pool in the Direct3d9 plugin.
- Engine module + CSV writer + sanitizer + overlay + ExitChain teardown (Wave 2, plan 10-03): DpvsProfileInstrumentation.
- RenderWorld brackets + Game::run hook + CuiIoWin F10/F11 + /setrunlabel (Wave 3 = this plan, plan 10-04): the four hook sites that close the link.

**Pre-conditions for Wave 5 met:**

- `SwgClient_d.exe` rebuilt at 72,552,448 bytes; all `DpvsProfileInstrumentation::*` + `Graphics::dpvsGpuTiming*` + `RenderWorld::setDisableOcclusionCulling` + `RenderWorld::getDisableOcclusionCulling` symbols resolve.
- F10 / F11 / /setrunlabel are wired and (per the prompt's `<key_context>`) "compile" -- functional verification is Wave 5's job.
- The two-gate per-frame `onFrameEnd` pattern from Wave 2 + the bracket recording in this plan together populate every CSV column the analysis.py from Wave 0 expects.

**Open items carried forward:**

- DPVS-01 requirement: STILL PARTIAL (the measurement half ships when Wave 5 capture + Wave 7 verdict lands).
- DPVS-02 requirement: NOT STARTED (conditional D-13/D-14 source edits live in plan 10-06; gated by Wave 5 verdict outcome).
- Wave 7 cleanup commit will revert all THROWAWAY-marked code from this plan: RenderWorld.cpp (4 new markers + the bracket block + the 2 includes), Game.cpp (the include + the onFrameEnd call), CuiIoWin.cpp (the _DEBUG include block + the F10/F11 intercept block), SwgCuiCommandParserDefault.cpp (the include + MAKE_COMMAND + cmds[] entry + performParsing branch). Total ~83 lines across 4 files for this plan's contribution to the cleanup diff.
- Pre-existing Koogie post-build copy MSB3073: still logged for deferred-items.md; unchanged from Wave 0/1/2/3.
- Pre-existing Direct3d9.vcxproj / Direct3d9_vsps.vcxproj MSB8012 warnings: still logged; Phase 11 candidate.

---
*Phase: 10-dpvs-culling-experiment*
*Plan: 10-04 (Wave 3 -- DpvsProfileInstrumentation hook wiring; checkpoint to Wave 5 capture session)*
*Completed: 2026-05-11*
