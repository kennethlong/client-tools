---
plan: 27-03
phase: 27-d3dcompile-port
status: moot
outcome: deferred-to-x64
requirements: [HARD-05]
date: 2026-06-14
---

# 27-03 SUMMARY — Validate/finalize the HLSL-only port: MOOT (port reverted) → DEFERRED to x64

## Outcome

Plan 03's job was to validate and finalize the Plan 02 D3DCompile port (verify the asm branch stayed
on `D3DXAssembleShader`, run the dual-renderer parity verdict, and finalize the HLSL-path SEH-guard
disposition). **Plan 02 was reverted** (see `27-02-SUMMARY.md`) after the boot smoke proved the
D3DCompile swap re-fights the full gl11 shader-modernization battle and is x64-milestone-sized. With no
port to validate, Plan 03 is **moot** and **deferred to x64** alongside the port itself.

## Disposition of Plan 03's three tasks

1. **Verify asm branch untouched on D3DXAssembleShader** — TRUE by construction: the revert restored
   `Direct3d9_VertexShaderData.cpp` to the Fix-A state, so the asm branch is the original
   `D3DXAssembleShader` call (it was never ported; D-02-R). No `D3DAssemble` anywhere.
2. **Dual-renderer parity verdict + Tatooine Fix-A spot** — N/A: nothing changed from the Plan 01 A/B
   baseline (the engine still uses `D3DXCompileShader` + Fix-A, exactly as when the baselines were
   captured). `rasterMajor=5` renders via Fix-A; `rasterMajor=11` (D3D11) was never in scope for this phase.
   The Tatooine Fix-A spot remains handled by the retained SEH guard, as before this phase.
3. **Finalize HLSL-path SEH guard** — KEPT: the HLSL path stays on `D3DXCompileShader` with the Phase-19
   SEH guard retained (the ":85 TODO (Fix B, deferred)" comment is back in place). The guard is NOT
   removed — Fix-B did not land. This satisfies HARD-05's "SEH guard retained for any path still on D3DX"
   (now BOTH the HLSL and asm paths are on D3DX with the guard).

## Final renderer state (HARD-05 satisfied-by-Fix-A)

- HLSL VS path: `D3DXCompileShader` + Phase-19 SEH guard (Fix-A) — retained.
- asm VS path: `D3DXAssembleShader` + guard — unchanged (D-02-R).
- gl05+gl07 build clean Debug+Release (0 unresolved); client boots `rasterMajor=5` to char-select +
  world via Fix-A (the same state that produced the Plan 01 baselines); `rasterMajor=11` end-state restored.
- Clean `D3DXCompileShader`→`D3DCompile` + D3DX-removal port deferred to the x64 milestone.

## Self-Check: PASSED (re-scope verified)
- asm branch on `D3DXAssembleShader`, HLSL branch on `D3DXCompileShader` + SEH guard, no `D3DCompile`/`D3DAssemble`.
- HARD-05 marked satisfied-by-Fix-A in REQUIREMENTS.md; clean port deferred to x64.
