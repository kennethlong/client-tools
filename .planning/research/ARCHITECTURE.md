# Architecture Research

**Domain:** Asset pixel-shader pipeline + visual-fix integration into an existing D3D11 renderer plugin (`gl11_d.dll`) for a legacy SWG client engine (clientGraphics shader abstraction)
**Researched:** 2026-05-27
**Confidence:** HIGH (all integration points read directly from the live source tree; cross-checked against Phase 11 SUMMARY, the Iter-44 CODEX/Cursor deep-dive consult, and the phase12-baseline COMPARISON)

> **Scope note.** This is *not* a greenfield architecture. The D3D11 plugin already exists and renders to visual-parity-minus-deferrals. This document maps where the NEW v2.2 visual work plugs into the EXISTING components, names the exact divergence point where D3D11 falls back to magenta, and recommends a build order honoring char-select-first. "Standard Architecture" below describes the *engine's* shader-binding contract (the thing both renderers implement), not an idealized external pattern.

---

## Standard Architecture

### System Overview — the engine shader-binding contract both renderers implement

```
┌──────────────────────────────────────────────────────────────────────────┐
│  ENGINE (clientGraphics, renderer-agnostic) — src/.../library/clientGraphics │
│                                                                            │
│  .sht asset ──► StaticShaderTemplate ──► StaticShader (per-instance)       │
│       │              (TextureData map,       (texture/material/factor       │
│       │               sampler defaults)       overrides)                    │
│       ▼                                                                    │
│  .eft asset ──► ShaderEffect ──► ShaderImplementation (m_pass[])           │
│                                       │                                     │
│         ShaderImplementation::Pass ───┤  (per-pass render-state + shaders) │
│           ├─ m_vertexShader  ─► ...PassVertexShader  ─► m_text (HLSL SRC)  │ ◄── VS = SOURCE
│           ├─ m_pixelShader   ─► ...PassPixelShader                          │
│           │       ├─ m_program ─► ...PixelShaderProgram ─► m_exe (PEXE!)   │ ◄── PS = D3D9 BYTECODE
│           │       └─ m_textureSamplers[] (m_textureIndex, m_textureTag)     │
│           └─ m_stage[] (FFP TextureOperation cascade — VSPS path unused)   │
│                                                                            │
│  Graphics:: factory dispatch (the renderer boundary):                      │
│     createVertexShaderData()        ─► plugin VS wrapper                    │
│     createPixelShaderProgramData()  ─► plugin PS wrapper  ◄── DIVERGES HERE │
│     createStaticShaderData()        ─► plugin per-shader wrapper            │
└───────────────────────────────────┬────────────────────────────────────────┘
                                     │  Gl_api function table (119 funcs)
        ┌────────────────────────────┴────────────────────────────┐
        ▼                                                          ▼
┌─────────────────────────────┐                  ┌─────────────────────────────────┐
│  gl05_d.dll (D3D9, ref)     │                  │  gl11_d.dll (D3D11, under dev)   │
│  Direct3d9_*                │                  │  Direct3d11_*                    │
│  • PS: CreatePixelShader(   │                  │  • PS: m_exe is D3D9 bytecode ─► │
│      m_exe) — ACCEPTED      │                  │      CreatePixelShader REJECTS ─►│
│  • SetVertexShaderConstantF │                  │      m_d3dPS = NULL ─► MAGENTA   │
│    / SetPixelShaderConstantF│                  │  • cbuffer slot model (Map/Discard)│
│    register-file model      │                  │  • per-VS dynamic PS (tex0*color)│
│  • SetGammaRamp             │                  │  • setBrightnessContrastGamma=NOP │
│  • multitexture stages      │                  │  • SRV/sampler slots 0..7        │
└─────────────────────────────┘                  └─────────────────────────────────┘
```

### Component Responsibilities (real names from the tree)

