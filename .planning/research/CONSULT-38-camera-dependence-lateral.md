# CONSULT-38 — Why does the SAME precalc bump batch flip black→lit on a 15° camera turn? (LATERAL angle)

Read the actual source in `D:\Code\swg-client-v2`; cite file:line. You are the skeptic / lateral
thinker. Your job is the ONE thing the others can't get from static lighting-math: explain the
camera-dependence. D3D9 = reference.

## LOCKED GROUND TRUTH (measured on the GPU via RenderDoc — axioms, do NOT contradict or re-derive)
Capture25 = bump building wall PURE BLACK; Capture26 = the SAME wall mesh + shader, camera
rotated ~15° left, now renders dim-lit (~0.2).
1. Black geometry: **VS 10950 / PS 10952** (per-pixel dot3/bump; pos/normal/uv/uv/uv/TANGENT, no
   COLOR0). Lighting rides on PS dot3/hemispheric constants packedRegister1/3/4 (PS cb0
   `SwgVertexConstants`).
2. VS ambient base (PS input v1, incl. c16) = (0,0,0,0) in BOTH frames — c16 is NOT the variable.
   CONSULT-37's full-ambient/c16 story is FALSIFIED as primary.
3. The black→lit flip = **packedRegister1/3/4 going 0 → ≈+0.49/−0.20**. i.e. in Capture25 this
   batch's dot3 light snapshot is ZERO; in Capture26 (+15°) it is POPULATED.
4. Same frame as the black wall (Capture25): terrain and a DIFFERENT-shader object (5543,
   VS2214/PS2217) are LIT. So lights exist; this VS10950/PS10952 batch alone is starved.
5. D3D9 renders the wall correctly at presumably any angle.

## Your task (LATERAL / camera-dependence — only this)
A precalc-vertex-lit batch's lighting should NOT depend on camera angle. Explain why this one
does. Try to REFUTE each hypothesis; rank by code evidence.

1. **DPVS / portal / cell activation:** does which cell's lights get enabled (and thus which
   dynamic/dot3 snapshot is live when this batch draws) depend on camera view? Trace
   `ShaderPrimitiveSorter` pushCell/popCell + light enable/disable vs the DPVS visit order. Could
   a 15° turn change which portal is traversed, so the batch draws with vs without an active
   light set?
2. **LOD / shader-tier swap:** could the 15° turn select a different LOD child or shader template
   for the same mesh (one that gets a dot3 snapshot, one that doesn't)? Note: GPU shows the SAME
   VS10950/PS10952 in both — so a tier swap would have to land on the same shader. Refute or
   support.
3. **State-leak / snapshot staleness:** `s_pixelDot3State` is global state set at `setLights`
   and consumed at `composeSlot0Shadow`. Could draw ORDER (which neighbor's setLights ran last)
   differ between the two frames so the batch inherits a zeroed snapshot in one and a populated
   one in the other? Is the snapshot re-established per-draw or per-cell?
4. **Sort/visibility ordering:** could the batch be sorted before vs after its own cell's lights
   are enabled, depending on view?
5. Name the single cheapest runtime probe (one log line: for a VS10950 draw, dump cell id,
   `listSz`, `s_pixelDot3State.valid`, draw index) that distinguishes DPVS-cell vs snapshot-leak.

Output: ranked hypotheses with for/against code evidence (file:line), an explicit pick for the
most likely camera-dependence mechanism, and the one discriminating runtime probe. Be contrarian
where the code supports it. Do not re-litigate the 5 locked facts.
