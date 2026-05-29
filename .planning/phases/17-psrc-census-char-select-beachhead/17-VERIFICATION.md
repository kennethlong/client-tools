---
phase: 17
phase_name: psrc-census-char-select-beachhead
verifier: orchestrator-synthesized-from-boot-evidence
verified_at: 2026-05-29
status: partial — infrastructure landed, requirements gated on gap-closure
source: |
  Synthesized from 17-04-SUMMARY.md (Task 3 checkpoint state), stage/d3d11-debug.log
  (Kenny's 2026-05-28 22:20 host boot under rasterMajor=11), and post-04 commit
  04ef66976 (writeVarByName recognizes materialSpecularColor). NOT produced by
  /gsd:verify-work — this stand-in is the canonical input for the /gsd:plan-phase 17
  --gaps invocation that follows. Authored by orchestrator on 2026-05-29 with Kenny's
  explicit "plan all 3 gaps" scope confirmation.
requirements:
  CHAR-01: NOT VERIFIED — no A/B screenshot captured against phase12-baseline/COMPARISON.md
  CHAR-02: NOT VERIFIED — no A/B screenshot captured; eye color/depth not screenshotted
  CHAR-03: NOT VERIFIED — multi-stage head A/B not captured
---

# Phase 17 Verification Gap Inventory (synthesized)

Phase 17 closed as "INFRASTRUCTURE COMPLETE" (commit `41d7f2fec`) with all four plans landed.
A formal `/gsd:verify-work 17` pass was never run. This document distills the boot evidence,
the Plan 17-04 SUMMARY's Task-3 checkpoint state, and the post-04 follow-up commit into the
gap inventory that `/gsd:plan-phase 17 --gaps` consumes.

## Boot evidence (host, 2026-05-28 22:20, rasterMajor=11)

Captured in `stage/d3d11-debug.log` (12,428 lines). Char-select reached cleanly; no FATAL.

| Signal | Expected (per 17-04 SUMMARY §Boot evidence placeholders) | Actual | Verdict |
|--------|---------------------------------------------------------|--------|---------|
| `id=342` count | 0 | 0 (clean) | PASS |
| `id=343` count | < 100 (Task 1 reduces from 24,408 baseline) | ≈ 0 (replaced by per-pair-compat dual-route INFO) | PASS — instrumentation supplanted device-id firehose |
| `PSRC recompile FAILED` count | 0 (Plan 17-02 contract) | 0 | PASS |
| `wroteDiffuse=1` count | > 0 (Task 2 lands diffuse for at least one shader) | 0 across all anchors | **FAIL** |
| `wroteSpecular=1` count | (informational) | ≥ 9 (recent commit `04ef66976` lands materialSpecularColor) | PASS (post-04 fix) |
| `wroteEmissive=1` count | (informational) | 0 across all anchors | **FAIL** |
| Asset-PS bind rate (`COMPATIBLE` vs `INCOMPATIBLE`) | "any" per PLAN — both acceptable | 0 COMPATIBLE / 9 INCOMPATIBLE (all asset PSes rejected → selectFallbackPSForVS owns every bind) | **NOT-AS-DESIGNED** |
| Visual A/B screenshot `char_default_d3d11_0003.png` | textured character at char-select default pose | NOT CAPTURED | **MISSING** |
| Matched-pair diff vs `docs/research/phase12-baseline/COMPARISON.md` | required by ROADMAP success criterion #5 + memory `[Iter-44B minimap over-claim lesson]` | NOT PRODUCED | **MISSING** |

### Per-anchor cbuffer discovery dump (sul_eye.sht + sul_m_head.sht, identical layout)

The Task 2 one-shot dump fired (`wrote(D,S,E)=(0,1,0)` triggered the discovery gate) and
enumerated the **real** reflected variable list for `SwgVertexConstants` at b0:

```
totalSize=400  varsCount=9  layoutName='SwgVertexConstants'  bindPoint=0
  var[0]:  packedRegister0      offset=  0   size=16
  var[1]:  packedRegister1      offset= 16   size=16
  var[2]:  packedRegister2      offset= 32   size=16
  var[3]:  packedRegister3      offset= 48   size=16
  var[4]:  packedRegister4      offset= 64   size=16
  var[5]:  textureFactor        offset= 80   size=16
  var[6]:  textureFactor2       offset= 96   size=16
  var[7]:  materialSpecularColor offset=112  size=16
  var[8]:  userConstants        offset=128   size=272  (17 × float4)
```

Plan 17-04's `material[0/1/2]` array-element hypothesis is **falsified** — there is no
`material[N]` array in this reflection. The only `material*` name is
`materialSpecularColor`; `materialDiffuseColor` / `materialEmissiveColor` are either
packed into `packedRegister0..4` per the `Direct3d11_HlslRewrite.cpp` Rule D wrap, or
live inside `userConstants` (17 float4 slots, currently opaque).

## Gap inventory (drives the plan_phase --gaps planning pass)

### GAP-1 — CHAR-01/02/03 A/B verification not captured (BLOCKING for requirements close)

- Phase 17 success criteria require D3D9-vs-D3D11 matched-pair screenshot diff against
  `docs/research/phase12-baseline/COMPARISON.md`. Memory `[Iter-44B minimap over-claim
  lesson]` explicitly bans "no magenta == done" claims — every parity claim needs a diff.
- `char_default_d3d11_0003.png` (Task 3 checkpoint deliverable per 17-04 PLAN) was never
  captured into `.planning/phases/17-psrc-census-char-select-beachhead/evidence/`.
- Phase cannot formally close on CHAR-01/02/03 without this artifact pair.

**Scope-of-fix:** capture both screenshots at char-select default pose (D3D9 = rasterMajor=5
baseline, D3D11 = rasterMajor=11), commit them to `evidence/`, produce a written
side-by-side diff (skin tone / clothing color / eye color+occlusion / head shading) against
the phase12-baseline pair, write a real `17-VERIFICATION.md` that overrides this synthesized
stand-in, mark CHAR-01/02/03 as VERIFIED-or-DEFERRED with rationale.

### GAP-2 — `materialDiffuseColor` + `materialEmissiveColor` writes never land

- 17-04 Task 2 went 1-of-3 on material writes: `wroteSpecular=1` (after commit `04ef66976`'s
  `materialSpecularColor` recognition), but `wroteDiffuse=0` and `wroteEmissive=0` across
  every char-select anchor.
- The reflected cbuffer at b0 exposes `packedRegister0..4` + `materialSpecularColor` +
  `userConstants` — no `materialDiffuse*` / `materialEmissive*` scalar names exist for
  `writeVarByName` to bind to.
- This is the slot-mapping problem: diffuse + emissive material constants are packed into
  the generic `packedRegisterN` slots per `Direct3d11_HlslRewrite.cpp` Rule D wrap, and
  the executor needs to discover which packedRegisterN.{xyzw} channel each one occupies.

**Scope-of-fix:** read `Direct3d11_HlslRewrite.cpp` Rule D wrap source to learn the canonical
packing layout; cross-check against `vertex_shader_constants.inc` if it exists in the asset
TRE; map D3D9 register c[N] → SwgVertexConstants packedRegister[K].channel using the rewrite
table; extend `writeVarByName` in `Direct3d11_StaticShaderData::apply()` to write
diffuse/emissive into the correct packedRegister channels with fallback to current
`materialDiffuse/materialEmissive` scalar names for PSes that don't `#include` the VS rewrite
header; verify post-boot that `wrote(D,S,E)=(1,1,1)` on at least sul_eye and sul_m_head
anchors; the discovery dump self-suppresses once writes complete.

### GAP-3 — Asset-PS bind rate is 0% (COLOR0 register-position disagreement)

- All 9 logged (VS, PS) pairs returned `INCOMPATIBLE` from Plan 17-04's `isCompatibleWithVS`
  validator. Concrete reason in every case: `"ps input 'COLOR0' expects register v0 but vs
  writes semantic at register o1 (D3D11 stage linkage is register-position-strict; id=343)"`.
- The Phase 11 Iter-4 dynamic PS fallback (`selectFallbackPSForVS`) owns 100% of asset-PS
  bind sites. The Plan 17-02/03/04 asset-PS recompile lane never won a single bind.
- This contradicts the Phase 17 GOAL ("Prove the asset pixel-shader pipeline end-to-end on
  the deterministic, isolated character-select screen"). The infrastructure landed but the
  beachhead's stated purpose — that the asset-PS lane *actually delivers* CHAR-01/02/03 —
  was not demonstrated. Whatever the visual A/B shows in GAP-1, it shows the dynamic-fallback
  PS's output, not the asset-PS lane.

**Scope-of-fix:** root-cause why VS writes COLOR0 at output register `o1` instead of `o0`
when the asset PS declares its input as `COLOR0 : v0`. Two reasonable axes:
  (a) VS recompile lane (Phase 11 Iter-3 `buildHlslForVSOutputs`) emits outputs in
      declaration order which doesn't always start COLOR at `o0` — fix by output-reorder
      so COLOR0 lands at register 0 in the VS bytecode, OR
  (b) PS input-signature rewrite — rewrite the asset PSRC's input struct so `COLOR0`'s
      register-position matches whichever slot the VS actually wrote it at (mirroring
      `buildHlslForVSOutputs` but applied to the PS side).
Either path must keep the existing dynamic-fallback PS as the safety net (do NOT remove
`selectFallbackPSForVS`); the gate just needs to flip from 0/9 → ≥ 8/9 COMPATIBLE so the
asset-PS lane actually demonstrates parity. Per-pair-compat log lines provide the success
metric.

## What Phase 17 actually demonstrates today

- ✓ Discovery census ran (Plan 17-01).
- ✓ Recompile lane plumbed + DXBC retain + reflection cache (Plan 17-02).
- ✓ Per-pass cbuffer upload with offset-aware reflection (Plan 17-03).
- ✓ VS↔PS pair-compat validator at bind time + materialSpecularColor wires (Plan 17-04 + 04ef976).
- ✗ Visual A/B verification artifact (GAP-1).
- ✗ Diffuse + Emissive material writes (GAP-2).
- ✗ Asset-PS lane winning binds (GAP-3) — the literal goal of the beachhead.

## Cross-cutting landmines for executors of 17-05/06/07

- **Shared-header struct touches break stale plugin dll ABI** — memory
  `[Shared-header struct touches break stale plugin dll ABI]`. Any change to
  `ShaderImplementation.h` or `Direct3d11_PixelShaderProgramData.h` requires a full plugin
  rebuild before next boot. Use `$env:MSBUILD` from settings.json.
- **D3D9 reference path stays untouched.** Memory `[D3D9 Compare[] table swap]`,
  PROJECT.md's "never regress D3D9". GAP-3 fixes must NOT modify
  `Direct3d9_*.{cpp,h}`.
- **Don't re-enable per-pass blend factors early** (Iter-44C amplification regression
  — already a landmine in CONTEXT.md).
- **cbuffer matrix uploads need `XMMatrixTranspose`** — memory
  `[D3D11 cbuffer matrices must be transposed at upload]`. Probably not relevant to 17-05/06
  but flag it for 17-07 if any VS-output-reorder pass touches matrix register packing.
- **stage/client_d.cfg accumulated test settings** — memory
  `[stage/client_d.cfg deferred cleanup]`. Cleanup is post-visual-parity, NOT in scope here.
- **Debug exe reads client_d.cfg** — memory `[Debug exe reads client_d.cfg]`. If any plan
  flips a rasterMajor or other config flag for verification, edit the right file.

## Validation strategy (existing — VALIDATION.md still applies)

The Phase 17 `17-VALIDATION.md` validation architecture is unchanged. Gap-closure plans
17-05/06/07 must each map their tasks back to Validation Architecture dimensions per the
existing strategy — boot-gate every shared-header edit on `rasterMajor=5` AND `=11`,
preserve the dual-route log discipline (DEBUG_REPORT_LOG_PRINT + InfoQueue), and keep
discovery-dump diagnostic gates (one-shot per anchor, fires only on failure signature).

---
*Phase: 17-psrc-census-char-select-beachhead*
*Verification synthesized: 2026-05-29 — replace with real /gsd:verify-work output once GAP-1's A/B capture closes CHAR-01/02/03.*
