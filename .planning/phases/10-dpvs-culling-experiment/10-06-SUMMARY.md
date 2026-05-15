---
phase: 10-dpvs-culling-experiment
plan: 06
subsystem: verdict-doc + source-edit (D-13 + D-14) + boot-smoke
tags: [dpvs, verdict, source-edit, occlusion-culling, config-key-cleanup, boot-smoke, autonomous-false, option-alpha]

# Dependency graph
requires:
  - phase: 10-dpvs-culling-experiment
    plan: 05
    provides: "Scene-conditional capture data (mosEisley plaza + starport + walking outdoor `remove`; cantinaInterior V1+V2 indoor `keep`) and `verdict = remove` gate line in `10-05-analysis.txt` driving the conditional Task 3 source edits"
provides:
  - "docs/recon/10-dpvs-profiling.md fully populated with Option α framing: applied `remove` globally; outdoor wins dominate; indoor regression is below human perception; Phase 11 reconsideration captured as ROADMAP success criterion #6"
  - "Source edits applied per CONTEXT D-13 (strip OCCLUSION_CULLING bit at RenderWorld.cpp:909 #ifdef _DEBUG and :913 release branch — both same value via conditional compilation; VIEWFRUSTUM_CULLING retained at both) and CONTEXT D-14 (delete `ms_disableOcclusionCulling` static + `setDisableOcclusionCulling` / `getDisableOcclusionCulling` from RenderWorld.cpp/.h and ConfigClientGraphics.cpp/.h; delete `KEY_BOOL(disableOcclusionCulling)` config-key registration); plus 4 caller-site fixes the plan didn't enumerate (GroundScene.cpp pre-Phase-10 scene-conditional toggle now redundant, DpvsProfileInstrumentation.cpp writeRow + reportOverlay neutralized via constants, CuiIoWin.cpp F11 keybind hook commented out)"
  - "10-06-SUMMARY.md (this file) closing plan 10-06"
  - "Two atomic commits: `af34b7edd` docs corrections + gate file + 10-05-analysis.txt; `18bc4fdc5` D-13 + D-14 source edits (7 files, net -20 lines)"
affects: [10-07, 11]  # 10-07 (Wave 6) THROWAWAY cleanup is now unblocked and final; Phase 11 reconsideration captured in ROADMAP criterion #6

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Architectural reality discovered during execution: yesterday's verdict-doc + ROADMAP-criterion-#6 wording assumed RenderWorld.cpp:908/911 were per-path (indoor vs outdoor) camera-setup paths. Code read disproved that — both lines are the same cullingParameters value via #ifdef _DEBUG / release branching, feeding the single ms_dpvsCamera that handles ALL rendering. No code-level scene split available without larger refactor. Option α (apply remove globally) chosen as the strategically-correct trade given outdoor magnitude × frequency vs indoor regression × frequency."
    - "Mid-execution checkpoint protocol applied correctly: discovered the wrong assumption BEFORE applying any edits; surfaced to user with three concrete options (α apply globally, β skip entirely, γ implement runtime scene split); user picked α with the caveat to defer architectural reconsideration to Phase 11 D3D11 work."
    - "Plan-vs-codebase drift handled: plan body called for edits at lines 903/906 but file had drifted to 908/911. Plan's grep-by-name acceptance criteria still worked unchanged (token references, not line references). Plus discovered 4 caller-site references the plan didn't enumerate (GroundScene.cpp:691, DpvsProfileInstrumentation.cpp:330, ConfigClientGraphics.h:52, plus the expected ones in CuiIoWin.cpp + DpvsProfileInstrumentation.cpp:394) — fixed inline as part of the same commit since deleting the setter/getter without fixing the callers would break the build."

