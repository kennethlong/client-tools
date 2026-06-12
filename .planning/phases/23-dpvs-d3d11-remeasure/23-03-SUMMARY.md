---
phase: 23-dpvs-d3d11-remeasure
plan: 03
subsystem: infra
tags: [dpvs, occlusion-culling, d3d11, profiling, renderworld, performance]

# Dependency graph
requires:
  - phase: 23-dpvs-d3d11-remeasure (plan 01)
    provides: DpvsProfileInstrumentation core restored (CPU-only, gpu_us structural 0, F11→toggleForceDisableOcclusionCulling)
  - phase: 23-dpvs-d3d11-remeasure (plan 02)
    provides: harness wired + activated (install at SetupClientGraphics, onFrameEnd, F10/F11 keybinds, /setrunlabel); analysis.py accepts the writer header
provides:
  - "D3D11 DPVS occlusion keep/remove verdict per scene (outdoor=keep, indoor=remove)"
  - "docs/recon/23-dpvs-d3d11-profiling.md — the v2.0 SPEC R7 / D-12 deferred D3D11 re-measure"
  - "gl11 capture protocol (tools/dpvs-profile/test-protocol.md)"
affects: [renderworld, dpvs-option-alpha, v2.2-milestone-close]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Per-scene verdict via run-label prefix segmentation: analysis.py pools all CSVs in --csv-dir by -on/-off suffix, so each scene's 6 CSVs are staged into a temp dir and analyzed separately"
    - "visible_object_count column is the mechanistic key: outdoor culls ~140 objects (359 vs ~500), indoor culls nothing (443 vs 443) — explains the sign of each verdict"

key-files:
  created:
    - docs/recon/23-dpvs-d3d11-profiling.md
  modified:
    - tools/dpvs-profile/test-protocol.md  # Task 1 (committed e78c0c88b)

key-decisions:
  - "Both DPVS verdicts flip under D3D11 vs Phase 10 D3D9: outdoor remove→keep, indoor keep→remove. Option α's central premise (remove globally because outdoor wins dominate) is REVISED — under D3D11 outdoor prefers occlusion ON."
  - "No code change in this measurement phase: shipped Option α #else branch in RenderWorld.cpp untouched (T-23-02). Acting on the verdict (scene-aware split, or revert outdoor removal) is a follow-up decision, out of scope for the re-measure."
  - "Debug-build relative A/B is the defensible method (Open Question 1): the verdict turns on the ON-vs-OFF relative delta, which Debug preserves; the toggle is _DEBUG-only anyway."

patterns-established:
  - "DPVS re-measure verdict doc lives in docs/recon/ as a sibling to the D3D9 verdict (23- mirrors 10-)"
  - "dpvs_occlusion_flag is authoritative over run-label (0=ON, 1=OFF); 100% flag purity per pool proves the F11 toggle flipped culling live"

requirements-completed: [DPVS-01]

# Metrics
duration: 18min
completed: 2026-06-12
---

# Phase 23 Plan 03: DPVS D3D11 Re-measure Verdict Summary

**Both DPVS occlusion verdicts flip under D3D11 vs Phase 10 D3D9 — outdoor `remove→keep`, indoor `keep→remove` — revising the Option α premise; recorded in docs/recon/23-dpvs-d3d11-profiling.md (DPVS-01 closed).**

## Performance

- **Duration:** ~18 min (analysis + doc + summary; excludes the live capture session)
- **Started:** 2026-06-12 (post-capture, Task 3)
- **Completed:** 2026-06-12
- **Tasks:** 3 (Task 2 was the human/orchestrator-piloted live capture session)
- **Files modified:** 2 (1 created, 1 modified across Tasks 1+3)

## Accomplishments

- **Re-measured DPVS occlusion cost under D3D11** (gl11, Debug, live SWGSource server) across an outdoor (Mos Eisley plaza) and indoor (cantina) scene — the v2.0 SPEC R7 / Phase 10 D-12 deferral.
- **Per-scene verdicts computed by the unchanged D-10/D-11 rule:** outdoor `verdict = keep`, indoor `verdict = remove`. Both flip sign vs Phase 10.
- **Wrote the verdict doc** mirroring the Phase 10 structure: methodology (gl11/Debug, engine-timer/QPC, not RenderDoc), per-scene Summary Statistics tables, gpu_us=0 note, §Verdict, §Rationale with explicit Option α REVISE, and a cross-link to docs/recon/10-dpvs-profiling.md.
- **Identified the mechanism via visible_object_count:** outdoor occlusion culls ~140 objects (359 vs ~500) and now nets positive; indoor portals already bind the set (443 vs 443) so the occlusion query is dead overhead.
- **Shipped Option α `#else` branch untouched** — no RenderWorld.cpp in this plan's diff (acceptance + T-23-02 honored).

