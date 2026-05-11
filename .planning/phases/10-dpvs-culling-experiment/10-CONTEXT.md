# Phase 10: DPVS Culling Experiment - Context

**Gathered:** 2026-05-10
**Status:** Ready for planning
**Active tree:** `D:/Code/swg-client-v2/` branch `koogie-msvc-cpp20-base` (v2). whitengold (`D:/Code/swg-client/`) is research history; no source edits there.

<domain>
## Phase Boundary

Measure the cost of DPVS occlusion culling (`resolveVisibility()` + the `OCCLUSION_CULLING` bit on the DPVS camera) on a modern discrete GPU in **one** busy outdoor zone — Mos Eisley cantina plaza — and decide whether to remove occlusion culling permanently from the rendering path. View-frustum culling and portal traversal for indoor cells stay regardless of outcome; they ride along on the same `resolveVisibility()` call.

Phase scope is bounded by DPVS-01 and DPVS-02 (REQUIREMENTS.md §DPVS). The phase delivers:
1. In-engine instrumentation (D3D9 timestamp queries + QueryPerformanceCounter pair + CSV logger + on-screen overlay + F11 runtime A/B toggle).
2. A capture protocol the user (Kenny) executes against the SWGSource VM.
3. A scored decision documented in `docs/recon/10-dpvs-profiling.md`.
4. Conditional source-level removal of the `OCCLUSION_CULLING` bit from `RenderWorld.cpp:906` if the data supports it.

Out of scope: deleting the DPVS library, removing portal traversal, multi-zone studies (single-zone result is the verdict), retroactively re-running under Phase 11's D3D11 plugin (Phase 11 owns its own re-measurement).

</domain>

<decisions>
## Implementation Decisions

### Profiling methodology

- **D-01: GPU timing — D3D9 `IDirect3DQuery9` timestamp queries in-engine.** Add a `D3DQUERYTYPE_TIMESTAMP` query pair plus a `D3DQUERYTYPE_TIMESTAMPDISJOINT` containing query around the DPVS/scene-render block. Convert to microseconds via the disjoint frequency. Reproducible by anyone with the build; closest to engineering-grade for the A/B. External capture tools (PIX Legacy, Intel GPA, NVIDIA Nsight) explicitly NOT used — D3D9 support is patchy across modern releases and the workflow is heavy for a single-zone study.

- **D-02: CPU timing — existing `ms_dpvsQueryProfilerBlock` + new `QueryPerformanceCounter` pair.** Reuse the existing ProfilerBlock at `RenderWorld.cpp:1041-1044` for the broader Profiler signal. Add a fresh `QueryPerformanceCounter` start/stop pair flanking the `resolveVisibility()` call (or a tight scope guard equivalent) for raw µs numbers independent of the Profiler's sampling cadence. Both contribute columns to the CSV.

- **D-03: Output channels — CSV log file + on-screen overlay.** Per-frame samples append to a CSV at a fixed path with columns `{frame_no, wall_ms_iso, run_label, dpvs_occlusion_flag, gpu_us, cpu_qpc_us, profiler_dpvs_us, total_frame_ms, draw_call_count_if_available}`. Overlay shows current `disableOcclusionCulling` state, current run-label, and a rolling capture indicator so the user can confirm in-frame that capture is on. Researcher/planner pick the exact CSV path and overlay text shape.

- **D-04: Statistics — per-frame raw + distributional summary.** Retain the per-frame raw CSV. Compute median, p95, p99, max, and stdev per condition × per scene. Means are NOT the primary stat — frame-time distributions are long-tail-dominated. Means MAY appear in the analysis as cross-check only.

### Test scenes + sample protocol

- **D-05: Single test scene — Mos Eisley cantina plaza.** Per REQUIREMENTS.md DPVS-01. Single-zone result is sufficient for the verdict. Multi-zone expansion (Coronet starport, Theed plaza) is deferred to v3+ if data warrants.

