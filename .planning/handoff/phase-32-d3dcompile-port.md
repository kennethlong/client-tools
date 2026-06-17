# Handoff — Phase 32 (D3DX → d3dcompiler_47): 32-02 LANDED; OOM deferred to x64

**Status (2026-06-17):** 32-01 GATE PASS + 32-02 (D3DCompile/HlslRewrite/X4016-fix/bytecode-cache) COMPLETE & committed. The 32-bit cantina OOM is a D3DCompile-on-32-bit limit, **DEFERRED TO x64 — do NOT revert** (Kenny's call). Next session = **x64 milestone work**, not more 32-bit chasing.
**Mode:** `/gsd:execute-phase 32` interactive-inline; worktrees OFF, sequential, on `master`.
**Last session:** 2026-06-16/17 (overnight).

---

## Resume command
Pivot to the **v3.0 x64 milestone** (Phase 33 `x64-build-platform`). The OOM dissolves once the 2 GB ceiling is gone. (32-03/32-05 below are leftover phase-32 plans, now x64-era — see PENDING.)

---

## DONE (committed)
- **32-01** D3DAssemble census/dialect GATE = PASS (`9b71aef9b`/`0228783c8`).
- **32-02** HLSL `//hlsl` `.vsh` → `D3DCompile` + `Direct3d9_HlslRewrite` (`f6ffe4570`/`405bba1c1`) + X4016 fix = Rule-E pragma-strip (`4877f5a94`). Renders correctly (Tatooine/cantina, user-verified).
- **VS bytecode cache** (this session) — process-global `ms_byteCodeCache` in `Direct3d9_VertexShaderData.cpp` (gl11 `Direct3d11_ShaderCache` pattern). Fixes the re-entry recompile leak; validated on Release (compiles plateau at 59, hits climb, no key recompiled). KEEP on x64 (avoids wasteful per-cell recompiles).
- **Minidump-reserve fix** (this session) — `DebugHelp.cpp`: emergency `VirtualAlloc` reserve freed in `writeMiniDump` so OOM crashes write a NON-zero `.mdmp` (was 0 bytes). Validated twice.
- **AllTargets leak fix** (this session) — `SwgCuiAllTargets.cpp`: `removeUnusedStatusPages` reaps `SwgCuiStatusGround` mediators via `CuiMediator::garbageCollect(false)` (old leak from the initial commit; crew-validated, double-free-safe).

## The 32-bit cantina OOM — why deferred, not reverted
`D3DCompile`'s per-compile heap fragmentation (~2.2 MB/compile, which static `D3DXCompileShader` never produced) bloats even the FIRST cantina load; on the heavy 32-bit Debug build that exceeds 2 GB at ~59s on first entry. The cache fixes RE-entry recompiles but not first-load misses. Reverting to `D3DXCompileShader` would fix the symptom but undo the deliverable + re-add the FP-crash dependency on Fix-A. **x64 removes the 2 GB ceiling → symptom gone.** Full diagnosis (4-way crew, runtime A/B, the de-anchored round that found it) in memory `project_phase32_d3dcompile_recompile_leak`; crew artifacts in `.planning/research/CONSULT-32-*`.

## PENDING (x64-era)
- **32-03** asm `.vsh` → `D3DAssemble` (PASS branch per 32-01; 32-04 skipped). Asm has no recompile-fragmentation leak (D3DXAssembleShader flat in the A/B) — safe on 32-bit or x64.
- **32-05** closeout: remove the Fix-A SEH guard once BOTH paths are off D3DX.
- **NEXT MILESTONE:** v3.0 x64 (Phase 33 `x64-build-platform`).
