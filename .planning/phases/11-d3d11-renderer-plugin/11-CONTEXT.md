# Phase 11: D3D11 Renderer Plugin - Context

**Gathered:** 2026-05-15
**Status:** Ready for planning
**Active tree:** `D:/Code/swg-client-v2/` branch `koogie-msvc-cpp20-base` (v2).
whitengold (`D:/Code/swg-client/`) is research history; no source edits there.

<domain>
## Phase Boundary

Ship a new `Direct3d11.dll` plugin that satisfies the existing 119-function
`Gl_api` table, runs alongside the existing `Direct3d9.dll` plugin
(selectable at startup via `client.cfg`), and renders Tatooine outdoor +
Mos Eisley cantina interior with full subsystem coverage (terrain,
character/creature skeletal, particles, UI HUD overlays) under D3D 11.0
feature level (HLSL SM5.0, Win7+ baseline). D3D11 becomes the default once
it reaches the stability and visual criteria from SPEC.md; D3D9 stays as a
maintained fallback for the transition phase.

The phase delivers:
1. A new `Direct3d11/` source tree at `src/engine/client/application/Direct3d11/`
   producing `Direct3d11.dll` (gl11_*.dll naming per existing convention).
2. A FFP-usage spike (first plan) that decides whether the FFP pixel-shader
   generator gets built or descoped.
3. Subsystem coverage for terrain, character/creature skeletal, particles,
   and UI HUD overlays in both target scenes.
4. Visual-parity screenshot evidence at known camera positions.
5. DPVS remeasurement under D3D11 + architectural decision per SPEC R7.

Out of scope: space scenes, other planets, pixel-perfect parity, performance
parity, deletion of the D3D9 plugin tree, restored source-level DPVS
instrumentation, lost-device recovery code (no D3D11 equivalent). Full
out-of-scope list in `11-SPEC.md` §Boundaries.

</domain>

<spec_lock>
## Requirements (locked via SPEC.md)

**7 requirements are locked.** See `11-SPEC.md` for full requirements,
boundaries, acceptance criteria, and the ambiguity report (0.11 — well
under the 0.20 gate).

Downstream agents MUST read `11-SPEC.md` before planning or implementing.
Requirements (D3D11-01..D3D11-05 + DPVS remeasurement per CONTEXT D-12 +
visual-parity verification method) are not duplicated here.

**In scope (from SPEC.md):**
- New `Direct3d11` plugin source tree producing `Direct3d11.dll`
- D3D 11.0 feature level baseline (HLSL SM5.0; Win7+ hardware minimum)
- Tatooine outdoor (Mos Eisley plaza or equivalent) rendering
- Mos Eisley cantina interior (indoor cell + portal traversal) rendering
- Four target subsystems: terrain, character/creature skeletal, particles,
  UI HUD overlays
- VSPS programmable shader recompilation to SM5.0
- FFP emulation via runtime pixel shader generator — conditional on spike
- DPVS remeasurement via external tools and architectural decision
- Runtime renderer selection via existing `client.cfg` config key
- D3D9 plugin maintained as fallback (kept building, kept stable)
- Reference screenshots and visual comparison notes

**Out of scope (from SPEC.md):**
- Space scenes (JTL nebulae, ship combat)
- Other planets (Naboo, Corellia, Dantooine, Endor)
- Pixel-perfect parity with D3D9
- Performance parity with D3D9 ("playable" is enough)
- Deletion of `Direct3d9` plugin tree
- Restored source-level DPVS instrumentation
- Lost-device recovery code (no D3D11 equivalent)
- Bink video playback (already removed in Phase 7)
- Cinematics (already config-disabled)
- Compute shaders, tessellation, geometry shaders (unused by SWG)

</spec_lock>

<decisions>
## Implementation Decisions

### D3D11 plugin source-tree layout

