# CONSULT: GAP-4 — reconcile asset-PS cbuffer (b0 SwgVertexConstants) with the engine's D3D11 constant push

You are a senior graphics/engine engineer doing an independent design review. I need a concrete,
implementable design — not a restatement of the problem. Be specific about WHERE to write code and
WHAT the data flow should be. Flag wrong assumptions. No repo access; everything you need is below.

## System context

This is a Star Wars Galaxies (SOE) game client being ported from a D3D9 fixed-pipeline renderer to a
new D3D11 renderer plugin (`gl11_d.dll`). The renderer is a runtime-loaded DLL; the engine talks to
it through a C API vtable (`Gl*`). We are proving the "asset pixel-shader pipeline" on the isolated,
deterministic **character-select** screen as a beachhead before open world.

Shaders ship as IFF assets. Each pixel shader (`.psh`) carries a discarded `TAG_PSRC` chunk holding
the ORIGINAL HLSL source (D3D9-era, e.g. `//hlsl ps_2_0`). Our lane RECOMPILES that retained HLSL to
`ps_4_0` at load time (D3DCompile, with `D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY`), rather than
trying to consume the pre-compiled D3D9 bytecode.

## What now works (GAP-3, just landed — do NOT redesign this)

The recompiled asset PS and the independently-recompiled VS had mismatched signature REGISTER numbers
(D3D11 SM4 stage linkage is register-position-strict). We fixed it by reconstructing the asset PS's
input signature to mirror the VS output signature exactly (SV_Position@reg0 + one field per VS output
in register order, matching type/mask), wrapping the asset's renamed `main`→`assetMain1707` in a new
`main(PsIn1707)` that calls it. Result: **9/9 char-select asset PSes now bind and run** with correct
interpolators. Verified by reflection: PS input registers now equal VS output registers.

## The problem (GAP-4): the asset PS now renders a TOTALLY BLACK silhouette

The asset PS runs with correct interpolators + textures, but outputs black. Example body shader
`a_specmap_pp_ps20` (recompiled from retained HLSL):

```hlsl
//hlsl ps_2_0
#include "pixel_program/include/pixel_shader_constants.inc"   // declares dot3LightDirection, dot3LightDiffuseColor, dot3LightSpecularColor, materialSpecularColor, materialSpecularPower, bloomEnabled, ... as register(cN) globals
sampler diffuseMap : register(s0);
sampler specularMap : register(s1);
float4 main(in float3 vertexDiffuse:COLOR0, in float3 vertexSpecular:COLOR1, in float2 tcs_MAIN:TEXCOORD0, in float3 normal_o:TEXCOORD1, in float3 halfAngle_o:TEXCOORD2) : COLOR {
    float3 diffuseColor = tex2D(diffuseMap, tcs_MAIN);
    ...
    float3 allDiffuseLight = calculateHemisphericLighting(dot3LightDirection, normal_o, vertexDiffuse);
    ... uses dot3LightSpecularColor, materialSpecularColor, materialSpecularPower ...
    result.rgb = (diffuseColor * allDiffuseLight) + allSpecularLight;
    return result;
}
```
If `dot3LightDirection`/light colors/material are zero, `allDiffuseLight≈0` → `diffuseColor*0` → BLACK.
Boot evidence: `wroteDiffuse=1` count = 0; the lighting constants are zero at draw time.

## ROOT CAUSE — three constant paths that don't agree on cbuffer slot OR layout

Our HLSL rewriter (`Direct3d11_HlslRewrite.cpp`) wraps the loose `register(cN)` globals from the
shared `.inc` into a cbuffer and emits it at **b0**:

```
cbuffer SwgVertexConstants : register(b0) { <each .inc global> : packoffset(cN); };
```

D3DReflect of a recompiled char-select body PS shows `SwgVertexConstants`@**b0**, totalSize=400, 9 vars:
```
var[0..4]: packedRegister0..4   offset 0..79   (the wrapped c-register globals: matrices, light dir/colors, etc.)
var[5]:    textureFactor         offset 80
var[6]:    textureFactor2         offset 96
var[7]:    materialSpecularColor  offset 112
var[8]:    userConstants[17]      offset 128..399  (272B; reflects as a FLAT float4[17], no member names)
```

