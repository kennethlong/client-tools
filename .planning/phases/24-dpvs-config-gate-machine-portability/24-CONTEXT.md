# Phase 24: DPVS Config-Gate + Machine Portability - Context

**Gathered:** 2026-06-12
**Status:** Ready for planning

<domain>
## Phase Boundary

Operationalize the Phase 23 DPVS verdict behind a config knob, and make a fresh clone + build produce a bootable, machine-independent client. Three requirements: HARD-01 (DPVS occlusion config-gate), PORT-01 (no machine-specific absolute paths; `stage/override` + `stage/miles` handled), PORT-02 (`stage/client_d.cfg` cleaned of accumulated Phase-11+ test settings). Core invariant: the client stays bootable to character select under BOTH `rasterMajor=5` (D3D9) and `rasterMajor=11` (D3D11) after the phase.

Out of scope: acting further on DPVS beyond the config-gate (scene-aware culling redesign is future work), the corner-snap fix (Phase 25), instrumentation removal (Phase 26), D3DCompile port (Phase 27), TRE tool (Phases 28–30).

</domain>

<decisions>
## Implementation Decisions

### DPVS knob design
- **D-01:** Add a `dpvsOcclusionMode = auto | on | off` config knob per the folded todo's design, replacing the hardcoded Option α `#else` branch in `RenderWorld.cpp` (~line 914). `auto` = OR the `DPVS::Camera::OCCLUSION_CULLING` bit into the culling parameters only when the camera is NOT inside a POB cell (outdoor on / indoor off, the Phase 23 measured optimum). `on`/`off` = unconditional overrides.
- **D-02:** **Default is `off`** — NOT `auto`. Rationale: Phase 23 deltas were measured on a Debug build; in Release they likely compress to sub-millisecond. Shipped behavior stays Option α (occlusion removed) until a Release-build measurement justifies flipping the default.
- **D-03:** Release-build A/B measurement is **deferred to Phase 26** (it pairs with instrumentation removal: measure with the harness before stripping it, then decide whether to flip the default to `auto`). Do NOT add a Release-safe CSV variant in this phase.
- **D-04:** The `_DEBUG`-only F11 force-disable flag (`ms_forceDisableOcclusionCulling`) keeps working on top — force-disable wins over config.

### client_d.cfg cleanup (PORT-02)
- **D-05:** Full per-key audit with three verdicts: keep / remove / verify-then-remove. Risky removals (e.g. `runtimeDisableAsynchronousLoader=true` — removing it re-enables the background loader thread, never re-tested since the Phase 19 corruption root causes were fixed elsewhere) get an explicit boot + play test. Obviously-dead keys (vivox `voiceChatEnabled`, `disableG15Lcd` — subsystems deleted in v2.1) just go. Phase ends with the dual-renderer boot gate.
- **D-06:** Canonical committed `rasterMajor=11` (D3D11 — the modernized, parity-achieved renderer). D3D9 stays one config flip away; the boot gate tests both regardless.
- **D-07:** Small engine-side default flips are ALLOWED so permanently-required override keys can disappear from the cfg (e.g. make multi-stream skinned VBs the engine default so `disableMultiStreamVertexBuffers=false` goes away; wire or no-op the D3D11 Bloom slot so `[ClientGame/Bloom] enable=0` goes away). Each flip must be small, reversible, and boot-verified.

