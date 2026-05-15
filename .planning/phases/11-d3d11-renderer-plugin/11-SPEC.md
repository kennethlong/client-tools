# Phase 11: D3D11 Renderer Plugin — Specification

**Created:** 2026-05-15
**Ambiguity score:** 0.11 (gate: ≤ 0.20)
**Requirements:** 7 locked

## Goal

Ship a new `Direct3d11.dll` plugin that satisfies the existing 119-function
`Gl_api` table, runs alongside the existing `Direct3d9.dll` plugin
(selectable at startup via `client.cfg`), and renders Tatooine outdoor +
Mos Eisley cantina interior with full subsystem coverage (terrain,
character/creature, particles, UI HUD overlays) under D3D 11.0 feature
level (HLSL SM5.0, Win7+ baseline). D3D11 becomes the default once it
reaches the stability and visual criteria below; D3D9 stays as a
maintained fallback for the transition phase.

## Background

The current renderer is `Direct3d9` at
`src/engine/client/application/Direct3d9/` and ships as **three DLLs**
today — `Direct3d9.dll`, `Direct3d9_ffp.dll`, `Direct3d9_vsps.dll` —
fixed-function-pipeline, vertex/pixel-shader, and main variants. The
plugin satisfies a 119-function `Gl_api` function-pointer table loaded
by `clientGraphics`. Phase 11 builds a parallel `Direct3d11` plugin
satisfying the same surface; the engine side (clientGraphics) does
not change.

The D3D9 plugin uses `IDirect3DDevice9`, `D3DPOOL_MANAGED`, and
lost-device recovery — none of which translate directly to D3D11. The
codebase has VS/PS 1.1–2.0 HLSL shaders that need recompiling to
SM5.0 (`vs_5_0` / `ps_5_0`, `SV_POSITION`, modern constant buffers).
SWG's material system uses `D3DTSS_*` texture combiner operations
extensively; D3D11 has no fixed-function pipeline, so a runtime pixel
shader generator may be required (D3D11-04). Whether the target scenes
actually exercise FFP paths is open and resolved by an early Phase 11
spike.

Phase 10 verdict was `remove globally` for DPVS occlusion culling
(applied as Option α at `RenderWorld.cpp:909/913`), based on
scene-conditional data favoring outdoor over indoor. Phase 11 must
re-measure DPVS cost under D3D11 and revisit the keep/remove/scene-aware
decision (CONTEXT D-12). Phase 10 removed all source-level
instrumentation in plan 10-07; Phase 11 uses external tools (PIX /
Nsight / GPUView) to capture timing rather than re-adding source
instrumentation.

## Requirements

1. **D3D11 plugin scaffold + Gl_api parity (D3D11-01)**: New
   `Direct3d11.dll` exporting the same 119-function `Gl_api` table as
   `Direct3d9.dll`.
   - Current: No `Direct3d11/` source tree exists. Only `Direct3d9/`
     (three vcxproj variants) ships today.
   - Target: `src/engine/client/application/Direct3d11/` produces
     `Direct3d11.dll` staged to `build/bin/Debug/` (or equivalent) via
     POST_BUILD. The 119-function `Gl_api` surface is satisfied.
   - Acceptance: `dumpbin /exports Direct3d11.dll` lists all 119
     `Gl_api` functions; `Direct3d9.dll` continues to build and load
     unchanged; client launches with `renderer=Direct3d11` (or
     equivalent existing config key) and reaches char-select without
     crash.

2. **Resource management migration (D3D11-02)**: Replace D3D9 resource
   model with D3D11 explicit resource lifetime.
   - Current: D3D9 plugin uses `IDirect3DDevice9`, `D3DPOOL_MANAGED`,
     and lost-device recovery callbacks throughout
     `Direct3d9_*Data.cpp` files.
   - Target: D3D11 plugin uses `ID3D11Device` + `ID3D11DeviceContext`;
     explicit resource creation and lifetime; no `D3DPOOL_MANAGED`; no
     lost-device recovery code (D3D11 has no equivalent — explicitly
     out of scope).
   - Acceptance: Grep across `Direct3d11/` source returns zero matches
     for `D3DPOOL_MANAGED`, `OnLostDevice`, `OnResetDevice`, or
     equivalent recovery primitives. Plugin survives Alt-Tab, window
     resize, and mode switch without crash during a 5-minute play
     session.

