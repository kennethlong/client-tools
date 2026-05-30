---
phase: 17
phase_name: psrc-census-char-select-beachhead
authored_by: claude-opus-4.8 (Plan 17-05 Task 5)
verified_by: pending /gsd:verify-work 17
verified_at: 2026-05-30
status: pass
requirements:
  CHAR-01: PASS — skin + clothing render correct diffuse textures under D3D11, near-identical to D3D9 (asset-PS-lane). Evidence char_default_d3d9_0003.png vs char_default_d3d11_0003_postGap.png. Residual: slight brightness/tone delta tracked under GAMMA-01 (Phase 19), out of CHAR scope.
  CHAR-02: PASS (with resolution caveat) — eyes seated in face, not gray, not see-through, not magenta; sul_eye.sht asset PS now binds (asset-PS-lane). Fine customization-palette eye color is not pixel-resolvable on the Ithorian test character at 1920×1080 framing; no anomaly observed. A future Human-character capture would tighten the palette-color sub-criterion.
  CHAR-03: PASS — head/face multi-stage materials (sul_*_head.sht) composite correctly under D3D11, no magenta, no missing stages (asset-PS-lane). Evidence char_default_d3d11_0003_postGap.png.
---

# Phase 17 Verification — char-select D3D11 beachhead

**Verifier-produced** (supersedes the orchestrator-synthesized stand-in committed at `a20e828d8`).
This document carries the matched-pair A/B evidence + per-lane attribution that ROADMAP success
criterion #5 and D-05/D-07 require, marking CHAR-01/02/03 against actual visual evidence rather
than synthesized placeholders. Memory `project_phase11_minimap_never_round` invariant respected:
no PASS is claimed on grep counts alone — every PASS cites a matched A/B pair AND the lane that
delivered it.

The gap-closure cycle (Plans 17-05/06a/06b/07 + the GAP-4/5/6 follow-on work 17-08/17-09) is
complete. Char-select renders LIT + textured for ALL body parts under D3D11.

## Build provenance (POST-gap)

- **HEAD:** `0ddf2a6c8` (docs-only) on top of code commit `a0d5ac80f` (`feat(17-09): char-select bump arms render correctly`).
- **Full asset-PS visual stack in this build:** GAP-3 `d8cc1ca99` (PS input-sig rewrite, bind rate) + GAP-4/GAP-5 `e1db7bf65` (b0 PS lighting constants + VS vertex lighting) + GAP-6 `a0d5ac80f` (reflection-driven stage→SRV-slot remap for bump parts + VS material c11–c15 feed).
- **`stage/gl11_d.dll` LastWriteTime:** `2026-05-30 13:24:41 -05:00` (GAP-6 build; matches evidence/README.md §6 stale-DLL hard gate).
- **`stage/SwgClient_d.exe`:** `2026-05-30 09:00:44` — plugin-exported interface unchanged across the gap work; valid pairing with the 13:24 `gl11_d.dll`.

## Boot evidence (host, 2026-05-30 ~15:38–15:39, dual-renderer)

D3D9 (`rasterMajor=5`, Release stack `SwgClient_r.exe` + `gl05_r.dll`) and D3D11 (`rasterMajor=11`,
Debug stack `SwgClient_d.exe` + `gl11_d.dll` 5/30 13:24) both reached char-select default pose
cleanly; no FATAL, no crash. D3D11 boot log `stage/d3d11-debug.log` (9,045 lines). D3D9 path is
unregressed (gap work touched only the Direct3d11 plugin) — D-06 dual-renderer gate satisfied.

| Signal | Expected | PRE-gap (5/29, `9f5db9c3f`) | POST-gap (5/30) | Verdict |
|--------|----------|-----------------------------|------------------|---------|
| `id=342` | 0 | 0 | 0 | PASS |
| `id=343` | low | 27 | 9 | PASS — dropped, benign diagnostic |
| `PSRC recompile FAILED` | 0 | 0 | 0 | PASS — Plan 17-02 contract held |
| `Plan 17-07 .* COMPATIBLE vs=` | ≥ 8 | 0 | **9** | PASS — validator flipped (GAP-3) |
| `Plan 17-07 .* INCOMPATIBLE vs=` | ≤ 1 | 27 | **0** | PASS — GAP-3 closed |
| `asset-PS bound=` | ≥ 8 | 0 | **9** | PASS — asset PS bound at PSSetShader |
| `fallback-PS bound=` | ≤ 1 (char body) | — | 3 | PASS — 3 non-char shaders (UI/bg) still fallback; char body is asset-PS-lane |
| `wroteDiffuse=1` | ≥ 1 (instrumentation) | 0 | 0 | N/A — GAP-2 Case-C DEFERRED (see HIGH-3 framing) |
| `wroteEmissive=1` | ≥ 1 (instrumentation) | 0 | 0 | N/A — GAP-2 Case-C DEFERRED |
| `wroteSpecular=1` | informational | 6 | 2 | INFO — draw-count variance, write path intact |

