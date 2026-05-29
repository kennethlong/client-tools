---
phase: 17-psrc-census-char-select-beachhead
plan: 07
type: execute
status: complete
gap_closure: true
requirements: [CHAR-01, CHAR-02, CHAR-03]
completed: 2026-05-29
commits:
  - f9e5ac569  # feat(17-07): PS parameter-list rewrite axis-(b) + per-VS cache + bind-path attribution
  - 5a1f9c1bf  # docs(17-07): POST-gap build provenance
  - 07f56a1cb  # diag(17-07): rewrite-diag + tombstone reasons + grep-contract log format
  - d8cc1ca99  # feat(17-07): redesign rewriter to RECONSTRUCT PS input sig vs VS skeleton
---

# Plan 17-07 SUMMARY — GAP-3 closed (asset-PS bind rate 0/9 → 9/9); GAP-4 (black) discovered

## Outcome

**GAP-3 CLOSED on both measurable axes.** Kenny's POST-gap rasterMajor=11 boot (2026-05-29 17:26):
- `Plan 17-07 rewritten-lane COMPATIBLE vs=` = **9** (≥8 required)
- `Plan 17-07 asset-PS bound=` = **9**, all `path=rewritten` (≥8 required)
- `rewritten-lane INCOMPATIBLE` = 0 · `fallback-PS bound=` = 3 (the non-`//hlsl` UI shaders, by design)

The asset pixel-shader lane now WINS the bind for 9/9 char-select body shaders — the dynamic fallback
no longer owns every bind site. The Phase 17 beachhead goal ("prove the asset-PS pipeline end-to-end")
is proven *bindable* end-to-end.

**GAP-4 DISCOVERED (new blocker, NOT a 17-07 failure):** with the asset PS now delivering pixels, the
char-select body renders a **totally black silhouette** — the asset PS reads zero lighting/material
constants. Root cause is a cbuffer slot/layout mismatch (see Key Decisions + Boot Evidence). A cross-AI
consult is in flight (`.planning/research/CONSULT-17-07-gap4-cbuffer-reconcile*`). GAP-4 is out of
17-07's scope (GAP-3 = bind rate, which is met).

## Dependency graph

- **Requires:** 17-04 (isCompatibleWithVS validator + selectFallbackPSForVS), 17-05-task3 (PRE-gap
  baseline commit), 17-06b (Wave-7 predecessor; both rebuild gl11_d.dll — 17-07 built last).
- **Provides:** PS VS-signature reconstruction rewriter + per-VS rewrite cache + bind-path attribution
  log. Consumed by Plan 17-05 Task 4 (POST-gap measurement) and any future GAP-4 constant-reconcile plan.

## Key files modified

| File | Change |
|---|---|
| `Direct3d11_PixelShaderProgramData.cpp` | `rewritePsMainParameterListForVSOutputs` (now RECONSTRUCTS the PS input sig vs the VS skeleton — see redesign below) + `rewritePsMainStructBoundParameterForVSOutputs` (rare-asset fallback, returns Failed) + `tryGetOrBuildRewrittenPSForVS` (const, mutable per-VS cache) + `isCompatibleWithVS_withExplicitPSInputs` overload + `rewrite-diag`/`TOMBSTONE` instrumentation + cache-key VS-output-sig salt + `D3D11_REWRITE_VERSION` 21→22 (both define sites) |
| `Direct3d11_PixelShaderProgramData.h` | public `tryGetOrBuildRewrittenPSForVS` + `isCompatibleWithVS_withExplicitPSInputs` + `getFileName` + `PerVsRewriteEntry` mutable cache; `<map>`/`<utility>` |
| `Direct3d11_StateCache.cpp` | bind priority native > rewritten > fallback; `emitPlan1707BindAttribution` (`asset-PS bound=` / `fallback-PS bound=`, path=native|rewritten) |
| `Direct3d11_VertexShaderData.{h,cpp}` | `computeOutputSignatureHash()` (FNV-1a over sorted outputs) — cache salt + raw-VS-pointer-reuse staleness guard |

Build: `gl11_d.dll` rebuilt (Debug Win32, 0 errors / 0 unresolved externals in Direct3d11 + SwgClient
projects). Headers are **Direct3d11-plugin-internal** → only `gl11_d.dll` relinks; `SwgClient_d.exe`
(5/28) is unchanged and compatible. This is NOT the `ShaderImplementation.h` shared-header ABI trap;
the all-plugin-rebuild mandate in the plan was over-cautious (empirically confirmed: incremental build
relinked only gl11_d.dll).

## Key decisions

1. **Axis-(b) "reorder the parameter list" was WRONG — the mismatch is a register-BASE offset, not an
   ordering problem.** The first boot's `rewrite-diag` proved every body shader's PS input semantics were
   ALREADY in VS order (`status=Unchanged`); the VS reserves output register o0 for SV_Position so its
   varyings start at o1, while the asset PS starts varyings at v0. Reorder can never add the +1 base shift,
   and some PSes skip a VS output (needs padding). See `project_17_07_ps_register_base_offset`.

2. **Fix = RECONSTRUCT the PS input signature to mirror the VS output signature** (the proven
   `buildHlslForVSOutputs` invariant): emit a `PsIn1707` struct with `SV_Position`@reg0 + one field per VS
   output in register order with matching type/mask, rename the asset `main`→`assetMain1707`, and call it
   from a new `main(PsIn1707)` sourcing each asset param from its mirror field. The compiler re-derives the
   SAME registers + component packing as the VS (including the +1 base and skipped-output padding) → register-
   compatible by construction. Asset shading body preserved verbatim. Guards: float4-entry-only,
   SV_Position-only system input, unmatched consumed input → Failed → fallback.

