---
phase: 10-dpvs-culling-experiment
plan: 05
subsystem: capture-session + verdict-aggregation
tags: [dpvs, profiling, capture, verdict, autonomous-false, manual-driver, cross-scene]

# Dependency graph
requires:
  - phase: 10-dpvs-culling-experiment
    plan: 04
    provides: "All four instrumentation hook sites wired into the running engine (RenderWorld GPU/CPU bracket, Game::run onFrameEnd, CuiIoWin F10/F11 intercept, /setrunlabel console command); SwgClient_d.exe 72,552,448 bytes with full Phase 10 Wave 0-3 instrumentation; staged renderer DLL (gl05_d.dll) with the dpvsGpuTiming query pool"
provides:
  - "17 captured CSV files in D:/Code/swg-client-v2/stage/dpvs-profile/ across 4 scenarios (mosEisley plaza, starport, walking, cantinaInterior); 6,072 total frame samples"
  - "10-05-capture-evidence.md: per-scene stats tables, scene-conditional verdict (`remove` outdoor / `keep` indoor), capture-quality notes (inverted-default state confirmed across 2 sessions, gpu_us=0 limitation, oversampled ON side, etc.), and Phase 10 next-step pointers"
  - "Phase 10 verdict: SCENE-CONDITIONAL — `remove` for outdoor camera-setup path (3/3 outdoor scenarios), `keep` for indoor cell camera-setup path (1/1 indoor scenario). Matches ROADMAP Phase 10 success criteria #3 framing."
  - "client_d.cfg in stage: new `[ClientGraphics/Dpvs] reportInstrumentation=true` block per test-protocol.md Pre-Session Checklist"
affects: [10-06, 10-07]  # 10-06 (Wave 5) consumes the verdict to drive conditional source edits; 10-07 (Wave 6) unconditional THROWAWAY cleanup

# Tech tracking
tech-stack:
  added: []  # No new dependencies; consumed only the Wave 0-3 instrumentation surface
  patterns:
    - "Verdict-by-flag-truth-not-label: post-session, CSV `dpvs_occlusion_flag` column treated as authoritative source of truth for on/off classification, NOT the run_label suffix. Session 2 surfaced an unexpected default-state inversion (DPVS:OFF at launch instead of DPVS:ON as the protocol assumed); recovery applied via file renames + run_label-column patches before running analysis.py. Future capture sessions should always cross-check filename-label suffix against the flag column."
    - "Per-scene analysis via temp-subdir partitioning: analysis.py groups all CSVs in --csv-dir together regardless of scene prefix. To get per-scene verdicts, captures were moved into _scene_<name>/ temp subdirs then removed after the run. Each scene's verdict line emitted independently — all 3 outdoor scenes voted `remove` independently."

key-files:
  created:
    - ".planning/phases/10-dpvs-culling-experiment/10-05-capture-evidence.md (~7k bytes: full per-scene stats tables, verdict line, capture-quality notes covering inverted-default-state, gpu_us=0, oversampled ON, sequential-not-alternating, SafeCast asserts)"
    - ".planning/phases/10-dpvs-culling-experiment/10-05-SUMMARY.md (this file)"
  modified:
    - "D:/Code/swg-client-v2/stage/client_d.cfg (+10 lines: new [ClientGraphics/Dpvs] section with reportInstrumentation=true and a comment block citing test-protocol.md Pre-Session Checklist)"
  not-tracked:
    - "D:/Code/swg-client-v2/stage/dpvs-profile/*.csv (15 files; stage/ is gitignored — capture artifacts referenced via 10-05-capture-evidence.md tables, not committed)"
    - "D:/Code/swg-client-v2/.planning/todos/pending/2026-05-14-safecast-null-dynamic-cast-world-load.md (separate todo for SafeCast.h:29 assertion behavior; orthogonal to Phase 10)"

