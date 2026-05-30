# Handoff: D3D11 char-select bump arms — purple/green (VS lighting NaN)

**Updated:** 2026-05-30 (~12:25). **Worktree:** `D:\Code\swg-client-v2-d3d11` (branch `koogie-msvc-cpp20-base`).
**Boot:** main stage `D:\Code\swg-client-v2\stage` (`SwgClient_d.exe` + `gl11_d.dll`); `rasterMajor=11` char-select.
**Build:** gl11-only — `& $env:MSBUILD <worktree>\src\build\win32\swg.sln /t:Direct3d11 /p:Configuration=Debug /p:Platform=Win32 /m /nodeReuse:false`. The worktree `stage` **junction** auto-deploys `gl11_d.dll` to main stage (no manual copy). Exit 0 = success (junction makes the postbuild copy work).

## ✅ RESOLVED 2026-05-30 (~13:25) — root cause was a TEXTURE-SLOT SWAP, not the VS NaN

Arms render correctly (lit + textured), confirmed in-client. The VS-lighting-NaN premise of this
handoff was **stale**: the GAP-5 commits + a new VS material c11–c15 feed had already made the VS
output finite (RenderDoc "Debug this Vertex" showed COLOR0=(0.0169…), COLOR1=0 — no NaN). The
remaining purple/green was the **diffuse and normal textures bound to swapped SRV slots**.

Root cause: legacy `sampler NAME : register(sN)` pins only the SAMPLER register; fxc assigns the SM4
TEXTURE register by FIRST-USE order when splitting `sampler2D`. specmap_bump samples `normalMap`
before `diffuseMap` → normalMap texture=t0, diffuseMap texture=t1, while samplers stay s0/s1. The
engine binds SRVs by D3D9 stage index → the diffuse SRV landed in the normalMap slot → blue normal
sampled as albedo → purple/green.

Fix = reflection-driven SRV-slot remap. Detail: memory `project_d3d11_multitexture_srv_slot_swap`.
Files: `Direct3d11_PixelShaderProgramData.{h,cpp}` (`getSrvSlotForStage` + reflection of texture/
sampler bindings), `Direct3d11_StaticShaderData.cpp` (bind loop uses the remap), `Direct3d11_StateCache.{h,cpp}`
(`setVertexMaterialColors` VS material c11–c15 feed). gl11_d.dll = 5/30 13:24. Remaining nit: minor
arm scaling (deferred — unrelated to lighting/textures). ALL CHANGES STILL UNCOMMITTED.

---

## Symptom (was UNRESOLVED — see RESOLVED block above)
Char-select **bump arm parts (sleeves/hands) render purple/green**. Everything else (face/skin, tunic, pants, shoes) renders correctly. 4 fix attempts this session did NOT change the visual.

## Mechanism (CONFIRMED via RenderDoc)
- The bump **VS Output `COLOR0` (diffuse) and `COLOR1` (specular) are NaN** for every vertex.
- In this VS, `COLOR0`/`COLOR1` come from `calculateDiffuseSpecularLighting(true, position, normal)` + the VS lighting/material cbuffer (VS b0 = `Direct3d11_VertexSlot0CB`, 1152B). They do **NOT** depend on the tangent.
- The PS has a **NaN-guard** (`any(isnan(COLOR0)) ? 0 : COLOR0`), so NaN `COLOR0` → diffuse term zeroed in the PS → only the normal-map/bump term renders → **that's the purple/green** (bare normal map).
- So the fix MUST make the VS emit finite `COLOR0`/`COLOR1`. Killing the VS-side NaN is the goal.
- First vertex-debug showed `r2.w = INF` early; `r2.w` is the point-light `distanceAttenuation` (`div r2.w, l(1.0), r2.w`). INF = `1/denom`, `denom = dot(attenuationCoeffs, factors) = 0` for unused point lights with coeffs `(0,0,0,0)`.

## Ground-truth resource (USE THIS)
The VS recompile dumps full disassembly (functions.inc INLINED, with SWG variable-name comments) to
**`stage/stage/vs-disasm-all.txt`** (note double `stage/stage/`). The bump VS is
`a_specmap_bump_vs20_for_ps20.vsh`. Read its section and list EVERY `cb0[N]` read by every
`div`/`log`/`exp`/`rsq` — those are the NaN/INF candidates. Confirmed reads:
- `mul r4.w, r4.w, cb0[15].x` = `material[4].x` = **materialSpecularPower** (specular pow `log/exp`).
- point-light attenuation `div r2.w, l(1.0), r2.w` (unrolled ×4 point + 1 pointSpecular).

## What was changed this session (all UNCOMMITTED in worktree)

