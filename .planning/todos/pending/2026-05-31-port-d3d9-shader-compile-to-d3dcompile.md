---
created: 2026-05-31
title: Port D3D9 shader compile from D3DXCompileShader to D3DCompile (Fix B — supersede the Phase-19 SEH guard)
area: client graphics / Direct3d9 plugin / shader compilation toolchain
next_action: replace D3DXCompileShader in Direct3d9_VertexShaderData.cpp (+ any D3DXCompilePixelShader sites) with D3DCompile, porting the include handler + macro defines; then remove the SEH FP guard
files:
  - src/engine/client/application/Direct3d9/src/win32/Direct3d9_VertexShaderData.cpp  (createVertexShader -> compileVertexShaderFpGuarded -> D3DXCompileShader; the SEH guard to remove)
  - src/engine/client/application/Direct3d9/src/win32/Direct3d9_PixelShaderData.cpp  (if it also uses D3DXCompile*)
  - reference: src/engine/client/application/Direct3d11/src/win32/Direct3d11_VertexShaderData.cpp  (already uses D3DCompile; PATTERNS line 520 notes the D3DX-era FPU bug doesn't apply to D3DCompile)
references:
  - Phase 19 diagnosis (2026-05-31): the statically-compiled-from-source D3DX, built with the modern
    MSVC toolchain, trips an FP invalid-op/divide-by-zero (0xC0000090) during its post-compile FP
    cleanup on certain bump/dot3 vertex shaders. Confirmed by control test: pristine SWG Source
    VS2003 client renders the same shader/asset/spot clean; ours crashes. Same unfixed code exists
    in client-tools-koogie (PlatformToolset v145). Fix A (SEH guard) shipped as the immediate fix.
  - memory: D3D9 crash diagnosis (modern-toolchain compiled-D3DX FP fault)
status: deferred_cleanup
priority: low (Fix A — the SEH guard — already stops the crash and keeps the compiled bytecode; this
  is the principled long-term replacement that removes the D3DX dependency on the compile path)
---

## What this is

The D3D9 vertex-shader compile path uses the legacy **`D3DXCompileShader`** (D3DX statically
compiled from source into gl05/gl07). Built with the modern MSVC toolchain, that D3DX unmasks FP
exceptions and faults (0xC0000090) during its post-compile FP cleanup on certain shaders. **Fix A
(Phase 19)** wraps the call in SEH (`compileVertexShaderFpGuarded`), catches the FP fault, resets
the FPU, and keeps the already-produced bytecode — a contained, correct band-aid.

**Fix B (this todo)** is the principled replacement: use **`D3DCompile`** (the modern d3dcompiler
API) instead of `D3DXCompileShader`, exactly as the **D3D11 path already does**
(`Direct3d11_VertexShaderData.cpp`). `D3DCompile` does not have the D3DX-era FPU bug (PATTERNS line
520), so the SEH guard can then be removed and the D3DX dependency dropped from the compile path.

## Why deferred

Fix A already stops the crash with low risk. Fix B is a larger change — porting the include
handler (`ID3DXInclude` -> `ID3DInclude`), the `D3DXMACRO` defines (-> `D3D_SHADER_MACRO`), the
profile/flags, and the output `ID3DBlob` handling — and must keep byte-for-byte shader-compile
parity. Worth doing during a graphics-toolchain cleanup pass, not under crash pressure.

## Done when

- Direct3d9 vertex (and pixel, if applicable) shader compilation uses `D3DCompile`;
- the SEH FP guard (`compileVertexShaderFpGuarded`) is removed;
- the Tatooine crash spot renders clean under gl05 and gl07;
- D3D9 shader output is unchanged vs the Fix-A build (no visual regression).
