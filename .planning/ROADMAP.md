# Roadmap: whitengold — SwgClient Modernisation Port

## Milestones

- ✅ **v1.0 Compile + Launch** — Phases 1–6 (CMake foundations → launch/login handshake). Authored in the original whitengold CMake repo; imported here 2026-05-25 for the record.
- ✅ **v2.0 Modernisation** — Phases 7–11 (shipped 2026-05-25). Dead-code removal, STLPort→MSVC STL (via Koogie MSBuild adoption), DPVS verdict, D3D11 renderer plugin. Full detail: `milestones/v2.0-ROADMAP.md`.
- ✅ **v2.1 Decruft** — Phases 12–16 (shipped 2026-05-27). Re-applied the orphaned CLEAN-01..04 dead-code removals against the active Koogie/MSBuild tree; five dormant subsystems unlinked + deleted, client kept bootable under both renderers. Full detail: `milestones/v2.1-ROADMAP.md`. Audit: `milestones/v2.1-MILESTONE-AUDIT.md`.
- 🚧 **v2.2 Visual Parity** — Phases 17–23 (active, started 2026-05-27). Close the D3D11 visual gaps catalogued in `docs/research/phase12-baseline/COMPARISON.md` so `rasterMajor=11` matches the known-good `rasterMajor=5` (D3D9) baseline. Char-select beachhead first (textures + eyes), then extend outward. Root cause: the asset pixel-shader pipeline (D3D9 PEXE bytecode rejected by `CreatePixelShader` → magenta/slot-0 fallback). Success = D3D9-vs-D3D11 screenshot diff against the COMPARISON.md matched pairs.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

<details>
<summary>✅ v1.0 Compile + Launch (Phases 1–6) — historical (whitengold CMake)</summary>

- [x] Phase 1: CMake skeleton — foundations spike & lock
- [x] Phase 2: Shared engine libraries
- [x] Phase 3: Client engine libraries (SDK-heavy tier)
- [x] Phase 4: SwgClient executable link
- [x] Phase 5: Onboarding / developer experience
- [x] Phase 6: Launch + login handshake

Artifacts in `.planning/phases/01..06`. Describes the CMake build superseded by v2's Option-D MSBuild adoption.

</details>

<details>
<summary>✅ v2.0 Modernisation (Phases 7–11) — SHIPPED 2026-05-25 (tech_debt)</summary>

- [x] Phase 7: Dead Code Removal — Track A (CLEAN-01..05). *Done on CMake tree; orphaned by Phase 9 Option-D base swap — re-applied to the MSBuild tree in v2.1 Decruft.*
- [◑] Phase 8: Dead Code Removal — Track B (CLEAN-06). Closed-as-scoped: ~12 tools wired, ~30 deferred.
- [x] Phase 9: STLPort → MSVC STL (STL-01..05). Option D — adopted Koogie MSBuild tree (`479d35df3`); Tatooine zone-in PASS.
- [x] Phase 10: DPVS Culling Experiment (DPVS-01/02). Verdict Option α; D3D11 remeasure deferred.
- [x] Phase 11: D3D11 Renderer Plugin (D3D11-01..05). gl11_d.dll; D3D9+D3D11 selectable; PASS-WITH-DEFERRALS.

Full detail + per-plan history: `milestones/v2.0-ROADMAP.md`. Audit: `milestones/v2.0-MILESTONE-AUDIT.md`.

</details>

<details>
<summary>✅ v2.1 Decruft (Phases 12–16) — SHIPPED 2026-05-27 (tech_debt)</summary>

- [x] Phase 12: Orphaned Directory & Project Deletes (DECRUFT-01/02/03). trackIR + stationapi dirs deleted, SwgClientSetup + lcdui dropped from `swg.sln`; dual-renderer boot baseline re-established. (3/3 plans, 2026-05-25)
- [x] Phase 13: VideoCapture Library Unlink (DECRUFT-04). Live caller + wrapper + lib tokens removed atomically; vendored `videocapture/` tree deleted; Debug+Release link clean. (3/3 plans, 2026-05-26)
- [x] Phase 14: Voice Chat (Vivox) Source Removal (DECRUFT-05). ~24 source files + 3 messages + 3 vendored voice trees deleted; CR-01 radial-menu ordinal regression caught + fixed. (3/3 plans, 2026-05-26)
- [x] Phase 15: In-Game Browser (XPCOM/Mozilla) Removal & Milestone Gate (DECRUFT-06/07). `libMozilla.vcxproj` dropped (11 GUID locations) + 1,866-file vendored tree deleted; full-removal-set dual-renderer boot gate PASS. (4/4 plans, 2026-05-27)
- [x] Phase 16: v2.1 Tech-Debt Cleanup (INSERTED — post-audit closure, no new reqs). SwgGodClient `989crypt.lib` dead token unlinked; dead `finalUrl` block + orphaned voice-volume API removed; SwgClient link+boot gate clean. (3/3 plans, 2026-05-27)