### 1. Fix A — per-key VS texcoord-set remap (VERIFIED WORKING, keep). REWRITE_VERSION 15→16.
Mirrors D3D9 `Direct3d9_VertexShaderData::createVertexShader` per-`textureCoordinateSetKey` recompile.
6 files: `Direct3d11_VertexShaderData.{h,cpp}` (per-key `Variant` map, tag parse, macro emit),
`Direct3d11_StaticShaderData.{h,cpp}` (`m_passTexcoordKey` computed in construct, fed via
`setCurrentVSData(vs,key)` in apply), `Direct3d11_StateCache.{h,cpp}` (`ms_currentVSKey`, resolves
`getVariant(key)`, passes Variant to layout / key to PS methods), `Direct3d11_VertexDeclarationMap.{h,cpp}`
(`getOrCreate*` take `Variant const&`), `Direct3d11_PixelShaderProgramData.{h,cpp}` (4 methods take key).
RenderDoc CONFIRMED: bump VS now reads real DOT3 tangent at `TEXCOORD1` (`vsin TEXCOORD idx=1 mask=0xf`,
no phantom `TEXCOORD2`). `directionToLight_t`/`halfAngle_t` VS outputs are FINITE. So Fix A is correct
and needed for the per-pixel bump — it just was NOT the cause of the NaN/purple-green.

### 2. materialSpecularPower → `material[4].x` (c15.x). Disasm-proven register.
`Direct3d11_StateCache::setSpecularPower(float)` (new, public) writes `s_slot0Shadow.material[4].x` (and
`lightData[25].w` for the PS dot3 path). `StaticShaderData::apply` calls it per-pass with `pm.m_power`
(captured from `material.getSpecularPower()`), clamped 0/NaN→32 (D3D9 default). `ms_specularPower` static.

### 3. Unused point-light attenuation `k0=1` (mirror D3D9 `:685/:717`).
`composeSlot0Shadow` now sets `lightData[10/14/17/20/23].x = 1.0f` (pointSpecular + point[0..3]
attenuation.k0) so `denom>=1`, killing the `1/0=INF`. We never fill point lights, so all are "unused".

**Result: arms STILL purple/green after all 3.** Either the NaN has yet another source, the fixes aren't
reaching the bound cbuffer, or the diffuse path reads an unfed register too.

## RECOMMENDED NEXT APPROACH (systematic — stop guessing)
1. **Verify the fixes reached the GPU**: RenderDoc the bump draw's VS cbuffer — are `material[4].x`==32 and
   `lightData[10/14/17/20/23].x`==1.0 NOW? If still 0 → `composeSlot0Shadow`/`setSpecularPower` writes
   aren't reaching this draw's cbuffer (upload/ordering/wrong-cbuffer bug) — that alone would explain why
   nothing worked. (ambient/parallelSpecular DID show fed earlier, so slot0 generally reaches the VS.)
2. **Get the EXACT first NaN instruction**: RenderDoc "Debug this Vertex" on a bump arm vertex, step
   (F10/F11) INTO `calculateDiffuseSpecularLighting`, find the FIRST instruction whose output goes INF/NaN
   and which `cb0[N]` it reads. That ends the guessing.
3. **Audit material feed**: `composeSlot0Shadow` fills ambient/parallelSpecular/dot3/ext + (now) point
   k0 + material[4].x — but does NOT upload the full VS **material** struct (c11–c15: materialDiffuse/
   /specular/emissive). D3D9 uploads the material to the VS. If the diffuse/specular path reads
   material.diffuse/specular (c11–c14) and they're 0/garbage → NaN or wrong. Walk D3D9
   `Direct3d9_LightManager::applyLights_vertexShader` (`:528–726`) AND the D3D9 material upload
   (`getSpecularPower`/setMaterial path) and confirm EVERY register the bump VS reads (per the disasm
   cb0[N] list) is fed in D3D11. This is the user-suggested "walk D3D9, cover everything" audit.
4. Char-select light counts: ParallelSpecular=1, Parallel=2, PointSpecular=1, Point=4
   (`Direct3d9_LightManager.h:63-67`). LightData layout (28 float4 @ c16): ambient[0], parallelSpecular[1-3],
   parallel[4-7], pointSpecular[8-11] (atten@10), point[0..3]@[12-23] (atten@14/17/20/23), dot3[24-27].

## Key files
- `Direct3d11_StateCache.cpp` — `composeSlot0Shadow()` (~328), `setSpecularPower()` (~2330), `ms_currentVSKey`/`ms_specularPower` (~168).
- `Direct3d11_StaticShaderData.cpp` — `construct()` key calc (~399), `apply()` setSpecularPower (~720).
- `Direct3d11_ConstantBuffer.h` — `Direct3d11_VertexSlot0CB` layout (~215, material[5]@c11, lightData[28]@c16).
- D3D9 refs: `Direct3d9_LightManager.cpp:528-726` (applyLights_vertexShader), `Direct3d9_VertexShaderData.cpp:344-533` (createVertexShader), `Direct3d9_StaticShaderData.cpp:540-585` (key calc).
- Disasm: `stage/stage/vs-disasm-all.txt` (bump VS = `a_specmap_bump_vs20_for_ps20.vsh`).

## Commit guidance
All uncommitted. Fix A (the per-key VS refactor) is correct + verified — worth committing on its own. The
GAP-5 lighting-feed fixes (#2/#3) are correct-but-insufficient; keep them but the audit will likely add more.
