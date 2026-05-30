# Project Research Summary

**Project:** v2.2 Visual Parity — D3D9 to D3D11 Renderer/Shader-Pipeline Parity
**Domain:** Legacy game-engine renderer porting (D3D9 to D3D11 asset pixel-shader pipeline, SWG/whitengold client)
**Researched:** 2026-05-27
**Confidence:** HIGH

## Executive Summary

This milestone closes the remaining visual gap between the D3D11 (gl11_d.dll) and D3D9 (gl05_d.dll) renderer paths in the whitengold SWG client. The Phase 11 work achieved geometry, matrix, blend, depth, and alpha-test parity; what remains is almost entirely one root cause: the engine asset pixel-shader pipeline never landed. The engine ships .psh assets containing pre-compiled D3D9-era bytecode (TAG_PEXE) that ID3D11Device::CreatePixelShader rejects outright. D3D11 therefore falls back to a dynamic PS that samples only texture slot 0 / TEXCOORD0, uploads no per-pass material or lighting constants, and ignores the engine 8-stage TextureOperation cascade. Every named visual symptom — untextured surfaces, gray eyes, eyes through skull, square minimap, missing specular/glow, flat-white interiors, magenta slivers — is a different face of this single gap.

The critical reframe for implementation is that every .psh asset already carries the original shader source text (TAG_PSRC), which the engine loads and then discards at ShaderImplementation.cpp:2904. The correct fix is NOT decompiling bytecode, NOT a runtime D3D9-asm interpreter, and NOT per-asset hand-authored replacements. It is: load the discarded PSRC chunk and recompile it via the existing Phase-11 VS-compile toolchain (D3DCompile + Direct3d11_HlslRewrite + .cso ShaderCache + D3DReflect). PSRC is either HLSL (recompile to ps_4_0 via lane b) or D3D9 assembly (generate equivalent HLSL from the pass TextureOperation stage model via lane d). Both lanes emit DXBC through the same compilePixelShaderFromHlsl machinery that already exists in Direct3d11_PixelShaderProgramData.cpp. The HLSL:ASM ratio across the live .psh population is the single open measurement that governs how much of the milestone is lane b vs lane d — a D3DReflect census tool is the mandatory first task.

Two gaps are architecturally independent of the PS-pipeline blocker and can land in any order: the load-screen half-texel seam (D3D9 vertex data bakes in -0.5 via the live constant CuiManager::ms_pixelOffset = -0.5f; the D3D11 fix is a central correction to the c9 viewportData XYZRHW->NDC convention in Direct3d11_StateCache.cpp:1593-1602, sign/magnitude consult-gated per Phase 18 D-01 — NOT the dead getOneToOneUVMapping scaffold_fatal_stub at Direct3d11.cpp:1056, which has zero callers and must not be implemented) and the gamma LUT post-pass (the setBrightnessContrastGamma_impl no-op at Direct3d11.cpp:253; replicate D3D9 pow() ramp as a fullscreen pass — do NOT flip to sRGB views, which double-corrects). The seam fix is a safe early canary; gamma must come after PS-pipeline so the corrected image is shaded correctly before the LUT is calibrated.

---

## Key Findings

### Recommended Stack

The stack is entirely in-tree — this milestone wires and extends existing infrastructure, introducing no new dependencies. D3DCompile (d3dcompiler_47, targeting ps_4_0) is already the engine VS compile path and becomes the PS compile path via the same route. Direct3d11_HlslRewrite (Rules A-E) already clears D3D9-era HLSL idioms for SM4 and applies identically to PS source. Direct3d11_ShaderCache (FNV-1a hash + .cso disk cache) already avoids per-boot recompilation and just needs its cache key extended to cover the pass-material signature. D3DReflect is already used for VS output-signature matching and extends to PS input/cbuffer layout. Compile flags are settled: D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY + PACK_MATRIX_ROW_MAJOR, no ENABLE_STRICTNESS — mirror the VS choice, since strictness and backwards-compat are mutually exclusive. The profile is ps_4_0 — not ps_5_0 (reserved keywords), not *_level_9_x (re-imposes the SM2 instruction cap that already broke a VS). DXC/SM6/DXIL are explicitly out of scope.

