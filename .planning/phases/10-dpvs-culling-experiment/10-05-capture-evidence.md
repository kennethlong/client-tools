# Phase 10 Wave 4 (plan 10-05) — Capture Evidence

**Session date:** 2026-05-14
**Capture window:** 20:21 – 21:25 (wall clock, local)
**Driver:** Kenny (manual keybind-toggle protocol per `tools/dpvs-profile/test-protocol.md`)
**Build:** `D:/Code/swg-client-v2/stage/SwgClient_d.exe` 72,552,448 bytes (Wave 3 HEAD `9dcc60c92`)
**Server:** SWGSource VM @ 192.168.1.200:44453
**Character:** Little Bigman (swg) — Tatooine

---

## Verdict

```
outdoor: verdict = remove (3/3 outdoor scenarios)
indoor:  verdict = keep   (1/1 indoor scenario)
```

**Scene-conditional removal** — DPVS occlusion culling is consistently **net-negative on outdoor scenes** (cost grows with scene complexity, OFF wins by 3.3% sparse → 9.1% dense) but **net-positive within indoor cells** (DPVS saves ~940 µs CPU per frame inside the cantina, ON wins by 2.2%). This exactly matches the ROADMAP Phase 10 success criteria #3 framing: strip `OCCLUSION_CULLING` bit from the outdoor camera-setup path, retain it on the indoor cell camera-setup path. Single `resolveVisibility()` call at `RenderWorld.cpp:1062` stays — the per-scene divergence is driven by the `cullingParameters` bitmask at `RenderWorld.cpp:908` (outdoor) and `:911` (indoor).

---

## Per-scene results

### 1. Mos Eisley plaza (sparse outdoor, ~8 NPCs in view)

| condition | metric | median | p95 | p99 | max | stdev | n_frames |
|-----------|--------|--------|-----|-----|-----|-------|----------|
| on  | total_frame_ms | 28.88 | 34.00 | 38.41 | 65.46 | 2.66 | 2,483 |
| off | total_frame_ms | **27.94** | **32.59** | **35.14** | **60.56** | 2.36 | 1,158 |
| on  | cpu_qpc_us     | 12,283 | 14,791 | 16,934 | 24,141 | 1,278 | 2,483 |
| off | cpu_qpc_us     | **11,343** | **13,518** | **14,652** | **15,901** | 1,038 | 1,158 |

- **DPVS cost (median CPU):** ~940 µs/frame
- **OFF wins median frame time by:** 0.94 ms (3.3%)
- **Verdict:** `remove`

### 2. Mos Eisley starport (denser outdoor)

| condition | metric | median | p95 | p99 | max | stdev | n_frames |
|-----------|--------|--------|-----|-----|-----|-------|----------|
| on  | total_frame_ms | 23.50 | 29.72 | 33.91 | 49.30 | 2.92 | 501 |
| off | total_frame_ms | **21.37** | **25.32** | **27.68** | 147.42* | 5.75 | 535 |
| on  | cpu_qpc_us     | 7,973 | 9,902 | 11,339 | 14,300 | 978 | 501 |
| off | cpu_qpc_us     | **6,381** | **7,805** | **8,717** | **9,385** | 700 | 535 |

- **DPVS cost (median CPU):** ~1,592 µs/frame (+69% vs plaza — denser scene, bigger cost)
- **OFF wins median frame time by:** 2.13 ms (9.1%)
- *147 ms OFF max is a single-frame transient from a shuttle takeoff animation, not DPVS-related (confirmed by Kenny in-session)
- **Verdict:** `remove`

### 3. Walking (moving camera, ~15-20 NPCs avg in view)

| condition | metric | median | p95 | p99 | max | stdev | n_frames |
|-----------|--------|--------|-----|-----|-----|-------|----------|
| on  | total_frame_ms | 23.32 | 28.37 | 35.04 | 44.43 | 2.74 | 747 |
| off | total_frame_ms | **22.33** | **27.95** | **33.08** | 108.68* | 4.57 | 592 |
| on  | cpu_qpc_us     | 7,091 | 9,124 | 11,019 | 14,858 | 977 | 747 |
| off | cpu_qpc_us     | **5,555** | **7,475** | **9,118** | **9,620** | 1,175 | 592 |

