# CONSULT-39 — Why do some bump dot3 draws get a zero b0 (black) while others get the sun (lit)? (TRACE)

Read the actual source in `D:\Code\swg-client-v2`; cite file:line. You are a call-graph tracer.
Do NOT anchor on framing — if code contradicts the brief, say so. But the LOCKED facts below are
GPU/probe-measured truth: do not contradict them.

## LOCKED GROUND TRUTH (measured via RenderDoc + in-engine probes — axioms)
The bug: certain bump/dot3 BUILDING meshes in the D3D11 renderer render PURE BLACK, deterministic
by camera angle (rotate ~3–15° toggles black↔lit). Same geometry is correct in D3D9.

1. Black geo = asset bump/dot3 PS (hash `88fafe9d-aa8bc600-203712ff-08ec32e3` + siblings):
   `o0 = (v1 + c3 + c1 ± dot3(v4,normalMap)·c3/c4) × diffuseMap × detailMap`. It reads b0
   `SwgVertexConstants` regs **c1/c3/c4** = `PSCR_dot3LightDiffuseColor` /
   `PSCR_dot3LightTangentMinusDiffuseColor` / `PSCR_dot3LightTangentMinusBackColor`.
2. BLACK draw: b0 **c1/c3 = 0** → o0=0. LIT draw: b0 c1/c3 = sun. The SAME shader is black on
   one mesh and lit on another **in the SAME frame** (RenderDoc Capture30: draw eventId 4475
   black, 6911 lit; siblings 4597/4621 are two 2376-index wall meshes). v1 (VS base) = 0 in both.
3. The dot3 snapshot `Direct3d11_LightManager::s_pixelDot3State` is ALWAYS valid + good sun
   (in-engine [P38B] log: 0/60000 draws had d3valid=0; diffuse ≈ (1.03,0.98,0.85)). NOT the cause.
   A prior CONSULT-38 fix removed the per-`setLights` `s_pixelDot3State.valid=false` reset so the
   snapshot persists (continuous log: 0 `prevValid=1→nowValid=0`). KEEP that fix; it did NOT fix
   the black walls.
4. `Direct3d11_StaticShaderData::apply` (Producer B) — for the legacy b0 (BindPoint==0 &&
   TotalSize==400, which the bump b0 IS): seeds staging from the shared legacy-b0 shadow, then
   `if (d3.valid)` writes c0..c4 from the snapshot (~`StaticShaderData.cpp:945-957,975-980`), then
   `Direct3d11_ConstantBuffer::updatePS(layout.BindPoint, staging, TotalSize)` **unconditionally**
   (~`:1427`), then `writeShadowBack(staging)` (~`:1436`). In-engine [P38C] log: EVERY apply has
   legacyB0=1, d3valid=1, diff nonzero.
5. b0 is ONE shared per-slot GPU buffer updated via `Map(WRITE_DISCARD)`
   (`Direct3d11_ConstantBuffer::updatePS`, ~`:194-206`).
6. THEREFORE: whenever `apply()` runs for a bump draw its b0 gets the good sun → lit. A BLACK
   draw reads zero b0 → it must NOT be getting a Producer-B `apply()` (with the current snapshot)
   applied to the b0 it actually samples: a skipped/cached apply, a different draw-submission
   path, a stale/clobbered shared b0, or a second pass that re-uploads zero.
7. D3D9 renders all of this correctly (reference).

## THE QUESTION
Why do SOME bump dot3 draws get Producer-B's b0 dot3 write (lit) while OTHERS — same shader, same
frame — read b0 c1/c3 = 0 (black)?

## Your task (TRACE — only this)
1. Trace the COMPLETE per-draw submission chain for a world static-shader primitive:
   `ShaderPrimitiveSorter::Phase::draw` → `Graphics::setStaticShader` → D3D11
   `Direct3d11_StateCache::setStaticShader` → `Direct3d11_StaticShaderData::apply` → the b0
   `updatePS` + bind, and finally the `Draw`/`DrawIndexed`. Cite file:line for each hop.
2. Find EVERY place this chain can be SHORT-CIRCUITED so a draw happens WITHOUT a fresh
   Producer-B `apply()` writing b0 for the current object: setStaticShader/state dedup, a
   redundant-shader cache, an early-out in apply, a per-object vs per-batch split, or a separate
   draw path (e.g. optimized/cached ShaderPrimitive, software-skinned, DPVS direct draw) that
   binds the asset PS + b0 but never calls StaticShaderData::apply.
3. Does the b0 dot3 write depend on anything PER-OBJECT that could be missed for some draws (note
   c0 = object-local light direction is recomputed per draw via worldDirectionToObjectLocal —
   does the WHOLE c0..c4 block get rewritten per draw, or could c1..c4 be skipped while c0 is
   redone)? Could two passes of the same primitive upload different b0?
4. In D3D9, what guarantees EVERY such draw gets the dot3 PS constants (PSCR_dot3*) re-applied
   per draw? Map the D3D9 per-draw apply (`applyLights_vertexShader_dot3` etc.) and contrast with
   where D3D11 could skip it.

Output: the caller→callee chain (file:line) for both renderers, every short-circuit/skip point
marked, and the single most likely place a bump draw reaches `Draw` with a zero (un-rewritten)
b0 dot3 block. Flag any inaccuracy except the 7 locked facts.
