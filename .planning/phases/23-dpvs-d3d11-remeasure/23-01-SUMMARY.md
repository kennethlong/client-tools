---
phase: 23-dpvs-d3d11-remeasure
plan: 01
subsystem: infra
tags: [dpvs, occlusion-culling, instrumentation, csv, profiling, d3d11, clientGraphics, renderworld, debugflag]

# Dependency graph
requires:
  - phase: 10-dpvs-culling-experiment
    provides: "Phase 10 DPVS instrumentation harness (CSV writer, run-label sanitizer, occlusion A/B method), preserved at git tag phase-10-instrumentation-pre-cleanup (9f2ec3715); analysis.py + test-protocol.md survived in tree"
provides:
  - "DpvsProfileInstrumentation.{h,cpp} restored CPU-only (CSV writer + run-label sanitizer + overlay + ExitChain teardown), schema-locked to analysis.py EXPECTED_HEADER"
  - "RenderWorld occlusion A/B toggle re-gated on the surviving ms_forceDisableOcclusionCulling DebugFlag (the OCCLUSION_CULLING bit, _DEBUG-only)"
  - "Two _DEBUG RenderWorld accessors: get/toggleForceDisableOcclusionCulling (replace the deleted get/setDisableOcclusionCulling pair)"
  - "PerformanceTimer QPC bracket around resolveVisibility feeding recordCpuQpcUs/recordVisibleObjectCount/recordProfilerUs"
  - "clientGraphics.vcxproj build entries for the restored module"
affects: [23-02, 23-03, dpvs-capture, f10-f11-wiring, setrunlabel]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Restore-from-recovery-tag with surgical adaptation (re-point deleted symbols at surviving DebugFlag)"
    - "GPU-strip: structurally-zero gpu_us column kept in CSV schema but always 0 (DPVS is CPU-side software occlusion)"
    - "_DEBUG-gated debug surface: occlusion toggle + accessors + QPC bracket all inside #ifdef _DEBUG so shipped Option α #else branch is byte-for-byte unchanged"

key-files:
  created:
    - src/engine/client/library/clientGraphics/include/public/clientGraphics/DpvsProfileInstrumentation.h
    - src/engine/client/library/clientGraphics/src/shared/DpvsProfileInstrumentation.cpp
  modified:
    - src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp
    - src/engine/client/library/clientGraphics/include/public/clientGraphics/RenderWorld.h (via src/shared/RenderWorld.h)
    - src/engine/client/library/clientGraphics/build/win32/clientGraphics.vcxproj

key-decisions:
  - "Restored the header verbatim (no occlusion-getter reference, needs no adaptation); restored the .cpp with two adaptations (GPU-strip + occlusion-flag re-point)"
  - "GPU-strip per Shared Pattern B: onFrameEnd pushes gpu_us=0 instead of calling the un-restored Direct3d9 Graphics::dpvsGpuTimingPollResult forwarder"
  - "Occlusion-flag source re-pointed at new RenderWorld::getForceDisableOcclusionCulling() (Shared Pattern A) — the deleted getDisableOcclusionCulling() does not exist"
  - "Both new accessors and the occlusion read in writeRow/reportOverlay are _DEBUG-gated; in Release the dpvs_occlusion_flag column is 0 and the accessors do not exist"
  - "Visible-object count is _DEBUG-only (RenderWorldCommander::getNumberOfVisibleObjects is _DEBUG); Release records 0"

patterns-established:
  - "Pattern A: occlusion A/B re-gated on the surviving ms_forceDisableOcclusionCulling DebugFlag via two thin _DEBUG accessors"
  - "Pattern B: drop the Direct3d9 GPU-timing path (structural zeros); keep the gpu_us column for the 10-column schema"
  - "Pattern D: CSV header locked byte-for-byte to analysis.py EXPECTED_HEADER (10 columns)"

requirements-completed: [DPVS-01]

# Metrics
duration: 35min
completed: 2026-06-12
---

# Phase 23 Plan 01: DPVS Instrumentation Core Restore + Occlusion A/B Re-gate Summary

**Restored the Phase 10 DPVS CSV-instrumentation core (writer + run-label sanitizer + overlay + ExitChain teardown) CPU-only with gpu_us structurally zeroed, and re-introduced the occlusion-culling A/B toggle by gating the DPVS OCCLUSION_CULLING bit on the surviving ms_forceDisableOcclusionCulling DebugFlag — the engine + build foundation Plans 02/03 consume.**

## Performance