key-files:
  created:
    - ".planning/phases/10-dpvs-culling-experiment/10-05-analysis.txt (~30 lines: verdict gate file for plan 10-06 Task 3; emits `verdict = remove` plus per-scene context; required by plan 10-06's `findstr /R /C:\"^verdict = \"` gate check)"
    - ".planning/phases/10-dpvs-culling-experiment/10-06-SUMMARY.md (this file)"
    - ".planning/todos/pending/2026-05-15-cantina-corner-snap-regression.md (orthogonal finding from boot smoke: cantina interior corner-snap exists in build-v145 and v2 lineages but NOT in `D:/Code/swg-client/build/bin/Debug/SwgClient_d.exe` v1 milestone build; regression window ~5 hours on 2026-05-08; bisection plan + suspects captured; routes to /gsd-debug)"
  modified:
    - "docs/recon/10-dpvs-profiling.md (+183 lines, -60 lines: populated from yesterday's skeleton with Option α framing; corrected the wrong per-path assumption; documented gpu_us=0 as structural; documented all scene caveats; added unified-mechanism rationale)"
    - ".planning/ROADMAP.md (+1 line, -1 line: corrected Phase 11 success criterion #6 wording to reflect Option α and three reconsideration options)"
    - "src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp (+6 lines, -16 lines: D-13 stripped OCCLUSION_CULLING bit + Option α comment markers; D-14 deleted ms_disableOcclusionCulling static + setter/getter bodies)"
    - "src/engine/client/library/clientGraphics/src/shared/RenderWorld.h (+0 lines, -4 lines: D-14 deleted setter/getter declarations)"
    - "src/engine/client/library/clientGraphics/src/shared/ConfigClientGraphics.cpp (+0 lines, -8 lines: D-14 deleted ms_disableOcclusionCulling static, KEY_BOOL registration, getter body)"
    - "src/engine/client/library/clientGraphics/src/shared/ConfigClientGraphics.h (+0 lines, -1 lines: D-14 deleted getter declaration)"
    - "src/engine/client/library/clientUserInterface/src/shared/core/CuiIoWin.cpp (+8 lines, -5 lines: F11 keybind hook neutralized by comment-out — Wave 7 deletes the entire #ifdef _DEBUG block)"
    - "src/engine/client/library/clientGraphics/src/shared/DpvsProfileInstrumentation.cpp (+5 lines, -2 lines: writeRow dpvs_occlusion_flag column hardcoded to 1; reportOverlay 'OFF/ON' replaced with 'removed' literal — Wave 7 deletes this file entirely)"
    - "src/engine/client/library/clientGame/src/shared/scene/GroundScene.cpp (+4 lines, -1 lines: pre-Phase-10 scene-conditional setDisableOcclusionCulling call removed with explanatory comment — redundant under Option α)"
  not-tracked:
    - "D:/Code/swg-client-v2/stage/SwgClient_d.exe (rebuilt 2026-05-15 09:02, 72,552,448 bytes — same byte count as Wave 3 baseline; small edits + dead-code stripping fits within 512-byte alignment boundary)"

key-decisions:
  - "Option α (apply `remove` globally) over β (skip entirely) or γ (runtime scene split): outdoor magnitude × frequency wins over indoor regression × frequency. Indoor 0.66 ms / 2.2% loss is below human perception; outdoor 0.94-2.13 ms / 3.3-9.1% wins are dominant gameplay time. Decision deliberately defers architectural reconsideration to Phase 11 D3D11 work per ROADMAP criterion #6 — D3D11's cheaper per-draw-call cost may flip the indoor verdict to `remove` too (making Option α a non-issue) or may flip outdoor to net-positive (justifying revert to `keep` or implementing γ)."
  - "Mid-execution code review caught the wrong assumption about RenderWorld.cpp:908 (outdoor) vs :911 (indoor) being separate paths BEFORE applying any source edits. Surfaced the discrepancy as a clean three-option checkpoint with user; received explicit Option α direction. This is the canonical 'discover something during execution' deviation handling per GSD conventions."
  - "Enumerated 4 caller-site references the plan didn't anticipate (GroundScene.cpp setDisableOcclusionCulling caller, DpvsProfileInstrumentation.cpp:330 writeRow getter consumer, ConfigClientGraphics.h declaration leftover, plus the planned CuiIoWin.cpp F11 hook + DpvsProfileInstrumentation.cpp reportOverlay). Fixed all inline in the same atomic commit since deleting setter/getter without fixing callers would break the build."

threats:
  mitigated:
    - "T-10-W6-01 (T: applying D-13/D-14 against verdict=keep): mitigated via the 10-05-analysis.txt gate file containing `verdict = remove`. Task 3 conditional gate verified verdict-match before applying edits."
    - "T-10-W6-02 (T: accidental deletion of VIEWFRUSTUM bit): mitigated. Acceptance grep `grep -c 'DPVS::Camera::VIEWFRUSTUM_CULLING' RenderWorld.cpp` returns 2 (both branches retained). The view-frustum culling bit is explicitly retained at both `#ifdef _DEBUG` and release variants."
  remaining:
    - "T-10-CORNER-SNAP (orthogonal regression discovered during boot smoke): cantina interior corner-snap exists in build-v145 and v2 lineages but not in v1 milestone Debug build (`D:/Code/swg-client/build/bin/Debug/SwgClient_d.exe`). Regression window ~5 hours on 2026-05-08. Tracked separately in `.planning/todos/pending/2026-05-15-cantina-corner-snap-regression.md`. Not blocking Phase 10."

