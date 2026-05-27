# Consult Synthesis — v2.2 "slot-0 / TAG_PSRC reframe" (CODEX + Cursor, 2026-05-27)

Two independent reviewers (CODEX `ba6yxag14`, Cursor `blz28p7ts`), both reading the **live tree**, reviewed the research's central reframe. Raw outputs: `CONSULT-slot0-codex.out` (review slice lines 8454-8515), `CONSULT-slot0-cursor.out`. Prompt: `CONSULT-slot0-reframe.md`.

**This document SUPERSEDES `SUMMARY.md` where they differ on the asm-pass approach (Claim 2).**

## Verdict at a glance

| Claim | CODEX | Cursor | Net |
|-------|-------|--------|-----|
| 1 — slot-0 fallback PS is THE single root cause | "mostly right, **overstated** as single bug" | "directionally right, **oversimplified**" | **Spine = yes; literal single-bug = no** |
| 2 — keep+recompile PSRC, not decompile PEXE | sound; **census mandatory** | sound; **but TextureOp-generator-for-asm is WRONG for VSPS** | **Reframe holds; one lane corrected** |
| 3 — per-pass `Pass::apply` constants required | correct | correct | **Confirmed required, in parallel** |
| 4 — gamma + half-texel independent | correct | correct | **Confirmed independent** |

## What was VALIDATED (both, against tree)

- **PEXE rejected, `m_d3dPS` stays null** — `Direct3d11_PixelShaderProgramData.cpp:719`/:748-756 (explicit comment: "CANNOT pass to CreatePixelShader — E_INVALIDARG"). D3D9 consumes the same `m_exe` at `Direct3d9_PixelShaderProgramData.cpp:34`.
- **Fallback PS samples only `t0`/`s0`** — `Direct3d11_PixelShaderProgramData.cpp:339`/:405-472 (returns `t.Sample`, `t.Sample*COLOR0`, vertex color, or magenta at :506-509). Bound via `Direct3d11_StateCache.cpp:1107`.
- **SRVs 1..7 ARE bound (Iter-44E) but unsampled** — `Direct3d11_StaticShaderData.cpp:711-728` + the deferral comment at :601-605.
- **PSRC discarded** — `ShaderImplementation.cpp:2895-2908` (`enterChunk(TAG_PSRC); exitChunk(TAG_PSRC,true);` then reads only `TAG_PEXE`→`m_exe`). PS header has no `m_text`, only `m_exe` (`ShaderImplementation.h:626`).
- **ShaderBuilder preserves PSRC** — `Node.cpp:5818`/:5843; compiles `//hlsl` via `D3DXCompileShader` else assembles asm (`PixelShaderProgramView.cpp:308-328`/:461).
- **`compilePixelShaderFromHlsl` already exists, unused for assets** — `Direct3d11_PixelShaderProgramData.cpp:86-171`.
- **Per-pass constants deferred on D3D11** — `Direct3d11_StaticShaderData.cpp:601-602`; D3D9 reference uploads material/textureFactor/textureScroll/stencil/fog at `Direct3d9_StaticShaderData.cpp:820-955`. PerMaterialCB exists (`Direct3d11_ConstantBuffer.h:58-64`) but material fields stay zero.
- **Gamma**: D3D11 no-op stub `Direct3d11.cpp:253`; D3D9 `SetGammaRamp` at `Direct3d9.cpp:2198`. D3D9 samples sRGB OFF (`Direct3d9_StateCache.cpp:193`) → confirms "don't flip SRVs to _SRGB."
- **Half-texel**: `getOneToOneUVMapping` STUB `Direct3d11.cpp:995`; D3D9 +0.5 inset `Direct3d9.cpp:3676`.

## CORRECTIONS that change the plan (the value of the consult)

### A. The asm lane was WRONG — biggest correction (Cursor, CODEX-aligned)
The research said: for asm `PSRC`, "generate HLSL from the `TextureOperation` stage model." **This is wrong for the char-select VSPS shaders.** For VSPS passes the engine picks pixel-shader-program **OR** FFP stages, **not both** (`ShaderImplementation.cpp:1592-1602`): `sul_eye.sht`-style materials use `TAG_PPSH` with `numberOfStages == 0` — the **compositing lives in the `.psh` program (PSRC→PEXE), NOT in `Pass::m_stage` `TextureOperation` fields.** The `TextureOperation` cascade is the **FFP** model (`Direct3d9_ShaderImplementationData.cpp:306`), which D3D11 explicitly descoped (D-04a). So:
- **Primary lane:** `PSRC`(`//hlsl`) → `compilePixelShaderFromHlsl` → bind `m_d3dPS`, mirroring the VS stack.
- **Secondary lane:** asm `PSRC` → **port asm → HLSL → `D3DCompile` ps_4_0** (NOT re-assemble — `D3DXAssembleShader` reproduces the same D3D9 bytecode D3D11 rejects; "asm→PEXE rebuild" is a named landmine).
- **Tertiary/narrow:** FFP `TextureOperation` generator only for genuine FFP-only passes (no `TAG_PPSH`) — likely small after D-04a.
- A minimal multi-sampler dynamic PS is a **diagnostic bridge only**, not the parity strategy.

