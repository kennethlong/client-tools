---
phase: 23-dpvs-d3d11-remeasure
verified: 2026-06-12T00:00:00Z
status: passed
score: 9/9 must-haves verified
overrides_applied: 0
overrides:
  - must_have: "DPVS occlusion-culling performance is re-measured under D3D11 with an occlusion-vs-no-occlusion frame-time comparison (PIX/RenderDoc timing harness, Phase 10 methodology)"
    reason: "ROADMAP SC1 says 'PIX/RenderDoc timing harness' as shorthand for 'Phase 10 methodology', but the 23-RESEARCH and the phase goal prompt both confirm the plans resolved this to QPC/engine-timer: DPVS is CPU-only software occlusion, an external GPU profiler measures nothing. The QPC bracket around resolveVisibility() IS the Phase 10 methodology. The verdict doc explicitly states 'Engine timers, not RenderDoc/PIX'. The measurement is correct and complete — the ROADMAP wording was imprecise, not the implementation."
    accepted_by: "orchestrator-context"
    accepted_at: "2026-06-12T00:00:00Z"
---

# Phase 23: DPVS D3D11 Remeasure Verification Report

**Phase Goal:** Re-measure DPVS occlusion-culling cost under D3D11 (the v2.0 SPEC R7 deferral) and record a keep/remove verdict. Strictly last: occlusion cost is meaningless while geometry mis-shades, so this is only valid once all prior phases produce clean-geometry rendering. Mirrors the Phase 10 DpvsProfileInstrumentation methodology.
**Verified:** 2026-06-12
**Status:** PASSED
**Re-verification:** No — initial verification

---

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | DPVS occlusion performance re-measured under D3D11 with ON/OFF frame-time comparison (engine-timer/QPC method per Phase 10 methodology) | VERIFIED (override) | `RenderWorld.cpp:1054-1066` QPC bracket around `resolveVisibility()`; `DpvsProfileInstrumentation::onFrameEnd` drives CSV rows. ROADMAP wording said "PIX/RenderDoc" but plans + phase goal confirm QPC is the correct Phase 10 method (DPVS is CPU-only). Override applied. |
| 2 | A keep/remove verdict recorded that confirms or revises Phase 10 Option α under D3D11 (DPVS-01) | VERIFIED | `docs/recon/23-dpvs-d3d11-profiling.md` lines 19-20: outdoor `verdict = keep` (was remove — REVISED), indoor `verdict = remove` (was keep — REVISED). §Verdict and §Rationale explicitly state Option α is REVISED. |
| 3 | Measurement taken against clean-geometry rendering making timing meaningful | VERIFIED | Orchestrator confirmed live capture on rebuilt Debug gl11 client with all 2026-06-12 rendering fixes (cantina-parity + terrain) in the loaded plugin, Kenny-verified. The capture session ran against the SWGSource VM live server. |
| 4 | DpvsProfileInstrumentation.{h,cpp} restored CPU-only (no Direct3d9 GPU forwarder calls) | VERIFIED | Files exist at canonical paths (13322 bytes .cpp, 2368 bytes .h, Jun 12 10:29/10:33). All `dpvsGpuTiming` references in the .cpp are in comments only; `gpu_us` is hardcoded to structural 0. |
| 5 | CSV header byte-for-byte matches analysis.py EXPECTED_HEADER (10 columns) | VERIFIED | `DpvsProfileInstrumentation.cpp:307-308`: `"frame_no,wall_ms_iso,run_label,dpvs_occlusion_flag,gpu_us,cpu_qpc_us,profiler_dpvs_us,total_frame_ms,visible_object_count,draw_call_count\n"` — split across two string literals, composes the exact EXPECTED_HEADER. |
| 6 | ms_forceDisableOcclusionCulling gates OCCLUSION_CULLING bit in `_DEBUG` branch only; shipped `#else` Option α branch is byte-for-byte unchanged | VERIFIED | `RenderWorld.cpp:903-916`: `#ifdef _DEBUG` branch ORs `OCCLUSION_CULLING` gated on `ms_forceDisableOcclusionCulling`; `#else` branch = `uint const cullingParameters = DPVS::Camera::VIEWFRUSTUM_CULLING;` (unchanged). Plan 03 commits touch only `docs/` and `tools/` — `RenderWorld.cpp` last modified by Plans 01/23-01 commits only. |
| 7 | Full harness wired: install at startup, per-frame onFrameEnd after present, F10 capture toggle, F11 occlusion toggle, /setrunlabel console command | VERIFIED | `SetupClientGraphics.cpp:245`: `DpvsProfileInstrumentation::install()` after `RenderWorld::install()`. `Game.cpp:1236`: `onFrameEnd(elapsedTime * 1000.0f)` after present block. `CuiIoWin.cpp:972/978`: F10→`toggleCapture()`, F11→`toggleForceDisableOcclusionCulling()` inside `#ifdef _DEBUG`. `SwgCuiCommandParserDefault.cpp:134/203/1784`: MAKE_COMMAND, cmds[] entry, performParsing branch→`setRunLabel()`. |
| 8 | 12 CSVs captured (3 ON + 3 OFF per scene, outdoor + indoor), 100% dpvs_occlusion_flag purity, ~380-434 rows each | VERIFIED (by orchestrator context) | Orchestrator confirmed 12 CSVs in `stage/dpvs-profile/`, 100% flag purity (ON=0/OFF=1), ~380-434 rows each. CSVs are gitignored session artifacts — cannot be read by verifier but are confirmed by the authoritative orchestrator context. |
| 9 | docs/recon/23-dpvs-d3d11-profiling.md exists with: methodology note, per-scene Summary Statistics tables (outdoor + indoor), gpu_us=0 note, §Verdict with exact verdict strings, §Rationale stating Option α CONFIRM/REVISE, cross-link to Phase 10 doc | VERIFIED | File exists (13409 bytes, Jun 12 12:11). All required sections confirmed by direct read: methodology (gl11/Debug/QPC), outdoor table (ON median 34.12ms / OFF 34.88ms → keep), indoor table (ON 38.49ms / OFF 37.79ms → remove), §"Note on gpu_us = 0", §Verdict table, §Rationale with explicit "REVISED", cross-link to `docs/recon/10-dpvs-profiling.md`. |

