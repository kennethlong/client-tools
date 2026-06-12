# CONSULT-37 — Dark walls/buildings in D3D11 (lighting-math / spec angle)

Read the actual source in `D:\Code\swg-client-v2`; cite file:line. Reason from the lighting equations, not from my framing.

## Repo
SWG client. D3D11 renderer plugin (`src/engine/client/application/Direct3d11/`) ported from D3D9 (`src/engine/client/application/Direct3d9/`). D3D9 = reference/spec.

## Observed facts
- In D3D11, some building/wall geometry renders **much too dark — but NOT pure black** (some light still reaches it). Same geometry is correct in D3D9.
- Some dark draws get an empty dynamic light list (size 0) in D3D11; correct objects get 3 lights (ambient+sun+fill).
- D3D9 "precalculated vertex lighting" path: `Direct3d9_StaticShaderData.cpp:776,955` → `setFullAmbientOn(containsPrecalculatedVertexLighting())`; on → `ms_fullAmbient=(1,1,1,0)` seeded as ambient (`Direct3d9_LightManager.cpp:404`).
- It is believed the D3D11 asset world VS input layout is position/normal/uv/uv/tangent with **no COLOR input**, whereas the D3D9 asset world VS reads vertex COLOR0. VERIFY this independently — do not take it on faith.

## Your task (math/spec angle — ONLY this)
1. Derive the **exact final-color equation** the D3D9 world VS/PS produces for: (a) a normal dynamic-lit surface, and (b) a precalculated-vertex-lit surface. Show the terms: ambient × ?, baked vertex color, diffuse Σ(N·L)·color, dot3/bump, hemispheric back/tangent. Where does the baked vertex color enter and what does full-ambient (1,1,1,0) multiply against?
2. Given "dark but not black": which term is **present** in D3D11 (so it's not zero) and which term is **missing/attenuated** to produce the darkening? Reason about whether the deficit is the ambient floor, the directional diffuse sum, the baked vertex color, or the hemispheric back/tangent contribution. Use the register/equation structure to argue, not screenshots.
3. **Is a VS COLOR input strictly necessary** to reproduce the D3D9 appearance, or is there an alternate (e.g. the baked color folded elsewhere, or the darkness coming entirely from a missing dynamic-light term that has nothing to do with baked color)? Give the minimal-correct D3D11 fix per case.
4. Sanity-check the numbers: D3D9 ambient floor ~0.13; full-ambient=1.0. What output ratio (D3D11/D3D9) would each hypothesis predict, and which matches "dark but clearly not black"?

Output: the equations, the term-by-term present/missing analysis, and a ranked set of candidate root causes with the predicted brightness ratio for each. Correct any wrong assumption in this brief.