verification:
  procedure: |
    1. `grep -c 'DPVS::Camera::OCCLUSION_CULLING' src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp` returns 0 (PASS).
    2. `grep -c 'DPVS::Camera::VIEWFRUSTUM_CULLING' src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp` returns 2 (PASS — both `#ifdef _DEBUG` and release branches retain the bit).
    3. `grep -c 'ms_disableOcclusionCulling' src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp` returns 0 (PASS — static + setter + getter bodies all gone).
    4. `grep -c 'setDisableOcclusionCulling\|getDisableOcclusionCulling' src/engine/client/library/clientGraphics/src/shared/RenderWorld.h` returns 0 (PASS — declarations gone).
    5. `grep -c 'disableOcclusionCulling' src/engine/client/library/clientGraphics/src/shared/ConfigClientGraphics.cpp` returns 0 (PASS — static + KEY_BOOL + getter all gone).
    6. Build: SwgClient.vcxproj succeeded (May 15 09:02 build log shows "Finished pass 2" and "SwgClient.vcxproj -> ...SwgClient.exe" cleanly; pre-existing MSB3073 post-build copy is the only failure, identical to all prior Phase 10 wave builds).
    7. Boot smoke: launched `D:/Code/swg-client-v2/stage/SwgClient_d.exe`, Ignore × 2 SafeCast asserts, login as Little Bigman, zoned into Tatooine. Indoor cantina renders without crash; outdoor Mos Eisley plaza renders without crash. No FATAL output. F10 capture toggle still works (block intact); F11 does nothing (block commented out per plan). PASS — meets DPVS-02 acceptance criterion #5 ("client boots and renders without crash").
    8. Boot smoke observation: cantina interior corner-snap reproduces but is orthogonal pre-existing regression (May 8 2026, predates Phase 10 by a week; same lineage as build-v145; NOT in v1 milestone Debug). Captured for /gsd-debug separately.

deviations:
  - severity: minor (caught before applying any edits)
    summary: "Yesterday's verdict-doc and ROADMAP-criterion-#6 wording assumed RenderWorld.cpp:908 was an outdoor camera-setup path and :911 was an indoor camera-setup path"
    rationale: "Code review during plan 10-06 execution revealed both lines are the SAME cullingParameters value via #ifdef _DEBUG / release conditional compilation, feeding the single ms_dpvsCamera that handles ALL rendering. There is no code-level per-path split at this site. Caught BEFORE making any source edits. Surfaced the discovery as a three-option checkpoint; user chose Option α (apply `remove` globally) with deliberate deferral of architectural reconsideration to Phase 11. Yesterday's doc + ROADMAP were corrected in commit `af34b7edd` before the source edits in `18bc4fdc5`."
  - severity: minor (additional caller sites)
    summary: "Plan body enumerated 6 edit sites; execution found 4 additional caller-side references that would break the build without inline fixes"
    rationale: "Plan listed: RenderWorld.cpp:906 (D-13), :903 (D-13 mirror), :1151 setter, getter body, .h:104 declaration, ConfigClientGraphics.cpp:38/101/245, plus CuiIoWin F11 + DpvsProfileInstrumentation overlay neutralizations. Execution also found: GroundScene.cpp:691 (pre-Phase-10 scene-conditional caller of setDisableOcclusionCulling), DpvsProfileInstrumentation.cpp:330 (writeRow's dpvs_occlusion_flag column reads getter), ConfigClientGraphics.h:52 (getter declaration the plan said 'search by name' for and didn't find pre-execution). All fixed inline in the same atomic commit (`18bc4fdc5`) since deleting the setter/getter without fixing callers would break the build."
  - severity: information (orthogonal finding)
    summary: "Boot smoke surfaced a pre-existing cantina interior corner-snap regression"
    rationale: "While verifying indoor cantina rendering, observed camera/character snap when turning corners between cantina sub-areas. Cross-tested against `D:/Code/swg-client/build/bin/Debug/SwgClient_d.exe` (v1 milestone, May 8 16:44) and `D:/Code/swg-client/build-v145/bin/Debug/SwgClient_d.exe` (build-v145, May 8 21:51): v1 milestone has NO corner snap, build-v145 HAS the corner snap. Regression introduced between these two binaries in a ~5-hour window on 2026-05-08. Not caused by Phase 10 work. Captured for /gsd-debug in `.planning/todos/pending/2026-05-15-cantina-corner-snap-regression.md`. Does not block plan 10-06 closure — DPVS-02 boot smoke acceptance criterion is `client renders Mos Eisley plaza without crash`, met for both indoor and outdoor."

next-up: "Plan 10-07 (Wave 6): unconditional THROWAWAY cleanup commit — revert all Phase 10 instrumentation (delete DpvsProfileInstrumentation.{h,cpp}, revert RenderWorld GPU/CPU bracket and ms_dpvsQueryProfilerBlock changes, revert Game.cpp onFrameEnd hook, revert CuiIoWin F10/F11 #ifdef _DEBUG intercept entirely (currently only F11 commented out), revert /setrunlabel /setrunlabel console command in SwgCuiCommandParserDefault, revert Direct3d9.cpp dpvsGpuTiming pool + Gl_api function pointers). One revert-shaped commit per D-15. Then 10-SUMMARY.md (phase closeout) + final boot smoke. Tag `phase-10-instrumentation-pre-cleanup` at commit 9f2ec3715 already preserves the pre-cleanup HEAD for Phase 11 revert-anchor recovery if needed."
---

