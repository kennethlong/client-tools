# Phase 27: D3DCompile Port — Discussion Log

**Date:** 2026-06-13
**Mode:** discuss (default)

> Human-reference record of the discussion. Decisions are canonical in `27-CONTEXT.md`.

## Carried forward (not re-asked)
- Census-first; `D3DCompile` is HLSL-only; keep Fix-A SEH guard until the asm path is off D3DX
  (from STATE.md HARD-05 decision).

## Scout findings presented
- Live D3D9 compile = `Direct3d9_VertexShaderData.cpp`: HLSL `D3DXCompileShader` (Fix-A SEH-guarded)
  + asm `D3DXAssembleShader`. Explicit `// TODO (Fix B): port to D3DCompile` in the code.
- `D3DCompile` already used by the D3D11 plugin (`Direct3d11_CompileIncludeHandler`, `_ShaderCache`).
- Fix-A = commit `db83b0f5c` (modern-toolchain FP fault on bump/dot3 VS, Tatooine).

## Areas discussed (user selected all 4)

### 1. Asm path scope (D3DXAssembleShader)
- **Options presented:** HLSL-only keep asm on D3DX (recommended) / also migrate asm / census asm first.
- **User response (pivotal reframe):** "Confused — we can get the shaders from Restoration TRE
  extracts, they are already using the non-D3DX paths, why would we not lift and shift everything."
- **Resolution:** Reframed the approach. Lift Restoration's modernized (HLSL) shaders from
  `D:\SWG Restoration\SwgRestoration_*.tre` + write our own `D3DCompile` call (mirror D3D11 plugin).
  Asm path expected to dissolve. Confirmed on-disk: `D:\SWG Restoration` = full client install with
  `gl05/06/07_r.dll` + `x64\gl05_r.dll` (binary, reference only) + `SwgRestoration_*.tre` (shaders).
  We do NOT have their plugin source → lift shaders, not code. (D-01, D-02, D-03)
- **Follow-up Q:** "what does lift-and-shift give us / what do we have access to" → user chose
  "Their TRE shaders + our own D3DCompile call" and named the location `D:\SWG Restoration`.

### 2. CORNERSNAP / door-snap boundary
- **Options:** keep probes + re-baseline only (recommended) / attempt fix+removal here / fully out.
- **User selected:** Keep probes in; re-baseline door snap only. (D-04)

### 3. Parity validation bar
- **Options:** boot smoke + Tatooine Fix-A spot check (recommended) / full screenshot-diff / boot only.
- **User selected:** Boot smoke + Tatooine Fix-A spot check. (D-05)

### 4. ShaderBuilder editor scope
- **Options:** out of scope — runtime only (recommended) / include ShaderBuilder.
- **User selected:** Out of scope — runtime gl05/gl07 path only. (D-06)

## Deferred ideas captured
- Asm-path standalone migration (only if Restoration shaders don't fully cover).
- CORNERSNAP removal + door-snap fix → future x64 port.
- x64 port (door snap + 32-bit OOM; matches Restoration).
- ShaderBuilder D3DCompile migration (non-shipping editor).
