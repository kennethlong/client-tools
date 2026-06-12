# CONSULT-39 — RenderDoc + source: pin the per-draw divergence (SYNTHESIS / math)

Read the actual source in `D:\Code\swg-client-v2` AND drive the RenderDoc MCP on the captures.
Cite file:line and RenderDoc event IDs. The LOCKED facts below are measured truth.

## LOCKED GROUND TRUTH (axioms)
D3D11 bump/dot3 BUILDING meshes render PURE BLACK, deterministic by camera angle. Correct in D3D9.
1. Bump PS `o0 = (v1 + c3 + c1 ± dot3(v4,normal)·c3/c4) × diffuseMap × detailMap`; reads b0
   `SwgVertexConstants` c1/c3/c4 = PSCR_dot3LightDiffuseColor / TangentMinusDiffuse / TangentMinusBack.
   BLACK draw: b0 c1/c3 = 0. LIT draw: b0 c1/c3 = sun. Same shader black+lit in the SAME frame.
2. dot3 snapshot `s_pixelDot3State` ALWAYS valid+good sun (probe 0/60000 d3valid=0); persistence
   fix is in and works. NOT the cause.
3. `Direct3d11_StaticShaderData::apply` writes b0 c0..c4 from the snapshot `if(d3.valid)` then
   `updatePS(0,staging,size)` UNCONDITIONALLY (~`:1427`) then writeShadowBack. b0 is ONE shared
   per-slot buffer via Map(WRITE_DISCARD). So apply()⇒b0=sun⇒lit. Black draw read zero b0 ⇒ no
   fresh Producer-B b0 write for that object.
4. D3D9 is the reference.

## THE QUESTION
Same shader, same frame: why do some bump draws read b0 c1/c3 = sun (lit) and others 0 (black)?

## Captures (use the renderdoc-mcp; open via mcp__renderdoc-mcp__open_capture)
- `D:\Code\swg-client-v2\stage\Capture30.rdc` — ONE frame with BOTH a black and a lit bump mesh.
  * Black draw: **eventId 4475** (939 idx, VS `10820`/PS `10819`), screen px (1450,175), o0=0,
    PS packedReg1/3 (=c1/c3) read 0.
  * Lit draw: **eventId 6911** (456 idx, VS `2220`/PS `11822`), screen px (1545,214), o0≈0.35.
  * Two adjacent 2376-idx wall meshes same shader: **4597, 4621**. Scene RT `ResourceId::323`.
- `Capture28.rdc` = all-black wall (draw 4469); `Capture29.rdc` = same wall lit 3° later (4424).
  Same PS hash `88fafe9d-aa8bc600-203712ff-08ec32e3` confirmed across both.

## Your task (RenderDoc synthesis — only this)
1. In Capture30, take the BLACK bump draw 4475 and a LIT bump draw of the SAME shader if you can
   find one (probe building draws 4451–4621 with debug_pixel/pixel_history to find one lit and
   one black with VS10820/PS10819). For each, read the bound b0 cbuffer c1/c3 (debug_pixel trace
   reads packedReg1/3). Confirm black=0, lit=sun.
2. KEY: use the MCP to inspect the EVENT SEQUENCE between a lit bump draw and the next black bump
   draw. Is the black draw preceded by its own state setup, or is it a bare Draw with no cbuffer
   update between it and a prior draw? (Use list_events / get_resource_usage on the bound b0
   cbuffer resource to see which events WRITE it and whether the black draw is preceded by a
   write.) This tells us if the black draw skipped its apply/updatePS.
3. Find the b0 cbuffer RESOURCE id (get_bindings won't show it; try get_resource_usage on
   candidate small buffers, or get_pipeline_state detail) and dump its write-event history around
   4475. If the last WRITE to that buffer before 4475 was for a DIFFERENT object whose dot3
   was... reason about what value it left.
4. Math check: lit b0 c1≈sun diffuse (1.03,0.98,0.85)? c3 = tangent − diffuse? Confirm the lit
   values are internally the sun hemispheric set, and the black values are a CLEAN zero (not a
   tiny epsilon) — clean zero ⇒ zero-init staging (apply didn't write dot3) vs stale ⇒ some prior
   value. Which is it?
5. Tie to source: given what RenderDoc shows about whether black draws get a b0 write, name the
   exact `Direct3d11_StateCache::setStaticShader` / `StaticShaderData::apply` /
   `ShaderPrimitiveSorter` code path responsible.

WRITE your findings as markdown to
`D:\Code\swg-client-v2\.planning\research\CONSULT-39-renderdoc-synthesis-opus.out` (Write tool),
starting with a one-line bottom-line. Return a 2–3 sentence summary as your final message. Do not
modify source files (read-only). Drive RenderDoc via the renderdoc-mcp tools (open_capture,
list_draws, get_pipeline_state, get_bindings, debug_pixel, pixel_history, get_resource_usage,
list_events, list_resources).
