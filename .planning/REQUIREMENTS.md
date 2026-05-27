# Requirements: whitengold — v2.2 Visual Parity

**Defined:** 2026-05-27
**Core Value:** Every change leaves the client bootable to character select **under both renderers** — and v2.2 adds: D3D11 (`rasterMajor=11`) must visually match the known-good D3D9 (`rasterMajor=5`) baseline. **Never regress the D3D9 reference path** when editing shared `clientGraphics` code.

> Requirements are user-observable, testable visual outcomes. The "how" (asset-PS census, generated-PS `TextureOperation` evaluator vs `PSRC` recompile, `Pass::apply` constants, gamma LUT, half-texel fix) lives in the roadmap/research, not here. Validation is by **D3D11-vs-D3D9 screenshot diff** against the `docs/research/phase12-baseline/COMPARISON.md` matched pairs.

## v2.2 Requirements

### Character Rendering — char-select beachhead (first vertical slice)

- [ ] **CHAR-01**: On the character-select screen, character skin & clothing render with correct diffuse textures under D3D11 (matches D3D9) — not untextured, flat-white, or magenta.
- [ ] **CHAR-02**: Character eyes render correctly under D3D11 — correct customization-palette color, seated in the face, not gray and not visible through the back of the head.
- [ ] **CHAR-03**: Character head/face multi-stage materials (e.g. `sul_*_head.sht`) composite their texture stages correctly under D3D11.

### World Surface Rendering — open world

- [ ] **WORLD-01**: Open-world surfaces (terrain, buildings, props) render with correct diffuse textures under D3D11, matching D3D9.
- [ ] **WORLD-02**: Multi-texture material stages (detail / specular / overlay) composite correctly under D3D11 (the engine's multi-stage `TextureOperation` cascade is evaluated, not slot-0-only).
- [ ] **WORLD-03**: Interior lighting renders with correct ambient/diffuse under D3D11 — not blown-out flat white.

### Color / Gamma

- [ ] **GAMMA-01**: Overall scene tone/brightness under D3D11 matches the D3D9 baseline (no washed-out sky/tone), via a gamma path that replicates D3D9's gamma ramp — without sRGB-view double-correction.

### 2D / UI

- [ ] **UI-01**: The loading screen renders with no vertical centerline seam under D3D11 (half-texel fullscreen-blit fix). Image-independent: verified across multiple splash images.
- [ ] **UI-02**: The minimap/radar renders round (not square) under D3D11, matching D3D9. *(Verified by screenshot diff — a prior iteration falsely pre-claimed this; do not mark done without a diff.)*

### Effects

- [ ] **FX-01**: Particle effects render correctly under D3D11 (blending, textures, additive/alpha modes match D3D9).
- [ ] **FX-02**: Ribbon / swoosh effects render without distortion or stretch under D3D11.

### Geometry Integrity

- [ ] **GEO-01**: Exterior static-mesh geometry renders without shard/stretch distortion under D3D11. *(Investigation-gated: a fully-settled exterior re-capture must first separate real distortion from mid-load LOD streaming before a fix is scoped. The skeletal-path twin was already fixed at `905fb5d64`.)*

### Performance / Culling

- [ ] **DPVS-01**: DPVS occlusion-culling performance is re-measured under D3D11 (the v2.0 SPEC R7 deferral) and a keep/remove verdict recorded — performed last, once geometry renders cleanly enough for the measurement to be meaningful.

## Future Requirements (deferred — not in v2.2)

- **HDR / tone-mapping beyond D3D9 parity** — v2.2 targets *parity*, not visual improvement over the original.
- **Shader hot-reload / authoring tooling** — useful for dev velocity but not a parity outcome.

## Out of Scope

Explicitly excluded for v2.2. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| Decompiling/translating the rejected D3D9 PEXE bytecode | The original PS source (`TAG_PSRC`) is still in the asset and can be recompiled — bytecode archaeology is unnecessary (research-confirmed). |
| A runtime D3D9-bytecode interpreter | Higher cost than recompiling source / generating from the stage model; no parity benefit. |
| Flipping the backbuffer/SRVs to `_SRGB` formats | Double-correction trap — D3D9 uses a scanout gamma ramp with sRGB sampling OFF; parity = replicate the ramp, not sRGB encode. |
| Improving visuals beyond the D3D9 baseline | v2.2 is parity, not enhancement. |
| Render-state-only iteration as a fix strategy | Proven (Phase 11) to amplify, not close, the asset-PS gap. |
| Chasing exterior shards before a settled re-capture | 0013/0014 were mid-load; some smear is LOD streaming, not a bug. Gate GEO-01 on a clean capture first. |

## Traceability

*(Populated by the roadmapper — each requirement maps to exactly one phase. Phases continue from 16 → start at 17.)*

| Requirement | Phase | Status |
|-------------|-------|--------|
| CHAR-01 | TBD | Pending |
| CHAR-02 | TBD | Pending |
| CHAR-03 | TBD | Pending |
| WORLD-01 | TBD | Pending |
| WORLD-02 | TBD | Pending |
| WORLD-03 | TBD | Pending |
| GAMMA-01 | TBD | Pending |
| UI-01 | TBD | Pending |
| UI-02 | TBD | Pending |
| FX-01 | TBD | Pending |
| FX-02 | TBD | Pending |
| GEO-01 | TBD | Pending |
| DPVS-01 | TBD | Pending |

**Coverage:**
- v2.2 requirements: 13 total
- Mapped to phases: 0 (pending roadmap)
- Unmapped: 13 ⚠️

---
*Requirements defined: 2026-05-27 — milestone v2.2 Visual Parity. Source: `docs/research/phase12-baseline/COMPARISON.md` + `.planning/research/SUMMARY.md`.*
*Last updated: 2026-05-27 after initial definition.*
