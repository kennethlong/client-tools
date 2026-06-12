# CONSULT-37 — Dark walls/buildings render black in D3D11 (light-assignment call-graph angle)

You are a strong repo tracer / call-graph mapper. Read the actual source; cite file:line. Do NOT anchor on this brief's framing — if the code contradicts it, say so.

## Repo
SWG game client. A **D3D11 renderer plugin** (`src/engine/client/application/Direct3d11/`) ported from the original **D3D9 plugin** (`src/engine/client/application/Direct3d9/`). D3D9 is the reference.

## Observed facts (treat as given)
- In D3D11, certain building/wall geometry renders **much too dark — but NOT pure black** (some light still reaches it); the same geometry is correctly lit in D3D9.
- Per-draw logging in the D3D11 `setLights` shows some of these dark draws get an **EMPTY dynamic light list (size 0)**; correctly-lit objects get **list size 3**. "Dark but not black" implies a partial contribution (e.g. ambient) survives — the empty list may not be the whole story.
- D3D9 handles a class of geometry called **precalculated vertex lighting** specially:
  - `Direct3d9_StaticShaderData.cpp:776,955` → `setFullAmbientOn(shader.containsPrecalculatedVertexLighting())`
  - `Direct3d9_LightManager.cpp:404` seeds `ms_fullAmbient=(1,1,1,0)` as the ambient when full-ambient is on.

## Your task (call-graph / submission-path angle — ONLY this)
Trace the **complete chain that decides a building draw gets an empty dynamic light list, and the chain D3D9 uses to still display it.** Specifically:

1. Start at `ShaderPrimitiveSorter` (`src/engine/client/library/clientGraphics/src/shared/ShaderPrimitiveSorter.cpp`). How are primitives bucketed by lighting? Is there a separate pool/bitset for precalculated-vertex-lighting geometry? Where does the light list get attached (or deliberately NOT attached) to a draw?
2. Follow the path from that sorter down into the per-renderer `setLights` / static-shader apply for BOTH D3D9 and D3D11. Where do they diverge?
3. Answer crisply: **Is `listSz=0` for these draws the CORRECT, expected behavior** (i.e., D3D9 also gets zero dynamic lights for them and compensates elsewhere), or is it a D3D11-specific bug where lights are being dropped that D3D9 keeps?
4. If it is expected: map **exactly what D3D9 does to render the geometry with no dynamic lights** (the full-ambient + baked-vertex-color path) and identify every step the D3D11 plugin is missing.
5. Find `StaticShader::containsPrecalculatedVertexLighting()` and how a shader becomes "precalc" — what asset/template attribute drives it.

Output: a call-graph (caller→callee with file:line) for both renderers from sorter → final light/shader apply, the divergence points marked, and a yes/no verdict on whether listSz=0 is correct. Flag any inaccuracy in my framing.
