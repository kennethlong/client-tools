# Phase 12 Visual Baseline — D3D11 vs D3D9 (Mos Eisley, Tatooine)

**Captured:** 2026-05-25 (character "Little Bigman", parked in Mos Eisley)
**Purpose:** Matched-pair reference for Phase 12 (asset PS pipeline + gamma + remaining D3D11 visual gaps). D3D9 (`rasterMajor=5`, `gl05_d.dll`) is the known-good ground truth; D3D11 (`rasterMajor=11`, `gl11_d.dll`) is the plugin under development.

**Method:** Same character, same login. cfg `rasterMajor` toggled `11↔5` between captures; client restarted in place per pair. Exterior positions persist across logout/login (→ exact pairs); interior positions do **not** (logout ejects you from buildings → interior pair is approximate framing only).

---

## Image inventory

| File | Renderer | Spot | Shot | Match | Notes |
|------|----------|------|------|-------|-------|
| `spotA_interior_d3d11_0009.jpg` | D3D11 | A — cantina interior | 0009 | approx | the rich "everything wrong" interior frame |
| `spotA_interior_d3d9_0010.jpg`  | D3D9  | A — cantina interior | 0010 | approx | ground-truth interior |
| `spotB_exterior_d3d11_0013.jpg` | D3D11 | B — outside cantina door | 0013 | exact* | *captured mid-load; some smear is LOD streaming |
| `spotB_exterior_d3d9_0011.jpg`  | D3D9  | B — outside cantina door | 0011 | exact | ground-truth exterior |
| `spotC_exterior_d3d11_0014.jpg` | D3D11 | C — Mos Eisley street | 0014 | exact* | "kitchen-sink" messy frame |
| `spotC_exterior_d3d9_0015.jpg`  | D3D9  | C — Mos Eisley street | 0015 | exact | ground-truth exterior |
| `loadscreen_tatooine_centerline_seam_d3d11_0012.jpg` | D3D11 | load screen | 0012 | — | **centerline seam artifact** (BEFORE — pre-Phase-18 reference, leave intact) |
| `loadscreen_tatooine_d3d9_noseam_0016.jpg` | D3D9 | load screen | 0016 | corrob. | no seam (different splash variant; seam is blit-path-level, not image-specific) |

### Phase 18 verification — in-client by Kenny, no captures retained (2026-05-30)

**No new screenshot artifacts were captured for Phase 18.** Verification was done live in-client by Kenny across three distinct splash images and judged seam-free; screenshots were intentionally waived once the fix was visibly confirmed. Recorded honestly here (rather than as a fabricated matched-pair set) so this is not mistaken for a captured-evidence diff — the authority is Kenny's direct in-client A/B observation. The pre-Phase-18 BEFORE seam capture (`loadscreen_tatooine_centerline_seam_d3d11_0012.jpg`) remains as the documented prior state.

**Image-independence method (forced-image substitution down the Tatooine route):** the seam is image-independent, so 3 distinct splash *images* shown through the same Tatooine ground-load path satisfy the ≥3-variant intent (route-travel not required). Real splashes were extracted from `data_texture_03.tre` (`texture/loading/<planet>/ui_load_<planet>.dds`) and rotated into the loose dev-bundle override `D:/swg_dev_bundle/texture/loading/tatooine/ui_load_tatooine.dds` (searchPath priority 12 > TRE) via `D:/swg_dev_bundle/_seam_variants/seam_variant.ps1`.

---

## Findings (bucketed for Phase 12)

### 1. Asset pixel-shader pipeline — CONFIRMED, #1 blocker
Across every in-world pair, D3D11 surfaces render **untextured** (flat white/mauve) where D3D9 shows full diffuse textures, with **magenta fallback-PS patches** where shaders fail to bind (e.g. the lower-right doorframe sliver in 0009, the stray sliver in 0013/0014). Geometry is present and roughly correct — this is purely a **shading/texture-binding gap**. Matches the known Phase 11 deferral: asset PS pipeline → `Pass::apply` constant uploads → gamma LUT.
- Evidence: 0009 vs 0010 (interior), 0013 vs 0011 and 0014 vs 0015 (exterior).

### 2. Lighting / gamma — observed
D3D11 interiors are **blown-out flat white**; D3D9 shows proper ambient/diffuse interior lighting and atmospheric haze. Consistent with missing PS lighting + the deferred **gamma LUT**. Likely resolves alongside bucket 1 but track separately in case the LUT is independent.
- Evidence: 0009 vs 0010 most clearly.

### 3. In-world geometry distortion — PARTIALLY linked, open question
D3D11 shows **stretched/shard geometry** absent in D3D9:
- **Interior NPCs (0009):** characters render as white shards/spikes — skeletal meshes. This is the **rendering-side twin of the c0000005 crash** (commit `905fb5d64`): both live on the D3D11 single-stream skeletal path (`SoftwareBlendSkeletalShaderPrimitive`, `ms_useMultiStreamVertexBuffers == false`). The crash was `collide` *reading* bad vertices; the stretch is the *draw* path skinning into bad vertex positions on the same branch. **High-confidence link.**
- **Exterior large objects (0014 upper-left wreck):** broken into dark shards in D3D11, coherent in D3D9 (0015). May be **static** geometry, not skeletal → distortion might be broader than the skeletal path. **OPEN — needs Phase 12 investigation.** Caveat: 0013/0014 were partly mid-load, so some smear is LOD streaming, not a settled bug. Re-capture fully-settled exterior frames to separate transient from real.

### 4. Minimap shape — PS-gen family
D3D11 radar renders **square**; D3D9 renders **round**. Known deferral (`project-phase11-minimap-never-round`); grouped with the PS-gen / 2D-texturing family.
- Evidence: 0009 (square) vs 0010 (round); visible in all pairs.