The gamma fix uses a fullscreen post-process LUT pass implementing pow(0.5 + contrast*((f*brightness)-0.5), 1/gamma) — the same formula D3D9 used for SetGammaRamp. An sRGB RTV is technically permitted on a DXGI_FORMAT_B8G8R8A8_UNORM back-buffer but does NOT match the D3D9 parity baseline (D3D9 applied a ramp curve, not sRGB transfer), so sRGB-as-primary-gamma is rejected. All texture SRVs stay _UNORM (mirroring D3D9 D3DSAMP_SRGBTEXTURE=0). The persistent baked RT stays B8G8R8A8_UNORM per d3d11_baked_rt_bgra_format. Any cbuffer matrix requires XMMatrixTranspose per d3d11_cbuffer_transpose_quirk.

**Core technologies (all already in-tree):**
- D3DCompile / ps_4_0: Compile asset PSRC to DXBC — mirrors the working VS path; zero new infrastructure.
- Direct3d11_HlslRewrite Rules A-E: Clear D3D9 HLSL idioms (cbuffer-wrap, #pragma def strip) — same rewriter applies to PS source.
- Direct3d11_ShaderCache + FNV-1a: Cache compiled PS by (VS-sig + material-sig) hash — avoids per-boot recompile; bump D3D11_REWRITE_VERSION on any generator change.
- D3DReflect / ID3D11ShaderReflection: Read VS output signature for PS input struct matching — already used; extend to PS cbuffer layout.
- ID3D11InfoQueue to stage/d3d11-debug.log: Route diagnostics (PS compile failures, census output, id=342/343 linkage errors) — the established safe channel.
- RenderDoc: Frame capture for per-draw SRV/cbuffer/PS state inspection; A/B D3D9 vs D3D11 per gap.

### Expected Features

The features are visual behaviors that already exist under D3D9 and must be replicated under D3D11. They fall into three dependency tiers.

**Must have — character-select beachhead (P1, gates everything):**
- Asset-textured surfaces on char-select avatar — proves the FFP-stage PS generator binds real diffuse textures; smoke test for the whole pipeline.
- Character eyes correct on char-select (sul_eye.sht, 3-stage) — colored irises, correct eye-color from customization palette, occluded by head, not gray / not through-skull; the headline milestone target.
- Character head shape correct (sul_m_head.sht) — same 3-stage multi-stage gap, validates together with eyes.
- Multi-stage texture sampling (detail/specular/overlay cascade) — the core PS generator output; unblocks eyes, head, terrain, minimap together.
- Pass::apply per-pass constant uploads — material (VSCR_material, 5 regs), textureFactor x2, textureScroll, alpha-test ref; mirrors Direct3d9_StaticShaderData::Pass::apply at :820-966.

**Must have — extend outward (P2, depends on P1):**
- World/terrain textured surfaces + detail-blend multi-stage — same generator, broader TextureOperation coverage.
- Minimap round disc — circular alpha-mask in uicanvas_radar.sht; gates on multi-stage PS; MUST be screenshot-diff verified (Iter-44B falsely pre-claimed).
- Gamma/tone LUT post-pass — independent of PS pipeline; fixes cream-washed sky; schedule after PS so a correctly-shaded image is calibrated.
- Load-screen half-texel seam — independent 2D canary; central c9 viewportData fix in Direct3d11_StateCache.cpp:1593-1602 (sign consult-gated, Phase 18 D-01); NOT the dead getOneToOneUVMapping stub at Direct3d11.cpp:1056; best early win.
- FFP lighting / material-source parity — fixes blown-out flat-white interiors once the PS lighting cbuffer constants are wired.

**Should have — post-baseline (P3):**
- Particles / ribbon — needs PS-gen AND Pass::apply constants; instrument draw path before fixing; ribbon stretch is NOT a transform bug.
- Stencil-dependent effects (decals, outlines, portals).
- Skeletal-shard resolution (single-stream-skinning: choose multi-stream caps flip or single-stream math fix).
- DPVS D3D11 remeasure (SPEC R7, deferred from v2.0) — strictly last.

**Defer / anti-scope:**
- More render-state-only iterations (proven to amplify the PS-gen gap — the Iter-43/44C reverts).
- Modulate-everything naive multi-stage PS (actively wrong for additive glow, alpha-lerp, specular).
- Re-enabling per-pass blend factors before multi-stage PS sampling lands (reverted Iter-44C; do not re-enable early).
- sRGB framebuffer as the gamma fix (double-correction trap).
- Decompiling D3D9 PEXE bytecode (PSRC source already exists).
- DXC / SM6 / DXIL (needless scope for SM2-era assets).
- Chasing exterior static-mesh shards before a settled re-capture separates real distortion from mid-load LOD smear.
- DPVS remeasure before geometry renders cleanly.
- All 26 TextureOperation modes up front (implement dominant ~10-12; log+fallback the rare ones).

### Architecture Approach

All new work plugs into the existing gl11_d.dll plugin at named, already-present seams — no new engine-level subsystems. The divergence point is precisely located at Direct3d11_PixelShaderProgramData.cpp:716-756 (constructor): the ctor reads m_exe D3D9 bytecode, looksLikeDxbc() fails, m_d3dPS is left NULL, and at draw time Direct3d11_StateCache::applyPreDrawState falls through to the per-VS dynamic PS (slot-0-only sample + vertex color) or magenta. The fix extends buildHlslForVSOutputs (:339) so the generated body evaluates the full TextureOperation stage cascade rather than sampling slot 0 only, re-keys the cache on (VS-sig hash + pass-material-sig hash), and adds the deferred Pass::apply constant uploads to Direct3d11_StaticShaderData::apply (:587). The compilePixelShaderFromHlsl helper (:86-261) already exists and is dead-but-correct — it just needs to be fed real material-driven HLSL bodies instead of returning NULL.

**Major components and their v2.2 roles:**

1. Direct3d11_PixelShaderProgramData.cpp (MODIFY) — primary locus: extend buildHlslForVSOutputs into a stage-op evaluator; re-key getOrCompilePSForVS on (VS-sig + material-sig); keep magenta fallback as visible diagnostic tombstone. Add a Direct3d11_PassMaterialPSGenerator helper implementing the dominant TextureOperation subset (TO_disable, selectArg1/2, modulate/2x/4x, add/subtract, blendTextureAlpha, lerp, plus alpha-op equivalents and TA_* arg modifiers) with log-and-fallback for rare ops.
2. Direct3d11_StaticShaderData.cpp ::apply (MODIFY) — add deferred per-pass constant uploads: material (VSCR_material c11-c15), textureFactor x2 (c44/c45), textureScroll (c47), alpha-test ref, stencil ref; mirror Direct3d9_StaticShaderData::Pass::apply:820-966. Pass per-stage TextureOperation + coord-set through to the PS generator. Add stencil state mapping into DSS.
3. ShaderImplementation.cpp:2900-2901 (ENGINE EDIT, gated) — change the PSRC load to KEEP the source text (mirror how m_text is kept for VS at :2155-2159). Gate behind D3D9-ignore so the working reference path is unaffected.
4. Direct3d11.cpp:253 setBrightnessContrastGamma_impl + new LUT-pass module (NEW) — wire the no-op stub; add a fullscreen LUT post-pass in the present path using the D3D9 pow() formula.
5. Direct3d11_StateCache.cpp:1593-1602 c9 viewportData (MODIFY) — fold the central half-pixel correction into the .z/.w offset terms of the screen-space->NDC convention; sign/magnitude consult-gated per Phase 18 D-01. The getOneToOneUVMapping stub at Direct3d11.cpp:1056 is a DEAD scaffold_fatal_stub with zero callers — do NOT implement it.
6. Direct3d11_ConstantBuffer.h (MODIFY as needed) — extend Direct3d11_PerMaterialCB / add stage-params struct if the 4 userConstants[4] slots are insufficient.

**Key integration invariants that must not break:**
- Cbuffer matrices: always XMMatrixTranspose at upload (d3d11_cbuffer_transpose_quirk).
- Persistent render targets: B8G8R8A8_UNORM, not BGRX8 (d3d11_baked_rt_bgra_format).
- Compare[] swap: C_GreaterOrEqual <-> C_NotEqual is intentional asset-parity; mirror it, never fix it (d3d9_compare_table_swap).
- D3D9 reference path: boot-test BOTH rasterMajor=5 AND rasterMajor=11 before any shared-code edit is claimed done.
- Koogie patches: fix callers/data, not Koogie diagnostic strictness.
- Per-pass state wires to Direct3d11_StaticShaderData::apply(passNumber), NOT to the dead Direct3d11_ShaderImplementationData::apply().

### Critical Pitfalls

1. **Gamma double-correction** — using sRGB backbuffer or sRGB SRVs instead of a single LUT post-pass replicating the D3D9 SetGammaRamp formula. D3D9 sampled textures raw (D3DSAMP_SRGBTEXTURE=0) and applied gamma only at scanout. Prevention: keep all texture SRVs _UNORM; implement a single fullscreen post-process LUT using pow(0.5+contrast*((f*brightness)-0.5), 1/gamma); schedule after PS-pipeline so you correct a correctly-shaded image.

2. **Claiming visual wins without D3D9/D3D11 screenshot diff** — the Iter-44B minimap over-claim propagated a false win through a full plan cycle. The minimap was never round in D3D11. Every parity claim requires a matched D3D9/D3D11 pair from the same scene/pose.

3. **PS-VS input linkage mismatch (id=342/id=343)** — the D3D11 runtime enforces that the PS input signature is a strict subset of the VS output signature, matched by semantic AND register position. Prevention: keep the reflection-driven input struct generation (buildHlslForVSOutputs :350-513); gate every PS-generator change on id=342==0 && id=343==0 in d3d11-debug.log.

4. **Re-enabling per-pass blend factors before multi-stage PS sampling lands** — Iter-44C demonstrated that wiring blend factors while the PS still ignores slots 1..7 amplifies unsampled-slot content into a whitening/brightening regression. Do not re-enable until the stage-op evaluator samples all bound SRVs and material constants upload.

5. **Single-stream skeletal shards** — the crash fix (905fb5d64) used defensive guards rather than flipping to multi-stream caps. The draw-side distortion likely persists. Must be resolved at the char-select beachhead: (a) flip Direct3d11_VertexBufferVectorData to report multi-stream support, or (b) correct single-stream skinning math. Confirm via RenderDoc mesh-viewer A/B. Do not conflate with exterior static-mesh shards (0013/0014 were mid-load captures).

6. **Textures appear is not pipeline complete** — when single-diffuse textured surfaces land, eyes will still be wrong (multi-stage), minimap still square (alpha-mask stage), specular absent (textureFactor constant), interiors blown-out (lighting constant). Track residual magenta and multi-stage gaps as explicit open items.

---

## Implications for Roadmap

All phases continue from Phase 16 (v2.1 Decruft). v2.2 starts at Phase 17.

### Phase 17: Asset-PS Census + Generator Core (Character-Select Beachhead)

**Rationale:** The single root cause blocker. Everything else gates on the PS generator landing on a known, bounded, deterministic shader surface (char-select exercises sul_m_head.sht / sul_eye.sht and a small body/clothing set enumerated by the Iter-44E multi-stage log). The census tool converts the biggest open risk (HLSL:ASM ratio) into a measured number before any generator work begins.

**Delivers:** (1) One-shot D3DReflect/D3DDisassemble census tool measuring PSRC-present / asm-vs-hlsl / ps-version across char-select .psh population. (2) Stage-op evaluator body in buildHlslForVSOutputs implementing the dominant ~10-12 TextureOperation modes. (3) Pass::apply constant uploads (material, textureFactor x2, textureScroll, alpha-test ref). (4) PSRC chunk retained at ShaderImplementation.cpp:2900-2901. (5) Cache re-keyed on VS-sig + material-sig. Result: char-select avatar renders with correct diffuse textures + correct eyes (colored irises, occluded by head).

**Addresses:** Single-diffuse textured surfaces (P1), character eyes (P1), character head (P1), multi-stage texture sampling (P1), Pass::apply constants (P1).

**Avoids:** Pitfall 5 (magenta/flat-white as same bug), Pitfall 4 (PS-VS linkage), Pitfall 6 (render-state defaults), Pitfall 9 (missing constants). Do NOT re-enable per-pass blend factors. Do NOT claim minimap fixed without screenshot diff.

**Research flag:** NEEDS per-phase research. Open questions: (a) HLSL:ASM ratio — census tool is the gating deliverable; (b) dominant TextureOperation subset that char-select exercises; (c) single-stream-skinning fix strategy.

**Verification gate:** D3D9/D3D11 matched screenshot diff at char-select default pose. Eyes: colored irises on face, occluded, match selected color. Clothing/skin: diffuse texture visible. id=342==0 && id=343==0 in d3d11-debug.log. Both rasterMajor=5 and =11 boot to char-select without crash.

### Phase 18: Load-Screen Half-Texel Seam (Independent Canary)

**Rationale:** Architecturally independent of Phase 17. Zero dependency on the PS pipeline. A small, isolated, deterministic win that exercises the 2D screen-space vertex/UV path. Best scheduled in parallel with Phase 17 or immediately after.

**Delivers:** Centerline seam eliminated on all D3D11 load screens. Half-pixel convention fixed once centrally at the c9 viewportData .z/.w offset terms in Direct3d11_StateCache.cpp:1593-1602 (sign/magnitude consult-gated, Phase 18 D-01). The getOneToOneUVMapping stub at Direct3d11.cpp:1056 is a DEAD scaffold_fatal_stub (zero callers) and is explicitly NOT implemented.

**Addresses:** Load-screen fullscreen blit (P2).

**Avoids:** Pitfall 2 (half-texel seam). Do NOT scatter +0.5 fudge factors per draw — fix the convention once centrally.

**Research flag:** Standard patterns (D3D9 to D3D10+ half-pixel fix is well-documented). No per-phase research needed.

### Phase 19: Gamma/Tone LUT + Lighting Constants

**Rationale:** Depends on Phase 17. Gamma judgment is invalid while surfaces are untextured because blown-out white is ambiguous between missing LUT and missing PS lighting. Must correct a correctly-shaded image, not a magenta one.

**Delivers:** (1) setBrightnessContrastGamma_impl at Direct3d11.cpp:253 wired to a fullscreen LUT/color-correction post-pass using D3D9 pow() formula. (2) Light-data + fog constant uploads (VSCR_lightData c16-c43, VSCR_extendedLightData c60-c67, fog c10) mirroring Direct3d9_LightManager::applyLights. Result: sky shows correct sunset tone; interiors show ambient/diffuse falloff instead of flat white.

**Addresses:** Lighting + gamma/tone (P2), blown-out flat-white interiors (P2).

**Avoids:** Pitfall 1 (gamma double-correction — no sRGB view flip, no sRGB SRVs, single LUT only).

**Research flag:** Standard for the LUT pass. The LightManager constant layout needs a focused code read against Direct3d9_LightManager to confirm offset-for-offset mapping into Direct3d11_VertexSlot0CB — flag that sub-area for brief per-phase research.

### Phase 20: Open-World PS Pipeline Extension + Minimap

**Rationale:** Phase 17 proves the generator on the bounded char-select set. Phase 20 extends it to the open-world shader population, which introduces the long tail of TextureOperation modes, per-stage texture-coordinate indices, and texture transforms. Minimap is gated here because it requires the multi-stage combiner (circular alpha-mask in uicanvas_radar.sht).

**Delivers:** (1) Extended TextureOperation coverage (terrain detail blends, specular/overlay composite, per-stage coord-set + textureScroll). (2) Stencil state mapping into DSS for decals/outlines/portals. (3) Minimap confirmed round via D3D9/D3D11 screenshot diff. (4) Shader permutation cache scale validation.

**Addresses:** World/terrain textured surfaces (P2), minimap round disc (P2), multi-stage sampling at world scale (P2), stencil-dependent effects (P3 start).

**Avoids:** Pitfall 5 (do not pre-claim minimap without screenshot diff). State-descriptor cache poisoning (zero-init all descriptors; verify create-count is stable).

**Research flag:** NEEDS per-phase research. Open questions: (a) extended TextureOperation census from an open-world session; (b) settled re-capture of exterior static-mesh shards (0013/0014) to scope real vs LOD-streaming distortion.

### Phase 21: Particles, Ribbon, and Swoosh Effects

**Rationale:** Depends on Phase 17 (PS-gen) AND Pass::apply constants (textureFactor, textureScroll). Ribbon/swoosh stretch is confirmed NOT a transform bug — needs draw-path instrumentation before a fix can be scoped.

**Delivers:** (1) Draw-path instrumentation to classify particle/ribbon/swoosh draws (appearance type, topology, VB format, transform class). (2) Particle billboard materials fixed (additive/alpha-blend PS correctly evaluated; textureFactor/scroll constants flowing). (3) Ribbon/swoosh fix based on what instrumentation surfaces.

**Addresses:** Particles/ribbon (P3).

**Research flag:** NEEDS per-phase research. The ribbon-stretch draw-path instrumentation output is the gating deliverable before any fix is designed.

### Phase 22: Exterior Geometry Distortion + Skeletal Shards (if not resolved in 17)

**Rationale:** Exterior static-mesh shards (0013/0014 captures) are potentially transient LOD smear. Skeletal shards may be resolved in Phase 17. This phase handles whatever skeletal question remains and resolves the exterior distortion question definitively via settled-frame re-capture.

**Delivers:** (1) Settled re-capture of 0013/0014 exterior frames to scope real vs LOD-streaming distortion. (2) Skeletal single-stream fix if not already resolved. (3) Any static-mesh distortion fix that the settled capture confirms as real.

**Research flag:** NEEDS per-phase research on single-stream-skinning-fix vs multi-stream-flip decision and settled re-capture findings.

### Phase 23: DPVS D3D11 Remeasure (SPEC R7 Deferral)

**Rationale:** Strictly last. Occlusion cost measurement is meaningless while geometry mis-shades. Only valid after all previous phases have produced clean-geometry rendering. Mirrors Phase 10 DpvsProfileInstrumentation methodology.

**Delivers:** PIX/RenderDoc timing harness; occlusion vs no-occlusion frame-time comparison under D3D11; final verdict confirming or revising Phase 10 Option alpha decision.

**Research flag:** Standard patterns (mirrors Phase 10 methodology). No per-phase research needed.

### Phase Ordering Rationale

- Phase 17 first: single root cause that blocks all visual parity work.
- Phase 18 independent: safe early win; establishes 2D sampling convention before Phase 20 minimap work.
- Phase 19 after Phase 17: gamma calibration against a magenta scene gives meaningless results.
- Phase 20 after Phase 17: TextureOperation combiner must prove out on char-select before world-scale extension.
- Phases 21-22 after Phase 20: particles need Pass::apply constants; settled exterior re-capture benefits from a fully-working PS pipeline.
- Phase 23 strictly last: Phase 10 reasoning and v2.0 deferral contract.

### Research Flags

Phases needing deeper research during planning:
- **Phase 17:** HLSL:ASM ratio (census tool is mandatory first deliverable); dominant TextureOperation subset on char-select; single-stream-skinning strategy.
- **Phase 19:** LightManager constant offset-for-offset mapping into Direct3d11_VertexSlot0CB.
- **Phase 20:** Extended TextureOperation census from open-world session; settled exterior re-capture scope.
- **Phase 21:** Ribbon/swoosh draw-path instrumentation (output scopes the fix; no fix design before the log).
- **Phase 22:** Single-stream-skinning-fix vs multi-stream-flip decision; settled re-capture findings.

Phases with standard patterns (skip research):
- **Phase 18:** D3D9 to D3D10+ half-pixel offset is well-documented; the central fix is the c9 viewportData convention in Direct3d11_StateCache.cpp:1593-1602 (sign consult-gated, Phase 18 D-01) — NOT the dead getOneToOneUVMapping stub at Direct3d11.cpp:1056.
- **Phase 23:** Mirrors Phase 10 DPVS methodology; no new patterns needed.

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | All APIs, compile flags, format constraints, and invariants read directly from the live plugin source and cross-checked against Microsoft docs. No ambiguity in the two-lane strategy. |
| Features | HIGH | Root cause confirmed from direct code reads (ShaderImplementation.cpp:2904, Direct3d11_PixelShaderProgramData.cpp:716-756). Feature boundaries and dependency graph derived from Phase 11/12 artifacts and the Iter-44 CODEX/Cursor deep-dive. |
| Architecture | HIGH | All integration points named with exact file:line. Divergence chain fully traced. Phase 11 precedent (VS compile pipeline) de-risks the PS generator approach. The compilePixelShaderFromHlsl dead-but-correct path reduces implementation risk significantly. |
| Pitfalls | HIGH | Most pitfalls are already-confirmed-in-this-engine (Iter-38B transpose, Iter-44B minimap over-claim, Iter-44C blend-factor revert, Iter-32A global blend trap, Phase 11 X4016 saga). Not predictions — recorded history. |

**Overall confidence:** HIGH

### Gaps to Address

- **HLSL:ASM ratio across live .psh population** — the single largest open unknown. Governs lane b vs lane d split. Handle by building the census tool as the FIRST deliverable in Phase 17.
- **Dominant TextureOperation subset** — which of the 26 ops actually appear in char-select + Mos Eisley live draws. Handle by extending the census tool to log op usage per draw; implement only the confirmed dominant subset and log+fallback the rest.
- **Single-stream-skinning-fix vs multi-stream-flip** — crash fix (905fb5d64) chose defensive guards over a caps change. The draw-side distortion likely persists. Requires a RenderDoc mesh-viewer capture during Phase 17 planning.
- **Ribbon/swoosh root cause** — confirmed NOT a transform bug but cause unknown from source search alone. Handle by instrumenting the live draw path in Phase 21 before designing a fix.
- **Exterior static-mesh shards (0013/0014)** — potentially transient LOD smear from mid-load captures. Handle by re-capturing fully-settled exterior frames in Phase 22.
- **Eye color mechanism** — confirmed palette-driven via CustomizableShader hue (NOT animated UV). The multi-stage PS fix is the correct path. No additional validation needed before Phase 17.

---

## Sources

### Primary (HIGH confidence)

- Live source tree: ShaderImplementation.cpp:2895-2911 (PSRC discarded / PEXE kept); ShaderImplementation.h:498-526 (26-op TextureOperation enum); Direct3d11_PixelShaderProgramData.cpp:716-756 (divergence ctor), :339-513 (generator), :605-623 (magenta fallback), :86-261 (compilePixelShaderFromHlsl); Direct3d11_StaticShaderData.cpp:587 (apply), :820-966 (D3D9 Pass::apply parity reference); Direct3d11.cpp:253 (gamma no-op), :1056 (dead getOneToOneUVMapping scaffold_fatal_stub, zero callers); Direct3d11_StateCache.cpp:1029 (applyPreDrawState), :1107-1123 (PS-bind chain); Direct3d11_ConstantBuffer.h; Direct3d9_VertexShaderConstantRegisters.h / Direct3d9_PixelShaderConstantRegisters.h; ShaderBuilder PixelShaderProgramView.cpp:172,308 (PSRC is asm or HLSL); Node.cpp:5843-5849 (PSRC+PEXE both written to asset).
- Phase 11 artifacts: 11-SUMMARY.md; 11-09.15-CODEX-CONSULT-iter44-pipeline-deepdive.md (+ RESPONSES); 11-07-LEARNINGS.md.
- docs/research/phase12-baseline/COMPARISON.md — authoritative 5-bucket gap catalogue with matched D3D9/D3D11 image evidence.
- Project memory records: d3d11_cbuffer_transpose_quirk, d3d11_baked_rt_bgra_format, d3d9_compare_table_swap, project_phase11_close_pass_with_deferrals, project_phase11_minimap_never_round, project_phase12_visual_baseline, project_d3d11_collide_use_after_free, project_rastermajor_values.

### Secondary (MEDIUM confidence)

- Specifying Compiler Targets (Microsoft Learn) — D3DCompile drops legacy ps_1_x; *_4_0_level_9_x backward-compat profiles.
- Converting data for the color space (Microsoft Learn) — sRGB RTV special exception on *_UNORM swap-chain buffers (documented but rejected as the primary gamma fix).
- Directly Mapping Texels to Pixels D3D9 (Microsoft Learn) + Solving DX9 Half-Pixel Offset (Aras Pranckevicius) + Half-Pixel Offset in DirectX 11 (Adam Sawicki) — canonical D3D9 -0.5 rule and removal in D3D10+.
- HLSL, FXC, and D3DCompile (Chuck Walbourn) — fxc/D3DCompile status, SM4/5 support, fxc maintenance posture.
- RenderDoc docs — D3D11 vertex/pixel/compute shader debugging; mesh viewer; D3DCOMPILE_DEBUG required for symbols.

### Tertiary (LOW confidence — listed to substantiate rejections)

- HlslDecompiler / dx-shader-decompiler — referenced only to substantiate rejection of bytecode-decompile option (a); PSRC source already exists.

---
*Research completed: 2026-05-27*
*Ready for roadmap: yes*
