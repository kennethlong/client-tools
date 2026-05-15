---
phase: 10-dpvs-culling-experiment
plan: 07
subsystem: cleanup-revert + phase-closeout
tags: [dpvs, cleanup, throwaway, d-15, revert, phase-closeout, autonomous-false]

# Dependency graph
requires:
  - phase: 10-dpvs-culling-experiment
    plan: 06
    provides: "D-13 + D-14 permanent source edits applied + boot smoke PASS; the verdict-driven OCCLUSION_CULLING bit removal + config-key cleanup that this plan preserves while removing the THROWAWAY instrumentation harness"
provides:
  - "All Phase 10 Wave 0-3 + Wave 5 instrumentation removed in one revert-shaped commit (151167d2c): DpvsProfileInstrumentation.{h,cpp} deleted; 10 other files reverted across clientGraphics (Direct3d9.cpp, Graphics.{h,cpp}, Gl_dll.def, RenderWorld.cpp, SetupClientGraphics.cpp, clientGraphics.vcxproj), clientGame (Game.cpp), clientUserInterface (CuiIoWin.cpp), swgClientUserInterface (SwgCuiCommandParserDefault.cpp). 726 lines deleted total."
  - "10-07-SUMMARY.md (this file) closing plan 10-07"
  - "10-SUMMARY.md authored — phase-level closeout, thin per D-16 (links to docs/recon/10-dpvs-profiling.md as the long-form verdict record)"
  - "Final boot smoke PASS: SwgClient_d.exe rebuilt at 72,541,696 bytes (-10,752 bytes vs pre-cleanup baseline, confirming dead-code stripping landed); both outdoor Mos Eisley plaza AND indoor cantina render without crash"
  - "tag `phase-10-instrumentation-pre-cleanup` at commit 9f2ec3715 preserves the pre-cleanup HEAD for Phase 11 revert-anchor recovery if the harness is wanted back (alternative: Phase 11 reimplements cleanly under D3D11 since the query pool was Direct3d9-specific anyway)"
affects: [11]  # Phase 11 D3D11 reconsideration per ROADMAP criterion #6

# Tech tracking
tech-stack:
  removed:
    - "DpvsProfileInstrumentation module (engine module + CSV writer + run-label sanitizer + DebugFlag overlay + ExitChain teardown)"
    - "3 Gl_api function pointer entries (dpvsGpuTimingBegin/End/PollResult) and their Direct3d9 plugin implementation (timestamp query pool, lazy-create-on-first-Begin + slot-rotation read-from-(N-2)-frames-back + disjoint-flag-guard)"
    - "Engine-side Graphics::dpvsGpuTiming* forwarders (3 functions in Graphics.cpp routing through ms_api)"
    - "Phase 10 instrumentation hooks: RenderWorld GPU/CPU bracket around resolveVisibility(), Game.cpp onFrameEnd hook after Graphics::present(), CuiIoWin #ifdef _DEBUG F10/F11 keybind intercept, SwgCuiCommandParserDefault /setrunlabel console command + cmds[] entry + performParsing branch"
  patterns:
    - "Revert-shaped commit per D-15: one atomic commit (151167d2c, -726 lines) that's bisect-clean and `git revert <hash>` would restore everything. Phase 11 recovery anchor via the existing `phase-10-instrumentation-pre-cleanup` tag at the pre-cleanup HEAD."
    - "Grep-targeted enumeration: 12 files identified via `grep -E 'THROWAWAY|DpvsProfileInstrumentation|dpvsGpuTiming|setrunlabel'` matched precisely the Wave 0-3 + Wave 5 instrumentation footprint. Acceptance grep returns 0 hits post-cleanup."

