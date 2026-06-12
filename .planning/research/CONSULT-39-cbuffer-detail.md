# CONSULT-39 — b0 cbuffer lifetime + setStaticShader dedup: why is the dot3 block zero on some draws? (DETAIL)

Read the actual source in `D:\Code\swg-client-v2`; cite file:line for EVERY claim. Meticulous
code reader. The LOCKED facts below are GPU/probe-measured truth — do not contradict them.

## LOCKED GROUND TRUTH (measured via RenderDoc + in-engine probes — axioms)
D3D11 bump/dot3 BUILDING meshes render PURE BLACK, deterministic by camera angle (3–15° toggles
black↔lit). Correct in D3D9.
1. Bump PS reads b0 `SwgVertexConstants` c1/c3/c4 = `PSCR_dot3LightDiffuseColor` /
   `PSCR_dot3LightTangentMinusDiffuseColor` / `PSCR_dot3LightTangentMinusBackColor`. Black draw:
   b0 c1/c3 = 0. Lit draw: b0 c1/c3 = sun. SAME shader black on one mesh, lit on another in the
   SAME frame (Capture30: draw 4475 black, 6911 lit).
2. dot3 snapshot `s_pixelDot3State` ALWAYS valid + good sun (probe: 0/60000 d3valid=0). Snapshot
   persistence fix is in and works. NOT the cause.
3. `Direct3d11_StaticShaderData::apply` for legacy b0 (bind==0 && size==400) writes c0..c4 from
   the snapshot if `d3.valid`, then `Direct3d11_ConstantBuffer::updatePS(0, staging, size)`
   UNCONDITIONALLY (~`:1427`), then `writeShadowBack` (~`:1436`). Probe [P38C]: every apply
   legacyB0=1 d3valid=1 diff nonzero.
4. b0 is ONE shared per-slot buffer updated via `Map(WRITE_DISCARD)` (`Direct3d11_ConstantBuffer
   ::updatePS` ~`:194-206`). So when apply() runs, b0 = sun → lit. Black draw reads zero → it
   did NOT get a fresh Producer-B b0 write for its object.
5. D3D9 is the reference and renders all correctly.

## THE QUESTION
Same shader, same frame: why do some bump draws read b0 c1/c3 = sun (lit) and others read 0 (black)?

## Your task (DETAIL — only this)
1. `Direct3d11_StateCache::setStaticShader` — does it DEDUP / early-out when the same
   StaticShader (or same shader/pass) was bound on the previous call, skipping
   `StaticShaderData::apply`? Quote the exact guard (cached-pointer compare, dirty flag, pass
   index). If apply is skipped, b0 is NOT re-uploaded — what was the last thing to write b0?
2. The shared-b0 shadow (`Direct3d11_LegacyPSConstants` getShadow/writeShadowBack): trace its
   read (StaticShaderData ~`:909`) and write (~`:1436`). Can the shared shadow ever hold ZERO
   in c1/c3/c4 at the moment a bump draw seeds staging from it AND that draw's path skips the
   `if(d3.valid)` overwrite? Enumerate every writer of the shadow and whether any writes zero.
3. `Direct3d11_ConstantBuffer::updatePS`: is there exactly ONE buffer per PS slot, or a ring?
   After `Map(WRITE_DISCARD)`+Unmap, is the slot re-BOUND every draw, or only on change? Could a
   draw be issued with b0 bound to a buffer version that was last written zero?
4. Multi-pass: for a building primitive with N passes, does EACH pass call apply() (re-uploading
   b0 dot3), or only pass 0? Could a later pass upload a b0 WITHOUT the dot3 c1..c4 (e.g. a
   material-only or alpha pass) that the PS still samples? Capture30 has TWO adjacent 2376-index
   draws (4597,4621) same shader — consistent with 2 passes/instances.
5. Build a table: every code path that can bind the asset bump PS + issue a Draw, and for each:
   does it (a) call StaticShaderData::apply with the current d3, (b) re-upload b0, (c) leave b0
   stale. The path with (a)=no or (b)=no is the bug.

Output: the setStaticShader dedup guard (file:line), the shadow read/write enumeration, the
updatePS buffer-lifetime answer, the per-pass apply answer, and a ranked list of the paths that
leave a bump draw's b0 dot3 block un-rewritten. Flag any inaccuracy except the 5 locked facts.
