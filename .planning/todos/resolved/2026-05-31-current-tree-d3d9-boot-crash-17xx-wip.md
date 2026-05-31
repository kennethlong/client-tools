---
created: 2026-05-31
title: Current tree (swg-blender-m16, 17-01..17-07 WIP) crashes the D3D9 boot path — c0000005 in setPixelShaderConstants
area: client graphics / D3D9 (gl05) light+pixel-shader-constant path / shared clientGraphics 17-xx shader-pipeline WIP
next_action: diagnose why setPixelShaderConstants -> d3d9 SetPixelShaderConstantF calls through a null pointer on the first post-process frame in a CURRENT-tree build (clean rebuild reproduces; 05-27 pre-17 build is fine)
files:
  - src/engine/client/application/Direct3d9/src/win32/Direct3d9_StateCache.cpp  (setPixelShaderConstants — the call that faults into d3d9)
  - src/engine/client/application/Direct3d9/src/win32/Direct3d9_LightManager.cpp  (applyLights_vertexShader -> setPixelShaderConstants(PSCR_dot3LightDirection,...,5))
  - src/engine/client/library/clientGraphics/src/shared/PostProcessingEffectsManager.cpp  (postSceneRender -> drawTriangleFan, first-frame trigger)
  - src/engine/client/library/clientGraphics/src/shared/ShaderImplementation.h / .cpp  (17-01 added members; shared ABI both renderers use)
  - 17-xx commits: 3667b3b06/5eea33474 (17-01), 04ef66976 (17-04.X), cafbe6111/97d7bbf93 (17-06), 07f56a1cb/f9e5ac569/d8cc1ca99 (17-07)
references:
  - minidump faulting stack (cdb .ecxr): eip=0 (call through null) <- d3d9 <- gl05_r!Direct3d9_StateCache::setPixelShaderConstants(index=0,numberOfConstants=5) <- Direct3d9_LightManager::applyLights_vertexShader <- selectLights <- Direct3d9::drawPrimitive/drawTriangleFan <- PostProcessingEffectsManager::postSceneRender, MainLoop 1 (first frame, pre-login)
  - none of the crash-path .cpp changed since pre-17 -> regression is via shared 17-xx clientGraphics changes (ShaderImplementation/StaticShaderData), not a direct edit
  - 05-27 pre-17 Release binaries (binary-backups/pre-p19-nanhunt-20260531/) boot D3D9 clean; the current-tree CLEAN rebuild crashes -> genuine source regression, not build staleness
  - CONTROL (2026-05-31): the pristine SWG Source VS2003 client (D:/Code/SWGSource Client v3.0/SwgClient_r.exe, rasterMajor=5/gl05) BOOTED CLEANLY and rendered the crash spot on the SAME machine/server/assets/character. So neither this boot crash NOR the D3DXCompileShader FP crash is environmental (driver/Windows/assets) — both are our-build-side. The pristine client is a fully-working D3D9 reference on this setup.
  - memory project_d3d9_d3dxcompileshader_fp_crash (separate bug; this blocks runtime-verifying its Fix A on a current build)
status: resolved
priority: high (blocks a shippable D3D9 client from the current tree, and blocks runtime-verifying the D3D9 D3DXCompileShader Fix A; the D3D9 default renderer is the reference path — must boot)
resolved: 2026-05-31
---

## RESOLUTION (2026-05-31)

**Was a swg-blender-m16 partial/stale-17-07 artifact — ABSENT on koogie's complete-17 tree.** A full Release D3D9 stack built from the koogie worktree (`koogie-msvc-cpp20-base`, which carries the COMPLETE 17-08/17-09 + `bb4b13a00` shader pipeline) — `SwgClient_r.exe` + `gl05_r.dll`, clean link, 0 unresolved externals — BOOTS clean at `rasterMajor=5` past the pre-login first frame with no crash dump. The c0000005 null-call never occurs there. The crash was specific to this branch's incomplete 17-07 WIP (same family as "swg-blender-m16 char renders BLACK"). No separate fix needed; the resolution is "do D3D9/renderer work on koogie, not swg-blender-m16." This unblocked runtime-verifying D3DXCompileShader Fix A (now VERIFIED on koogie — see handoff `.planning/handoff/d3d9-d3dx-fp-regression.md` in the koogie worktree + memory `project_d3d9_d3dxcompileshader_fp_crash`).

## What this is

A CURRENT-tree (`swg-blender-m16`, which carries 17-01 through 17-07 D3D11 shader-pipeline WIP)
**Release D3D9 build crashes on the first frame, before login**, with `c0000005` — a **call
through a null function pointer inside d3d9.dll**, reached from
`Direct3d9_StateCache::setPixelShaderConstants` (called by
`Direct3d9_LightManager::applyLights_vertexShader` during the first post-process
`drawTriangleFan`). The `ui_planet_sel_*` breadcrumb in the auto crash dump is just the
last-touched shader template, NOT the fault site (use the minidump `.ecxr` stack).

## Why it matters / scope

- The D3D9 (`rasterMajor=5` gl05) path is the **parity reference** and the **default renderer** —
  it must boot. The 05-27 pre-17 binaries boot fine; the current tree does not.
- It's a **side effect of the 17-xx shared-clientGraphics WIP** (ShaderImplementation/StaticShaderData
  changes for the D3D11/asset-PS work) breaking the D3D9 path — same family as "main checkout stuck
  at 17-07 -> gl11 char renders BLACK", now confirmed to hit D3D9 too.
- It **blocks runtime-verifying** the D3D9 `D3DXCompileShader` Fix A (commit 99a4ac5a1) on a current
  build — Fix A had to be tested on the broken current tree because the fix lives there. Workaround
  to verify Fix A in isolation: build gl05+FixA on the pre-17 base and pair with the 05-27 exe.

## Investigation start

- Clean rebuild reproduces (not build staleness). Diff the shared 17-xx changes to
  `ShaderImplementation`/`StaticShaderData` and how D3D9's `setPixelShaderConstants` /
  `applyLights_vertexShader` consume them; a struct-layout or a now-null shader/program pointer
  is the likely cause of the d3d9 null-call.
- Repro: boot `stage/SwgClient_r.exe` (current-tree Release build) at `rasterMajor=5`; crashes
  MainLoop 1. Get the stack from the minidump via `cdb -z <dump> -c ".ecxr; kp"`.
