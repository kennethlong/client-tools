# DPVS Occlusion Culling — D3D11 Re-measure Verdict (Phase 23)

**Date measured:** 2026-06-12 (live capture session, ~11:50–12:10 local)
**Tree:** `D:/Code/swg-client-v2/` branch `koogie-msvc-cpp20-base`
**Renderer:** **D3D11 (`rasterMajor=11`, gl11)** — the Phase 11 D3D11 re-measure that D-12 / Phase 10's "Phase 11 Revisit Note" deferred.
**Build:** Debug (`SwgClient_d.exe`), reads `stage/client_d.cfg`. Debug relative A/B per 23-RESEARCH Open Question 1 — the verdict turns on the relative ON-vs-OFF delta, which Debug preserves; the F10/F11 toggle + occlusion re-gate are `_DEBUG`-only.
**Scenes:** Mos Eisley plaza (outdoor, NPC/jawa density in view) + cantina interior (POB indoor).
**Server:** SWGSource VM @ 192.168.1.200:44453, live Tatooine character.
**Cross-reference:** This is the D3D11 counterpart to the D3D9 verdict in [`docs/recon/10-dpvs-profiling.md`](10-dpvs-profiling.md). The Phase 10 baseline is **its summary tables** (the raw Phase 10 CSVs were gitignored; the old Phase 10 CSVs are archived under `stage/dpvs-profile-phase10-archive/`).

---

## Verdict

The locked `tools/dpvs-profile/analysis.py` D-10/D-11 rule emits, per scene:

| Scene | Topology | analysis.py verdict | vs Phase 10 (D3D9) |
|-------|----------|---------------------|--------------------|
| Mos Eisley plaza | Outdoor | **`verdict = keep`** | Phase 10 said `remove` → **REVISED** |
| Cantina | Indoor | **`verdict = remove`** | Phase 10 said `keep` → **REVISED** |

**Both signs flipped under D3D11.** Outdoor occlusion culling is now net-*positive* (keep it ON); indoor occlusion culling is now net-*negative* (the portal system already culls; the occlusion query is dead overhead). This is the exact "the trade may re-balance under D3D11" outcome that Phase 10 flagged as requiring reconsideration — and it re-balanced in *both* directions at once.

**Effect on the shipped Option α (`remove` globally) decision:** Option α removes the `OCCLUSION_CULLING` bit from the single global `cullingParameters` in `RenderWorld.cpp` for both branches. Under D3D11:
- For **outdoor** (the scene class Option α was optimizing for — "outdoor wins dominate"), removing occlusion is now the *wrong* call: ON wins. Option α's central premise is **revised** under D3D11.
- For **indoor**, Option α's `remove` *happens to match* the D3D11 measurement (indoor now also prefers OFF).

So Option α (`remove` globally) is, under D3D11, **right for indoor but wrong for outdoor** — the inverse of the Phase 10 per-scene split that motivated it. See §Rationale.

**No code change is made by this measurement phase.** Per the plan's acceptance criteria and threat T-23-02, this phase writes only this doc + the capture protocol; the shipped Option α `#else` branch in `RenderWorld.cpp` is left untouched (no `RenderWorld.cpp` in this plan's diff). The verdict is recorded as a decision artifact; acting on it (a scene-aware `cullingParameters` split, or reverting the outdoor removal) is a follow-up for a future phase.

---

## Methodology

- **Renderer:** D3D11 `rasterMajor=11` (gl11), Debug build. The DPVS occlusion A/B is gated on the surviving `ms_forceDisableOcclusionCulling` `DebugFlag`, toggled live by **F11 → `RenderWorld::toggleForceDisableOcclusionCulling()`** (the Phase 10 `setDisableOcclusionCulling` accessor was deleted by plan 10-06; only the force-disable flag survived the Option α cleanup).
- **CPU timing:** `QueryPerformanceCounter` pair bracketing `resolveVisibility()` (`cpu_qpc_us`), plus the per-frame `total_frame_ms`. **Engine timers, not RenderDoc/PIX** — DPVS is a CPU-side software occlusion pass; an external GPU profiler measures nothing here.
- **GPU timing:** `gpu_us` is GPU-stripped to a structural 0 in the D3D11 instrumentation (see §"Note on `gpu_us = 0`").
- **Statistics:** median + p95 + p99 + max + stdev per condition × metric (D-04). Means are NOT primary.
- **Primary metric (D-09):** `total_frame_ms` median + p95.
- **Verdict rule (D-10 / D-11):** `off.median ≤ on.median` **AND** `off.p95 ≤ on.p95` → `remove`; any regression on either, or missing/mixed data → `keep`.
- **Aggregator:** unchanged `tools/dpvs-profile/analysis.py` (Python 3, stdlib only). It pools every CSV in `--csv-dir` by the `-on` / `-off` run-label suffix, so each scene was analyzed separately by staging that scene's 6 CSVs (3 ON + 3 OFF) into its own directory.
- **Capture rhythm:** ≥3 passes per condition per scene, alternating ON-OFF, F11 between every pass, ~15 s still-camera hold per pass, `/setrunlabel <scene>-pass<i>-{on,off}`. Clean `/quit` (no taskkill) so the last CSV is not truncated. CSVs land in `stage/dpvs-profile/`.