| Component | File | Responsibility |
|-----------|------|----------------|
| `ShaderImplementation` / `::Pass` | `src/engine/client/library/clientGraphics/src/shared/ShaderImplementation.{h,cpp}` | Engine-side per-pass render state + VS/PS handles + FFP `m_stage[]`. Renderer-agnostic. **The contract both plugins implement.** |
| `ShaderImplementationPassPixelShaderProgram` | same header, class at `:626`, `m_exe` at `:680` | Owns the **pre-compiled D3D9 PEXE bytecode** (`DWORD *m_exe`). Loaded from the `PEXE` IFF chunk (`ShaderImplementation.cpp:2904`). This is the data that D3D11 cannot consume. |
| `ShaderImplementationPassVertexShader` | same header; `m_text` loaded at `ShaderImplementation.cpp:2155-2159` | Owns the **HLSL source text** (`m_text`) — read from the `.vsh` TreeFile. Asymmetry root: VS = source, PS = bytecode. |
| `ShaderImplementationPassPixelShaderTextureSampler` | same header, class at `:686` | Per-stage `m_textureIndex` + `m_textureTag` + address/filter — the multi-stage binding list (VSPS path). |
| `ShaderImplementationPassStage` (`::TextureOperation`) | same header, enum at `:498-526` (26 ops) | The **FFP texture-stage cascade** (`TO_modulate`, `TO_lerp`, `TO_blendTextureAlpha`, ...). Drives the D3D9 FFP path; the VSPS path bypasses it. A new generated-PS material evaluator would read this. |
| `Direct3d9_PixelShaderProgramData` | `src/.../Direct3d9/src/win32/Direct3d9_PixelShaderProgramData.cpp` | D3D9 reference: `CreatePixelShader(m_exe)` succeeds. |
| `Direct3d11_PixelShaderProgramData` | `src/.../Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.cpp` | **THE divergence component.** Constructor (`:716-756`) detects D3D9 bytecode, leaves `m_d3dPS = NULL`; magenta fallback compiled in `install()` (`:605-623`); per-VS dynamic PS generated in `getOrCompilePSForVS` (`:647`) + `buildHlslForVSOutputs` (`:339`, samples slot 0 only). |
| `Direct3d11_StaticShaderData::apply` | `src/.../Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp:587` | Per-draw per-pass bind. Currently sets VS/PS pointers, alpha-blend enable (Iter-39C), alpha-test (44B), depth (44A), color-write (44A), and binds SRV/sampler slots 0..7 (44E). **Does NOT upload material/textureFactor/textureScroll constants.** |
| `Direct3d9_ShaderImplementationData::Pass::apply` | `src/.../Direct3d9/src/win32/Direct3d9_ShaderImplementationData.cpp:341` | D3D9 reference: pushes the full per-pass render-state packet (`construct` at `:224-308` builds it via `RSM`/`RSB`/`RSV` macros: shade mode, dither, z, blend, alpha-test, color-write, **stencil**, FFP material-source). |
| `Direct3d9_StaticShaderData::Pass::apply` | `src/.../Direct3d9/src/win32/Direct3d9_StaticShaderData.cpp:820` | D3D9 reference: uploads `VSCR_material`/`PSCR_materialSpecularColor` (`:847-863`), `textureFactor` ×2 (`:895-896`), `textureScroll` (`:922`), alpha-test ref, stencil ref, fog. **This is the work `Direct3d11_StaticShaderData::apply` has NOT yet ported.** |
| `Direct3d11_ConstantBuffer` (+ `Direct3d11_VertexSlot0CB`) | `src/.../Direct3d11/src/win32/Direct3d11_ConstantBuffer.{h,cpp}` | The D3D11 cbuffer model: 4 VS slots + 4 PS slots, each 1152B, `Map(WRITE_DISCARD)`. `Direct3d11_VertexSlot0CB` mirrors the D3D9 VSCR register file (WVP c0, World c4, material c11, lightData c16, ...). |
| `Direct3d11_StateCache` | `src/.../Direct3d11/src/win32/Direct3d11_StateCache.cpp` | Hash-cached DSS/BS/RS; `applyPreDrawState` (`:1029`) flushes SRV/sampler arrays + binds VS/PS (`:1106-1123`) + cbuffers (`:1136-1139`). PS-bind decision: asset PS (null) → per-VS dynamic PS → magenta. |
| `Direct3d11_VertexShaderData` | `src/.../Direct3d11/src/win32/Direct3d11_VertexShaderData.cpp` | The **working** VS-compile pipeline: `D3DCompile(m_text, ...)` with HLSL textual rewrite + `.cso` cache + `D3DReflect` outputs (`getReflectedOutputs`/`getBytecodeHash`). The asset-PS bridge must mirror this. |

---

## The Divergence Point (precisely pinpointed)

**File:** `src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.cpp`
**Function:** `Direct3d11_PixelShaderProgramData::Direct3d11_PixelShaderProgramData(...)` — constructor, lines **716-756**.

