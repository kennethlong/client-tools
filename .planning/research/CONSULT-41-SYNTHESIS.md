# CONSULT-41 SYNTHESIS — D3D11 character FACE lit blue (bluish-white); BODY warm/correct

Date 2026-06-08. 4-way crew: Codex (trace), Cursor (detail), Opus (numeric), Sonnet (lateral/lead).
RenderDoc Capture34 (post alpha-fade fix) locked as ground truth; D3D9 reasoned from source.

## UNANIMOUS (4/4)
- The blue is in the **VS vertex lighting `v1`** (= ambient c16 + sun N·L + fill/bounce parallel
  diffuse c20-23 + sky/tangent), NOT created by the PS dot3 pass. Opus: PS hemispheric add
  `r1 − v1 = (0.032,0.032,0.016)` is tiny and *warm*. **Do NOT edit the PS c1/c3/c4** — that shifts
  face and body together and re-breaks the warm shoulder.
- **No register/slot bug.** Cursor + Codex: back/tangent/delta order (c60-63), dot3 c1/c3/c4, ambient
  c16, parallel c20-23 all map 1:1 to D3D9. No tangent↔back swap, no wrong getter. (Cursor correction:
  dot3 diffuse/spec are at c42-43, not c26-27.)
- The split is **normal-dependent / hemispheric**: forehead (faces up/away from sun) → blue; shoulder
  (faces down/side) → warm. Same shader for both.

## THE DIVERGENCE (is it even a bug?) — RESOLVED
Sonnet (lead) correctly flagged that an outdoor face turned toward the blue sky/fill *should* be blue,
and might match D3D9 (i.e. NOT a regression). **Resolved by the user's own D3D9 screenshot: D3D9 renders
Allura's face as warm/natural skin, not bluish-white.** So D3D11 IS bluer than D3D9 for the same scene →
real divergence. (Sonnet had no D3D9 capture, so couldn't use this.)

## WHERE THEY SPLIT (the productive part)
1. **Opus (numeric) — the lever + the trap.** Forehead−shoulder `v1` delta = `(−0.063, +0.075, +0.142)`
   → blue AND **red-deficit**. Up-selective ⇒ directional, not flat ambient (ambient cancels). The warm
   **sun goes to the dot3/specular path (psSlot→c1)** while the VS main-diffuse for up-normals is fed by
   the **cool fill/bounce (c20-23)**. So up-faces are lit by blue fill; the warm sun is under-represented
   on them. **TRAP:** the fix must *inject warm red* (get the sun onto up-faces), not merely suppress
   blue — else the face goes gray-white and a screenshot fools us into a false PASS. Validate by forehead
   R/B → ~1.0 (shoulder is 1.066), not just "B dropped." Fix region: `StateCache.cpp:417-430` (c20-23
   fill) ← `LightManager.cpp:217-236`.
2. **Sonnet (lead) — the structural mechanism.** The D3D11 HLSL dot3 VS **skips the VS-side hemispheric**
   (`if (!dot3)` guard, `stage/shader-rewrite-inc-1-input.txt:~433`) and defers hemispheric to the PS
   (per-pixel, normal-map). D3D9's dot3 path does the hemispheric **in the VS per-vertex** (`diffuse.inc`
   lines 19-24: unconditional `+tangentColor`, blend toward warm `diffuseColor` by `max(N·L,0)`). So in
   D3D9 the warm-sun hemispheric floor is baked into `v1`; in D3D11 it is NOT (PS does a weaker version,
   measured +0.032). **This is the concrete source of the missing warmth Opus measured.**
3. **Codex — obeysLightScale gap (real, but NOT this bug).** D3D11 never calls the per-shader
   `setObeysLightScale()` that D3D9 does at `Direct3d9.cpp:3071` → D3D11 always uses scaled getters.
   BUT default is `true` (config `enableLightScaling`), and only terrain/clouds set it false; NPC chars
   use the default, so both renderers scale identically here. **Worth fixing for terrain/cloud parity,
   not the face.** Fix: add `Direct3d11_LightManager::setObeysLightScale`, wire at `StateCache.cpp:2440`.

## CONVERGED ROOT CAUSE
Opus (numeric) + Sonnet (structural) point at the SAME thing from two directions: **up-facing dot3
surfaces lose the warm-sun hemispheric contribution that D3D9 bakes into the VS, so they're lit by the
cool fill/sky alone → blue, red-deficient.** Cursor (slots correct) and Codex (obeysLightScale is a
red herring for this) bound the search.

## DECISIVE MEASUREMENT (before coding) — agreed by all
Dump at draw 687 (forehead) and 739 (shoulder): PS c1 (sun diffuse — expect WARM R>B), c3/c4; VS c16
(ambient), c20-c21 (fill — expect BLUE B>R), c22-23 (bounce), c61 (tangentColor/sky). Signature
confirming the convergence: **c1 sun WARM + c20-21 fill BLUE + the warm sun absent from up-normal v1.**
Partial RenderDoc decode already shows fill ≈ (0.39,0.38,0.46) blue-dominant. Cleanest full dump = a
small runtime fopen_s log in composeSlot0Shadow at the face draw (RenderDoc VS-trace decode is noisy).

## FIX DIRECTION (post-confirm)
Make the warm sun's diffuse reach up-facing dot3 surfaces as D3D9 does — i.e. restore the **VS-side
hemispheric warm floor for the dot3 path** (Sonnet's `if(!dot3)` skip) OR feed the sun diffuse into the
VS main-diffuse / hemispheric so up-normals get warm, matching `diffuse.inc` + `applyLights_vertexShader`.
NOT a PS edit, NOT a slot swap, NOT obeysLightScale. Validate by forehead R/B≈1.0 (Opus's anti-false-pass
check). Beware regressing the CONSULT-38 sticky parallel/dot3 state (empty setLights) and the black-walls
3-directional feed (`3b977c98a`).
