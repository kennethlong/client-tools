# Stack Research — D3D11 Asset Pixel-Shader Pipeline & Visual-Parity Gaps

**Domain:** Legacy game-engine renderer porting (D3D9 → D3D11 shader-pipeline engineering) — SWG "whitengold" client, v2.2 Visual Parity
**Researched:** 2026-05-27
**Confidence:** HIGH (codebase-grounded; all D3D11/DXGI/HLSL API claims verified against current Microsoft docs)

> This is renderer engineering, not a greenfield stack pick. "Stack" here = the APIs, tooling,
> and techniques to bind real asset pixel shaders in the existing `gl11_d.dll` plugin and close
> the catalogued visual gaps. Everything is grounded in the actual engine source, not generic advice.

---

## The Headline Finding (changes the whole framing)

The "PEXE rejection" blocker has been framed as "we have only D3D9 bytecode; we must decompile or
re-author from scratch." **The engine asset format already carries the original shader SOURCE.**

`ShaderImplementation.cpp::ShaderImplementationPassPixelShaderProgram::load_0000()` (line 2895) reads
a `PSHP` form with **two chunks**:
- `TAG_PSRC` — the original shader **source text** (line 2900–2901: *entered and immediately exited — discarded*)
- `TAG_PEXE` — the pre-compiled D3D9 bytecode kept as `m_exe` (line 2904–2908)

The same dual-chunk structure is written by the original `ShaderBuilder` tool
(`ShaderBuilder/src/win32/Node.cpp:5843–5849` inserts both `PSRC` and `PEXE`), and
`PixelShaderProgramView.cpp` shows the source is **either**:
- hand-written **D3D9 shader assembly** (`D3DXAssembleShader`, the default path, line 172), **or**
- **HLSL** flagged by a `//hlsl <target>` directive line, compiled via `D3DXCompileShader` (line 308–328).

**Implication for the recommended path:** the realistic option is NOT "decompile bytecode." It is
**"load the discarded `PSRC` chunk and recompile it with `D3DCompile`"** — the engine already has a
working HLSL→DXBC path (the Phase 11 VS pipeline + `Direct3d11_HlslRewrite` rules A–E + the
`Direct3d11_CompileIncludeHandler`). The asset PS path becomes a near-mirror of the VS path that
already renders the world. The split (HLSL vs ASM) determines how much per-shader work each asset costs.

**Confidence:** HIGH — read directly from the engine + ShaderBuilder source. The one open question is
the HLSL-vs-ASM ratio across the live `.psh` asset population (must be measured — see Pitfall in PITFALLS.md
and the char-select proving path below).

---

## Recommended Stack

### Core Technologies (already present in the tree — this milestone wires/extends them)

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| `D3DCompile` (d3dcompiler_47) | SDK-current (Win10/11 SDK 26100) | Compile asset HLSL `PSRC` → `ps_4_0` DXBC at load | Already the engine's VS compile path; the PS path is `[[maybe_unused]]` plumbing waiting for a source feed. Lowest-risk reuse. Verified: legacy `ps_1_x`/`ps_2_x` *bytecode* and `ps_1_x` *profiles* are unsupported by `D3DCompile`, but **HLSL source** recompiles to `ps_4_0` cleanly. |
| `ID3D11Device::CreatePixelShader` | D3D11 | Create the PS object from DXBC | Rejects D3D9 bytecode (the blocker) but accepts `ps_4_0` DXBC from `D3DCompile`. Already wired in `Direct3d11_PixelShaderProgramData`. |
| `ID3D11ShaderReflection` / `D3DReflect` | d3dcompiler_47 | Read VS output signature + cbuffer layout | Already in use (`Direct3d11_VertexShaderData` reflected outputs drive the per-VS dynamic PS). Extend to drive PS-input matching and `Pass::apply` constant offsets. |
| `Direct3d11_HlslRewrite` (Rules A–E) | in-repo | Rewrite D3D9-era HLSL idioms for SM4 | Already clears the `register(cN)` overlap (Rule D cbuffer-wrap) + `#pragma def` strip (Rule E) for VS. PS source uses the same idioms → same rewriter applies. |
| `Direct3d11_ShaderCache` (`.cso` + FNV-1a hash) | in-repo | Cache compiled DXBC keyed on source+defines+rewrite-version | Avoids recompiling every `.psh` each boot. Already keyed by `D3D11_REWRITE_VERSION`; bump it when PS rules change. |
| DXGI `*_UNORM_SRGB` RTV on a `*_UNORM` back-buffer | DXGI 1.1+ | Hardware gamma at present | Microsoft-documented special exception: an sRGB RTV is allowed on a non-typeless `*_UNORM` swap-chain buffer. The hardware-correct way to match D3D9's `SetGammaRamp`, which flip-model swap chains no longer expose. |