- **D-01: Clean rewrite of `Direct3d11/` from scratch — D3D9 as design
  reference, not as starting skeleton.** Do not copy the 89-file
  `Direct3d9/` tree wholesale. Author idiomatic D3D11 directly: explicit
  `ID3D11Device` + `ID3D11DeviceContext` lifetime, `Microsoft::WRL::ComPtr`
  for COM resource ownership, modern shader-model constant buffers, no
  `D3DPOOL_MANAGED`, no `OnLostDevice`/`OnResetDevice` paths (D3D11-02
  acceptance: zero grep matches for those primitives). Read D3D9 source
  for design intent on each Gl_api function — but write the D3D11
  implementation from a clean page. RATIONALE: D3D9 idioms (lost-device,
  pool flags, fixed-function combiners) are deeply baked into D3D9 file
  shapes; a copy carries forward what we explicitly want excised by
  D3D11-02. Smaller initial commits, modern C++ patterns, no D3D9 bug
  port-forward. Per memory `feedback_dont_modify_koogie_changes.md` —
  Direct3d11 is wholly new code on top of Koogie's modernized tree;
  Koogie's diagnostic patches are untouched.

### Renderer selection mechanism

- **D-02: Extend existing `rasterMajor` int — D3D11 = `rasterMajor=11` →
  `gl11_d.dll`.** No new config key. The selection plumbing at
  `Graphics.cpp:184` (read `rasterMajor`) → `:209` (range check) → `:219`
  (`sprintf("./gl%02d_d.dll", rasterMajor)`) → `:220` (`LoadLibrary`)
  stays as-is. Range check at line 209 extends from `[5..7]` to also
  accept `11`; the `DLL_NAME_FORMAT` macro is unchanged.
  RATIONALE: zero churn to the selection plumbing the engine already
  trusts; mnemonic mapping (11 = D3D11); future D3D12 would naturally be
  `rasterMajor=12`. The 8/9/10 gap is acceptable visual oddity, not a
  functional issue. Existing D3D9 variants (5/6/7) stay valid;
  `Direct3d9.dll` continues to build and load per D3D11-01 acceptance.
  Output DLL names follow the existing `gl%02d_X.dll` convention:
  `gl11_d.dll` (Debug), `gl11_r.dll` (Release), `gl11_o.dll` (Optimized).

- **D-02a: Update `GraphicsOptionTags` for the new range.** `Graphics.cpp:209-215`
  currently sets `TAG_DX9=true` when `rasterMajor ∈ [5,7]` and `DEBUG_FATAL`s
  otherwise. The `[11]` branch sets a new `TAG_DX11` (or equivalent — researcher
  picks the cleanest seam to add the tag). Existing `TAG_DX9` callers across
  the codebase need audit during the plugin scaffold plan.

### Shader compile pipeline

- **D-03: Hybrid — compile-on-first-use via `D3DCompile`, persistent disk
  cache, recompile on source-hash change.** First run of a given HLSL
  shader: invoke `D3DCompile` (from `d3dcompiler_47.dll`, ships with
  modern Windows SDK / DirectX runtime — verify packaging path during
  research), then serialize the resulting bytecode blob to a per-shader
  `.cso` file in a local cache directory. Subsequent runs: load the
  cached blob directly via `CreateVertexShader` / `CreatePixelShader`,
  bypassing compile. Cache invalidates when source-hash header (stored
  inside the .cso or alongside it) doesn't match current source hash.
  RATIONALE: smallest delta from current D3D9 runtime-compile model
  (`Direct3d9_VertexShaderData.cpp:433 D3DXCompileShader`) for the
  first port pass; fast warm starts on subsequent launches; cache
  survives shader edits (recompile only what changed); no upfront
  asset-pipeline rebuild. Acceptable cost: first-run latency cost is
  paid once per fresh checkout. Shader-compile target strings update
  from D3D9-era `vs_1_1`/`vs_2_0`/`ps_1_1`/`ps_2_0` to `vs_5_0`/`ps_5_0`
  (SPEC R3). Cache-directory location is `<Claude's discretion>` (see
  below).

### FFP spike methodology (D3D11-04 first-plan deliverable)