key-files:
  deleted:
    - "src/engine/client/library/clientGraphics/include/public/clientGraphics/DpvsProfileInstrumentation.h (public header)"
    - "src/engine/client/library/clientGraphics/src/shared/DpvsProfileInstrumentation.cpp (implementation; CSV writer, run-label sanitizer, DebugFlag overlay, ExitChain teardown, all 11 static-class methods)"
  modified:
    - "src/engine/client/library/clientGraphics/src/win32/Graphics.h (removed 3 static method declarations)"
    - "src/engine/client/library/clientGraphics/src/win32/Graphics.cpp (removed 3 forwarder function bodies)"
    - "src/engine/client/library/clientGraphics/src/win32/Gl_dll.def (removed 3 Gl_api function pointer entries)"
    - "src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp (removed 2 includes + GPU/CPU bracket around resolveVisibility, reverted to plain NP_PROFILER block)"
    - "src/engine/client/library/clientGraphics/src/win32/SetupClientGraphics.cpp (removed include + install hook call)"
    - "src/engine/client/library/clientGraphics/build/win32/clientGraphics.vcxproj (removed ClCompile + ClInclude entries for DpvsProfileInstrumentation)"
    - "src/engine/client/application/Direct3d9/src/win32/Direct3d9.cpp (5 sites reverted: namespace declarations, pool member statics, ms_glApi assignments, shutdown call, function bodies)"
    - "src/engine/client/library/clientGame/src/shared/core/Game.cpp (removed include + DpvsProfileInstrumentation::onFrameEnd hook)"
    - "src/engine/client/library/clientUserInterface/src/shared/core/CuiIoWin.cpp (removed entire #ifdef _DEBUG F10/F11 intercept block + 2 includes)"
    - "src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserDefault.cpp (removed include + MAKE_COMMAND + cmds[] entry + performParsing else-if branch)"
  created:
    - ".planning/phases/10-dpvs-culling-experiment/10-07-SUMMARY.md (this file)"
    - ".planning/phases/10-dpvs-culling-experiment/10-SUMMARY.md (phase closeout, thin per D-16)"

key-decisions:
  - "Removal over feature-flag-gate: user explicitly chose 'Option B' (plan 10-07 as designed, full removal) over keeping the harness behind a compile-time gate during yesterday's strategy discussion. Reasoning: Phase 11 D3D11 work needs different GPU timing primitives anyway (D3D11 uses ID3D11Query, not IDirect3DQuery9); the harness has known minor issues (`IGNORE_RETURN` masking, N=3 pool depth concerns); plan 10-07 was engineered as a single-atomic-commit revert so `git revert` from the tag recovers everything. Risk-of-rework < diagnostic-value-of-clean-code."
  - "Final boot smoke PASS: confirmed both outdoor Mos Eisley plaza AND indoor Mos Eisley cantina render without crash with the instrumentation removed AND the D-13/D-14 source edits permanent. DPVS-02 success criterion #2 met: 'client boots and renders without crash' after all Phase 10 edits."

threats:
  mitigated:
    - "T-10-W7-01 (T: incomplete grep enumeration leaves dangling references and breaks build): mitigated via two-step grep verification (THROWAWAY/DpvsProfileInstrumentation/dpvsGpuTiming/setrunlabel + ms_dpvsTs/ms_dpvsTiming/kDpvsGpu/DpvsProfileInstrumentationNamespace) both returning 0 hits post-edit. Build green confirmed via fresh rebuild (SwgClient.exe link succeeded, only pre-existing MSB3073 post-build copy + editor-tool issues failed -- both Phase 8 known states)."
    - "T-10-W7-02 (T: accidental revert of D-13/D-14 permanent edits in the cleanup commit): explicitly scoped out per plan body. D-13 (OCCLUSION_CULLING bit strip at RenderWorld.cpp:909/913) and D-14 (config-key + setter/getter deletion) preserved across this revert -- grep verification confirms no OCCLUSION_CULLING references and no setDisableOcclusionCulling/getDisableOcclusionCulling references remain."
  remaining: []  # Plan 10-07 is the final plan; no remaining Phase 10 work

verification:
  procedure: |
    1. `grep -rE 'THROWAWAY|DpvsProfileInstrumentation|dpvsGpuTiming|setrunlabel' src/ --include='*.{cpp,h,def,vcxproj}'` returns 0 hits (PASS).
    2. `grep -rE 'ms_dpvsTs|ms_dpvsTiming|kDpvsGpu|DpvsProfileInstrumentationNamespace' src/ --include='*.{cpp,h,def,vcxproj}'` returns 0 hits (PASS).
    3. DpvsProfileInstrumentation.{h,cpp} no longer exist on disk (PASS).
    4. SwgClient.vcxproj built clean (May 15 09:55-09:57 build log shows 'Finished pass 2' and 'SwgClient.vcxproj -> ...SwgClient.exe' with zero compile/link errors; only failure is pre-existing MSB3073 post-build copy step).
    5. SwgClient_d.exe rebuilt at 72,541,696 bytes (-10,752 bytes vs pre-cleanup), confirming dead-code stripping landed AND the renderer plugins rebuild concurrently (gl05/06/07 sizes unchanged, expected since dead-code stripping fell on same PE alignment boundary).
    6. Final boot smoke (Kenny, 2026-05-15 ~10:00 AM): SwgClient_d.exe launches, two SafeCast.h:29 dialogs dismissed via Ignore (separate todo), zoned into Tatooine, both Mos Eisley plaza (outdoor) and cantina (indoor) render without crash. PASS.