### Supporting Libraries / APIs

| API | Purpose | When to Use |
|---------|---------|-------------|
| `D3DDisassemble` (d3dcompiler_47) | Disassemble a DXBC/D3D9 blob to text | Asset audit only — confirm what a given `.psh` actually contains (asm vs hlsl, ps version) when triaging a shader that renders wrong. Not a runtime path. |
| `ID3D11InfoQueue::AddApplicationMessage` | Route diagnostics into `stage/d3d11-debug.log` | Already the working diagnostic channel (explorer-launched smokes drop `OutputDebugString`). Use it to log PS compile failures + asset format census during char-select. |
| `clip()` in HLSL PS | Alpha-test emulation (no FFP alpha-test in D3D11) | Already used in the dynamic fallback PS (Iter-44B). Carry the `PSAlphaTest` cbuffer pattern into the asset PS path. |
| D3D11 `D3D11_DEPTH_STENCIL_DESC` stencil fields | Stencil ref/op mapping (decals, outlines, interiors) | `Pass::apply` stencil-ref upload is currently missing in D3D11; map through DSS, mirroring the D3D9 `Compare[]` table **including its intentional `C_GreaterOrEqual`↔`C_NotEqual` swap**. |

### Development / Debugging Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| **RenderDoc** (latest) | Frame capture, per-draw state, live PS source/DXBC view, texture/SRV inspection | Primary tool. Capture a char-select frame under both `gl05`(D3D9) and `gl11`(D3D11); diff the bound PS + SRV + samplers per draw. Free, excellent D3D11 support, shows the exact DXBC and reflected resources. Best first instrument for "which draw binds magenta and why." |
| **PIX on Windows** (latest) | GPU timing, deeper HW counters, shader debugging | Secondary; use when RenderDoc's shader debugger is insufficient or for DPVS-remeasure perf counters. Heavier setup than RenderDoc. |
| **`fxc.exe`** (Win SDK) | Offline compile/disassemble a `.psh` source extracted from TRE | Sanity-check that a specific asset's `PSRC` compiles to `ps_4_0` outside the engine before wiring it. `fxc /T ps_4_0 /Fc out.asm shader.psh`. Note `fxc` is in maintenance mode (Microsoft steers new work to DXC), but `ps_4_0`/SM4–5 + `D3DCompile` are fully supported and exactly what this engine targets — **do NOT introduce DXC/`dxc.exe`/DXIL** (SM6 is needless scope for an SM2-era asset set). |
| **`D3DReflect` census script** (in-engine, one-shot) | Count asset `.psh` by `PSRC`-present / asm-vs-hlsl / ps-version | Build this first (char-select scope). It converts the biggest open risk (HLSL:ASM ratio) into a measured number that scopes the rest of the milestone. |

## Recommended Path for the PEXE Blocker — Compared

The question lists four options. Grounded in the codebase, here is the comparison and the pick.

| Option | What it is | Verdict | Why |
|--------|-----------|---------|-----|
| **(b) Recompile `PSRC` HLSL → `ps_4_0` via `D3DCompile`** | Load the discarded `PSRC` chunk; for `//hlsl`-flagged sources, feed through the existing VS-style rewrite + `D3DCompile` + cache | **RECOMMENDED (primary)** | Reuses the entire working Phase 11 VS pipeline (`HlslRewrite`, `CompileIncludeHandler`, `ShaderCache`, reflection). For the HLSL subset of assets this is near-free. Highest fidelity, no per-shader hand-work. |
| **(d) Reconstruct from `.sht`/effect/material + a small FFP-stage generator** | For asset passes whose `PSRC` is D3D9 *assembly* (not HLSL), or FFP-only passes, generate an HLSL PS from the pass's TextureSampler stages + `TextureOperation` combiners | **RECOMMENDED (fallback for the ASM subset)** | D3D9 PS assembly cannot go through `D3DCompile` (asm → DXBC is not a supported path; `ps_1_x`/`ps_2_x` *profiles* are gone). But the engine already exposes the stage/sampler/combiner model that the asm shader *implements*. Generate equivalent HLSL from the structured pass data. Implement the dominant ~10–12 of 26 `TextureOperation` modes; log+fallback the rest. This is the path the Phase 11 SUMMARY already named (#5 "optional FFP stage-cascade generator"). |
| **(a) Decompile/translate D3D9 PS bytecode → DXBC** | Run `m_exe` through a bytecode→HLSL decompiler (HlslDecompiler / dx-shader-decompiler) then recompile | **REJECT** | Pointless given `PSRC` source exists. Decompilers are best-effort/WIP, produce unreviewed HLSL, and add an offline asset-rebuild dependency. Only consider for a one-off asset whose `PSRC` is missing AND whose stage model is too exotic for (d) — not expected at char-select scope. |
| **(c) Hybrid/interpreter** (runtime D3D9-asm VM emulating PS in a generic SM4 shader) | Bind a generic PS that interprets D3D9 PS tokens via a uniform-uploaded program | **REJECT** | Massive complexity, performance cliff, and a research project of its own. The asset set is small and fixed-function-ish; (b)+(d) cover it. Anti-scope. |

