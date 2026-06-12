# Handoff: Phase 19 — D3D11 world/interior rendering (2026-06-01, end of a long session)

**Branch/checkout:** `koogie-msvc-cpp20-base` in MAIN checkout `D:\Code\swg-client-v2`.
**Read this with** memory `project_d3d11_world_view_anomalies_phase19` (kept fully in sync with this doc).
**This handoff SUPERSEDES the prior BC-mip version of this file (that hypothesis was FALSIFIED).**

---

## TL;DR — what happened this session
We had wholesale D3D11 world/interior corruption. We root-caused and FIXED the big ones, and root-caused (not yet fixed) the rest. Method that worked every time: **characterize the D3D9 reference, get hard RenderDoc/log evidence, and verify before believing** (3-AI consults repeatedly caught wrong guesses — consensus ≠ correctness).

### FIXED (real fixes, KEEP in tree)
1. **Color rainbow** (whole surfaces cycling red/yellow/teal, flickering): root cause = **partial-write `Map(WRITE_DISCARD)` constant buffer**. `Direct3d11_ConstantBuffer::updateVS/updatePS` (`Direct3d11_ConstantBuffer.cpp:153/174`) write only `sizeBytes` of a `kMaxCBufferBytes` buffer; the undefined tail → garbage when a shader reads past it (e.g. `setFog`/`setFade` write 16B to PS slot 0). **Fix = `P19_CBUF_ZERO_TAIL` (Direct3d11_ConstantBuffer.cpp:35)**: memset full buffer to 0 after Map. This ALSO explained the whole "RenderDoc replay-clean Heisenbug" (live read undefined tail varying per frame; replay zero-inits). Proper long-term fix = route setFog/setFade through the slot-0 shadow / always upload full payload; the zero-tail is the working band-aid.
2. **Crash when capturing with RenderDoc**: it was the **DEBUG client's D3D11 debug layer + RenderDoc on a 32-bit process** (`#ifdef _DEBUG` debug layer, `Direct3d11_Device.cpp:175`). **ALWAYS capture on the RELEASE client** (`SwgClient_r.exe`, reads `client.cfg`, rasterMajor=11 already set). gl11_r.dll has all fixes.
3. **NPC bodies + matte surfaces rendering black** (interior + some exterior): root cause = **specular `pow(0,0)` NaN**. Asset spec-map PS (`a_specmap_pp_ps20.psh:32` and 6 siblings) do `pow(saturate(dot(halfAngle,normal)), materialSpecularPower)`; fxc lowers to `exp2(power*log2(x))`; when `saturate(dot)==0` AND `materialSpecularPower==0` (matte) → `exp2(0*-INF)=NaN` → poisons whole pixel. D3D9 ran frozen ps_2_0 bytecode (no NaN); D3D11 recompiles → NaN. **Fix = Rule F** in `Direct3d11_HlslRewrite.cpp` (specular-pow guard, 2 exact-string variants `_o`/`_t`: `(power>0 && nh>0) ? pow(...) : 0`) + `D3D11_REWRITE_VERSION` bumps (PS 24→25 in BOTH define lists `Direct3d11_PixelShaderProgramData.cpp:153,304`; VS 16→17 `Direct3d11_VertexShaderData.cpp:472`). VERIFIED-HELPING (capture8: NPCs/matte surfaces render now).

### ROOT-CAUSED, NOT YET FIXED
4. **Camera-locked "spaceship engine" prop** (follows camera): PROVEN camera-locked — a real 264-idx 3D mesh whose post-transform (vs-out) clip verts are IDENTICAL across a camera step (capture6 draw 7652 vs capture7 draw 8024; x-range −21.78..17.39 in both). Its VS gets a **camera-independent WVP** (view/world not applied). NOT a billboard, NOT texture. Root = how THIS object's world/view matrix is computed/fed in D3D11. Separate small bug; deferred.
5. **Interiors (cell walls/floor/ceiling) + exterior SKY not drawn** — THE current frontier. Root-caused this session (see next section). A VS-fallback consult is IN FLIGHT (see "Pending").

---

