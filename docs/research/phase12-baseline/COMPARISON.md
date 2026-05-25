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
| `loadscreen_tatooine_centerline_seam_d3d11_0012.jpg` | D3D11 | load screen | 0012 | — | **centerline seam artifact** |
| `loadscreen_tatooine_d3d9_noseam_0016.jpg` | D3D9 | load screen | 0016 | corrob. | no seam (different splash variant; seam is blit-path-level, not image-specific) |

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

### 5. Loading-screen centerline seam — clean 2D repro, half-texel candidate
The Tatooine load screen (0012, D3D11) has a **faint vertical seam dead-center** (x≈512); D3D9 (0016) has none. This is a **2D fullscreen image** — no 3D geometry, skinning, or lighting involved — so it isolates a **texture-sampling / fullscreen-blit bug** away from all in-world noise. Most likely cause: the splash art is drawn as **two side-by-side halves** (large splash exceeds max texture width) and the sample at the shared edge is off by a **half-texel** — the canonical D3D9→D3D11 porting bug (D3D9's -0.5 texel offset rule, removed in D3D10+). Deterministic repro: just zone and watch the load screen.
- **Best first target for Phase 12's 2D/blit work** — small, isolated, deterministic.

---

## Caveats
- **Interior pair (A)** is approximate framing only (logout repositions you indoors). Differences shown are gross enough to read qualitatively; do not use for pixel alignment.
- **0013 / 0014** were captured mid-load; treat their geometry smear as suspect (LOD streaming) until re-captured settled.
- **Load-screen pair** uses two different splash variants (random rotation); the seam is blit-path-level so this is corroboration, not a same-image A/B. A same-image A/B would be stronger if a fixed splash can be forced.

## Suggested Phase 12 entry order
1. **Asset PS pipeline** (bucket 1) — unblocks textures + magenta + likely lighting (bucket 2) and minimap (bucket 4) at once.
2. **Loading-screen seam** (bucket 5) — small isolated half-texel fix; good standalone win / sampling-path canary.
3. **Exterior geometry distortion** (bucket 3) — investigate whether static meshes distort too, beyond the now-fixed skeletal single-stream path.