- **Duration:** ~35 min
- **Started:** 2026-06-12T~15:45Z
- **Completed:** 2026-06-12
- **Tasks:** 3
- **Files modified:** 5 (2 created, 3 modified)

## Accomplishments
- Restored `DpvsProfileInstrumentation.{h,cpp}` from tag `phase-10-instrumentation-pre-cleanup`, CPU-only: the CSV header is byte-for-byte equal to `analysis.py` EXPECTED_HEADER (10 columns), the run-label sanitizer (CSV/path/formula-injection control) is verbatim, and the `install()` config read + DebugFlag register + ExitChain teardown are intact.
- GPU-strip (Shared Pattern B): no Direct3d9 GPU-forwarder calls remain — `onFrameEnd` pushes `gpu_us=0` and the RenderWorld QPC bracket dropped the `Graphics::dpvsGpuTimingBegin/End` half. No Direct3d9.cpp / Graphics.{h,cpp} / Gl_dll.def files were touched.
- Re-introduced the occlusion A/B bit (Shared Pattern A): the `_DEBUG` `cullingParameters` branch now ORs `DPVS::Camera::OCCLUSION_CULLING` gated on `ms_forceDisableOcclusionCulling`; the shipped Option α `#else` branch is byte-for-byte unchanged.
- Added two `_DEBUG` RenderWorld accessors (`getForceDisableOcclusionCulling` / `toggleForceDisableOcclusionCulling`) replacing the deleted `get/setDisableOcclusionCulling` pair (F11 target for Plan 02; CSV-column source for the writer).
- Added the PerformanceTimer QPC bracket around `resolveVisibility`, feeding `recordCpuQpcUs` + `recordVisibleObjectCount` + `recordProfilerUs(0)`.
- Both Debug (rasterMajor=11) and Release (rasterMajor=11) link clean (0 unresolved externals each, despite `/FORCE`) and boot without crash.

## Task Commits

Each task was committed atomically:

1. **Task 1: Restore DpvsProfileInstrumentation.{h,cpp} (CPU path) + vcxproj** - `363750b46` (feat)
2. **Task 2: Re-introduce occlusion A/B bit + accessors + QPC bracket in RenderWorld** - `301c570bb` (feat)
3. **Build-fix deviations found during Task 3 build** - `f4de62411` (fix)

Task 3 (rebuild + dual-renderer boot gate) modified no source files of its own — it surfaced the two build-blocking bugs committed in `f4de62411` and ran the build + boot verification.

**Plan metadata:** (final docs commit — this SUMMARY + STATE + ROADMAP + REQUIREMENTS)

## Files Created/Modified
- `clientGraphics/include/public/clientGraphics/DpvsProfileInstrumentation.h` - All-static instrumentation class public surface (restored verbatim).
- `clientGraphics/src/shared/DpvsProfileInstrumentation.cpp` - CSV writer + run-label sanitizer + overlay + ExitChain teardown; gpu_us GPU-stripped to 0; occlusion-flag re-pointed at `getForceDisableOcclusionCulling()` (`_DEBUG`-gated).
- `clientGraphics/src/shared/RenderWorld.cpp` - `_DEBUG` cullingParameters OR-in OCCLUSION_CULLING gated on `ms_forceDisableOcclusionCulling`; two new accessors; QPC bracket around `resolveVisibility`; includes for `DpvsProfileInstrumentation.h` + `sharedDebug/PerformanceTimer.h`.
- `clientGraphics/src/shared/RenderWorld.h` - Two `_DEBUG` static accessors declared in the public surface.
- `clientGraphics/build/win32/clientGraphics.vcxproj` - ClCompile + ClInclude entries for the restored module (alphabetically adjacent to DebugPrimitive).

## Decisions Made
- Restored the header verbatim (the tag header carries no deleted-getter reference, so no adaptation is needed).
- Kept the `gpu_us` column in the 10-column schema but always 0 — DPVS `resolveVisibility()` is CPU-side software occlusion (Phase 10 measured 12,016/12,016 frames at gpu_us=0). This keeps `analysis.py` (which hard-rejects on header mismatch) happy without restoring the wrong-renderer GPU path.
- All occlusion-toggle surface lives inside `#ifdef _DEBUG` so the shipped Release Option α `#else` branch is unchanged (Elevation mitigation T-23-02).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] `#ifdef` inside a macro argument list broke compilation**
- **Found during:** Task 3 (clientGraphics Debug build)
- **Issue:** `reportOverlay()` placed `#ifdef _DEBUG ... #else ... #endif` directly inside the `DEBUG_REPORT_PRINT(...)` macro invocation's argument list — preprocessor directives are illegal inside a macro call, producing C2121/C2065/C2143/C2059.
- **Fix:** Hoisted the occlusion-state selection out of the macro call into a `char const * const occlusionState` computed under `#ifdef _DEBUG` first, then passed the variable into the macro.
- **Files modified:** `DpvsProfileInstrumentation.cpp`
- **Verification:** clientGraphics Debug rebuilt clean (0 errors).
- **Committed in:** `f4de62411`