Full detail + per-plan history + success criteria: `milestones/v2.1-ROADMAP.md`. Audit: `milestones/v2.1-MILESTONE-AUDIT.md`.

</details>

### 🚧 v2.2 Visual Parity (Phases 17–23) — ACTIVE

**Milestone Goal:** Close the D3D11 (`rasterMajor=11`) visual gaps so the renderer matches the known-good D3D9 (`rasterMajor=5`) baseline catalogued in `docs/research/phase12-baseline/COMPARISON.md`. Prove the asset pixel-shader pipeline first on the deterministic character-select screen (textures + eyes + head), then extend outward to the open world, then color/effects/geometry/culling. Validation is by **D3D11-vs-D3D9 screenshot diff** against the COMPARISON.md matched pairs — success means "matches `rasterMajor=5`", NOT merely "no magenta". **Invariant: never regress the D3D9 reference path** when editing shared `clientGraphics` code; boot-gate BOTH renderers on any `ShaderImplementation.cpp` edit.

**Root cause (research + CODEX/Cursor consult):** the engine ships pre-compiled D3D9 PEXE pixel-shader bytecode (`TAG_PEXE`) that `ID3D11Device::CreatePixelShader` rejects, leaving `m_d3dPS` null and falling back to a slot-0-only dynamic PS (or magenta), with per-pass `Pass::apply` constants never uploaded. The fix recompiles the discarded `TAG_PSRC` source (primary: `//hlsl` → `compilePixelShaderFromHlsl`; secondary: asm `PSRC` ported to HLSL → `ps_4_0`; tertiary/narrow: FFP `TextureOperation` generator only for genuine FFP-only passes) and uploads per-pass constants **reflection-driven (D3DReflect)**, not via copied D3D9 register indices. Re-assembling asm just reproduces the rejected D3D9 bytecode — a named landmine.

- [ ] **Phase 17: PSRC Census + Char-Select Beachhead** - Census the real asset PS source, then prove the recompile + reflection-driven constant pipeline on char-select textures, eyes, and head
- [ ] **Phase 18: Load-Screen Half-Texel Seam** - Eliminate the load-screen centerline seam via the half-pixel UV-mapping fix (independent 2D canary)
- [ ] **Phase 19: Gamma LUT + Interior Lighting** - Replicate the D3D9 gamma ramp as a LUT post-pass and fix blown-out flat-white interior lighting
- [ ] **Phase 20: Open-World PS Extension + Minimap** - Extend the PS pipeline to open-world surfaces, multi-stage compositing, and the round minimap
- [ ] **Phase 21: Particles & Ribbon Effects** - Restore correct particle blending and ribbon/swoosh rendering under D3D11
- [ ] **Phase 22: Exterior Geometry / Skeletal Shards** - Resolve exterior static-mesh shard distortion after a settled re-capture
- [ ] **Phase 23: DPVS D3D11 Remeasure** - Re-measure DPVS occlusion-culling cost under D3D11 and record a keep/remove verdict (strictly last)

## Phase Details

