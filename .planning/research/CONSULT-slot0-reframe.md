# Phone-a-Friend Consult — v2.2 Visual Parity, the "slot-0 / TAG_PSRC reframe"

You are an independent senior graphics/engine reviewer. We are bringing a **new D3D11 renderer plugin** (`gl11_d.dll`) to **visual parity** with a known-good **D3D9** renderer (`gl05_d.dll`) in a legacy Star Wars Galaxies client (now MSVC/C++20/MSBuild). The renderer is a swappable plugin chosen at runtime by `rasterMajor` (5 = D3D9, 11 = D3D11), behind a single 119-function `Gl_api` table. D3D11 already boots and renders **geometry** correctly; the problem is **shading/texturing**.

Our research (codebase-grounded, HIGH confidence) reached a central reframe that will become the spine of the whole milestone roadmap. **Before we lock that roadmap, we want independent validation.** Please verify the claims against the actual source tree where you can, then give your verdict and any landmines we're missing.

## Repo
`D:\Code\swg-client-v2` — the D3D11 plugin and shader abstraction live under `src/` (search for `Direct3d11`, `Gl_api`, `gl11`, and the engine shader layer in `clientGraphics`: `ShaderImplementation`, `StaticShader`, `Pass`, `ShaderCapability`, `TextureOperation`). Full synthesized research: `.planning/research/SUMMARY.md` (+ STACK/FEATURES/ARCHITECTURE/PITFALLS).

## The claims to validate

**Claim 1 — single root cause.** The reason D3D11 surfaces render untextured / magenta — and the reason character **eyes render gray**, **eyes show through the back of the head**, the **minimap renders square (not round)**, and specular/glow are missing — is ONE bug, not several: when `CreatePixelShader` rejects the shipped D3D9 PS bytecode, the plugin falls back to a **dynamically-generated PS that samples only `t0` / TEXCOORD0** and (a) never samples the SRVs the engine *already binds* on slots 1..7, (b) never evaluates the engine's multi-stage `TextureOperation` cascade, and (c) uploads no per-pass material/light/fog constants. Anchors to verify:
- `Direct3d11_PixelShaderProgramData.cpp` ~716-756 (ctor leaves `m_d3dPS = NULL` when `CreatePixelShader` rejects the D3D9 `m_exe` bytecode) and ~447-509 (the dynamic fallback PS generator: only `t.Sample`/`tex*color`/magenta).
- SRVs 1..7 are bound but unsampled (a prior iteration "Iter-44E" reportedly bind them).
- `ShaderImplementation.h` ~498 — the ~26-entry `TextureOperation` enum the real PS would evaluate.

**Claim 2 — the PEXE reframe (the big one).** We do NOT need to decompile/translate the rejected D3D9 PS bytecode. Every `.psh` asset *also* carries the **original shader source** (`TAG_PSRC`) which the engine currently **loads and immediately discards** at `ShaderImplementation.cpp` ~2904 (the VS path keeps its source `m_text` ~2155, which is why VS recompilation already works; the PS path throws its source away — that asymmetry is the whole bug). So the fix is to **keep `PSRC` and recompile it** through the already-working Phase-11 toolchain (`D3DCompile` → `ps_4_0` + the `Direct3d11_HlslRewrite` rules + `.cso ShaderCache` + `D3DReflect`), exactly mirroring how VS already works. For passes whose `PSRC` is **D3D9 assembly** (not HLSL), generate HLSL from the structured `TextureOperation` stage model instead (asm can't go through `D3DCompile`).

**Claim 3 — per-pass constants.** Beyond compiling a real PS, we must upload the per-pass constants the asset PS expects (material c11, textureFactor c44/45, textureScroll c47, fog, stencil), mirroring the working D3D9 reference at `Direct3d9_StaticShaderData.cpp` ~820-966 / `Pass::apply`. The D3D11 cbuffer slots reportedly already exist but aren't written per-pass.

**Claim 4 — two independent gaps** (schedulable in parallel, no PS dependency):
- Load-screen **half-texel seam**: D3D9 bakes the -0.5 texel offset into vertex data; D3D11 (no half-pixel rule since D3D10) over-corrects → faint centerline seam. Fix: add +0.5 back on the D3D11 transformed-vertex path / the `getOneToOneUVMapping` stub at `Direct3d11.cpp` ~995.
- **Gamma**: D3D9 applies a gamma *ramp* at scanout (`SetGammaRamp`) and samples textures raw (sRGB OFF); D3D11's gamma is a **no-op stub** at `Direct3d11.cpp` ~253. Fix = replicate the ramp as a full-screen LUT post-pass. Explicitly do NOT flip backbuffer/SRVs to `_SRGB` (that would be double-correction).

## Questions
1. **Does Claim 1 hold?** Is slot-0-only sampling in the fallback PS genuinely the single root cause of those 5+ symptoms? Anything in that list that is NOT explained by it (i.e., a second independent bug hiding in the set)?
2. **Is Claim 2 (PSRC recompile) sound and the right primary path** vs. bytecode translation? Any reason the discarded `PSRC` would be unusable (e.g., stripped at asset-build time, mostly assembly, preprocessor/include hazards)?
3. **Design call:** lead with a **generated-PS `TextureOperation` evaluator** inside gl11 (works for all passes regardless of PSRC language), OR a **two-lane** approach (recompile PSRC-HLSL where present + generate for the rest)? Is a `D3DReflect`/`PSRC`-language **census tool as the gating first task** the right call to decide the mix?
4. **Char-select-first** beachhead: is proving the pipeline on `sul_*_head.sht` + `sul_eye.sht` (textures + eyes) a sound, deterministic first vertical slice before the open world?
5. **Landmines** we haven't flagged for porting D3D9 asset PS → D3D11 in an engine like this, especially anything that could **regress the working D3D9 path** when editing shared `clientGraphics` code.

Be concrete, cite file/line where you can verify, and say plainly if you think any claim is wrong or oversimplified.