### B. Census is genuinely GATING — and must run on REAL assets
The HLSL:asm ratio decides how much of the milestone is cheap recompile vs asm→HLSL porting. **Neither reviewer could verify `.psh`/`.sht` contents — this checkout has no extracted assets.** The census MUST run against the actual asset tree / retail-TRE extraction, classifying `//hlsl` vs asm, target profiles, includes, sampler slots, and constants referenced — starting with char-select + the Iter-44E multi-sampler set, NOT the whole TRE day one.

### C. Claim 1 is the SPINE, not a literal single bug — separate the satellite gaps
Slot-0 fallback + missing `Pass::apply` constants explains: untextured/magenta, gray eyes (multi-stage), missing multi-texture spec/glow, likely square minimap (if radar mask is a stage/alpha pass). It does **NOT** subsume:
- **"Eyes through back of head"** — that was a **depth-state** problem, already addressed by Iter-44A per-pass `m_zEnable/m_zWrite/m_zCompare` (`Direct3d11_StaticShaderData.cpp:658-677`). Do **not** re-fold into "PS only" without a fresh A/B screenshot.
- **Interior "flat white" / specular** — PS + constants **plus** missing gamma LUT **plus** a **simplified `Direct3d11_LightManager`** (lighting is independently simplified vs D3D9).
- **Stencil** — independent state gap (decals, masks, possibly UI shapes).
- **Skeletal shard distortion** — separate single-stream skinning path.

### D. Use D3DReflect-driven constant upload, NOT D3D9 register folklore
`HlslRewrite` + `D3DCompile` reallocate registers — recompiled PS may not use c11/c44/c45/c47 where the D3D9 shader did. Drive `Set*ShaderConstants` from the **reflected** layout, not copied register indices. (Cursor ties this to proven pitfalls id=342/343.)

## Char-select beachhead — confirmed, with constraints
Sound + deterministic. Constraints both raised:
- Use `sul_eye.sht` + `sul_*_head.sht` **plus one single-stage control** (body/clothing) to separate "pipeline works" from "multi-sampler works."
- Success criterion = **screenshot diff vs `rasterMajor=5`**, NOT "no magenta."
- **Confirm single-stream vs multi-stream skinning** before blaming residual head weirdness on the PS.

## Landmines (convergent) — for planners/executors
- **Shared `clientGraphics` PSRC change:** keep D3D9 `load_0000` behavior byte-for-byte identical except storing text; careful ctor/reload/destructor ownership; store PSRC graphics-agnostically.
- **`D3DSAMP`/register stripping → reflect, don't fold registers.**
- **D3D9 Compare[] swap is intentional** (`C_GreaterOrEqual`↔`C_NotEqual`) — mirror, don't "fix."
- **Every new cbuffer matrix needs `XMMatrixTranspose`.**
- **Do NOT re-enable per-pass blend factors early** (Iter-44C amplification regression) — wait until the real PS samples all bound SRVs.
- **Full-buffer/discard-safe cbuffer writes** (partial writes leave garbage).
- **Shader-cache invalidation:** generator/rewrite/profile changes must feed the `.cso` cache key/version, or stale caches mislead debugging.
- **Boot-gate BOTH `rasterMajor=5` and `=11`** on any `ShaderImplementation.cpp` edit.

## Roadmap deltas (fed to roadmapper)
1. **Phase 17 first task = PSRC census on real assets** (gating; classify //hlsl vs asm over char-select + Iter-44E set).
2. **Phase 17 fix = PSRC-HLSL recompile (primary) + asm→HLSL port (secondary) + `Pass::apply` constants (reflect-driven)** — **drop "FFP TextureOperation generator as primary for VSPS"** (tertiary/narrow only).
3. **Keep gamma (after PS) + half-texel (parallel/early) as independent phases** — confirmed.
4. **Add explicit room for satellite gaps**: depth already mostly handled (44A) — verify by screenshot, not assume; interior lighting needs `Direct3d11_LightManager` work, not just PS; stencil is its own state-parity item; skeletal-shard phase must decide single-stream-fix vs flip-to-multistream.