### Path portability mechanism (PORT-01)
- **D-08:** A **setup script generates the cfgs** from a tracked template (PowerShell; accepts/prompts for the machine's TRE asset root, e.g. `D:/Code/SWGSource Client v3.0/`). The script auto-substitutes repo-relative paths (e.g. `stage/override`) from the repo root it runs in — no user input needed for those. This is the build-system-independent successor to the old CMake `configure_file` story (whose stale header still tops `client_d.cfg`).
- **D-09:** **Template tracked, generated cfgs untracked**: `stage/client_d.cfg` and `stage/client.cfg` come off the tracked tree (gitignored); the template + script are the source of truth. This permanently fixes the recurring "DO NOT COMMIT this cfg" problem and lets `rasterMajor` be flipped freely per-machine.
- **D-10:** PORT-01 verification is a **real fresh-clone test**: clone the repo to a new directory on this machine, run setup script + build, boot to character select.

### Miles redist handling (PORT-01)
- **D-11:** **Vendored redist + postbuild copy**: add the 2 missing 3D providers (`Msseax.m3d`, `msssoft.m3d`) to the already-tracked `src/external/3rd/library/miles-7.2e/redist/` (which already holds 9 of stage/miles' 12 files: all 3 `.asi` codecs, all 5 `.flt` filters, `mss32.dll`), then a postbuild step or the setup script copies redist → `stage/miles`. Single source of truth; `stage/` stays generated. Provenance verified clean: `stage/miles` is byte-identical to the public SWGSource Client v3.0 `miles/` dir, so nothing new in kind enters git (~240 KB added). `AudioCapture.flt` likely excluded (AudioCapture subsystem removed in v2.1 Phase 13 — researcher verifies the engine doesn't enumerate it); zero-byte `index.html` excluded.
- **D-12:** Absence detection lives in **both** places: the setup script validates the expected files exist at setup time, AND the engine emits one clear startup warning (not a per-sample flood) if the Miles codec providers fail to load. Catches fresh clones, later deletions, and non-git deployments.

### Claude's Discretion
- Config key naming and section placement (e.g. `[ClientGraphics/Dpvs] occlusionMode` vs `[ClientGraphics] dpvsOcclusionMode`) — the existing `[ClientGraphics/Dpvs]` section and the registered DebugFlag naming are reasonable precedents.
- Auto-mode detection mechanics: `ms_cameraCell` is already tracked at file scope in `RenderWorld.cpp`; per-frame evaluation vs cell-notification hook is the planner's call (the existing `ms_lastCullingParameters` change-detection already makes per-frame cheap).
- Setup script UX details (prompt vs parameter, validation messages), template format, and exact gitignore arrangement.
- Per-key verdicts for the client_d.cfg audit beyond the ones explicitly decided above — anything uncertain stays as a documented cfg key rather than a risky removal.

### Folded Todos
- **`2026-06-12-config-gate-dpvs-occlusion-per-phase-23-verdict.md`** — "Config-gate DPVS occlusion per Phase 23 verdict (outdoor on, indoor off)". Tagged `resolves_phase: 24`. Its proposed design (knob shape, auto semantics, F11 interplay, RenderWorld.cpp file/line pointers) is the adopted starting point, with one amendment: default `off` instead of `auto` (D-02), and its optional item 4 (Release-safe CSV variant) explicitly NOT taken (D-03).

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### DPVS verdict + gate site
- `docs/recon/23-dpvs-d3d11-profiling.md` — THE spec for HARD-01 per ROADMAP: Phase 23 verdict (outdoor `keep` / indoor `remove`, both flipped vs Phase 10), per-scene data, Debug-build caveat.
- `docs/recon/10-dpvs-profiling.md` — Phase 10 D3D9 baseline the verdict revises (context only).
- `.planning/todos/pending/2026-06-12-config-gate-dpvs-occlusion-per-phase-23-verdict.md` — folded todo: proposed knob design + exact code pointers.
- `src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp` (lines ~83, ~229, ~904–929, ~1190–1202) — `ms_forceDisableOcclusionCulling` DebugFlag, the Phase 23 `_DEBUG` A/B branch, the Option α `#else` (`cullingParameters = VIEWFRUSTUM_CULLING` only), `ms_lastCullingParameters` change detection, and `ms_cameraCell` for POB detection.

### Portability surface
- `stage/client_d.cfg` — the PORT-02 cleanup target; per-key comments document each setting's origin (Phase 11/17/18/19 toggles, diagnostics, instrumentation flags).
- `stage/client.cfg` — Release cfg; also carries the absolute `stage/override` searchPath (line ~27) and the same machine-specific TRE root; becomes generated alongside client_d.cfg (D-09).
- `src/external/3rd/library/miles-7.2e/redist/` — already-tracked Miles redist (9 of 12 files); D-11 adds the 2 `.m3d` providers here.
- `D:/Code/SWGSource Client v3.0/miles/` — external known-good copy source for the missing `.m3d` files (byte-verified identical to `stage/miles`).

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `RenderWorld.cpp` already tracks `ms_cameraCell` (`const CellProperty *`) and `ms_dpvsCameraCell` at file scope — POB-cell membership for auto mode is a comparison against the world cell, no new plumbing.
- The culling-parameter change-detection (`cullingParameters != ms_lastCullingParameters` → `setParameters`) already handles cheap per-frame toggling of the occlusion bit.
- The `[ClientGraphics/Dpvs]` config section exists (DebugFlag registration at `RenderWorld.cpp:229`, instrumentation key in client_d.cfg) — natural home for the new knob.
- `src/external/3rd/library/miles-7.2e/redist/` — vendored redist dir, mostly complete.
- Koogie's existing postbuild copy (lands `SwgClient_d.exe` + `gl0X_d.dll` into `stage/`) — pattern to extend for the miles redist copy. Do not modify Koogie's source-level changes without strong reason; adding a copy step is additive.

### Established Patterns
- Dual-renderer boot gate (`rasterMajor=5` and `=11` to character select) is the phase-close invariant — established across v2.1/v2.2 phases.
- Debug exe reads `client_d.cfg`, Release exe reads `client.cfg` — both files are in scope for path de-hardcoding; cleanup (PORT-02) targets client_d.cfg specifically.
- `stage/override/` is tracked in git (commit `cb2b16c1a`) and loaded via `searchPath` with priority above the TREs — the generated cfg must preserve this priority arrangement.

### Integration Points
- `RenderWorld::install` / drawScene for the knob read + per-frame bit selection.
- Audio init path (Miles startup / provider enumeration) for the D-12 runtime absence warning — researcher locates the exact failed-sample warning site from the 2026-06-12 audio fix.
- `.gitignore` + the SwgClient postbuild for the generated-cfg + miles-copy arrangement.

</code_context>

<specifics>
## Specific Ideas

- The setup script is the explicit successor to the dead CMake `configure_file` story — client_d.cfg's header still claims "Generated by CMake … re-run cmake to regenerate", which is false since the Phase 9 Option-D MSBuild adoption. The new template should carry forward the Phase 6 decision annotations (D-01..D-09 comments) where still true.
- "Not a silent half-dead-audio failure" has a known signature: UI rollovers work but everything else is silent + a 141k-line failed-sample warning flood. The runtime check should fire ONE clear warning, not per-sample.
- The mystery `searchPath_00_12=D:/swg_dev_bundle` entry in client_d.cfg is an audit candidate — likely removable, verify nothing references it.

</specifics>

<deferred>
## Deferred Ideas

- **Phase 26 scope addition:** Release-build DPVS A/B measurement using the new knob (+ the D-15 harness, before it's stripped), then decide whether to flip the `dpvsOcclusionMode` default from `off` to `auto`. Capture this when Phase 26 is discussed/planned.

### Reviewed Todos (not folded)
- `2026-05-15-swgsource-vs-whitengold-tre-asset-diff.md` — the TRE compare tool's first use case; belongs to Phases 28–30.
- `2026-05-31-port-d3d9-shader-compile-to-d3dcompile.md` — HARD-05, Phase 27.
- `2026-05-15-cantina-corner-snap-engine-improvement.md` — HARD-02, Phase 25.
- `2026-05-27-options-toolbar-cooldown-ui-data-mismatch-crash.md` — HARD-04, Phase 26.

</deferred>

---

*Phase: 24-DPVS Config-Gate + Machine Portability*
*Context gathered: 2026-06-12*