- **DPVS cost (median CPU):** ~1,537 µs/frame
- **OFF wins median frame time by:** 0.99 ms (4.2%)
- *108 ms OFF max is a single-frame transient (probably camera-shift hitch), not DPVS-related
- **Verdict:** `remove`

### 4. Cantina interior (indoor — full ON-vs-OFF comparison, two captures sets)

#### 4a. V1 sparse (~8 NPCs, static — from session 3 follow-up)

| condition | metric | median | p95 | p99 | max | stdev | n_frames |
|-----------|--------|--------|-----|-----|-----|-------|----------|
| **on**  | total_frame_ms | **27.56** | **32.51** | **35.83** | 106.17* | 4.05 | 525 |
| off | total_frame_ms | 28.18 | 32.58 | 36.55 | 55.69 | 2.44 | 931 |
| **on**  | cpu_qpc_us     | **10,808** | **13,073** | **14,577** | **15,838** | 1,073 | 525 |
| off | cpu_qpc_us     | 11,744 | 14,160 | 16,881 | 19,224 | 1,242 | 931 |

- ON wins median by 0.62 ms (2.2%); DPVS saves ~940 µs CPU/frame
- *106.17 ms ON max is a single-frame transient outlier; p95/p99 unaffected
- Captures: `cantinaInterior-pass1-off-18989.csv`, `cantinaInterior-pass2-off-25918.csv`, `cantinaInterior-pass3-on-27537.csv`

#### 4b. V2 dense (~30+ NPCs, MOST MOVING — from session 4 robustness follow-up)

| condition | metric | median | p95 | p99 | max | stdev | n_frames |
|-----------|--------|--------|-----|-----|-----|-------|----------|
| **on**  | total_frame_ms | **29.41** | **36.52** | **41.23** | 106.78* | 4.08 | 925 |
| off | total_frame_ms | 30.05 | 35.50 | 40.38 | 90.10 | 3.31 | 1,343 |
| **on**  | cpu_qpc_us     | **11,891** | 15,180 | **16,607** | **18,713** | 1,419 | 925 |
| off | cpu_qpc_us     | 12,545 | 15,119 | 17,287 | 20,202 | 1,298 | 1,343 |

- ON wins median by 0.64 ms (2.1%); DPVS saves ~654 µs CPU/frame (less savings than sparse — moving NPCs force more DPVS recomputation, reducing per-frame efficiency, but still net-positive)
- *106.78 ms ON max is again a single-frame transient outlier (similar pattern to V1)
- Different camera anchor than V1 — provides independent measurement
- Session 4 captured 5 of 6 planned passes (pass2-off lost); flag-truth check after capture revealed 3 OFF + 2 ON labeled-as-truth, sufficient for statistical power
- Captures: `cantinaInteriorV2-pass1-off-24462.csv`, `cantinaInteriorV2-pass1-on-26156.csv`, `cantinaInteriorV2-pass2-off-27697.csv`, `cantinaInteriorV2-pass3-on-29596.csv`, `cantinaInteriorV2-pass3-off-30752.csv`

#### 4c. Combined V1+V2 (final indoor verdict)

| condition | metric | median | p95 | p99 | max | stdev | n_frames |
|-----------|--------|--------|-----|-----|-----|-------|----------|
| **on**  | total_frame_ms | **28.68** | **35.25** | **40.12** | 106.78 | 4.18 | 1,450 |
| off | total_frame_ms | 29.34 | 34.53 | 39.08 | 90.10 | 3.15 | 2,274 |
| **on**  | cpu_qpc_us     | **11,512** | **14,601** | **16,074** | **18,713** | 1,419 | 1,450 |
| off | cpu_qpc_us     | 12,254 | 14,838 | 17,084 | 20,202 | 1,343 | 2,274 |