- **D-06: Automation level — keybind-toggle capture, manual everything else.** Kenny drives the capture: parks the camera, picks the moment NPC density looks representative, decides duration. F10 toggles capture on/off. F11 toggles `RenderWorld::setDisableOcclusionCulling()`. A console command (researcher to name; suggested `/setrunlabel <string>`) writes the current run-label into the CSV column for every subsequent frame. NO scripted camera path replay — the engine cost of wiring that into Camera is out of proportion for a profiling experiment.

- **D-07: Runtime A/B toggle — new F11 keybind calling `RenderWorld::setDisableOcclusionCulling()`.** The setter already exists at `RenderWorld.cpp:1151`. Wire the keybind through whatever input-binding seam the planner identifies (likely `SwgCuiHud` or `clientDirectInput` — researcher to confirm). Toggle is instantaneous; no relog or zone-change required. F11 must NOT conflict with an existing binding (check `client.cfg` + `commands.tab`).

- **D-08: Sample count — 3 capture passes per condition.** ~10 seconds each, ~600 frames at a typical 60fps. 6 captures total per session. Three passes per side absorbs short-term variance (NPC spawn batches, spawner timing, autoclock effects); 10s is short enough that camera-position drift is minimal. Capture session order is at Kenny's discretion (suggestion: alternate ON-OFF-ON-OFF-ON-OFF to balance any time-of-day drift).

### Decision threshold

- **D-09: Primary metric — total frame time, median + p95.** End-to-end frame time is what "fps regression" in the ROADMAP means. CPU and GPU components recorded for diagnostic value but the verdict turns on the total. If GPU-only OR CPU-only metrics disagree with total, the analysis must explain why; total stays the deciding stat.

- **D-10: Threshold rule — DPVS-off median+p95 BOTH ≤ DPVS-on by any margin → remove; any regression on either → keep.** Strictest reading of "no fps regression (or positive gain)". Sidesteps having to pick an arbitrary noise-floor band. The "by any margin" phrasing is intentional — the experiment is set up to absorb sample noise via multi-pass averaging (D-08); after averaging, "off ≤ on" should be a real signal, not noise.

- **D-11: Inconclusive outcome → default to keep.** If results are mixed (e.g., median wins but p95 loses, or CPU wins but GPU loses) the verdict is "keep DPVS occlusion culling." Status quo wins ties. Phase 10 still closes with the data captured and the recommendation; DPVS removal can be revisited in a future milestone if needed.

- **D-12: Decision scope — D3D9 renderer only.** Phase 11 (D3D11 plugin) changes draw-call cost; DPVS savings depend on draw-call cost; verdict can flip under D3D11. The verdict doc MUST state explicitly: "This decision is valid for the D3D9 renderer. Phase 11 must re-measure under D3D11 before keeping the verdict." If Phase 10 says "remove," Phase 11's success-criteria list gains an item to confirm or reverse this decision after D3D11 is operational.

### Removal mechanism (if verdict is "remove")

- **D-13: Removal shape — source-edit the `OCCLUSION_CULLING` bit off in `RenderWorld.cpp:906`.** Single-line edit: change `(ms_disableOcclusionCulling ? 0 : DPVS::Camera::OCCLUSION_CULLING) | DPVS::Camera::VIEWFRUSTUM_CULLING` to `DPVS::Camera::VIEWFRUSTUM_CULLING` (drop the OCCLUSION_CULLING bit entirely). `resolveVisibility()` still gets called every frame — it does view-frustum culling AND portal traversal, which we keep. The DPVS occlusion query simply doesn't run. Matches the roadmap's "removed permanently from outdoor scenes; portal traversal retained" intent precisely. ALSO mirror the same change in the `#ifdef _DEBUG` branch at line 903 so debug builds match release behavior.

- **D-14: Delete the `disableOcclusionCulling` config key.** After D-13 lands, the config key is a dead toggle. Remove from `ConfigClientGraphics.cpp`: the `ms_disableOcclusionCulling` static (line 38), the `KEY_BOOL` declaration (line 101), the getter (line 245), and any header declaration. Also remove `RenderWorld::setDisableOcclusionCulling()` (line 1151) and `RenderWorld.h` line 104 — they have no callers after D-13. Small follow-up commit, separate from D-13 for clean bisect.