# Phase 10 Wave 5 (plan 10-06) — DPVS Verdict Writeup + Conditional Source Edits

Plan 10-06 executes the verdict-driven cleanup half of Phase 10. Part A populated the public `docs/recon/10-dpvs-profiling.md` (begun yesterday during the cantina V2 follow-up session). Part B applied CONTEXT D-13 (strip `OCCLUSION_CULLING` bit) and D-14 (delete `disableOcclusionCulling` runtime toggle machinery) per the Option α decision — apply `remove` globally rather than attempting a code-level scene-conditional split that the architecture doesn't natively support.

## Headline outcome

**Plan 10-06 closed cleanly.** All 5 plan acceptance grep checks pass. Boot smoke met "client renders without crash" for both indoor cantina AND outdoor Mos Eisley plaza. DPVS occlusion culling permanently disabled at the source level per the Phase 10 verdict.

## What changed in source (D-13 + D-14)

**D-13 — strip `OCCLUSION_CULLING` bit:**
- `RenderWorld.cpp:909` (#ifdef _DEBUG): `(ms_disableViewFrustumCulling ? 0 : DPVS::Camera::VIEWFRUSTUM_CULLING)` — bit dropped, view-frustum-culling retained with debug-flag respect.
- `RenderWorld.cpp:913` (release): `DPVS::Camera::VIEWFRUSTUM_CULLING` — bit dropped, view-frustum-culling unconditional.

**D-14 — delete runtime toggle machinery:**
- `RenderWorld.cpp`: deleted `ms_disableOcclusionCulling` static, `setDisableOcclusionCulling()` body, `getDisableOcclusionCulling()` body.
- `RenderWorld.h`: deleted setter + getter declarations.
- `ConfigClientGraphics.cpp`: deleted `ms_disableOcclusionCulling` static, `KEY_BOOL(disableOcclusionCulling, false)` registration, `getDisableOcclusionCulling()` body.
- `ConfigClientGraphics.h`: deleted getter declaration.

**Caller fixes (4 sites the plan body didn't explicitly enumerate but the build requires):**
- `GroundScene.cpp:691`: pre-Phase-10 scene-conditional `setDisableOcclusionCulling(strstr(terrainFilename, "space_") != 0)` caller — interesting historical artifact (the engine WAS doing space-vs-ground scene-conditional DPVS toggling) now redundant under Option α. Removed with explanatory comment.
- `DpvsProfileInstrumentation.cpp:330`: `writeRow` dpvs_occlusion_flag column hardcoded to literal `1`. Wave 7 deletes this file.
- `DpvsProfileInstrumentation.cpp:394`: `reportOverlay` "OFF/ON" replaced with literal "removed". Wave 7 deletes this file.
- `CuiIoWin.cpp:978-983`: F11 keybind hook block commented out. Wave 7 deletes the entire `#ifdef _DEBUG` intercept.

## Verdict doc state (`docs/recon/10-dpvs-profiling.md`)

Fully populated with the scene-conditional analysis from plan 10-05's data + the Option α decision + the unified-mechanism rationale + the Phase 11 revisit note. Now the canonical published home of Phase 10's findings.

## Boot smoke (Task 4)

PASSED. Both indoor cantina and outdoor Mos Eisley plaza render without crash. Pre-existing SafeCast.h:29 dialogs (Ignore × 2) tracked separately.

## Orthogonal finding

Cantina interior corner-snap pre-existing regression (May 8 2026, predates Phase 10 by a week). Tracked in `.planning/todos/pending/2026-05-15-cantina-corner-snap-regression.md`. Bisection plan + suspect candidates captured. Not blocking Phase 10.

## Phase 10 status after this plan

| Plan | Status |
|------|--------|
| 10-01 Wave 0 scaffolding | ✓ Complete |
| 10-02 Wave 1 GPU-timing plumbing | ✓ Complete |
| 10-03 Wave 2 engine module | ✓ Complete |
| 10-04 Wave 3 hook wiring | ✓ Complete |
| 10-05 Wave 4 capture session | ✓ Complete (verdict = scene-conditional, Option α chosen) |
| **10-06 Wave 5 verdict writeup + source edits** | **✓ Complete (this plan) — D-13/D-14 applied; boot smoke PASS** |
| 10-07 Wave 6 THROWAWAY cleanup revert | ○ Not started — final plan |

Phase 10 is **6 of 7 plans complete (86%)**. Plan 10-07 is the mechanical cleanup that ends the phase.
