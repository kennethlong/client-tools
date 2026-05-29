---
phase: 17-psrc-census-char-select-beachhead
plan: 06a
type: summary
status: complete
requirements: [CHAR-02, CHAR-03]
gap_closure: true
authored_by: claude-opus-4.8 (gsd:execute-phase 17 --gaps, inline)
completed: 2026-05-29
commits:
  - cafbe6111  # diag(17-06a): extend cbuffer-vars-discovery to walk userConstants inner reflection
boot_evidence_host: Kenny, 2026-05-29 ~13:08, rasterMajor=11
---

# Plan 17-06a SUMMARY — userConstants inner-field discovery (GAP-2 DISCOVERY half)

## Dependency graph

- **requires:** 17-04 (cached `m_reflectedCbufferLayouts` + `m_psBytecodeBlob` + the one-shot
  `s_dumpedHeadVars`/`s_dumpedEyeVars` discovery gate this plan extends) · `17-05-task3`
  (PRE-gap park point, commit `6668b7cac`).
- **provides:** boot-captured inner layout of `userConstants` so Plan 17-06b can map (or DEFER)
  diffuse + emissive writes — **no guessing**.

## Key files

| File | Change |
|---|---|
| `src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp` | +116 lines: inner-walk extension of the one-shot discovery dump (re-reflect via D3DReflect, walk `GetVariableByIndex->GetType->GetMemberTypeByIndex`, depth-2 cap; emit `Plan 17-06a cbuffer-vars-discovery` lines). Added `<d3d11shader.h>` + `<d3dcompiler.h>` includes. |

Plugin-local: `gl11_d.dll` Debug Win32 rebuilt clean (0 unresolved externals, 0 errors, 5 pre-existing
DirectXMath C4459 warnings). `SwgClient_d.exe` UNCHANGED (no header touch → no ABI cascade). No
shared/clientGraphics, no Direct3d9.

## Key decisions

- **Re-reflect at dump time is MANDATORY (Round-5 item 6).** The cached layout-var struct
  (`Direct3d11_ReflectedPSCbufferVar`, PixelShaderProgramData.h:66-70) holds only
  `Name`/`StartOffset`/`Size` — no `Class`/`Type`/`Members`/`Elements`. So the inner walk re-runs
  `D3DReflect` on the bound PS bytecode. **Bytecode accessor used:** `m_passPS[idx]->getPsBytecode()`
  (returns the cached `m_psBytecodeBlob` `ID3DBlob*` retained by Plan 17-02's recompile lane).
- **One-shot, never per-draw.** The walk lives inside the existing `s_dumpedHeadVars`/`s_dumpedEyeVars`
  + `incomplete-write` gate — exactly 2 `D3DReflect` calls per boot (head + eye anchors). Recursion
  capped at depth 2; per-anchor budget 17 lines.
- **`wrote*` / `writeVarByName` chains UNCHANGED** — discovery only; mapping/writes are 17-06b.

## Boot evidence (the discovery — what 17-06b consumes)

`userConstants` reflects as a **flat `float4[17]` array** — NOT a nested struct. `GetType()` on the
top-level var returned `Elements=17, Members=0`, so the array branch fired (no member names available;
`typeClass=3` = `D3D_SVC_VECTOR`, `typeName='?'` since the array element type name is not surfaced).
Both char-select anchors show the identical layout (34 lines total = 17 × 2 anchors):

```
Plan 17-06a cbuffer-vars-discovery shader='shader/sul_m_head.sht' userConstants[0]  offset=128 size=16 typeClass=3 typeName='?'
Plan 17-06a cbuffer-vars-discovery shader='shader/sul_m_head.sht' userConstants[1]  offset=144 size=16 typeClass=3 typeName='?'
Plan 17-06a cbuffer-vars-discovery shader='shader/sul_m_head.sht' userConstants[2]  offset=160 size=16 typeClass=3 typeName='?'
... (contiguous) ...
Plan 17-06a cbuffer-vars-discovery shader='shader/sul_m_head.sht' userConstants[16] offset=384 size=16 typeClass=3 typeName='?'
```

Full enumeration (per anchor): `userConstants[N]` at `offset = 128 + N*16`, `size=16`, for `N = 0..16`.
`sul_eye.sht` is identical. The 17-04 top-level dump (9 vars) also fired (20 header/var lines) for context.

## Mapping implication for 17-06b (decisive)

Reflection exposes **only flat indices**, no semantic names — there is **no `userConstants.material.diffuse`
dotted path**. Per Plan 17-06b's Round-5 item 3 constraint, an in-bounds write at the WRONG index would
still set `wroteDiffuse=1` and falsely close GAP-2. Therefore 17-06b MUST either:
- **Case B** — derive the concrete `userConstants[N]` index for diffuse + emissive from engine source
  (`setVertexShaderUserConstants_impl` in Direct3d11.cpp — the write site that populates this region),
  recording the source line as evidence, then use `writeVarFloat4AtOffset` at `128 + N*16`; OR
- **Case C** — take **DEFERRED** if no concrete source evidence pins the indices (folklore guessing is
  forbidden). Acceptable per HIGH-3: char-select PSes consume `textureFactor.rgb` + `COLOR0`, not these
  cbuffer slots, so GAP-2 is instrumentation-completeness for FUTURE open-world shaders — deferring does
  not block CHAR-01/02/03 visual delivery (that is GAP-3 / 17-07).

## Acceptance grep table

| Criterion | Result |
|---|---|
| `Plan 17-06a` in source | 4 (≥1 ✓) |
| `GetMemberTypeByIndex` in source | 2 (≥1 ✓) |
| re-reflect token (`D3DReflect`/`GetType`) | present ✓ |
| `s_dumpedHeadVars` / `s_dumpedEyeVars` preserved | 3 / 3 ✓ |
| `materialSpecularColor` (baseline, unchanged) | 3 (note: plan's "exactly 1" is stale; edit added 0) |
| build `unresolved external symbol` | 0 ✓ |
| `stage/gl11_d.dll` fresh / `SwgClient_d.exe` unchanged | ✓ / ✓ |
| boot: `Plan 17-06a cbuffer-vars-discovery .* userConstants` | 34 (≥1 ✓) |

## Deviations from plan

- Plan anticipated a possible **nested struct** (`userConstants.material.diffuse`). Reality: **flat
  `float4[17]`**. The struct/depth-2 branch of the walk did not fire; the array branch did. This
  changes 17-06b's strategy from name-based dotted-path mapping to index-based-with-source-evidence
  (or DEFERRED) — captured above.

## D-06

Plugin-local change; `SwgClient_d.exe` untouched, `gl05*` untouched → D3D9 reference path unaffected.
Full D-06 dual-renderer re-confirmation is folded into Plan 17-05 Task 4's POST-gap re-boot.