- **D-15: Instrumentation cleanup — single commit removes it all post-verdict.** The D3D9 timestamp queries, QPC timer pair, CSV logger, F11 keybind, F10 capture toggle, overlay strings, and `/setrunlabel` console command are Phase 10 scaffolding. After the verdict is documented and the conditional D-13/D-14 changes (if applicable) are committed, a single revert-shaped cleanup commit removes all of it. The codebase doesn't keep dead profiling toggles. Memory `feedback_empirical_validation_first.md` + the project habit of not leaving feature flags behind motivate this.

- **D-16: Verdict documentation lives at `docs/recon/10-dpvs-profiling.md`.** Per ROADMAP §Phase 10 success criterion #2 explicit path. Contents: methodology (D-01..D-04), scene + protocol (D-05..D-08), raw CSV file links (paths + a one-line manifest), summary stats table (DPVS-on vs DPVS-off, GPU/CPU/total × median/p95/p99/max/stdev), verdict + rationale per D-09..D-12, Phase 11 revisit note per D-12. The phase's `.planning/phases/10-dpvs-culling-experiment/10-SUMMARY.md` is a thin closeout artifact that links out to this doc — long-form research record stays in `docs/recon/`.

### Claude's Discretion

The following details are deliberately left to the researcher/planner:
- Exact CSV file path on disk (suggestion: `build-v2/dpvs-profile/<run-label>.csv` or similar)
- Overlay rendering surface — whether to use SwgCuiHud, an existing debug overlay, or a fresh `DebugPrimitive` text — researcher picks based on what already exists
- Console command grammar for `/setrunlabel` — match existing command-parser conventions
- Where the F10/F11 keybindings are wired (likely `clientDirectInput` or `SwgCuiHud`; check for conflicts with existing bindings)
- Whether to capture DPVS-reported visible-object counts as a diagnostic column (would help explain a GPU delta but not required for the verdict)
- Microsecond vs millisecond unit choice in the CSV (suggestion: microseconds for raw, millisecond derivations in the analysis script)

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Requirements & Roadmap
- `.planning/REQUIREMENTS.md` §DPVS — DPVS-01 (config flag + frame-time measurement) and DPVS-02 (conditional removal). Anchors the phase scope.
- `.planning/ROADMAP.md` §Phase 10 — Goal statement, success criteria, depends-on Phase 7 / optional Phase 9.
- `.planning/PROJECT.md` — Core value invariant: every change must leave the client bootable.

### v2 tree & SWGSource VM
- `D:/Code/swg-client-v2/` — Active repo. Branch `koogie-msvc-cpp20-base`. All Phase 10 work lands here.
- Build entry point: `D:/Code/swg-client-v2/src/build/win32/swg.sln` via MSBuild `/p:Configuration=Debug /p:Platform=Win32 /p:PlatformToolset=v145` (NOT CMake — Koogie did not port to CMake).
- Runtime stage: `D:/Code/swg-client-v2/stage/SwgClient_d.exe` against SWGSource VM at 192.168.1.200:44453.
- Phase 9 closeout artifact: `.planning/phases/09-stlport-msvc-stl/09-02-SUMMARY.md` (Tatooine zone-in PASS), 09-CONTEXT.md D-13..D-19 for the v2 working-tree topology that Phase 10 inherits.

### DPVS code (v2 paths — all source-edits land here)
- `src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp:78` — `ms_disableOcclusionCulling` static declaration
- `src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp:903-908` — `cullingParameters` bitmask for the DPVS camera (debug + release branches)
- `src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp:1037-1047` — `resolveVisibility()` call site wrapped in `ms_dpvsQueryProfilerBlock`
- `src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp:1151` — `setDisableOcclusionCulling()` setter
- `src/engine/client/library/clientGraphics/include/public/clientGraphics/RenderWorld.h:104` — setter declaration
- `src/engine/client/library/clientGraphics/src/shared/ConfigClientGraphics.cpp:38, 101, 245` — `disableOcclusionCulling` config-key plumbing
- `src/external/3rd/library/dpvs/interface/` — DPVS public headers (`dpvsCamera.hpp`, `dpvsCell.hpp`, `dpvsObject.hpp`, `dpvsModel.hpp`). Library STAYS regardless of verdict (indoor portals need it).