3. **Shader recompilation to HLSL SM5.0 (D3D11-03)**: All SWG vertex
   and pixel shaders compile under D3D 11.0 feature level (HLSL SM5.0,
   `vs_5_0` / `ps_5_0`).
   - Current: SWG's HLSL shaders are written against VS/PS 1.1–2.0
     model with D3D9 register-based semantics.
   - Target: All shaders accepted by FXC/D3DCompile at
     `vs_5_0` / `ps_5_0`. `SV_POSITION` semantics in place. Constant
     buffer model updated.
   - Acceptance: A pre-flight shader-compile pass (CI-style or local
     batch) succeeds across the full shader catalog with zero errors.
     Runtime D3D11 device-creation negotiates feature level 11.0
     minimum or fails loudly.

4. **FFP pixel shader generator — conditional on spike (D3D11-04)**: If
   target scenes exercise D3D9 fixed-function texture-combiner paths,
   D3D11 plugin generates equivalent pixel shaders at runtime.
   - Current: `Direct3d9_ffp.dll` uses `SetTextureStageState` for FFP
     combiner ops (`D3DTSS_COLOROP`, `D3DTSS_ALPHAOP`, `MODULATE`,
     `ADD`, `SELECTARG1`, etc.). D3D11 has no fixed-function pipeline.
   - Target: First Phase 11 plan is a SPIKE that determines whether
     target scenes use FFP paths.
     - If YES: Implement a runtime HLSL pixel shader generator that
       emits a `ps_5_0` shader for each FFP texture-combiner state
       combination encountered in target scenes. Cover the common ops
       (MODULATE, ADD, SELECTARG1) at minimum.
     - If NO: Descope FFP emulation entirely. Document the spike result
       and remove `Direct3d9_ffp.dll` from the plan's responsibility
       set (the existing D3D9 FFP variant remains in the tree for
       D3D9-side fallback; D3D11 doesn't need to mirror it).
   - Acceptance: Spike result committed as a written finding in a
     Phase 11 plan artifact. If FFP generator is in scope, at least 3
     common D3DTSS_* combinations render visually correct output
     verified against D3D9 baseline screenshots.

5. **Target-scene rendering (D3D11-05 expanded per scope decisions)**:
   Tatooine outdoor + Mos Eisley cantina interior render under D3D11
   with all four target subsystems functional.
   - Current: Both scenes render under D3D9. D3D11 plugin does not
     exist.
   - Target: D3D11 plugin renders both scenes with all four target
     subsystems present: terrain (clientTerrain procedural), character
     and creature skeletal rendering (clientSkeletalAnimation), particle
     systems (clientParticle), and UI HUD overlays (chat, hotbar,
     minimap). The plugin matches D3D9 stability — no crash during
     normal play beyond known D3D9 long-tails (e.g. the ~11-min
     ExceptionHandler issue noted in deferred items).
   - Acceptance: User plays Tatooine outdoor for ≥5 min and Mos Eisley
     cantina interior for ≥5 min under D3D11 without renderer-specific
     crash. All four subsystems verified visible by direct observation.
     No all-black or all-white frames. No persistent z-fighting on
     cantina portals.

6. **Visual parity by side-by-side screenshot (D3D11-05 verification
   method)**: D3D11 output "looks substantially similar" to D3D9 output
   at matched camera positions in both target scenes.
   - Current: No D3D11 output exists to compare.
   - Target: A pair of D3D9 reference screenshots (one Tatooine plaza,
     one cantina interior) at known camera positions, plus matching
     D3D11 captures at the same positions. Human visual judgment of
     "substantially similar" — same characters, terrain, lighting in
     roughly the same positions; no missing geometry; no glaring
     corruption. Minor differences in lighting tone, anti-aliasing
     pattern, mipmap LOD bias, and shader precision are accepted.
   - Acceptance: Four reference screenshots committed under
     `docs/recon/11-d3d11-screenshots/` (D3D9 outdoor, D3D11 outdoor,
     D3D9 cantina, D3D11 cantina). Visual comparison documented in
     `docs/recon/11-d3d11-screenshots/comparison-notes.md` with
     PASS verdict from user inspection.

7. **DPVS remeasurement under D3D11 + decision (success criterion #6
   from ROADMAP, per CONTEXT D-12)**: Re-measure
   `resolveVisibility()` cost under D3D11 and revisit the
   Option α global-remove decision from Phase 10.
   - Current: Phase 10 verdict `remove globally` applied at
     `RenderWorld.cpp:909/913` based on D3D9 measurements
     (3/3 outdoor scenes voted remove; 1/1 indoor voted keep; Option α
     chose remove to optimize outdoor magnitude × frequency over
     indoor regression × frequency). Phase 10 instrumentation removed
     in plan 10-07.
   - Target: Phase 11 captures GPU/CPU timing of `resolveVisibility()`
     under D3D11 using **external tools** (PIX, Nsight, GPUView) — no
     source-level instrumentation restore. Three options evaluated:
     (a) keep Option α — if D3D11 numbers still favor outdoor;
     (b) revert globally to `keep` — if D3D11 makes outdoor net-positive
         again, restore `OCCLUSION_CULLING` bit + config-key plumbing at
         `RenderWorld.cpp:909/913`;
     (c) implement runtime scene-aware split — pass different
         `cullingParameters` per cell context at the camera-setup site
         (non-trivial refactor; justify only if D3D11 shows large
         bidirectional gap).
   - Acceptance: `docs/recon/11-dpvs-d3d11-remeasure.md` written with
     captured external-tool timing data, verdict, and decision. If
     verdict diverges from Option α, source edits applied at
     `RenderWorld.cpp:909/913` and committed atomically.

## Boundaries

**In scope:**
- New `Direct3d11` plugin source tree producing `Direct3d11.dll`
- D3D 11.0 feature level baseline (HLSL SM5.0; effectively Win7+
  hardware minimum)
- Tatooine outdoor (Mos Eisley plaza or equivalent) rendering
- Mos Eisley cantina interior (indoor cell + portal traversal) rendering
- Four target subsystems: terrain, character/creature skeletal,
  particles, UI HUD overlays
- VSPS programmable shader recompilation to SM5.0
- FFP emulation via runtime pixel shader generator — conditional on
  spike outcome
- DPVS remeasurement via external tools and architectural decision
- Runtime renderer selection via existing `client.cfg` config key
- D3D9 plugin maintained as fallback (kept building, kept stable)
- Reference screenshots and visual comparison notes

**Out of scope:**
- Space scenes (JTL nebulae, ship combat) — different rendering
  pipeline; v1 had pre-existing TRE-data artifacts unrelated to
  renderer; defer to follow-up phase
- Other planets (Naboo, Corellia, Dantooine, Endor) — each exercises
  different terrain textures and ambient lighting; defer to follow-up
- Pixel-perfect parity with D3D9 — minor visual differences accepted
  (lighting, AA, LOD bias)
- Performance parity with D3D9 — "playable" is enough; optimization
  passes deferred to follow-up
- Deletion of `Direct3d9` plugin tree — kept as fallback for the
  transition phase
- Restored source-level DPVS instrumentation — external tools (PIX /
  Nsight / GPUView) used instead
- Lost-device recovery code — D3D11 has no equivalent; explicitly
  excluded
- Bink video playback — already removed in Phase 7
- Cinematics — out of scope (already config-disabled)
- Compute shaders, tessellation, geometry shaders — not used by SWG
  rendering paths

**Adjacent — not in this phase:**
- TRE-asset diff between SWGSource and whitengold codebase for space
  scenes (user's parallel research interest from 2026-05-15 spec
  interview) — tracked as a separate todo, not a Phase 11 requirement

## Constraints

- D3D 11.0 feature level minimum (HLSL SM5.0)
- Win7+ baseline (effectively Win10/11 in practice; solo dev cadence)
- 119-function `Gl_api` table contract — engine-side surface is unchanged
- D3D9 plugin must continue to build and load — Phase 11 cannot break
  the existing renderer (cross-cutting test gate per CLEAN-05 / STL-05
  precedent)
- Stability floor: D3D11 plugin must match D3D9 stability — no
  renderer-specific crash beyond known D3D9 long-tails (e.g. ~11-min
  ExceptionHandler issue in deferred items)
- No mandatory new dependencies beyond DirectX 11 SDK headers (already
  present in `src/external/3rd/library/directx9/include/` — D3D11 SDK
  headers will need adding to that or a parallel directory)
- Build system: MSBuild on `swg.sln` (NOT CMake — Koogie tree did not
  port to CMake per Phase 9 CONTEXT D-09)

## Acceptance Criteria

- [ ] `src/engine/client/application/Direct3d11/` source tree exists
      with a `Direct3d11.vcxproj` (or equivalent) producing
      `Direct3d11.dll`
- [ ] `dumpbin /exports Direct3d11.dll` lists all 119 `Gl_api` functions
- [ ] `Direct3d9.dll` still builds and loads after Phase 11 changes
      (no regression in fallback renderer)
- [ ] Setting `renderer=Direct3d11` in `client.cfg` (or the existing
      equivalent key) loads `Direct3d11.dll`; client reaches char-select
      without crash
- [ ] Tatooine outdoor renders under D3D11: terrain visible, player
      character visible, NPCs visible, HUD visible, particles where
      they exist in D3D9, no all-black or all-white frames
- [ ] Mos Eisley cantina interior renders under D3D11: cell visible,
      NPCs inside visible, HUD overlay visible, no persistent
      z-fighting on portals
- [ ] User plays Tatooine outdoor ≥5 min under D3D11 without
      renderer-specific crash beyond known D3D9 long-tails
- [ ] User plays Mos Eisley cantina interior ≥5 min under D3D11
      without renderer-specific crash
- [ ] Side-by-side screenshot pair (D3D9 vs D3D11) committed for
      Tatooine plaza at a known camera position
- [ ] Side-by-side screenshot pair (D3D9 vs D3D11) committed for
      cantina interior at a known camera position
- [ ] `docs/recon/11-d3d11-screenshots/comparison-notes.md` written
      with PASS verdict for visual similarity
- [ ] FFP spike result documented in a Phase 11 plan artifact (either
      FFP generator implemented OR FFP emulation descoped with
      evidence)
- [ ] `docs/recon/11-dpvs-d3d11-remeasure.md` written with external-tool
      timing data, verdict, and architectural decision (keep Option α
      / revert global / scene-aware split)
- [ ] DPVS source edits applied at `RenderWorld.cpp:909/913` IF
      verdict diverges from current Option α

## Ambiguity Report

| Dimension          | Score | Min  | Status | Notes                              |
|--------------------|-------|------|--------|------------------------------------|
| Goal Clarity       | 0.90  | 0.75 | ✓      | Outdoor + cantina interior locked  |
| Boundary Clarity   | 0.95  | 0.70 | ✓      | Explicit out-of-scope list         |
| Constraint Clarity | 0.85  | 0.65 | ✓      | D3D 11.0 SM5.0, Win7+, no perf parity |
| Acceptance Criteria| 0.85  | 0.70 | ✓      | 13 falsifiable pass/fail criteria  |
| **Ambiguity**      | 0.11  | ≤0.20| ✓      |                                    |

Status: ✓ = met minimum, ⚠ = below minimum (planner treats as
assumption)

## Interview Log

| Round | Perspective         | Question summary                              | Decision locked                                                  |
|-------|---------------------|-----------------------------------------------|------------------------------------------------------------------|
| 1     | Researcher          | D3D9 coexistence horizon                      | Transition phase: D3D11 becomes default once stable; D3D9 fallback maintained |
| 1     | Researcher          | Minimum scene render gate                     | Tatooine outdoor + Mos Eisley cantina interior                   |
| 2     | Researcher+Simplifier | FFP/VSPS/main variants — which paths?       | Defer the variants decision; first Phase 11 plan is a SPIKE to determine if target scenes use FFP |
| 2     | Researcher+Simplifier | Which adjacent subsystems are required?     | All four: UI HUD, character/creature skeletal, particles, terrain |
| 3     | Boundary Keeper     | Out-of-scope perimeter                        | Space, other planets, pixel-perfect parity, performance parity all excluded |
| 3     | Boundary Keeper     | Minimum D3D feature level                     | D3D 11.0 (HLSL SM5.0, Win7+)                                     |
| 4     | Failure Analyst     | Stability floor                               | Match D3D9 stability — no crash during normal play (≥5 min Tatooine + ≥5 min cantina) |
| 4     | Failure Analyst     | DPVS remeasurement approach                   | External tools (PIX / Nsight / GPUView) — no source instrumentation restore |
| 4     | Failure Analyst     | "Renders correctly" falsifiable criterion     | Side-by-side screenshot comparison — "looks substantially similar"; committed to `docs/recon/11-d3d11-screenshots/` |

## Adjacent research item (NOT a Phase 11 requirement)

User noted during Round 3 interview: "space worked barely — most of the
graphics artifacts didn't load properly — due to differences in tre
files I think. We should add a comparison between SWGSource and our
code and tre files maybe."

This is a separate diagnostic / research item to be tracked as its own
todo (`.planning/todos/pending/2026-05-15-swgsource-vs-whitengold-tre-asset-diff.md`).
It is explicitly NOT part of Phase 11 scope — Phase 11 excludes space
scenes from the rendering targets.

---

*Phase: 11-d3d11-renderer-plugin*
*Spec created: 2026-05-15*
*Next step: /gsd-discuss-phase 11 — implementation decisions (plugin scaffold approach, shader compile pipeline, FFP spike design, renderer selection mechanism, etc.)*