### Phase 17: PSRC Census + Char-Select Beachhead
**Goal**: Prove the asset pixel-shader pipeline end-to-end on the deterministic, isolated character-select screen — textures, eyes, and multi-stage head materials all render correctly under D3D11, matching D3D9. This is the milestone's beachhead: it converts the biggest open risk (HLSL:asm ratio) into a measured number, then validates the recompile + reflection-driven-constant approach on a known, bounded shader surface before any open-world work.
**Depends on**: Nothing (first phase of v2.2; builds on the Phase 11 D3D11 plugin)
**Requirements**: CHAR-01, CHAR-02, CHAR-03
**Success Criteria** (what must be TRUE — validated by D3D11-vs-D3D9 screenshot diff at char-select default pose):
  1. A PSRC language census has been run against the **real** retail-TRE asset extraction (not the empty repo checkout), classifying `//hlsl` vs asm, target profiles, includes, sampler slots, and referenced constants across the char-select + Iter-44E shader set — and the HLSL:asm ratio is recorded as the input that decides the lane mix.
  2. On char-select, character skin & clothing render with correct diffuse textures under D3D11 (matches `rasterMajor=5`) — not untextured, flat-white, or magenta (CHAR-01). The single-stage body/clothing control proves "pipeline works" independent of multi-sampler.
  3. Character eyes render correctly under D3D11 — correct customization-palette color, seated in the face, occluded by the head (verify "not visible through the back of the head" by fresh A/B screenshot, since Iter-44A depth wiring already addressed it — do not re-fold into PS scope) (CHAR-02).
  4. Character head/face multi-stage materials (`sul_*_head.sht`, `sul_eye.sht`) composite their texture stages correctly under D3D11 via the recompiled PSRC program, matching D3D9 (CHAR-03).
  5. Single-stream vs multi-stream skeletal skinning is confirmed (RenderDoc mesh-viewer A/B) before any residual head/mesh weirdness is attributed to the PS; `id=342==0 && id=343==0` in `stage/d3d11-debug.log`; both `rasterMajor=5` and `=11` boot to char-select without crash.
**Plans**: 3 plans (3 waves — sequential; census gates recompile gates reflection)
- [x] 17-01-PLAN.md — PSRC retain + flag-gated census on real char-select boot; record HLSL:asm ratio; COMPARISON dir (gating)
- [ ] 17-02-PLAN.md — recompile lane: //hlsl PSRC -> compilePixelShaderFromHlsl -> bind m_d3dPS; CHAR-01 single-stage control A/B
- [ ] 17-03-PLAN.md — D3DReflect-driven material/textureFactor upload; CHAR-02 eyes + CHAR-03 head A/B; D-08 skinning confirm
**UI hint**: yes
**Mode:** standard
**Research:** NEEDS per-phase research — (a) census tool is the gating first deliverable (HLSL:asm ratio); (b) which TextureOperation/FFP passes char-select actually exercises; (c) single-stream-skinning-fix vs multi-stream-flip decision.
**Approach notes:** Primary lane = recompile `TAG_PSRC` `//hlsl` via the existing `compilePixelShaderFromHlsl` (mirror the working VS compile stack); secondary lane = port asm `PSRC` → HLSL → `ps_4_0` (NEVER re-assemble — that reproduces the rejected D3D9 bytecode); tertiary/narrow = FFP `TextureOperation` generator only for genuine FFP-only passes. Per-pass `Pass::apply` constants (material/textureFactor/scroll/stencil) upload **reflection-driven** (D3DReflect), not via copied D3D9 register indices. Keep the magenta fallback as a visible diagnostic tombstone. Shared `ShaderImplementation.cpp` PSRC-retain edit must keep D3D9 byte-for-byte identical except storing the text. Do NOT re-enable per-pass blend factors early (Iter-44C amplification regression).

### Phase 18: Load-Screen Half-Texel Seam
**Goal**: Eliminate the vertical centerline seam on D3D11 load screens so the fullscreen splash blit matches D3D9. Architecturally independent of the PS pipeline — a small, isolated, deterministic 2D-sampling canary that establishes the half-pixel convention before the Phase 20 minimap work.
**Depends on**: Nothing (independent of Phase 17 — can run early/in parallel)
**Requirements**: UI-01
**Success Criteria** (what must be TRUE — validated by screenshot diff vs D3D9 across multiple splash images):
  1. The loading screen renders with no vertical centerline seam under D3D11, matching D3D9 (UI-01).
  2. The fix is image-independent: verified across multiple splash images (the seam is a blit-path-level artifact, confirmed image-independent in COMPARISON.md, not a quirk of one texture).
  3. The half-pixel convention is corrected once centrally (implement the `getOneToOneUVMapping` stub / viewport convention), NOT scattered as per-draw +0.5 fudge factors.
**Plans**: TBD
**UI hint**: yes
**Research:** Standard patterns — the D3D9 → D3D10+ half-pixel offset removal is well-documented; the `getOneToOneUVMapping` stub location is known. No per-phase research needed.