The chain:

1. Engine loads a `.sht`/`.eft`, builds `ShaderImplementation::Pass`. For each pass's pixel shader, `ShaderImplementation.cpp:2765` calls `Graphics::createPixelShaderProgramData(*this)`, dispatched through `Gl_api` into the plugin.
2. In D3D11 the wrapper ctor reads `pixelShaderProgram.m_exe` (the `DWORD*` PEXE bytecode). It checks `looksLikeDxbc(exe)` (`:268-274`, tests for `'DXBC'` magic `0x43425844`). The shipped bytecode is **D3D9-era `ps_2_x` DWORD bytecode**, not DXBC → the check fails.
3. The ctor logs the D3D9 PS version and **returns with `m_d3dPS` left NULL** (`:748-755`). It does NOT call `CreatePixelShader` (would `E_INVALIDARG`).
4. At draw time, `Direct3d11_StateCache::applyPreDrawState` (`:1107-1123`): `ms_currentPSData->getPixelShader()` is NULL → falls to `selectFallbackPSForVS` → `getOrCompilePSForVS` returns the per-VS dynamic PS (`tex0.Sample(s0,uv) * COLOR0`, or magenta on validation failure).
5. The per-VS PS samples **only SRV slot 0** and applies no multi-stage combiner → multi-stage materials (eyes, head, minimap alpha-mask, terrain detail) render wrong; FFP-only / no-COLOR0 passes fall through to literal **magenta `float4(1,0,1,1)`**.

**Root asymmetry:** VS ships HLSL **source** (`m_text`, `ShaderImplementation.cpp:2155-2159`) → D3DCompile works. PS ships pre-compiled **D3D9 bytecode** (`m_exe`, `ShaderImplementation.cpp:2904-2908`) → no source to recompile. This is why the Phase-11 VS pipeline succeeded and the PS pipeline did not.

---

## Recommended Integration: where the new asset-PS bridge lives

**Decision: a generated-PS material evaluator inside the gl11 plugin, NOT engine-side asset recompile, NOT per-asset hand-authored bytecode.** This is the CODEX/Cursor-converged option (e) from the Iter-44 deep-dive: read engine stage/material data, generate HLSL, compile via the existing VS toolchain.

Rationale (HIGH confidence — from the consult + the live constraints):
- **Engine-side `.psh` → SM5 offline translation** is rejected: there is no SM5 DXBC for the shipped assets, no licensed re-author source, and the user reads `.tre` from a retail install (read-only, no redistribution). A translator for D3D9 `ps_2_x` bytecode → DXBC is a multi-month tool project.
- **Per-asset hand-authored bytecode** is rejected as the *first* move: it does not generalize and creates per-asset churn before the renderer can express the engine's material algebra.
- **The generated-PS approach reuses the proven Phase-11 VS toolchain**: `D3DCompile` + `Direct3d11_HlslRewrite` + `Direct3d11_ShaderCache` (`.cso` cache) + `D3DReflect`. The infrastructure already exists in `Direct3d11_PixelShaderProgramData::compilePixelShaderFromHlsl` (`:86-261`) — it is dead-but-correct today.

### How it relates to the Phase-11 VS pipeline

| Phase-11 VS pipeline (working) | New asset-PS bridge (to build) |
|--------------------------------|--------------------------------|
| Input: `m_text` HLSL source | Input: engine `Pass` material model — `m_textureSamplers[]` + (for FFP) `m_stage[]` `TextureOperation` cascade + `StaticShader` factor/material data |
| `Direct3d11_VertexShaderData::compileOrLoad` → `D3DCompile` | **Extend** `Direct3d11_PixelShaderProgramData`: generate HLSL body from the pass material model, compile via the **existing** `compilePixelShaderFromHlsl` |
| Cache key: bytecode hash | **New** cache key: VS-output signature hash **+ pass-material signature** (CODEX: "per-pass material PS, not only per-VS PS") |
| `D3DReflect` → input-layout match | Keep the per-VS reflected input-struct generation (`buildHlslForVSOutputs`) — it solved the register-position linkage; replace only the **body** with the stage-op evaluator |

### Concrete: NEW vs MODIFIED components

