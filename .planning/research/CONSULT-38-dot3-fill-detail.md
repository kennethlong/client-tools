# CONSULT-38 — Does D3D9 seed the dot3/hemispheric getters for precalc empty-list draws? (DETAIL angle)

Read the actual source in `D:\Code\swg-client-v2`; cite file:line for EVERY claim. You are a
meticulous code reader. Do NOT trust prior framing over the code — correct it if wrong.

## Repo
SWG client. D3D11 plugin (`src/engine/client/application/Direct3d11/`) ported from D3D9
(`src/engine/client/application/Direct3d9/`). D3D9 = reference/spec.

## LOCKED GROUND TRUTH (measured on the GPU via RenderDoc — axioms, do NOT contradict or re-derive)
Two D3D11 captures: Capture25 = bump-mapped building wall renders PURE BLACK; Capture26 = same
wall +15° camera = dim-lit (~0.2).
1. Black geometry shaders: **VS 10950 / PS 10952**, a per-pixel dot3/bump world shader. VS
   inputs = position/normal/uv/uv/uv/**TANGENT** (v5). **NO COLOR0.** PS:
   `o0 = (v1 + packedRegister3 + packedRegister1 ± dot3(v4,normalMap)·packedRegister3/4) ×
   diffuseMap × detailMap`; packedRegisterN from PS cb0 `SwgVertexConstants`.
2. VS per-vertex base (PS input **v1** = dynamic diffuse + emissive + `lightData.ambient` c16)
   = **exactly (0,0,0,0) in BOTH black and lit frames.** The c16/full-ambient floor is NOT the
   variable. **CONSULT-37's "seed c16=(1,1,1,0) / setFullAmbientOn" story is FALSIFIED as the
   primary cause — do not propose it as the fix.**
3. The ONLY thing flipping black→lit is the **PS dot3/hemispheric constants packedRegister1/3/4**:
   exactly 0 (black frame) vs ≈ +0.49 / −0.20 (lit frame).
4. A whole batch sharing VS10950/PS10952 is uniformly zero-dot3 in the black frame, while
   terrain + a different-shader object are lit in the SAME frame.
5. D3D9 renders this geometry correctly.

## Your task (DETAIL — only this)
Pin down, with exact file:line and the literal values, what D3D9 writes into the dot3 and
extended/hemispheric light constants for a PRECALC static-shader draw whose dynamic light list
is EMPTY — and whether D3D11 has any equivalent.

1. Find the D3D9 functions that compute/upload: dot3 `direction_o`, dot3 diffuse/specular, and
   the extended/hemispheric `backColor` / `tangentColor` / deltas (engine c40–c43 and c60–c63).
   Cite the getters (e.g. `getDot3*`, `getBackColor`, `getTangentColor`,
   `_vsps_setExtendedLightData`).
2. CRITICAL: when `setFullAmbientOn(true)` is active (precalc), and the dynamic light list is
   empty, are the dot3 `direction_o` / back / tangent values still NON-ZERO in D3D9? Trace each:
   does full-ambient / the sun getter / a default light populate them even with an empty list?
   If D3D9's dot3 direction is nonzero here while D3D11's is zero, THAT is the parity gap.
3. In D3D11, grep for the equivalent dot3/hemispheric fill in `Direct3d11_LightManager` /
   `Direct3d11_StateCache` (`s_pixelDot3State`, `composeSlot0Shadow`). Is it gated on a
   non-empty list / `d3.valid`? Show the exact gate.
4. Produce a side-by-side table: dot3/hemispheric constant | D3D9 value (empty-list precalc) |
   D3D11 value | divergence.

Output: the getter list (file:line), the empty-list-precalc value of each dot3/hemispheric
constant in D3D9, the D3D11 gate that zeros them, and a ranked list of the divergences most
likely to make the bump PS output zero. Flag any inaccuracy except the 5 locked facts.