### Phase 19: Gamma LUT + Interior Lighting
**Goal**: Make overall scene tone/brightness under D3D11 match the D3D9 baseline (no washed-out sky/tone) and fix blown-out flat-white interiors. Scheduled AFTER the PS pipeline so gamma is calibrated on correctly-shaded content — judging gamma against an untextured/magenta scene gives meaningless results.
**Depends on**: Phase 17 (correctly-shaded surfaces required before gamma can be calibrated; interior lighting needs the per-pass lighting constants the PS pipeline introduces)
**Requirements**: GAMMA-01, WORLD-03
**Success Criteria** (what must be TRUE — validated by screenshot diff vs D3D9, esp. interior pair 0009/0010):
  1. Overall scene tone/brightness under D3D11 matches the D3D9 baseline (no washed-out sky/tone), via a gamma path that replicates D3D9's gamma ramp (GAMMA-01).
  2. The gamma fix is a single fullscreen LUT post-pass replicating the D3D9 `SetGammaRamp` `pow()` formula — NOT sRGB-view / sRGB-SRV double-correction (texture SRVs stay `_UNORM`, mirroring D3D9 `D3DSAMP_SRGBTEXTURE=0`).
  3. Interior lighting renders with correct ambient/diffuse under D3D11 — not blown-out flat white (WORLD-03), via the per-pass light-data/fog constant uploads PLUS the simplified `Direct3d11_LightManager` brought toward D3D9 parity (lighting is independently simplified vs D3D9, not just a PS gap).
**Plans**: TBD
**Research:** NEEDS focused per-phase research — confirm the `Direct3d11_LightManager` / `Direct3d9_LightManager::applyLights` constant layout offset-for-offset into the D3D11 cbuffer before wiring. LUT pass itself is a standard pattern.

### Phase 20: Open-World PS Extension + Minimap
**Goal**: Extend the proven char-select PS pipeline to the open-world shader population (terrain, buildings, props), evaluate the engine's multi-stage `TextureOperation` cascade rather than slot-0-only, and deliver the round minimap. The open world introduces the long tail of multi-texture modes, per-stage coord-sets, and texture transforms that char-select doesn't exercise.
**Depends on**: Phase 17 (the generator must prove out on the bounded char-select set before world-scale extension)
**Requirements**: WORLD-01, WORLD-02, UI-02
**Success Criteria** (what must be TRUE — validated by screenshot diff vs D3D9, exterior pairs 0013/0011 and 0014/0015):
  1. Open-world surfaces (terrain, buildings, props) render with correct diffuse textures under D3D11, matching D3D9 (WORLD-01).
  2. Multi-texture material stages (detail / specular / overlay) composite correctly under D3D11 — the engine's multi-stage cascade is evaluated, not slot-0-only (WORLD-02).
  3. The minimap/radar renders round (not square) under D3D11, matching D3D9 — confirmed by a matched D3D9/D3D11 screenshot pair (UI-02). Do NOT mark done without a diff; Iter-44B falsely pre-claimed this once.
  4. Both renderers boot to the open world without crash; `id=342==0 && id=343==0`; D3D9 reference path unregressed.
**Plans**: TBD
**UI hint**: yes
**Research:** NEEDS per-phase research — extended `TextureOperation` census from an open-world session (which of the 26 ops actually appear; implement the dominant ~10–12, log+fallback the rare ones).

### Phase 21: Particles & Ribbon Effects
**Goal**: Restore correct particle effects (blending, textures, additive/alpha modes) and ribbon/swoosh rendering under D3D11 so effects match D3D9. Depends on both the PS generator and the per-pass `Pass::apply` constants (textureFactor, textureScroll). Ribbon stretch is confirmed NOT a transform bug — the draw path must be instrumented before a fix is designed.
**Depends on**: Phase 17 (PS-gen) + Phase 20 (broader TextureOperation coverage); particles need `Pass::apply` constants flowing
**Requirements**: FX-01, FX-02
**Success Criteria** (what must be TRUE — validated by screenshot diff vs D3D9):
  1. Particle effects render correctly under D3D11 — blending, textures, and additive/alpha modes match D3D9 (FX-01).
  2. Ribbon / swoosh effects render without distortion or stretch under D3D11, matching D3D9 (FX-02).
  3. A draw-path instrumentation pass has classified particle/ribbon/swoosh draws (appearance type, topology, VB format, transform class) — the gating deliverable that scopes the ribbon-stretch fix before any fix is designed.
**Plans**: TBD
**Research:** NEEDS per-phase research — the ribbon/swoosh draw-path instrumentation output is the gating deliverable; no fix design before the log confirms the root cause.

