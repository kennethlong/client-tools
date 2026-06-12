# CONSULT-38 — Bump-PS lighting math for an empty-list precalc draw (MATH angle)

Read the actual source in `D:\Code\swg-client-v2`; cite file:line. Reason from the lighting
equations and the register layout, not from prior framing. D3D9 = reference/spec.

## LOCKED GROUND TRUTH (measured on the GPU via RenderDoc — axioms, do NOT contradict or re-derive)
Capture25 = bump building wall PURE BLACK; Capture26 = same wall +15° = dim-lit (~0.2).
1. Shaders: **VS 10950 / PS 10952**, per-pixel dot3/bump. VS inputs = position/normal/uv/uv/uv/
   **TANGENT** (v5), **NO COLOR0**. PS (exact, from disasm):
   ```
   r0 = v1 + packedRegister3 + packedRegister1
   nrm = 2*(sample(normalMap) - 0.5)
   d = dot3(v4, nrm);  front = max(d,0);  back = min(d,0)
   r0 = r0 - front*packedRegister3
   r0 = saturate(r0 + back*packedRegister4)
   o0.rgb = r0 * diffuseMap * detailMap ;  o0.w = packedRegister2.w
   ```
   v4 = tangent-space light direction. packedRegisterN from PS cb0 `SwgVertexConstants` (400B).
   VS (key lines): per-vertex diffuse accumulated into o1, then `o1.xyz = r0 +
   lightData.ambient.ambientColor` (c16); o4 = dot3 light dir via `lightData.dot3.direction_o`.
2. PS input **v1** (= VS o1 = dynamic diffuse + emissive + c16 ambient) = **exactly (0,0,0,0) in
   BOTH black and lit frames.** c16/full-ambient is NOT the variable. **CONSULT-37's
   "seed c16=(1,1,1,0)" story is FALSIFIED as primary — do not propose it as the fix.**
3. The ONLY black→lit change is **packedRegister1/3/4** (PS hemispheric/dot3 consts): 0 in black,
   ≈ +0.49 / −0.20 in lit.
4. A whole VS10950/PS10952 batch is uniformly zero-dot3 in the black frame; terrain + a
   different-shader object are lit the same frame.
5. D3D9 renders this geometry correctly.

## Your task (MATH/spec — only this)
1. Map packedRegister1/2/3/4 (PS) and the VS dot3 inputs to the engine light registers
   (c40–c43 dot3, c60–c63 extended/hemispheric, material). What real light/material quantity is
   each packedRegisterN? (e.g. is packedRegister1 = hemispheric backColor, packedRegister3 =
   tangentColor, packedRegister4 = backColor delta?) Cite the struct/register defs.
2. Derive D3D9's bump-PS output for an empty-dynamic-list PRECALC draw term-by-term. With the
   dynamic list empty, which of packedRegister1/3/4 must D3D9 still supply as NONZERO to make the
   wall visible (~0.2)? i.e., what is the D3D9 hemispheric/ambient contribution that lands in the
   PS dot3 constants (NOT in c16) for precalc geometry?
3. Given v1=0 in both frames, the entire visible lighting must come from
   `packedRegister3 + packedRegister1` (the constant term before the dot3 modulation). Compute
   what D3D9 puts there for precalc-empty-list, and state the minimal D3D11 fix: which
   constant(s) must `composeSlot0Shadow` write nonzero, and from which engine getter, to match.
4. Sanity-check: lit-frame packedRegister1≈+0.49, packedRegister3≈−0.20 → const term ≈ +0.29,
   final ≈0.2 after dot3+texture. Does that magnitude match a hemispheric ambient/back term, or a
   single directional? What does D3D9 source say that value should be for precalc?

Output: the packedRegister→engine-register map, the term-by-term D3D9 empty-list-precalc bump
equation, the specific nonzero dot3/hemispheric constant D3D11 is missing, and the minimal fix
with predicted brightness. Correct any wrong assumption except the 5 locked facts.