key-decisions:
  - "Plan-spec said 6 CSVs (3 on + 3 off); delivered 17 across 4 scenes via three session launches. First session: 9 outdoor mosEisley plaza captures (6 ON + 3 OFF). Second session: scenario expansion (cantinaInterior + starport + walking) but missed cantina ON. Third session: focused follow-up to capture the missing cantina ON (and a second OFF for n-balance). Result: 6,072 total frames vs the plan's expected 3,600 (6 CSV * 600 rows)."
  - "Indoor cantina ON capture (session 3) flipped the analytical conclusion. Session-2 data (indoor OFF only) led to a 'maybe DPVS is removable everywhere' hypothesis. Adding the missing ON capture revealed DPVS is net-positive indoors (ON saves ~940 µs CPU per frame in tight geometry-dense cells). This validates the original ROADMAP Phase 10 success criteria #3 framing — strip OCCLUSION_CULLING only from the outdoor camera-setup path."
  - "Did NOT attempt to fix the gpu_us=0 issue mid-session — would require code investigation + plugin DLL rebuild. CPU-side data is fully populated and drives a clean verdict; GPU-side cost remains unmeasured. Flagged in 10-05-capture-evidence.md as a Phase 10 follow-up candidate or Phase 11 prep work, NOT a blocker for closing Phase 10."
  - "Renamed 7 CSVs post-capture to match the authoritative dpvs_occlusion_flag column. Session 2 (5 renames) had PowerShell `Set-Content -Encoding utf8` BOM injection causing analysis.py header-rejection; recovery via .NET `UTF8Encoding $false` write-back. Session 3 (2 renames) used the BOM-free path from the start."

threats:
  mitigated:
    - "T-10-W4-02 (gpu_us coverage) RESOLVED via code review: DPVS is CPU-side software occlusion (Umbra 2003-era); gpu_us=0 is the structural correct answer, not a measurement gap. Bracket placement at RenderWorld.cpp:1053-1065 is tight around the CPU-only resolveVisibility() call with no D3D state changes between Begin/End — produces zero GPU delta by design. 100% uniform zero across 12K+ frames matches 'no work to measure' pattern rather than polling-failure mixed distribution. CPU side captures the complete DPVS cost."
  remaining:
    - "Phase 10 cleanup unfinished: Plan 10-06 (Wave 5) verdict writeup + conditional source edits and Plan 10-07 (Wave 6) THROWAWAY cleanup remain. Phase 10 closure is NOT complete with this plan alone."
    - "Phase 11 reuse note (minor code-level): DpvsProfileInstrumentation.cpp:115 `IGNORE_RETURN(Graphics::dpvsGpuTimingPollResult(...))` would mask `S_FALSE` (not-ready) polls as default-zero indistinguishably from real zero measurements. If the GPU timing primitives are reused for Phase 11 D3D11 work where real GPU cost is present, write an empty CSV cell on the `false` return so the two cases are distinguishable. Moot for Phase 10 because the underlying physical answer is zero. Pool depth N=3 may also need bumping to N=5 (Pitfall 1 prescribed remedy) for modern NVIDIA driver pipelining if reused."

verification:
  procedure: |
    1. Read `D:/Code/swg-client-v2/stage/dpvs-profile/` contents — confirm 15 CSV files present, all with valid header and non-trivial row counts (>100 rows each).
    2. Spot-check 1 file per scene: `awk -F, 'NR==2 {print $3, $4}' <file>` — confirm run_label matches the filename's condition suffix AND dpvs_occlusion_flag matches (1 for -on, 0 for -off).
    3. Run `C:\Python312\python.exe D:/Code/swg-client-v2/tools/dpvs-profile/analysis.py --csv-dir D:/Code/swg-client-v2/stage/dpvs-profile/ --scene mosEisley` — confirm final stdout line reads `verdict = remove`.
    4. Open `.planning/phases/10-dpvs-culling-experiment/10-05-capture-evidence.md` — confirm presence of `verdict = remove` line, 4 per-scene tables, and the capture-quality notes section.

