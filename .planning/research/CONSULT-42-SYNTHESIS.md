# CONSULT-42 SYNTHESIS — D3D11 NPC face blue, D3D9 pink, identical light data — ROOT CAUSED + FIXED

Date 2026-06-09. 4-way crew: Codex (order-trace), Cursor (sticky detail), Opus (numeric), Sonnet
(lateral/refute). Ground truth: D3D9-vs-D3D11 per-draw light-color dumps are byte-for-byte identical
(NOT a swap, NOT a color bug). UNANIMOUS 4/4.

## ROOT CAUSE (confirmed 4/4)
D3D11's dot3 lighting snapshot `s_pixelDot3State` is a **process-global, sticky** buffer. The CONSULT-38
(`:181` dot3 sun) and CONSULT-39 (`:217` parallel fill) `if(psSlot)` gates persist it across ANY
non-directional `setLights` call. The sorter emits **sunless calls**, so a **PRIOR cell's blue interior
light's `parallel[0]` (c20-c23, the VS main-diffuse fill) survives onto a later object whose own list is
sunless** — the cantina NPC face. Her face reads the stale blue fill -> blue `v1`. **D3D9 cannot do this:**
`Direct3d9::drawPrimitive` calls `selectLights()` PER DRAW (`Direct3d9.cpp:4012`), which NULLs
`ms_currentLights.parallel[0..1]` every call (`Direct3d9_LightManager.cpp:380-391`) and rebuilds -> her
face falls back to warm/ambient -> pink.

### Opus's numeric proof (which state lands on her face)
v1=(0.524,0.537,0.707), B/R=1.349 (blue-dominant).
- **S1 warm sun** (all components B<=R): **mathematically impossible** to produce B>R. Hard proof, not a fit.
- **S2 cyan medical**: err 0.159.
- **S3 gray-sun + blue-fill** (fill0=(0.106,0.159,0.220), B/R=2.07): **err 0.064 — the match.** The blue
  enters EXCLUSIVELY through `parallel[0]`/c20-21 (the blue cell-fill), x forehead N.L.

### Candidate verdicts
- (a) Snapshot stickiness — **CONFIRMED** (4/4). Localized to `parallel[0..1]` (Opus) + dot3 sun.
- (b) Selection cascade — **REFUTED**: identical light list -> same winners; the bug is persistence across
  an empty list, not a different winner.
- (c) Legit blue cell / full-ambient / color path — **REFUTED**: D3D9 renders the same data warm (so not
  legit-blue); Sonnet refuted full-ambient bleed (D3D9 `setFullAmbientOn(false)` resets per-object before
  selectLights); colors identical (not a swap).
- Sonnet secondary: `:243` wrote `s_pixelDot3State.ambient` unconditionally -> empty calls zeroed it
  (darkening). Same persistence-gap class.

## FIX SHIPPED (Direct3d11_LightManager.cpp, this session)
Change the persistence gate from `if (psSlot)` to `if (!lightList.empty())` on all three blocks, so a
**populated** call rebuilds (D3D9 parity: fill, else CLEAR) and only a **truly empty popCell `{}`** call
persists (black-walls/flicker case CONSULT-38/39 preserved):
1. **dot3 sun** (`:181`): populated+sunless now CLEARS the dot3 sun (valid=false) instead of inheriting.
2. **parallel fill** (`:217`): the Opus line -- populated call clears unfilled `parallel[0..1]`.
3. **ambient** (`:243`): gated on `!empty` so empty popCell no longer zeros the snapshot ambient.
No regression: black walls are lit by populated sun-bearing calls (still fill); the transient empty
popCell still persists; flicker fix intact.

## VERIFY
Restart SwgClient_r.exe, cantina NPC face should render warm/pink (matching shoulders + matching D3D9).
Temp probe `blueface-draw.txt` (StaticShaderData per-draw) still in -- post-fix her draw's snapshot should
read cleared/warm, not stuck blue. REMOVE all temp probes (D3D11 blueface-draw, D3D9 blueface-d9) after verify.

## RESIDUAL (if face still blue after fix)
Then her face's preceding call is a TRULY empty `{}` (not populated-sunless) -> the `!empty` gate can't
clear it. Fallback = Codex's draw-time refresh: rebuild the snapshot from the current object's light state
at `Direct3d11_StateCache::applyPreDrawState` before `flushSlot0IfDirty` (:1214), mirroring D3D9's
per-draw selectLights. The blueface-draw probe (with listSize) tells us which.