### Capture-quality / instrumentation notes (verified in code, not data problems)

- **`dpvs_occlusion_flag` is authoritative** over the run-label. All 12 CSVs are 100% flag-pure: every `-on` pass = flag `0` (occlusion ON), every `-off` pass = flag `1` (occlusion OFF, F11). The `0 = ON / 1 = OFF` semantics are documented at `DpvsProfileInstrumentation.cpp` `writeRow` (~line 329). No inverted-default trap fired this session.
- **`profiler_dpvs_us = 0` across all rows** — the profiler counter was not fed in this session. It does **not** drive the verdict; the primary metric per D-09 is `total_frame_ms` (fully populated), and `cpu_qpc_us` (the `resolveVisibility()` QPC bracket) is populated too.
- **`draw_call_count = 0` across all rows** — `Graphics::getRenderedVerticesPointsLinesTrianglesCalls` returns 0 calls under D3D11. Informational only; not a verdict input.
- **Build-staleness caveat (resolved before capture):** the initial Plan-02 exe carried a stale `clientGraphics.lib` (`SetupClientGraphics.obj` predated the install-hook commit), so `DpvsProfileInstrumentation::install()` was absent. `clientGraphics` was rebuilt and `SwgClient_d.exe` relinked before capture — build outputs only, no source changes.

### Note on `gpu_us = 0` across all captures

`gpu_us` is uniformly `0` across every captured D3D11 frame. **This is the correct answer, not a measurement gap** — exactly as in the Phase 10 D3D9 verdict. DPVS is CPU-side software occlusion: `resolveVisibility()` walks Umbra's in-memory occlusion structures, issues no draw calls, and produces no GPU work. In the D3D11 instrumentation the GPU timestamp path was deliberately stripped to a structural `0` (Phase 23-01: "gpu_us GPU-stripped to structural 0; DPVS is CPU-only"), since there is no GPU work to bracket. The complete DPVS cost is captured CPU-side by `cpu_qpc_us` and `total_frame_ms`.

---

## Summary Statistics

Per-scene tables, pasted from `analysis.py` markdown output. Each scene = 3 ON + 3 OFF passes.

### Outdoor — Mos Eisley plaza (live, jawas/NPCs in view)

| condition | metric            | median | p95 | p99 | max | stdev | n_frames |
|-----------|-------------------|--------|-----|-----|-----|-------|----------|
| on        | total_frame_ms    | **34.12** | **41.27** | 45.18 | 50.80 | 3.41 | 1300 |
| off       | total_frame_ms    | 34.88 | 42.33 | 46.96 | 54.38 | 3.57 | 1266 |
| on        | cpu_qpc_us        | **18,851.50** | **23,606.85** | 26,112.25 | 30,047.00 | 2,117.57 | 1300 |
| off       | cpu_qpc_us        | 19,592.00 | 24,447.40 | 27,600.21 | 30,206.00 | 2,197.50 | 1266 |
| on        | gpu_us            | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 1300 |
| off       | gpu_us            | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 1266 |
| on        | profiler_dpvs_us  | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 1300 |
| off       | profiler_dpvs_us  | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 1266 |

**ON wins median by 0.76 ms (2.2%) and p95 by 1.06 ms.** Visible-object count: ON culls to a median **359** objects vs OFF's **~500** — occlusion culling removes ~140 objects/frame outdoor and the draw savings now outweigh the query cost. `off.median > on.median` ⇒ **`verdict = keep`.** (Phase 10 D3D9 outdoor was `remove`.)

### Indoor — Cantina (POB interior)

| condition | metric            | median | p95 | p99 | max | stdev | n_frames |
|-----------|-------------------|--------|-----|-----|-----|-------|----------|
| on        | total_frame_ms    | 38.49 | 45.84 | 51.88 | 63.42 | 3.86 | 1153 |
| off       | total_frame_ms    | **37.79** | **44.87** | **49.06** | **55.73** | 3.55 | 1177 |
| on        | cpu_qpc_us        | 22,480.00 | 27,453.20 | 32,329.54 | 37,211.00 | 2,488.18 | 1153 |
| off       | cpu_qpc_us        | **21,744.00** | **26,365.30** | **29,341.40** | **32,808.00** | 2,302.72 | 1177 |
| on        | gpu_us            | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 1153 |
| off       | gpu_us            | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 1177 |
| on        | profiler_dpvs_us  | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 1153 |
| off       | profiler_dpvs_us  | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 1177 |