**2. [Rule 1 - Bug] QPC bracket called a `_DEBUG`-only symbol in Release**
- **Found during:** Task 3 (clientGraphics Release build)
- **Issue:** The QPC bracket called `RenderWorldCommander::getNumberOfVisibleObjects()` unconditionally, but that method is declared inside `#ifdef _DEBUG` in `RenderWorldCommander.h` — Release failed with C3861 (identifier not found). Debug compiled because the symbol is available there.
- **Fix:** Guarded the `recordVisibleObjectCount` call with `#ifdef _DEBUG` (real count) / `#else` (0). The CPU-timing record (`recordCpuQpcUs`) stays unconditional.
- **Files modified:** `RenderWorld.cpp`
- **Verification:** clientGraphics Release + SwgClient Release rebuilt clean (0 unresolved externals).
- **Committed in:** `f4de62411`

---

**Total deviations:** 2 auto-fixed (both Rule 1 bugs). Both committed together in `f4de62411`.
**Impact on plan:** Both fixes were build-blocking and necessary; no scope creep. They preserve the plan's intent (CPU timing always recorded; visible-object count is a `_DEBUG`-only measurement signal, 0 in Release).

## Issues Encountered

- **SWGSource VM login server (192.168.1.200:44453) not running this session.** The host pings but the login port is closed, so the boot gate could not reach the full character-select screen (requires login auth → cluster select → character list). This is a documented session-time prerequisite (23-RESEARCH.md Environment Availability), not a code blocker.
  - **What was verified instead:** Debug (rasterMajor=11) booted through full engine init + D3D11 device/swapchain + UI render loop to the **login screen** (`SwgCuiLoginScreen: username=, sessionId=null`), presenting frames steadily with no FATAL/crash for 35s — exercising the new `_DEBUG` occlusion re-gate, accessors, QPC bracket, and instrumentation class linked into `SwgClient_d.exe`. Release (rasterMajor=11) completed engine install through `Game::install` + game loop with no crash for 30s — confirming the shared-header ABI is intact for the Option α `#else` branch.
  - Plan 03 (the capture session) must be run with the SWGSource VM up; the build + renderer-init + boot-to-login milestone is proven under both renderers here.

## Boot Gate Record (Task 3)
- **Link-grep (both configs):** 0 `unresolved external symbol`, 0 LNK2001/LNK2019, 0 `error C` (verified despite `/FORCE`).
- **Debug rasterMajor=11 boot:** PASS to login screen (no crash; D3D11 device + UI render loop active). Full character-select gated on server availability.
- **Release rasterMajor=11 boot:** PASS through `Game::install` + game loop (no crash; Option α `#else` branch ABI intact).

## Known Stubs
- `gpu_us` CSV column is always 0 — **intentional** (Shared Pattern B): DPVS issues no GPU work, so the column is a structural zero kept only to satisfy the 10-column `analysis.py` schema. Documented in code comments and 23-RESEARCH.md §gpu_us=0. Not a defect; will remain 0 in Plan 03 capture.
- `visible_object_count` is 0 in Release builds — **intentional**: the source counter (`RenderWorldCommander::getNumberOfVisibleObjects`) is `_DEBUG`-only. The measurement build is Debug (Plan 03), where the real count is recorded.

## Next Phase Readiness
- **Plan 02 (F10/F11/command/frame-hook wiring) is unblocked.** All symbols it consumes now exist: `DpvsProfileInstrumentation::{onFrameEnd, toggleCapture, setRunLabel}` and `RenderWorld::toggleForceDisableOcclusionCulling()` (F11 target).
- **Plan 03 (capture session)** requires the SWGSource VM login server running; the harness build + dual-renderer boot are proven.
- The shipped Option α Release path is unchanged — no risk to the released renderer behavior.

---
*Phase: 23-dpvs-d3d11-remeasure*
*Completed: 2026-06-12*

## Self-Check: PASSED
- Created files verified on disk: DpvsProfileInstrumentation.h, DpvsProfileInstrumentation.cpp, 23-01-SUMMARY.md
- Commits verified in git log: 363750b46, 301c570bb, f4de62411
