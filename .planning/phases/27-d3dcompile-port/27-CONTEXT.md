# Phase 27: D3DCompile Port - Context

**Gathered:** 2026-06-13
**Status:** Ready for planning

<domain>
## Phase Boundary

HARD-05: get the **live D3D9 runtime shader compile (gl05/gl07)** off the modern-toolchain-faulting
`D3DXCompileShader`/`D3DXAssembleShader` path and onto `D3DCompile`. The chosen approach is **not** a
minimal in-place `D3DXCompileShader → D3DCompile` swap that babysits an asm path on D3DX. Instead:
**adopt SWG Restoration's already-modernized (HLSL) shader set — extracted from their TREs — and pair
it with our own `D3DCompile` call** (mirroring the D3D11 plugin's existing `D3DCompile` + include
handler). If Restoration's shaders are all HLSL, the asm `.vsh` path dissolves and D3DX is fully
retired from the runtime compile.

**In scope:** the live gl05 (D3D9 default) + gl07 (D3D9 vsps) runtime compile path in
`Direct3d9_VertexShaderData.cpp`; extracting + integrating Restoration's HLSL shaders; writing the
`D3DCompile` call; D3D9 visual parity held.

**Out of scope (this phase):** ShaderBuilder offline editor; CORNERSNAP probe removal / door-snap
fix; the x64 port; gl06 FFP (no HLSL compile); gl11 D3D11 (already on `D3DCompile`).
</domain>

<decisions>
## Implementation Decisions

> **⚠ CLOSE-OUT DECISION 2026-06-14 (post-execution — supersedes D-01-R/D-03/D-05 below for THIS phase).**
> The D-01-R in-place `D3DCompile` swap was implemented (Plan 02) and **REVERTED** after the boot smoke.
> Modern `d3dcompiler_47` is strict where 2003 `D3DXCompileShader` is lenient, so gl05/gl07 re-fight the
> entire gl11 Phase-11 shader-modernization battle: `X3000` reserved-keyword `point` (fixable via a ported
> `Direct3d11_HlslRewrite` Rule A) → `X3202` struct-member `register(vN)` from the engine-injected
> `DECLARE_textureCoordinateSets` macro (gl11 omits it) → behind it cbuffer wrapping, X4016 register
> management, reauthored override shaders (stage/override/), and a VS fallback. That is x64-milestone-sized.
> - **D-07 (close-out, Kenny 2026-06-14):** KEEP the proven **Fix-A SEH guard** on `D3DXCompileShader`
>   (it catches the 0xC0000090 FP fault and rendered the Plan 01 baselines). Mark **HARD-05 satisfied-by-Fix-A**.
>   **Defer** the clean `D3DCompile` / D3DX-removal port to the **x64 milestone** (D3DX removal unavoidable
>   there + a working x64 reference exists). Revert commit `c0f890875`; finding in `27-02-SUMMARY.md`; memory
>   `project_hard05_d3dcompile_deferred_to_x64`. Both the HLSL and asm VS paths now stay on D3DX with the guard.
> - **D-06 unchanged:** gl06 (FFP) was never in scope — the entire VS compile branch is `#ifdef VSPS`-excluded
>   for gl06, so it has no `D3DXCompileShader` call and no FP fault to fix.
> - **Kept as x64 inputs:** `27-ASM-CENSUS.md`, `docs/research/phase27-baseline/`, tracked `d3dcompiler_47.dll`,
>   the (unused, harmless) `d3dcompiler.lib` link, and the ported-but-reverted rewrite approach (documented).
>
> The D-01-R/D-02-R/D-03/D-05 text below is retained for history; for THIS phase D-07 governs.