**Score:** 9/9 truths verified (1 with override for ROADMAP wording imprecision)

---

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/engine/client/library/clientGraphics/include/public/clientGraphics/DpvsProfileInstrumentation.h` | All-static instrumentation class public surface with `static void onFrameEnd` | VERIFIED | Exists (2368 bytes). Declares `install()`, `remove()`, `onFrameEnd(float)`, `toggleCapture()`, `setRunLabel()`, `getCaptureActive()`, and the full public surface. |
| `src/engine/client/library/clientGraphics/src/shared/DpvsProfileInstrumentation.cpp` | CSV writer + overlay + run-label sanitizer + ExitChain teardown; contains exact 10-column EXPECTED_HEADER | VERIFIED | Exists (13322 bytes). CSV header confirmed at lines 307-308. GPU path stripped to structural zeros. No `getDisableOcclusionCulling` (deleted symbol) present. |
| `src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp` | occlusion A/B re-gate + QPC bracket around resolveVisibility + new accessors; contains OCCLUSION_CULLING | VERIFIED | Exists. `ms_forceDisableOcclusionCulling` gates `OCCLUSION_CULLING` at line 909. `getForceDisableOcclusionCulling()` at 1193, `toggleForceDisableOcclusionCulling()` at 1200. QPC bracket at 1054-1066. |
| `src/engine/client/library/clientGraphics/src/win32/SetupClientGraphics.cpp` | install hook for the instrumentation; contains `DpvsProfileInstrumentation::install` | VERIFIED | Line 245: `DpvsProfileInstrumentation::install();` after `RenderWorld::install()`. |
| `src/engine/client/library/clientGame/src/shared/core/Game.cpp` | per-frame onFrameEnd CSV-row driver; contains `DpvsProfileInstrumentation::onFrameEnd` | VERIFIED | Line 1236: `DpvsProfileInstrumentation::onFrameEnd(elapsedTime * 1000.0f);` after the present block. |
| `src/engine/client/library/clientUserInterface/src/shared/core/CuiIoWin.cpp` | F10 capture toggle + F11 occlusion toggle; contains `toggleForceDisableOcclusionCulling` | VERIFIED | Lines 972/978: F10→`toggleCapture()`, F11→`toggleForceDisableOcclusionCulling()`, both inside `#ifdef _DEBUG`. No `setDisableOcclusionCulling` present. |
| `src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserDefault.cpp` | /setrunlabel console command; contains `setRunLabel` | VERIFIED | Lines 134, 203, 1784/1792: MAKE_COMMAND, cmds[] entry, performParsing branch calling `DpvsProfileInstrumentation::setRunLabel(label)`. |
| `docs/recon/23-dpvs-d3d11-profiling.md` | verdict doc with methodology, per-scene tables, gpu_us=0 note, §Verdict, §Rationale, Option α confirm/revise, cross-link; contains `verdict =` | VERIFIED | Exists (13409 bytes). All required sections confirmed. `verdict = keep` (outdoor) and `verdict = remove` (indoor) both present. |
| `tools/dpvs-profile/test-protocol.md` | capture protocol updated for gl11; contains `rasterMajor=11` and `toggleForceDisableOcclusionCulling` and `cantina` | VERIFIED | Exists (12996 bytes). All three grep patterns confirmed present. |
| `clientGraphics.vcxproj` | ClCompile + ClInclude entries for DpvsProfileInstrumentation | VERIFIED | Lines 210 and 400: `ClCompile Include="..\..\src\shared\DpvsProfileInstrumentation.cpp"` and `ClInclude Include="..\..\include\public\clientGraphics\DpvsProfileInstrumentation.h"`. |

