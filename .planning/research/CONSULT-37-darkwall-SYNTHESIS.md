# CONSULT-37 — Dark walls/buildings in D3D11 — 4-way SYNTHESIS

Date: 2026-06-08. Crew: Cursor (register-map / detail), Codex (light-assignment / trace),
Sonnet (falsify-frame / lateral), Opus (precalc-math / math). All four read live source.

## Bottom line

For a **precalc-vertex-lit building/interior draw**, `listSz=0` is **correct and by design** —
not a bug, not a race. The single register-level divergence between D3D9 and D3D11 is **c16**
(D3D9 seeds full-ambient `(1,1,1,0)`; D3D11 seeds `(0,0,0,0)` — it has no full-ambient hook
anywhere). **Whether that c16 gap is the cause of the darkness depends entirely on which
vertex shader the dark geometry binds**, and that is the one runtime fact not yet pinned —
the RenderDoc capture resolves it.

## What all four agree on (high confidence)

1. **`listSz=0` on precalc draws is intentional.** Codex found the proof Sonnet was missing:
   interior cell lights explicitly opt out of precalc geometry —
   `CellObject.cpp:153-155` `setAffectsShadersWithPrecalculatedVertexLighting(false)` on the
   diffuse light, `:158-163` `setAffectsShadersWithoutPrecalculatedVertexLighting(false)` on
   the specular companion. The sorter keeps two separate bitsets
   (`ShaderPrimitiveSorter.cpp:1080-1106`, routed by
   `staticShader.containsPrecalculatedVertexLighting()` at `:1198-1201`). So precalc
   buildings genuinely receive zero dynamic lights, by design — **Sonnet's popCell/cell-stack
   race hypothesis is falsified.** D3D9 does NOT keep those lights either; it compensates
   elsewhere.

2. **The full-ambient framing in the brief was per-family imprecise.** D3D9 turns on
   `setFullAmbientOn(containsPrecalculatedVertexLighting())`
   (`Direct3d9_StaticShaderData.cpp:776,955`) → `ms_fullAmbient=(1,1,1,0)`
   (`Direct3d9_LightManager.h:201-215`) → seeded into `selectLights` ambient (`:404`).
   D3D11 has **no equivalent** (grep over `Direct3d11/**` finds zero `fullAmbient` /
   `setFullAmbientOn` / `containsPrecalculatedVertexLighting`).

3. **"D3D11 world VS has no COLOR input" is FALSE as a blanket claim.** The cell VS family
   `tfcl.vsh:32` / `tfcsl.vsh:31` declares `float4 color0 : COLOR0 : register(v5)`. The
   "no COLOR input" disasm `46c05cc8` (2026-06-08 handoff) is a different family (bump/dot3,
   pos/normal/uv/uv/tangent).

4. **The discriminating experiment is the same for all four:** one RenderDoc capture of a
   dark building wall — read the bound VS identity, the COLOR0 vertex output, and cbuffer b0
   c16. No code change, ~2 min with the installed MCP.

## The decisive split (Cursor + Opus + Sonnet) — depends on VS family

For a `listSz=0` precalc draw, **c16 is the only light-register that differs**; everything
else (c17–c43 directionals, c60–c63 hemispheric) is **zero on both paths**, and the `k0=1`
point-attenuation guards match (`Direct3d9_LightManager.cpp:685,717` ↔
`Direct3d11_StateCache.cpp:444-448`). So the fix is determined by which VS reads what:

| VS family | Base/floor term | c16 gap fatal? | D3D9-parity fix |
|---|---|---|---|
| **HLSL `a_simple`** (`a_simple_vs20_for_ps20.vsh:40`) | reads **c16** `lightData.ambient` | **YES** — D3D9 ≈ (1,1,1)+emissive, D3D11 ≈ emissive only | **Port `setFullAmbientOn`** — seed c16=(1,1,1,0) for precalc shaders |
| **`tfcl`/`tfcsl`** (cell, asm + //hlsl override) | reads **vColor0** (`c_ambient.inc:16`) | **NO** — c16 inert | **VB COLOR0 stream / input layout** — ensure `format.hasColor0()` so v5 isn't phantom-zeroed (`Direct3d11_VertexBufferDescriptorMap.cpp:241-242,288-290`) |

D3D9's full-ambient is what lets the **baked vertex color survive** instead of being crushed
by missing dynamic lights (Codex). For `a_simple` that survival is via c16; for `tfcl` it's
via vColor0 carried in the VB.

## Two distinct symptoms = two distinct bugs (Opus)

- **"Black" buildings** (`listSz=0`, vColor0/c16 = 0): the only non-emissive term is zeroed →
  `tex × saturate(0)` ≈ black. Predicted ratio 0.0.
- **"Dark but not black" shadow walls** (`listSz=3`): the //hlsl reauthoring **dropped the
  hemispheric back/tangent term** — `diffuse.inc:20-24` (asm) adds `tangentColor` +
  tangent/back modulation; `inc_functions.inc:166-221` (live //hlsl) omits it. Shadow faces
  get only `Σ max(N·L,0)` ≈ 0 with no back-fill. Predicted ratio ~0.6–0.7. **Fix:** add the
  hemispheric term to `calculateDiffuseLighting` using already-fed `extendedLightData`
  (c60-63, `StateCache.cpp:411-414`).

## Falsified / corrected

- **Sonnet's popCell-race** — falsified by Codex's `CellObject.cpp:153-163` opt-out.
- **"empty list alone fully explains darkness"** — overstated (all three): D3D9 still has a
  large c16 floor with list=0; empty list is the trigger, missing full-ambient is the gap.
- **"force (1,1,1) into c16 = complete fix"** — incomplete (the reverted experiment): it
  flat-whites tfcl faces that read vColor0, not c16. Per-family fix required.
- **Brief's `selectLights:404` seeds (1,1,1,0)** — minor: `:404` copies `ms_fullAmbient`; the
  literal write is `Direct3d9_LightManager.h:201-215` via `StaticShaderData.cpp:955` (Codex).
- **`a_simple` VS does read c16** — so Opus/Sonnet's "c16 is a red herring" is true only for
  the tfcl family (Cursor).

## Secondary divergences (Cursor, lower priority)

- Point-light positions/colors not mapped into cb0 (c24–c39) — irrelevant for outdoor walls,
  matters for point-lit interiors.
- Dot3 alpha-fade `.w` channels (c42.a/c43.a) not mirrored.
- `c16.w` = 0 (D3D9) vs forced 1.0 (D3D11).
- No `setObeysLightScale` in D3D11 (always scaled) — uniform dimming, secondary.

## RenderDoc VERDICT (Capture25 black vs Capture26 +15° lit) — REFRAMES the above

Drove the MCP on both captures. Center pixel resolves to the SAME wall triangle in both
(RT 323, mesh 2376 indices, primitiveId 62 — confirmed wall, not a window).

**The dark wall's VS is the dot3/hemispheric BUMP family, NOT tfcl and NOT a_simple-cell.**
VS `ResourceId::10950` inputs = position/normal/uv/uv/uv/**tangent** (v5 = unit tangent +
handedness sign; lines 90-92 build the binormal via cross product). **No COLOR0 input.** It
DOES read `lightData.ambient.ambientColor` (c16) at VS line 86 as its per-vertex floor. PS
`10952` is per-pixel dot3: `o0 = (v1 + packedReg3 + packedReg1 ± dot3·packedReg3/4) ×
diffuseMap × detailMap`.

| Probe (pixel 800,450) | Capture25 (black) | Capture26 (+15°, lit) |
|---|---|---|
| VS base `v1` = dynamic-diffuse + emissive + **c16 ambient** | **(0,0,0,0)** | **(0,0,0,0)** — STILL zero |
| PS hemispheric consts `packedRegister1` / `3` | **0 / 0** | **≈ +0.49 / −0.20** |
| PS output `o0` | **(0,0,0,0)** | **(0.197, 0.201, 0.219)** |

**Two findings that revise the consult ranking:**

1. **The c16 full-ambient gap is real but is NOT the cause of the blackness.** `v1` (which
   carries c16 ambient) is zero in BOTH frames, yet Capture26 renders lit. Porting
   `setFullAmbientOn` would only add a constant floor — it does not explain black-vs-lit.
   Demoted from "the fix" to "a secondary floor fix."

2. **The deciding variable is the PS dot3/hemispheric snapshot (`packedRegister1/3/4`),
   which flips exactly-zero → nonzero with a 15° camera rotation.** Those constants come from
   `s_pixelDot3State` (`Direct3d11_LightManager.cpp:85` sets `.valid=false` on empty list) →
   `composeSlot0Shadow`. So the black wall is **view-dependent light-snapshot assignment** —
   the draw intermittently receives an empty/invalid dot3 snapshot. This is **Sonnet's
   Root-cause-A/C** (empty-list → snapshot zeroed/stale), which the other three deprioritized.

**Consensus-trap flagged** (per `feedback_renderdoc_d3d9_vs_d3d11_is_the_diagnostic`): all
four consults ranked the precalc/full-ambient/c16 framing #1; the capture shows the deciding
term is the dot3 snapshot, view-dependent. Correlated agreement ≠ correctness.

**Open question the captures raise:** WHY does this wall draw get a valid dot3 snapshot after
a 15° rotation but not before? Two candidates, not yet distinguished:
(a) the wall's cell/portal lights activate only when DPVS brings a portal into view
    (deterministic, camera-driven), or
(b) `s_pixelDot3State` is global state set at `setLights` time and consumed at
    `composeSlot0Shadow` time — a neighbor draw's snapshot leaks in one frame and a zeroed one
    leaks in the other (draw-order/staleness race).
Distinguish by logging `listSz` and `s_pixelDot3State.valid` for THIS wall draw across frames.

### Capture25 grid scan (a-vs-b) — verdict: BATCH/shader-family level

Grid-sampled 8 points + identified each winning draw:

| Draw(s) | Shader | dot3 consts | Output |
|---|---|---|---|
| 3465, 3563, 3661 | **VS 10950 / PS 10952** (dot3-bump) | **ZERO** | **pure (0,0,0)** — dominant visible wall, ~7/8 of screen |
| 3931 | **VS 10953** / PS 10952 (sibling VS, same dot3 PS) | ZERO | black (behind 3465 in samples) |
| 6491, 6539 | VS 2223 / PS 11187 (different shader) | — | black-ish but occluded; not confirmed visible |
| 463 (terrain) | terrain shader | populated | lit ~0.23 |
| 5543 (foreground obj) | VS 2214 / PS 2217 (different) | populated | lit ~0.20 (v1 also =0; rides dot3 consts) |

**Multiple distinct bump shaders go dark together (VS10950 + VS10953 → PS10952), while terrain
and a different-shader object are lit in the SAME frame.** So it is NOT one shader template and
NOT random-per-draw — it is a **cell/region-level** dot3-light starvation: a whole region of
bump geometry gets a zeroed dot3 snapshot regardless of which bump shader, while other cells'
geometry is lit. This points at **Codex's precalc-routing / cell-light mechanism**
(`ShaderPrimitiveSorter` → separate `ms_lightsAffectingShadersWithPrecalculatedVertexLighting`
bucket that interior cell lights opt out of, `CellObject.cpp:153-163`) operating at cell scope.
The RenderDoc correction stands: the D3D11 SYMPTOM is zeroed **dot3/hemispheric PS constants**,
not the c16/full-ambient floor (zero in both the black and lit frames).

**Still unexplained:** the same VS10950 wall is lit in Capture26 (+15°). Camera-dependence of
a precalc batch's lighting → likely DPVS/portal deciding which cell's lights are active for the
snapshot, or an LOD/shader-tier swap between views. THIS is the next thing to pin (engine-side
log of listSz + dot3.valid for a VS10950 draw across the two camera angles).

---

# CONSULT-38 — dot3-fill four-way result (de-anchored re-task)

Re-ran the crew with the RenderDoc facts LOCKED and the c16/full-ambient story marked
falsified-as-primary. No false consensus this time — strong agreement on mechanism, real split
on the fix.

## CONVERGENCE — the mechanism (all four agree, cited)
- `packedRegister1/3/4` = PS b0 c1/c3/c4 = `dot3LightDiffuseColor` / `dot3LightTangentMinusDiffuseColor`
  / `dot3LightTangentMinusBackColor` (`Direct3d11_LegacyPSConstants.h:54-68`,
  `Direct3d9_PixelShaderConstantRegisters.h:23-33`). The PS const term `c1 + c3 = tangentColor`
  (hemispheric sky color) is the entire visible floor; dot3 only modulates around it.
- These come ONLY from the **sun directional** (`parallelSpecular[0]` / `psSlot`), NEVER from
  ambient/full-ambient.
- D3D11 zeroes them via `if (d3.valid)` gates (`Direct3d11_StaticShaderData.cpp:945`,
  `Direct3d11_StateCache.cpp:382/405/410`). `d3.valid` is reset false at the top of EVERY
  `setLights` (`Direct3d11_LightManager.cpp:85`) and set true only if a T_parallel is in THAT
  call's list (`:176/:196`). Building batch's per-draw list had no directional → valid=false →
  c1/c3/c4 keep zero-init → black. Matches measured facts #3/#4.
- **Camera-dependence = the snapshot is global/per-call state** whose value is whatever the last
  `setLights` left. Sonnet: the world directional is a DPVS Region-of-Influence object, so
  whether `enableLight()` runs before/after the building's `add()` flips with frustum traversal
  order on a 15° turn → the building's `lightBitSet` is all-zeros in Capture25, populated in 26.

## KEY CONVERGENCE (codex + opus + cursor traces): full-ambient does NOT seed dot3
D3D9 ALSO zeroes dot3 c1/c3/c4 for a truly empty `parallelSpecular[0]`
(`Direct3d9_LightManager.cpp:610-624`). `setFullAmbientOn` only sets `ms_fullAmbient=(1,1,1,0)`
→ VS c16 ambient — which the bump PS reads only via VS `v1`, and v1=0 in BOTH frames.
**Therefore: since D3D9 renders this wall LIT, D3D9's `parallelSpecular[0]` is NOT empty for it —
D3D9 gets the sun. The "empty dynamic list" is a D3D11-only condition.** (codex closing; opus §2)

## THE SPLIT — the fix (reduces to ONE empirical question)
- **Opus + Codex (persist-the-sun):** the wall SHOULD get the sun; D3D9's PS constants are
  STICKY (register file persists across draws) so the frame's sun survives the building's
  empty-list draw; D3D11 wrongly invalidates the dot3 snapshot per-call. **Fix = persist
  `s_pixelDot3State` across draws (don't reset valid per `setLights`; only UPDATE when a
  directional is present) and/or drop the `d3.valid` write-gate.** A data-lifetime fix, NOT a
  shader/equation change, NOT ambient. (opus §3, "fix is in the LIFETIME of s_pixelDot3State")
- **Cursor + Sonnet (full-ambient hook):** add a `setFullAmbientOn`-equivalent for precalc
  passes (sonnet: force `s_pixelDot3State.ambient=(1,1,1,1)`). BUT their own traces show
  full-ambient only feeds c16→v1, which yields FLAT full-bright texture `(1,1,1)×tex` — NO dot3
  shading. Sonnet's own DPVS-staleness mechanism actually supports persistence, not ambient.

## The discriminator (decides which camp is right)
Does the correct **D3D9** wall show **directional/hemispheric shading** (lit/shadow sides,
tangent-sky floor — matches Capture26's look) or **flat full-bright texture** (uniform, no
shading)?
- Hemispheric shading ⇒ **Opus/Codex persist-the-sun is the fix** (full-ambient would over-flatten it).
- Flat full-bright ⇒ Cursor/Sonnet full-ambient is the fix.

Weight of evidence favors **persist-the-sun**: 3 of 4 traces prove full-ambient can't seed dot3;
D3D9 zeroes dot3 on empty list (so D3D9 must have a real sun here); a dot3 bump shader is
inherently directional. Confirm with a gl05 D3D9 RenderDoc capture of the same interior (read
its c1/c3/c4 + c16 for the equivalent draw), or by eyeballing whether D3D9 walls are shaded vs
flat. Then the fix is the `s_pixelDot3State` lifetime change (no Koogie/shader edits).

## Next action (superseded by RenderDoc verdict — see above)

RenderDoc on one dark building wall:
1. `debug_vertex` → COLOR0 output.
2. cbuffer b0 → c16 (`lightData[0]`), c20–c23 (`lightData[4..7]`).
3. Note the bound VS source-tag (a_simple vs tfcl) and whether a COLOR stream is bound.

| COLOR0 | lights | VS | Verdict → fix |
|---|---|---|---|
| ≈0 | c16 zero | `a_simple` | port full-ambient seed (c16) |
| ≈0 | c16 zero, `hasColor0=false` | `tfcl`/`tfcsl` | VB COLOR0 stream / input layout |
| ≈0.13 nonzero | all zero | either | vColor0 flows, lights not fed — lighting-feed/cell issue |
| ≈0 | parallel nonzero | either | normal-space / N·L mismatch (Sonnet D) |