- **D-04: Two-phase spike — static analysis first, runtime instrumentation
  to validate.** Phase A (static): grep + parse asset-side shader
  templates and material definitions for FFP-tier references
  (`D3DTSS_COLOROP`, `D3DTSS_ALPHAOP`, `MODULATE`, `ADD`, `SELECTARG1`,
  `D3DTOP_*` verbs); output a candidate list of FFP-using materials
  with a coverage map of which materials are referenced by Tatooine /
  Mos Eisley cantina assets. Phase B (runtime, contingent on Phase A
  finding non-empty FFP candidates): add temporary instrumentation in
  the D3D9 plugin's FFP path (`Direct3d9_StateCache::setTextureStageState`
  at `Direct3d9_StateCache.cpp:159,162` and the
  `Direct3d9_ShaderImplementationData` D3DTSS_* setters at lines
  147,152,382-383) that logs every FFP-tier activation with a
  scene-context tag; Kenny plays Tatooine outdoor + Mos Eisley cantina
  for the target ≥5 min each per SPEC R5; CSV gets dumped; instrumentation
  is reverted in a single THROWAWAY commit (mirrors Phase 10 D-15 pattern
  established in plan 10-07).
  RATIONALE: static analysis alone misses runtime-conditional FFP paths
  (material variants chosen by shader-template fallback when SM2.0
  pathway fails on a given GPU/driver, etc.). Runtime alone is expensive
  and noisy. Static→runtime gives a bounded validation set; if Phase A
  returns empty, Phase B is skipped (FFP descope evidence is the
  static-analysis output itself). Spike result committed as a written
  finding in a Phase 11 plan artifact per SPEC R4 acceptance.
- **D-04a: Verdict threshold for the FFP generator decision.** If Phase B
  shows any FFP activation in either target scene, build the runtime HLSL
  pixel-shader generator covering at minimum MODULATE, ADD, SELECTARG1
  (the three SPEC R4 calls out). If Phase B is empty across both target
  scenes, descope the FFP generator entirely — document static + runtime
  evidence in a Phase 11 plan artifact and remove `Direct3d9_ffp.dll`'s
  mirror from the D3D11 plan's responsibility set (the existing D3D9
  FFP variant `gl06_*.dll` stays on disk for D3D9-side fallback; D3D11
  doesn't need to mirror it).

### Cross-cutting D3D9 protection

- **D-05: D3D9 plugin stays buildable and loadable through every Phase 11
  wave.** Per SPEC §Constraints + D3D11-01 acceptance "`Direct3d9.dll`
  continues to build and load unchanged." Practical operational rule:
  every plan that lands a D3D11 change must close with a quick
  build-verify pass on `Direct3d9.vcxproj` (and `Direct3d9_ffp.vcxproj` /
  `Direct3d9_vsps.vcxproj` if changes touch shared engine surface).
  Boot-verify cadence (i.e., relaunch SwgClient_d.exe under `rasterMajor=5`
  and reach char-select) is at wave boundaries, not per-plan — the
  inner-loop build check catches the most likely regressions; full
  boot covers integration gaps. Mirrors the CLEAN-05 / STL-05 gate
  pattern from Phase 7 and Phase 9. Pattern reuse rather than fresh
  invention.

### Claude's Discretion

The following details are deliberately left to the researcher / planner:

- **Shader-cache directory location** (D-03). Suggestion: `<user-data>/shader-cache/`
  or `build-v2/shader-cache/`. Researcher decides based on existing user-data
  path conventions in the engine + Phase 11 build-output structure. Must
  survive across launches; must not be committed to git.
- **`d3dcompiler_47.dll` packaging path** (D-03). System-supplied via
  modern Windows SDK / DirectX redist, or vendored alongside Phase 11
  build output? Researcher checks what ships on minimum-target hardware.
- **D3D11 SDK header sourcing.** SPEC §Constraints notes "D3D11 SDK headers
  will need adding to that or a parallel directory" to the vendored
  `src/external/3rd/library/directx9/`. Options: rely on Windows SDK
  system headers (cleanest for D3D11 since the SDK is stable), vendor
  alongside DX9, or new parallel directory. Researcher picks based on
  precedent in the v2 tree's modernized include patterns.
- **`Microsoft::WRL::ComPtr` use** (D-01). Idiomatic D3D11 resource
  ownership; whether to use it project-wide in `Direct3d11/` source or
  pick a different RAII wrapper. ComPtr is the recommended default;
  researcher justifies alternatives if any.
- **Wave boundaries for the clean-rewrite plan series.** Suggested
  progression: spike (D-04) → plugin scaffold + Gl_api stub table
  (D3D11-01) → clear-to-color minimum-viable boot → resource layer
  (textures, vertex/index buffers, render targets) → shader layer
  (SM5.0 recompile + cache plumbing) → state cache + draw-call wiring
  → subsystem coverage (terrain → skeletal → particles → HUD) →
  DPVS remeasurement → visual-parity screenshots + comparison notes.
  Researcher refines into a plan series in `/gsd-plan-phase`.
