---
phase: 17-psrc-census-char-select-beachhead
plan: 06b
type: summary
status: complete
outcome: CASE-C-DEFERRED
requirements: [CHAR-02, CHAR-03]
gap_closure: true
authored_by: claude-opus-4.8 (gsd:execute-phase 17 --gaps, inline)
completed: 2026-05-29
commits:
  - 97d7bbf93  # feat(17-06b): writeVarFloat4AtOffset latent helper; GAP-2 mapping DEFERRED (Case C)
---

# Plan 17-06b SUMMARY — GAP-2 mapping: Case C (DEFERRED), latent helper landed

## Dependency graph

- **requires:** 17-06a (the flat-`float4[17]` discovery evidence) · `17-05-task3` (PRE-gap park).
- **provides:** `writeVarFloat4AtOffset` offset-write primitive (HIGH-1 capability) as latent
  infrastructure; a formally-deferred GAP-2 mapping with the rationale 17-05 Task 5 cites.

## Key files

| File | Change |
|---|---|
| `src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp` | +35 lines: `writeVarFloat4AtOffset(absoluteOffset, value)` lambda (sibling to `writeVarByName`, same bounds-check), `(void)`-cast as latent (Case C). `wrote*`/`writeVarByName` chains UNCHANGED. |

Plugin-local: `gl11_d.dll` Debug Win32 rebuilt clean (0 unresolved externals, 0 errors).
`SwgClient_d.exe` UNCHANGED. No header / shared-clientGraphics / Direct3d9 touch.

## Decision: Case C (DEFERRED) — no evidence-backed offset for diffuse/emissive

- **Not Case A:** 17-06a discovery showed `userConstants` reflects as a nameless flat `float4[17]`
  (typeClass=3 `D3D_SVC_VECTOR`, elements at `128 + N*16`) — no `userConstants.diffuse` path.
- **Not Case B:** `setPixelShaderUserConstants_impl` (Direct3d11.cpp:679-704) fills the slots
  POSITIONALLY from a generic `VectorRgba const*`; HlslRewrite Rule D (HlslRewrite.cpp:747,973)
  only wraps `register(cN)`→`packoffset(cN)` with no semantic map. The D3D9 register file proves
  material (`VSCR_material=11`) is SEPARATE from user constants (`VCSR_userConstant0=52`), and the
  D3D11 layout repacks (`materialSpecularColor` at offset 112 = c7, not c11), so the D3D9 map does
  not survive into a deterministic `userConstants[N]` index. Per-slot semantics are asset-defined in
  `pixel_shader_constants.inc` (TRE archives), not deterministic engine code.
- **Folklore forbidden (Round-5 item 3):** a guessed-slot write would set `wroteDiffuse=1` and
  FALSELY close GAP-2 — so the candidate chains are left unchanged (no speculative write).
- **Scope (HIGH-3):** char-select PSes consume `textureFactor.rgb` + interpolated `COLOR0`, NOT
  cbuffer material diffuse/emissive, so deferral does NOT block CHAR-01/02/03 visual delivery — the
  visual driver is GAP-3 / Plan 17-07.
- **Specular nuance (Cursor):** `materialSpecularColor` is evidence-backed **Case A for specular**
  (04ef976) and is preserved; deferring diffuse/emissive does not contradict that partial closure.

### Cross-AI confirmation

The Case C decision was independently pressure-tested by two consults against the live tree, both
returning **VERDICT: CASE-C-CORRECT** with no Case-B citation found:
- **Codex** — Rule D has no `materialDiffuse/Emissive → userConstants[N]` table; positional
  `userConstants[i]` upload; D3D9 material (c11) ≠ user constants (c52). Reasoning sound; folklore
  prohibition correctly applied.
- **Cursor** — same conclusion + verified there is no `PSCR_materialDiffuse`/`PSCR_materialEmissive`
  enum (only `PSCR_materialSpecularColor`), and D3D9 `Direct3d9_StaticShaderData.cpp:862-863` does
  not push diffuse/emissive to PS registers.

Raw consult outputs: `.planning/research/CONSULT-17-06b-casec-{codex,cursor}.out`.

## Patterns established

- `writeVarFloat4AtOffset` is the offset-addressed companion to `writeVarByName` — the primitive a
  FUTURE evidence-grounded mapping will wire (e.g. an open-world shader whose `.inc` names the slots,
  or a documented engine upload-order line). HIGH-1 capability now exists without a speculative write.

## To flip to Case B later (non-guess sources only)

A TRE `pixel_shader_constants.inc` that names `materialDiffuseColor : register(cN)` plus its reflected
offset, OR an engine call site that sets `userConstants[k]` with a documented material `k`. None exist
in the tree today.

## Acceptance grep table

| Criterion | Result |
|---|---|
| `writeVarFloat4AtOffset` in source | 2 (decl + `(void)`; Case C minimum ✓) |
| `auto writeVarByName` unchanged | 1 ✓ |
| `wrote*` chains unchanged (diff hits on chain decls) | 0 ✓ |
| `materialSpecularColor` preserved | 3 (unchanged) ✓ |
| `Plan 17-06a` discovery dump preserved | 5 ✓ |
| build `unresolved external symbol` / errors | 0 / 0 ✓ |
| `stage/gl11_d.dll` fresh / `SwgClient_d.exe` unchanged | ✓ / ✓ |

## Deviations from plan

- Plan offered Case A/B/C; evidence + dual-AI consult drove **Case C**. Per Round-5 item 3 this is a
  first-class outcome, not a failure — the discovery (17-06a) proved the work was attempted; 17-05
  Task 5 records GAP-2 mapping as DEFERRED with this rationale.

## Boot evidence (POST-gap)

The 17-06a/06b discovery dump self-suppresses only when `wrote(D,S,E)=(1,1,1)`. Since diffuse/emissive
remain unwritten (Case C), the dump will STILL fire POST-gap — expected, not a regression. The
`wroteDiffuse=1`/`wroteEmissive=1` counts measured at Plan 17-05 Task 4 are expected to stay 0 (Case C);
17-05 Task 5 records that as DEFERRED instrumentation, distinct from the CHAR visual verdict (GAP-3).