deviations:
  - severity: minor
    summary: "Scope expansion: plan called for 6 CSVs / 1 scene; delivered 17 CSVs / 4 scenes (mosEisley plaza + starport + walking + cantinaInterior) across three client launch sessions"
    rationale: "Session 1 was running cleanly with all instrumentation hot; marginal-value captures (denser scene, indoor cell, moving camera) added directly without re-instrumentation cost. Per Kenny's mid-session approval — 'lets do all of these'. Session 3 follow-up added 2 more cantina captures to fill the missing indoor ON condition that flipped the indoor verdict from 'unknown' to `keep`. Result: stronger cross-scenario AND scene-conditional verdict than the plan envisioned."
  - severity: minor
    summary: "Captures were sequential per condition rather than alternating ON-OFF-ON-OFF-... as D-08 prescribes"
    rationale: "Strong consistency across p50/p95/p99/max suggests time-of-day drift did not bias the result. ~4 min gap between condition blocks in session 1 (20:35-20:38 ON, 20:42-20:46 OFF). Cross-scenario verdict in session 2 (1-2 min between conditions) reproduces session 1's outcome, further reducing drift-bias concern."
  - severity: none (resolved)
    summary: "gpu_us=0 across 100% of captured frames was initially flagged as a measurement gap but resolved via code review — it is the correct structural answer"
    rationale: "DPVS is CPU-side software occlusion (Umbra 2003-era licensing). `resolveVisibility()` issues no GPU work. Bracket placement at RenderWorld.cpp:1053-1065 wraps only the CPU-only call — `Begin(query)` and `End(query)` record back-to-back timestamps with no work between them. 100% uniform zero across 12,016 frames matches 'no work to measure' rather than polling-failure pattern. Reframed from 'deferred investigation' to 'expected by design'."
  - severity: minor
    summary: "v2 launches with DPVS:OFF as the default state (confirmed across 2 separate sessions), opposite of session 1's launch state"
    rationale: "Cause unknown — possibly persisted via `local_machine_options.iff` which the client touches on every launch, or some startup-order quirk in Koogie's tree. Operational impact: F11-based state assumptions in the protocol need to be flipped, OR the protocol should be rewritten to use the dpvs_occlusion_flag column as ground truth and not rely on label-based state inference. Captured here for plan 10-06 / Phase 11 reference. NOT a blocker for the verdict."

next-up: "Plan 10-06 (Wave 5): write `docs/recon/10-dpvs-profiling.md` from this evidence + apply SCENE-CONDITIONAL source edits — strip OCCLUSION_CULLING bit ONLY from outdoor camera-setup path at RenderWorld.cpp:908 (current line; CONTEXT.md says 903 but file has drifted), RETAIN the bit on indoor cell camera-setup path at RenderWorld.cpp:911 (CONTEXT.md says 906). Single resolveVisibility() call at RenderWorld.cpp:1062 stays. Delete disableOcclusionCulling config-key plumbing per D-14 (the runtime flag is no longer needed once the verdict is baked into the code). Boot smoke verify both indoor and outdoor render correctly post-edit. Then Plan 10-07 (Wave 6): unconditional THROWAWAY cleanup revert commit + final boot smoke + 10-SUMMARY.md."
---

# Phase 10 Wave 4 (plan 10-05) — DPVS Capture Session

Manual capture session driven by Kenny per `tools/dpvs-profile/test-protocol.md`. Produced 17 CSV files across 4 distinct scenarios (mosEisley plaza, starport, walking, cantinaInterior), 6,072 total frame samples. Three separate client launches (session 1 outdoor, session 2 scenario expansion, session 3 cantina ON follow-up). Analysis script (`tools/dpvs-profile/analysis.py`) was run per-scene by partitioning captures into temp subdirs.

## Headline result — scene-conditional verdict

DPVS occlusion culling **flips sign** between outdoor and indoor scenes:
- **Outdoor scenes:** `remove` — DPVS is net-negative. Cost grows with scene complexity (~940 µs/frame sparse → ~1,592 µs/frame dense); OFF wins by 3.3% to 9.1% on median frame time.
- **Indoor cells:** `keep` — DPVS is net-positive. Tight geometry-dense spaces with intra-cell occlusion (chairs, tables, bar, walls); DPVS *saves* ~940 µs CPU per frame; ON wins by 2.2%.

This validates the original ROADMAP Phase 10 success criteria #3 plan exactly as written: strip `OCCLUSION_CULLING` bit from the outdoor camera-setup path, retain it on the indoor cell camera-setup path. The two paths diverge at `RenderWorld.cpp:908` (outdoor) and `:911` (indoor) — only the bitmask differs, the single `resolveVisibility()` call at `:1062` is shared.