## Task Commits

1. **Task 1: Update capture protocol for gl11 + outdoor+indoor + F11 rewire** — `e78c0c88b` (docs) — committed in a prior session
2. **Task 2: Live D3D11 capture session** — no source commit; produced 12 CSVs in `stage/dpvs-profile/` (gitignored). Human/orchestrator-piloted live session on the SWGSource VM.
3. **Task 3: Run analysis.py + write the verdict doc** — `f38caf409` (docs)

**Plan metadata:** (this commit) — docs: complete plan

## Files Created/Modified

- `docs/recon/23-dpvs-d3d11-profiling.md` (created) — the D3D11 keep/remove verdict doc: per-scene tables, gpu_us=0 note, Option α REVISE rationale, cross-link to the Phase 10 doc.
- `tools/dpvs-profile/test-protocol.md` (modified, Task 1) — gl11/Debug capture protocol, F11 → `toggleForceDisableOcclusionCulling`, outdoor+indoor scene set, gpu_us=0 / flag-authority / gl11-noise notes.

## Verdict Detail

| Scene | Topology | total_frame_ms median (ON / OFF) | p95 (ON / OFF) | analysis.py | vs Phase 10 |
|-------|----------|----------------------------------|----------------|-------------|-------------|
| Mos Eisley plaza | Outdoor | 34.12 / 34.88 | 41.27 / 42.33 | **verdict = keep** | was `remove` → REVISED |
| Cantina | Indoor | 38.49 / 37.79 | 45.84 / 44.87 | **verdict = remove** | was `keep` → REVISED |

- Outdoor: ON wins (occlusion culls ~140 objects, 359 vs ~500 visible). `off.median > on.median` ⇒ keep.
- Indoor: OFF wins (identical 443 visible both ways — portals already cull; query is overhead). `off.median ≤ on.median` AND `off.p95 ≤ on.p95` ⇒ remove.
- Both pools 100% `dpvs_occlusion_flag`-pure (ON=0, OFF=1) across all 12 CSVs — F11 toggle proven live.

## Decisions Made

- **Verdict is recorded as a decision artifact, no code change.** Option α (`remove` globally) is, under D3D11, right for indoor but wrong for the dominant outdoor case — its premise is revised. Whether to revert the outdoor removal, leave Option α as-shipped, or implement a scene-aware `cullingParameters` split is a follow-up decision for a future phase, explicitly out of scope for this measurement plan (per the plan action + threat T-23-02).
- **Per-scene segmentation method:** analysis.py pools by `-on`/`-off` suffix only, so each scene's 6 CSVs were copied into a separate temp dir and analyzed independently to get per-scene verdicts. The source CSVs in `stage/dpvs-profile/` were never modified or deleted.

## Deviations from Plan

None — plan executed exactly as written. Task 3 ran analysis.py per scene, recorded the exact verdict lines, and produced the verdict doc with all required sections.

## Issues Encountered

- **Instrumentation facts noted in the doc (not data problems), carried from the capture session:** `profiler_dpvs_us = 0` (profiler counter not fed this session) and `draw_call_count = 0` (`getRenderedVerticesPointsLinesTrianglesCalls` returns 0 under D3D11) — neither drives the verdict; the D-09 primary metric `total_frame_ms` and `cpu_qpc_us` are fully populated. `gpu_us = 0` is the correct structural answer (DPVS is CPU-only). All documented in the doc's methodology/notes.
- **Build-staleness (resolved pre-capture, by orchestrator):** the Plan-02 exe had a stale clientGraphics.lib so `install()` was absent; clientGraphics was rebuilt + SwgClient_d.exe relinked before capture (build outputs only, no source changes — nothing to commit).

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness

- **DPVS-01 satisfied** — D3D11 re-measure complete, keep/remove verdict recorded per scene, Option α confirm/revise documented. This was the STRICTLY-LAST v2.2 requirement.
- **Follow-up (not blocking):** the verdict implies the shipped Option α global `remove` is no longer optimal for outdoor under D3D11 (outdoor now prefers occlusion ON). A future phase may evaluate a scene-aware `cullingParameters` split or reverting the outdoor removal. The single global `ms_dpvsCamera->setParameters()` makes a per-cell split a non-trivial refactor (as Phase 10 noted).

## Self-Check: PASSED

- FOUND: docs/recon/23-dpvs-d3d11-profiling.md
- FOUND: .planning/phases/23-dpvs-d3d11-remeasure/23-03-SUMMARY.md
- FOUND: tools/dpvs-profile/test-protocol.md
- FOUND commit: e78c0c88b (Task 1)
- FOUND commit: f38caf409 (Task 3)

---
*Phase: 23-dpvs-d3d11-remeasure*
*Completed: 2026-06-12*