### Profiling & instrumentation references
- `src/engine/shared/library/sharedDebug/include/public/sharedDebug/Profiler.h` — Existing in-engine Profiler. `ProfilerBlock`, `NP_PROFILER_AUTO_BLOCK_DEFINE`, `NP_PROFILER_BLOCK_ENTER`. Reused per D-02.
- D3D9 timestamp-query reference: `IDirect3DDevice9::CreateQuery` with `D3DQUERYTYPE_TIMESTAMP` + `D3DQUERYTYPE_TIMESTAMPDISJOINT`. MSDN/community docs. Per D-01.
- Windows `QueryPerformanceCounter` / `QueryPerformanceFrequency` for the CPU pair. Per D-02.

### Capture protocol & VM setup
- Memory `project_vm_server_setup.md` — VM + SWGSource startup sequence each session (manual IP, Oracle startup, stationchat, ant build, client launch).
- Memory `project_v2_fork_location.md` — v2 working-tree topology, remotes, branch state.
- Memory `feedback_source_edits_allowed.md` — source edits authorized for v2 (D-13, D-14, D-15 all involve source edits).
- Memory `feedback_empirical_validation_first.md` — recurring lesson: prove premise empirically before locking strategic decisions. Phase 10 IS this in operational form (measure before deciding).

### Verdict output location
- `docs/recon/10-dpvs-profiling.md` — Path named in ROADMAP success criterion #2. NEW doc; doesn't exist yet. Sibling format to existing `docs/recon/05-client-boot-sequence.md`, `07-swg-source-diff-findings.md`.
- `.planning/phases/10-dpvs-culling-experiment/10-SUMMARY.md` — Phase closeout artifact (thin; links to docs/recon doc).

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- **`disableOcclusionCulling` config-key plumbing is ALREADY complete end-to-end.** `ConfigClientGraphics.cpp:101` reads it from `client.cfg`; `RenderWorld.cpp:78` is the consumer static; `RenderWorld.cpp:906` is the consumer call site; `RenderWorld::setDisableOcclusionCulling()` at line 1151 is the runtime setter. **DPVS-01 success criterion #1 ("`disableOcclusionCulling` config key is read from `client.cfg` and wired to `RenderWorld.cpp` line 78") is already satisfied by the Koogie merge anchor inherited at Phase 9 closeout.** Phase 10's only DPVS-01 work is the *measurement* half, not the config-wiring half.
- **In-engine Profiler infrastructure is in place.** `ProfilerBlock ms_dpvsQueryProfilerBlock("dpvs query and immediate callbacks")` at `RenderWorld.cpp:98` already brackets the DPVS query work. `NP_PROFILER_AUTO_BLOCK_DEFINE("resolveVisibility")` already brackets the call at line 1041. The Profiler's log-dump is enabled via config; no new infrastructure needed.
- **`_DEBUG`-only `ms_disableResolveVisibility` flag (line 81) and `ms_forceDisableOcclusionCulling` (line 82)** already exist as debug knobs. Phase 10 does NOT touch these; they're a separate developer feature. F11 calls `setDisableOcclusionCulling()`, which manipulates `ms_disableOcclusionCulling` (the release-build flag), not the debug ones.

### Established Patterns
- **Engine input bindings** live in `client.cfg` `[ClientInput]` plus the compiled `commands.tab` system. Adding F10/F11 = an entry in the input config that maps to a registered command. Researcher to identify the cleanest seam (likely an existing `Game::registerCommand` or `CommandTable::register` pattern).
- **Console commands** (for `/setrunlabel`) follow the `CuiCommandParser` / `CommandParser` registration pattern. Existing commands in `SwgCuiCommandParserDefault` are the template.
- **CSV / debug log output** — the engine has `DebugFlags`, `DEBUG_REPORT_LOG_PRINT`, and `Report`-tier logging. Researcher to decide whether to piggyback on one of these or open a fresh file handle. A fresh file handle is probably cleaner for CSV-shaped output.
- **On-screen overlay** — there's an existing debug overlay system (`DebugFlags`, `DebugPrimitive`, the F1 console). Researcher picks between adding to an existing overlay or rendering text via the UI canvas.

