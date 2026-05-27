# Feature Research — v2.2 Visual Parity (D3D11 → D3D9 baseline)

**Domain:** Legacy game-engine renderer porting — closing D3D11 visual gaps against a known-good D3D9 reference path in the SWG/whitengold client.
**Researched:** 2026-05-27
**Confidence:** HIGH

> **Framing note.** This is not greenfield product research — these "features" are
> already-shipped visual behaviors that render *correctly under D3D9* (`rasterMajor=5`,
> `gl05_d.dll`) and render *wrong under D3D11* (`rasterMajor=11`, `gl11_d.dll`). "Table
> stakes" = behaviors that must match the D3D9 baseline for the client to be called
> visually at parity. The MVP target is the **character-select beachhead** (textures +
> eyes), then the open world. Sources are the codebase itself plus the Phase 11
> closeout artifacts — especially the CODEX+Cursor Iter-44 pipeline deep-dive
> (`.planning/phases/11-d3d11-renderer-plugin/11-09.15-CODEX-CONSULT-iter44-pipeline-deepdive.md`),
> `11-SUMMARY.md`, and `docs/research/phase12-baseline/COMPARISON.md`.

---

## The single root cause behind almost every gap

The dominant blocker is **the asset pixel-shader pipeline**. Confirmed mechanism (HIGH):

1. The engine ships **pre-compiled D3D9-era pixel-shader bytecode** (PEXE / `.psh`).
   `ID3D11Device::CreatePixelShader` rejects it (wrong bytecode container/SM level).
2. D3D11 therefore falls back to a **dynamic fallback PS keyed only by the VS output
   signature** (`Direct3d11_PixelShaderProgramData.cpp` dynamic-gen path). That fallback
   evaluates `sample(t0, uv0) * COLOR0` only — it samples **slot 0 with TEXCOORD0** and
   ignores everything bound to texture stages 1..7.
3. Where even that fails to bind, a **magenta debug PS** shows through (the magenta
   slivers in `spotA_interior_0009`, `spotB_exterior_0013/0014`).
4. The engine's real material model is a **D3D9 fixed-function texture-stage cascade**
   (`ShaderImplementation::Pass::Stage`, up to 8 stages, ~26 `TextureOperation` modes:
   modulate/add/lerp/blend*/dot3/bumpenv/…). The D3D11 backend **binds SRVs to all 8
   stages** (Iter-44E) but **never evaluates the stage cascade**, so multi-texture
   materials are structurally wrong even though the textures are correctly bound.

Consequence: **eyes, head, mini-map, terrain detail-blends, specular/overlay surfaces,
and particle materials are all the same bug wearing different costumes.** They will not
resolve via more render-state tuning (the Iter-43/44C reverts proved this); they need a
**generated FFP texture-stage PS evaluator** (CODEX recommendation: read engine
`TextureOperation` data, implement the dominant ~12 ops, log+fallback the rare ones).

Two gaps are **independent** of this blocker and can be done in isolation:
**gamma/lighting tone** (a missing post-process LUT pass) and the **load-screen seam**
(a 2D fullscreen-blit half-texel sampling bug).

---

## Feature Landscape

### Table Stakes (must match the D3D9 baseline for "parity")

