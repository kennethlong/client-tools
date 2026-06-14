---
status: passed
phase: 27-d3dcompile-port
requirements: [HARD-05]
verdict: re-scoped — HARD-05 satisfied-by-Fix-A; clean D3DCompile port deferred to x64
date: 2026-06-14
method: orchestrator-recorded (re-scope, not goal-backward against the original D3DCompile criteria)
---

# Phase 27 Verification — D3DCompile Port (re-scoped to satisfied-by-Fix-A)

## Why this is not a standard goal-backward verification

Phase 27's original success criteria assumed the `D3DXCompileShader`→`D3DCompile` swap would land.
During execution it was found to require re-doing the entire gl11 Phase-11 shader-modernization battle
(x64-milestone-sized) and was deliberately **reverted and deferred to x64**, with HARD-05 satisfied by
the existing Fix-A SEH guard. Running the goal-backward verifier against the original D3DCompile criteria
would false-flag "gaps" for a deliberate re-scope, so this record documents the actual, decided outcome.

## Success criteria — disposition

1. **Asm-shader census produced first, scopes the asm path** — ✅ DONE. `27-ASM-CENSUS.md`
   (286 unique .vsh = 190 //hlsl + 96 //asm), committed in Plan 01. Asm path explicitly scoped and not dropped.
2. **D3D9 HLSL compilation runs through `D3DCompile`** — ⏸ DEFERRED to x64. Attempted (Plan 02) and reverted:
   gl05/gl07 on D3DCompile re-fight the full gl11 strictness battle (`X3000` `point` → `X3202` member
   `register(vN)` → cbuffer/X4016/override-shaders/fallback). The HLSL path remains on `D3DXCompileShader`.
3. **D3D9 visual parity held; SEH guard retained on D3DX paths** — ✅ HELD (trivially). No code-path change
   from the Plan 01 A/B baseline; the SEH guard (Fix-A) is retained on the HLSL path and the asm path —
   both still on D3DX. The Tatooine Fix-A spot remains handled by the retained guard.

## Requirement outcome

- **HARD-05: SATISFIED-BY-FIX-A.** Intent (D3D9 shader compile immune to the modern-toolchain D3DX FP
  fault, 0xC0000090) is met by the Phase-19 SEH guard (`db83b0f5c`), which catches the fault and keeps the
  produced bytecode. The clean `D3DCompile` / D3DX-removal port moves to the x64 milestone.

## Core invariant check

- Client boots to character select under `rasterMajor=5` (D3D9, via Fix-A — the state that rendered the
  Plan 01 baselines) and `rasterMajor=11` (D3D11, untouched by this phase). gl05+gl07 build clean
  Debug+Release (0 unresolved external symbols). `stage/client.cfg` at `rasterMajor=11` end-state.

## Artifacts kept as x64 inputs

`27-ASM-CENSUS.md`, `docs/research/phase27-baseline/` (5 A/B spots), tracked `d3dcompiler_47.dll`,
the (unused, harmless) `d3dcompiler.lib` link, and the ported-but-reverted `Direct3d11_HlslRewrite`
approach documented in `27-02-SUMMARY.md` + memory `project_hard05_d3dcompile_deferred_to_x64`.

## Human verification

None outstanding — the renderer is back to the proven Fix-A state. (The original Plan 02/03 boot-smoke
gates are superseded by the revert: the smoke that triggered the re-scope is itself the evidence.)