**MODIFY (existing files):**
- `Direct3d11_PixelShaderProgramData.cpp` — `buildHlslForVSOutputs` body becomes a stage-op evaluator; `getOrCompilePSForVS` re-keys on (VS sig + material sig). Keep the magenta fallback as the validation-failure tombstone.
- `Direct3d11_StaticShaderData.cpp` `::apply` (`:587`) — add the deferred **constant uploads** (material, textureFactor ×2, textureScroll, alpha-test ref) by calling `Direct3d11_ConstantBuffer::updateVS/updatePS`, mirroring `Direct3d9_StaticShaderData::Pass::apply`. The plumbing exists; the per-pass uploads are TODO. Iter-45 already fixed the PS-slot-2 dead-write prerequisite.
- `Direct3d11_StaticShaderData.cpp` `::construct` — already resolves all 8 stages (Iter-44E); pass the per-stage `TextureOperation` through to the PS generator (today it's read only for binding, not for combiner logic).
- `Direct3d11_ConstantBuffer.h` — extend `Direct3d11_PerMaterialCB` / add a stage-params struct for textureFactor/textureScroll if the 4 `userConstants[4]` slots are insufficient.

**NEW (likely new translation units):**
- A `Direct3d11_PassMaterialPSGenerator` helper (or a new namespace inside `Direct3d11_PixelShaderProgramData.cpp`) that emits HLSL for the dominant `TextureOperation` subset (CODEX list: `TO_disable`, `TO_selectArg1/2`, `TO_modulate`/`2x`/`4x`, `TO_add`/`subtract`, `TO_blend*Alpha`, `TO_lerp`, + alpha-op equivalents + `TA_*` arg modifiers), with log-and-fallback for rare ops (`TO_bumpEnvMap`, `TO_dotProduct3`, `TO_multiplyAdd`).
- A gamma/color-correction post-process pass module (see "Gamma" below).

---

## Pass::apply Constant Uploads — what the asset PS expects and how it maps

**The constants the engine's VS/PS bytecode reads** are fixed register slots, defined in two enums (do not renumber — encoded into asset bytecode):

`Direct3d9_VertexShaderConstantRegisters.h`:
| Register | Symbol | Bytes | Maps to in `Direct3d11_VertexSlot0CB` |
|----------|--------|-------|----------------------------------------|
| c0–c3 | `VSCR_objectWorldCameraProjectionMatrix` (WVP) | 64 | `objectWorldCameraProjectionMatrix` (offset 0) ✓ already uploaded |
| c4–c7 | `VSCR_objectWorldMatrix` | 64 | `objectWorldMatrix` ✓ already uploaded |
| c8 | `VSCR_cameraPosition` | 16 | present in slot-0 layout |
| c9 | `VSCR_viewportData` | 16 | present (Iter-37/8 area) |
| c10 | `VSCR_fog` | 16 | present |
| **c11–c15** | **`VSCR_material`** (5 regs) | 80 | `material[5]` (offset 176) — **slot allocated, NOT uploaded per-pass** |
| c16–c43 | `VSCR_lightData` (28 regs) | 448 | `lightData[28]` (offset 256) — LightManager territory |
| **c44/c45** | **`VSCR_textureFactor` / `textureFactor2`** | 32 | **not yet uploaded** |
| **c47** | **`VSCR_textureScroll`** | 16 | **not yet uploaded** (animated UV scroll) |
| c48–c51 | `VSCR_currentTime`/`unitX/Y/Z` | — | per-frame |
| c52–c59 | `VCSR_userConstant0..7` | 128 | `Direct3d11_PerObjectCB::userConstants[8]` |
| c60–c67 | `VSCR_extendedLightData` | 128 | LightManager territory |

`Direct3d9_PixelShaderConstantRegisters.h`:
| Symbol | Maps to |
|--------|---------|
| `PSCR_textureFactor` / `textureFactor2` | PS cbuffer — **not uploaded** |
| `PSCR_materialSpecularColor` | `Direct3d11_PerMaterialCB::materialSpecular` — **not uploaded per-pass** |
| `PSCR_dot3Light*` | dot3 lighting (rare; defer) |
| `PSCR_userConstant` | `Direct3d11_PerMaterialCB::userConstants[4]` |

**Integration:** mirror `Direct3d9_StaticShaderData::Pass::apply` (`:820-948`). For each per-pass `apply` in `Direct3d11_StaticShaderData::apply`, when `m_materialValid`/`m_textureFactorValid`/`m_textureScrollValid`, pack the values into the cbuffer struct and call `Direct3d11_ConstantBuffer::updateVS(0, ...)` / `updatePS(2, ...)`. **Cbuffer transpose quirk applies** to any matrix (memory `d3d11_cbuffer_transpose_quirk`) — material/factor are vectors so unaffected, but if a texture-transform matrix is added it must be `XMMatrixTranspose`d at upload. **The D3D9 `Compare[]` swap** (`C_GreaterOrEqual`↔`C_NotEqual`, memory `d3d9_compare_table_swap`) is already mirrored in `Direct3d11_StaticShaderData::translateCompare` (`:159-173`) — preserve it.

---

## Texture/Sampler Stage Binding — D3D11 vs D3D9 multitexture

**D3D9 model:** FFP multitexture — each `Stage` sets `D3DTSS_COLOROP/ALPHAOP/COLORARG*/RESULTARG` and `SetTexture(stage, tex)`; the cascade terminates with `D3DTOP_DISABLE`. For VSPS shaders the stage ops are bypassed and the authored `.psh` samples its own registers. Stage→texture mapping is by `m_textureIndex` (`Direct3d9_StaticShaderData.cpp:786-789`: `m_stage[(*i)->m_textureIndex].construct(...)`).

**D3D11 model (already built):** `Direct3d11_StaticShaderData::construct` (`:404-536`) walks `m_textureSamplers`, resolves each to a `Direct3d11_TextureData` SRV at its `m_textureIndex` slot, builds a `D3D11_SAMPLER_DESC` (`buildSamplerDescForStage0`, `:196-237`). `apply` (`:717-736`) binds slots 0..7 via `setPixelShaderResource`/`setPixelShaderSampler`; absent slots are explicitly unbound (avoids sticky SRV bleed). `applyPreDrawState` (`:1142`) flushes the full SRV array.

**The gap is NOT binding — it is consumption.** SRVs 1..7 are bound but the generated PS samples only `t0`. Iter-44E proved binding works (eye uses 3 textures, body 2, all bound) but produced no visual change. **The fix is the PS generator (above), not more binding work.** When the stage-op evaluator lands, it reads each stage's `TextureOperation` + `TextureArgument` and emits `result = combine(sample(tN, uv), prev, ...)` per stage — turning bound-but-unused SRVs into actual multitexture compositing.

Sampler/texture address/filter translation is complete (`toD3D11Filter`/`toD3D11Address`, `:70-189`). Remaining stage-binding work: honor per-stage **texture-coordinate index** and **texture transform** (today every stage samples `TEXCOORD0`; terrain detail/projected textures need the stage's coord set + `textureScroll`).

---

## Gamma Correction — where it belongs

**Architectural decision: a final fullscreen post-process color-correction pass (or 1D/2D LUT) before present — NOT a swap-chain sRGB view, NOT `IDXGIOutput6`.** (CODEX-confirmed; HIGH confidence.)

- D3D9 used `IDirect3DDevice9::SetGammaRamp` with the formula `pow(0.5 + contrast*((x*brightness) - 0.5), 1/gamma)` (`Direct3d9` builds a 256-entry ramp). DXGI flip-model has **no device-level `SetGammaRamp`**.
- A swap-chain sRGB view only does the fixed sRGB transfer curve — it cannot express the engine's brightness/contrast/gamma triple.
- `IDXGIOutput6` is HDR/display-metadata oriented, not a portable per-swap-chain gamma replacement.

**Integration point:** `Direct3d11.cpp:253-256` `setBrightnessContrastGamma_impl` is currently a no-op. Wire it to **store** brightness/contrast/gamma; if all default, skip. Otherwise, in the present path, render the resolved scene through a fullscreen triangle that applies the D3D9 formula (or samples a LUT texture built with it) into the presentable backbuffer. The fullscreen-blit infrastructure for this is the same one used by the load-screen path (below) and the baked-RT path (already BGRA8-correct per memory `d3d11_baked_rt_bgra_format`). This is **coupled to the PS pipeline only loosely** — it can land independently but should come *after* the PS pipeline so the "washed sky" symptom is judged against correctly-shaded content, not magenta.

---

## Fullscreen-Blit (load-screen) + 2D / Minimap Paths

**Where they live:** the 2D / fullscreen-image path runs through the engine's UI canvas + `setRenderTarget(NULL)` "restore back buffer" idiom (`11-07-LEARNINGS.md`: `setRenderTargetToPrimary`). The load-screen splash is drawn as a 2D textured quad (the `2d_texture.vsh` / `2d.vsh` backdrop VS family — `Direct3d11_PixelShaderProgramData.cpp:478-505` handles these branches). `drawQuadList` (`Direct3d11_StateCache`) was the stub that silently dropped UI quads until Iter-27 implemented it — UI/2D now routes through it.

**Half-texel seam (load-screen):** the splash exceeds max texture width → drawn as two side-by-side halves; the shared edge samples off by a half-texel (D3D9's `-0.5` texel-center rule, removed in D3D10+). This is a **2D sampling/blit-path bug, image-independent** (confirmed across many D3D11 load screens, never on D3D9). **Slots in at the 2D vertex/UV generation** — either the `getOneToOneUVMapping` slot (currently `STUB` at `Direct3d11.cpp:995`) or the screen-space vertex emit. It is the cleanest, most isolated canary: no 3D, no lighting, no multitexture. **Independent of the PS pipeline** — can be fixed early as a standalone win, but it exercises the same sampling path that 2D minimap work will touch.

**Minimap (square→round):** NOT a separate 2D bug — it is the **same PS-gen / multi-stage family** (memory `project_phase11_minimap_never_round`, confirmed 2026-05-24). The round disc in D3D9 comes from a circular **alpha-mask texture** that the D3D11 dynamic PS does not sample (multi-stage gap). **Therefore the minimap fix is gated on the multi-stage PS combiner** — re-check the radar only after the stage-op evaluator lands; do not treat it as standalone 2D work.

---

## Data Flow — the new shading path (after integration)

```
Engine builds StaticShader/Pass (unchanged)
        │
        ▼
Direct3d11_StaticShaderData::construct
   • resolve SRV/sampler per m_textureIndex (slots 0..7)   [exists, Iter-44E]
   • NEW: capture per-stage TextureOperation + TextureArgument + coord set
        │
        ▼
Direct3d11_StaticShaderData::apply(pass)         [per draw]
   • set VS/PS data pointers                              [exists]
   • per-pass render state: blend/alphatest/depth/colorwrite  [exists, 39C/44A/44B]
   • NEW: per-pass STENCIL state into DSS                 [gap — decals/outlines]
   • NEW: upload material (VSCR_material/PSCR_materialSpecular),
          textureFactor×2, textureScroll, alpha-test ref  [gap — mirror D3D9 Pass::apply]
   • bind SRV/sampler slots 0..7                          [exists]
        │
        ▼
Direct3d11_PixelShaderProgramData::getOrCompilePSForVS
   • key = VS-output-sig hash + pass-material-sig hash     [NEW key]
   • generate HLSL: reflected input struct (exists) +
     NEW stage-op evaluator body (multitexture combine)
   • compile via existing compilePixelShaderFromHlsl       [exists, dead today]
        │
        ▼
Direct3d11_StateCache::applyPreDrawState
   • PSSetShader(generated material PS)                    [exists, selection path]
   • flush SRV/sampler arrays + cbuffers                   [exists]
   • Draw*                                                 [exists]
        │
        ▼
present path
   • NEW: if brightness/contrast/gamma != default →
     fullscreen color-correction pass (LUT)               [NEW — replaces SetGammaRamp]
```

---

## Recommended Build Order (honors char-select-first; respects dependencies)

The character-select screen is the deterministic beachhead: quick to reach, isolated, exercises a small known shader set (`sul_*_head.sht`, `sul_eye.sht` — 3-stage eye/head materials + body textures). The Iter-44E log already enumerated which assets bind which stages, so the test surface is known.

| # | Workstream | Why this order | Primary files |
|---|-----------|----------------|---------------|
| **1** | **Asset-PS generator core on char-select** — stage-op evaluator body + per-pass constant uploads (material, textureFactor, textureScroll, alpha-test ref). Prove on `sul_m_head.sht` / `sul_eye.sht` (eyes + textures correct). | THE blocker; everything else (minimap, lighting, particles) is gated on multi-stage PS evaluation. Char-select is the smallest deterministic surface. | `Direct3d11_PixelShaderProgramData.cpp`, `Direct3d11_StaticShaderData.cpp`, `Direct3d11_ConstantBuffer.h` |
| **2** | **Generalize the generator to open-world surfaces** — terrain detail blends, building trim, reflection/spec maps; widen the `TextureOperation` subset; add per-stage texture-coordinate index + texture transform; add **stencil** state mapping (decals/interiors). | Same generator, broader op coverage + coord/stencil. Re-uses #1; reveals the long tail of ops. | same as #1 + `Direct3d11_StateCache` (DSS stencil) |
| **3** | **Load-screen half-texel seam** (can run in parallel with #1 — independent) | Isolated 2D sampling canary; standalone win; de-risks the sampling path before minimap. No PS-pipeline dependency. | `Direct3d11_StateCache` (drawQuadList/UV emit), `Direct3d11.cpp` (`getOneToOneUVMapping`) |
| **4** | **Gamma / lighting** — wire `setBrightnessContrastGamma_impl` to a fullscreen LUT pass; LightManager constant parity (`Direct3d11_LightManager` vs `Direct3d9_LightManager::applyLights`, `VSCR_lightData`/`extendedLightData`). | Must be judged against correctly-shaded content (after #1/#2), or "washed sky" can't be distinguished from missing-PS. | `Direct3d11.cpp:253`, `Direct3d11_LightManager.cpp`, new LUT-pass module |
| **5** | **Minimap (square→round)** | Gated on the multi-stage combiner from #1/#2 (round = circular alpha-mask texture sampled by a multi-stage PS). Only meaningful after the combiner works. | re-check after #1/#2; `Direct3d11_StaticShaderData` / generator |
| **6** | **Particles / ribbon / swoosh** — draw-path instrumentation (appearance/topology/transform class) first, then fix per the identity-W vs owner-W path map. | Post-PS-pipeline; CODEX says source search alone can't prove the live path — needs draw-time logging. Independent of #1 but lower priority. | `Direct3d11_StateCache` draw dispatch; engine `ParticleEmitter`/`RibbonAppearance`/`SwooshAppearance` |
| **7** | **DPVS D3D11 remeasure** (SPEC R7, deferred from v2.0) | **LAST.** Occlusion-cost measurement is meaningless until all geometry renders cleanly (magenta/untextured surfaces corrupt the GPU-cost signal). Phase 10 verdict was Option α "remove globally"; the D3D11 remeasure validates that decision on the new backend. | `RenderWorld.cpp` (cullingParameters), new throwaway timing harness (mirror Phase 10's `DpvsProfileInstrumentation`) |

**Dependency summary:** 1 → 2 → 5 (minimap needs the combiner). 1 → 4 (gamma judged post-shading). 3 is independent (can land any time, good early win). 6 is post-pipeline. 7 is strictly last.

---

## Anti-Patterns (specific to this integration)

### Anti-Pattern 1: "Sample all bound slots" / modulate-everything PS
**What people do:** make the dynamic PS sample `t0..t7` and multiply them.
**Why it's wrong:** CODEX-confirmed — it produces false wins for ordinary diffuse overlays but is *actively wrong* for additive glow, alpha-lerp masks, specular, reflection, and dot3 paths, and hides which semantic was needed.
**Do this instead:** generate the PS from the engine's `TextureOperation`/`TextureArgument` per stage (the FFP cascade algebra), keyed by material signature.

### Anti-Pattern 2: Treating multi-stage SRV binding as visible progress
**What people do:** bind more SRVs and expect pixels to change (Iter-44E).
**Why it's wrong:** binding without a PS that *evaluates* the stages is visually inert.
**Do this instead:** land the binding and the generator together; judge by pixels, not by "binding works" logs.

### Anti-Pattern 3: Default-and-silently-ignore unimplemented states
**What people do:** install() defaults for states the PS-gen doesn't yet honor.
**Why it's wrong:** hides whether a visual bug is "state never needed" vs "state silently ignored" (CODEX hindsight note).
**Do this instead:** log pass usage of unimplemented ops/states (the log-and-fallback discipline) so the long tail surfaces explicitly.

### Anti-Pattern 4: Claiming a visual win without a D3D9-vs-D3D11 screenshot diff
**What people do:** mark minimap/eyes "fixed" from a single capture (the Iter-44B "minimap circular" over-claim).
**Why it's wrong:** propagated a false win for a full plan cycle.
**Do this instead:** every visual claim needs a matched D3D9/D3D11 pair (the phase12-baseline method).

### Anti-Pattern 5: Editing Koogie's diagnostic patches or the Direct3d9 reference tree
**What people do:** "fix" the D3D9 `Compare[]` swap or touch Koogie's patches.
**Why it's wrong:** the swap is intentional asset parity (memory `d3d9_compare_table_swap`); D3D9 must stay the known-good reference (invariant D-05).
**Do this instead:** mirror D3D9 quirks in D3D11; fix the caller/data, not the reference.

---

## Integration Points

### Internal Boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| Engine `ShaderImplementation` ↔ plugin | `Gl_api` factory dispatch (`createPixelShaderProgramData`, `createVertexShaderData`, `createStaticShaderData`) | The renderer boundary. New work stays plugin-side; engine contract unchanged. Plugin reads engine fields via **friend-access** (non-inline clientGraphics members aren't DLL-exported — `Direct3d11_StaticShaderData.cpp:299-308`). |
| `StaticShaderData::apply` ↔ `ConstantBuffer` | `updateVS/updatePS(slot, data, bytes)` + `bindVS/bindPS` | Constant uploads route here; never call `VSSetConstantBuffers` directly from apply. |
| `StaticShaderData::apply` ↔ `StateCache` | public setters (`setPixelShaderResource`, `setPixelShaderSampler`, `setAlphaTest`, ...) | CODEX Q1: never call `PSSet*` directly from `StaticShaderData`; preserve the lazy-bind/flush model. |
| `PixelShaderProgramData` ↔ `VertexShaderData` | `getReflectedOutputs()` / `getBytecodeHash()` | The generator reads VS reflection to build a register-matched PS input struct. |
| Generated PS ↔ `ShaderCache` | FNV-1a `hashSource` + `.cso` store/load; `D3D11_REWRITE_VERSION` macro is the cache-invalidation lever | Bump the version on any generator change or stale `.cso` blobs serve. |

### External / Runtime

| Service | Integration | Notes |
|---------|-------------|-------|
| Retail `.tre` assets | read-only via `TreeFile` | `.vsh` = HLSL source (works); `.psh` = D3D9 PEXE bytecode (the blocker). No redistribution; no asset re-author. |
| `rasterMajor` selector | `client_d.cfg` → `gl11_d.dll` (value 11) vs `gl05_d.dll` (value 5) | Debug exe reads `client_d.cfg`; D3D9 stays the parity reference. Memory `project_rastermajor_values`. |
| D3D11 debug layer | `ID3D11InfoQueue` → `stage/d3d11-debug.log` | Mandatory before any cbuffer/draw change — Iter-18 BSOD'd the OS (TDR) from unguarded shader reads. Generator output is logged here. |

---

## Sources

- Live source tree (HIGH): `Direct3d11_PixelShaderProgramData.cpp` (divergence ctor `:716-756`, generator `:339-513`, fallback `:605-623`), `Direct3d11_StaticShaderData.cpp` (`apply :587`, `construct :331`), `Direct3d11_ConstantBuffer.{h,cpp}`, `Direct3d11_StateCache.cpp` (`applyPreDrawState :1029`, PS-bind `:1107`), `Direct3d9_StaticShaderData.cpp` (`Pass::apply :820`), `Direct3d9_ShaderImplementationData.cpp` (`Pass::construct :224`, `apply :341`), `ShaderImplementation.{h,cpp}` (`m_exe :680`, `TextureOperation :498`, PEXE load `:2904`, VS source load `:2155`), `Direct3d9_VertexShaderConstantRegisters.h`, `Direct3d9_PixelShaderConstantRegisters.h`.
- Phase 11 artifacts (HIGH): `.planning/phases/11-d3d11-renderer-plugin/11-SUMMARY.md` (Phase 12 successor scope), `11-09.15-CODEX-CONSULT-iter44-pipeline-deepdive.md` (gap inventory + Q2 generator shape + Q5 gamma recommendation), `11-07-LEARNINGS.md` (VS-compile pipeline, BSOD/TDR safety, cache-invalidation).
- Phase 10 (HIGH): `10-SUMMARY.md` (DPVS Option α verdict — context for the deferred remeasure).
- COMPARISON (HIGH): `docs/research/phase12-baseline/COMPARISON.md` (5 gap buckets, entry order, image-independent seam finding).
- Memory (MEDIUM, point-in-time): `d3d11_cbuffer_transpose_quirk`, `d3d11_baked_rt_bgra_format`, `d3d9_compare_table_swap`, `project_rastermajor_values`, `project_phase11_minimap_never_round`, `project_phase11_close_pass_with_deferrals`.

---
*Architecture research for: asset-PS pipeline + visual-fix integration into gl11 D3D11 plugin (v2.2 Visual Parity)*
*Researched: 2026-05-27*