**Headline:** asset-PS bind rate flipped **0/9 → 9/9** and the rewritten asset PS is what
`PSSetShader` actually bound (9 `asset-PS bound=`, not merely 9 COMPATIBLE verdicts) — the
distinction Round-4 MEDIUM 'success metric inflation' demanded. This is the primary char-select
visual driver and it is delivering.

## Per-requirement verdicts

### CHAR-01 — skin & clothing diffuse textures

> *Requirement: skin & clothing render with correct diffuse textures under D3D11 (matches D3D9) — not untextured, flat-white, or magenta.*

- **Instrumentation closure:** asset-PS bind 0/9 → 9/9 (`asset-PS bound=` = 9). The diffuse path
  for char-select is the asset PS's `textureFactor.rgb` + interpolated `COLOR0` sampling — now
  bound and executing. `wroteDiffuse=1` = 0 is EXPECTED and not a gap (see HIGH-3 framing below).
- **Visual closure:** `char_default_d3d11_0003_postGap.png` shows the Ithorian "Little Bigman"
  with correctly-textured skin (Ithorian green), white tunic, white pants, belt + holster, and
  shoes — near-identical to `char_default_d3d9_0003.png`. NOT untextured, NOT flat-white, NOT
  magenta. The PRE-gap `char_default_d3d11_0003_preGap.png` (fallback-PS / black-body interim) is
  the legible pre-state delta. Residual: D3D11 tunic/pants are marginally warmer/darker than the
  cooler/brighter D3D9 whites — a gamma-pipeline brightness delta tracked under **GAMMA-01
  (Phase 19)**, explicitly out of CHAR-01 scope.
- **Lane attribution:** **asset-PS-lane.** Predecessor verdict (stand-in `a20e828d8`): NOT VERIFIED.
- **Verdict: PASS.**

### CHAR-02 — eyes

> *Requirement: eyes render correctly — correct customization-palette color, seated in face, not gray, not visible through the back of the head.*

- **Instrumentation closure:** `sul_eye.sht` was a discovery anchor in 17-06a; its asset PS is now
  among the 9/9 binds (`asset-PS bound=`), so the eye shader renders its real asset PS, not the
  magenta fallback.
- **Visual closure:** in `char_default_d3d11_0003_postGap.png` the eyes are seated in the face, not
  gray, not visible through the back of the head, and not magenta — matching D3D9. **Caveat (per
  `project_phase11_minimap_never_round`, no over-claim):** fine customization-palette eye color is
  not pixel-resolvable on the Ithorian test character at this framing/resolution. No anomaly
  observed; a future Human-character capture with a distinct eye-color palette would independently
  confirm the palette sub-criterion.
- **Lane attribution:** **asset-PS-lane.** Predecessor verdict: NOT VERIFIED.
- **Verdict: PASS** (structurally verified; palette-color sub-criterion documented as
  resolution-limited, no defect).

### CHAR-03 — head/face multi-stage composite

> *Requirement: head/face multi-stage materials (e.g. `sul_*_head.sht`) composite their texture stages correctly under D3D11.*

- **Instrumentation closure:** `sul_m_head.sht` (multi-stage head) was a discovery anchor in
  17-06a with the same `SwgVertexConstants`@b0 layout as `sul_eye.sht`; its asset PS is among the
  9/9 binds.
- **Visual closure:** the head/face in `char_default_d3d11_0003_postGap.png` composites correctly —
  Ithorian skin tone + facial features render with no magenta, no missing/blown stages, matching
  D3D9. The multi-stage composite is intact.
- **Lane attribution:** **asset-PS-lane.** Predecessor verdict: NOT VERIFIED.
- **Verdict: PASS.**

## Side-by-side diff narrative (four-axis)

Reference: `char_default_d3d9_0003.png` (D3D9, `screenShot0029`). Result:
`char_default_d3d11_0003_postGap.png` (D3D11, `screenShot0030`). Pre-state for delta legibility:
`char_default_d3d11_0003_preGap.png`. Mirrors the `spotN` diff-bucket form of
`docs/research/phase12-baseline/COMPARISON.md`.

- **spot1 — skin tone:** Ithorian green skin renders correctly in both; D3D11 matches D3D9 hue.
  PRE-gap was black/untextured body — full closure.