| Feature | What "correct" looks like (D3D9 baseline) | Complexity | Notes / mechanism |
|---------|-------------------------------------------|------------|-------------------|
| **Asset-textured surfaces** (single-diffuse world + character meshes) | Meshes show their full diffuse textures (walls, armor, terrain ground texture). D3D11 today: flat white/mauve untextured + magenta slivers. | **HIGH** | The headline blocker. Single-diffuse + vertex-color materials are the *simplest* class and may improve with the first FFP-stage PS pass. Geometry is already correct (post Iter-38B transpose + Iter-42v2 row-scale). Recognize "correct" = char-select avatar's clothing/skin shows texture, not flat white. |
| **Character EYES** (char-select headline target) | Eyes are a **distinct iris/sclera texture** on the head, correct color (driven by `pn_hum_eyes` customization palette), correctly occluded by the head — NOT showing through the back of the skull, NOT flat gray. | **HIGH** | `sul_eye.sht` is a **3-stage** material; eye texture sits on **stage 1+**, not stage 0. Iter-44E confirmed: before it, no SRV was bound on stage 1+, so the PS read *sticky gray* from the previous draw → "gray eyes." Iter-44E binds the SRVs but the slot-0-only PS still ignores them → eyes stay wrong. "Eyes through back of head" is the **same multi-stage gap** combined with the missing per-pass depth/alpha-test compositing the eye material relies on. **No evidence of animated UV or a separate eye-blink shader** — eye color is customization-palette-driven (CustomizableShader hue), and the "specialized shader" is simply the multi-stage `sul_eye.sht`. Recognize "correct" = colored irises sitting on the face, occluded by the head, matching the chosen eye color. |
| **Lighting + gamma / tone** | Interiors show proper ambient + diffuse falloff and atmospheric haze; sky shows sunset tone. D3D11 today: blown-out flat white interiors, cream-washed sky. | **MEDIUM** (gamma LUT) + **HIGH** (FFP lighting) | **Two sub-parts, decouple them.** (a) **Gamma/tone** is *independent* of the PS blocker: D3D9 used `SetGammaRamp` with `pow(0.5 + contrast*(x*brightness-0.5), 1/gamma)`; DXGI flip-model has no `SetGammaRamp`, so `setBrightnessContrastGamma_impl` is a **no-op stub** → needs a fullscreen LUT/color-correction post pass. (b) **FFP lighting/material-source** (ambient/diffuse/specular sources, `Direct3d9_LightManager` parity) is *coupled* to the PS pipeline — light/material cbuffer ranges init to zero in the default D3D11 path. The "blown-out flat white" is mostly (b) (no lighting term applied → texture+light missing reads as white); the sky wash is mostly (a). |
| **Multi-stage texture sampling** (detail / specular / overlay stages) | Terrain shows detail-texture blends; surfaces with specular/overlay stages composite correctly. D3D11 today: only the stage-0 texture (if any) shows. | **HIGH** | This *is* the core of the asset-PS work — the generated PS must evaluate `ShaderImplementation::Pass::Stage` ops (modulate/add/lerp/blend-alpha/selectArg, plus alpha-op equivalents) honoring per-stage texcoord index. CODEX: implement the dominant ~12 ops; log+magenta the rare ones (BUMPENVMAP, DOTPRODUCT3, MULTIPLYADD) until a live asset needs them. **This unblocks eyes, head, terrain detail, and specular together.** |
| **Minimap / radar shape** (round vs square) | Radar is a **round disc** clipped to a circle, with a compass overlay. D3D11 today: square/diamond. | **MEDIUM** | NOT a separate geometry bug. D3D9 renders terrain into an offscreen ARGB8888 texture (`ClientProceduralTerrainAppearance::Radar::createShader` + `renderChunksInto`), then draws it through **`shader/uicanvas_radar.sht`** whose **circular alpha-mask** clips the square texture to a disc (`CuiWidgetGroundRadar::Render`, `m_clipToCircle`). The D3D11 dynamic PS doesn't sample/apply that alpha-mask stage → no circular clip → square. **Same multi-stage / alpha-mask sampling family as eyes.** Strong candidate to resolve *for free* once the asset-PS / multi-stage pipeline lands; re-check with a screenshot diff, do not pre-claim (Iter-44B falsely claimed it). |
| **Loading-screen fullscreen blit** (centerline seam) | The fullscreen splash blits seamlessly edge-to-edge. D3D11 today: faint vertical seam dead-center (x≈512). | **LOW–MEDIUM** | **Independent of the PS blocker** — best standalone first win + sampling-path canary. It is a 2D UI image (Backdrop/Splash mediators → UI canvas textured-quad path), no 3D/skinning/lighting. Confirmed *image-independent + renderer-specific* (many D3D11 screens seam; ~20 D3D9 load-ins never did). Most likely the splash drawn as **two side-by-side halves** with a **half-texel offset** at the shared edge — the canonical D3D9 (−0.5 texel rule) → D3D10+ porting bug. May also implicate `getOneToOneUVMapping` (currently a D3D11 stub). |