## Facet 5 root cause (the current frontier) — fully scoped
Interior cell shell + sky render as flat clear/fog color (brown) because **their geometry is never drawn**:
- Skip-audit (`P19_SKIP_AUDIT` in `Direct3d11_StateCache.cpp`, logs `[P19SKIP]`) named **120 unique shaders** skipped at `resolveShaders()` returning false (`Direct3d11_StateCache.cpp:1130-1159` → `applyPreDrawState:1179` returns false → no DrawIndexed). `skippedNullVS=38927`/session.
- Culprits: `stco_cantina_wall_*`, `intr_{hotel,tech,library,medical,commerce,combat}_*`, `despot_*`, `spaceport_*`, `frn_tatt_wall_*_cantina`, `glss_cantina_window`, `gradient_sky`, etc. (full list in the [P19SKIP] log / memory).
- Reason: `resolveShaders` fails because the VS variant's `d3dVS` is null. `d3dVS` is null because these use **`//asm` (legacy D3D9 vertex assembly) vertex programs** — D3D11 can't compile D3D9 VS asm (`Direct3d11_VertexShaderData.cpp:8-10,252-260`). Confirmed NOT compile-failure (zero HLSL X-errors in log). Reauthored `//hlsl` VS (`vertex_program/*_vs20_for_ps20.vsh`) render fine; the interior/structural/sky set uses unconverted `//asm` VS.
- **Asymmetry = the gap:** the PIXEL path has a fallback (Phase 17: `selectFallbackPSForVS` → `buildHlslForVSOutputs` GENERATES a PS matching the VS output sig). The VERTEX path has NO fallback → null `d3dVS` just skips the draw.

