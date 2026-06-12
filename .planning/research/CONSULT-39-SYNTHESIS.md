# CONSULT-39 — Toggling pure-black bump walls in D3D11 — 4-way SYNTHESIS + FIX

Date 2026-06-08. Crew: Codex (trace), Cursor (cbuffer detail), Opus (RenderDoc, dissent),
Sonnet (lateral/skeptic). Question: why do some bump dot3 draws read b0 c1/c3 = 0 (black) while
others — same shader, same frame — read the sun (lit)?

## ROOT CAUSE (confirmed — primary, the toggling pure-black walls)
`Direct3d11_StateCache::setAlphaFadeOpacity` (and `setFog`) do a **16-byte `updatePS(0, …, 16)`**
into the shared PS b0. With **`P19_CBUF_ZERO_TAIL`** (Phase-19 rainbow fix) `updatePS`
**memsets the whole 1152B mapped buffer to 0** then copies 16 bytes — so it **zeroes the b0 dot3
block c1/c3/c4** (offsets 16/48/64). Then `ShaderPrimitiveSorter` calls `setStaticShader`, and
`Direct3d11_StaticShaderData::apply` **cache-hits** (`ms_active==this && ms_activePass==pass`,
`StaticShaderData.cpp:762`) on the **shared-per-template `StaticShader`** singleton
(`StaticShaderTemplate.cpp:169`) — the entire `if(!cacheHit)` body, INCLUDING the dot3 write +
`updatePS(0,staging,400)` at `:1427`, is skipped. The draw then samples b0 c1/c3/c4 = **clean
zero** → bump PS base `v1 + c1 + c3 = 0` → **pure BLACK**.

- **Camera-angle toggle:** sort order decides which mesh of a shared template is FIRST
  (cache miss → apply runs → lit) vs Nth consecutive (cache hit → black). 3° rotation reorders.
- **Why D3D9 is immune:** D3D9 applies dot3 from the DRAW path (`selectLights` in `drawPrimitive`,
  `Direct3d9.cpp:3999-4012`) every draw regardless of static-shader cache, and folds alpha-fade
  into the dot3 payload `.a` — never a separate b0 overwrite (Codex).

## Convergence (3 of 4 + the dissenter's own measurement)
- **Cursor + Codex**: independent source traces → identical mechanism (cache-skip + setAlphaFade
  zero-tail). Codex: the b0 write `:1427` is INSIDE `if(!cacheHit)`; not unconditional.
- **Sonnet**: PS trace confirms c1/c3=0 cause the black; pixel_history confirms draw 4475 is the
  winning fragment (no black overlay). Refuted the overlay + dot3-modulation alternatives.
- **Opus (dissent, resolved)**: argued it's a white-ambient sun (tangent=0) in the snapshot, not
  cache-skip. BUT (a) Opus's own debug_pixel of 4475 measured **c1≈0 AND c3≈0** (individually),
  which contradicts its white-ambient claim (c1=1, c3=−1); (b) a white-ambient draw is NOT pure
  black — its dot3 front term `−front·c3 = +front·diffuse` lights the visible face; only an
  all-zero b0 is pure black on every facing; (c) Opus's "p38b shows d3valid=0 = 0" is moot — that
  probe lives inside `if(!cacheHit)` so it is STRUCTURALLY BLIND to the black (cache-hit) draws.

## SECONDARY bug (Opus's genuine find — separate, NOT the toggle)
p38b has **5556 cache-miss draws with `tangent=(0,0,0)`** (white-ambient light, diffuse=(1,1,1)).
These lose the hemispheric tangent FLOOR → **dim shadow faces, not pure black** (front faces
still dot3-light). This is the known `project_d3d11_black_walls_full_ambient` / brightness
(deficit-B) issue — worth a separate fix in `Direct3d11_LightManager` psSlot selection /
tangentColor source (`:198-218`). Not addressed here.

## Also-found (Sonnet, separate, deficit-B): `Direct3d11_LightManager.cpp:224-238` unconditionally
zeroes the VS `parallelDiffuseColor[0..1]` on every `setLights` — same bug class as the CONSULT-38
snapshot-persistence fix but a register set it didn't cover. Affects v1/brightness, not the toggle.

## FIX (shipped this session — step-1 confirming fix)
`Direct3d11_StaticShaderData::invalidateApplyCache()` (new public static; resets ms_active /
ms_activePass) called at the end of `setAlphaFadeOpacity` and `setFog` — so the next
`setStaticShader` is a cache MISS and re-uploads the full 400B b0 (dot3 restored). Provably
correct; cost = the apply cache is defeated for primitives that set fade/fog (≈ pre-cache /
D3D9-comparable perf). If perf shows up, the cache-PRESERVING follow-up is to route fade/fog
through the legacy-b0 shadow + `flushToB0()` (currently dead code) so the partial write preserves
the dot3 block.

KEEP: CONSULT-38 snapshot-persistence fix (correct). REVERT after verify: P38/P38B/P38C probes.