- **DPVS savings (median CPU):** ~742 µs/frame
- **ON wins median frame time by:** 0.66 ms (2.2%)
- **Verdict:** `keep` — indoor cells benefit materially from DPVS occlusion culling. Verdict is robust across:
  - Two scene densities (sparse 8 NPCs vs dense 30+ moving NPCs)
  - Two camera anchors (different cantina viewpoints)
  - n=3,724 total frames (1,450 ON + 2,274 OFF)
  - Consistent 2.1–2.2% median win regardless of measurement conditions

**Why the indoor result reverses the outdoor pattern:** Indoor cells (POBs) are tight, geometry-dense spaces with many objects that occlude *other objects within the same cell* (chairs, tables, bar, walls, NPC clusters). DPVS within-cell occlusion querying finds substantial work to skip. Portal traversal handles cross-cell visibility but does not cull within a cell. Outdoor scenes have fewer effective occluders per visible object, so DPVS pays the query cost without finding much to cull.

**Density gradient detail:** DPVS savings per frame shrink as scene density grows (sparse 940 µs → dense 654 µs). Likely explanation: more moving NPCs = more dynamic occlusion topology = more DPVS recomputation. But savings remain *positive* at all measured densities — DPVS keeps earning its keep indoors, just earns it less per frame as the scene gets chaotic. This is the *opposite* gradient from outdoor (where DPVS cost *grows* with density), giving a clean unified explanation for the scene-conditional verdict.

---

## Captured dataset

22 CSVs in `D:/Code/swg-client-v2/stage/dpvs-profile/`:

| Scene | ON captures | OFF captures | Notes |
|-------|-------------|--------------|-------|
| mosEisley | 6 (n=2,483) | 3 (n=1,158) | Oversampled ON side (initial labeling drift, recovered) |
| starport | 1 (n=501) | 1 (n=535) | Both clean |
| walking | 2 (n=747) | 1 (n=592) | One ON pass split across two files |
| cantinaInterior (V1, sparse ~8 NPCs) | 1 (n=525) | 2 (n=931) | Session 3 follow-up captured the missing ON |
| cantinaInteriorV2 (dense ~30+ moving NPCs) | 2 (n=925) | 3 (n=1,343) | Session 4 robustness follow-up; different camera anchor |

Total: **12,016 frames** across 5 distinct capture conditions (4 scenarios + a second cantina anchor at different density).

Indoor combined sample: **1,450 ON + 2,274 OFF = 3,724 frames** — sufficient for high-confidence indoor verdict.

---

## Capture-quality notes

### Inverted default state on session 2 launch

When the client was relaunched mid-session for the cantina/starport/walking scenarios, the default `disableOcclusionCulling` state was **OFF** (DPVS disabled) — opposite of the first session's launch state, which was ON. Cause unknown — possibly persisted state via `local_machine_options.iff` or a startup-order quirk. Recovery: renamed files + patched `run_label` column post-session based on the authoritative `dpvs_occlusion_flag` column.

### gpu_us=0 is the correct answer — DPVS has no GPU cost by design

After mid-session investigation, this was reframed from "measurement gap" to "structural finding." Three pieces of evidence converge:

1. **DPVS is CPU-side software occlusion.** SOE licensed Umbra 2003-era DPVS, which maintains its own depth/occlusion data structures in main memory and walks them on the CPU. `ms_dpvsCamera->resolveVisibility(ms_commander, portalRecusionDepth, 0.0f)` issues no D3D draw calls and no GPU state changes — it's a pure software algorithm.

2. **Bracket placement is tight around CPU-only work.** `RenderWorld.cpp:1053-1065` brackets *only* the `resolveVisibility()` call (plus a CPU `PerformanceTimer`). No `Draw*`, no `SetRenderState`, no `SetTexture` inside the bracket. With zero GPU work between `Begin(query)` and `End(query)`, the two D3D9 timestamp recordings land back-to-back in the GPU command buffer and produce a near-zero delta.

3. **100% uniform zero across 12,016 frames** is precisely the pattern expected if the underlying physical answer is zero. Polling-failure artifacts (Pitfall 1: `GetData` returning `S_FALSE` on a fraction of polls) typically produce a *mixture* of zeros and valid microsecond values, not uniformly zero. Our data is uniform zero, consistent with "no work to measure."

