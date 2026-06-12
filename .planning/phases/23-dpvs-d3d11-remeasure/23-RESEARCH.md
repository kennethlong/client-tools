# Phase 23: DPVS D3D11 Remeasure - Research

**Researched:** 2026-06-12
**Domain:** GPU/CPU frame-time profiling under D3D11; DPVS (Umbra 2003) occlusion-culling cost A/B measurement; revisiting the Phase 10 Option α verdict
**Confidence:** HIGH (the methodology, tooling, and source sites are all preserved from Phase 10; the only genuinely new work is the D3D11 timestamp-query plugin path, which the engine already demonstrates with `ID3D11Query`)

## Summary

Phase 23 is a **measurement task, not a fix**. It re-runs the Phase 10 DPVS occlusion-culling A/B (occlusion ON vs OFF) under the D3D11 renderer (`rasterMajor=11`, `gl11`) and records a keep/remove verdict that either confirms or revises Phase 10's Option α (remove occlusion globally). This was an explicit deferral baked into Phase 10 decision D-12 ("decision valid for D3D9 renderer only; Phase 11/D3D11 must re-measure") and lives as the sole remaining v2.2 gap (DPVS-01).

The entire Phase 10 harness — in-engine CSV instrumentation, F10 capture toggle, F11 occlusion A/B toggle, `/setrunlabel` console command, on-screen overlay, and the Python aggregator/verdict-emitter — was deliberately deleted as THROWAWAY in a single revert-shaped commit (`151167d2c`, -726 lines) after the Phase 10 verdict. A recovery git tag `phase-10-instrumentation-pre-cleanup` (commit `9f2ec3715`) preserves the pre-cleanup HEAD, and `tools/dpvs-profile/{analysis.py,test-protocol.md}` survived in the tree. So Phase 23's harness work is mostly **restore-and-adapt**, not greenfield.

The **single material new piece** is the GPU-timing path: the deleted harness used Direct3d9-specific `IDirect3DQuery9` timestamp queries that have NO D3D11 equivalent in the codebase. BUT — and this is the load-bearing finding — Phase 10's own verdict doc records that `gpu_us` was **uniformly zero across all 12,016 frames**, because DPVS `resolveVisibility()` is CPU-side software occlusion (Umbra walks in-memory structures, issues no draw calls). The complete DPVS cost lives in the CPU columns (`cpu_qpc_us`, `total_frame_ms`). This means the GPU-query plumbing can likely be **dropped entirely** for Phase 23, collapsing the new D3D11 work to near-zero. If the planner still wants a GPU column for completeness, the D3D11 device already uses `ID3D11Query`/`D3D11_QUERY_EVENT` (Direct3d11_Device.cpp:1157-1168), so `D3D11_QUERY_TIMESTAMP` + `D3D11_QUERY_TIMESTAMP_DISJOINT` is a 1-file add with `ms_device`/`ms_context` already in scope.

**Primary recommendation:** Restore the Phase 10 CPU-timing harness (CSV writer + F10 capture + F11 occlusion A/B + `/setrunlabel`) by cherry-picking/reverting from tag `phase-10-instrumentation-pre-cleanup`, **drop the Direct3d9 GPU-query module** (gpu_us was structurally zero), re-introduce the runtime occlusion toggle by gating the `OCCLUSION_CULLING` bit on the already-surviving `ms_forceDisableOcclusionCulling` debug flag, run the four-scene protocol under `rasterMajor=11` against a Release build (`SwgClient_r.exe`), feed the CSVs through the unchanged `tools/dpvs-profile/analysis.py`, and write the verdict to a new `docs/recon/23-dpvs-d3d11-profiling.md` mirroring the Phase 10 doc. Use the engine's own timers (QPC + Profiler), NOT RenderDoc captures — captures perturb timing and RenderDoc cannot capture the D3D9 baseline anyway.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| DPVS occlusion query (`resolveVisibility`) | Engine (`clientGraphics/RenderWorld.cpp`) | DPVS library (`src/external/3rd/library/dpvs`) | Renderer-AGNOSTIC. Runs in shared engine code BEFORE any plugin draw calls; identical under gl05 and gl11. This is why the Phase 10 verdict could even be deferred to D3D11 — the cull itself doesn't change, only the draw cost it gates does. |
| Occlusion ON/OFF A/B toggle | Engine (`RenderWorld.cpp` cullingParameters bitmask) | Config (`ConfigClientGraphics` / DebugFlags) | The `OCCLUSION_CULLING` bit on `ms_dpvsCamera->setParameters()` is the single switch. Currently hard-removed (Option α). Phase 23 must re-introduce it gated on a runtime flag. |
| Per-frame CSV row write | Engine (`clientGraphics`, restored `DpvsProfileInstrumentation`) | — | Independent of renderer; writes to `stage/dpvs-profile/`. |
| CPU timing (QPC + Profiler) | Engine (`RenderWorld.cpp` + `sharedDebug/Profiler`) | — | `QueryPerformanceCounter` pair around `resolveVisibility()` + existing `ms_dpvsQueryProfilerBlock`. Renderer-agnostic. **This is where the real DPVS cost is measured.** |
| GPU timing (OPTIONAL, likely skip) | Renderer plugin (`Direct3d11`) | Engine (`Graphics` forwarders) | Only if a gpu_us column is wanted. DPVS is CPU-only → expect zeros again. New D3D11 query path replaces the deleted Direct3d9 one. |
| Frame-time total | Engine (`Game::runGameLoopOnce` / present) | Renderer (`Direct3d11_Device::present`) | `total_frame_ms` = wall time around the whole loop; captured engine-side. |
| Verdict computation | Tooling (`tools/dpvs-profile/analysis.py`, Python 3.12 stdlib) | — | Unchanged from Phase 10; reads CSVs, emits `verdict = remove|keep`. |

