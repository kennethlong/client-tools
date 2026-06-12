# VS-fallback design — CODEX + Cursor SYNTHESIS (2026-06-01)

Both AIs converged (evidence-grounded, with file:line cites). Inputs: `CONSULT-19-vs-fallback-interior-asm-{codex,cursor}.out`. This is the implementable design for the interior/sky `//asm`-VS gap (handoff facet 5).

## Agreed design
**Inject inside `Direct3d11_VertexShaderData::compileVariant()`** (NOT StateCache). Today `compileVariant()` returns an empty variant when `m_isAssembly` (`Direct3d11_VertexShaderData.cpp:248` detect, `:260` log, `:350` empty return). Instead: for `m_isAssembly`, compile a **fallback HLSL VS** into that Variant and fill `d3dVS`, `bytecode`, `bytecodeHash`, `reflectedInputs`, `reflectedOutputs`, `outputSigHash` — reusing the existing reflection block (`:752`, `:785`). Then EVERYTHING downstream works unchanged: input-layout cache (`VertexDeclarationMap` keys on VBFormat+VS-bytecode-hash and needs non-null bytecode at `Direct3d11_VertexDeclarationMap.cpp:147`), the PS fallback, and bind attribution. `resolveShaders()` then succeeds (d3dVS non-null).

**WVP source:** `cbuffer SwgVertexConstants : register(b0)`, `objectWorldCameraProjectionMatrix` at `packoffset(c0)` (c0..c3) — `Direct3d11_ConstantBuffer.h:194/215`. Already composed+transposed at upload (`Direct3d11_StateCache.cpp:352` composeSlot0Shadow). Row-vector form: `mul(float4(pos,1), objectWorldCameraProjectionMatrix)`.

**Output signature (chains to existing PS fallback):** `SV_Position` + `TEXCOORD0` (float2 .xy) + `COLOR0` (float4). `buildHlslForVSOutputs` (`Direct3d11_PixelShaderProgramData.cpp:489`) keys purely on reflected VS outputs → picks textured-passthrough when it sees float `TEXCOORD0.xy` (`:506`), uses `COLOR0` (`:522`), samples t0/s0 (`:616`), magenta only if neither (`:658`). So a clean tc0+color0 output "just works."

**Input signature — CRITICAL (CODEX, strongest point):** declare MINIMAL input (`POSITION0` float3 + `TEXCOORD0` float2). Do NOT declare a superset (NORMAL/COLOR0/TEXCOORD1…). Missing VS inputs are backed by phantom-zero elements (slot 15; `augmentWithPhantomElements` `VertexBufferDescriptorMap.cpp:277`, bound `StateCache.cpp:1247`). **Emit CONSTANT WHITE `COLOR0=float4(1,1,1,1)` in the VS body — do NOT declare COLOR0 as an INPUT** (if the mesh lacks vertex color, the PS multiplies texture by zero → black surfaces).

**Two fallback variants needed (Cursor):**
1. **Static-world** — `mul(float4(pos,1), WVP)` as above. Covers cell walls/floor/interiors.
2. **Transformed/RHW** — when `VBFormat::isTransformed()` (POSITION is float4 RHW; `VertexBufferDescriptorMap.cpp:220-227`), DON'T apply WVP — pass POSITION→SV_Position (2d/sky-style). `gradient_sky` likely needs this (or a dedicated sky VS — see pitfalls). Classify by `VertexBufferFormat::getFlags()`.

**Force the fallback PS lane for asm interiors (Cursor caveat):** when the effective VS is the fallback, route through `selectFallbackPSForVS` — don't let an asset PS bind against fallback VS outputs (it expects asm-computed varyings → wrong lighting). Gate on `m_isAssembly`.

## Pitfalls (both AIs) + RenderDoc detection
1. **Baked/vertex lighting loss** — const-white COLOR0 → walls visible but flat/over-bright vs D3D9. Detect: fallback VS COLOR0=1,1,1,1 vs D3D9 brightness.
2. **Multi-UV / lightmap loss** — only TEXCOORD0 forwarded; lightmaps/detail (TEXCOORD1+) vanish. Detect: D3D9 has baked shadow gradients, D3D11 uniformly textured.
3. **Sky** — `gradient_sky` may not be plain texture-passthrough (vertex color / height-fog / special depth). Detect: fallback VS bound on `gradient_sky.sht` but looks solid/wrong → author a dedicated sky VS.

## Recommended sequence
1. Land the **generic static-world fallback** (+ transformed variant) in `compileVariant()` with **filename telemetry** (log the `//asm` `.vsh` name per skipped shader).
2. Run an interior capture; success = `skippedNullVS` drops sharply, interior wall draw has fallback VS bound, input layout creates, PS path = fallback, pixels textured-passthrough (lighting may be wrong).
3. **Census the distinct `//asm` `.vsh`** behind the 120 shaders (CODEX saw e.g. `intr_tech_floorb_cs8.sht` → `vertex_program/c_simple.vsh`; likely a handful: `c_simple`, `gradient_sky`, a few structural). Then **author `//hlsl` versions** of that small shared set for correctness (lighting/lightmaps) — the long-term fix; the fallback is the visibility bridge.

## Net
Fallback-VS-in-`compileVariant` is the agreed, lowest-friction path (everything downstream already wants a populated Variant). Minimal input + const-white color + WVP-from-b0 + tc0/color0 output → chains to the PS fallback → interiors render textured. Then census + author HLSL for the shared `//asm` VS set to restore lighting/lightmaps/sky.
