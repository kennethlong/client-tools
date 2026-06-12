# CONSULT-41 — D3D11 character FACE lit BLUE (washed bluish-white); BODY lit warm/correct

Date 2026-06-08. Branch `koogie-msvc-cpp20-base` (D:\Code\swg-client-v2). Plugin:
`src/engine/client/application/Direct3d11`. Reference: `Direct3d9` sibling. RenderDoc CANNOT capture
D3D9 here — reason the D3D9 side from source. You are one of 4 independent consultants; read SOURCE,
reach your OWN conclusion. The §4 conclusion is the leading theory to CONFIRM or REFUTE, not a given.

## 1. SYMPTOM
NPC face renders bluish-white/washed; her body/shoulders/arms render correct warm skin. Same
character, same frame, same lighting. Constant with distance.

## 2. WHAT IS ALREADY RULED OUT (prior consult CONSULT-40, shipped + verified)
- NOT alpha/translucency. A real alpha-fade-register leak was found + fixed (dot3 PS output alpha was
  a spurious 0.6 from leaked light alpha; now packs the engine fade state at
  `Direct3d11_StaticShaderData.cpp:962-968` + `Direct3d11_StateCache.cpp:2025`). VERIFIED: face alpha
  0.6 -> ~0, but the visible color did NOT change -> the washout is the LIGHTING RGB, not alpha.
- NOT the diffuse texture: the skin texel is correct (pale, slightly warm).
- NOT shader selection: face and body draws use the SAME shader.

## 3. LOCKED GROUND TRUTH — RenderDoc Capture34, scene RT 323 (BGRA8), post-fix
Face = draw EID 687, Shoulder = draw EID 739. BOTH: VS=ResourceId::2208, PS=ResourceId::2224
(dot3 bump; samples diffuseMap/normalMap/specularMap; PS b0 `SwgVertexConstants` 400B; VS b0 1088B).

Forehead (740,420), PS 2224 trace decoded (float):
| value | RGB | note |
|---|---|---|
| diffuseMap sample (skin tex) | (0.969, 0.938, 0.938) | pale, slightly WARM — CORRECT |
| PS input **v1** (VS vertex lighting, interpolated) | **(0.524, 0.537, 0.707)** | **BLUE-dominant** |
| PS **r1** (dot3 lit base after hemispheric) | **(0.556, 0.569, 0.723)** | **BLUE-dominant** |
| specular r2 | ~0 | no highlight here |
| **OUTPUT o0.rgb** = diffuse×r1 + spec | **(0.506, 0.502, 0.660)** | bluish |

Shoulder (690,500), SAME shader: **OUTPUT o0.rgb = (0.600, 0.463, 0.545)** — WARM (R>B). Correct.

PS 2224 dot3 base (disasm), `packedRegisterN` = PS b0 register cN:
```
12: add  r1.yzw, v1.xxyz, packedRegister3.xxyz        ; r1 = v1 + c3 (tangentMinusDiffuseColor)
13: add  r1.yzw, r1.yyzw, packedRegister1.xxyz        ;        + c1 (diffuseColor)
14: mad  r1.xyz, -r1.xxxx, packedRegister3.xyzx, r1.yzwy
15: mad_sat r1.xyz, r0.wwww, packedRegister4.xyzx, r1.xyzx ; + r0.w * c4 (tangentMinusBackColor)
```
So the lit base r1 = f(v1, c1, c3, c4). BOTH the VS vertex color v1 (already blue) AND the dot3
hemispheric constants c1/c3/c4 feed it. (Register map confirmed in CONSULT-40: c1=offset16,
c2=offset32, c3=48, c4=64; `Direct3d11_LegacyPSConstants.h:54-58`.)