### Differentiators / nice-to-have (visual fidelity beyond baseline-correct)

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| **Particles / ribbon effects** | Smoke, glows, weapon swooshes, contrails look like effects, not solid squares/stretched bands. | **HIGH** | Two distinct sub-issues. (a) **Particle billboards render as solid colored squares** — the additive/alpha-blended particle *material* isn't evaluated (PS-gen + `Pass::apply` textureFactor/scroll constants gap) and point-sprite sizing is a deliberate no-op. (b) **Ribbon/swoosh stretching** — CODEX ruled this is **NOT a transform bug** (RibbonAppearance/TrailAppearance *do* call `setObjectToWorldTransformAndScale`; identity-W is intentional for ParticleEmitter/Swoosh whose verts are already world-space). More likely a dynamic-VB count/stride or topology (strip/fan) mismatch, or the PS mishandling the strip's texture/alpha making it *look* like a stretched band. Needs draw-path instrumentation (log appearance/shader/topology/VB-format/transform-class) before fixing. Defer until after the char-select beachhead and the FFP-stage PS lands — particles depend on both PS-gen and per-pass `Pass::apply` constants. |
| **Stencil-dependent effects** (decals, outlines, some interior portals) | Decals/outlines composite correctly. | **MEDIUM** | D3D9 pushes full stencil state per pass; D3D11 `D3D11_DEPTH_STENCIL_DESC` defaults stencil **disabled, ref 0**. Low visibility on char-select; surfaces in-world. |
| **Bloom / HDR-ish glow** | Bright sources bloom as on D3D9. | **MEDIUM** | `setBloomEnabled` is a D3D11 stub. Part of output/lighting parity; trails the baseline. |
| **Alpha-test compare-function fidelity** | Foliage/cutout edges match exactly. | **LOW** | Iter-44B ported alpha-test as `clip(a - ref)` assuming `>= ref`; the D3D9 `m_alphaTestFunction` enum (less/equal/never/not-equal…) is ignored. Fold into the PS-gen key. |

### Anti-Features (do NOT chase in this milestone)

| Feature | Why it seems worth doing | Why problematic | Do instead |
|---------|--------------------------|-----------------|------------|
| **More render-state-only iterations** to "fix" eyes/head/mini-map | Each prior state tweak closed *something* | The Iter-43 / Iter-44C reverts proved state-only iteration **amplifies** the PS-gen gap and creates false wins + regressions. Both CODEX and Cursor converged on this. | Build the generated FFP texture-stage PS evaluator. Treat state tuning as done. |
| **"Modulate-everything" naive multi-stage PS** | Quick, would improve some eye/head cases | Actively *wrong* for additive glow, alpha-lerp masks, specular, reflection, dot3 — and it *hides* which semantic was needed, worsening future diagnosis. | Read engine `TextureOperation` per stage; implement the dominant ~12 ops; log+fallback the rest (CODEX option (e)). |
| **Per-asset hand-authored PS bytecode injection** (first move) | Would give true parity for authored D3D9 PS passes | Per-asset churn before the renderer can even express the engine's common material algebra; doesn't solve FFP-stage assets generically. | Do the generic FFP-stage generator first; treat authored-D3D9-PS translation/reauthoring as a **separate later track**. |
| **`IDXGIOutput6` for gamma** | "Native" display-side gamma | Output/HDR-metadata oriented, not a portable per-swap-chain replacement for the D3D9 gamma ramp; won't reproduce the formula. | Fullscreen post-process color-correction/LUT pass using the D3D9 `pow()` formula. |
| **Chasing the exterior "shard/stretched geometry"** as a parity blocker | Looks dramatic in `spotC_0014` | The skeletal twin was already fixed (`905fb5d64`); the remaining exterior shards in 0013/0014 were **captured mid-load** — some smear is LOD streaming, not a settled bug. | Re-capture fully-settled exterior frames first to separate transient LOD smear from a real bug before committing scope. |
| **Fixing dither / hardware point-sprite size exactly** | Completeness | No clean D3D11 equivalent; negligible visual impact. | Document as accepted known limitations. |
| **DPVS occlusion re-measure now** | It's a v2.0 deferral (SPEC R7) | Occlusion math is meaningless while geometry mis-shades; would corrupt the measurement. | Defer until after the asset-PS pipeline lands and geometry renders cleanly. |

---

## Feature Dependencies