> **⚠ RE-DECISION 2026-06-14 (post-research — supersedes D-01/D-02 below).** Phase-27 research
> (`27-RESEARCH.md`) FALSIFIED the D-01 premise by direct tool extraction: Restoration's
> `SwgRestoration_*.tre` `.vsh`/`.psh` are **binary/encrypted, NOT HLSL source** (header `AD F0 C2 26`,
> ~42% printable, 0 of 275 contain `//hlsl`/`//asm`, 531 use a non-zlib compressor). There is nothing
> to lift. Meanwhile OUR own shaders are already clean ASCII HLSL/asm in the SWGSource TREs (census:
> **286 unique `.vsh` = 190 `//hlsl` + 96 `//asm`**). Kenny re-decided (AskUserQuestion 2026-06-14):
> - **D-01-R (core approach):** Do the **in-place `D3DXCompileShader → D3DCompile` swap on our own
>   HLSL** in `Direct3d9_VertexShaderData.cpp` (`vs_2_0`/`vs_3_0`, `ENABLE_BACKWARDS_COMPATIBILITY`,
>   adapt the existing `ID3DXInclude` handler → `ID3DInclude`, stage `d3dcompiler_47.dll`). This is the
>   approach the original D-01 explicitly rejected — it is now the chosen path because the lift premise
>   is impossible and our text is already clean. Do NOT lift Restoration shaders. Do NOT port gl11's
>   `Direct3d11_HlslRewrite` rules (those bridge SM2→SM4; we recompile to the SAME SM2/SM3 profile).
> - **D-02-R (asm path) — REVISED 2026-06-14:** The asm path does NOT dissolve (96 `//asm` shaders:
>   sabers/decals/detail). **This phase ports the HLSL path ONLY.** The 96 `//asm` shaders STAY on the
>   proven `D3DXAssembleShader` + SEH-guard path — this satisfies HARD-05's "SEH guard retained for any
>   path still on D3DX". The asm→`D3DAssemble` port (the LOW-confidence SWG-asm-dialect item,
>   RESEARCH A3) and **complete D3DX removal for D9** are DEFERRED to the **x64 conversion milestone**
>   (where `d3dx9.lib` removal becomes tractable alongside the 4 other D3DX uses). Rationale: the FP
>   crash HARD-05 targets is the HLSL `D3DXCompileShader` path; `d3dx9.lib` must stay linked this phase
>   regardless (4 other gl05/gl07 files use D3DX), so porting the asm compile now buys no lib removal
>   and only adds dialect risk. (Kenny, AskUserQuestion 2026-06-14, superseding the earlier
>   "port asm to D3DAssemble" choice.)
> - **Cache:** No bytecode disk cache this phase (out of HARD-05 scope; compile-per-launch is current
>   acceptable behaviour).
> - **`d3dx9.lib` STAYS LINKED.** PATTERNS.md verified D3DX is used in 4 OTHER gl05/gl07 files beyond
>   the compile surface (`D3DXMatrixMultiply`, `D3DXLoadSurfaceFromSurface`, `D3DXSaveSurfaceToFile`,
>   `D3DXMesh`, `D3DXMATRIX`). The port retires D3DX from the runtime *compile* surface only; do NOT
>   drop `d3dx9.lib`. ADD `d3dcompiler.lib`. (/FORCE false-pass guard still applies: link must show 0
>   `unresolved external symbol`.)
>
> The D-01/D-02 text below is retained for history. Where it conflicts with D-01-R/D-02-R, the
> re-decision wins.

### Port approach — lift Restoration shaders, write our own D3DCompile call [SUPERSEDED by D-01-R]
- **D-01:** The port is **Restoration's TRE shaders + our own `D3DCompile` call**, NOT a 1:1
  `D3DXCompileShader → D3DCompile` swap and NOT lifting Restoration's plugin *source* (we have their
  `gl0X_r.dll` as binaries only). We extract Restoration's modernized (assumed HLSL / non-D3DX)
  shader assets from `D:\SWG Restoration\SwgRestoration_*.tre` and feed them to a `D3DCompile` call we
  write, mirroring the **existing** D3D11-plugin pattern (`Direct3d11_CompileIncludeHandler` +
  `Direct3d11_ShaderCache`).