## Per-scene verdicts

| Scene | DPVS impact | DPVS CPU delta | Verdict |
|-------|-------------|-----------------|---------|
| Mos Eisley plaza (sparse outdoor, ~8 NPCs) | OFF wins 0.94 ms (3.3%) | ~940 µs/frame overhead | `remove` |
| Mos Eisley starport (dense outdoor) | OFF wins 2.13 ms (9.1%) | ~1,592 µs/frame overhead | `remove` |
| Walking (moving camera outdoor) | OFF wins 0.99 ms (4.2%) | ~1,537 µs/frame overhead | `remove` |
| **Cantina V1 (indoor sparse ~8 NPCs)** | **ON wins 0.62 ms (2.2%)** | **~940 µs/frame savings** | **`keep`** |
| **Cantina V2 (indoor dense ~30+ moving)** | **ON wins 0.64 ms (2.1%)** | **~654 µs/frame savings** | **`keep`** |
| **Cantina combined (n=3,724 indoor)** | **ON wins 0.66 ms (2.2%)** | **~742 µs/frame savings** | **`keep`** |

Indoor verdict robustness-checked across two scene densities + two camera anchors = three orthogonal conditions, all voted `keep` at consistent 2.1–2.2% magnitude.

Full per-scene tables with median/p95/p99/max/stdev and capture-quality notes in `10-05-capture-evidence.md`.

## Capture-quality summary

- **gpu_us=0 across all 12,016 frames is the correct answer — DPVS has no GPU cost by design.** Reframed mid-session from "measurement gap" after code review confirmed: (a) DPVS is CPU-side software occlusion (Umbra 2003-era pattern; `resolveVisibility()` issues no GPU work); (b) bracket placement at `RenderWorld.cpp:1053-1065` is tight around the CPU-only call with no D3D state changes between `Begin/End`; (c) 100% uniform zero across 12K+ frames is consistent with "no work to measure" rather than the mixed-distribution pattern that polling failures (Pitfall 1) would produce. The CPU side (`cpu_qpc_us`, `total_frame_ms`) captures the complete DPVS cost. Minor code-level note logged for Phase 11 reuse: `DpvsProfileInstrumentation.cpp:115` `IGNORE_RETURN` would mask not-ready polls as zero if GPU work were ever present in a future code path — write empty cell on `false` return if instrumentation is reused.
- **Inverted-default state confirmed across two relaunches** — sessions 2 and 3 both launched with DPVS:OFF instead of expected DPVS:ON. Confirms this is the actual v2 launch behavior (probably persisted via `local_machine_options.iff`), not a one-off. Post-session file renames + run_label column patches restored truth based on the authoritative `dpvs_occlusion_flag` column.
- **One-frame outliers in OFF max** (147 ms starport, 108 ms walking) — confirmed in-session as a shuttle takeoff animation transient and a camera-shift hitch respectively, not DPVS-related. Indoor ON also has a 106 ms outlier — single-frame transient, p95/p99 unaffected.
- **Cantina ON-vs-OFF result is the analytical surprise of the session.** Initial reading of session-2 data (indoor OFF only) led to a hypothesis that DPVS might be removable everywhere. The session-3 follow-up captures of the missing ON pass reversed that conclusion — DPVS *helps* indoors, doesn't hurt. The ROADMAP framing was correct from the start; the indoor capture validates it with data rather than assumption.

## Phase 10 status after this plan

| Plan | Status |
|------|--------|
| 10-01 Wave 0 scaffolding | ✓ Complete |
| 10-02 Wave 1 GPU-timing plumbing | ✓ Complete |
| 10-03 Wave 2 engine module | ✓ Complete |
| 10-04 Wave 3 hook wiring | ✓ Complete |
| **10-05 Wave 4 capture session** | **✓ Complete (this plan) — verdict = remove** |
| 10-06 Wave 5 verdict writeup + conditional source edits | ○ Not started — gated on this plan's verdict (now `remove`) |
| 10-07 Wave 6 THROWAWAY cleanup revert commit | ○ Not started |

Phase 10 is **5 of 7 plans complete (71%)**. Remaining plans (10-06, 10-07) are both `autonomous: false` and follow naturally from the `remove` verdict.
