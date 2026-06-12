# CONSULT-38 — D3D11 dot3/hemispheric PS constants are ZERO for a precalc bump batch (TRACE angle)

Read the actual source in `D:\Code\swg-client-v2`; cite file:line for every claim. You are a
call-graph tracer. Do NOT anchor on prior framing — if the code contradicts the brief, say so.

## Repo
SWG client. D3D11 renderer plugin (`src/engine/client/application/Direct3d11/`) ported from
D3D9 (`src/engine/client/application/Direct3d9/`). D3D9 is the reference.

## LOCKED GROUND TRUTH (measured on the GPU via RenderDoc — axioms, do NOT contradict or re-derive)
Two D3D11 captures: Capture25 = a bump-mapped building wall renders PURE BLACK filling the
screen; Capture26 = same wall, camera rotated 15°, now renders dim-lit (~0.2). From debug_pixel/
debug_vertex/shader disasm:
1. The black geometry's shaders: **VS ResourceId 10950, PS 10952** — a per-pixel dot3/bump
   world shader. VS inputs = position, normal, uv, uv, uv, **TANGENT** (v5, unit-length +
   handedness sign). **NO COLOR0 input.** PS does:
   `o0 = (v1 + packedRegister3 + packedRegister1 ± dot3(v4,normalMap)·packedRegister3/4) ×
   diffuseMap × detailMap`, where v4 = tangent-space light dir, packedRegisterN come from PS
   cb0 `SwgVertexConstants` (400 bytes).
2. The VS per-vertex base (PS input **v1** = dynamic diffuse + emissive + `lightData.ambient`
   (c16)) is **exactly (0,0,0,0) in BOTH the black and the lit frame.** So the c16 / full-ambient
   floor is NOT the variable. **The CONSULT-37 "missing setFullAmbientOn / seed c16=(1,1,1,0)"
   story is FALSIFIED as the primary cause — do not propose it as the fix.**
3. The ONLY thing that flips black→lit is the **PS dot3/hemispheric constants
   packedRegister1/3/4**: exactly 0 in the black frame, ≈ +0.49 / −0.20 in the lit frame.
4. In the black frame an ENTIRE batch sharing VS10950/PS10952 is uniformly zero-dot3 (draws
   3465,3563,3661,3737,3931,6491,6539); meanwhile terrain and a DIFFERENT-shader object (5543,
   VS2214/PS2217) are LIT in the SAME frame. Lights reach the frame; this one template batch
   gets a zeroed dot3 snapshot.
5. D3D9 renders this same geometry correctly.

## Your task (TRACE — only this)
Trace the COMPLETE fill chain for the dot3 + extended/hemispheric vertex-shader constants
(engine register map: dot3 = c40–c43; extended/hemispheric back+tangent = c60–c63) for a single
static-shader world draw, in BOTH renderers, and mark where D3D11 leaves them zero that D3D9 does not.

1. D3D11: walk `Direct3d11_LightManager::setLights` → `s_pixelDot3State` (note where `.valid`
   is set false on an empty list) → `Direct3d11_StateCache::composeSlot0Shadow` → the PS cb0
   `SwgVertexConstants` fill. Exactly which fields land in packedRegister1/3/4, and under what
   condition are they zero? Is the dot3/hemispheric data gated on `d3.valid` / a non-empty list?
2. D3D9: walk the equivalent — `Direct3d9_LightManager` dot3 + `_vsps_setExtendedLightData`
   (or whatever fills c40–c43 / c60–c63), and `setFullAmbientOn`. For a PRECALC draw with an
   EMPTY dynamic light list, does D3D9 still write nonzero dot3 direction / back / tangent
   constants? From WHERE (which getter / static)?
3. The divergence: for an empty-list precalc dot3 draw, what does D3D9 put in c40–c43 / c60–c63
   vs what D3D11 puts (zero)? Name the precise missing write(s) in D3D11.
4. Find the shader template behind a dot3/bump world VS that declares pos/normal/uv/uv/uv/tangent
   with no color, and whether `containsPrecalculatedVertexLighting()` is true for it.

Output: caller→callee chains (file:line) for both renderers from light selection → PS/VS dot3
constant upload, the divergence points marked, and the exact D3D11 missing write. Flag any
inaccuracy you find — except the 5 locked facts above, which are measured truth.