**Net recommendation:** a two-lane strategy — **(b) for HLSL-source passes, (d) for assembly/FFP passes** —
both emitting `ps_4_0` DXBC through the existing compile+cache+`CreatePixelShader` path. The per-VS dynamic
PS that exists today (`getOrCompilePSForVS`) is the scaffold to evolve: keep it as the universal safety-net,
but prefer the asset's real shader when `PSRC` yields a compilable PS. **The HLSL:ASM ratio (measured by the
census tool) decides how much of the milestone is lane (b) vs lane (d).**

## Adjacent Gaps — Recommended Technique per Gap

| Gap | Recommended technique | Why this fits THIS engine |
|-----|----------------------|---------------------------|
| **Gamma / sRGB (washed sky, blown-out interiors)** | Create an `*_UNORM_SRGB` **RTV** over the existing `*_UNORM` back-buffer (DXGI special exception). Keep linear texture content sampling correct. **Verify against D3D9 first** — D3D9 used `SetGammaRamp` (a post-tonemap ramp), not sRGB framebuffer encoding. | The Phase 11 SUMMARY proposed an explicit LUT post-pass encoding `pow(0.5 + contrast*(x*brightness-0.5), 1/gamma)`. That LUT **exactly reproduces D3D9's gamma-ramp semantics**, whereas an sRGB RTV reproduces *physically-correct* gamma, which D3D9 did NOT do. **Recommendation: implement the engine's gamma-ramp LUT pass to match the D3D9 baseline (the parity target), not an sRGB swap-chain.** Reserve the sRGB-RTV option only if the LUT proves visually off. The blown-out interiors are more likely the missing PS lighting/material constants (`Pass::apply`) than gamma — fix the PS pipeline first, then re-judge gamma. |
| **Half-texel fullscreen-blit seam (load screen)** | Remove the engine's baked **`-0.5` half-pixel offset** for transformed (XYZRHW) vertices on the D3D11 path. | CONFIRMED from the Iter-4 XYZRHW capture: the splash quad is submitted at pixel coords `(-0.5,-0.5)…(1023.5,767.5)` — the D3D9 half-pixel convention is *in the vertex data*. D3D11 has no half-pixel offset (SV_Position of pixel 0 is `0.5`, not `0`), so the `-0.5` now **over-corrects** → the centerline seam where two splash halves meet. Fix: in the D3D11 transformed-vertex path, add `+0.5` back (or zero the engine's offset) so texels map to pixel centers. This is the canonical D3D9→D3D10+ porting fix. Deterministic 2D canary — fix it standalone, second after the PS pipeline. |
| **Multi-stage texture sampling** | Generate PS that samples all bound stages and composites per the pass's `TextureSampler` + `TextureOperation` combiner chain (lane (d) territory). | Today the dynamic PS samples only `t0`. The engine already binds all 8 SRV stages (Iter-44E) — plumbing is correct, the PS just ignores stages 1–7. Drive the composite from the reflected/structured stage data. |
| **Square → round minimap** | Falls out of multi-stage sampling: sample the circular alpha-mask texture stage and apply it. | Confirmed same PS-gen family (`project_phase11_minimap_never_round`). Not a separate technique — re-check after multi-stage lands; do NOT claim fixed without a D3D9-vs-D3D11 screenshot diff. |
| **Particles / ribbon** | Post-PS-pipeline. Particle/swoosh use identity-W (verts already world-space); ribbon uses owner-W. Classify with draw-time logging, not transform changes. | Phase 11 SUMMARY note #4: ribbon stretch is NOT a transform bug. Defer until asset PS + `Pass::apply` constants land, then triage. |
| **DPVS D3D11 remeasure (SPEC R7)** | Re-run the occlusion measurement once geometry renders cleanly; use PIX counters. | Explicitly deferred in Phase 11 ("DPVS math is meaningless until geometry renders cleanly"). Last step of the milestone, after parity. |