## Standard Stack

### Core (already present in v2)
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| DPVS (Umbra) | 2003-era SOE license, vendored at `src/external/3rd/library/dpvs/` [VERIFIED: 10-CONTEXT.md canonical_refs] | `resolveVisibility()`, `OCCLUSION_CULLING` / `VIEWFRUSTUM_CULLING` camera bits, portal-cell traversal | The engine's occlusion + frustum + portal culler. STAYS regardless of verdict (indoor portals need it). The verdict only governs whether the `OCCLUSION_CULLING` bit is set. |
| sharedDebug Profiler | in-tree | `ProfilerBlock ms_dpvsQueryProfilerBlock` at `RenderWorld.cpp:97`; `NP_PROFILER_AUTO_BLOCK_DEFINE("resolveVisibility")` at `:1044` | Already brackets the DPVS query; no new infra. [VERIFIED: repo grep] |
| Windows QPC | Win32 | `QueryPerformanceCounter`/`QueryPerformanceFrequency` for raw µs CPU timing pair | Phase 10 D-02 method; renderer-independent; THE primary signal for a CPU-only cull. |
| Python | 3.12, stdlib only | `tools/dpvs-profile/analysis.py` aggregator + verdict emitter | Survived Phase 10 cleanup; needs NO changes. [VERIFIED: file present + read] |
| Direct3d11 (`gl11`) | in-tree plugin | `rasterMajor=11` renderer under test; `ID3D11Query` available (`ms_device`/`ms_context`) | The renderer this phase measures against. [VERIFIED: Direct3d11_Device.cpp present()] |