The engine pushes PS constants via the vtable callback `setPixelShaderUserConstants(VectorRgba* constants, int count)`.
Our D3D11 impl (`Direct3d11.cpp:679-754`) copies them into a `Direct3d11_PerMaterialCB` struct's
`userConstants[]` and flushes the WHOLE struct to **`updatePS(2)` → slot b2** (PerMaterialCB's own layout).

Separately, `Direct3d11_StaticShaderData::apply()` builds a ZERO-initialized 400B staging buffer, writes
ONLY `materialSpecularColor`/`textureFactor`/`textureFactor2` by reflected NAME, and uploads the whole
400B to **b0** (`updatePS(layout.BindPoint=0, staging, 400)`). So b0's `packedRegister0..4` and
`userConstants[17]` are written as ZERO.

Net effect:
- Asset PS reads everything from **b0** (`SwgVertexConstants`).
- The engine's real PS constant values (light/material, via `setPixelShaderUserConstants`) land in **b2**
  in the `PerMaterialCB` layout — the asset PS never reads b2.
- The `register(cN)` light globals (→ b0 `packedRegister0..4`) are pushed to b0 by NOBODY. The rewriter's
  own comment says b0 is "uninitialized at runtime" pending a deferred "Plan 11-08" c-register push.
- `StaticShaderData::apply` actively zeroes the non-material regions of b0.

The VS works (geometry transforms correctly) because the VS gets its matrix from a different upload path
(VS slot-0 buffer via the transform setters), not from this PS b0.

## Hard constraints

- **D3D9 reference renderer (`gl05`) must NOT regress** — byte-for-byte. Only touch D3D11 plugin files.
- The **dynamic fallback PS** (a generated PS that samples t0 * COLOR0; used for the 3 non-`//hlsl`
  shaders and any residual) must keep working — it does not read these constants.
- ComPtr ownership only; cbuffers go through `Direct3d11_ConstantBuffer::updatePS(slot, ptr, bytes)` /
  `bindPS(slot)`. Slots currently used: PS b0 (asset SwgVertexConstants OR StaticShaderData material),
  b2 (engine userConstants/PerMaterialCB). The fallback PS uses b1 (PSAlphaTest).
- **Asset-data gap (census gate):** `pixel_shader_constants.inc` and `vertex_shader_constants.inc` live
  in the retail TRE archives and are NOT extracted in our checkout. So we currently CANNOT see which
  `userConstants[N]` / `packedRegister[N]` index corresponds to which semantic (dot3LightDirection vs
  materialDiffuse vs ...). A prior plan refused to GUESS a slot (it would falsely report success).
  `setPixelShaderUserConstants` gives us a generic positional `VectorRgba[]` with no names.
- The rewriter KNOWS the mapping it emitted (it wraps each `register(cN)` global → `packoffset(cN)` in
  declaration order), and D3DReflect gives us each var's name + offset for b0.

## Design questions (please answer each concretely)

1. **What is the correct target architecture** for feeding the asset PS's b0 `SwgVertexConstants`? Options
   we see: (a) redirect the rewriter to emit `SwgVertexConstants` at b2 to match the engine push, but the
   `PerMaterialCB` layout differs from the rewriter's packoffset(cN) layout — would that even line up? (b)
   make `StaticShaderData::apply` build ONE merged b0 staging buffer that includes the engine's
   userConstants[] + c-register globals + material, in the rewriter's reflected layout, and stop zeroing;
   (c) wire a dedicated "legacy c-register push" to b0. Which is least-risky and most-correct, and why?

2. **The c-register globals (`packedRegister0..4`, incl. light direction/colors):** the engine's
   `setPixelShaderUserConstants(VectorRgba*, count)` is the ONLY PS-constant callback we see. In the SOE
   shader system, are the `register(cN)` globals (declared in the shared `.inc`) supplied through that
   same `userConstants` array (i.e. the effect packs ALL its PS constants into one positional array), or
   through a separate engine path we haven't wired? If the former, the fix is purely a slot/layout
   re-route; if the latter, we must find/wire that path. How would you determine which, given we can't
   read the `.inc`?

3. **Positional vs semantic mapping without the `.inc`:** can we reconstruct the full b0 buffer purely
   from (the rewriter's known declaration-order packoffset mapping) + (the engine's positional
   `VectorRgba[]` from `setPixelShaderUserConstants`) WITHOUT knowing the semantic names — i.e. is the
   engine's `VectorRgba[]` ordering guaranteed to match the `.inc` `register(cN)` ordering 1:1? If yes,
   we can map positionally and avoid the census-gate entirely. What evidence would confirm/refute this?

4. **Is extracting the two `.inc` files from the TRE archives a prerequisite**, or can this be solved
   without them? If a prerequisite, what's the minimum we need from them?

5. **Instrumentation:** what one or two cheap boot-time log lines would most quickly confirm the chosen
   design is feeding the right values to b0 (we can add logs + reboot to capture)?

6. Any failure modes / simpler alternative we're missing (e.g. is there a reason the original D3D9 path's
   constant-register layout maps cleanly that we should exploit)?

Give a recommended approach with concrete file/function touch points and the data-flow, then the risks.
