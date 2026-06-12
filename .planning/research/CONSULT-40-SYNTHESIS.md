# CONSULT-40 SYNTHESIS — D3D11 character FACE translucent/washed (bluish-white), BODY correct

Date 2026-06-08. Branch `koogie-msvc-cpp20-base`. 4-way crew: Codex (trace), Cursor (detail),
Opus (numeric), Sonnet (lateral/refute). All read source independently. RenderDoc Capture33 facts
were locked as ground truth; D3D9 reasoned from source (RenderDoc can't capture gl05).

## VERDICT: §5 hypothesis CONFIRMED 4/4 — with one sufficiency caveat from the refuter.

### Root cause (unanimous, file:line verified)
The asset dot3 bump PS (ResourceId::2224) computes its **output alpha** as:
```
o0.w = (c1.a > 0) ? c2.a : luminanceBloom        ; PS 2224 disasm lines 42-43
```
- `c1` = b0 `PSCR_dot3LightDiffuseColor` (offset 16), `c2` = `PSCR_dot3LightSpecularColor` (offset 32)
  — verified via `Direct3d11_LegacyPSConstants.h:54-58` (Cursor, Codex, Opus, Sonnet all concur).
- **D3D9** packs these `.a` channels with the alpha-fade control (`Direct3d9_LightManager.cpp:582,584`):
  `diffuseColor.a = alphaFadeOpacityEnabled` (**0 when not fading**), `specularColor.a = alphaFadeOpacity`.
  So a non-fading object → `c1.a = 0` → PS takes the **luminance** branch → opaque.
- **D3D11** instead copied the **light's own** alphas (`Direct3d11_LightManager.cpp:193-194`):
  `diffuseColor.a = sun diffuse.a`, `specularColor.a = sun specular.a`, carried verbatim into the
  per-draw b0 staging at `Direct3d11_StaticShaderData.cpp:964-965`.
- The sun's `diffuse.a == 1.0` **exactly** (Sonnet: `PackedRgb::convert(alpha=1.f)` default,
  `GroundEnvironment.cpp:997-998`) → `c1.a > 0` **always** → fade branch fires on **every** dot3 draw.
- The emitted `c2.a == sun specular.a == mainSpecularColorScale × 1.0 ≈ 0.60` (Codex: `GroundEnviron-
  ment.cpp:1005/1071` → `Light.h:386` scales the whole VectorArgb incl. alpha; Sonnet: it's the
  time-of-day specular ramp, ~0.6 at midday). → measured `o0.w = 0.60`.

### Why face wrong / body right (the signature, not a contradiction — Opus)
The body uses the **vertex-lit** shader (PS 10948, `o0 = v1`, alpha 1.0) which **never reads c1/c2**.
Only **dot3-bump-shaded** surfaces (face, bump walls) read the leaked registers. Same light snapshot
→ opaque body + α=0.60 face is exactly predicted.

### Numeric proof the 0.60 is the fade branch, not bloom (Opus)
Measured RGB (0.506,0.502,0.639) → luminance = 0.518, bloom variant ×0.65 = 0.337. Measured α=0.60
matches **neither** → line 43 selected `c2.a` → line 42 predicate `c1.a>0` was TRUE. Airtight.

### The misrouted real fade (Codex, Cursor, Opus)
`Direct3d11_StateCache::setAlphaFadeOpacity` wrote the genuine opacity to b0 **offset 0 (c0)** — a
slot the PS never reads for alpha, AND which holds the dot3 light direction/specular power (so the
write clobbered c0; the CONSULT-39 `invalidateApplyCache` was masking that). The real fade value was
dropped; the light's specular.a took over.

## SONNET'S SUFFICIENCY CAVEAT (the productive split — heed it on test)
The packing fix changes non-fading α from 0.60 → **luminance (~0.34)**, still ≠ 1.0. Whether the face
becomes visibly **opaque** depends on the dot3 pass's **blend state** and what reads the RT alpha:
- Blend is asset-driven: `setAlphaBlendEnable(engPass->m_alphaBlendEnable)` (`StaticShaderData.cpp:1506`).
  An opaque skin pass has `m_alphaBlendEnable = false` → α only lands in the **RT alpha channel** (drives
  bloom; and Sonnet's Win11-DWM swapchain-alpha-composite concern), NOT 3D translucency.
- The Cap32 "chairs through her" was the SEPARATE, legit **proximity-fade** (camera point-blank → fade
  path forces blend on). At normal distance there is no fade → blend should be off → the α=0.60 was a
  bloom/RT-alpha artifact, not 3D blending.
- **If after the fix the face still looks washed**, the residual is RT-alpha→bloom (or DWM); next step
  would be ensuring opaque character passes write RT alpha 1.0 or the swapchain uses DXGI_ALPHA_MODE_IGNORE.

## FIX SHIPPED (this session)
1. `Direct3d11_StateCache::setAlphaFadeOpacity` (`Direct3d11_StateCache.cpp:2025`): record
   `enabled`/`opacity` in statics + add `getAlphaFadeEnabled()`/`getAlphaFadeOpacity()`; **removed** the
   c0-clobbering `updatePS(0,...)`; kept `invalidateApplyCache()` (forces the dot3 re-pack — answers
   Opus's apply-cache risk).
2. `Direct3d11_StaticShaderData.cpp:962-968` (per-draw, per Cursor — NOT the per-frame snapshot):
   override `c1.a = getAlphaFadeEnabled()?1:0`, `c2.a = getAlphaFadeOpacity()` on LOCAL copies of
   `d3.diffuseColor/specularColor` (snapshot left intact for the VS lightData path).
3. Header getters in `Direct3d11_StateCache.h`.

Mirrors D3D9 exactly; non-fading dot3 surfaces now take the opaque luminance branch.

## OPEN / FOLLOW-UP (Opus + Sonnet)
- Audit the other three dot3 `.a` channels: `tangentMinusDiffuseColor.a`/`tangentMinusBackColor.a`
  (`Direct3d11_LightManager.cpp:196-198`) inherit the same light-alpha leak; D3D9 puts `bloomEnabled`
  and `ms_currentTime` there (`Direct3d9_LightManager.cpp:599`). Not implicated in this pixel.
- Sonnet's RT-alpha/bloom/DWM residual — verify on the in-game test; scope a follow-up only if needed.
- Decisive confirmation (Opus): with P19_LIGHTDUMP on, c1.a/c2.a at a face draw should now read
  0.0 / opacity instead of 1.0 / 0.6.
