# Consult: D3D11 interior surfaces + NPC bodies render BLACK — asset-PS specular `pow()` emits NaN

## TL;DR
In our D3D11 renderer port of the Star Wars Galaxies client (`gl11`, rasterMajor=11), **building interiors (POB cells) and most NPC bodies render black / missing**, while the **D3D9 path (`gl05`) renders them correctly**. Via RenderDoc we traced it to the recompiled asset pixel shader **`a_specmap_pp_ps20.psh`** producing **NaN** in its specular term. We want your help confirming the exact trigger and the minimal fix that MATCHES D3D9 output — and a comparison of how the D3D9 vs D3D11 path handles this shader. **Be skeptical / challenge the framing** — we have repeatedly converged on wrong root causes this project and only trust evidence.

## Established context (already fixed / ruled out — do NOT re-litigate)
- A separate, now-FIXED bug: world color corruption was a partial-write `Map(WRITE_DISCARD)` constant buffer (tail left undefined). FIXED. The black-interior bug below is DIFFERENT.
- Renderer architecture: scene renders into an offscreen baked RT (`ResourceId::323`), then one full-screen quad blits it to the swapchain. (Irrelevant to this bug, but note backbuffer pixel-history only shows the blit — we analyze the baked RT.)
- This is a single-threaded renderer; not a race.

## Hard evidence (RenderDoc, Release-client capture `stage/capture1.rdc`, cantina interior)
1. Interior geometry IS submitted and drawn (it is NOT a "geometry not submitted" or culling problem). At a dark interior pixel, draw **eventId 1612** (a 258-index mesh, VS `ResourceId::2210` / PS `ResourceId::2212`, PS samples `diffuseMap`+`specularMap`) writes the pixel.
2. `debug_pixel` on that draw: **PS output `o0 = [NaN, NaN, NaN, NaN]`**.
3. VS-output signature and PS-input signature **MATCH exactly** (SV_Position, COLOR0, FOG, COLOR1, TEXCOORD0/1/2) — so it is **NOT** a VS→PS interpolator/register mismatch.
4. VS **input** `NORMAL` is valid/unit (`[~0, -0.701, 0.713]`).
5. The PS is the recompiled asset shader **`a_specmap_pp_ps20.psh`**. Disassembly shows the NaN is born in the **specular term** (source line 32):

```
; :32  dot3SpecularIntensity = pow(saturate(dot(halfAngle_o, normal_o)), materialSpecularPower)
 21: dp3 r0.z, r3, r3      ; normal_o = normalize(normal_o)   (r3 = v4, valid)
 22: rsq r0.z
 23: mul r3.xyz, r0.zzz, r3
 24: dp3 r0.z, r4, r4      ; halfAngle_o = normalize(halfAngle_o)  (r4 = v5, valid)
 25: rsq r0.z
 26: mul r4.xyz, r0.zzz, r4
 ...
 44: dp3 r0.z, r4, r3      ; dot(halfAngle_o, normal_o)  -> can be <= 0
 45: max r0.z, r0.z, 0     ; saturate ...
 46: min r0.z, r0.z, 1     ; -> 0 on back-facing / non-specular pixels
 47: log r0.z              ; log2(0) = -INF
 48: mul r0.z, r0.z, packedRegister0.w   ; * materialSpecularPower
 49: exp r0.z              ; pow result -> NaN propagates from here
 50: mul r3.xyz, r0.zzz, packedRegister2.xyz   ; * dot3LightSpecularColor -> NaN
 ...
 78: mov o0.xyzw, r1.xyzw  ; o0 = NaN
```

6. fxc lowers `pow(x,p)` to `exp2(p * log2(x))`. When `saturate(dot(halfAngle, normal)) == 0` (true on most of a surface), `log2(0) = -INF`, and the pow degenerates (`exp2(0 * -INF)` / `pow(0,0)` is NaN; `pow(0, p)` is also a hazard). The resulting NaN is multiplied into the color → black/garbage → the surface visually disappears.
7. The shader's OWN NaN guards only sanitize the **vertex inputs** (`main()` wraps inputs: `any(isnan(i.f_0)) ? 0 : i.f_0`, same for f_2) — they do **NOT** guard this internally-generated specular NaN.

## Why interior/NPC but not exterior (our hypothesis — challenge it)
Matte materials (likely `materialSpecularPower == 0`, common for interior walls/floors and NPC cloth/skin) make `pow(0, 0)` → NaN over the whole surface. Specular exterior materials (power > 0) give `exp2(power * -INF) = exp2(-INF) = 0` → no NaN. So matte surfaces (interiors, NPC bodies) are the ones that vanish.

## Questions
1. **Confirm or refute the trigger.** Is the NaN from `pow(saturate(dot)==0, materialSpecularPower)` with `materialSpecularPower == 0` (matte) → `exp2(0 * log2(0))` = NaN? Or is `materialSpecularPower` non-zero and the NaN comes from `exp2(-INF)` / another path? What is the precise IEEE chain that yields NaN on real D3D11 hardware here, and which exact input (`packedRegister0.w` = materialSpecularPower) must we inspect to confirm?
2. **D3D9 vs D3D11 divergence (the key question).** The SAME `a_specmap_pp_ps20.psh` renders fine under D3D9. Why? Trace the D3D9 path: does D3D9 consume the original `.psh` **assembly** (which may implement specular via a clamped instruction / `lit` / fixed-function-derived path that never does `pow(0,0)`), while the D3D11 port **recompiles to HLSL** and lowers `pow` to `log2/exp2` introducing the NaN? Or does D3D9 supply a clamped/non-zero `materialSpecularPower` where D3D11 supplies 0? Where in the codebase does each path compute/feed `materialSpecularPower` and the specular term? (search the Direct3d9 vs Direct3d11 plugins under src/engine/client/application, the shader/material upload, and the .psh→HLSL recompile lane in Direct3d11_PixelShaderProgramData.cpp).
3. **Minimal correct fix that MATCHES D3D9.** Options: (a) guard in shader source / recompile: `dot>0 ? pow(saturate(dot), power) : 0`, or `pow(max(saturate(dot), 1e-8), max(power, 1e-8))`; (b) clamp `materialSpecularPower` to ≥1 at upload; (c) a NaN-sanitize on the specular term. Which best reproduces D3D9's actual output (D3D9 `pow(0, 0)` semantics?) without changing correct specular on exterior surfaces? Is there a global place (the recompile lane) to fix ALL asset shaders that use this `pow` pattern, vs per-shader?
4. **Scope.** How many asset PS templates use this `pow(saturate(dot(...)), power)` specular pattern (so we know if fixing it unblocks interiors broadly)? Any other `pow`/`log`/`rsq`/`/`-of-zero hazards the recompile lane introduces vs the D3D9 asm originals?

## What we need back
A confirmed root-cause statement, the D3D9-vs-D3D11 path divergence with file:line cites, and a ranked minimal-fix recommendation (with the exact edit + where) that matches D3D9 output and covers the shader family — plus the cheapest way to verify (e.g., which cbuffer field to read, or a one-line shader guard to test).