**Fix directions:** (1) add a symmetric **VS fallback** (generic HLSL world-transform+passthrough VS bound when asset VS is //asm/null; must match the mesh input format via `VertexDeclarationMap`, read WVP from `SwgVertexConstants` b0, emit a sig the PS fallback consumes) — in-plugin, recommended; (2) author //hlsl versions of the //asm interior VS (asset pipeline, heavier). The VS fallback is the INVERSE/harder problem (no compiled VS to reflect → must DEFINE input+output sigs).

---

## How to analyze (RenderDoc MCP — works great now)
- **Capture on the RELEASE client only** (`SwgClient_r.exe`); debug client + RenderDoc crashes (32-bit + debug layer).
- Render architecture: scene → baked RT **`ResourceId::323`** (cleared ~event 9, ~hundreds of ColorTarget draws) → ONE fullscreen composite quad (VS `ResourceId::1800` / PS `ResourceId::1802`, reads ::323) → swapchain `ResourceId::35`.
- **Backbuffer (::35) pixel-history shows ONLY the composite blit.** To analyze real geometry, pixel-history `::323` at the LAST scene event (get it from `get_resource_usage(ResourceId::323)` → last ColorTarget entry).
- MCP server `renderdoc-mcp` (loaded via ToolSearch `select:mcp__renderdoc-mcp__*`). Key tools used: open_capture, list_events(filter present), goto_event, export_texture/export_render_target, pixel_history(x,y,eventId), get_draw_info, get_bindings, get_pipeline_state, get_shader(reflect/disasm), debug_pixel/debug_vertex (mode trace), export_mesh(vs-out). Large results spill to a file under the session tool-results dir — slice with cut/grep (no python3; jq absent; use grep/awk).
- Captures on disk: `stage/capture1,2`=cantina interior, `capture4`=cylinder, `capture5`=exterior voids, `capture6/7`=cylinder before/after step, `capture8`=cantina post-Rule-F.

## Build / run
- Build: `& "$env:MSBUILD" "D:\Code\swg-client-v2\src\engine\client\application\Direct3d11\build\win32\Direct3d11.vcxproj" /p:Configuration=Debug|Release /p:Platform=Win32 /nodeReuse:false /v:minimal /m` (USE ABSOLUTE PATH — relative failed once this session). `$env:MSBUILD` is set in settings.json. Postbuild auto-stages gl11_d.dll / gl11_r.dll to stage/.
- Bumping `D3D11_REWRITE_VERSION` invalidates the .cso shader cache → first boot recompiles all shaders (slower once).
- Release client reads `client.cfg` (rasterMajor=11 set); Debug client reads `client_d.cfg`.
- C4459/C4100/C4189 warnings on these builds are PRE-EXISTING noise, not errors.

## Current tree state (uncommitted diagnostics — REVERT before shipping)
- **KEEP (real fixes):** `P19_CBUF_ZERO_TAIL=1` (ConstantBuffer.cpp:35); Rule F in HlslRewrite.cpp + the 3 REWRITE_VERSION bumps; the BC staging zero-fill in `Direct3d11_TextureData.cpp` lock() (latent-correctness, harmless).
- **REVERT when done diagnosing:** `P19_SKIP_AUDIT=1` (StateCache.cpp, before applyPreDrawState + the 2 skip sites) — served its purpose (named the 120 shaders). `P19_RETAIN_TEXTURES`, `P19_THREAD_AUDIT`, `P19_DISCARD_ONLY`, `P19_MAXLOD0`, `P19_LIGHTDUMP` are all already 0/off. GPU-finish in Device.cpp present() is `#if 0`.

## UPDATE 2026-06-01b — VS fallback IMPLEMENTED + RUNTIME-VERIFIED ✅
The VS-fallback design below is IMPLEMENTED, builds clean Debug + Release (gl11_{d,r}.dll staged), and is **RUNTIME-VERIFIED**:
- **Visual (Kenny):** cantina floor + walls render (capture9); sky renders correctly (capture10).
- **`skippedNullVS` 38927 → 0** (d3d11-debug.log shutdown stats, prior vs fixed session) — every previously-dropped draw now resolves.
- **Sky was NOT double-transformed:** `gradient_sky.vsh` is a WORLD-SPACE skydome → the static-world WVP fallback is the correct transform. The feared transformed/RHW variant is likely UNNEEDED.
- **Census = exactly 11 distinct `//asm` `.vsh`** behind the 120 skipped shaders (the `[P19VSFALLBACK]` set = the long-term //hlsl-authoring worklist):
  `c_simple`, `gradient_sky`, `cloudlayer`, `envmask_specmap_c`, `tfcl`, `tfcl_2uv`, `tfcl_4uv`, `tfcl_env`, `tfcsl`, `tfcsl_2uv`, `tfcsl_2uv_env`.

REMAINING (not blocking): author `//hlsl` for those 11 to restore lighting/lightmaps (fallback is flat/over-bright + TEXCOORD0-only); revert `P19_SKIP_AUDIT` (harmless now — 0 skips → no output); camera-locked prop (facet 4) still open. NOT committed (Kenny's call).

### UPDATE 2026-06-01c — tfcl/tfcsl family reauthored to //hlsl (7 of 11; AWAITING runtime verify)
Authored real `//hlsl` VS for the 7 `tfcl*`/`tfcsl*` shaders → restores per-vertex Gouraud lighting (the generic fallback only emitted const-white COLOR0 = flat/over-bright).
- **Source of truth:** extracted the `//asm` from `D:/Code/SWGSource Client v3.0/data_other_00.tre` via the swg-blender-plugin TRE toolkit; dumps + the `//hlsl` `functions.inc`/`vertex_shader_constants.inc` in `.planning/research/vsh-extract/`.
- **asm→hlsl mapping:** `tfcl` oD0 = `saturate(vColor0 + calculateDiffuseLighting(false,pos,n))`; `tfcsl` adds `oD1 = saturate(ds.specular * material.specularColor)` via `calculateDiffuseSpecularLighting`. `vColor0` = cell/ambient (asm `c_ambient.inc`), `dot3=false` keeps the hemispheric light, `saturate` matches the ps_1_1 color-register clamp. Mirrors the proven `a_simple_vs20_for_ps20.vsh` template idiom.
- **PS:** the tfcl-tier PS (`a_simple.psh` = `mul r0.rgb,t0,v0`, etc.) is legacy ps.1.1 asm → null pixelShader → routes through the PS-fallback, whose `t0.Sample * COLOR0` IS the correct tfcl pixel math. So VS-only authoring is complete for the base case. **Deferred (need a richer PS-fallback):** multi-texture blend (`tfcl_2uv/4uv` detail/dirt/mask), specular add (`COLOR1`), fog, env reflection (`_env` oT1).
- **Deployment (no C++ rebuild — runtime assets):** loose override dir `stage/override/vertex_program/*.vsh` + `client.cfg` `searchPath_00_10` (priority 10 > TREs 7/8; `maxSearchPriority=12`). Iterate loose, then **repack into a TRE** with the swg-blender-plugin tre tools when final (Kenny).
- **VERIFY:** `SwgClient_r.exe` cantina → walls/floor should show proper light gradients (not flat). `[P19VSFALLBACK]` for `tfcl*`/`tfcsl*` should DISAPPEAR (now compile as //hlsl); the other 4 (`c_simple`, `gradient_sky`, `cloudlayer`, `envmask_specmap_c`) remain on the generic fallback. Watch the D3D11 log for any `D3DCompile vs_4_0` X-errors on these names.

**What landed (all in `Direct3d11_VertexShaderData.cpp` unless noted):**
- New file-scope `kAssemblyFallbackVSSource` HLSL: `cbuffer SwgVertexConstants : b0 { float4x4 objectWorldCameraProjectionMatrix : packoffset(c0); }`; in `float3 POSITION0` + `float2 TEXCOORD0`; out `SV_Position` + `float2 TEXCOORD0` + `float4 COLOR0`; body = `mul(float4(pos,1), WVP)`, forward tc0, **const-white COLOR0**. Compiled with `vs_4_0` + `PACK_MATRIX_ROW_MAJOR` + `BACKWARDS_COMPATIBILITY` (matches transform3d convention).
- `compileAssemblyFallbackVS()` helper (D3DCompile + .cso cache; key-independent, salt define `D3D11_VS_FALLBACK=1`; non-FATAL on failure → null d3dVS → prior skip behavior).
- `logAssemblyFallbackOnce()` telemetry: `[P19VSFALLBACK] generic world-transform VS bound for //asm '<file>'` (dual route; InfoQueue path only live on DEBUG client).
- Extracted the post-compile tail (disasm + CreateVertexShader + D3DReflect in/out sig + outputSigHash) into shared `finalizeVariantFromBytecode()` so the //asm and //hlsl paths share it verbatim.
- `compileVariant()`: `m_isAssembly` branch now compiles the fallback (was empty-return at old :350); constructor no longer early-returns on //asm.
- `Direct3d11_VertexShaderData.h`: added `bool isAssembly() const`.
- `Direct3d11_StateCache.cpp applyPreDrawState`: when `ms_currentVSData->isAssembly()`, **skip native+rewritten asset-PS lanes**, route straight to `selectFallbackPSForVS` (asset PS expects asm varyings the fallback VS doesn't emit). `vsIsFallback` gate.

**KNOWN first-cut limits (by design):** no lighting/baked vertex color (flat/over-bright); only TEXCOORD0 (lightmaps/detail UVs lost); **static-world transform only — transformed/RHW geometry (gradient_sky) is double-transformed → sky will look wrong** (dedicated sky/RHW variant is the follow-up; long-term = author //hlsl for the shared //asm `.vsh` set).

**VERIFY NEXT (cantina):**
1. VISUAL (primary): `SwgClient_r.exe` → cantina → interior walls/floor/ceiling should render TEXTURED (flat/over-bright OK; sky may still be wrong). RenderDoc pixel-history `::323` at last scene event = non-clear.
2. TELEMETRY (use `SwgClient_d.exe`, client_d.cfg rasterMajor=11 — InfoQueue live there): D3D11 log shows `[P19VSFALLBACK]` lines (census the `.vsh` names) and `skippedNullVS` drops sharply from ~38927.
3. Then census distinct //asm `.vsh` → author //hlsl for the shared set (restores lighting/lightmaps/sky); add dedicated transformed/sky VS; revert P19_SKIP_AUDIT; revisit camera-locked prop (facet 4).

## Pending — RESOLVED (consults completed + synthesized)
VS-fallback consult done (CODEX + Cursor CONVERGED, code-grounded). **Design is in `.planning/research/CONSULT-19-vs-fallback-interior-asm-SYNTHESIS.md` — read that first** (raw: `…-{codex,cursor}.out`).
Agreed design (short): inject the fallback in **`Direct3d11_VertexShaderData::compileVariant()`** for `m_isAssembly` (fill the Variant: d3dVS/bytecode/hash/reflectedInputs+outputs, reusing the reflection block ~:752/:785) so everything downstream works. Two fallback VS: (1) static-world `mul(float4(pos,1), objectWorldCameraProjectionMatrix@b0 c0..3)`; (2) transformed/RHW passthrough when `VBFormat::isTransformed()` (sky). **Minimal input (POSITION+TEXCOORD0); emit CONST WHITE COLOR0 (do NOT declare COLOR0 as input → black); rely on phantom-zero for the rest.** Output SV_Position+TEXCOORD0+COLOR0 → chains to existing PS fallback (textured-passthrough). Force fallback PS lane when VS is the fallback. Pitfalls: baked/vertex-lighting loss (flat/overbright), lightmap loss (only TC0 forwarded), sky may need a dedicated VS. Long-term: census the distinct `//asm` `.vsh` (likely a few: `c_simple`, `gradient_sky`, …) and author `//hlsl` for correctness.

## NEXT STEP (start here next session)
1. Read `CONSULT-19-vs-fallback-interior-asm-SYNTHESIS.md`.
2. Implement the fallback in `Direct3d11_VertexShaderData::compileVariant()` (m_isAssembly path, currently empty-return at ~:350) per the synthesis; add `.vsh` filename telemetry on the asm path.
3. Build Debug+Release (absolute path), test cantina on `SwgClient_r.exe`: interior walls/floor + sky should render textured (lighting may be wrong); verify via RenderDoc pixel-history on ::323 = non-clear + `skippedNullVS` drops sharply.
4. Census distinct `//asm` `.vsh` from telemetry → author `//hlsl` for the shared set (restores lighting/lightmaps/sky).
5. Then revert P19_SKIP_AUDIT; revisit the camera-locked prop (facet 4).