### 5. Loading-screen centerline seam — clean 2D repro, half-texel candidate (image-independent, CONFIRMED)
The Tatooine load screen (0012, D3D11) has a **faint vertical seam dead-center** (x≈512). **Image-independent and renderer-specific (Kenny, 2026-05-25): the seam appears on MANY different D3D11 load screens, and has NEVER been seen across ~20 D3D9 load-ins on various splash images.** Because it tracks the renderer rather than the image, it is a **path-level** artifact, not a quirk of one splash texture — stronger evidence than a same-image A/B.

This is a **2D fullscreen image** — no 3D geometry, skinning, or lighting involved — so it isolates a **texture-sampling / fullscreen-blit bug** away from all in-world noise. Most likely cause: the splash is drawn as **two side-by-side halves** (large splash exceeds max texture width) and the sample at the shared edge is off by a **half-texel** — the canonical D3D9→D3D11 porting bug (D3D9's -0.5 texel offset rule, removed in D3D10+). Deterministic repro: just zone and watch any load screen.
- **Best first target for Phase 12's 2D/blit work** — small, isolated, deterministic, and now confirmed image-independent.

---

## Caveats
- **Interior pair (A)** is approximate framing only (logout repositions you indoors). Differences shown are gross enough to read qualitatively; do not use for pixel alignment.
- **0013 / 0014** were captured mid-load; treat their geometry smear as suspect (LOD streaming) until re-captured settled.
- **Load-screen seam** is confirmed image-independent: many D3D11 screens show it, ~20 D3D9 load-ins across various splashes never did (Kenny, 2026-05-25). The 0012/0016 files happen to be different splash variants, but the conclusion rests on the multi-sample observation, not that single pair.

## Suggested Phase 12 entry order
1. **Asset PS pipeline** (bucket 1) — unblocks textures + magenta + likely lighting (bucket 2) and minimap (bucket 4) at once.
2. **Loading-screen seam** (bucket 5) — small isolated half-texel fix; good standalone win / sampling-path canary.
3. **Exterior geometry distortion** (bucket 3) — investigate whether static meshes distort too, beyond the now-fixed skeletal single-stream path.

---

## Phase 18 — Load-screen centerline seam FIXED (UI-01) — PASS (in-client, 2026-05-30)

**Fixed:** 2026-05-30 (branch `koogie-msvc-cpp20-base`)
**Fix:** central single-site half-pixel correction folded into the c9 `viewportData` `.z`/`.w` offset terms in `Direct3d11_StateCache.cpp:setViewport` (`.z += 1.0f/width`, `.w -= 1.0f/height`), resolution-independent. Confirmed by the unanimous D-01 CODEX+Cursor consult (`.planning/research/CONSULT-18-half-texel-seam-SYNTHESIS.md`). Root cause: the engine bakes `CuiManager::ms_pixelOffset = -0.5f` into every 2D XYZRHW vertex (fingerprint `(-0.5,-0.5)…(1023.5,767.5)`), which aligned under D3D9's pre-D3D10 `-0.5` texel rule but misaligned under D3D11 center-sampling. gl11-only; D3D9 path untouched (D-04). `getOneToOneUVMapping` stays a dead `scaffold_fatal_stub` (D-01).

### Result (UI-01) — PASS
- **Seam GONE** under `rasterMajor=11` on the world load screen, confirmed in-client by Kenny 2026-05-30.
- **Image-independent:** verified across **three distinct splash images** shown down the same Tatooine ground-load route via the forced-image override (native **Tatooine**, **Naboo**, **Corellia** splashes) — all three rendered seam-free. (The seam is image-independent, so 3 distinct images on one route satisfy the ≥3-variant intent.)
- **Character renders correctly** (char-select 2D + 3D), i.e. no gross 2D regression introduced by the global c9 edit.
- **Mechanism = global XYZRHW position bias** (consult judgment); the split-texture join was only a *visibility amplifier*. The c9 correction removed it, so the named UV/sampler-edge fallback was NOT needed.

### Evidence status — honest record
- **No screenshot artifacts were captured/retained.** Kenny waived captures once the fix was visibly confirmed across the three splash variants. The authority for this PASS is Kenny's **direct in-client A/B observation**, not a stored matched-pair diff. (Recorded plainly to avoid the Iter-44B minimap over-claim trap: this is a human-confirmed visual PASS, not a pixel-diffed one.)
- **Not formally performed (waived with the captures):** the higher-resolution (1440p) capture, the odd-width / non-default-viewport capture, and the formal zoom-crop / pixel-aligned full-2D diff on thin HUD text + radial edges. Resolution-independence holds *by construction* (the correction is `1/w`,`1/h` in NDC, derived from the live viewport size — no fixed pixel constants), and HUD/fonts/radial/char-select 2D rendered correctly during Kenny's live sessions, but a sub-pixel (±0.5px) shift on thin text was not pixel-diffed. If a future pass wants that rigor, the forced-image harness (`D:/swg_dev_bundle/_seam_variants/seam_variant.ps1`) + matched rasterMajor toggle reproduce it without new setup.

### D3D9 byte-for-byte unregressed (D-04)
- Structurally guaranteed: no file under `…/Direct3d9/…` was modified (git diff scope = one `…/Direct3d11/…` file). The `rasterMajor=5` reference path is unchanged.

### Verdict (UI-01 human A/B gate): **APPROVED** — seam gone, image-independent across 3 variants, no observed 2D regression, D3D9 unregressed. Phase 18 closed on Kenny's in-client confirmation (screenshot evidence intentionally waived).
