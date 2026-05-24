# Phase 11: D3D11 Renderer Plugin — SUMMARY

**Closed:** 2026-05-24
**Verdict:** PASS-WITH-DEFERRALS
**Branch:** koogie-msvc-cpp20-base
**Stage:** D:/Code/swg-client-v2/stage/ (gl11_d.dll 1,889,280 bytes at close)

> **Reality vs. plan:** Plan 11-11 was authored before the Plan 11-09.15 visual-parity
> cascade existed. The phase actually closed after a **17-plan + ~45-iteration** arc, not
> the 16-plan reality the Plan 11-11 skeleton anticipated. This SUMMARY reflects what
> shipped, including the 11-09.15 Iter-1..45 visual-parity work and the CODEX+Cursor
> deep-dive consult that converged on the PASS-WITH-DEFERRALS verdict.

---

## Requirements Status

| Requirement | Status | Verification |
|-------------|--------|--------------|
| D3D11-01 plugin scaffold + Gl_api parity | ✓ Complete | Plan 11-02 scaffold (GetApi exports, rasterMajor=11 boots) → all Gl_api slots wired through Plan 11-09.x cascade |
| D3D11-02 resource mgmt (no D3DPOOL_MANAGED / lost-device) | ✓ Complete | D-13 grep gate (0 functional hits) held across all plans; Plan 11-04 resources + cascade smoke |
| D3D11-03 shaders SM5.0+ + SV_POSITION | ✓ Complete | Plan 11-05 D3DCompile pipeline + Plan 11-09.6 vs_4_0 profile bump (CODEX) + Plan 11-09.13 per-VS dynamic PS compile |
| D3D11-04 FFP generator | ✓ Closed-as-DESCOPED | Plan 11-01 D-04a verdict: engine VSes use SM2+ programmable paths exclusively; no FFP-emulation PS generator needed |
| D3D11-05 ground-scene render + visual parity | ✓ Complete (PASS-WITH-DEFERRALS) | Plan 11-09.14 visible-textured-Tatooine + Plan 11-09.15 cascade (transpose + per-pass blend + parity) + SPEC R6 verdict below |
| SPEC R7 DPVS remeasure | DEFERRED to post-Phase-11 | Plan 11-10 deferred: DPVS occlusion math is meaningless until all geometry renders cleanly; current Phase 12 deferrals (asset PS gap) would corrupt the measurement |

---

## SPEC R6 Visual-Parity Verdict

**Overall: PASS-WITH-DEFERRALS.** See `docs/recon/11-d3d11-screenshots/comparison-notes.md`
for the full side-by-side. Basis: the 45-iteration 11-09.15 smoke cascade, this session's
Mos Eisley outdoor smoke (screenShot0008 vs d3d9-tatooine-outdoor.png), and the independent
CODEX + Cursor deep-dive consult convergence.

**Present and correct:** splash, login, char-select avatar, world load, terrain, opaque
world meshes (correct geometry post matrix-transpose Iter-38B + row-scale Iter-42v2),
UI canvas / fonts / panels, Tatooine palette, continuous frame loop with audio.

**Named Phase 12 deferrals (not REJECT-worthy):** see Phase 12 scope below. None are
missing-geometry / glaring-corruption / persistent-z-fighting; all are documented
PS-generation / constant-upload gaps with a clear structural cause.

---

## Wave-by-Wave Summary (actual 17-plan cascade)