3. **GAP-4 root cause (the black):** the asset PS reads `SwgVertexConstants`@**b0** (rewriter layout:
   `packedRegister0..4` c-register globals + `textureFactor`/`textureFactor2`/`materialSpecularColor` +
   `userConstants[17]`@128). But the engine's `setPixelShaderUserConstants` flushes the real constant values
   to **b2** (`PerMaterialCB` layout, `Direct3d11.cpp:754`), and `StaticShaderData::apply` writes only
   material/textureFactor to b0 (zeroing `packedRegister0..4` + `userConstants[]`). The c-register light
   globals reach b0 from nobody (HlslRewrite.cpp:800-805 deferred "Plan 11-08" push). → asset PS reads zero
   lighting → black. **Corrects the prior assumption** that char-select needs only `textureFactor`+`COLOR0` —
   the lit body shaders need `SwgVertexConstants` (light + material).

4. **Task 0 spike deferred to Kenny's boot** (the executor cannot boot the client). The decisive
   `rewrite-diag` instrumentation (reflected PS-input registers vs VS-output registers) served as the real
   compile+reflect register check the spike specified — and it correctly localized both the base-offset
   finding and the GAP-4 constant gap.

## Patterns established

- **VS-signature-reconstruction wrapper** (`rewritePsMainParameterListForVSOutputs`): mirror the VS output
  signature exactly + wrap the asset's renamed entry — reusable for any asset PS whose register base differs
  from the VS. Generalizes `buildHlslForVSOutputs` to preserve real asset shading.
- **Per-VS rewrite cache** keyed on `(VS*, computeOutputSignatureHash())` with raw-pointer-reuse staleness
  guard + `.cso` cache-key salt.
- **rewrite-diag / TOMBSTONE instrumentation** as the boot-validation harness for the deferred spike.

## Metrics

| | PRE-gap (17-05 baseline) | POST-gap (this plan) |
|---|---|---|
| asset-PS bound | 0/9 | **9/9** |
| rewritten-lane COMPATIBLE | 0 | **9** |
| fallback-PS bound | 9 (native lane all INCOMPATIBLE) | 3 (non-`//hlsl` UI shaders only) |
| char-select body visual | fallback (textured passthrough) | asset PS active, **black** (GAP-4 constants) |

## Deviations from plan

- Core mechanism changed from "parameter-list reorder" (plan's axis-b) to "VS-signature reconstruction"
  after the boot proved reorder cannot fix a register-base offset. Same goal (register-compatible asset PS),
  different (correct) mechanism.
- All-plugin rebuild reduced to gl11_d.dll-only (plugin-internal headers; empirically verified).
- Task 0 transient spike not added (deferred to boot per the chosen execution path); `Task 0 SPIKE` grep = 0.

## Acceptance grep table (verified)

| Check | Result |
|---|---|
| `rewritePsMainParameterListForVSOutputs` ≥2 | ✓ |
| `rewritePsMainStructBoundParameterForVSOutputs` ≥2 | ✓ |
| `tryGetOrBuildRewrittenPSForVS` (.h) ≥1 | ✓ |
| `isCompatibleWithVS_withExplicitPSInputs` (.cpp) ≥2 | ✓ |
| native `isCompatibleWithVS(` def = 1 (unchanged) | ✓ |
| `vsOutputSignatureHash` ≥2 | ✓ |
| `D3D11_REWRITE_VERSION","  "22"` ×2, no `"21"` | ✓ |
| `asset-PS bound=` + `fallback-PS bound=` in StateCache | ✓ |
| `Task 0 SPIKE` = 0 | ✓ |
| build 0 unresolved externals (Direct3d11 + SwgClient) | ✓ |
| `stage/gl11_d.dll` fresh | ✓ (5/29 17:23) |

## Boot evidence (Kenny's host, rasterMajor=11, 2026-05-29 17:26)

- 9/9 `asset-PS bound= path=rewritten`; 9/9 rewritten-lane COMPATIBLE; 0 INCOMPATIBLE; 3 fallback (UI).
- `rewrite-diag status=Rewritten`, registers aligned (e.g. `a_specmap_pp_ps20`: psIn `COLOR0=v1 FOG0=v1
  COLOR1=v2 TEXCOORD0=v3...` matches vsOut `COLOR0=o1 FOG0=o1 COLOR1=o2 TEXCOORD0=o3...`). fxc did not
  dead-strip unused mirror fields.
- Visual: **black silhouette** → GAP-4 (see Key Decision 3).
- **D-06 rasterMajor=5 re-boot: PENDING** (Kenny to run; 17-07 is gl11-only so D3D9 is byte-for-byte
  unchanged — also needed for 17-05 Task 4's D3D9 re-capture).

## Handoff

- GAP-3 done. **GAP-4 (b0↔b2 constant-buffer reconcile)** is the blocker for actual char-select visual
  parity — cross-AI consult in flight. Likely needs TRE `.inc` extraction (census gate) to map constants.
- 17-05 Tasks 4–5 (POST-gap A/B capture + real 17-VERIFICATION.md) will verdict CHAR-01/02/03 = PARTIAL
  (asset-PS lane active, black pending GAP-4).