### Supporting (restore from recovery tag)
| Artifact | Source | Purpose | When to Use |
|----------|--------|---------|-------------|
| `DpvsProfileInstrumentation.{h,cpp}` | `git show phase-10-instrumentation-pre-cleanup:...` | CSV writer + run-label sanitizer + overlay + ExitChain teardown (407+64 lines) | Restore as the CSV-logging core. The Direct3d9 GPU-query calls inside it should be stubbed/removed. |
| F10 capture + F11 occlusion toggle | `CuiIoWin.cpp` `#ifdef _DEBUG` block in the tag | Keybind intercepts | Restore; rewire F11 to the occlusion toggle (see Pattern 2). |
| `/setrunlabel` command | `SwgCuiCommandParserDefault.cpp` in the tag | Writes run-label into the CSV column | Restore verbatim. |
| `onFrameEnd` hook | `Game.cpp` in the tag | Drives per-frame CSV row after present | Restore; renderer-agnostic. |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Restore the full Phase 10 harness | RenderDoc / PIX capture timing | REJECTED — captures perturb timing (serialize CPU↔GPU), RenderDoc cannot capture D3D9 for an apples-to-apples baseline, and DPVS cost is CPU-side where capture tools add the most distortion. Engine timers are the Phase 10 method and the right call. (Note: ROADMAP success-criterion #1 says "PIX/RenderDoc timing harness" — treat that as loose phrasing; the Phase 10 methodology it also cites used engine queries, not capture tools. Flag for discuss-phase if the literal PIX/RenderDoc wording is treated as binding.) |
| Re-introduce GPU timestamp queries | Drop gpu_us entirely | RECOMMENDED to drop — Phase 10 proved gpu_us≡0 (DPVS issues no GPU work). Keeping it means writing a brand-new D3D11 query pool for a column that will be all zeros. |
| `git revert 151167d2c` (whole harness) | Cherry-pick only the CPU-timing files | Cherry-pick is cleaner — the revert would also restore the Direct3d9 GPU-query plumbing (121 lines in Direct3d9.cpp + Gl_api forwarders) that targets the wrong renderer and produces zeros. |

**Restore command (starting point):**
```bash
# Inspect what to bring back:
git show phase-10-instrumentation-pre-cleanup:src/engine/client/library/clientGraphics/src/shared/DpvsProfileInstrumentation.cpp
git show --stat 151167d2c   # the 12 files the cleanup deleted/edited
```

**Version verification:** No new npm/registry packages. The DPVS library, Profiler, QPC, and Python 3.12 are all already in the tree and verified present this session.

## Architecture Patterns

### System Architecture Diagram

```
                       rasterMajor=11 (client.cfg / client_d.cfg)
                                    │
                                    ▼
   ┌──────────────────────────────────────────────────────────────────┐
   │ Game::runGameLoopOnce (Game.cpp:1059)                             │
   │   ... scene render ...                                            │
   │   RenderWorld::renderScene                                        │
   │     ├─ build cullingParameters bitmask  (RenderWorld.cpp:904/908) │◄── F11 toggles
   │     │     OCCLUSION_CULLING bit gated on ms_forceDisableOcclusion │     occlusion flag
   │     │     (Phase 23 re-introduces this term)                      │
   │     ├─ ms_dpvsCamera->setParameters(...)  (:923)                  │
   │     │                                                             │
   │     ├─ [QPC start] ─────────────────────────┐ (Phase 23 restore) │
   │     ├─ NP_PROFILER_BLOCK_ENTER(dpvsQuery)    │                    │
   │     ├─ ms_dpvsCamera->resolveVisibility(...) │ ◄── CPU-only cull  │
   │     │     (RenderWorld.cpp:1049)             │     (Umbra; 0 GPU)  │
   │     └─ [QPC end]  ───────────────────────────┘                    │
   │   ... draw visible objects (gl11 plugin) ...                      │
   │   Graphics::present → Direct3d11_Device::present (:1121)          │
   │                                    │                              │
   │   Game::onFrameEnd hook ───────────┘ (Phase 23 restore)           │
   │     └─ DpvsProfileInstrumentation::writeRow                       │
   │           → stage/dpvs-profile/<run_label>-<frameStart>.csv       │
   └──────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼  (post-session, offline)
        python tools/dpvs-profile/analysis.py --csv-dir stage/dpvs-profile/
                                    │
                                    ▼
        markdown summary table  +  final line "verdict = remove|keep"
                                    │
                                    ▼
        docs/recon/23-dpvs-d3d11-profiling.md   (NEW verdict doc)
```

### Recommended File Touch Map (restore-and-adapt)
```
src/engine/client/library/clientGraphics/
  ├── include/public/clientGraphics/DpvsProfileInstrumentation.h   # RESTORE from tag
  ├── src/shared/DpvsProfileInstrumentation.cpp                    # RESTORE; strip GPU-query calls
  ├── src/shared/RenderWorld.cpp                                   # re-add OCCLUSION_CULLING term + QPC pair
  └── build/win32/clientGraphics.vcxproj                           # re-add ClCompile/ClInclude entries
src/engine/client/library/clientGame/src/shared/core/Game.cpp     # RESTORE onFrameEnd hook
src/engine/client/library/clientUserInterface/.../CuiIoWin.cpp    # RESTORE F10/F11 intercept; F11→occlusion toggle
src/game/client/.../SwgCuiCommandParserDefault.cpp                # RESTORE /setrunlabel
tools/dpvs-profile/{analysis.py,test-protocol.md}                 # UNCHANGED (already present)
docs/recon/23-dpvs-d3d11-profiling.md                             # NEW verdict doc
# DO NOT restore (Direct3d9 GPU path — wrong renderer + zeros):
#   Direct3d9.cpp dpvsGpuTiming pool, Gl_dll.def entries, Graphics.{h,cpp} forwarders, SetupClientGraphics hook
```

### Pattern 1: Restore the CPU-timing harness from the recovery tag
**What:** The Phase 10 instrumentation lives intact at git tag `phase-10-instrumentation-pre-cleanup` (commit `9f2ec3715`). Cleanup commit `151167d2c` lists every file and exact line range removed.
**When to use:** First wave of Phase 23 — bring back the CSV writer, capture toggle, command, and frame hook.
**Note:** The cleanup commit message itself is the precise restore manifest (12 files, with the Direct3d9 GPU path clearly separated as "Wave 1 — GPU timing plumbing").

### Pattern 2: Re-introduce the runtime occlusion A/B toggle (the critical adaptation)
**What:** Option α (`docs/recon/10-dpvs-profiling.md`, D-13) HARD-REMOVED the `OCCLUSION_CULLING` bit AND deleted the `ms_disableOcclusionCulling` static + config key + `setDisableOcclusionCulling` setter. The current `cullingParameters` is occlusion-free:
```cpp
// RenderWorld.cpp CURRENT state (Option α applied):
#ifdef _DEBUG
    uint const cullingParameters = (ms_disableViewFrustumCulling ? 0 : DPVS::Camera::VIEWFRUSTUM_CULLING);  // :904
#else
    uint const cullingParameters = DPVS::Camera::VIEWFRUSTUM_CULLING;                                       // :908
#endif
```
The runtime A/B toggle Phase 10 relied on (F11 → `setDisableOcclusionCulling`) **no longer exists**. To measure occlusion ON vs OFF again, Phase 23 must re-introduce the bit gated on a runtime-flippable flag.
**Cleanest seam:** `ms_forceDisableOcclusionCulling` (a `_DEBUG`-only bool at `RenderWorld.cpp:81`) **survived** the cleanup and is still registered as a DebugFlag under `[ClientGraphics/Dpvs] disableOcclusionCulling` at `RenderWorld.cpp:227` — but it is currently **dead** (nothing reads it; line 904 only checks `ms_disableViewFrustumCulling`). Re-wire the measurement build's culling param to:
```cpp
// Phase 23 measurement build (_DEBUG) — re-introduce occlusion gated on the surviving flag:
uint const cullingParameters =
      (ms_forceDisableOcclusionCulling ? 0 : DPVS::Camera::OCCLUSION_CULLING)
    | (ms_disableViewFrustumCulling    ? 0 : DPVS::Camera::VIEWFRUSTUM_CULLING);
```
Then F11 flips `ms_forceDisableOcclusionCulling`. This reuses surviving config + DebugFlag plumbing and keeps the change inside `_DEBUG` so the shipped (`#else`/Release-as-shipped) Option α state is untouched. **CAVEAT:** Phase 10 measured against a Release-style build for clean perf numbers but the F-toggle was wired through a `_DEBUG` keybind block (`CuiIoWin.cpp #ifdef _DEBUG`). The planner must reconcile "Release build for perf" vs "toggle lives under `_DEBUG`" — see Pitfall 3.
**When to use:** This is the single most important adaptation; without a working A/B toggle there is no measurement.

### Pattern 3: CPU timing via QPC pair around resolveVisibility (D-02)
**What:** A `QueryPerformanceCounter` start/stop pair flanking `ms_dpvsCamera->resolveVisibility(...)` at `RenderWorld.cpp:1049`, converted to µs via `QueryPerformanceFrequency`, written to the `cpu_qpc_us` CSV column. The existing `NP_PROFILER_BLOCK_ENTER(ms_dpvsQueryProfilerBlock)` already brackets the same call and feeds `profiler_dpvs_us`.
**When to use:** The core measurement. Renderer-agnostic; identical under gl11.

### Pattern 4 (OPTIONAL): D3D11 GPU timestamp query — only if a gpu_us column is wanted
**What:** If the planner insists on a GPU column, the D3D11 equivalent of the deleted Direct3d9 path uses `D3D11_QUERY_TIMESTAMP_DISJOINT` (containing query, gives frequency + validity flag) + two `D3D11_QUERY_TIMESTAMP` queries, double-buffered N=3 to absorb CPU-ahead-of-GPU latency.
**Evidence it's trivial here:** `Direct3d11_Device::present()` already creates and polls an `ID3D11Query` (`D3D11_QUERY_EVENT`) with `ms_device->CreateQuery` / `ms_context->End` / `ms_context->GetData` (Direct3d11_Device.cpp:1157-1168). Same API surface; `ms_device`/`ms_context` in scope.
**Strong recommendation:** SKIP IT. Phase 10's verdict doc §"Note on gpu_us = 0" proves DPVS issues no GPU work → 12,016/12,016 frames read zero. A GPU column adds a new plugin code path, a DLL-ABI surface (Gl_api forwarder — see the shared-header ABI cascade trap in project memory), and the double-buffer pitfall, all to measure structural zeros. CPU columns + total_frame_ms capture the complete cost.
**Source:** [CITED: https://therealmjp.github.io/posts/profiling-in-dx11-with-queries/] (the canonical DX11 timestamp-query reference; cited in 10-RESEARCH.md).

### Anti-Patterns to Avoid
- **Using RenderDoc/PIX captures for the timing numbers.** Captures serialize CPU↔GPU and inflate frame time; RenderDoc can't capture the D3D9 baseline for comparison. RenderDoc is a structural-diff tool here, not a timing tool. Use engine QPC/Profiler.
- **Restoring the Direct3d9 GPU-query module.** Wrong renderer (this phase is gl11) AND it measured zeros. `git revert 151167d2c` would drag it back — cherry-pick the CPU-timing files instead.
- **Forgetting the inverted default-state trap.** Phase 10 launched with `disableOcclusionCulling=true` by default (DPVS:OFF), persisted via `local_machine_options.iff`, which inverted first-capture labels. The overlay + the authoritative `dpvs_occlusion_flag` CSV column existed precisely to recover from this. Keep both.
- **Touching the shipped Option α `#else` branch.** Keep the re-introduced occlusion bit inside `_DEBUG` so the released behavior is unchanged unless the verdict says to revert it.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| CSV aggregation + percentiles + verdict | A new analysis script | `tools/dpvs-profile/analysis.py` (unchanged) | Already implements D-04 stats (median/p95/p99/max/stdev), D-09 primary-metric, D-10 threshold, D-11 default-to-keep; stdlib-only; verified present. |
| Capture procedure | A new protocol | `tools/dpvs-profile/test-protocol.md` (adapt scene list) | Step-by-step session checklist already written; only the renderer (gl11) and scenes need restating. |
| In-engine CSV writer + overlay + ExitChain teardown | New instrumentation | Restore `DpvsProfileInstrumentation.{h,cpp}` from the recovery tag | 471 lines already written, tested, and run for 12,016 frames. |
| GPU timing pool | A from-scratch D3D11 query pool | Skip it (gpu_us≡0) OR mirror the existing `ID3D11Query` usage in present() | DPVS is CPU-only; the column is structurally zero. |
| Occlusion ON/OFF switch | A new config system | Gate `OCCLUSION_CULLING` on the surviving `ms_forceDisableOcclusionCulling` DebugFlag | Config + DebugFlag plumbing already registered at RenderWorld.cpp:227. |

**Key insight:** Phase 23 is ~80% restore-from-git-tag, ~15% one source adaptation (re-introduce the occlusion bit + QPC pair), and ~5% run-the-protocol-and-write-the-doc. The hand-rolling temptation is the GPU-query path — resist it; the data says it's zeros.

## Runtime State Inventory

> This phase RESTORES deleted code and re-introduces a removed toggle. Treating it like a refactor surfaces what survived vs what must come back.

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Deleted harness source | `DpvsProfileInstrumentation.{h,cpp}` removed by `151167d2c`; recoverable from tag `phase-10-instrumentation-pre-cleanup` | Restore (CPU path only) — code edit |
| Removed runtime toggle | `ms_disableOcclusionCulling` static + `setDisableOcclusionCulling` setter + `disableOcclusionCulling` KEY_BOOL config key — all DELETED by D-13/D-14 (Option α). `ms_forceDisableOcclusionCulling` DebugFlag SURVIVED but is dead (unreferenced). | Re-introduce occlusion gating on the surviving flag — code edit |
| Surviving tooling (NOT in git-ignored output) | `tools/dpvs-profile/analysis.py` + `test-protocol.md` present and tracked | None — reuse as-is |
| Config knobs | `[ClientGraphics/Dpvs] reportInstrumentation=true` STILL in `stage/client_d.cfg:124-131` (left over from Phase 10); `rasterMajor=11` set at `stage/client_d.cfg:41`. The cfg is marked "DO NOT COMMIT". | Set `rasterMajor=11` for the run; re-confirm `reportInstrumentation` is consumed by the restored writer — config edit |
| Captured raw data location | `stage/dpvs-profile/` (gitignored) — Phase 10's 22 CSVs are NOT in git; only the aggregated tables in `docs/recon/10-dpvs-profiling.md` survive | Empty `stage/dpvs-profile/` before the session; the Phase 10 baseline for apples-to-apples is the **tables in the recon doc**, not raw CSVs |
| Build artifacts | `clientGraphics.vcxproj` had its DpvsProfileInstrumentation ClCompile/ClInclude entries removed | Re-add the two vcxproj entries when restoring the .cpp/.h — build edit |
| Recovery anchor | git tag `phase-10-instrumentation-pre-cleanup` @ `9f2ec3715` confirmed present this session | None — verified live |

**Apples-to-apples baseline note:** Phase 10 raw CSVs are gone (gitignored, not preserved). The Phase 10 D3D9 numbers for comparison are the **summary tables** in `docs/recon/10-dpvs-profiling.md` §Summary Statistics (plaza/starport/walking/cantina, median+p95 per condition). Phase 23 should reproduce the same four scenes under gl11 and compare table-to-table. Re-capturing a fresh D3D9 (gl05) pass in the same session would give the cleanest A/B/A but costs a second renderer toggle + rebuild reconcile — flag as a planner choice (Open Question 2).

## Common Pitfalls

### Pitfall 1: Treating gpu_us≡0 as a measurement failure (it's the correct answer)
**What goes wrong:** Seeing an all-zero gpu_us column and adding/debugging GPU-query plumbing to "fix" it.
**Why it happens:** Intuition says occlusion culling is GPU work. It isn't — DPVS/Umbra is CPU-side software occlusion; `resolveVisibility()` issues no draw calls.
**How to avoid:** Read 10-dpvs-profiling.md §"Note on gpu_us = 0". Measure CPU (`cpu_qpc_us`) + `total_frame_ms`. If a GPU column is kept at all, expect zeros and document them as correct.
**Warning signs:** Spending time on the D3D11 query pool before capturing a single CPU number.

### Pitfall 2: No working A/B toggle after Option α
**What goes wrong:** Restoring the F11 keybind but finding it calls a deleted `setDisableOcclusionCulling` → no-op or compile error; or the `OCCLUSION_CULLING` bit isn't in `cullingParameters` at all, so flipping any flag does nothing.
**Why it happens:** Option α (D-13/D-14) deleted the static, setter, and config key, AND stripped the bit from the bitmask. The harness's F11 target is gone.
**How to avoid:** Pattern 2 — re-introduce the `OCCLUSION_CULLING` term gated on the surviving `ms_forceDisableOcclusionCulling` flag; point F11 at that flag. Verify in-frame via the overlay (DPVS:ON green / OFF red) that the toggle actually changes culling (visible-object-count column, if captured, should jump).
**Warning signs:** ON and OFF passes produce statistically identical numbers — the toggle isn't wired.

### Pitfall 3: Release-for-perf vs `_DEBUG`-only toggle/instrumentation mismatch
**What goes wrong:** Phase 10's F10/F11 keybinds and several DPVS flags live in `#ifdef _DEBUG` blocks, but meaningful perf numbers want a Release build (`SwgClient_r.exe`, reads `client.cfg`). A pure Release build won't have the toggle.
**Why it happens:** `ms_forceDisableOcclusionCulling`, the F10/F11 `CuiIoWin` intercept, and several render flags are `_DEBUG`-gated.
**How to avoid:** Decide the measurement-build strategy explicitly. Options: (a) a Debug build accepting Debug-vs-Release frame-time offset but valid for a *relative* ON-vs-OFF delta (the delta is what the verdict turns on, not absolute fps); (b) un-gate just the occlusion toggle + capture hooks for the measurement build via a dedicated `DPVS_REMEASURE` macro; (c) measure in Debug and note the caveat. Phase 10 captured on `SwgClient_d.exe` (Debug) per its test-protocol Pre-Session Checklist, and the verdict was a *relative* comparison — so Debug is defensible. **Reconcile with the project memory** that Release exe reads `client.cfg` / Debug reads `client_d.cfg`, and `rasterMajor` must be set in the matching cfg.
**Warning signs:** F11 does nothing in a Release build; CSV writer never fires because `reportInstrumentation` is read only in a `_DEBUG` path.

### Pitfall 4: Inverted default occlusion state mislabels the first pass
**What goes wrong:** Client launches with occlusion OFF by default (persisted in `local_machine_options.iff`), so "pass1-on" is actually captured with occlusion off.
**Why it happens:** Documented Phase 10 capture-quality caveat #1.
**How to avoid:** Trust the `dpvs_occlusion_flag` CSV column (written from the actual flag state) as authoritative over the `run_label`, exactly as Phase 10 did; confirm the overlay state before each pass.
**Warning signs:** ON and OFF numbers look swapped vs the D3D9 baseline direction.

### Pitfall 5: gl11-specific frame-time noise (LOD thrash, async loads)
**What goes wrong:** D3D11 path has known intermittent furniture LOD-thrash flicker and async-asset load races (per project memory) that inject single-frame transients.
**Why it happens:** Open D3D11 issues parked as non-blocking during v2.2.
**How to avoid:** Phase 10's stats are long-tail-robust (median + p95, NOT mean; D-04). Single-frame outliers (the doc records 147ms/108ms/106ms transients) don't move p95/p99. Hold camera still ~10s, alternate ON-OFF-ON-OFF-ON-OFF, keep ≥3 passes/side. Let the median+p95 rule (D-10) absorb it.
**Warning signs:** stdev >> Phase 10's (~2-6ms); re-capture if median across same-side passes differs >30%.

## Code Examples

### Re-introducing the occlusion A/B bit (RenderWorld.cpp, _DEBUG measurement build)
```cpp
// Current (Option α, occlusion removed) — RenderWorld.cpp:901-910:
#ifdef _DEBUG
    uint const cullingParameters = (ms_disableViewFrustumCulling ? 0 : DPVS::Camera::VIEWFRUSTUM_CULLING);
    portalRecusionDepth = ms_disablePortalTraversal ? 0 : 8;
#else
    uint const cullingParameters = DPVS::Camera::VIEWFRUSTUM_CULLING;
    portalRecusionDepth = 8;
#endif

// Phase 23 (re-add occlusion gated on the SURVIVING ms_forceDisableOcclusionCulling flag,
// kept inside _DEBUG so shipped Option α behavior is unchanged):
#ifdef _DEBUG
    uint const cullingParameters =
          (ms_forceDisableOcclusionCulling ? 0 : DPVS::Camera::OCCLUSION_CULLING)
        | (ms_disableViewFrustumCulling    ? 0 : DPVS::Camera::VIEWFRUSTUM_CULLING);
    portalRecusionDepth = ms_disablePortalTraversal ? 0 : 8;
#else
    uint const cullingParameters = DPVS::Camera::VIEWFRUSTUM_CULLING;   // shipped Option α — unchanged
    portalRecusionDepth = 8;
#endif
// Source: RenderWorld.cpp:81 (flag), :227 (DebugFlag registration), :904/908 (bitmask)
```

### D3D11 timestamp query (ONLY if gpu_us kept — pattern already in the codebase)
```cpp
// The engine already does this with D3D11_QUERY_EVENT in present():
// Direct3d11_Device.cpp:1157-1168 (ms_device->CreateQuery / ms_context->End / GetData)
// For GPU timing, same API with TIMESTAMP + TIMESTAMP_DISJOINT, double-buffered N=3:
D3D11_QUERY_DESC dj{ D3D11_QUERY_TIMESTAMP_DISJOINT, 0 };
D3D11_QUERY_DESC ts{ D3D11_QUERY_TIMESTAMP, 0 };
// issue into slot frame%N; read slot (frame+1)%N; check disjoint.Disjoint==FALSE; us = (end-start)*1e6/disjoint.Frequency
// Source: [CITED: https://therealmjp.github.io/posts/profiling-in-dx11-with-queries/]
// RECOMMENDATION: skip — DPVS is CPU-only, gpu_us will be 0 (10-dpvs-profiling.md §gpu_us=0).
```

### Aggregator invocation (unchanged from Phase 10)
```bash
python D:/Code/swg-client-v2/tools/dpvs-profile/analysis.py \
  --csv-dir D:/Code/swg-client-v2/stage/dpvs-profile/
# Final stdout line: "verdict = remove" or "verdict = keep" (D-10 threshold, D-11 default-keep)
# Source: tools/dpvs-profile/analysis.py (verified present + read)
```

## State of the Art

| Old Approach (Phase 10 / D3D9) | Current Approach (Phase 23 / D3D11) | When Changed | Impact |
|--------------------------------|--------------------------------------|--------------|--------|
| `IDirect3DQuery9` TIMESTAMP/DISJOINT GPU queries via Gl_api forwarders | Skip GPU queries (gpu_us≡0) OR `ID3D11Query` TIMESTAMP path | Phase 11 D3D11 plugin landed | New renderer; but DPVS is CPU-only so GPU path is optional. Cross-plugin Gl_api ABI surface is a known cascade trap (project memory) — avoid adding to it. |
| F11 → `RenderWorld::setDisableOcclusionCulling()` | F11 → flip `ms_forceDisableOcclusionCulling` (re-gated bit) | Option α deleted the setter | Toggle target moved; restore via surviving DebugFlag. |
| Captured on `SwgClient_d.exe` (Debug) | Decide Debug-vs-Release for perf validity | — | Phase 10 Debug capture was a valid *relative* A/B; Release gives cleaner absolute numbers but needs `_DEBUG` un-gating. |
| Verdict valid for D3D9 only (D-12) | Verdict for D3D11; confirm/revise Option α | This phase | DPVS-01 satisfied; v2.2 milestone closeable. |

**Deprecated/outdated:**
- The D3D9 `dpvsGpuTiming*` Gl_api functions + Direct3d9 query pool — deleted, renderer-wrong, and measured zeros. Do not restore.
- `disableOcclusionCulling` KEY_BOOL config key + `ms_disableOcclusionCulling` static + `setDisableOcclusionCulling` setter — deleted by Option α; Phase 23 uses the surviving `ms_forceDisableOcclusionCulling` DebugFlag instead.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | DPVS cost is still CPU-only under D3D11 (gpu_us would again be ≡0), so the GPU-query path can be dropped | Pattern 4, Pitfall 1 | LOW — `resolveVisibility()` is renderer-agnostic engine code; D3D11 doesn't change that it issues no draw calls. If somehow a GPU column is mandated, the existing `ID3D11Query` usage makes it cheap to add. |
| A2 | `ms_forceDisableOcclusionCulling` is the cleanest re-toggle seam and its DebugFlag plumbing is fully functional | Pattern 2 | LOW-MED — verified the static (:81) and DebugFlag registration (:227) survive, but it's currently unreferenced ("dead"); a planner should compile-check the re-wire. Worst case: re-add a dedicated bool. |
| A3 | ROADMAP success-criterion #1's "PIX/RenderDoc timing harness" is loose phrasing and the intended method is the Phase 10 engine-timer methodology (also cited in the same criterion) | Alternatives Considered | MED — if the criterion is read literally as requiring PIX/RenderDoc, the approach changes. Recommend discuss-phase confirm engine-timer method (captures perturb timing + can't capture D3D9 baseline). |
| A4 | Debug-build capture is acceptable for a *relative* ON-vs-OFF verdict (as Phase 10 did), OR the toggle/hooks get un-gated for a Release measurement build | Pitfall 3 | MED — affects absolute frame-time validity. The verdict turns on the relative delta (D-09/D-10), which Debug preserves; but if absolute fps parity vs D3D9 is wanted, build strategy matters. Planner decision. |
| A5 | The clean-geometry precondition (success criterion #3) is satisfied as of 2026-06-12 | Summary | LOW — corroborated by v2.2-MILESTONE-AUDIT (12/13 reqs satisfied, all visual reqs ✅) + git log (terrain/gamma/minimap/buildings fixes through fcfbb2226). Verify lightly in-client at session start. |
| A6 | Phase 10 raw CSVs are unavailable for direct comparison (gitignored `stage/dpvs-profile/`); the recon-doc summary tables are the baseline | Runtime State Inventory | LOW — confirmed `stage/dpvs-profile/` is gitignored and only the recon-doc tables are tracked. A fresh in-session D3D9 pass is the alternative for a same-machine A/B/A. |

## Open Questions

1. **Build strategy: Debug vs Release for the measurement.**
   - What we know: Phase 10 captured on Debug (`SwgClient_d.exe`); the verdict is a relative ON-vs-OFF delta; F10/F11 + `ms_forceDisableOcclusionCulling` are `_DEBUG`-gated; Release reads `client.cfg`, Debug reads `client_d.cfg`.
   - What's unclear: whether the planner wants absolute Release fps numbers (requires un-gating the toggle/hooks) or accepts a Debug relative A/B.
   - Recommendation: Debug relative A/B mirrors Phase 10 exactly and is the lowest-risk path; document the Debug caveat. If absolute parity is desired, add a `DPVS_REMEASURE` macro that un-gates only the toggle + capture in an Optimized/Release build.

2. **Baseline comparison shape: table-vs-table, or fresh same-session D3D9 A/B/A.**
   - What we know: Phase 10 raw CSVs are gone; only summary tables survive in the recon doc. Hardware (RTX 3060) and server (SWGSource VM) may differ slightly from the Phase 10 session.
   - What's unclear: whether comparing gl11 numbers to the year-old D3D9 tables is rigorous enough, or whether a fresh gl05 pass in the same session is warranted.
   - Recommendation: capture a fresh D3D9 (gl05) baseline pass in the same session per scene for a clean A(d3d9)/B(d3d11) on identical hardware/scene — costs one extra `rasterMajor` toggle + build reconcile but removes cross-session drift. If time-boxed, table-vs-table is acceptable per the Phase 10 methodology.

3. **Scene set: four scenes (full Phase 10 mirror) or the single Mos Eisley plaza (DPVS-01 minimum).**
   - What we know: DPVS-01 / ROADMAP criterion #1 require an occlusion-vs-no-occlusion comparison; Phase 10 used four scenes (plaza, starport, walking, cantina) to derive the outdoor-remove/indoor-keep split that Option α balances.
   - What's unclear: whether Phase 23 must reproduce all four to confirm/revise Option α, or whether the plaza alone suffices for a keep/remove verdict.
   - Recommendation: reproduce at minimum one outdoor (plaza) + one indoor (cantina) scene — Option α is fundamentally an outdoor-vs-indoor tradeoff, so a single scene can't confirm/revise it. The full four-scene set is ideal; two scenes is the defensible minimum.

4. **Verdict doc location.**
   - What we know: Phase 10 wrote `docs/recon/10-dpvs-profiling.md`; the ROADMAP doesn't pin a Phase 23 path.
   - Recommendation: new `docs/recon/23-dpvs-d3d11-profiling.md`, sibling format, cross-linking the Phase 10 doc and stating whether Option α is confirmed or revised.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Python | `analysis.py` aggregator | ✓ (assumed on PATH per Phase 10 use) | 3.12 (stdlib-only) | none needed |
| gl11 / Direct3d11 plugin | the renderer under test | ✓ | in-tree, `rasterMajor=11` | n/a — this is the subject |
| gl05 / Direct3d9 plugin | optional fresh D3D9 baseline | ✓ | `rasterMajor=5` | use Phase 10 recon-doc tables instead |
| SWGSource VM @ 192.168.1.200:44453 | live scene capture (NPCs, zones) | ⚠ session-dependent | per `project_vm_server_setup.md` | none — capture needs a live server |
| MSBuild (`$env:MSBUILD`) | rebuild clientGraphics + SwgClient after source restore | ✓ | set in settings.json | n/a |
| git tag `phase-10-instrumentation-pre-cleanup` | restore the harness | ✓ | @ `9f2ec3715` | re-implement from the recon doc spec |
| `tools/dpvs-profile/{analysis.py,test-protocol.md}` | aggregation + protocol | ✓ | present in tree | n/a |
| RenderDoc CLI/MCP | NOT used for timing (structural only) | ✓ (`D:\Code\renderdoc-mcp`) | v0.3.0 | n/a — engine timers are the method |

**Missing dependencies with no fallback:** SWGSource VM must be up for the capture session (live NPC density drives the DPVS delta). This is a session-time prerequisite, not a code blocker.

**Missing dependencies with fallback:** A fresh D3D9 baseline is optional — fall back to the Phase 10 recon-doc summary tables.

## Validation Architecture

> nyquist_validation is enabled (config workflow.nyquist_validation=true). This phase is a measurement, so its "tests" are the capture protocol + the aggregator's deterministic verdict logic, not unit tests of game code.

### Test Framework
| Property | Value |
|----------|-------|
| Framework | None for game code (measurement phase). The aggregator `analysis.py` is plain Python 3.12 stdlib; verifiable by running it on a fixture CSV set. |
| Config file | none |
| Quick run command | `python tools/dpvs-profile/analysis.py --csv-dir <dir>` (emits table + `verdict = ...`) |
| Full suite command | the capture protocol in `tools/dpvs-profile/test-protocol.md` (manual, live-server) |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| DPVS-01 | Occlusion-vs-no-occlusion frame-time A/B captured under D3D11 | manual (live capture) | `test-protocol.md` session, then `analysis.py` | ✅ protocol + aggregator present |
| DPVS-01 | Verdict computed by the locked D-10/D-11 rule | unit-ish (deterministic) | `python tools/dpvs-profile/analysis.py --csv-dir stage/dpvs-profile/` → final line | ✅ analysis.py present |
| DPVS-01 | Keep/remove verdict recorded confirming/revising Option α | doc artifact | manual write of `docs/recon/23-dpvs-d3d11-profiling.md` | ❌ Wave N (new doc) |

### Sampling Rate
- **Per restored-source commit:** client builds + boots to character select (boot gate, AGENTS.md), F11 visibly toggles DPVS state in the overlay.
- **Per capture session:** ≥3 passes/condition/scene, alternating ON-OFF; `analysis.py` runs clean (no header-mismatch warnings).
- **Phase gate:** verdict line emitted + recorded in the recon doc; client still boots with the shipped Option α `#else` branch untouched.

### Wave 0 Gaps
- [ ] Restore `DpvsProfileInstrumentation.{h,cpp}` (CPU path) from tag — covers DPVS-01 capture
- [ ] Re-add `clientGraphics.vcxproj` ClCompile/ClInclude entries — build wiring
- [ ] Re-introduce `OCCLUSION_CULLING` bit gated on `ms_forceDisableOcclusionCulling` + QPC pair — covers the A/B toggle
- [ ] Restore F10/F11 intercept (`CuiIoWin.cpp`) + `/setrunlabel` (`SwgCuiCommandParserDefault.cpp`) + `onFrameEnd` hook (`Game.cpp`)
- [ ] (Optional) framework: none required; `analysis.py` already exists

## Security Domain

> security_enforcement is enabled (ASVS L1). This phase adds no network endpoints, no input parsing of untrusted data, no auth/session/crypto surface. It restores a local-only CSV writer (writes to `stage/dpvs-profile/` on the dev machine) and a debug keybind/console command.

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | — |
| V3 Session Management | no | — |
| V4 Access Control | no | — |
| V5 Input Validation | minimal | `/setrunlabel <string>` writes a user string into a CSV column; the Phase 10 writer already sanitizes the run-label (DpvsProfileInstrumentation run-label sanitizer per recon doc). Restore that sanitizer with the file. |
| V6 Cryptography | no | — |

### Known Threat Patterns for this stack
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| CSV injection via `/setrunlabel` into a file later opened in a spreadsheet | Tampering (low, dev-local) | The restored run-label sanitizer strips path/format chars; the column is consumed by `analysis.py` (stdlib `csv`, not a spreadsheet macro engine). Local dev-only file. |
| Debug keybind/command shipped to release | Elevation (very low) | Keep F10/F11 + `/setrunlabel` + the occlusion re-gate inside `_DEBUG`; the shipped Release Option α path stays untouched. |

## Sources

### Primary (HIGH confidence)
- `docs/recon/10-dpvs-profiling.md` — the canonical Phase 10 verdict doc: methodology (D-01..D-04), gpu_us≡0 finding, per-scene summary tables, Option α rationale, D-12 Phase 11 revisit note. [VERIFIED: read in full]
- `.planning/phases/10-dpvs-culling-experiment/10-CONTEXT.md` — decisions D-01..D-19 (the locked methodology this phase mirrors). [VERIFIED: read in full]
- `.planning/phases/10-dpvs-culling-experiment/10-SUMMARY.md` — what landed, what was removed as THROWAWAY, the recovery tag + cleanup commit. [VERIFIED]
- `tools/dpvs-profile/analysis.py` + `test-protocol.md` — the surviving aggregator + protocol. [VERIFIED: read]
- `src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp` lines 76-84, 218-229, 901-924, 1035-1060 — current Option α state, surviving flags, DPVS camera + resolveVisibility sites. [VERIFIED: grep + read]
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_Device.cpp:1098-1172` — present() + existing `ID3D11Query` usage proving the D3D11 timestamp path is cheap. [VERIFIED: read]
- git tag `phase-10-instrumentation-pre-cleanup` @ `9f2ec3715` + cleanup commit `151167d2c` (12-file restore manifest). [VERIFIED: git show]
- `.planning/v2.2-MILESTONE-AUDIT.md` — DPVS-01 sole gap; clean-geometry precondition satisfied. [VERIFIED: read]
- `git log` koogie-msvc-cpp20-base — D3D11 rendering fixes through fcfbb2226 (2026-06-12) confirming functional reality. [VERIFIED]

### Secondary (MEDIUM confidence)
- `.planning/ROADMAP.md` §Phase 23 — goal, success criteria (note the "PIX/RenderDoc" phrasing flagged as loose, A3). [VERIFIED: read]
- `.planning/REQUIREMENTS.md` DPVS-01. [VERIFIED]
- `stage/client_d.cfg:41,124-131` — `rasterMajor=11`, leftover `reportInstrumentation=true`. [VERIFIED]

### Tertiary (LOW confidence)
- https://therealmjp.github.io/posts/profiling-in-dx11-with-queries/ — DX11 timestamp-query reference (cited in 10-RESEARCH.md; only relevant if the optional GPU path is built). [CITED, not re-fetched this session]

## Metadata

**Confidence breakdown:**
- Standard stack / restore path: HIGH — every artifact verified present in tree or recoverable from a confirmed git tag; cleanup commit is the exact restore manifest.
- Architecture / toggle re-introduction: HIGH — current Option α source state read directly; surviving `ms_forceDisableOcclusionCulling` flag + DebugFlag registration confirmed at exact lines.
- Pitfalls: HIGH — drawn from Phase 10's own documented capture-quality caveats + verified current-code state.
- GPU-path decision: HIGH — gpu_us≡0 is a documented Phase 10 finding with code-review justification; D3D11 query feasibility confirmed by existing in-codebase usage.
- Build-strategy / criterion-literalness: MEDIUM — two genuine planner decisions (Debug-vs-Release, PIX/RenderDoc literalness) flagged as Open Questions / Assumptions for discuss-phase.

**Research date:** 2026-06-12
**Valid until:** 2026-07-12 (stable — methodology is locked from Phase 10; only the live capture session and a possible build-strategy decision remain open)