```
Asset PS pipeline (PEXE→SM5 translate OR generated FFP texture-stage PS)
    ├──unblocks──> Asset-textured surfaces (single-diffuse) [partial: simplest class first]
    ├──unblocks──> Multi-stage texture sampling (detail/specular/overlay)
    │                   ├──unblocks──> Character EYES (sul_eye.sht, 3-stage)
    │                   ├──unblocks──> Character HEAD shape (sul_m_head.sht, 3-stage)
    │                   └──unblocks──> Minimap round clip (uicanvas_radar.sht alpha-mask)
    └──+ Pass::apply per-pass constants (material/textureFactor/scroll/stencil-ref/fog)
            └──unblocks──> Particles / ribbon materials

FFP lighting / material-source parity (light+material cbuffer)
    └──fixes──> "blown-out flat white" interior lighting   [coupled to PS pipeline]

Gamma/tone LUT post-process pass   ── INDEPENDENT ──> sky/terrain/UI tone (cream wash)

Load-screen seam (2D blit half-texel + getOneToOneUVMapping)  ── INDEPENDENT ──> seam

Alpha-test compare function  ──folds-into──> Asset PS pipeline (PS key)
Stencil state in DSS         ── INDEPENDENT-ish ──> decals/outlines/portals
```

### Dependency Notes

- **Eyes / head / minimap all require the multi-stage PS.** They are the *same* root
  cause (slot-0-only PS) — schedule them to be **verified together** the moment the
  FFP-stage PS evaluator lands. Do not plan them as separate fixes.
- **Single-diffuse surfaces may resolve before multi-stage.** The simplest material
  class ("multiply a couple of ordinary textures with vertex color") is the first thing
  the FFP-stage PS will get right — so char-select clothing/skin texture is a plausible
  *early* win even before eyes.
- **Gamma and the load-screen seam are independent** of the big blocker → schedule them
  as parallel, low-coupling wins (good morale/canary milestones).
- **"Blown-out white" lighting is coupled** to the PS/lighting cbuffer, but the **sky
  cream-wash is the gamma LUT** — split the lighting requirement into the gamma half
  (independent) and the FFP-lighting half (coupled).
- **Particles depend on TWO things** (PS-gen *and* `Pass::apply` per-pass constants) →
  necessarily later than the char-select beachhead.

---

## MVP Definition

### Launch With (the character-select beachhead — first vertical slice)

