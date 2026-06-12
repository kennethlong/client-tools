# CONSULT-37 — Dark walls/buildings render black in D3D11 (register-map angle)

You are a meticulous code reader. Read the actual source in this repo; cite file:line for every claim. Do NOT trust this brief's framing over what the code says — correct me if the evidence below is wrong.

## Repo
SWG game client. A **D3D11 renderer plugin** (`src/engine/client/application/Direct3d11/`) was ported from the original **D3D9 plugin** (`src/engine/client/application/Direct3d9/`). They are meant to be visual-parity. D3D9 is the reference/spec.

## Observed facts (treat as given; verify if you can)
- In **D3D11**, certain building/wall geometry renders **much too dark** — clearly under-lit vs D3D9, but **NOT pure black** (some illumination is reaching it). The **same geometry is correctly lit in D3D9**. Broad outdoor terrain + most interior surfaces now render correctly in D3D11.
- Per-draw logging inside the D3D11 `setLights` shows some of these dark draws receive an **EMPTY dynamic light list (size 0)**. Correctly-lit objects receive **list size 3** (ambient + sun + 1 fill directional). NOTE: "dark but not black" means SOME term (ambient?) is still contributing — do not assume the empty list fully explains the darkness.
- D3D9 reference for "precalculated vertex lighting" geometry:
  - `Direct3d9_StaticShaderData.cpp:776` → `m_fullAmbient = shader.containsPrecalculatedVertexLighting();`
  - `Direct3d9_StaticShaderData.cpp:955` → `Direct3d9_LightManager::setFullAmbientOn(m_fullAmbient);`
  - When on, `ms_fullAmbient = (1,1,1,0)` is seeded into `selectLights` at `Direct3d9_LightManager.cpp:404` (`ms_currentLights.ambient = ms_fullAmbient;`).
- The D3D11 plugin appears to have **no equivalent full-ambient seeding** for precalc shaders.

## Your task (register-map angle — ONLY this angle)
Produce a precise **side-by-side map of every vertex-shader constant register the D3D9 light path writes vs the D3D11 light path writes**, for a single static-shader world draw. Concretely:

1. In D3D9, walk `Direct3d9_LightManager` `selectLights` and the vertex-shader light apply (the function that uploads light constants — find it). List every constant register / cN slot it writes and the value/source for each: ambient, parallel[] diffuse, parallelSpecular, dot3, hemispheric/back/tangent, etc.
2. In D3D11, walk `Direct3d11_LightManager.cpp` `setLights` and `Direct3d11_StateCache.cpp` (`composeSlot0Shadow` and any cbuffer-fill). List every cbuffer register it writes and the source.
3. **Diff them.** Which registers does D3D9 write that D3D11 leaves zero/unwritten, or writes with a different value? Pay attention to the precalc/full-ambient case and to any register the world VS reads for its base/diffuse color term.
4. For the **precalc (empty-light-list) draw specifically**: what does each path leave in the registers the world VS multiplies into final color? Where exactly does the D3D11 output go to zero that D3D9 does not?

Output: a register table (D3D9 value | D3D11 value | divergence) + a ranked list of the divergences most likely to cause black output. Cite file:line throughout. Flag anything in my "Observed facts" you find to be inaccurate.
