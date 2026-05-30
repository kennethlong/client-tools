---
phase: 18-load-screen-half-texel-seam
plan: 03
status: complete
completed: 2026-05-30
requirements: [UI-01]
autonomous: false
branch: koogie-msvc-cpp20-base
verdict: PASS (in-client human A/B, screenshots waived)
---

# 18-03 SUMMARY — UI-01 verified: seam gone, image-independent, no regression

## Outcome: PASS (UI-01 closed)

The load-screen centerline seam is **gone** under `rasterMajor=11`, confirmed in-client by Kenny
(2026-05-30), and the fix is **image-independent** — verified across **three distinct splash images**
down the same Tatooine ground-load route. Character renders correctly (no gross 2D regression). D3D9
reference path structurally unregressed (no `…/Direct3d9/…` file touched).

## Image-independence method (forced-image substitution)

Travel to other planets wasn't needed: the seam is image-independent, so 3 distinct splash *images* on
one route satisfy the ≥3-variant intent. Real splashes were extracted from `data_texture_03.tre`
(`texture/loading/<planet>/ui_load_<planet>.dds`, via `tools/TreeFileExtractor.exe`) and rotated into
the loose dev-bundle override `D:/swg_dev_bundle/texture/loading/tatooine/ui_load_tatooine.dds`
(searchPath priority 12 > TRE) using the helper `D:/swg_dev_bundle/_seam_variants/seam_variant.ps1`:

| Variant | Splash shown on Tatooine route | Seam (rasterMajor=11) |
|---------|--------------------------------|------------------------|
| 1 | native Tatooine | GONE |
| 2 | Naboo (forced) | GONE |
| 3 | Corellia (forced) | GONE |

(The on-screen planet *name* still reads "Tatooine" under variants 2/3 — expected for forced-image substitution.)

## Evidence status — honest record

- **No screenshots captured/retained.** Kenny waived captures once the fix was visibly confirmed across
  the three variants. The PASS authority is Kenny's **direct in-client A/B observation**, not a stored
  matched-pair diff. Recorded plainly in `COMPARISON.md` to avoid the Iter-44B minimap over-claim trap
  (a human-confirmed visual PASS, explicitly not a pixel-diffed one).
- **Waived with the captures (not formally performed):** the 1440p higher-res capture, the
  odd-width/non-default-viewport capture, and the formal zoom-crop/pixel-aligned full-2D diff on thin HUD
  text + radial edges. Mitigations: resolution-independence holds *by construction* (correction is
  `1/w`,`1/h` in NDC from the live viewport size; no fixed pixel constants), and HUD/fonts/radial/
  char-select 2D rendered correctly in Kenny's live sessions. A sub-pixel (±0.5px) thin-text shift was
  not pixel-diffed; the forced-image harness reproduces the rigor later if wanted.

## Mechanism

Global XYZRHW position bias (consult judgment) confirmed correct: the c9 correction removed the seam, so
the named UV/sampler-edge fallback was NOT needed. No wrong-sign (seam didn't worsen) and no wrong-mechanism
(seam responded to the c9 bias).

## Artifacts

- `docs/research/phase12-baseline/COMPARISON.md` — Phase 18 section: seam FIXED, in-client verification
  record, image-independence via forced-image substitution, honest evidence-status note, D-04 unregressed,
  APPROVED verdict. (No image files added — captures waived.)
- Forced-image harness (outside the repo, not committed): `D:/swg_dev_bundle/_seam_variants/` + `seam_variant.ps1`
  + override slot `D:/swg_dev_bundle/texture/loading/tatooine/`. Run `seam_variant.ps1 native` to restore default (done).

## Verification

- UI-01 seam gone (rasterMajor=11): PASS — in-client, across 3 splash variants.
- Image-independent (≥3 variants): PASS — native Tatooine + Naboo + Corellia, all seam-free.
- Full-2D no-regression (D-03): char-select + general UI rendered correctly in-client; NOT formally pixel-diffed (captures waived) — accepted by Kenny.
- D3D9 unregressed (D-04): structurally guaranteed (gl11-only edit; no Direct3d9 file modified).
- No config files committed; no source modified by this plan (COMPARISON.md only).

## Self-Check: PASSED