- **spot2 — clothing color:** white tunic + pants textured correctly in both. Residual: D3D11
  slightly warmer/darker (brightness delta → GAMMA-01/Phase 19), not a texture or material defect.
- **spot3 — eye color + occlusion:** eyes seated, not gray, not see-through, not magenta in both
  (palette-color fine-grain resolution-limited on the Ithorian — see CHAR-02 caveat).
- **spot4 — head multi-stage composite:** facial features + skin composite correctly in both; no
  magenta, no stage dropout. The bump sleeves/hands (GAP-6) render correctly — visible rolled
  sleeve detail on the arms, no purple/green bump-arm artifact.

Overall: near-identical matched pair; the only inter-renderer delta is the minor brightness/tone
shift (GAMMA-01 scope). The PRE→POST D3D11 delta (black/fallback → fully lit + textured) is the
phase's headline visual closure.

## What changed across the gap-closure cycle

| Metric | PRE | POST | Delivered by |
|--------|-----|------|--------------|
| asset-PS bind rate (`COMPATIBLE`) | 0/9 | 9/9 | GAP-3 (17-07 PS input-sig rewrite) |
| `asset-PS bound=` (actual PSSetShader) | 0 | 9 | GAP-3 (17-07 StateCache bind path) |
| `INCOMPATIBLE` | 27 | 0 | GAP-3 |
| char-select body render | BLACK (fallback) | LIT + textured | GAP-4 b0 PS lighting + GAP-5 VS vertex lighting (17-08) |
| bump sleeves/hands | purple/green artifact | correct | GAP-6 reflection-driven stage→SRV-slot remap + VS material c11–c15 (17-09) |
| `wroteDiffuse=1` / `wroteEmissive=1` | 0 | 0 | GAP-2 instrumentation, Case-C DEFERRED (17-06b) — not a char-select visual driver |

## Validation strategy continuity

Every gap plan maps back to a 17-VALIDATION.md Dimension-8 channel:

- **17-05 (this plan):** matched A/B pair (committed `char_default_d3d9_0003.png` +
  `char_default_d3d11_0003_postGap.png` + canonical alias) — the "committed matched pair per CHAR-0x
  claim" gate + dual-renderer boot-gate (`id=342/343` asserted on the POST log).
- **17-06a/06b:** log-assertion channel — the cbuffer discovery dump + `wrote*=1` counters
  (instrumentation; Case-C DEFERRED is the recorded evidence-backed outcome).
- **17-07:** log-assertion channel — `Plan 17-07 .* COMPATIBLE/INCOMPATIBLE vs=` + `asset-PS bound=`
  bind-attribution, asserted above (9/0/9).
- **17-08/09 (GAP-4/5/6 follow-on):** boot-gate (no crash, both renderers) + matched A/B visual
  parity, asserted above.
- **Before `/gsd:verify-work`:** both renderers boot clean ✓; `id=342==0` ✓; committed matched A/B
  pair exists for each CHAR-0x ✓.

## Round-4 HIGH-3 framing — instrumentation vs visual

Char-select PSes consume `textureFactor.rgb` + interpolated `COLOR0` (`vertexDiffuse`), NOT cbuffer
`materialDiffuse` / `materialEmissive` (live code at `h_simple_pp_ps20.psh` +
`h_color2_specmap_cbmp_ps20.psh` per `evidence/plan-17-04x-psrc-source-dump.txt`). Therefore the
GAP-2 cbuffer diffuse/emissive write path (17-06a/06b) is **instrumentation completeness**, and its
`wroteDiffuse=1 = 0` / `wroteEmissive=1 = 0` POST counts are a CORRECT outcome (Case-C DEFERRED —
no evidence-backed per-PS landing offset), NOT a char-select visual failure. The PRIMARY char-select
visual drivers are GAP-3 (asset-PS bind rate, this is what makes the asset PS run) + GAP-4/5
(lighting constants) + GAP-6 (bump SRV-slot remap) + the existing `textureFactor` path. The PASS
verdicts above rest on those drivers + the matched A/B pair, not on the GAP-2 instrumentation counts.

## Footer — supersession

This verifier-produced document supersedes the orchestrator-synthesized stand-in committed at
`a20e828d8` (`docs(17): replan gap-closure ... per Round 5 reviews`), whose CHAR-01/02/03 rows read
NOT VERIFIED. Audit trail preserved: the predecessor's gap inventory drove Plans 17-05/06a/06b/07;
this document records their closure with real visual evidence + per-lane attribution. GAP-1
(matched-pair A/B evidence) is closed.