- [ ] **Single-diffuse textured surfaces on the char-select avatar** — proves the FFP-stage PS binds real asset textures (essential; the whole pipeline's smoke test).
- [ ] **Character EYES render correctly on char-select** — colored irises, occluded by head, not gray, not through-the-skull (the headline target; validates multi-stage sampling).
- [ ] **Character HEAD shape correct on char-select** (`sul_m_head.sht`) — same multi-stage gap, same fix, free to verify alongside eyes.

### Add After Validation (extend the working PS pipeline outward)

- [ ] **World/terrain textured surfaces + detail-blend multi-stage** — same pipeline, applied to the open world.
- [ ] **Minimap round clip** — re-check after multi-stage lands (screenshot diff; do not pre-claim).
- [ ] **Gamma/tone LUT post pass** — independent; can land in parallel any time (fixes sky wash).
- [ ] **Load-screen seam fix** — independent; small isolated half-texel win + sampling canary (good early standalone).
- [ ] **FFP lighting / material-source parity** — fixes blown-out interiors once the PS lighting path is wired.

### Future Consideration (after baseline parity)

- [ ] **Particles / ribbon** — needs PS-gen + `Pass::apply` constants; instrument the live draw path first.
- [ ] **Stencil-dependent effects** (decals/outlines/portals).
- [ ] **Bloom enable + alpha-fade/bloom packing**.
- [ ] **Exterior geometry-distortion investigation** — only after a settled re-capture confirms it's real, not LOD smear.
- [ ] **DPVS D3D11 remeasure** (SPEC R7) — only after geometry renders cleanly.
- [ ] **Authored-D3D9-PS translation track** — separate, for passes that use real PS bytecode rather than FFP stages.

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| Asset PS pipeline (generated FFP-stage PS) | HIGH | HIGH | **P1** (gates almost everything) |
| Character eyes (char-select) | HIGH | (rides PS pipeline) | **P1** (headline) |
| Single-diffuse textured surfaces | HIGH | (rides PS pipeline) | **P1** |
| Multi-stage sampling (detail/specular/overlay) | HIGH | HIGH | **P1** |
| Gamma/tone LUT post pass | MEDIUM | MEDIUM | **P2** (independent) |
| Load-screen seam | LOW–MEDIUM | LOW–MEDIUM | **P2** (independent canary) |
| Minimap round | MEDIUM | (likely rides PS pipeline) | **P2** (re-check, don't pre-claim) |
| FFP lighting / material-source | HIGH | HIGH | **P2** |
| Particles / ribbon | MEDIUM | HIGH | **P3** (needs PS-gen + Pass::apply) |
| Stencil / bloom / alpha-test fidelity | LOW–MEDIUM | MEDIUM | **P3** |
| Exterior geometry distortion | MEDIUM | (investigate) | **P3** (confirm real first) |
| DPVS remeasure | LOW (internal) | MEDIUM | **P3** (after clean geometry) |

**Priority key:** P1 = char-select beachhead / core blocker; P2 = extend outward + independent wins; P3 = post-baseline polish.

## Per-gap "what correct looks like" + recognition cheatsheet

| Gap | Recognize CORRECT by (vs D3D9) | Coupling |
|-----|-------------------------------|----------|
| Textured surfaces | Avatar clothing/skin shows diffuse texture, not flat white; no magenta slivers | Core PS blocker |
| Eyes | Colored irises on the face, occluded by head, match selected eye color; not gray, not through-skull | Multi-stage PS (rides core) |
| Head | Full head shape (e.g. Ithorian hammerhead), not simplified/clipped | Multi-stage PS (rides core) |
| Lighting | Interior ambient+diffuse falloff, not blown-out white | FFP lighting (coupled to PS) |
| Gamma/tone | Sunset sky tone, not cream wash | **Independent** LUT pass |
| Multi-stage | Terrain detail blend visible; specular/overlay composite | Core PS blocker |
| Minimap | Round disc + compass, not square/diamond | Multi-stage alpha-mask (likely rides core) |
| Load-screen | Seamless fullscreen splash, no centerline line | **Independent** 2D blit half-texel |
| Particles | Soft smoke/glow, not solid colored squares | PS-gen + Pass::apply (later) |
| Ribbon/swoosh | Tapered trails, not stretched solid bands | Draw-path instrumentation first (NOT transform) |

## Sources

- `docs/research/phase12-baseline/COMPARISON.md` — authoritative 5-bucket gap catalogue with matched D3D9/D3D11 image evidence (HIGH).
- `.planning/phases/11-d3d11-renderer-plugin/11-SUMMARY.md` — Phase 11 closeout, Phase 12 successor scope, eyes/head/minimap/particle/ribbon classification (HIGH).
- `.planning/phases/11-d3d11-renderer-plugin/11-09.15-CODEX-CONSULT-iter44-pipeline-deepdive.md` (+ `-RESPONSES`) — full D3D9→D3D11 pipeline gap inventory + PS-gen recommendation; the single richest source (HIGH).
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.h` — confirms slot-0-only PS, 8-stage SRV binding, "gray eyes" diagnosis (HIGH, code).
- `src/engine/client/library/clientUserInterface/src/shared/widget/CuiWidgetGroundRadar.cpp` + `clientTerrain/.../ClientProceduralTerrainAppearance_Radar.cpp` — radar offscreen-texture + `uicanvas_radar.sht` circular-alpha-mask clip mechanism (HIGH, code).
- `src/engine/shared/library/sharedGame/src/shared/core/CustomizationManager.cpp` + `clientGraphics/.../CustomizableShader.cpp` — eye color is `pn_hum_eyes` palette-driven customization (CustomizableShader hue), not an animated shader (HIGH, code).
- `src/engine/client/library/clientGame/src/shared/core/Game.cpp` (Backdrop/Splash mediators) + `CuiLoadingManager.cpp` — load screen is a fullscreen UI image on the 2D canvas blit path (HIGH, code).
- Project memories: `project_open_runtime_issues_2026_05_25` (eyes flagged v2.2), `project_phase11_minimap_never_round`, `project_phase11_close_pass_with_deferrals`, `project_phase12_visual_baseline` (HIGH).

---
*Feature research for: D3D11 visual parity (char-select beachhead → open world)*
*Researched: 2026-05-27*