## 4. LEADING THEORY (confirm or refute)
Hemispheric dot3 lighting: upward-facing surfaces (forehead) catch the "sky/up" color; downward/side
surfaces (shoulder) catch the "ground/back" color. The D3D11 "sky/up" term is BLUE-shifted vs D3D9.
Candidate sources of the blue: (a) the VS vertex lighting v1 (ambient + parallel diffuse) is blue;
(b) the dot3 hemispheric c1/c3/c4 (diffuse / tangentMinusDiffuse / tangentMinusBack) carry a blue
sky/tangent color; (c) a blue fill/bounce directional dominates upward normals; (d) tangent/back
(sky/ground) swapped or mis-weighted. Same family as the long-standing "faces too light / white-
ambient hemispheric floor" item.

## 5. KEY SOURCE
D3D11:
- `Direct3d11_LightManager.cpp` setLights + dot3 snapshot (~:180-230): diffuseColor/specularColor
  (:193-194), backColor/tangentColor (:199-200), tangentMinusDiffuse/tangentMinusBack (:195-198),
  parallel[0..1] (:207-223), ambient (:230).
- `Direct3d11_StateCache.cpp` composeSlot0Shadow (~:329-454): VS lightData fill — ambient c16,
  parallel c20-23, dot3 diffuse/spec c26/27, extended hemispheric c60-63 (:410-414), parallel
  diffuse (:425-430).
D3D9 reference:
- `Direct3d9_LightManager.cpp` `_vsps_setExtendedLightData` (:732 — backColor=getScaledDiffuseBackColor,
  tangentColor=getScaledDiffuseTangentColor, tangentMinus* deltas), `applyLights_vertexShader`
  (:528-610), `selectLights` (:404 fullAmbient / :412-418 ambient sum / :423-440 parallel cascade).
- `Light::getScaledDiffuseTangentColor` / `getScaledDiffuseBackColor` (sky/up vs ground/down hemisphere
  colors). `GroundEnvironment.cpp` main/fill/bounce + back/tangent color ramps.

## 6. YOUR ANGLE (answer ONLY your lettered section)
### A — CODEX (dataflow trace)
Trace where the blue in v1 (VS vertex lighting = 0.524,0.537,0.707) and in the PS hemispheric c1/c3/c4
originates, in BOTH renderers, from the light/ambient/GroundEnvironment producers down to the cbuffer
write. State the first source line where D3D11's "sky/up" or ambient or fill contribution diverges
from D3D9 in a way that adds blue. Concrete float at draw 687 where possible.

### B — CURSOR (register/offset detail)
Establish the exact VS lightData + PS dot3 b0 register layout for the hemispheric path and map each
D3D11 fill (composeSlot0Shadow ambient c16 / parallel c20-23 / dot3 c26-27 / extended c60-63;
LightManager dot3 c1/c3/c4) to its D3D9 origin (PixelDot3Data / ExtendedLightData / LightData).
Is any sky/ground (tangent/back) color or ambient assigned to the wrong slot, swapped, or sourced
from the wrong getter vs D3D9? Cite file:line.

### D — OPUS (numeric)
From the measured forehead (v1=0.524,0.537,0.707; r1=0.556,0.569,0.723; diffuse texel 0.969,0.938,0.938;
out 0.506,0.502,0.660) and shoulder (out 0.600,0.463,0.545), back-solve the implied sky/up color vs
ground/down color and the ambient. Is the blue mathematically attributable to ambient, to a blue
fill/bounce directional, or to the hemispheric sky term? Compute the implied source colors and say
which single constant, if corrected, turns the forehead warm without breaking the (correct) shoulder.
State the one decisive dump (e.g. c1/c3/c4 + ambient + sky/ground source colors at draw 687).

(Angle C — SONNET, lateral/lead — already running separately.)

## 7. DELIVERABLE (per angle)
(1) finding with file:line; (2) confirm/refute §4 (is it hemispheric? or ambient/fill?); (3) minimal
D3D9-parity fix + exact location; (4) one risk the other angles miss. Be willing to disagree.