| Wave | Plan | Closed | Highlights |
|------|------|--------|------------|
| 1 | 11-01 FFP spike | 2026-05-16 | D-04a verdict: DESCOPED (engine VSes are SM2+ programmable; no FFP PS generator). Bonus: post-build auto-stage fix `266e173b3` |
| 2 | 11-02 Plugin scaffold | 2026-05-16 | gl11_d.dll exports GetApi; rasterMajor=11 boots; D3D9 baseline screenshots captured at 2 anchors |
| 3 | 11-03 Device + swap chain | 2026-05-16 | Clear-to-color MVP; flip-model FLIP_DISCARD 2-buffer; per-frame InfoQueue drain → stage/d3d11-debug.log |
| 4 | 11-04 Resource layer | 2026-05-17 | 6 resource types (texture/RT/static+dynamic VB+IB); ComPtr ownership; DXGI_FORMAT table for all TF_* |
| 5 | 11-05 Shader layer | 2026-05-17 | VS/PS D3DCompile + .cso cache + cbuffer model. **PEXE-rejection caveat surfaced** (D3D9 .psh bytecode rejected by CreatePixelShader → the structural blocker) |
| 6 | 11-06 State + draw dispatch | 2026-05-18 | StateCache + VertexDeclarationMap + LightManager + Metrics; ms_boundSRV[16]/Sampler[16] shadows; 72→29 STUBs |
| 7 | 11-07 Subsystem smoke | 2026-05-19 | 18-iter arc; X4016 global-register wall cleared via CODEX Rule E (#pragma def strip). Visible dark-blue-clear MVP |
| 8 | 11-08 cbuffer wiring | 2026-05-20 | Direct3d11_VertexSlot0CB (1152B) + matrix shadows + primeDefaults full-fill; structural-prereqs-complete |
| 9 | 11-09 visible-magenta | 2026-05-21 | 7-sub-iter CODEX arc; 4 architectural bugs upstream of PS bind cleared; first visible half-quad (magenta fallback) |
| 9.5 | 11-09.5 window-show | 2026-05-21 | ShowWindow(SW_SHOW) parity |
| 9.6 | 11-09.6 vs_4_0 profile (CODEX) | 2026-05-21 | Profile bump vs_4_0_level_9_3 → vs_4_0; X5615 instruction-slot cap cleared |
| 9.7 | 11-09.7 multi-stream VB | 2026-05-21 | Direct3d11_VertexBufferVectorData + multi-stream input-layout cache |
| 9.8 | 11-09.8 reflection-driven layout (CODEX) | 2026-05-21 | D3DReflect + slot-15 zero-buffer; TEXCOORD2 cascade + hair_s40 fix |
| 9.9 | 11-09.9 TF_RGB_888 lock-bridge | 2026-05-21 | 24→32bpp BGR(X\|A) staging bridge; terrain load completes |
| 9.10 | 11-09.10 point-sprite no-ops | 2026-05-22 | 6 STUBs → no-ops; reached post-loading-screen rendering |
| 9.11 | 11-09.11 pix-instrument no-ops | 2026-05-22 | 3 STUBs → no-ops; world fully loaded with audio + continuous frames |
| 9.12 | 11-09.12 dynamic-VB byte-offset | 2026-05-22 | One-line `getOffset()*stride` fix; debug-layer ERRORs 152,870 → 2/session |
| 9.13 | 11-09.13 per-VS dynamic PS | 2026-05-22 | 4-iter arc; CODEX Bucket 1→2 mid-plan override; 0 id=342 + 0 id=343 architecture-clean |
| 9.14 | 11-09.14 SRV0 per-pass binding | 2026-05-22 | CODEX Bucket A; **visible-textured-Tatooine milestone** |
| 9.15 | 11-09.15 visual-parity cascade | 2026-05-24 | ~45-iteration parity arc — see below. **VISUAL PARITY achieved Iter-39C**; closed PASS-WITH-DEFERRALS |
| 10 | 11-10 DPVS remeasure | DEFERRED | Skipped; needs clean draw surface to measure (post-Phase-12) |
| 11 | 11-11 Visual parity + closeout | 2026-05-24 | This closeout (verdict + SUMMARY + planning-doc updates). Plan predated the 11-09.15 cascade |

### Plan 11-09.15 inner cascade (visual-parity arc)

| Iter | Result |
|------|--------|
| 38B | **Matrix transpose fix** — cbuffer matrices must be `XMMatrixTranspose`d at upload (col-vec engine vs row-vec bytecode). Half the visual-parity win |
| 39C | **Per-pass blend wiring** — completed visual parity (the other half). Iter-32A was a workaround, retired here |
| 40 | Baked-RT format BGRX→BGRA — closed `id=281 CopySubresourceRegion` 7×/session |
| 41 | Screenshot impl + F12 keybind — visual-comparison capability |
| 42v2 | Row-scale `setObjectToWorldTransformAndScale` matching D3D9 — head shape, mountain colors |
| 44A | Per-pass depth + write mask — no visible win solo |
| 44B | Per-pass alpha-test via PS `clip()` + cbuffer slot 1 — **char-select avatar renders** |
| 44E | Multi-stage SRV binding (slot 0 → all 8 stages) — plumbing correct; visually inert (PS samples slot 0 only) |
| 44C | Per-pass blend factors — **REVERTED** (smoke regression); consult: amplifies PS-gen gap, doesn't fix it |
| 44 consult | CODEX + Cursor deep-dive → converged on PASS-WITH-DEFERRALS + named Phase 12 scope |
| 45 | **PerMaterialCB PS-slot-2 dead-write fix** — `setPixelShaderUserConstants` now uploads (was a pure dead-write). Correct plumbing, visually inert at Phase 11; Phase 12 prerequisite. No regression |

---

## Phase 12 Successor Scope (from consult convergence)

The remaining visual gaps (eyes-through-head, clipped head, **square mini-map** — see
correction below, particle squares, ribbon stretch, washed sky) all trace to a small set
of **structural** gaps, not more render-state tuning:

1. **Asset PS pipeline (THE blocker).** PEXE bytecode is rejected by `CreatePixelShader`.
   Need offline `.psh` → SM5 DXBC translation, or an HLSL rewrite path mirroring the VS
   pipeline. Until this lands, the dynamic PS samples only slot 0 and ignores multi-stage
   combiners — which is why eyes/head/mini-map/particles are wrong.
2. **`Pass::apply` constant uploads.** D3D9 uploads material, textureFactor (×2),
   textureScroll, stencil ref, fog mode, fullAmbient per pass; D3D11 uploads NONE in
   `Direct3d11_StaticShaderData::apply`. Iter-45 fixed the PS-slot-2 dead-write
   prerequisite; the per-pass uploads themselves are still TODO.
3. **Stencil state mapping** into `D3D11_DEPTH_STENCIL_DESC` (decals, outlines, interiors).
4. **Gamma LUT post-process pass** — DXGI flip-model has no `SetGammaRamp`; needs a 1D/2D
   LUT pass encoding `pow(0.5 + contrast*(x*brightness - 0.5), 1/gamma)`. Fixes washed sky.
5. **Optional FFP stage-cascade generator** — for FFP-only passes with no asset PS;
   implement the dominant 10-12 of 26 TextureOperation modes, log+fallback the rest.

**Should-do after baseline:** FFP lighting/material-source parity, light-manager constant
parity (dot3, extended lights), bloom enable + alpha-fade packing, point-sprite emulation,
resize/presentToWindow/cursor completeness, backbuffer-lock/writeImage parity.

**Accepted known limitations:** dither ignored (no D3D11 equivalent); fullscreen gamma-ramp
replaced by swap-chain-local LUT; hardware point sprites as 1px until emulated; rare texture
ops (BUMPENVMAP, DOTPRODUCT3, MULTIPLYADD) log+fallback until live assets surface them.

---

## Correction logged this closeout (Iter-45 smoke)

The Plan 11-09.15 handoff claimed "**mini-map circular (Iter-44B win)**". **This is FALSE.**
Verified 2026-05-24: D3D9 (screenShot0001) renders the radar as a round disc; D3D11
(screenShot0006 baseline, screenShot0008 current) has **never** been round — it renders as
a square/diamond. Iter-44B's `clip()` did not produce the circular radar. The square radar
is the **same PS-gen / multi-stage family** as eyes-through-head (the circular alpha-mask
texture isn't sampled). Reclassified to Phase 12, not a closed win. (Memory:
`project-phase11-minimap-never-round`.)

---

## Key Decisions Realized

- **D-01** clean rewrite (D3D9 as reference only): ✓ — Direct3d11/ is new code, no D3D9 copy.
- **D-02** rasterMajor=11 → gl11_d.dll: ✓ — Graphics.cpp range check extended.
- **D-03** hybrid D3DCompile + .cso cache: ✓ — stage/shader-cache/ populated.
- **D-04 / D-04a** FFP spike + DESCOPE verdict: ✓ — drove FFP-generator omission.
- **D-05** D3D9 stays buildable + loadable: ✓ across all 17 plans (final smoke under rasterMajor=5).
- **D-13** no lost-device primitives in Direct3d11/: ✓ — grep returns 0 functional hits.

## What Worked

- **CODEX phone-a-friend cadence.** 12+ consult docs across the cascade with very high accuracy; decisive in 11-09.6 / 11-09.8 / 11-09.13 / 11-09.14 and the Iter-44 deep-dive.
- **Cross-AI peer review (CODEX + Cursor).** The Iter-44 deep-dive converged independently on PASS-WITH-DEFERRALS and caught the Iter-45 dead-write bug.
- **Kenny's mid-plan override discipline** (11-09.13 Bucket-2 override) and visual catches (the mini-map-never-round correction at Iter-45).
- **6-field iteration log + consult-doc artifacts.** Preserves the reasoning trail; this closeout reconstructed the cascade from them.

## What Was Inefficient

- **Plan 11-07's 12-iteration X4016 arc** burned context before CODEX cleared it via Rule E.
- **The Iter-44 state-only cycle** (44A→44B→44E→44C-revert) chased visible wins that were structurally gated by the PS-gen blocker — necessary plumbing, but the blocker should have been named sooner.
- **Handoff over-claim** ("mini-map circular") propagated a false win until Kenny's Iter-45 smoke caught it. Lesson: don't claim a visual win without a D3D9-vs-D3D11 screenshot diff.

## Recovery Anchors

- Plan 11-09.14 close — visible-textured-Tatooine milestone.
- Plan 11-09.15 Iter-39C — visual-parity-achieved state.
- Iter-44C revert (`329c87a2e`) — last pre-regression blend state.
- Iter-45 (`cd2574503`) — PerMaterialCB dead-write fix; current close state.

---

## Hindsight Architectural Notes (carry into Phase 12)

1. **Dynamic PS keyed only by VS output** — correct as a linkage workaround, wrong as a material model. Evolve to VS signature + pass material signature.
2. **Multi-stage SRV binding alone cannot affect pixels** without generated PS stage evaluation.
3. **D3D9 `Compare[]` table has an intentional `C_GreaterOrEqual` ↔ `C_NotEqual` swap** — mirror for asset parity, don't "fix".
4. **`RibbonAppearance` uses owner-W** (Iter-42v2 reaches it); identity-W is the intentional pattern for ParticleEmitter / SwooshAppearance (vertices already in world space). The ribbon stretch is NOT a transform bug — classify with draw-time logging.
