# Handoff — D3D11 black bump walls FIXED; "faces too light" + VS parallel-wipe are next

Date: 2026-06-08. Branch: `koogie-msvc-cpp20-base`. Fix commit: **90f7fc493**.

## TL;DR
The camera-angle-dependent **pure-black bump walls/buildings** in D3D11 are **FIXED and
runtime-verified** (Mos Eisley starport interior renders fully). Two follow-ups remain, in this
priority order:
1. **"Faces too light"** — flat/over-bright surfaces (the secondary white-ambient / missing
   hemispheric-tangent-floor bug). Visible in `stage/screenshots/screenShot0341.jpg`.
2. **Sonnet's VS parallel-color wipe** — `Direct3d11_LightManager.cpp:224-238` zeroes the VS
   `parallelDiffuseColor[0..1]` on every `setLights`; affects brightness.

## What shipped this session (commit 90f7fc493, 4 files, +30/-3)
**Root cause (the toggling pure-black walls):** `Direct3d11_StateCache::setAlphaFadeOpacity` (and
`setFog`) do a 16-byte `updatePS(0,…,16)` into the shared PS b0. With `P19_CBUF_ZERO_TAIL`,
`updatePS` memsets the whole 1152B mapped buffer to 0 then copies 16B → it **zeroes the b0 dot3
block c1/c3/c4** (offsets 16/48/64). Then `ShaderPrimitiveSorter` calls `setStaticShader` →
`Direct3d11_StaticShaderData::apply` **cache-hits** (`ms_active==this && ms_activePass==pass`,
`StaticShaderData.cpp:762`) on the shared-per-template `StaticShader` singleton
(`StaticShaderTemplate.cpp:169`) → the whole `if(!cacheHit)` body (incl. the dot3 write +
`updatePS(0,staging,400)` at `:1427`) is skipped → bump PS samples c1/c3/c4 = 0 → **pure black**.
Toggle = sort order: FIRST mesh of a template = cache miss → apply runs → lit; Nth consecutive =
cache hit → black. D3D9 immune (applies dot3 from the draw path per draw, not gated on the cache).

**The fix:**
- NEW `Direct3d11_StaticShaderData::invalidateApplyCache()` (resets `ms_active`/`ms_activePass`),
  called at the end of `setAlphaFadeOpacity` and `setFog` (`Direct3d11_StateCache.cpp`). Forces
  the next `setStaticShader` to be a cache MISS → re-uploads the full 400B b0 (dot3 restored).
- KEPT the CONSULT-38 fix in `Direct3d11_LightManager.cpp:~83`: removed the per-`setLights`
  `s_pixelDot3State.valid = false;` so the dot3 snapshot persists (D3D9-sticky parity). Correct
  and runtime-verified; not strictly required for the black walls but a real parity improvement.

**Cost / follow-up:** this defeats the apply() cache for primitives that set fade/fog (≈ pre-cache
/ D3D9-comparable perf). If perf shows up, the cache-PRESERVING alternative is to route fade/fog
through the legacy-b0 **shadow + `flushToB0()`** (currently DEAD CODE in
`Direct3d11_LegacyPSConstants`) so the partial write preserves the dot3 block. Not needed unless
profiling flags it.

**Probes removed, logs deleted.** All P38/P38B/P38C diagnostic blocks reverted; `stage/p38*.log`
deleted. Build is clean (Release `gl11_r.dll` rebuilt 16:09).

## NEXT #1 — "faces too light" (secondary; Opus's find, RenderDoc-confirmed)
Surfaces lit by a **white-ambient directional** (`diffuse=(1,1,1)`, `tangentColor=0`) render
flat/over-bright: the bump-PS base term `v1 + c1 + c3 = v1 + tangentColor` loses its hemispheric
sky-floor, and the dot3 front term still lights the visible face → washed-out, low-contrast (NOT
pure black — front faces light, so it's distinct from the bug just fixed).
- **Evidence:** in the (now-deleted) `p38b_apply.log`, **5556 draws had `tangent=(0,0,0)`** with
  `diffuse=(1,1,1)` (and assorted `(0.244,…)`, `(0.351,…)` etc.) — vs 24683 lit draws with
  `tangent=(0.592,0.569,0.514)`.
- **Likely lever:** `Direct3d11_LightManager::setLights` psSlot selection + `tangentColor` source
  (`:198-218`, esp. `:204` `getScaledDiffuseTangentColor()`). The cell's selected directional is
  the full-white ambient-style light whose tangent color is 0. Check what D3D9
  (`Direct3d9_LightManager::applyLights_vertexShader_dot3`, ~`:777-830`) feeds these same
  precalc/interior objects — does D3D9 derive a nonzero tangent floor, or do these objects not
  take the dot3 path in D3D9? Mirror that.
- Relates to memory `project_d3d11_black_walls_full_ambient` (the white-full-ambient note) — same
  family. The "BUILDINGS black, empty dynamic light list" framing there was the OLD theory; the
  *pure-black* part is now fixed via 90f7fc493, leaving this *flat-bright* part.

## NEXT #2 — Sonnet's VS parallel-diffuse wipe (brightness)
`Direct3d11_LightManager.cpp:224-238` unconditionally zeroes the VS `parallelDiffuseColor[0..1]`
(c20–23) on every `setLights` when the call has no fill/bounce in its list — a sunless popCell
boundary call wipes the VS parallel-diffuse color even while the dot3 PS snapshot survives. Same
bug CLASS as the CONSULT-38 fix (persist last-good instead of zeroing on empty calls), but for the
VS parallel register set the CONSULT-38 fix didn't cover. Affects `v1`/brightness, not the toggle.
Fix = same pattern: only update parallel[] when present; leave the previous values on an
empty/sunless list (or persist like the dot3 snapshot now does).

## Reference / artifacts
- Synthesis: `.planning/research/CONSULT-39-SYNTHESIS.md` (+ CONSULT-37, CONSULT-38 synthesis).
- Consult outputs: `.planning/research/CONSULT-39-*.out` (codex/cursor/opus/sonnet).
- Captures (`stage/`): Capture28 = all-black wall; Capture29 = same wall lit 3° later;
  Capture30 = both lit+black meshes (black draw eventId 4475, lit draw 6911). Bump PS hash
  `88fafe9d-aa8bc600-203712ff-08ec32e3`.
- Memories: `project_d3d11_black_walls_cache_skip_zero_tail` (this fix),
  `project_d3d11_black_walls_full_ambient` (the "faces too light" family),
  `feedback_consultant_crew_protocol` (the de-anchoring re-task that broke the false consensus).

## Process lesson (logged in the crew-protocol memory)
The P38 probes were placed INSIDE the `apply()` `!cacheHit` block, so they never logged a single
black (cache-hit) draw — making "snapshot always valid" look like proof apply always ran. Cursor
caught it; Opus missed it and dissented (a white-ambient theory that was actually the *secondary*
bug). A probe inside a fast-path skip cannot see the slow-path-skipped draws. The productive
2-camp split + the decisive measurement (draw 4475 c1=c3=0, not the white-ambient c1=1/c3=-1)
resolved it — de-anchoring worked.
