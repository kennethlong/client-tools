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

  AMENDED 2026-05-29 (Round-4 cross-AI review fold-in): GAP-2 scope-of-fix and GAP-3
  scope-of-fix narratives below reflect HIGH-1 (writeVarByName cannot write sub-channels),
  HIGH-2 (bind decision lives in StateCache.cpp), HIGH-3 (char-select PSes don't consume
  cbuffer material colors), HIGH-4 (PSRC syntax is parameter-list not struct-bound),
  HIGH-5 (HlslRewrite Rule D contains no c[N]→packedRegister mapping table — Path A dead
  end), and HIGH-6 (m_reflectedPSInputs is ctor-time, requires per-VS cache for rewritten
  lane). Gap-closure plans split to 17-05 + 17-06a + 17-06b + 17-07.
requirements:
  CHAR-01: NOT VERIFIED — no A/B screenshot captured against phase12-baseline/COMPARISON.md
  CHAR-02: NOT VERIFIED — no A/B screenshot captured; eye color/depth not screenshotted
  CHAR-03: NOT VERIFIED — multi-stage head A/B not captured
---

# Phase 17 Verification Gap Inventory (synthesized; Round-4 amended 2026-05-29)

Phase 17 closed as "INFRASTRUCTURE COMPLETE" (commit `41d7f2fec`) with all four plans landed.
A formal `/gsd:verify-work 17` pass was never run. This document distills the boot evidence,
the Plan 17-04 SUMMARY's Task-3 checkpoint state, and the post-04 follow-up commit into the
gap inventory that `/gsd:plan-phase 17 --gaps` consumes. The Round-4 cross-AI review (Codex +
Cursor) corrected the scope-of-fix narratives below; the underlying gaps are unchanged.

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
`materialSpecularColor`; `materialDiffuseColor` / `materialEmissiveColor` live inside
`userConstants` (17 float4 slots, opaque at the top-level dump — Plan 17-06a extends the
discovery to walk userConstants's inner reflection).

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
stand-in, mark CHAR-01/02/03 as VERIFIED-or-DEFERRED with rationale. Plan 17-05 owns this gap
(PRE/POST capture cycle + canonical-alias PNG creation + verifier-produced replacement).
Per Round-4 MEDIUM 'success metric inflation', the verdict per CHAR-0x distinguishes
'instrumentation closed' from 'visual fixed' and attributes the delivering lane (asset-PS
vs fallback-PS) using Plan 17-07's new `asset-PS bound=` log.

### GAP-2 — `materialDiffuseColor` + `materialEmissiveColor` writes never land (INSTRUMENTATION COMPLETENESS — char-select visual impact LOW per Round-4 HIGH-3)

- 17-04 Task 2 went 1-of-3 on material writes: `wroteSpecular=1` (after commit `04ef66976`'s
  `materialSpecularColor` recognition), but `wroteDiffuse=0` and `wroteEmissive=0` across
  every char-select anchor.
- The reflected cbuffer at b0 exposes `packedRegister0..4` + `materialSpecularColor` +
  `userConstants` — no `materialDiffuse*` / `materialEmissive*` scalar names exist for
  `writeVarByName` to bind to.
- This is the slot-mapping problem: diffuse + emissive material constants live INSIDE
  `userConstants`'s 17 float4 slots (top-level reflection is opaque); the executor must
  discover the inner layout from D3DReflect.

**Round-4 HIGH-3 reframing (CONVERGED, both reviewers):** Char-select shaders (`h_simple_pp_ps20.psh`,
`h_color2_specmap_cbmp_ps20.psh` per `evidence/plan-17-04x-psrc-source-dump.txt`) consume
`textureFactor.rgb` (top-level cbuffer var, already-written by 04ef976) + interpolated
`COLOR0` (vertex-shader output `vertexDiffuse`), NOT cbuffer `materialDiffuse` /
`materialEmissive`. So GAP-2 closure (across 17-06a + 17-06b together) is INSTRUMENTATION
COMPLETENESS — closing the metric (`wroteDiffuse=1` + `wroteEmissive=1`) DOES land the
infrastructure for FUTURE open-world shaders that consume cbuffer material colors, but
does NOT materially drive char-select visuals. The PRIMARY char-select visual driver
is GAP-3 (asset-PS bind rate) + the existing textureFactor path. Plan 17-05 Task 5's
per-CHAR-0x verdict makes this distinction explicit.

**Round-4 HIGH-5 reframing (CONVERGED, both reviewers):** Rule D in `Direct3d11_HlslRewrite.cpp:747-767`
only wraps `: register(cN)` → `: packoffset(cN)` while preserving original declaration
names — it contains NO `c[N] → packedRegister[K].channel` translation table. The prior
plan's "Path A (Rule D source-read) FIRST, Path B (discovery dump extension) as fallback"
framing was a dead end on the Path A side. The diffuse + emissive material values do NOT
live in `packedRegisterN.{xyzw}` channels — they live INSIDE `userConstants`'s opaque 272-byte
region. The gap-closure plans default to Path B: extend the discovery dump to walk
userConstants's inner reflection; derive the mapping from the inner-field evidence.

**Round-4 HIGH-1 reframing (CONVERGED, both reviewers):** The existing `writeVarByName`
lambda at `Direct3d11_StaticShaderData.cpp:794` does `std::strcmp(var.Name, varName)` against
TOP-LEVEL `layout.Vars` only — it cannot reach nested members (e.g. `userConstants.material.diffuse`)
or array elements (e.g. `userConstants[N]`). Closing GAP-2 requires a new helper
`writeVarFloat4AtOffset(absoluteOffset, value)` that takes an explicit byte-offset (derived
from the discovery-dump evidence) and writes there directly with the same bounds-check
discipline. The new helper is a strict superset capability; the existing writeVarByName is
preserved for simple top-level cases (textureFactor, textureFactor2, materialSpecularColor).

**Scope-of-fix:** Split into two plans per Round-4 verdict:
  - **Plan 17-06a (discovery):** extend the existing 17-04 Task 2 one-shot cbuffer-vars-discovery
    dump to walk userConstants's inner reflection via `ID3D11ShaderReflectionType::GetMemberTypeByIndex`
    (or by re-reflecting if the cached layout doesn't carry type info). Emit one log line per
    inner float4 (or struct member); Kenny boots once, pastes the evidence to the executor.
  - **Plan 17-06b (mapping + write):** add the `writeVarFloat4AtOffset` helper; extend the
    `wroteDiffuse` + `wroteEmissive` candidate chains with offsets derived from 17-06a evidence
    (Case A — named members → use the captured offsets; Case B — flat float4 array → cross-reference
    `Direct3d11.cpp::setVertexShaderUserConstants_impl` for the engine-side slot order; Case C —
    opaque even at inner walk → close as DEFERRED, document in SUMMARY). Both plans are plugin-local
    Direct3d11_StaticShaderData.cpp edits — no shared file touched.

  Verify post-boot that `wrote(D,S,E)=(1,1,1)` on at least sul_eye and sul_m_head anchors (Plan 17-05
  Task 4); the discovery dump self-suppresses once writes complete. PASS metric ≠ char-select
  visual delivery per HIGH-3 — Plan 17-05 Task 5's verdict makes this distinction.

### GAP-3 — Asset-PS bind rate is 0% (COLOR0 register-position disagreement; PRIMARY char-select visual driver per Round-4 HIGH-3)

- All 9 logged (VS, PS) pairs returned `INCOMPATIBLE` from Plan 17-04's `isCompatibleWithVS`
  validator. Concrete reason in every case: `"ps input 'COLOR0' expects register v0 but vs
  writes semantic at register o1 (D3D11 stage linkage is register-position-strict; id=343)"`.
- The Phase 11 Iter-4 dynamic PS fallback (`selectFallbackPSForVS`) owns 100% of asset-PS
  bind sites. The Plan 17-02/03/04 asset-PS recompile lane never won a single bind.
- This contradicts the Phase 17 GOAL ("Prove the asset pixel-shader pipeline end-to-end on
  the deterministic, isolated character-select screen"). The infrastructure landed but the
  beachhead's stated purpose — that the asset-PS lane *actually delivers* CHAR-01/02/03 —
  was not demonstrated. Per Round-4 HIGH-3, GAP-3 IS the primary char-select visual driver:
  closing it activates the asset-PS lane so the `textureFactor.rgb` + interpolated COLOR0
  feeds (the actual char-select visual ingredients) flow through asset-specific PS logic
  instead of through the magenta-tinged dynamic fallback.

**Round-4 HIGH-4 reframing (CONVERGED, both reviewers):** The captured PSRC dump at
`evidence/plan-17-04x-psrc-source-dump.txt` shows 22 `float4 main(in <type> <name> : <SEMANTIC>, ...)`
parameter-list-syntax PSes; 0 struct-bound `main` PSes. The dominant char-select form is the
PARAMETER LIST, not a struct. The rewriter MUST target `main()` parameter declarations as
PRIMARY; the struct-bound path is a rare-asset fallback (zero char-select usage). The prior
plan's "struct bound to `main` parameter type" framing was wrong on the dominant case.

**Round-4 HIGH-2 reframing (CONVERGED, both reviewers):** The asset-PS bind decision lives in
`Direct3d11_StateCache.cpp:1121-1137` (the
`if (... isCompatibleWithVS) { psToBind = ms_currentPSData->getPixelShader(); } else if (... fallback)`
block). Any rewritten-PS lookup MUST happen at or below that call site. The prior plan's
single-file-scope `Direct3d11_PixelShaderProgramData.cpp`-only declaration was dishonest.
Plan 17-07 declares `Direct3d11_StateCache.cpp` + `Direct3d11_PixelShaderProgramData.h` in
files_modified upfront, with explicit ABI rebuild discipline per memory
`[Shared-header struct touches break stale plugin dll ABI]` (the STAGE 1 BLOCKING full-plugin
rebuild step that rebuilds ALL .vcxprojs against the new header).

**Round-4 HIGH-6 reframing (CONVERGED, both reviewers):** `m_reflectedPSInputs` is set ONCE
at ctor-time per `Direct3d11_PixelShaderProgramData.cpp:1060-1081`. A per-VS rewritten PS has
DIFFERENT reflected inputs, but the cached field stays from the ctor-time native PS — so
`isCompatibleWithVS` reading that cache returns INCOMPATIBLE for the rewritten lane. Plan
17-07's per-VS rewrite cache stores the per-VS reflected inputs alongside the rewritten PS,
and a new `isCompatibleWithVS_withExplicitPSInputs` overload validates the rewritten lane
against the per-VS data. The native-path `isCompatibleWithVS(vsData, psData)` is UNCHANGED
for the ctor-compile case — only the rewritten lane uses the explicit-inputs overload.
Acceptance relaxed to: `"isCompatibleWithVS unchanged for native ctor compile; rewritten PS
validated via per-VS reflected inputs before bind"`.

**Round-4 MEDIUM 'success metric inflation' reframing:** Plan 17-07 emits a NEW
`Plan 17-07 asset-PS bound=` / `Plan 17-07 fallback-PS bound=` attribution log at the
PSSetShader call site distinct from the existing `COMPATIBLE vs=` validator log. The
COMPATIBLE log proves the validator returned COMPATIBLE; the asset-PS-bound log proves
PSSetShader actually bound the asset PS (vs the fallback). Both metrics must move for
GAP-3 to close on the primary axis (visual lane delivery), not just the secondary axis
(validator passing).

**Scope-of-fix:** Plan 17-07. Implementation per Round-4 amendments:
  - Add `rewritePsMainParameterListForVSOutputs` helper (primary char-select form per HIGH-4)
  - Add `rewritePsMainStructBoundParameterForVSOutputs` as rare-asset fallback
  - Add per-(VS, PS) rewrite cache (private member of PSData; lazily populated; keyed by VS pointer + VS-output-signature-hash MEDIUM-salt)
  - Add public `tryGetOrBuildRewrittenPSForVS` method on PSData.h (HIGH-2 — shared-header ABI rebuild discipline applies)
  - Add `isCompatibleWithVS_withExplicitPSInputs` overload (HIGH-6)
  - Extend StateCache.cpp bind decision (HIGH-2): rewritten-PS > native-PS > fallback-PS
  - Add `Plan 17-07 asset-PS bound=` / `Plan 17-07 fallback-PS bound=` attribution log (MEDIUM)
  - Bump D3D11_REWRITE_VERSION from "20" to "21" (cache invalidation)
  - Pre-execution spike (MEDIUM 'targeted COLOR0/TEXCOORD-only spike') in Plan 17-07 Task 0 before full implementation

  Keep the existing dynamic-fallback PS as the safety net (do NOT remove `selectFallbackPSForVS`);
  the gate just needs to flip from 0/9 → ≥ 8/9 COMPATIBLE AND ≥ 8/9 `asset-PS bound=` so the
  asset-PS lane actually demonstrates parity AT THE BIND.

## What Phase 17 actually demonstrates today

- ✓ Discovery census ran (Plan 17-01).
- ✓ Recompile lane plumbed + DXBC retain + reflection cache (Plan 17-02).
- ✓ Per-pass cbuffer upload with offset-aware reflection (Plan 17-03).
- ✓ VS↔PS pair-compat validator at bind time + materialSpecularColor wires (Plan 17-04 + 04ef976).
- ✗ Visual A/B verification artifact (GAP-1 — Plan 17-05).
- ✗ Diffuse + Emissive material writes (GAP-2 — Plans 17-06a + 17-06b; INSTRUMENTATION ONLY per HIGH-3).
- ✗ Asset-PS lane winning binds (GAP-3 — Plan 17-07; PRIMARY char-select visual driver per HIGH-3).

## Cross-cutting landmines for executors of 17-05/06a/06b/07

- **Shared-header struct touches break stale plugin dll ABI** — memory
  `[Shared-header struct touches break stale plugin dll ABI]`. Plan 17-07 touches
  `Direct3d11_PixelShaderProgramData.h` (per Round-4 HIGH-2). Its build invocation
  MUST include the STAGE 1 [BLOCKING] full-plugin rebuild step (`/t:Rebuild` not
  `/t:Direct3d11:Rebuild`). Use `$env:MSBUILD` from settings.json + `/nodeReuse:false`.
  Plan 17-06a + 17-06b do NOT touch any header — single-target build is sufficient.
- **D3D9 reference path stays untouched.** Memory `[D3D9 Compare[] table swap]`,
  PROJECT.md's "never regress D3D9". GAP-3 fixes must NOT modify
  `Direct3d9_*.{cpp,h}`. (Plan 17-07's StateCache + PSData edits are plugin-local Direct3d11.)
- **Don't re-enable per-pass blend factors early** (Iter-44C amplification regression
  — already a landmine in CONTEXT.md). None of 17-05/06a/06b/07 touch blend factors.
- **cbuffer matrix uploads need `XMMatrixTranspose`** — memory
  `[D3D11 cbuffer matrices must be transposed at upload]`. N/A 17-05/06a/06b (no matrix
  touches); N/A 17-07 (rewrite is HLSL-source-level declaration shuffle, no matrix constants).
- **stage/client_d.cfg accumulated test settings** — memory
  `[stage/client_d.cfg deferred cleanup]`. Cleanup is post-visual-parity, NOT in scope.
- **Debug exe reads client_d.cfg** — memory `[Debug exe reads client_d.cfg]`. If any plan
  flips a rasterMajor or other config flag for verification, edit the right file.
- **17-05 dependency cycle** (Round-4 MEDIUM): downstream plans (17-06a, 17-06b, 17-07) declare
  `depends_on: ["17-04", "17-05-task3"]` — the post-Task-3 partial dependency — NOT
  `["17-04", "17-05"]` which would create a cycle (17-05 parks at Task 3 to commit PRE-gap
  evidence; resumes at Task 4 only after 17-07 lands).
- **Wave structure** (Round-4 fold-in): Wave 5 = 17-05 Tasks 1-3 (PRE-gap capture). Wave 6 =
  17-06a (discovery boot evidence). Wave 7 = 17-06b (mapping/write) + 17-07 (PS rewrite +
  bind-path wiring) — both wave-7 plans rebuild gl11_d.dll serially under the shared-header
  ABI trap discipline; 17-07's full-plugin rebuild also rebuilds 17-06b's plugin DLL
  consistently. After wave-7 lands, 17-05 resumes Task 4 (POST-gap capture) + Task 5
  (verifier-produced 17-VERIFICATION.md replacement).

## Validation strategy (existing — VALIDATION.md still applies)

The Phase 17 `17-VALIDATION.md` validation architecture is unchanged. Gap-closure plans
17-05/06a/06b/07 must each map their tasks back to Validation Architecture dimensions per
the existing strategy — boot-gate every shared-header edit on `rasterMajor=5` AND `=11`,
preserve the dual-route log discipline (DEBUG_REPORT_LOG_PRINT + InfoQueue), and keep
discovery-dump diagnostic gates (one-shot per anchor, fires only on failure signature).
Per Round-4 MEDIUM 'capture reproducibility', Plan 17-05 Task 1's evidence/README.md pins
the deterministic capture contract (character slot, camera, UI state, resolution, gamma).

---
*Phase: 17-psrc-census-char-select-beachhead*
*Verification synthesized: 2026-05-29 — Round-4 amended 2026-05-29 — replace with real /gsd:verify-work output once Plan 17-05 Task 5 lands the verifier-produced supersession.*
</content>
</invoke>