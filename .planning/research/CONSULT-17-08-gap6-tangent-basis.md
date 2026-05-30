# CONSULT: GAP-6 — bump-shader VS emits NaN vertex colors (missing tangent basis) on D3D11

You are a senior graphics/engine engineer doing an independent design review. I need a concrete,
implementable design — not a restatement of the problem. Be specific about WHERE the fix belongs and
WHY. Flag wrong assumptions. No repo access; everything you need is below. We have learned a LOT since
the last consult (GAP-4); this brief supersedes the earlier black-screen framing.

## System context

Star Wars Galaxies (SOE) client being ported from a D3D9 fixed-function/VSPS renderer to a new D3D11
renderer plugin (`gl11_d.dll`, runtime-loaded via a C API vtable). We are proving the "asset pixel
shader pipeline" on the deterministic character-select screen as a beachhead. Pixel/vertex shaders ship
as IFF assets carrying their ORIGINAL D3D9-era HLSL (`//hlsl ps_2_0` / vertex equivalents) in a retained
chunk; our lane RECOMPILES that HLSL to `ps_4_0` / `vs_4_0` at load (D3DCompile with
`D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY`).

## What now works (do NOT redesign — all RenderDoc-confirmed this session)

- **GAP-3:** asset PS bind rate 0/9 → **9/9** (`path=rewritten`). The recompiled PS input signature is
  reconstructed to mirror the VS output signature exactly (SV_Position@reg0 + one field per VS output).
- **GAP-4:** the asset PS reads light/material constants from a reflected cbuffer `SwgVertexConstants` @
  **PS b0** (`pixel_shader_constants.inc` wrapped: `dot3LightDirection`=packedRegister0.xyz=c0,
  `materialSpecularPower`=c0.w, `dot3LightDiffuseColor`=c1, ... `userConstants[17]`=c8). We now feed b0
  via 3 producers (LightManager dot3 push @ c0-c4, StaticShaderData RMW @ c5-c7, userConstants @ c8+).
  RenderDoc confirmed the b0 values are correct.
- **GAP-5:** the VS reads its OWN lighting from `SwgVertexConstants` @ **VS b0** (= the 1152-byte
  `VertexSlot0CB`; `vertex_shader_constants.inc` layout, VSCR map: WVP@c0, World@c4, cameraPosition@c8,
  material@c11, **lightData[28]@c16** [ambient@c16, parallelSpecular@c17-19, dot3@c40-c43],
  **extendedLightData[8]@c60**). We now fill these per-object in the VS cbuffer (mirroring D3D9
  `applyLights_vertexShader`). RenderDoc confirmed: for the failing draw, `dot3.direction_o =
  (0.735,0.399,0.549)`, `diffuseColor=(1,1,1)`, `specularColor=0.5`, `ambient=0.0169`, all FILLED and
  correct; the VS-input NORMAL is valid (e.g. `-0.913,0.136,0.401`).

**Result:** the character now renders LIT and textured via the asset-PS lane — face/skin, white tunic,
pants, shoes all correct. **Most of the model is right.**

## The problem (GAP-6): bump-shader parts render with wrong/underlit color (NaN vertex colors)

On the SAME character, the parts drawn by **bump pixel shaders** (e.g. `h_specmap_bump_ps20`,
`h_color2_specmap_bump_ps20`) — the **sleeves and hands** — come out the wrong color (dark/“purple”).
RenderDoc pixel + mesh inspection of one such draw shows:

- The PS inputs `COLOR0` (vertexDiffuse) and `COLOR1` (vertexSpecular) are **NaN**.
- The **VS OUTPUT** `COLOR0`/`COLOR1` are **NaN for every vertex** (so the VS itself produces NaN; it is
  not an interpolation/PS issue).
- The VS **INPUT** has position, NORMAL (valid), skinning, and TEXCOORD sets — but **no dedicated
  TANGENT/BINORMAL** element.

A NaN `vertexDiffuse` flows into the PS `calculateHemisphericLighting(...)` (vertexDiffuse is an additive
term) → `result.rgb = NaN`. (We added a wrapper NaN-guard `any(isnan(COLOR0)) ? 0 : COLOR0` so it no
longer renders fully black — it falls back to per-pixel N·L only, which is why the part is lit-but-wrong
rather than black. We want to fix the source, not rely on the guard.)

## ROOT-CAUSE HYPOTHESIS (the key mechanism — please validate or refute)

**SWG does NOT use `D3DDECLUSAGE_TANGENT`/`BINORMAL`.** Both the D3D9 and D3D11 vertex-declaration
mappings only emit POSITION / NORMAL / PSIZE / COLOR0 / COLOR1 / **TEXCOORD<n>**. So the tangent basis
for bump shaders is carried as **extra TEXCOORD sets** (dimension-3 texcoords), and the bump VS builds
its tangent-space basis from those TEXCOORD inputs.