### Phase 22: Exterior Geometry / Skeletal Shards
**Goal**: Resolve exterior static-mesh shard/stretch distortion under D3D11 — but only after a fully-settled exterior re-capture separates real distortion from mid-load LOD streaming smear (0013/0014 were mid-load). The skeletal-path twin was already fixed at `905fb5d64` and the single-stream-skinning question is decided at Phase 17; this phase handles whatever exterior/static distortion the settled capture confirms as real.
**Depends on**: Phase 20 (a fully-working PS pipeline makes the settled re-capture diagnosable — distortion can't be separated from missing-texture noise otherwise)
**Requirements**: GEO-01
**Success Criteria** (what must be TRUE — validated by settled-frame screenshot diff vs D3D9):
  1. A fully-settled exterior re-capture has been taken (0013/0014 re-captured after LOD streaming completes) and used to separate real distortion from transient mid-load smear before any fix is scoped.
  2. Exterior static-mesh geometry renders without shard/stretch distortion under D3D11, matching D3D9 (GEO-01) — OR the settled capture confirms the remaining smear was LOD-streaming-transient and the requirement closes as no-real-distortion, recorded with evidence.
  3. The single-stream-skinning-fix vs flip-to-multistream decision (if any skeletal distortion remains beyond Phase 17) is recorded with its rationale.
**Plans**: TBD
**Research:** NEEDS per-phase research — settled re-capture findings + single-stream-fix vs multi-stream-flip decision.

### Phase 23: DPVS D3D11 Remeasure
**Goal**: Re-measure DPVS occlusion-culling cost under D3D11 (the v2.0 SPEC R7 deferral) and record a keep/remove verdict. Strictly last: occlusion cost is meaningless while geometry mis-shades, so this is only valid once all prior phases produce clean-geometry rendering. Mirrors the Phase 10 DpvsProfileInstrumentation methodology.
**Depends on**: Phases 17–22 (all prior visual work must render clean geometry for the measurement to be meaningful)
**Requirements**: DPVS-01
**Success Criteria** (what must be TRUE):
  1. DPVS occlusion-culling performance is re-measured under D3D11 with an occlusion-vs-no-occlusion frame-time comparison (PIX/RenderDoc timing harness, Phase 10 methodology).
  2. A keep/remove verdict is recorded, confirming or revising the Phase 10 Option α decision (remove outdoor occlusion, keep indoor portals) under the D3D11 path (DPVS-01).
  3. The measurement is taken against clean-geometry rendering (not a mis-shaded scene), making the timing meaningful.
**Plans**: TBD
**Research:** Standard patterns — mirrors the Phase 10 DPVS methodology. No new patterns needed.

## Progress

**Execution Order:**
Phases execute in numeric order: 17 → 18 → 19 → 20 → 21 → 22 → 23. (Phase 18 is independent of Phase 17 and may run early/in parallel.)

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1–6 Foundations → Login | v1.0 | — | Complete (historical) | — |
| 7. Dead Code A | v2.0 | — | Complete (CMake tree) | 2026-05 |
| 8. Dead Code B | v2.0 | — | Closed-as-scoped | 2026-05-08 |
| 9. STLPort → MSVC STL | v2.0 | — | Complete (Option D) | 2026-05-10 |
| 10. DPVS Experiment | v2.0 | — | Complete (Option α) | 2026-05 |
| 11. D3D11 Renderer | v2.0 | — | Complete (PASS-WITH-DEFERRALS) | 2026-05-24 |
| 12. Orphaned Deletes | v2.1 | 3/3 | Complete | 2026-05-25 |
| 13. VideoCapture Unlink | v2.1 | 3/3 | Complete | 2026-05-26 |
| 14. Vivox Removal | v2.1 | 3/3 | Complete | 2026-05-26 |
| 15. XPCOM Removal + Gate | v2.1 | 4/4 | Complete | 2026-05-27 |
| 16. v2.1 Tech-Debt Cleanup | v2.1 | 3/3 | Complete | 2026-05-27 |
| 17. PSRC Census + Char-Select Beachhead | v2.2 | 1/3 | In Progress|  |
| 18. Load-Screen Half-Texel Seam | v2.2 | 0/TBD | Not started | - |
| 19. Gamma LUT + Interior Lighting | v2.2 | 0/TBD | Not started | - |
| 20. Open-World PS Extension + Minimap | v2.2 | 0/TBD | Not started | - |
| 21. Particles & Ribbon Effects | v2.2 | 0/TBD | Not started | - |
| 22. Exterior Geometry / Skeletal Shards | v2.2 | 0/TBD | Not started | - |
| 23. DPVS D3D11 Remeasure | v2.2 | 0/TBD | Not started | - |