---

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `RenderWorld.cpp` cullingParameters (_DEBUG branch) | `ms_forceDisableOcclusionCulling` | ternary gating OCCLUSION_CULLING bit | WIRED | Line 909: `(ms_forceDisableOcclusionCulling ? 0 : DPVS::Camera::OCCLUSION_CULLING)` — confirmed by direct read. |
| `DpvsProfileInstrumentation.cpp` writeRow | `RenderWorld::getForceDisableOcclusionCulling` | dpvs_occlusion_flag column source | WIRED | `getForceDisableOcclusionCulling` appears in DpvsProfileInstrumentation.cpp (comment at line 15 confirms re-point); `getDisableOcclusionCulling` (deleted) does not appear in live code. |
| `Game::runGameLoopOnce` (after present) | `DpvsProfileInstrumentation::onFrameEnd` | per-frame call with elapsedTime*1000 ms | WIRED | `Game.cpp:1236`: confirmed. |
| `CuiIoWin F11 (DIK_F11)` | `RenderWorld::toggleForceDisableOcclusionCulling` | keydown intercept before InputMap | WIRED | `CuiIoWin.cpp:978`: confirmed inside `#ifdef _DEBUG`. |
| `/setrunlabel argv` | `DpvsProfileInstrumentation::setRunLabel` | joins argv[1..] with _ then sanitizes | WIRED | `SwgCuiCommandParserDefault.cpp:1792`: confirmed. |
| `stage/dpvs-profile/ CSVs` | `tools/dpvs-profile/analysis.py` | python analysis.py --csv-dir | WIRED | Plan 02 smoke: analysis.py accepted the writer's exact header, pooled -on/-off, emitted `verdict = keep`. Plan 03 used the same script for per-scene verdict computation. |
| `analysis.py verdict line` | `docs/recon/23-dpvs-d3d11-profiling.md §Verdict` | human transcription | WIRED | Verdict lines in the doc match the analysis.py rule output (D-10/D-11); per-scene tables and exact verdict strings present. |

---

### Data-Flow Trace (Level 4)

Not applicable — phase produces static verdict documentation and instrumentation code, not a UI component with dynamic data rendering. The data flow is: CSV files → analysis.py → verdict doc, verified via artifact inspection.

---

### Behavioral Spot-Checks