- **DPVS remeasurement tool choice** (SPEC R7: "PIX / Nsight / GPUView").
  PIX on Windows is the natural D3D11 default; Nsight requires NVIDIA
  GPU; GPUView is a Windows-platform option. Researcher picks based on
  Kenny's hardware (likely NVIDIA per prior memory; check) and
  capture-protocol fit.
- **Renderer-tag plumbing for `TAG_DX11`** (D-02a). Where the new tag
  declaration goes; which call sites need to learn about it (terrain
  fallback paths, skeletal animation, particle effects often query
  `GraphicsOptionTags`).
- **D3D9 baseline screenshot capture timing.** SPEC R6 requires four
  reference screenshots (D3D9 outdoor, D3D11 outdoor, D3D9 cantina,
  D3D11 cantina). D3D9 baseline pair can be captured anytime D3D9 still
  works (suggestion: first plan after the FFP spike, before any D3D11
  code lands) so D3D11 captures are matched against a stable reference.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Locked specs & roadmap (read first)

- `.planning/phases/11-d3d11-renderer-plugin/11-SPEC.md` — Locked
  requirements (7), boundaries, constraints, acceptance criteria.
  MUST read before planning. Ambiguity 0.11 (≤ 0.20 gate).
- `.planning/REQUIREMENTS.md` §D3D11 — D3D11-01..D3D11-05 + ROADMAP
  success criterion #6 (DPVS remeasurement under D3D11).
