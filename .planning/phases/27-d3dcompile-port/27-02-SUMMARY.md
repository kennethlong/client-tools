---
plan: 27-02
phase: 27-d3dcompile-port
status: reverted
outcome: deferred-to-x64
requirements: [HARD-05]
date: 2026-06-14
---

# 27-02 SUMMARY — D3DCompile HLSL swap: ATTEMPTED → REVERTED → DEFERRED to x64

## Outcome

The Plan 02 swap (`D3DXCompileShader` → `D3DCompile` for the 190 //hlsl VS path) was
**implemented, then reverted** after the boot smoke proved the approach is far larger in scope
than HARD-05 planning assumed. **Fix-A (the SEH guard around `D3DXCompileShader`) is retained**
and HARD-05 is treated as **satisfied-by-Fix-A**; the clean D3DCompile / D3DX-removal port is
**deferred to the x64 milestone**.

## What was tried (committed, then reverted)

1. `db7ff4f27` — adapt include handler `ID3DXInclude→ID3DInclude`, macros `D3DXMACRO→D3D_SHADER_MACRO`.
2. `09c3e7ea9` — replace the compile internals with `D3DCompile` (SEH guard retained, `vs_2_0`/`vs_1_1`,
   `ENABLE_BACKWARDS_COMPATIBILITY`).
3. Deviation fix attempt (uncommitted): ported `Direct3d11_HlslRewrite.{h,cpp}` → `Direct3d9_HlslRewrite.{h,cpp}`
   and wired it into the HLSL path (main source + include handler, asm-gated, cache serving originals to asm).

All four configs (gl05+gl07 Debug+Release) linked clean at each step (0 unresolved). The failures were
all at **runtime shader compile**, not link.

## Why it was reverted — the real scope

`d3dcompiler_47` (modern fxc) is **strict** where the 2003-era `D3DXCompileShader` was **lenient**.
gl05/gl07 on D3DCompile re-fight the entire gl11 Phase-11 shader-modernization battle:

| # | Error | Cause | gl11's solution |
|---|-------|-------|-----------------|
| 1 | `X3000 unexpected token 'point'` (vertex_shader_constants.inc) | `point`/`line`/`triangle` are reserved SM4 GS-primitive keywords; SWG uses them as identifiers | `Direct3d11_HlslRewrite` Rule A (`point`→`point_id`) |
| 2 | `X3202 location semantics cannot be specified on members` (a_lava_alpha_vs11.vsh) | engine-injected `DECLARE_textureCoordinateSets` macro puts `: register(vN)` on a struct member | gl11 builds the macro WITHOUT `: register(vN)` (Direct3d11_VertexShaderData.cpp:756) |
| 3+ | (behind #2) X4016 overlapping registers, cbuffer wrapping of globals, reauthored override shaders (tfcl/tfcsl in stage/override/), generic VS fallback | SM4 register/binding rules + per-shader incompatibilities | the bulk of Phase 11 (~14 iterations) |

Error #1 was fixed by the ported rewrite (confirmed: the `point` FATAL disappeared). Error #2 lies
behind it, and behind that is the rest of gl11's treatment — effectively re-porting Phase 11's shader
work to D3D9, with real risk of D3D9 register-layout regressions (the rewrite's documented semantic-loss
caveat). This is x64-milestone-sized work, not an in-place swap.

## Decision (Kenny, 2026-06-14)

**Revert Plan 02, keep Fix-A, defer the D3DCompile port to x64.** Rationale:
- Fix-A already works: its SEH guard catches the modern-toolchain D3DX FP fault (0xC0000090), keeps the
  produced bytecode, and rendered the Plan 01 `rasterMajor=5` baselines. HARD-05's intent (don't die on
  the FP fault) is met.
- The x64 milestone must remove/replace D3DX anyway and has a working x64 reference — the natural home
  for the full D3DCompile port.
- Matches the existing phase posture (asm path already deferred to x64; D-02-R).

## Final state

- `Direct3d9_VertexShaderData.cpp` restored to the Fix-A `D3DXCompileShader` + SEH-guard version
  (revert commit `c0f890875`; the original ":85 TODO (Fix B, deferred)" comment is back).
- `Direct3d9_HlslRewrite.{h,cpp}` removed (were never committed).
- gl05+gl07 rebuilt clean Debug+Release (0 unresolved); Fix-A DLLs re-staged.
- Kept: Plan 01 census (`27-ASM-CENSUS.md`), A/B baseline (`docs/research/phase27-baseline/`),
  tracked `d3dcompiler_47.dll`, and the (now-unused, harmless) `d3dcompiler.lib` link — all useful x64 inputs.
- `stage/client.cfg` restored to `rasterMajor=11` end-state.

## Self-Check: PASSED (revert verified)
- cpp contains `D3DXCompileShader(`; contains no `D3DCompile(` / `Direct3d9_HlslRewrite`.
- gl05+gl07 link 0 unresolved Debug+Release.
- Renderer state identical to the proven Fix-A baseline.