### Integration Points
- `clientDirectInput` library — where keybind handlers register. F10/F11 toggle handlers wire here.
- `clientUserInterface` / `SwgCuiHud` — where the overlay text gets composited if the researcher picks a UI-canvas approach.
- `RenderWorld.cpp` — three integration points: timer-pair around line 1046 (D-02), bitmask edit at line 906 (D-13), config-key plumbing teardown across lines 78/906/1151 (D-14).
- `ConfigClientGraphics.cpp` — config-key plumbing teardown sites for D-14.

</code_context>

<specifics>
## Specific Ideas

- **Capture session ordering (suggested):** alternate ON-OFF-ON-OFF-ON-OFF rather than ON-ON-ON-OFF-OFF-OFF, so any time-of-day drift or NPC-population drift across the session affects both conditions equally.
- **CSV column suggestion:** `frame_no, wall_ms_iso, run_label, dpvs_occlusion_flag, gpu_us, cpu_qpc_us, profiler_dpvs_us, total_frame_ms, visible_object_count, draw_call_count`. Researcher refines.
- **Run-label suggestion:** Kenny sets via `/setrunlabel mosEisley-pass1-on` before pressing F10. Capture writes the label into every frame row. Aggregation script groups by label.
- **Threshold rule arithmetic:** the analysis script reads all CSVs, groups by run-label, aggregates per condition (DPVS-on vs DPVS-off across the 3 passes per side), computes median/p95/p99/max/stdev per metric (GPU/CPU/total), then emits a verdict line: `verdict = remove if (off.total_median <= on.total_median and off.total_p95 <= on.total_p95) else keep`.
- **Overlay should display:** current `disableOcclusionCulling` state (color-coded: red=ON, green=OFF), current run-label, frame-count of current capture, and a blinking "REC" indicator while F10 capture is active. Lets Kenny confirm at a glance that he's recording the condition he thinks he is.
- **Phase 11 inheritance note:** when Phase 11 ROADMAP is updated (post-Phase-10), add a success criterion variant: "If Phase 10 verdict was 'remove', confirm under D3D11 plugin that the verdict still holds via the same protocol; if 'remove' was reversed, re-add the OCCLUSION_CULLING bit before declaring D3D11 done."

</specifics>

<deferred>
## Deferred Ideas

- **Multi-zone study (Coronet starport portal, Theed plaza).** Considered as gray-area option but rejected in favor of single-zone (Mos Eisley plaza, D-05) to keep Phase 10 bounded. If the single-zone verdict is "inconclusive" per D-11, multi-zone could be a v3+ revisit.
- **Scripted camera-path recording/replay.** Considered as the strongest fairness mechanism but rejected as out of proportion for a profiling experiment (D-06). v3+ candidate if anyone runs more rendering experiments.
- **Permanent profiling instrumentation (keep CSV logger + timestamp queries as a library after verdict).** Considered but rejected per D-15. The project habit is not to leave dead profiling toggles behind. Memory: `feedback_empirical_validation_first.md` is about *prove the premise*, not *keep the harness forever*.
- **DPVS library deletion.** REQUIREMENTS.md pins the DPVS library as out-of-scope for v2 because indoor portal traversal still needs it. v3+ candidate IF a future cleanup proves portal traversal can stand without the DPVS library object graph (it probably can — the codebase uses DPVS's `Cell`/`Object`/`Camera` types, which are decoupleable, but that's a separate study).
- **Re-running Phase 10 under D3D11.** Belongs to Phase 11 — D-12 adds it as a Phase 11 success-criterion. Tracked there, not deferred to v3+.
- **Capturing draw-call count as a diagnostic column.** Listed in CSV column suggestion but optional. Researcher decides based on what the engine readily exposes (some D3D9 device-state introspection works; some doesn't depending on driver).

</deferred>

---

*Phase: 10-DPVS-Culling-Experiment*
*Context gathered: 2026-05-10*
