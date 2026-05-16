---
phase: 11-d3d11-renderer-plugin
plan: 01
subsystem: renderer
tags: [d3d11, d3d9, ffp, spike, throwaway, descope, build-system]

# Dependency graph
requires:
  - phase: 10-dpvs-culling-experiment
    provides: THROWAWAY-pattern precedent (plan 10-07 single revert-shaped commit), DpvsProfileInstrumentation CSV-path convention (exe-relative)
  - phase: 09-stlport-msvc-stl
    provides: Koogie-merge v2 tree at koogie-msvc-cpp20-base with IFF compat-guard ported (stable Tatooine zone-in baseline)
provides:
  - D-04a verdict resolved: DESCOPE the FFP pixel-shader generator
  - Plan 11-05 (Wave 5) input directive: OMIT Direct3d11_FfpGenerator.{h,cpp} from Direct3d11.vcxproj
  - Plan 11-02 (Wave 2) unblocked: conditional FFP-generator decision removed from scaffold scope
  - Phase 11 bonus deliverable: Koogie post-build copy steps now auto-stage all D3D9 variants + SwgClient binaries to stage/
affects: [11-02, 11-05, all-future-Phase-11-plans]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - THROWAWAY-instrumentation precedent reaffirmed (35 markers across 3 files, single revert commit; mirrors Phase 10 plan 10-07's 726-line revert)
    - CSV-path convention enforced (exe-relative, not project-relative; matches Phase 10 DpvsProfileInstrumentation)
    - Build-system auto-stage pattern fixed (post-build cp to stage/ in SwgClient.vcxproj + Direct3d9*.vcxproj — eliminates manual cp after every rebuild going forward)

key-files:
  created:
    - .planning/phases/11-d3d11-renderer-plugin/11-01-ffp-spike-finding.md (Phase A + Phase B + D-04a verdict)
    - .planning/phases/11-d3d11-renderer-plugin/11-01-SUMMARY.md (this file)
    - stage/dpvs-profile/ffp-spike.csv (Phase B capture artifact, gitignored under stage/)
  modified:
    - src/engine/client/application/Direct3d9/src/win32/Direct3d9.cpp (instrumented in 0293ef310, reverted in 82f068a4a)
    - src/engine/client/application/Direct3d9/src/win32/Direct3d9_StateCache.cpp (instrumented in 0293ef310, reverted in 82f068a4a)
    - src/engine/client/application/Direct3d9/src/win32/Direct3d9_ShaderImplementationData.cpp (instrumented in 0293ef310, reverted in 82f068a4a)
    - src/engine/client/application/Direct3d9/build/win32/Direct3d9.vcxproj (post-build cp fix in 266e173b3 — net add, not reverted)
    - src/engine/client/application/Direct3d9/build/win32/Direct3d9_ffp.vcxproj (post-build cp fix in 266e173b3 — net add)
    - src/engine/client/application/Direct3d9/build/win32/Direct3d9_vsps.vcxproj (post-build cp fix in 266e173b3 — net add)
    - src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj (post-build cp fix in 266e173b3 — net add)

key-decisions:
  - "DESCOPE Direct3d11_FfpGenerator per D-04a verdict — Phase A non-empty + Phase B empty"
  - "Build-system fix to post-build copy steps adopted as a Phase 11 bonus deliverable — benefits the whole project going forward, not just Plan 01"

patterns-established:
  - "THROWAWAY-instrumentation single revert-shaped commit (35 markers, 3 files, -42 lines) — second instance after Phase 10 plan 10-07; pattern is now stable across two phases"
  - "Exe-relative CSV-path convention for any future stage/-based instrumentation (matches Phase 10 DpvsProfileInstrumentation)"
  - "vcxproj post-build copy auto-stages debug binaries to stage/ — eliminates manual cp workflow; first applied to D3D9 variants + SwgClient, extensible to future D3D11 variants"

requirements-completed: [D3D11-04]

# Metrics
duration: 2 days elapsed (2026-05-15 Phase A + 2026-05-16 Phase B finalization); ~3 hours of active executor + Kenny play time across two sessions
completed: 2026-05-16
---

# Phase 11 Plan 01: FFP Spike — D-04 Resolution Summary

**Two-phase spike (static + runtime) decisively resolved D-04a as DESCOPE: 49 D3D9 #ifdef FFP regions + 11 IFF instantiation sites are infrastructure-hot but zero FFP setter sites fire post-init across ≥10 min of Tatooine outdoor + Mos Eisley cantina interior gameplay on the D3D9 baseline. Plan 11-05 (Wave 5) MUST omit Direct3d11_FfpGenerator.{h,cpp}. Bonus deliverable: build-system fix that auto-stages debug binaries to stage/.**

## Performance

- **Duration:** ~3 hours active time across 2 sessions (Phase A static analysis 2026-05-15; Phase B Kenny play + finalization 2026-05-16)
- **Started:** 2026-05-15 (Phase A — static-analysis finding committed at 69af9adb6)
- **Completed:** 2026-05-16T19:50:00Z (revert commit 82f068a4a + this SUMMARY)
- **Tasks:** 4 (Task 1 Phase A + Task 2 decision checkpoint phase-b-execute + Task 3 Phase B instrumentation/capture/finding + Task 4 revert + verdict)
- **Files modified:** 7 (3 D3D9 source files instrumented + reverted; 4 vcxproj files for bonus build-system fix; 1 finding doc + 1 CSV artifact added)
- **Commits:** 6 on plan 11-01 + this plan-close commit = 7 total

## Accomplishments

1. **D-04a verdict resolved DESCOPE.** Plan 11-05 (Wave 5 — Shader layer) source list MUST OMIT `Direct3d11_FfpGenerator.{h,cpp}`. The conditional FFP-generator decision that has gated Phase 11 plan scope since CONTEXT capture (2026-05-15) is now closed.
2. **Combined static + runtime evidence captured.** Phase A: 11 asset-driven `new FixedFunctionPipeline(iff, ...)` sites in `ShaderImplementation.cpp` + 49 `#ifdef FFP` regions in 8 D3D9-plugin source files (engine + plugin are fully primed). Phase B: zero post-init FFP setter activations across ≥10 min of Tatooine + cantina interior play on `gl05_d.dll` baseline; 16 init-time defaults are device-default writes during `StateCache::initialize()` and are not evidence of FFP rendering.
3. **THROWAWAY pattern reaffirmed.** Single revert-shaped commit `82f068a4a` removes all 35 `// THROWAWAY D-04` markers across 3 files in -42 lines. Second clean precedent after Phase 10 plan 10-07's 726-line revert; pattern is stable.
4. **D-05 protection upheld.** All three D3D9 plugin variants (`gl05_d.dll`, `gl06_d.dll`, `gl07_d.dll`) built clean both BEFORE Phase B instrumentation landed AND AFTER the revert. The D3D9 fallback rendering path is unaffected by this spike.
5. **Bonus deliverable — build-system fix.** The Koogie post-build copy steps in `SwgClient.vcxproj` + `Direct3d9*.vcxproj` (commit `266e173b3`) now auto-stage debug binaries to `stage/`. Eliminates the manual `cp` workflow that contributed to the mid-session stale-exe trap. This fix benefits every future Phase 11 plan that rebuilds plugin or client binaries.

## Task Commits

| # | Commit | Task | Type | Net |
|---|---|---|---|---|
| 1 | `69af9adb6` | Task 1 — Phase A static-analysis finding | spec | Finding doc Phase A section authored; 0 src/ edits |
| 2 | `0293ef310` | Task 3 ADD — THROWAWAY D-04 instrumentation at 8 FFP setter sites + helper in Direct3d9.cpp + frame counter in `Direct3d9Namespace::endScene` | spec | +42 lines across 3 D3D9 files |
| 3 | `6c11640bc` | Mid-session path fix — CSV path was project-relative, changed to exe-relative | spec (deviation Rule 1) | 1-line path-string fix in Direct3d9.cpp |
| 4 | `266e173b3` | Mid-session build-system fix — Koogie post-build cp steps now land binaries in stage/ | build (deviation Rule 3 → bonus deliverable) | +~20 lines across 4 vcxproj files |
| 5 | `200cc7694` | Task 3 close + Task 4b verdict — Phase B Findings + D-04a DESCOPE verdict in finding doc | spec | Finding doc Phase B section + D-04a verdict authored |
| 6 | `82f068a4a` | Task 4a — Single revert-shaped commit removes all 35 THROWAWAY markers across 3 D3D9 files | spec | -42 lines (mirrors Task 3 ADD; net-zero source delta from pre-spike baseline) |

**Plan close commit:** (this commit) — adds 11-01-SUMMARY.md + STATE.md + ROADMAP.md + REQUIREMENTS.md updates.

## Files Created/Modified

### Created
- `.planning/phases/11-d3d11-renderer-plugin/11-01-ffp-spike-finding.md` — Phase A + Phase B + D-04a verdict doc (~210 lines). Cited by Plan 11-05 and Plan 11-02.
- `.planning/phases/11-d3d11-renderer-plugin/11-01-SUMMARY.md` — this file.
- `stage/dpvs-profile/ffp-spike.csv` — 17-row Phase B capture artifact (gitignored under `stage/`). Retained as evidence; not committed.

### Modified (net-zero from pre-spike baseline after revert)
- `src/engine/client/application/Direct3d9/src/win32/Direct3d9.cpp` — instrumented in `0293ef310`, reverted in `82f068a4a`. Source state matches `HEAD~7` (pre-Phase 11 baseline).
- `src/engine/client/application/Direct3d9/src/win32/Direct3d9_StateCache.cpp` — instrumented in `0293ef310`, reverted in `82f068a4a`. Source state matches `HEAD~7`.
- `src/engine/client/application/Direct3d9/src/win32/Direct3d9_ShaderImplementationData.cpp` — instrumented in `0293ef310`, reverted in `82f068a4a`. Source state matches `HEAD~7`.

### Modified (net additions — bonus build-system fix in `266e173b3`)
- `src/engine/client/application/Direct3d9/build/win32/Direct3d9.vcxproj` — post-build copy to `stage/gl05_d.dll` + `.pdb`.
- `src/engine/client/application/Direct3d9/build/win32/Direct3d9_ffp.vcxproj` — post-build copy to `stage/gl06_d.dll` + `.pdb`.
- `src/engine/client/application/Direct3d9/build/win32/Direct3d9_vsps.vcxproj` — post-build copy to `stage/gl07_d.dll` + `.pdb`.
- `src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj` — post-build copy to `stage/SwgClient_d.exe` + `.pdb`.

## Phase A Grep Counts Summary

| Surface | Pattern | Result | Bearing |
|---|---|---:|---|
| Engine FFP class hierarchy | `ShaderImplementationPassFixedFunctionPipeline` declaration | 1 site (`ShaderImplementation.h:310`) | Class is present |
| Engine FFP class hierarchy | `new FixedFunctionPipeline(iff, ...)` asset-driven instantiation | **11 sites** in `ShaderImplementation.cpp` | Infrastructure is hot |
| Engine FFP class hierarchy | IFF loader entry points for FFP construction | 4 in `ShaderImplementation.cpp:1984,2012,2026,2042` | Asset path is wired |
| Engine FFP class hierarchy | Total citations in clientGraphics | ≥35 distinct references | Pervasive |
| D3D9 plugin FFP code paths | `#ifdef FFP` / `#if defined(FFP)` regions | **49 across 8 files** | Pervasive |
| D3D9 plugin FFP code paths | Per-DLL preprocessor variants | gl05 (FFP+VSPS), gl06 (FFP-only), gl07 (VSPS-only) — all built today | Three live variants |
| Asset-side on-disk shader sources | `*.shader`, `*.mat`, `*.tdef`, FFP-bearing `.iff` | **0 files on disk** | All shaders ship inside binary `.iff` TRE archives via retail SWG install |

**Phase A verdict:** non-empty. Phase B required (per D-04 design + D-04a recommendation).

## Phase B Activation Count Summary

| Site | Rows | Frame | Interpretation |
|---|---:|---|---|
| `StateCache_init` (device defaults at `Direct3d9_StateCache.cpp:164/168`) | **16** | 0 | Pre-frame device-init `D3DTOP_DISABLE` writes; expected; not FFP rendering |
| `StateCache_restore` (device-lost restore at `Direct3d9_StateCache.cpp:397/401`) | **0** | n/a | No device-lost events in ≥10 min play; expected on Win11/WDDM |
| `ImplStage_build` (asset-driven FFP construct at `Direct3d9_ShaderImplementationData.cpp:152/158`) | **0** | n/a | **Definitive: no asset in either target scene drove an FFP-pass IFF chunk** |
| `ImplStage_cascade` (cascade-terminate at `Direct3d9_ShaderImplementationData.cpp:389/391`) | **0** | n/a | Consistent with `ImplStage_build = 0` |
| **Total post-init FFP activations across both scenes** | **0** | — | DESCOPE per D-04a |

Capture session: ≥5 min Tatooine outdoor + ≥5 min Mos Eisley cantina interior (SPEC R5 target scenes), D3D9 baseline (`rasterMajor=5`, `gl05_d.dll`), clean exit to flush. Capture data: `stage/dpvs-profile/ffp-spike.csv` (17 rows = 1 header + 16 data).

## Input Directive for Plan 11-05 (Wave 5 — Shader layer)

**`Direct3d11.vcxproj` MUST OMIT `Direct3d11_FfpGenerator.{h,cpp}` from its source list.** No FFP-generator class is authored. SPEC R4's "FFP pixel shader generator covers MODULATE / ADD / SELECTARG1" acceptance is satisfied by descope evidence per CONTEXT D-04a. The existing D3D9 FFP variant `gl06_d.dll` stays on disk for D3D9-side fallback; D3D11 does not mirror it.

Plan 11-05 reduced scope:
- `D3DCompile` + `vs_5_0`/`ps_5_0` recompile pipeline (D-03)
- `.cso` disk cache plumbing (D-03)
- Constant-buffer wrapper migration (replaces `SetVertexShaderConstantF` / `SetPixelShaderConstantF`)
- HLSL SM2.0 → SM5.0 mechanical recompile of the existing VSPS shader set (D3D11-03)

## THROWAWAY Revert Confirmation

| Check | Result |
|---|---|
| `grep "// THROWAWAY D-04\|D04Spike"` in `src/engine/client/application/Direct3d9/` | **0 matches** (verified post-revert) |
| `MSBuild -t:Direct3d9 ... -p:PlatformToolset=v145` EXIT | **0** (gl05_d.dll rebuilt and staged) |
| `MSBuild -t:Direct3d9_ffp ... -p:PlatformToolset=v145` EXIT | **0** (gl06_d.dll rebuilt and staged) |
| `MSBuild -t:Direct3d9_vsps ... -p:PlatformToolset=v145` EXIT | **0** (gl07_d.dll rebuilt and staged) |
| Source delta from pre-spike baseline | **net zero** (Task 3 ADD `+42` lines reversed by Task 4 REVERT `-42` lines) |

## Decisions Made

1. **DESCOPE Direct3d11_FfpGenerator** per D-04a verdict rubric. ZERO post-init FFP activations across both SPEC R5 target scenes on the D3D9 baseline → Phase B empty → DESCOPE per D-04a's combined-evidence branch. Both interpretations of the frame=0 anomaly (endScene increment fired with no FFP firing, OR increment site is dead code) support the same conclusion.
2. **Build-system fix adopted as bonus deliverable.** Encountered the broken Koogie post-build copy as a Rule 3 blocker during the mid-session path-fix relaunch (manual `cp` workflow was unreliable and created the stale-exe trap). Fix landed in `266e173b3` and benefits the entire project going forward — not just Plan 01. Net add, not reverted with the rest of the spike.
3. **Stale-exe trap surfaced as Phase 11 lesson.** First Kenny play session ran against a stale `SwgClient_d.exe` (predated SafeCast fix `73e29eee7` AND the path fix) and produced zero CSV output — superficially resembling a clean "empty Phase B" signal but actually three bugs stacked (stale exe + wrong CSV path + missing post-build cp). The second session against the corrected pipeline produced the 17-row CSV. Documented in the finding doc's "Cross-reference to capture chronology" section as a lessons-learned artifact for future Phase 11 instrumentation work.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] CSV path was project-relative, not exe-relative**
- **Found during:** Task 3 first Kenny play session attempt
- **Issue:** Initial THROWAWAY helper opened `stage/dpvs-profile/ffp-spike.csv` (project-relative). When `SwgClient_d.exe` launches from `stage/`, this resolves to `stage/stage/dpvs-profile/ffp-spike.csv` — wrong location, zero CSV output.
- **Fix:** Changed path to `dpvs-profile/ffp-spike.csv` (exe-relative; resolves to `stage/dpvs-profile/ffp-spike.csv` correctly). Matches Phase 10 DpvsProfileInstrumentation convention.
- **Files modified:** `src/engine/client/application/Direct3d9/src/win32/Direct3d9.cpp` (1-line string fix)
- **Verification:** Second Kenny play session produced 17-row CSV at the correct path
- **Committed in:** `6c11640bc`

**2. [Rule 3 - Blocking → Bonus deliverable] Koogie post-build copy steps did not stage debug binaries**
- **Found during:** Task 3 after the path-fix relaunch attempt
- **Issue:** After the `6c11640bc` rebuild, `stage/SwgClient_d.exe` and `stage/gl05_d.dll` did not reflect the rebuild. Manual `cp` workflow was unreliable and led to repeated stale-exe confusion. Without auto-staging, every rebuild in this plan required manual intervention — fix-attempt budget would have exhausted before producing a clean Phase B signal.
- **Fix:** Added post-build `copy` commands to `SwgClient.vcxproj` + `Direct3d9.vcxproj` + `Direct3d9_ffp.vcxproj` + `Direct3d9_vsps.vcxproj` that copy each variant's `.dll`/`.exe` + `.pdb` from `compile/win32/<target>/Debug/` to `stage/`. Net add (not reverted) — this benefits every future Phase 11 plan and the entire project going forward.
- **Files modified:** 4 vcxproj files (~+5 lines each post-build copy block)
- **Verification:** Task 4 D-05 verification confirmed `stage/gl05_d.dll` + `stage/gl06_d.dll` + `stage/gl07_d.dll` updated with fresh 2026-05-16 timestamps automatically after each MSBuild target ran
- **Committed in:** `266e173b3` (marked `build(11):` not `spec(11-01):` since it benefits the whole project, not just Plan 01)

---

**Total deviations:** 2 auto-fixed (1 Rule 1 bug, 1 Rule 3 blocking → bonus deliverable).
**Impact on plan:** Both auto-fixes were necessary to produce a definitive Phase B signal. The build-system fix has positive scope (benefits all future Phase 11 plans + the entire project) and is justified as a bonus deliverable rather than a deviation to be reverted. No scope creep on the verdict-driving work.

## Issues Encountered

- **Stale-exe trap (first Kenny play session).** Kenny's first play session against `SwgClient_d.exe` ran a binary that predated both the SafeCast.h:29 fix (`73e29eee7`) and the CSV path fix (`6c11640bc`). The wrong-path CSV writer silently failed at the wrong location, producing zero output. This initially looked like a clean "empty Phase B" signal but was actually three bugs stacked. Surfaced by checking the binary's mtime against the source-edit chronology. Resolved by the build-system fix (`266e173b3`) which made future rebuilds auto-stage, and the second play session produced the definitive 17-row capture. Documented in the finding doc as a lessons-learned artifact.
- **No FFP-rendering signal vs. unknown bug ambiguity.** With `frame=0` across all 16 captured rows, two non-exclusive explanations exist for the captured pattern: (a) FFP truly doesn't fire post-init, so the increment in `Direct3d9Namespace::endScene` fires thousands of times with no FFP setter calls to drive log emissions, OR (b) the increment site itself is in dead code (a different endScene path or modern Koogie pre-Present flow bypasses it). Both interpretations support DESCOPE — under (a), zero FFP activation; under (b), still zero FFP setter site activations (the increment site doesn't matter for the verdict, only that the FFP setter sites didn't fire). Documented honestly in the finding doc.

## User Setup Required

None — Phase 11 Plan 01 is a spike + verdict; no external services involved. The CSV artifact at `stage/dpvs-profile/ffp-spike.csv` is gitignored as part of `stage/` and retained locally for traceability.

## Next Phase Readiness

- **Plan 11-02 (Wave 2 — plugin scaffold)** is UNBLOCKED. The conditional FFP-generator decision is resolved; the scaffold can author `Direct3d11/` source tree + `Direct3d11.vcxproj` knowing the source list does NOT include `Direct3d11_FfpGenerator.{h,cpp}`.
- **Plan 11-05 (Wave 5 — Shader layer)** has reduced scope: only `D3DCompile` + `.cso` cache + cbuffer migration + SM5.0 recompile; no FFP-generator class authoring.
- **D-05 maintained.** All three D3D9 variants build clean at this commit; D3D9 fallback rendering is unaffected.
- **Build-system bonus.** Future Phase 11 plans that rebuild D3D9 plugin variants or SwgClient will auto-stage binaries to `stage/` — eliminates the manual-cp workflow.

### Carry-forward observations (not blockers)

- `Direct3d9.vcxproj` MSB8012 TargetName warnings (pre-existing; surfaced again during build verification): noted at `D:\Code\swg-client-v2\src\engine\client\application\Direct3d9\build\win32\Direct3d9.vcxproj` lines 1415/1417. Out of scope for Plan 01 (Wave 0 deferred-items precedent). Future Phase 11 candidate.
- `C4456` "declaration hides previous local" warnings in `Direct3d9.cpp:2519` and `Direct3d9_VertexBufferDescriptorMap.cpp:140` — both pre-existing Koogie code; out of scope.

---

*Phase: 11-d3d11-renderer-plugin*
*Plan: 01 — FFP Spike (D-04 resolution)*
*Completed: 2026-05-16*

## Self-Check: PASSED

- Finding doc exists: `.planning/phases/11-d3d11-renderer-plugin/11-01-ffp-spike-finding.md` — FOUND
- CSV artifact exists: `stage/dpvs-profile/ffp-spike.csv` (17 rows) — FOUND
- THROWAWAY revert leaves zero markers: grep `// THROWAWAY D-04|D04Spike` in `src/engine/client/application/Direct3d9/` → 0 matches — VERIFIED
- All 3 D3D9 variants build clean post-revert: `gl05_d.dll` + `gl06_d.dll` + `gl07_d.dll` all EXIT=0 + staged with fresh timestamps — VERIFIED
- Commit chain complete: `69af9adb6`, `0293ef310`, `6c11640bc`, `266e173b3`, `200cc7694`, `82f068a4a` all present in `git log` — VERIFIED
- D-04a verdict explicit + references Plan 05 directive: DESCOPE + "MUST OMIT Direct3d11_FfpGenerator.{h,cpp}" — VERIFIED
- SPEC R4 acceptance ("Spike result committed as a written finding in a Phase 11 plan artifact") — SATISFIED