- `.planning/ROADMAP.md` §Phase 11 — Goal statement, success criteria
  (especially #6 DPVS remeasurement decision tree).
- `.planning/PROJECT.md` — Core value invariant: every change must
  leave the client bootable to character select. Plus reference-repo
  context (`C:\Code\swg-main`, retail SWG install for assets).

### v2 tree & SWGSource VM context

- `D:/Code/swg-client-v2/` — Active repo. Branch `koogie-msvc-cpp20-base`.
  All Phase 11 work lands here.
- Build entry point: `D:/Code/swg-client-v2/src/build/win32/swg.sln`
  via MSBuild `/p:Configuration=Debug /p:Platform=Win32
  /p:PlatformToolset=v145`. NOT CMake — Koogie did not port to CMake
  per Phase 9 CONTEXT D-09 (preserved at
  `.planning/phases/09-stlport-msvc-stl/09-CONTEXT.md`).
- Runtime stage: `D:/Code/swg-client-v2/stage/SwgClient_d.exe` against
  SWGSource VM at 192.168.1.200:44453.
- Phase 9 closeout: `.planning/phases/09-stlport-msvc-stl/09-02-SUMMARY.md`
  (Tatooine zone-in PASS evidence — the baseline Phase 11 builds on).

### Phase 10 inheritance (DPVS remeasurement basis)

- `.planning/phases/10-dpvs-culling-experiment/10-SUMMARY.md` —
  Phase 10 closeout. Verdict Option α (`remove` globally) applied at
  `RenderWorld.cpp:909/913`.
- `.planning/phases/10-dpvs-culling-experiment/10-CONTEXT.md` — Prior
  context including D-12 ("Phase 11 must re-measure under D3D11").
- `docs/recon/10-dpvs-profiling.md` — Long-form DPVS verdict record
  with the scene-conditional data Phase 11 will revisit.
- `RenderWorld.cpp:909/913` — Current `OCCLUSION_CULLING`-stripped
  bitmask call sites (debug + release branches). Phase 11 R7 conditional
  edits apply here if the verdict diverges from Option α.

### D3D9 plugin (read for design intent, NOT to copy)

- `src/engine/client/application/Direct3d9/src/win32/Direct3d9.cpp` —
  89-file plugin entry point; defines `Gl_api` table population and
  FFP/VSPS preprocessor gating. ~4,764 lines.
- `src/engine/client/application/Direct3d9/src/win32/Direct3d9_VertexShaderData.cpp:433` —
  Runtime `D3DXCompileShader` call site with D3D9-era shader-profile
  targets (`vs_1_1`/`vs_2_0`/`ps_1_1`/`ps_2_0`). D3D11 replaces with
  `D3DCompile` + `vs_5_0`/`ps_5_0`.
- `src/engine/client/application/Direct3d9/src/win32/Direct3d9_StateCache.cpp:159,162` +
  `Direct3d9_ShaderImplementationData.cpp:147,152,382-383` — FFP
  combiner instrumentation sites for the D-04 spike Phase B.
- `src/engine/client/application/Direct3d9/build/win32/Direct3d9.vcxproj` +
  `Direct3d9_ffp.vcxproj` + `Direct3d9_vsps.vcxproj` — vcxproj template
  for the new `Direct3d11.vcxproj`. Note: all three D3D9 variants build
  from the same source set with `FFP`/`VSPS` preprocessor flag gating;
  D3D11 plugin starts as a single variant.

### Engine-side renderer selection (D-02 edit sites)

- `src/engine/client/library/clientGraphics/src/win32/Graphics.cpp:184` —
  `ms_rasterMajor` read from config.
- `src/engine/client/library/clientGraphics/src/win32/Graphics.cpp:209-215` —
  Range check (currently `[5..7]`); D-02 extends to accept `11`.
- `src/engine/client/library/clientGraphics/src/win32/Graphics.cpp:219-220` —
  `LoadLibrary("./gl%02d_d.dll", rasterMajor)`. Unchanged by D-02.
- `src/engine/client/library/clientGraphics/src/win32/Graphics.cpp:235-238` —
  `GetApi` function-pointer resolution. Plugin export contract;
  `Direct3d11.dll` must export `GetApi` returning the 119-function
  `Gl_api` table.
- `src/engine/client/library/clientGraphics/src/shared/ConfigClientGraphics.cpp:82,131` —
  `rasterMajor` config-key plumbing. No edits needed if D-02 reuses
  the existing key.
- `src/engine/client/library/clientGraphics/include/public/clientGraphics/Gl_api.h`
  (or equivalent — researcher confirms path) — 119-function table
  contract surface.

### D3D11 SDK & runtime references

- Vendored DX9: `src/external/3rd/library/directx9/include/` +
  `src/external/3rd/library/directx9/lib/` — D3D9 SDK reference for
  current plugin's API surface.
- D3D11 SDK headers: **NOT yet present**. Sourcing decision is
  Claude's-discretion (see `<decisions>`).
- `d3dcompiler_47.dll` — required runtime for D-03 hybrid shader
  compile. Packaging path is Claude's-discretion.

### Output / verification artifacts

- `docs/recon/11-d3d11-screenshots/` — NEW dir. SPEC R6 reference
  screenshots (4 files: D3D9 outdoor, D3D11 outdoor, D3D9 cantina,
  D3D11 cantina) plus `comparison-notes.md` with PASS verdict.
- `docs/recon/11-dpvs-d3d11-remeasure.md` — NEW. SPEC R7 verdict
  + external-tool timing data + architectural decision (keep
  Option α / revert global / scene-aware split).
- `.planning/phases/11-d3d11-renderer-plugin/11-SUMMARY.md` —
  Thin Phase 11 closeout artifact; links to docs/recon docs.

### Pattern precedent (read for cross-cutting test-gate cadence)

- `.planning/phases/07-dead-code-track-a/` — CLEAN-05 boot-gate
  pattern (D-05 applies the same shape for D3D9 fallback protection).
- `.planning/phases/09-stlport-msvc-stl/09-02-SUMMARY.md` — STL-05
  Tatooine boot gate pattern (precedent for "matched zone-in
  verification = phase done").
- `.planning/phases/10-dpvs-culling-experiment/` plans 10-05..10-07 —
  THROWAWAY instrumentation pattern (D-04 spike Phase B revert
  follows the same shape).

### Operational memory (assistant-side)

- `feedback_dont_modify_koogie_changes.md` — Direct3d11 is wholly new
  code on top of Koogie's modernized tree. Koogie's diagnostic patches
  are not touched.
- `project_safecast_null_dynamic_cast.md` — RESOLVED 2026-05-15 via
  ContrailData D-18 guard port (commit 73e29eee7). Not a Phase 11 blocker.
- `project_cantina_corner_snap_engine_quirk.md` — Pre-existing SOE
  engine quirk, cross-client confirmed. Phase 11 cantina rendering
  is unaffected by this; do not re-investigate as a regression.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets

- **Renderer-selection plumbing is already in place.** `rasterMajor`
  int is read from `client.cfg` at `Graphics.cpp:184`, formatted into
  `gl%02d_d.dll` at `:219`, loaded at `:220`, and the `GetApi`
  function-pointer is resolved at `:235-238`. D-02 reuses this end-to-end
  — Phase 11 edits only the range check at `:209-215`.

- **`Gl_api` 119-function table is a stable contract.** The engine side
  (`clientGraphics`) calls through `ms_api->` indirection for every
  rendering op. The contract is unchanged by Phase 11 per SPEC
  §Constraints. D3D11 plugin only needs to *export* a `GetApi` function
  returning a populated 119-fn table — the engine doesn't care how
  it's implemented internally.

- **Profiler infrastructure (`Profiler.h`, `NP_PROFILER_AUTO_BLOCK_DEFINE`)
  is already wired.** Reusable in Phase 11 for CPU-side timing of
  draw-call paths during the D3D11 port (alongside the SPEC R7
  external-tool capture for the DPVS remeasurement).

- **POST_BUILD DLL staging pattern is established.** The existing
  `Direct3d9.vcxproj` family stages its outputs (`gl05_d.dll`, etc.)
  to `compile/win32/Direct3dN/Debug/` via the build process; engine
  loads from the runtime stage directory. `Direct3d11.vcxproj` follows
  the same pattern producing `gl11_d.dll`.

- **THROWAWAY instrumentation pattern proven in Phase 10.** Plan 10-07's
  single revert-shaped commit removed 726 lines across 12 files.
  D-04 spike Phase B's instrumentation revert mirrors this pattern
  exactly — comment markers (`// THROWAWAY D-04`) at every edit site,
  revert lands in one atomic commit after the spike write-up.

### Established Patterns

- **vcxproj-in-`build/win32/`-per-tool** is the project structure
  convention. `Direct3d11/build/win32/Direct3d11.vcxproj` is the
  natural placement.

- **Per-DLL preprocessor variants from shared sources** is the D3D9
  precedent (FFP, VSPS, FFP+VSPS flags). D3D11 starts as a single
  variant (no FFP/VSPS split — D3D11 has no FFP pipeline and SM5.0
  unifies the shader story).

- **Runtime A/B between plugins via config flag** is established by
  `rasterMajor`. No relog/restart-config-loading complexity needed —
  just edit `client.cfg`, relaunch, different plugin loads.

- **Source-edit budget is OPEN in v2** per memory `feedback_source_edits_allowed.md`
  (recorded during Phase 9 / Phase 10). Direct3d11 plugin authoring +
  Graphics.cpp range-check edit are both in-scope.

- **Cross-cutting boot-verification gate** at phase / wave boundaries
  follows CLEAN-05 (Phase 7) and STL-05 (Phase 9) precedent. D-05
  applies the same shape: D3D9 boot-verify at wave boundaries; per-plan
  inner-loop = D3D9 build-verify only.

### Integration Points

- **Engine-side `Graphics.cpp:184-238`** — single integration site for
  D-02 range-check extension + D3D11 plugin GetApi load.

- **Engine-side `GraphicsOptionTags`** — D-02a `TAG_DX11` declaration;
  callers across the codebase (terrain fallback, skeletal, particles)
  may currently key off `TAG_DX9` and need audit.

- **D3D9 plugin source-tree** at
  `src/engine/client/application/Direct3d9/src/win32/` — DESIGN
  REFERENCE ONLY per D-01. Read for Gl_api function semantics, not
  for source-copy.

- **Asset-side shader templates** (location TBD by research — likely
  in `dsrc/` or compiled .iff blobs) — D-04 spike Phase A static
  analysis target.

- **`RenderWorld.cpp:909/913`** — SPEC R7 conditional edit site if the
  D3D11 DPVS verdict diverges from Phase 10 Option α.

</code_context>

<specifics>
## Specific Ideas

- **`gl11_d.dll` / `gl11_r.dll` / `gl11_o.dll`** are the expected output
  DLL names per D-02 + the existing `DLL_NAME_FORMAT` macro at
  `Graphics.cpp:194-200`.

- **D-04 spike Phase A grep targets** (rough first pass — researcher
  refines): `D3DTSS_COLOROP`, `D3DTSS_ALPHAOP`, `D3DTOP_MODULATE`,
  `D3DTOP_ADD`, `D3DTOP_SELECTARG1`, `D3DTOP_DISABLE` literal references
  in shader templates and material definition files.

- **D-04 spike Phase B CSV columns** (suggestion): `frame_no, scene_tag,
  material_name, stage_idx, op_type, op_value, frequency_count`.
  Aggregation: distinct material + stage + op combinations seen in each
  target scene = the FFP-generator coverage list (or empty = descope
  evidence).

- **Shader-cache file naming** (D-03 suggestion): `<source-hash>.cso`
  with a tiny `.json` or text manifest mapping shader-template name →
  current cached hash. Researcher refines.

- **MVP "clear-to-color" boot target.** Sub-deliverable suggestion for
  the second plan (after the spike): D3D11 plugin loads, creates device
  + swap-chain + back-buffer + depth-stencil, clears to a recognizable
  color (e.g. dark blue), presents. No geometry. Proves the plugin DLL
  + Gl_api table-export contract before any rendering work lands.
  Mirrors the "Hello, Triangle" pattern from D3D11 sample code.

- **DPVS remeasurement capture cadence** (suggestion for SPEC R7):
  capture in the SAME four target zones Phase 10 used (Mos Eisley
  plaza, starport, walking, cantina interior) under PIX so the
  scene-conditional Phase 10 verdict has a directly-comparable
  external-tool dataset. Phase 10's CSV columns + scenes give the
  comparison shape; PIX captures replace the in-engine timestamp
  query / QPC pair.

</specifics>

<deferred>
## Deferred Ideas

- **Space scenes (JTL nebulae, ship combat) under D3D11.** Per SPEC
  §Boundaries. Pre-existing TRE-data artifacts in v1 were unrelated to
  renderer; defer to follow-up phase. User noted during SPEC interview
  that SWGSource-vs-whitengold TRE diff is a separate research item
  (todo `2026-05-15-swgsource-vs-whitengold-tre-asset-diff.md`).

- **Other planets (Naboo, Corellia, Dantooine, Endor) under D3D11.**
  Per SPEC §Boundaries. Each exercises different terrain textures and
  ambient lighting; defer to follow-up.

- **Pixel-perfect parity with D3D9.** Per SPEC §Boundaries. Minor
  differences in lighting tone, AA pattern, LOD bias, shader precision
  are accepted. "Substantially similar" is the verification bar (R6).

- **Performance parity with D3D9.** Per SPEC §Boundaries. "Playable"
  is enough; optimization passes are follow-up.

- **Deletion of the `Direct3d9` plugin tree.** Per SPEC §Boundaries.
  D3D9 stays as fallback during the transition phase. Future cleanup
  candidate.

- **`Direct3d9_ffp.vcxproj` (gl06) + `Direct3d9_vsps.vcxproj` (gl07)
  D3D11 mirrors.** D3D11 has no FFP pipeline; SM5.0 unifies the shader
  story. The D3D9 FFP/VSPS variants stay on disk for D3D9-side fallback;
  D3D11 doesn't mirror them.

- **Compute shaders, tessellation, geometry shaders.** Per SPEC
  §Boundaries. Not used by SWG rendering paths; explicitly out of scope
  for Phase 11.

- **Cantina corner-snap engine quirk fix.** Pre-existing SOE engine
  behavior, cross-client confirmed. Engine-improvement candidate per
  todo `2026-05-15-cantina-corner-snap-engine-improvement.md` — NOT a
  Phase 11 deliverable.

### Reviewed Todos (not folded)

Two todos surfaced by `gsd-sdk query todo.match-phase 11`:

- **`2026-05-15-cantina-corner-snap-engine-improvement.md`** — Pre-existing
  SOE engine quirk in collision pushback / portal traversal. Cross-client
  confirmed (Restoration server client + cross-toolchain). Not a Phase 11
  regression target. Files (`CellProperty.cpp`, `Object.cpp`,
  `sharedCollision/`) are NOT touched by Phase 11. Engine-improvement
  candidate for a future phase that touches collision response or
  portal traversal directly.
- **`2026-05-15-swgsource-vs-whitengold-tre-asset-diff.md`** — Space-scene
  TRE-data comparison research. Phase 11 explicitly excludes space scenes
  (SPEC §Boundaries). Tracked as separate diagnostic / research item;
  NOT a Phase 11 requirement per SPEC §"Adjacent research item."

</deferred>

---

*Phase: 11-d3d11-renderer-plugin*
*Context gathered: 2026-05-15*
