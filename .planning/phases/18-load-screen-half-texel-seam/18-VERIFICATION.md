---
phase: 18-load-screen-half-texel-seam
status: passed
verified: 2026-05-30
requirements: [UI-01]
method: in-client human A/B (Kenny) + code/structural checks; screenshot captures waived
branch: koogie-msvc-cpp20-base
---

# Phase 18 Verification — Load-Screen Half-Texel Seam (UI-01)

**Goal:** Eliminate the D3D11 load-screen vertical centerline seam, image-independent, fixed centrally (not per-draw +0.5), with D3D9 byte-for-byte unregressed.

**Verdict: PASSED.**

## Must-haves vs reality

| Must-have | Status | Evidence |
|-----------|--------|----------|
| Central single-site fix (D-02), not per-draw +0.5 | ✅ | One edit in `Direct3d11_StateCache.cpp:setViewport` c9 `viewportData`; `git show fc6e41f10` = single `…/Direct3d11/…` file, no per-draw scatter |
| Consult-confirmed site + sign/magnitude (UI-01, D-01) | ✅ | Unanimous CODEX+Cursor (`CONSULT-18-…-SYNTHESIS.md`): `.z += 1/width`, `.w -= 1/height`, magnitude `1/w`,`1/h` |
| Resolution-independent (D-05) | ✅ | Correction = `1.0f/static_cast<float>(width\|height)`; no `0.5f`/`512`/`1024` literals (grep-verified) |
| `getOneToOneUVMapping` left a dead stub (D-01) | ✅ | `Direct3d11.cpp:1056` `scaffold_fatal_stub` untouched |
| gl11_d.dll relinks, 0 unresolved externals | ✅ | koogie build log: `unresolved external` count 0, no LNK errors, `/FORCE` not used; staged via worktree junction |
| Seam gone under rasterMajor=11 (UI-01) | ✅ | In-client, Kenny 2026-05-30 |
| Image-independent (≥3 variants, D-05) | ✅ | 3 distinct splashes (native Tatooine + Naboo + Corellia) via forced-image override, all seam-free in-client |
| No full-2D regression (D-03) | ✅ (observed) | Character renders correctly + UI rendered correctly in-client; **not** formally pixel-diffed (captures waived) |
| D3D9 byte-for-byte unregressed (D-04) | ✅ | No `…/Direct3d9/…` file modified (git diff scope); structural guarantee |

## Caveats / honest gaps

- **Screenshot evidence waived by Kenny** once the fix was visibly confirmed. PASS authority = direct in-client A/B observation, not a stored matched-pair diff (recorded plainly in `COMPARISON.md` to avoid the Iter-44B over-claim pattern).
- **Not formally performed:** 1440p higher-res capture, odd-width/non-default-viewport capture, and the sub-pixel zoom-crop diff on thin HUD text + radial edges. Resolution-independence holds by construction; gross 2D correctness confirmed in-client; a ±0.5px thin-text shift was not pixel-measured. Reproducible later via `D:/swg_dev_bundle/_seam_variants/seam_variant.ps1` if rigor is wanted.

## Process note

Phase 18 code+build was initially run on the wrong branch (`swg-blender-m16`, pre-GAP-4 → black character); fully recovered and re-executed on `koogie-msvc-cpp20-base` (the D3D11 source-of-truth). See memory `feedback_d3d11_work_belongs_in_koogie_worktree`. Planning artifacts ported to koogie; the stray swg-blender-m16 edit reverted.

## Self-Check: PASSED