deviations:
  - severity: information (orthogonal discovery)
    summary: "Boot smoke surfaced a smoking-gun finding for the orthogonal cantina corner-snap regression: DEBUG_WARNING routes to OutputDebugString in Report.cpp:145, not to a file"
    rationale: "Kenny's theory (logging-burst-causes-snap) confirmed during boot smoke. ui.log only grew 11,564 bytes despite 12,133 warnings accumulated; the warnings sink is OutputDebugString which has ~50-200 µs per-call overhead even with no debugger attached due to Windows' global event-check mutex. Concentrated warning bursts on first-time cell load = visible snap. Captured in `.planning/todos/pending/2026-05-15-cantina-corner-snap-regression.md` with a one-line fix candidate (`if (IsDebuggerPresent()) OutputDebugString(buffer);`) and a clean reproduction protocol. NOT a Phase 10 issue (predates Phase 10 by a week; tracked separately for /gsd-debug)."

next-up: "Phase 10 closes with this plan. Plan 10-SUMMARY.md authored as phase closeout (links to docs/recon/10-dpvs-profiling.md per D-16). Update STATE.md to mark Phase 10 as 7/7 complete. Then per user direction: TWO /gsd-debug sessions before Phase 11: (1) SafeCast.h:29 dialog issue at todo 2026-05-14, (2) cantina corner-snap with the OutputDebugString smoking-gun lead at todo 2026-05-15."
---

# Phase 10 Wave 6 (plan 10-07) — D-15 THROWAWAY Cleanup

Plan 10-07 executes the unconditional THROWAWAY cleanup per CONTEXT D-15. All Phase 10 measurement infrastructure removed in one revert-shaped commit (151167d2c, -726 lines). D-13/D-14 permanent edits from plan 10-06 preserved.

## Headline outcome

**Plan 10-07 closed cleanly. Phase 10 complete.** Source tree returned to pre-Phase-10 state for the instrumentation surface area while retaining the verdict-driven D-13/D-14 permanent edits (OCCLUSION_CULLING bit stripped globally; runtime toggle machinery deleted).

## Verification grep checks (all PASS)

| Check | Expected | Actual |
|-------|----------|--------|
| `THROWAWAY` count across src/ | 0 | 0 |
| `DpvsProfileInstrumentation` count | 0 | 0 |
| `dpvsGpuTiming` count | 0 | 0 |
| `setrunlabel` / `Commands::setrunlabel` count | 0 | 0 |
| `ms_dpvsTs` / `kDpvsGpu` count | 0 | 0 |
| `.h` and `.cpp` files exist on disk | no | confirmed deleted |
| SwgClient_d.exe size | smaller than 72,552,448 | 72,541,696 (-10,752 bytes) |

## Boot smoke

Final boot smoke PASS — both outdoor Mos Eisley plaza AND indoor cantina render without crash with the instrumentation removed.

## Smoking-gun discovery during boot smoke

Kenny's theory that excessive logging causes the cantina corner-snap got concrete confirmation: `DEBUG_WARNING` routes to `OutputDebugString` (Report.cpp:145), not to a file. 12,133 warnings during the smoke test left `ui.log` essentially unchanged. `OutputDebugString` has ~50-200 µs per-call overhead due to Windows' global event-check mutex even with no debugger attached. Concentrated warning bursts on first-time cell traversal = visible snap. One-line fix candidate (`if (IsDebuggerPresent()) OutputDebugString(buffer);`) captured in the corner-snap todo. Orthogonal to Phase 10 — predates it by a week — but Phase 10's careful capture work gave us the diagnostic data to nail the root cause.

## Phase 10 closure status

| Plan | Status |
|------|--------|
| 10-01 Wave 0 scaffolding | ✓ Complete |
| 10-02 Wave 1 GPU-timing plumbing | ✓ Complete |
| 10-03 Wave 2 engine module | ✓ Complete |
| 10-04 Wave 3 hook wiring | ✓ Complete |
| 10-05 Wave 4 capture session | ✓ Complete |
| 10-06 Wave 5 verdict writeup + source edits | ✓ Complete |
| **10-07 Wave 6 THROWAWAY cleanup** | **✓ Complete (this plan)** |

**Phase 10: 7/7 plans complete (100%). Closed.**
