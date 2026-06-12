# CONSULT-40 — D3D11 character FACE renders translucent/washed (bluish-white) while BODY is opaque/correct

Date 2026-06-08. Renderer checkout: `koogie-msvc-cpp20-base` (D:\Code\swg-client-v2).
Plugin: `src/engine/client/application/Direct3d11`. Reference: `Direct3d9` sibling.
RenderDoc CANNOT capture D3D9 (gl05) here — the D3D9 side must be reasoned from source only.

You are one of 4 independent consultants. Read the SOURCE. Reach your own conclusion. Do NOT
assume the hypothesis in §5 is correct — it is ONE candidate, included only so you can confirm or
refute it. A productive disagreement between consultants is the desired outcome, not a consensus.

---

## 1. SYMPTOM (user-observed, in-game, D3D11 / gl11, rasterMajor=11)
- An NPC's **face/head renders washed-out, light, bluish-white**; her **body, shoulders, arms, hands
  render the CORRECT (darker) skin color**.
- Same character, same lighting, same frame.
- **Constant with camera distance** (looks identical at ~3 ft and ~6 ft); **no gradual fade**.
- In an extreme close-up capture, the WHOLE character was visibly **semi-transparent** (furniture
  behind her showed through her torso) — but at normal distance only the head looks wrong.

## 2. LOCKED GROUND TRUTH — RenderDoc Capture33 (D3D11), measured, not inferred
Resolution 1600x900. Scene RT = ResourceId::323 (BGRA8, depth D24S8 ResourceId::46). Final
swapchain = ResourceId::35 (scene composited via CopyResource, then 2D overlays).

**Face pixel (635,200), scene RT 323, winning fragment = draw EID 617:**
- VS = ResourceId::2208 — full `objectWorldCameraProjectionMatrix` transform + per-vertex diffuse/
  specular/point/parallel lighting + dot3 tangent-space output (it is the real 3D character VS).
- PS = ResourceId::2224 — dot3 bump: samples `diffuseMap`(t0)+`normalMap`(t1)+`specularMap`(t2),
  PS cb0 = `SwgVertexConstants` 400 bytes / 9 vars.
- **PS output o0 = (0.5059, 0.5020, 0.6392, 0.6000).**  ← RGB bluish, **alpha = 0.60**.
- depth 0.9582. The fragments that lose the depth tie behind it (EID 9476/12392/12416) are
  WORLD geometry (VS 10946 / PS 10948, untextured vertex-lit), i.e. background correctly occluded —
  NOT a second copy of her face. So there is NO double-draw / depth-write conflict at her face.

**Shoulder/body pixel (660,250 and 560,510), scene RT 323, winning fragment = draw EID 1509:**
- VS = ResourceId::10946 / PS = ResourceId::10948 — **untextured vertex-lit pass-through**
  (PS has NO cbuffer, NO textures; `o0 = v1`, the interpolated baked/Gouraud vertex color).
- **PS output ≈ (0.370, 0.386, 0.437, 1.0).**  ← **alpha = 1.0, opaque**, correct skin.

So: the **dot3-bump-shaded** head (PS 2224) is alpha=0.6; the **vertex-lit** body (PS 10948) is
alpha=1.0. The two use entirely different pixel shaders.

## 3. PS 2224 disassembly (her face) — the alpha-output tail (verbatim)
```
ps_4_0   cb0[8] (SwgVertexConstants); samplers diffuseMap/normalMap/specularMap; t0/t1/t2
... (normal-map unpack, N·L, specular, diffuse sample, etc.) ...
 34: dp3 r0.x, r0.xyzx, l(0.300000, 0.590000, 0.110000, 0.000000)   ; r0.x = luminance(color)
 39: mad o0.xyz, r3.xyzx, r1.xyzx, r2.xyzx                          ; RGB out = diffuse*light + spec
 40: mul r0.y, r0.x, l(0.650000)                                    ; r0.y = luminance * 0.65
 41: movc r0.x, r0.w, r0.y, r0.x          ; r0.w set @32 = (packedRegister3.w > 0). bloom select.
 42: lt  r0.y, l(0), packedRegister1.w    ; r0.y = (packedRegister1.w > 0) ? 1 : 0
 43: movc o0.w, r0.y, packedRegister2.w, r0.x   ; o0.w = (packedRegister1.w>0) ? packedRegister2.w : r0.x
 44: ret
```
Reading line 42-43: **output alpha = (packedRegister1.w > 0) ? packedRegister2.w : luminanceBloom.**
Measured o0.w = 0.60 ⇒ packedRegister1.w > 0 AND packedRegister2.w ≈ 0.60.

`packedRegister1` / `packedRegister2` are named PS-b0 constant registers in the recompiled asset
shader. **TO VERIFY (do not assume):** which b0 offsets they occupy and which engine value writes
each. In D3D9's hand-authored layout these correspond to the dot3 `diffuseColor` and `specularColor`
(see §4). Confirm the D3D11 mapping from the StaticShaderData b0 dot3 write
(`Direct3d11_StaticShaderData.cpp`, the `updatePS(layout.BindPoint, staging, layout.TotalSize)` ~:1414
and the staging fill above it) and the `Direct3d11_PixelDot3State` consumer.

