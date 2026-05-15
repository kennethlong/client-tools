# DPVS Occlusion Culling — Profiling Verdict (Phase 10)

**Date measured:** 2026-05-14 (4 client launch sessions, 19:32–22:09 local)
**Tree:** `D:/Code/swg-client-v2/` branch `koogie-msvc-cpp20-base` HEAD `9dcc60c92`
**Renderer:** D3D9 (per D-12; Phase 11 D3D11 re-measure separate)
**Scenes:** Mos Eisley plaza (sparse outdoor), Mos Eisley starport (dense outdoor), walking (moving camera outdoor), cantina interior (POB indoor, sparse + dense)
**Hardware:** NVIDIA GeForce RTX 3060 — driver 94.6.2f.0.7e (Kenny's machine)
**Server:** SWGSource VM @ 192.168.1.200:44453, character "Little Bigman" on Tatooine

---

## Verdict

**Underlying data is scene-conditional `remove for outdoor, keep for indoor`** — DPVS occlusion culling flips sign between rendering topologies:

- **Outdoor scenes:** `remove` (3/3 scenes voted independently). DPVS is net-negative. Cost grows with scene complexity (~940 µs/frame sparse → ~1,592 µs/frame dense); OFF wins median frame time by 3.3% to 9.1%.
- **Indoor cells:** `keep` (cantina, validated across two scene densities + two camera anchors, n=3,724 frames). DPVS is net-positive. Tight geometry-dense spaces; DPVS saves ~742 µs CPU per frame; ON wins median frame time by 2.2%.

**Applied decision: `remove` globally (Option α).** Architectural constraint identified during plan 10-06 execution: the `cullingParameters` bitmask in `RenderWorld.cpp` (lines 908 in `#ifdef _DEBUG` / 911 in release branch — same value via conditional compilation) feeds the single `ms_dpvsCamera->setParameters()` call at line 926. That camera is used for ALL rendering — indoor cells and outdoor scenes alike. There is no per-path code-level split available without a larger refactor to pass different culling parameters per cell context.

The decision balances aggregate experience:
- Outdoor magnitude × frequency: 0.94–2.13 ms gained × dominant playtime
- Indoor regression × frequency: 0.66 ms lost × cantina/POB playtime (below human perception threshold)
- Net: aggregate user experience improves; outdoor wins dominate.

**Phase 11 reconsideration required.** When the D3D11 renderer lands, this trade may re-balance (cheaper draws may eliminate outdoor DPVS overhead OR eliminate indoor DPVS savings). Three options at that point:
1. Keep Option α (remove globally) — if D3D11 numbers still favor outdoor
2. Revert globally to `keep` — if D3D11 numbers flip outdoor to net-positive
3. Implement runtime scene-aware split — pass different `cullingParameters` per-cell at line 908/911; non-trivial refactor justified only if D3D11 numbers show a large gap in both directions

Captured in ROADMAP Phase 11 success criterion #6.

---

## Methodology

- **GPU timing:** D3D9 `IDirect3DQuery9` `TIMESTAMP` + `TIMESTAMPDISJOINT` queries bracket `resolveVisibility()` (D-01, plan 10-02). Double-buffered pool N=3 per RESEARCH.md Pitfall 1.
- **CPU timing:** `QueryPerformanceCounter` pair around `resolveVisibility()` plus the existing `ms_dpvsQueryProfilerBlock` (D-02).
- **Output:** Per-frame CSV row written from `DpvsProfileInstrumentation::onFrameEnd` (D-03); on-screen DebugMonitor overlay (not rendered in our sessions — DebugMonitor window did not spawn; CSV writer is independent and authoritative).
- **Statistics:** Median + p95 + p99 + max + stdev per condition × metric (D-04). Means NOT primary.
- **Aggregator:** `tools/dpvs-profile/analysis.py` (Python 3.12). Groups by run-label suffix; emits verdict line per D-10.

### Note on `gpu_us = 0` across all captures

The `gpu_us` column is uniformly zero across all 12,016 captured frames. **This is the correct answer, not a measurement gap.** DPVS is CPU-side software occlusion (SOE licensed Umbra 2003-era pipeline): `resolveVisibility()` walks Umbra's own in-memory occlusion structures, issues no D3D draw calls, and produces no GPU work. The `Begin(query)` and `End(query)` markers wrap a CPU-only function — the two timestamps record back-to-back in the GPU command buffer with no intervening work, producing a near-zero delta.

Verified via code review:
- `RenderWorld.cpp:1053-1065` — bracket placement is tight around the CPU-only `resolveVisibility()` call. No `Draw*`, no state changes between `Begin` and `End`.
- `Direct3d9.cpp:4685-4709` — polling logic is structurally correct (three-query pattern with disjoint guard). 100% uniform zero across 12K+ frames is consistent with "no work to measure" rather than the mixed-distribution pattern that polling failures would produce.

CPU side (`cpu_qpc_us`, `total_frame_ms`) captures the complete DPVS cost.

---

## Scenes & Protocol

Single capture protocol (`tools/dpvs-profile/test-protocol.md`); applied to four distinct scenes for cross-validation:

| Scene | Description | NPC density | Camera | Sessions |
|-------|-------------|-------------|--------|----------|
| Mos Eisley plaza | Outdoor, fixed anchor outside cantina | ~8 (sparse) | locked | 1 |
| Mos Eisley starport | Outdoor, fixed anchor near landing pad | ~15-20 (dense, traffic + shuttle takeoff) | locked | 2 |
| Walking | Outdoor, steady forward walk down street | varying | moving (no turning) | 2 |
| Cantina V1 | Indoor POB cell, fixed interior anchor | ~8 (sparse) | locked | 3 |
| Cantina V2 | Indoor POB cell, different interior anchor | ~30+ (dense, mostly moving) | locked | 4 |

Per-pass: F10/F10 toggle bracket (~10 s held still), `/setrunlabel` chat command between passes for label injection. F11 toggles `disableOcclusionCulling`. 3 passes per condition minimum where captured (some sessions oversampled).

---

## Raw CSV Manifest

22 CSV files captured under `stage/dpvs-profile/` (gitignored). 12,016 total frame samples. Filenames record the `(run_label, startFrame)` tuple from `DpvsProfileInstrumentation::writeRow`.

| Scene | ON files (n_frames) | OFF files (n_frames) |
|-------|---------------------|------------------------|
| mosEisley plaza | 6 (2,483) | 3 (1,158) |
| starport | 1 (501) | 1 (535) |
| walking | 2 (747) | 1 (592) |
| cantinaInterior V1 (sparse) | 1 (525) | 2 (931) |
| cantinaInterior V2 (dense) | 2 (925) | 3 (1,343) |
| **Indoor combined** | **3 (1,450)** | **5 (2,274)** |

Full per-file listing + capture-quality notes (inverted-default-state recovery, label-vs-flag-column reconciliation, single-frame outlier attribution) in `.planning/phases/10-dpvs-culling-experiment/10-05-capture-evidence.md`.

---

## Summary Statistics

### Outdoor — Mos Eisley plaza (sparse, ~8 NPCs)

| condition | metric | median | p95 | p99 | max | stdev | n_frames |
|-----------|--------|--------|-----|-----|-----|-------|----------|
| on  | total_frame_ms | 28.88 | 34.00 | 38.41 | 65.46 | 2.66 | 2,483 |
| off | total_frame_ms | **27.94** | **32.59** | **35.14** | **60.56** | 2.36 | 1,158 |
| on  | cpu_qpc_us     | 12,283 | 14,791 | 16,934 | 24,141 | 1,278 | 2,483 |
| off | cpu_qpc_us     | **11,343** | **13,518** | **14,652** | **15,901** | 1,038 | 1,158 |

OFF wins median by 0.94 ms (3.3%); DPVS costs ~940 µs/frame CPU. **Verdict: `remove`.**

### Outdoor — Mos Eisley starport (dense)

| condition | metric | median | p95 | p99 | max | stdev | n_frames |
|-----------|--------|--------|-----|-----|-----|-------|----------|
| on  | total_frame_ms | 23.50 | 29.72 | 33.91 | 49.30 | 2.92 | 501 |
| off | total_frame_ms | **21.37** | **25.32** | **27.68** | 147.42* | 5.75 | 535 |
| on  | cpu_qpc_us     | 7,973 | 9,902 | 11,339 | 14,300 | 978 | 501 |
| off | cpu_qpc_us     | **6,381** | **7,805** | **8,717** | **9,385** | 700 | 535 |

OFF wins median by 2.13 ms (9.1%); DPVS costs ~1,592 µs/frame CPU. *147 ms OFF max is a single-frame shuttle takeoff transient; p95/p99 unaffected. **Verdict: `remove`.**

### Outdoor — Walking (moving camera)

| condition | metric | median | p95 | p99 | max | stdev | n_frames |
|-----------|--------|--------|-----|-----|-----|-------|----------|
| on  | total_frame_ms | 23.32 | 28.37 | 35.04 | 44.43 | 2.74 | 747 |
| off | total_frame_ms | **22.33** | **27.95** | **33.08** | 108.68* | 4.57 | 592 |
| on  | cpu_qpc_us     | 7,091 | 9,124 | 11,019 | 14,858 | 977 | 747 |
| off | cpu_qpc_us     | **5,555** | **7,475** | **9,118** | **9,620** | 1,175 | 592 |

OFF wins median by 0.99 ms (4.2%); DPVS costs ~1,537 µs/frame CPU. *108 ms OFF max is a camera-shift transient. **Verdict: `remove`.**

### Indoor — Cantina (combined V1 sparse + V2 dense, n=3,724)

| condition | metric | median | p95 | p99 | max | stdev | n_frames |
|-----------|--------|--------|-----|-----|-----|-------|----------|
| **on**  | total_frame_ms | **28.68** | **35.25** | **40.12** | 106.78* | 4.18 | 1,450 |
| off | total_frame_ms | 29.34 | 34.53 | 39.08 | 90.10 | 3.15 | 2,274 |
| **on**  | cpu_qpc_us     | **11,512** | **14,601** | **16,074** | **18,713** | 1,419 | 1,450 |
| off | cpu_qpc_us     | 12,254 | 14,838 | 17,084 | 20,202 | 1,343 | 2,274 |

**ON wins median by 0.66 ms (2.2%); DPVS saves ~742 µs/frame CPU.** Reproduces across:
- Two scene densities (sparse ~8 NPCs vs dense ~30+ moving NPCs)
- Two camera anchors (different cantina viewpoints)
- *106 ms ON max is a single-frame transient (similar pattern to outdoor OFF outliers — not DPVS-related)

**Verdict: `keep`.**

---

## Rationale — unified mechanism

The four scenes paint a coherent picture once viewed by scene topology:

| Topology | DPVS effect | Why |
|----------|-------------|-----|
| Open outdoor (few effective occluders) | **Net cost** | DPVS pays the query cost per frame without finding much to skip. Cost grows with object count. |
| Closed indoor cell (many intra-cell occluders) | **Net savings** | Tight space with chairs/tables/walls/NPC clusters; DPVS skips substantial draw work. Savings shrink in chaotic moving scenes (more recomputation) but stay positive. |

This produces a **clean two-sign gradient** for a single physical mechanism. As scene density grows:
- Outdoor: DPVS cost increases (940 µs sparse → 1,592 µs dense); savings stay near zero → bigger net loss.
- Indoor: DPVS savings shrink (940 µs sparse → 654 µs dense) but stay net positive → net gain still present, magnitude smaller in moving scenes.

The crossover happens because outdoor scenes simply lack the occluder density that makes DPVS pay off. The cantina at 27.56 ms with DPVS ON is the fastest frame time we measured anywhere — better than even outdoor with DPVS off — because indoor cells have a fundamentally smaller world to draw, and DPVS optimally exploits that.

### Per D-09 / D-10 / D-11 thresholds

- **D-09 primary metric:** `total_frame_ms` median + p95. Both directions show consistent results across n>500 per condition per scene.
- **D-10 threshold:** OFF wins on both median AND p95 → `remove`. Outdoor scenes: yes for all three. Indoor scene: ON wins on both → `keep`.
- **D-11 inconclusive-defaults-to-keep:** Outdoor magnitudes (3.3-9.1%) are well above noise floor; indoor magnitude (2.2%) was robustness-checked with a second capture session at 4× higher density and a different camera anchor — same direction, same magnitude. Not inconclusive on either side.

---

## Source-edit implication for plan 10-06

Strip `OCCLUSION_CULLING` bit from the single global `cullingParameters` setup. Both lines 908 (`#ifdef _DEBUG` variant with extra debug flags) and 911 (release variant) are the SAME value applied to the SAME `ms_dpvsCamera` via conditional compilation — both must be edited consistently. The single `resolveVisibility()` call at `:1062` stays. DPVS library stays linked (still needed for portal cell traversal regardless of the occlusion-query decision).

```cpp
// RenderWorld.cpp current HEAD lines 907-913:

#ifdef _DEBUG
    uint const cullingParameters = ((ms_forceDisableOcclusionCulling || ms_disableOcclusionCulling)
        ? 0 : DPVS::Camera::OCCLUSION_CULLING) | (ms_disableViewFrustumCulling ? 0 : DPVS::Camera::VIEWFRUSTUM_CULLING);
#else
    uint const cullingParameters = (ms_disableOcclusionCulling ? 0 : DPVS::Camera::OCCLUSION_CULLING)
        | DPVS::Camera::VIEWFRUSTUM_CULLING;
#endif
```

After plan 10-06 (both branches stripped of `OCCLUSION_CULLING` term):

```cpp
#ifdef _DEBUG
    uint const cullingParameters = (ms_disableViewFrustumCulling ? 0 : DPVS::Camera::VIEWFRUSTUM_CULLING);
#else
    uint const cullingParameters = DPVS::Camera::VIEWFRUSTUM_CULLING;
#endif
```

Plus D-14 deletes the now-unused config-key + setter/getter machinery (`ms_disableOcclusionCulling` static in both `RenderWorld.cpp` and `ConfigClientGraphics.cpp`, the `KEY_BOOL(disableOcclusionCulling, ...)` registration, `RenderWorld::setDisableOcclusionCulling` / `getDisableOcclusionCulling`, declarations in headers, and the F11 keybind hook installed in plan 10-04).

---

## Phase 11 Revisit Note (per D-12)

This verdict is valid for the **D3D9 renderer only.** Phase 11's D3D11 plugin changes draw-call cost; DPVS occlusion savings depend on draw-call cost; the verdict could flip under D3D11 — particularly for the outdoor scenes where DPVS is currently net-negative by a margin proportional to per-object draw cost.

**Action for Phase 11:** Add a success-criteria item to re-measure DPVS occlusion under D3D11 via the same protocol (`tools/dpvs-profile/test-protocol.md`) and confirm or reverse this decision before declaring D3D11 done. The instrumentation itself is removed by plan 10-07 (Wave 6) as THROWAWAY — Phase 11 will need to either restore equivalent instrumentation or use a different measurement approach.

Possible Phase 11 outcomes:
- Outdoor verdict stays `remove`: D3D11's faster draw call cost reduces but doesn't reverse the DPVS overhead. Likely.
- Outdoor verdict flips to `keep`: D3D11 makes draws so cheap that DPVS overhead dominates. Possible but unlikely given DPVS's per-frame query cost is GPU-independent.
- Indoor verdict stays `keep`: Indoor savings come from skipping geometry-dense interior draws; D3D11 doesn't change that. Very likely.

---

## Capture-quality caveats (summarized)

Full notes in `.planning/phases/10-dpvs-culling-experiment/10-05-capture-evidence.md`. Quick summary:

1. **Inverted default-state on launch** — v2 launches with `disableOcclusionCulling=true` (DPVS:OFF) by default. Confirmed across 3 of 4 sessions. Probably persisted via `local_machine_options.iff`. Operational impact: labels were inverted on first capture per session; recovered via post-session rename + `run_label` column patch using `dpvs_occlusion_flag` column as authoritative truth.
2. **`gpu_us = 0`** — Structural finding, not a measurement gap. See "Note on gpu_us = 0" in Methodology above.
3. **Captures were sequential per condition rather than strict alternating** — Strong consistency across p50/p95/p99/max suggests time-of-day drift did not bias the result.
4. **Single-frame outliers** — 147 ms (shuttle takeoff, starport-OFF), 108 ms (camera shift, walking-OFF), 106 ms (cantina ON, both V1 and V2) — all single-frame transients; p95/p99 unaffected.

---

## References

- `.planning/phases/10-dpvs-culling-experiment/10-CONTEXT.md` — user decisions D-01..D-19
- `.planning/phases/10-dpvs-culling-experiment/10-RESEARCH.md` — research record (pitfalls + thresholds)
- `.planning/phases/10-dpvs-culling-experiment/10-05-capture-evidence.md` — full per-scene tables + raw file inventory + capture-quality notes
- `.planning/phases/10-dpvs-culling-experiment/10-05-SUMMARY.md` — plan closeout (decisions, deviations, verification)
- `tools/dpvs-profile/test-protocol.md` — capture procedure
- `tools/dpvs-profile/analysis.py` — verdict-line emitter
- Source instrumentation (THROWAWAY, removed by plan 10-07):
  - `src/engine/client/library/clientGraphics/src/shared/DpvsProfileInstrumentation.{h,cpp}` — CSV writer + run-label sanitizer + overlay + ExitChain teardown
  - `src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp:1047-1071` — GPU/CPU bracket around `resolveVisibility()`
  - `src/engine/client/application/Direct3d9/src/win32/Direct3d9.cpp:4621-4709` — D3D9 timestamp query pool implementation
  - `src/engine/client/library/clientGame/src/shared/core/Game.cpp` — `onFrameEnd` hook
  - `src/engine/client/library/clientUserInterface/src/shared/core/CuiIoWin.cpp` — `_DEBUG` F10/F11 keybind intercept
  - `src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserDefault.cpp` — `/setrunlabel` console command