## Integration Points into the Existing `gl11` Plugin

The work threads into named, existing code — not new subsystems:

1. **Feed `PSRC` to the PS path.** Change `ShaderImplementation.cpp:2900–2901` to *keep* the `PSRC` text
   (store on `ShaderImplementationPassPixelShaderProgram`, mirroring how `m_text` is kept for VS at
   line 2156–2159). This is a clientGraphics engine edit (allowed in v2; keep it gated so D3D9 ignores it).
2. **Compile in `Direct3d11_PixelShaderProgramData`.** The `compilePixelShaderFromHlsl` helper +
   `D3DCompile`/`ps_4_0`/rewrite/cache machinery already exists and is exercised by the magenta fallback —
   point it at the real `PSRC` source instead of returning NULL.
3. **`Pass::apply` constant uploads.** Mirror `Direct3d9_StaticShaderData::Pass::apply` (verified at
   `Direct3d9_StaticShaderData.cpp:820–966`): per-pass material (`VSCR_material`, 5 regs), textureFactor
   (`VSCR_/PSCR_textureFactor`, 2 regs), textureScroll (time-modulated, 1 reg), alpha-test ref, stencil ref,
   fog mode, fullAmbient. Iter-45 already fixed the PS slot-2 dead-write prerequisite; the per-pass uploads
   themselves are TODO in `Direct3d11_StaticShaderData::apply`.
4. **cbuffer matrices stay transposed** (`d3d11_cbuffer_transpose_quirk`) and any **persistent RT stays BGRA8**
   (`d3d11_baked_rt_bgra_format`) — invariants any new PS-pipeline RT must honor.
5. **`Compare[]` swap** — any new alpha/depth/stencil compare translation mirrors the D3D9 table's actual
   (swapped) mapping, not the enum comments (`d3d9_compare_table_swap`).

## What NOT to Use / Attempt (Anti-Scope)

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| **DXC / `dxc.exe` / SM6 / DXIL** | These assets are SM2-era; SM6 is a toolchain migration with zero parity benefit | `D3DCompile` → `ps_4_0` (already in the tree) |
| **Decompiling `m_exe` D3D9 bytecode** | The `PSRC` source already exists in the asset | Load `PSRC`; recompile (lane b) or generate from stage model (lane d) |
| **A runtime D3D9-asm interpreter PS** | Enormous complexity + perf cliff for a small fixed-function-ish set | Lanes (b)+(d) |
| **Re-authoring/rebuilding the `.tre` shader assets offline** | Adds an asset-pipeline dependency + redistribution concerns; the user runs a retail install | Compile from `PSRC` at load time, in-process |
| **sRGB swap-chain as the gamma fix (as the primary)** | D3D9 used a gamma *ramp*, not sRGB framebuffer encoding → sRGB would NOT match the parity baseline | Engine gamma-ramp LUT post-pass; sRGB-RTV only as a fallback if the LUT looks wrong |
| **All 26 `TextureOperation` combiner modes up front** | Most never appear in live assets (BUMPENVMAP/DOTPRODUCT3/MULTIPLYADD are rare) | Implement the dominant ~10–12; log+fallback the rest until an asset surfaces them |
| **DPVS remeasure before parity** | Occlusion math is meaningless on broken geometry | Defer to the last phase |
| **"Fixing" the `Compare[]` swap** | Assets authored against the swap for years; fixing breaks content | Mirror the swap intentionally |

## The Char-Select-First Proving Path (concrete)

The first vertical slice is deterministic and isolated. Recommended order:

1. **Census tool** — one-shot `D3DReflect`/`D3DDisassemble` pass over every `.psh` loaded during a char-select
   session: count `PSRC` present, asm-vs-hlsl, ps-version. Logs via `ID3D11InfoQueue`. **Output: the HLSL:ASM
   ratio** that scopes lanes (b)/(d).
2. **Lane (b) on char-select textures** — keep `PSRC` for HLSL passes; recompile to `ps_4_0`; bind real PS.
   Char-select character textures + skin should go from untextured/magenta to correct.
3. **`Pass::apply` constants** — wire material/textureFactor/scroll so lit surfaces shade correctly.
4. **Eyes** (the named char-select target) — likely a multi-stage/`sul_eye.sht` sampling case (lane d
   multi-stage). Validate eyes render in the correct place with the right texture.