## 4. D3D9 REFERENCE (source only — cannot capture)
`Direct3d9_LightManager::applyLights_vertexShader` (`Direct3d9_LightManager.cpp`):
```
581: lightData.dot3.diffuseColor   = parallelSpecular.diffuseColor;
582: lightData.dot3.diffuseColor.a = alphaFadeAndBloomSettings.r;   // alphaFadeOpacityEnabled in diffuseColor.a
583: lightData.dot3.specularColor  = parallelSpecular.specularColor;
584: lightData.dot3.specularColor.a = alphaFadeAndBloomSettings.a;  // alphaFadeOpacity in specularColor.a
```
`Direct3d9::setAlphaFadeOpacity` (`Direct3d9.cpp:3579`): sets `ms_alphaFadeOpacity.r = enabled?1:0`,
`ms_alphaFadeOpacity.a = opacity`. `getAlphaFadeAndBloomSettings()` returns these (.r = enabled flag,
.a = opacity). So in D3D9, when an object is NOT fading, `diffuseColor.a = 0` → PS line 42-43 takes
the `luminance` branch → opaque. When fading, `diffuseColor.a = 1`, `specularColor.a = opacity`.

## 5. ONE CANDIDATE HYPOTHESIS (confirm OR refute — do not treat as given)
`Direct3d11_LightManager.cpp:192-201` builds the dot3 snapshot:
```
193: s_pixelDot3State.diffuseColor  = XMFLOAT4(diffuse.r, diffuse.g, diffuse.b, diffuse.a);
194: s_pixelDot3State.specularColor = XMFLOAT4(specular.r, specular.g, specular.b, specular.a);
```
i.e. D3D11 writes the **light's own** diffuse.a / specular.a into the dot3 diffuse/specular alpha —
it does NOT pack the alphaFade enabled-flag / opacity the way D3D9 does (§4). Candidate consequence:
the sun's `diffuse.a` (≈1.0, >0) makes PS 2224 always take the fade branch and emit
`specularColor.a` (whatever the sun's specular.a is) as output alpha → spurious α<1 on EVERY
dot3-shaded surface, while vertex-lit surfaces (PS 10948, which never reads these regs) stay opaque.
`Direct3d11_StateCache::setAlphaFadeOpacity` (`Direct3d11_StateCache.cpp:2025`) writes the real
opacity to PS b0 **offset 0** (the fog/first 16 bytes) + `invalidateApplyCache()` — a slot PS 2224
does NOT read, so the genuine fade value may be dropped entirely.

UNRESOLVED even if this is right: where does the measured `packedRegister2.w ≈ 0.60` actually come
from (the sun's specular.a? a different write? the setAlphaFadeOpacity value leaking via a path I
haven't traced?). If the sun's specular.a is NOT ~0.6, this hypothesis is incomplete or wrong.

---

## 6. YOUR ANGLE (each consultant answers ONLY their lettered section)

### A — CODEX (dataflow trace)
Trace, in BOTH renderers, the COMPLETE path of the two values PS 2224 reads at lines 42-43
(packedRegister1.w and packedRegister2.w): from the engine producer → the constant-buffer write →
the PS register. In D3D11, what concrete float lands in each at draw 617's time, and from which line
of which file? In D3D9, what lands there for the same object? State the first line where they diverge.

### B — CURSOR (register/offset detail)
Establish the EXACT b0 PS constant layout for the dot3 bump shader in D3D11: which 16-byte offset is
`packedRegister1`, which is `packedRegister2`, and which source statement fills each `.w`. Cross-map
to the D3D9 `Dot3Data`/`PixelDot3Data` register layout (`Direct3d9_LightManager.cpp` /
`Direct3d9_StateCache` PSCR_dot3* constants). Are the D3D11 `Direct3d11_PixelDot3State` field→register
assignments faithful to D3D9, specifically for the two `.a` channels?

### C — SONNET (lateral / refute)
Independently explain why a dot3-bump-shaded surface is α=0.6 while a vertex-lit surface is opaque,
and actively try to FALSIFY the §5 light-alpha hypothesis. Enumerate and weigh alternative causes:
(a) D3D11 blend-state enabled when it should be off for this pass; (b) a genuine engine-driven
alpha-fade that D3D9 would render the same (i.e. NOT a bug); (c) LOD cross-fade; (d) the diffuse/
specular TEXTURE alpha; (e) material alpha. Which survive the locked facts in §2-3?

### D — OPUS (numeric)
From the PS 2224 disasm (§3) and the measured output o0=(0.506,0.502,0.639,0.60), derive the exact
constraints on packedRegister1.w and packedRegister2.w. Then, assuming the §5 mechanism, what must
the sun light's diffuse.a and specular.a be? Is there any assignment of (light alpha) vs (alphaFade
enabled/opacity) consistent with BOTH a correct opaque body AND the α=0.60 face? Confirm or break the
hypothesis numerically, and state what single measurement (e.g. dumping packedRegister1.w at draw
617) would be decisive.

## 7. DELIVERABLE
Per angle: (1) your finding with file:line evidence; (2) confirm/refute §5 with reasoning; (3) the
minimal D3D9-parity fix you'd make and its exact location; (4) one risk or thing the other angles
might miss. Be willing to disagree.