The D3D11 plugin (Plan 11-09.8) has a **phantom-element mechanism**: `CreateInputLayout` under `vs_4_0`
validates the layout against the bytecode input signature strictly. The engine's D3D9-era HLSL declares
TEXCOORD inputs even when the bound vertex buffer's `VertexBufferFormat` doesn't provide them; to satisfy
strict validation, the plugin adds **phantom input elements at InputSlot 15 bound to a stride-0,
16-byte, ZERO-filled buffer** — so the VS reads **zero** for any declared input the VB doesn't cover.

So our hypothesis: the bump VS declares TEXCOORD input(s) for the tangent/binormal, but the char mesh's
`VertexBufferFormat` for these parts does NOT include those TEXCOORD set(s) → the phantom buffer feeds
**zero** → the VS does `normalize(0)` on the tangent (or `cross()` of zero vectors) → **NaN** → the NaN
contaminates the shared output including `COLOR0`.

**CONFIRMED via RenderDoc (decisive):** for the failing `h_specmap_bump` sleeve draw, the VS Input has
`TEXCOORD0` (real — diffuse uv) and `TEXCOORD1` (real) — but **`TEXCOORD2` is all `0.0`** (the phantom
zero buffer). So the VS reads `TEXCOORD2` (the tangent or binormal of the tangent-space basis) as a zero
vector and `normalize()`s it → NaN. The VB format reports fewer TEXCOORD sets than the bump VS's input
signature declares, so `TEXCOORD2` is phantom-augmented to zero.

The remaining unknown: is `TEXCOORD2` a set the char mesh's `VertexBufferFormat` SHOULD carry (i.e. the
VB has the tangent data but the D3D11 VB-format→input-element mapping under-reports the texcoord-set
count, dropping it), OR does this mesh genuinely store only TEXCOORD0/1 and the original D3D9 bump VS
DERIVED the third basis vector (e.g. binormal = cross(normal, tangent)) without faulting on a zero —
meaning the recompiled `vs_4_0` path is the regression?

## Design questions (answer each concretely)

1. **Is the phantom-zero → `normalize(0)` → NaN the right root cause?** The VS NORMAL is valid and the
   lighting constants are correct, yet `COLOR0` (vertex diffuse, which seemingly needs only normal+light)
   is NaN. How does a degenerate tangent basis poison `COLOR0` specifically — i.e. is it typical for these
   SOE dot3/bump VS shaders to compute the per-vertex diffuse color through a shared tangent-space
   transform (so one NaN intermediate kills all color outputs)?

2. **Where should the tangent basis come from?** Options we see: (a) the char mesh genuinely HAS the
   tangent/binormal as TEXCOORD sets in its vertex buffer, and the D3D11 input-layout build is dropping or
   mis-mapping them (so the fix is in the VB-format → input-element mapping); (b) the mesh genuinely lacks
   them and D3D9 tolerated the zero (different VS math / no normalize), so the D3D11-recompiled VS must be
   guarded; (c) the engine derives the tangent basis elsewhere (a CPU-side per-mesh tangent generation, or
   a separate stream the D3D11 path isn't binding). How would you determine which, from RenderDoc (we can
   inspect the VB contents, the VS input signature, and the D3D9 capture of the same mesh)?

3. **If the data IS present as TEXCOORD sets but fed zero by the phantom path:** what's the most likely
   mapping bug — a TEXCOORD UsageIndex/offset mismatch between the VB format and the VS input signature,
   or the phantom augmentation shadowing a real element? How to make the real TEXCOORD reach the VS.

4. **If the data is genuinely absent:** is guarding the VS `normalize` (e.g. emit a safe default basis
   when the tangent is zero) acceptable for parity, or does correct bump/specular REQUIRE a real tangent
   basis (meaning we must generate/supply one)? What's the minimal correct fix for the beachhead?

5. **Instrumentation:** what one or two RenderDoc observations (VB element list + the specific TEXCOORD
   values at a vertex, VS input signature semantics/indices, D3D9-capture comparison) would most quickly
   confirm the chosen root cause?

6. Any failure mode / simpler alternative we're missing (e.g. is the `D3DCOMPILE_ENABLE_BACKWARDS_
   COMPATIBILITY` recompile changing how the VS consumes the TEXCOORD tangent vs the original D3D9
   bytecode)?

Give a recommended approach with concrete touch points (VB-format→input-element mapping vs VS-recompile
guard vs tangent generation) and the data flow, then the risks.