5. **Half-texel seam** — standalone 2D canary; verify the load screen has no centerline seam under D3D11.
6. **Then extend outward** to open-world surfaces, minimap, particles/ribbon, and finally DPVS remeasure.

Each step gated by a **D3D9-vs-D3D11 screenshot diff** at the same char-select pose (the Phase 11 over-claim
lesson: never claim a visual win without the diff).

## Version Compatibility / Invariants

| Item | Constraint | Notes |
|------|-----------|-------|
| `D3DCompile` profile | `ps_4_0` (not `ps_5_0`, not `*_level_9_x`) | `ps_5_0` reserves D3D9-era HLSL identifiers as keywords; `level_9_x` re-imposes the SM2 256-instruction cap that already broke a VS (X5615). Mirror the VS choice exactly. |
| Compile flags | `D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY` + `PACK_MATRIX_ROW_MAJOR`, **no** `ENABLE_STRICTNESS` | Strictness is mutually exclusive with backwards-compat (X3116). Already settled for VS; PS mirrors it. |
| cbuffer matrices | Must be `XMMatrixTranspose`'d on upload | Col-vec engine vs row-vec bytecode dual (`d3d11_cbuffer_transpose_quirk`). |
| Persistent render targets | BGRA8 (`B8G8R8A8_UNORM`), not BGRX8 | `CopySubresourceRegion` strict typeless-family match (`d3d11_baked_rt_bgra_format`). |
| `rasterMajor` | `11` selects `gl11_d.dll` | Engine multiplexes `Gl_api`; D3D9 (`=5`) stays the known-good reference (invariant D-05). |

## Sources

- Engine source (HIGH — read directly): `ShaderImplementation.cpp:2895–2911` (PSRC discarded / PEXE kept),
  `ShaderImplementation.h:626–682` (PixelShaderProgram + `m_exe`), `Direct3d9_StaticShaderData.cpp:820–966`
  (working `Pass::apply` constant uploads — the parity reference), `Direct3d9_PixelShaderProgramData.cpp:34`
  (`CreatePixelShader(m_exe)` D3D9 path), `Direct3d11_PixelShaderProgramData.cpp` (current NULL-PS handling +
  `D3DCompile`/`ps_4_0` machinery + per-VS dynamic generator), `ShaderBuilder/.../PixelShaderProgramView.cpp:172,308`
  (asset built via `D3DXAssembleShader` for asm + `D3DXCompileShader` for `//hlsl`), `Node.cpp:5843–5849`
  (PSRC+PEXE both written).
- Memory files (HIGH — hard-won renderer facts): `d3d11_cbuffer_transpose_quirk`, `d3d11_baked_rt_bgra_format`,
  `d3d9_compare_table_swap`, `project_phase11_close_pass_with_deferrals`, `project_phase11_minimap_never_round`,
  `project_phase12_visual_baseline`. Phase 11 docs: `11-SUMMARY.md`, the XYZRHW CODEX consult (half-texel seam evidence).
- [Specifying Compiler Targets — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/specifying-compiler-targets) (HIGH) — `D3DCompile` drops legacy `ps_1_x`; `*_4_0_level_9_x` backward-compat profiles.
- [HLSL, FXC, and D3DCompile — Chuck Walbourn](https://walbourn.github.io/hlsl-fxc-and-d3dcompile/) (MEDIUM) — `fxc`/`D3DCompile` status, SM4/5 support, fxc maintenance posture.
- [Converting data for the color space — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/converting-data-color-space) (HIGH) — sRGB RTV special exception on `*_UNORM` swap-chain buffers.
- [Directly Mapping Texels to Pixels (D3D9) — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d9/directly-mapping-texels-to-pixels) + [Solving DX9 Half-Pixel Offset — Aras Pranckevičius](https://aras-p.info/blog/2016/04/08/solving-dx9-half-pixel-offset/) + [Half-Pixel Offset in DirectX 11 — Adam Sawicki](https://asawicki.info/news_1516_half-pixel_offset_in_directx_11) (HIGH/MEDIUM) — D3D9 `-0.5` rule removed in D3D10+; the fullscreen-blit fix.
- [HlslDecompiler](https://github.com/AndresTraks/HlslDecompiler) / [dx-shader-decompiler](https://github.com/aizvorski/dx-shader-decompiler) (LOW — listed only to substantiate the REJECT of option (a)).

---
*Stack research for: D3D11 asset pixel-shader pipeline + visual-parity gaps (v2.2)*
*Researched: 2026-05-27*