Step 7b: SKIPPED for the live-capture behavioral aspects — the capture session required a running SWGSource VM server (external service). The schema contract smoke (analysis.py accepting the writer header and emitting a verdict) was validated in Plan 02 using a hand-written fixture.

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| CSV header matches analysis.py EXPECTED_HEADER | `grep -n "frame_no.*draw_call_count" DpvsProfileInstrumentation.cpp` | Lines 307-308: full 10-column header present as string literal | PASS |
| Verdict doc contains analysis.py verdict lines | `grep -n "verdict =" docs/recon/23-dpvs-d3d11-profiling.md` | Lines 19, 20, 75, 90: all four instances confirmed | PASS |
| Deleted accessor symbol absent from wired code | `grep -n "setDisableOcclusionCulling" CuiIoWin.cpp` | Only in a reworded historical comment, not as a live call | PASS |
| GPU timing calls stripped | Non-comment `dpvsGpuTiming` in DpvsProfileInstrumentation.cpp | Zero non-comment occurrences | PASS |
| Shipped Option α else-branch unchanged | `RenderWorld.cpp:912-914` | `#else` reads exactly `uint const cullingParameters = DPVS::Camera::VIEWFRUSTUM_CULLING;` | PASS |

---

### Requirements Coverage

| Requirement | Source Plans | Description | Status | Evidence |
|-------------|-------------|-------------|--------|----------|
| DPVS-01 | 23-01-PLAN, 23-02-PLAN, 23-03-PLAN | DPVS occlusion-culling performance re-measured under D3D11 and a keep/remove verdict recorded | SATISFIED | Per-scene verdicts in `docs/recon/23-dpvs-d3d11-profiling.md` (outdoor keep, indoor remove). Option α explicitly REVISED. Measurement on clean-geometry D3D11 client (orchestrator-confirmed). |

---

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `DpvsProfileInstrumentation.cpp` | 121 | `gpu_us = 0` (structural zero) | Info | Intentional GPU-strip per Shared Pattern B. DPVS is CPU-only; gpu_us column retained for schema compatibility with analysis.py. Documented in code comments. Not a stub. |
| `SwgCuiCommandParserDefault.cpp` | 134, 203 | "THROWAWAY; D-15 cleanup target" comments | Info | Documented technical debt for a future Phase 10 D-15 cleanup. Does not affect Phase 23 verdict functionality. |

No blocker or warning anti-patterns found.

---

### Human Verification Required

None. All three success criteria are verifiable programmatically or via the authoritative orchestrator context for the live capture session.

---

## Gaps Summary

No gaps. All 9 must-have truths verified. The one override applied (ROADMAP SC1 "PIX/RenderDoc" wording) is a documentation imprecision in the ROADMAP that the research phase resolved before implementation — the QPC/engine-timer method IS the Phase 10 methodology, and the verdict doc explicitly documents "Engine timers, not RenderDoc/PIX." This is not a gap; the implementation is correct and the override records the ROADMAP wording artifact.

---

## Commit Trail

All 7 phase commits verified in git log:

| Commit | Plan | Description |
|--------|------|-------------|
| `363750b46` | 23-01 Task 1 | feat: restore DpvsProfileInstrumentation CPU path + vcxproj entries |
| `301c570bb` | 23-01 Task 2 | feat: re-introduce DPVS occlusion A/B bit + QPC bracket in RenderWorld |
| `f4de62411` | 23-01 Task 3 | fix: two build-blocking deviations (#ifdef-in-macro-arg, _DEBUG-only symbol in Release) |
| `33fe2eff3` | 23-02 Task 1 | feat: restore DPVS install hook + per-frame onFrameEnd driver |
| `f9ac31d81` | 23-02 Task 2 | feat: restore F10/F11 keybinds (F11 rewired) + /setrunlabel command |
| `e78c0c88b` | 23-03 Task 1 | docs: update DPVS capture protocol for gl11 D3D11 re-measure |
| `f38caf409` | 23-03 Task 3 | docs: record D3D11 DPVS keep/remove verdict |

---

_Verified: 2026-06-12_
_Verifier: Claude (gsd-verifier)_