- **D-02 (asm path dissolves — VERIFY FIRST):** The premise is that Restoration's shaders are HLSL,
  so `D3DCompile` covers 100% and the asm `D3DXAssembleShader` path
  (`Direct3d9_VertexShaderData.cpp:567`) is **retired**. The researcher's **first job** is to extract
  Restoration's shaders and confirm format + coverage vs our current shader set (how many were asm
  `.vsh`, do the HLSL replacements render identically). FALLBACK: if any shaders cannot be replaced
  by a Restoration HLSL equivalent, keep just those on `D3DXAssembleShader` + the SEH guard — but the
  goal is full D3DX retirement from the runtime compile.
- **D-03 (SEH guard):** `D3DCompile` is immune to the modern-toolchain post-compile FP fault that
  Fix-A (`db83b0f5c`) guards, so the SEH wrapper (`compileVertexShaderFpGuarded`) can be **relaxed /
  removed on the HLSL path** once it is on `D3DCompile` — but retain it through validation and drop it
  from the HLSL path only after the Tatooine bump/dot3 spot (Fix-A, `db83b0f5c`) renders clean under
  `D3DCompile` (D-05). **The asm path KEEPS the SEH guard** this phase (it stays on
  `D3DXAssembleShader`, D-02-R revised).

### CORNERSNAP / door snap boundary
- **D-04:** Phase 27 is the D3DCompile port ONLY. The CORNERSNAP `_DEBUG` probes
  (`CollisionResolve.cpp`, `CellProperty.cpp`) stay **untouched**. Re-baseline the door snap on the
  new D3DCompile build as an **observation only** (the port is expected NOT to fix the 32-bit
  build/codegen door transient). Probe removal + any door-snap fix are deferred to a future **x64
  port** (not yet scheduled; x64 also addresses the chronic 32-bit OOM and matches Restoration's
  x64 build).

### Validation bar
- **D-05:** Parity = **dual-renderer boot smoke** (rasterMajor=5 D3D9 AND =11 D3D11) to
  char-select / in-game, **plus a targeted check on the exact Tatooine bump/dot3 vertex-shader spot
  Fix-A was created for** (`db83b0f5c` — confirm it renders clean post-port). Evaluate whether the
  SEH guard can be dropped on the now-`D3DCompile` HLSL path. Boot invariant applies: Debug exe reads
  `client_d.cfg`, Release exe reads `client.cfg` (Claude owns the rasterMajor toggle on switches).

### ShaderBuilder editor
- **D-06:** OUT of scope. Leave the ShaderBuilder Qt editor's `D3DXCompileShader`/`D3DXAssembleShader`
  calls untouched (pre-broken editor, not a shipping build target — consistent with prior phases).
  Port only the live gl05/gl07 runtime.

### Claude's Discretion
- Exact `D3DCompile` wiring (d3dcompiler version, include-handler reuse, shader cache integration)
  is for research/planning to mirror from the D3D11 plugin.
</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase definition
- `.planning/ROADMAP.md` — Phase 27 "D3DCompile Port" entry
- `.planning/REQUIREMENTS.md` — HARD-05 acceptance text (line 16)
- `.planning/STATE.md` — HARD-05 census-first decision; boot invariant; /FORCE false-pass guard

### Port target (the live D3D9 runtime compile)
- `src/engine/client/application/Direct3d9/src/win32/Direct3d9_VertexShaderData.cpp` — HLSL compile
  via `compileVertexShaderFpGuarded`→`D3DXCompileShader` (helper ~:87-110, call site :477) AND asm
  `D3DXAssembleShader` (:567); the Fix-A SEH guard + `_clearfp`/`_fpreset` live here. **The TODO at
  ~:85 explicitly names this port: "Fix B, deferred: port D3D9 shader compile to D3DCompile".**