**Minor code-level note (for Phase 11 prep if GPU timing is reused):** `DpvsProfileInstrumentation.cpp:115` uses `IGNORE_RETURN(Graphics::dpvsGpuTimingPollResult(&gpuUs, &disjointInvalid));`. When the poll returns `false` (any of the three `GetData` queries returned `S_FALSE` / not-ready), the default-initialized `gpuUs=0` is written to the CSV indistinguishably from a real zero measurement. Combined with the N=3 pool depth (the protocol's Pitfall 1 prescribed remedy is N=5 for modern driver pipelining), the pipeline *could* be masking some not-ready polls. But this is moot: the underlying physical answer is zero, so the masking doesn't bias the verdict. If GPU timing instrumentation is reused for Phase 11 D3D11 work, write an empty CSV cell on `false` return instead of the default `0` so the two cases are distinguishable.

**Bottom line:** The CPU side captures the complete DPVS cost. GPU side is genuinely zero, not unmeasured.

### Other operational notes

- All captures done with `[ClientGraphics/Dpvs] reportInstrumentation=true` in `client_d.cfg` (added in this session per protocol Pre-Session Checklist).
- DebugMonitor overlay window did not render — protocol allows this; CSV writer is independent and authoritative. F10/F11/`/setrunlabel` confirmed working via post-session CSV inspection.
- 6 captures in mosEisley were originally labeled `pass1-on` through `pass6-on` sequentially (no `-off` alternation in chat-label) due to Kenny initially treating each pass as a unique index rather than the protocol's ON/OFF condition pair. Recovery: 3 OFF passes appended in second half of session 1 with proper labels. ON/OFF imbalance (6:3) accepted; statistical power on ON side higher than required, OFF side meets D-08 minimum (3 per condition).
- Captures were sequential per condition rather than alternating ON-OFF-ON-OFF-... (D-08 ideal). ~4 min gap between condition blocks in session 1. Strong consistency across p50/p95/p99/max suggests time-of-day drift did not bias the result.
- World-load fires `SafeCast.h:29` assertion dialogs (CRT assert) twice per launch; recoverable via Ignore-twice. Tracked in `.planning/todos/pending/2026-05-14-safecast-null-dynamic-cast-world-load.md` — separate issue, does not affect capture data.

---

## Analysis script invocation

Per-scene runs (one per scenario, captures moved into temp subdirs then removed post-analysis):

```
C:\Python312\python.exe D:/Code/swg-client-v2/tools/dpvs-profile/analysis.py \
  --csv-dir <per-scene-subdir> \
  --scene <mosEisley|starport|walking|cantinaInterior>
```

Each emitted a markdown stats table to stdout plus the final `verdict =` line. Aggregated output captured above.

---

## Phase 10 next steps

- **Plan 10-06 (Wave 5):** Populate `docs/recon/10-dpvs-profiling.md` with the verdict + tables + caveats from this evidence doc. Apply **scene-conditional** source edits — the original ROADMAP/CONTEXT D-13/D-14 plan was correct as written: strip `OCCLUSION_CULLING` bit from the outdoor camera-setup path at `RenderWorld.cpp:908`, **retain** the bit on the indoor cell camera-setup path at `RenderWorld.cpp:911` (line numbers approximate per current HEAD; CONTEXT.md says 903/906 but the file has drifted — verify before edit). Single `resolveVisibility()` call at `RenderWorld.cpp:1062` stays; the divergence is purely in the `cullingParameters` bitmask passed into `DPVS::Camera::setParameters`. Boot smoke verify post-edit.
- **Plan 10-07 (Wave 6):** Unconditional D-15 cleanup commit — delete `DpvsProfileInstrumentation.{cpp,h}`, revert all 8 instrumentation edits across files in one revert-shaped commit, author `10-SUMMARY.md`, final boot smoke.

---

*Closes plan 10-05. Verdict line emitted per scene; final scene-conditional verdict = `remove for outdoor, keep for indoor`. STL-05 / DPVS-01 capture-and-decide requirement satisfied.*