**OFF wins median by 0.70 ms (1.8%) AND p95 by 0.97 ms.** Visible-object count is **identical (443) in both conditions** — the POB portal system has already bound the visible set, so the occlusion query finds nothing extra to cull and is pure CPU overhead. `off.median ≤ on.median` AND `off.p95 ≤ on.p95` ⇒ **`verdict = remove`.** (Phase 10 D3D9 indoor was `keep`.)

---

## Rationale — both signs flipped, and why

The single mechanistic key is the **`visible_object_count` column**, which the Phase 10 protocol did not lean on:

| Topology | ON vs OFF visible count | What DPVS occlusion does | D3D11 verdict | Phase 10 (D3D9) |
|----------|-------------------------|--------------------------|---------------|-----------------|
| Outdoor (Mos Eisley plaza) | 359 vs ~500 (**culls ~140**) | Real work: removes ~140 objects' worth of draws | **keep** | remove |
| Indoor (cantina POB cell) | 443 vs 443 (**culls nothing**) | Dead overhead: portals already bound the set | **remove** | keep |

**Why outdoor flipped `remove → keep`:** In Phase 10 (D3D9) the per-object draw cost was cheap enough that culling ~140 objects didn't recover the per-frame occlusion-query cost, so OFF won (`remove`). Under D3D11 the relative balance shifted: ON now wins by 2.2% median / p95. The objects DPVS removes outdoor are worth more than the query that finds them. This is precisely the Phase 10 "outdoor verdict flips to keep — possible but unlikely" branch, and it landed.

**Why indoor flipped `keep → remove`:** In a POB cell the portal/cell traversal already determines the visible set — here, an identical 443 objects with occlusion ON or OFF. The occlusion *query* therefore culls nothing additional indoor; it is pure CPU cost. Under D3D9 the measured indoor delta favored ON by 2.2% (Phase 10 attributed savings to skipping geometry-dense interior draws); under D3D11 the identical-visible-set captures show the query is now net overhead, and OFF wins by 1.8% median / 0.97 ms p95.

**Net effect on Option α.** Option α (`remove` globally) was justified in Phase 10 as "outdoor wins dominate playtime; accept the small indoor regression." Under D3D11 that justification is **inverted**:
- Globally removing occlusion now *hurts* the dominant outdoor case (ON wins outdoor) — the central premise of Option α is **REVISED**.
- It *helps* the indoor case (OFF wins indoor) — so Option α's `remove` is coincidentally correct for cantina, but for the opposite reason than Phase 10 assumed.

The clean per-scene answer under D3D11 is the **opposite split** from Phase 10: keep occlusion outdoor, remove it indoor. As Phase 10 noted (its option 3), a code-level per-cell split is non-trivial — the single global `cullingParameters` feeds one `ms_dpvsCamera->setParameters()` used for both indoor and outdoor. Choosing among (a) revert to keep-outdoor, (b) leave Option α as-shipped (which now favors only indoor), or (c) implement the scene-aware split is a **follow-up decision**, explicitly out of scope for this measurement phase (T-23-02). DPVS-01's requirement — re-measure under D3D11 and record a keep/remove verdict — is satisfied here.

### Per D-09 / D-10 / D-11

- **D-09 primary metric:** `total_frame_ms` median + p95. Both scenes show consistent results across n≈1,200–1,300 frames per condition (3 passes each).
- **D-10 threshold:** Outdoor — `off.median (34.88) > on.median (34.12)` ⇒ not both-≤ ⇒ `keep`. Indoor — `off.median (37.79) ≤ on.median (38.49)` AND `off.p95 (44.87) ≤ on.p95 (45.84)` ⇒ `remove`.
- **D-11 inconclusive-defaults-to-keep:** Neither scene is inconclusive — both deltas (2.2% outdoor, 1.8% indoor) are consistent in direction across all three passes per condition, and the visible-object-count mechanism explains each sign. No default-to-keep tiebreak was needed.

---

## Cross-reference

- [`docs/recon/10-dpvs-profiling.md`](10-dpvs-profiling.md) — the Phase 10 D3D9 verdict (`remove` outdoor / `keep` indoor → Option α `remove` globally). Its "Phase 11 Revisit Note (per D-12)" mandated this exact D3D11 re-measure and predicted the trade could re-balance — it flipped both signs.
- `tools/dpvs-profile/test-protocol.md` — Phase 23 capture protocol (gl11 / Debug, outdoor+indoor, F11 → `toggleForceDisableOcclusionCulling`).
- `tools/dpvs-profile/analysis.py` — unchanged verdict-line emitter (D-04/D-09/D-10/D-11).
- `.planning/REQUIREMENTS.md` — DPVS-01.
- Raw CSVs: `stage/dpvs-profile/` (gitignored, not committed); Phase 10 archive under `stage/dpvs-profile-phase10-archive/`.