### Reference implementation to mirror (already on D3DCompile)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_CompileIncludeHandler.{cpp,h}` — the
  `D3DCompile` include handler pattern
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_ShaderCache.cpp` — `D3DCompile` +
  caching wiring
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_Device.cpp` / `Direct3d11.cpp` — how the D3D11 plugin invokes `D3DCompile`

### Shader source to lift (Restoration)
- `D:\SWG Restoration\` — Restoration client install. **Shaders to extract:** `SwgRestoration_*.tre`
  (+ `SwgRestoration.toc`). **Reference plugins (binary only — behavior reference, NOT source to
  lift):** `gl05_r.dll`, `gl06_r.dll`, `gl07_r.dll`, and `x64\gl05_r.dll` (Restoration runs x64).
  NOTE: `D:\SWGEmu-Client\SWGEmu\` is a *different*, retail-era install — Restoration is the lift
  source, not SWGEmu.

### History / context
- Commit `db83b0f5c` — Fix-A SEH guard (the modern-toolchain `D3DXCompileShader` FP fault this port
  supersedes); memory `project_d3d9_d3dxcompileshader_fp_crash`.
- Shader extract/repack + override workflow: swg-blender-plugin TRE tools; loose
  `stage/override/vertex_program/` + `client.cfg` `searchPath_00_10` (memory
  `project_d3d11_world_view_anomalies_phase19` — VS-fallback + shader override workflow).
</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- **D3D11 plugin's `D3DCompile` path** (`Direct3d11_CompileIncludeHandler` + `Direct3d11_ShaderCache`)
  — a working, in-repo `D3DCompile` + include-handler + cache implementation to mirror for the D3D9
  port. This is the single biggest accelerator; the port is largely "do what gl11 does, in gl05/gl07."
- **Shader override/extract workflow** — established TRE extract/repack tooling + the loose
  `stage/override/` + `searchPath` mechanism for dropping reauthored shaders in (Phase 19).

### Established Patterns
- gl05/gl07 **statically compile D3DX from source**; built with the modern MSVC toolchain, D3DX
  unmasks FP exceptions and faults post-compile on certain bump/dot3 shaders (Fix-A territory).
  `D3DCompile` (via `d3dcompiler_*.dll`) sidesteps the whole static-D3DX problem.
- The plugin keeps the compiled bytecode even when the FP fault fires (Fix-A); a clean `D3DCompile`
  removes the need for that workaround on the HLSL path.

### Integration Points
- `Direct3d9_VertexShaderData.cpp` is the single runtime entry point for both HLSL compile and asm
  assemble — the focused surface for the port.
- Restoration's extracted shaders enter via the override/searchPath workflow (or repacked into a TRE).
</code_context>

<specifics>
## Specific Ideas

- Kenny's framing (locked): "we can get the shaders from Restoration TRE extracts, they are already
  using the non-D3DX paths, why would we not lift and shift everything." → adopt Restoration's
  modernized shaders rather than hand-converting our asm set; write our own `D3DCompile` call.
- Restoration uses the **identical gl05/gl06/gl07 plugin architecture** as this client and ships an
  **x64** build — corroborates the deferred-x64 direction for the door snap + 32-bit OOM.
- Lift source is **`D:\SWG Restoration`** (NOT `D:\SWGEmu-Client`, which is a separate retail-era install).
</specifics>

<deferred>
## Deferred Ideas

- **Asm-path migration as a standalone effort** — only needed if Restoration's shaders don't fully
  cover our asm set; otherwise it dissolves. (Fallback: keep residual asm on D3DX + SEH guard.)
- **CORNERSNAP probe removal + door-snap fix** — deferred to a future x64 port (not scheduled). The
  D3DCompile port is expected not to fix the 32-bit-codegen door transient.
- **x64 port** — separate future milestone; addresses the door snap + chronic 32-bit OOM; matches
  Restoration's shipping x64 build.
- **ShaderBuilder editor `D3DCompile` migration** — not planned (pre-broken Qt editor, non-shipping).

</deferred>

---

*Phase: 27-d3dcompile-port*
*Context gathered: 2026-06-13*
