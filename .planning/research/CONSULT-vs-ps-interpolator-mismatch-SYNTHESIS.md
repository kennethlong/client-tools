# Cross-AI consult synthesis — VS↔PS interpolator-schema mismatch

**Date:** 2026-05-28
**Consult prompt:** `CONSULT-vs-ps-interpolator-mismatch-codex.in`
**Raw outputs:** `CONSULT-vs-ps-interpolator-mismatch-codex.md` (Codex review at lines 796-906) + `CONSULT-vs-ps-interpolator-mismatch-cursor.md` (full text)
**Context:** Phase 17 mid-execution; Plan 17-02 surfaced 565K `id=343` register-position-mismatch errors per session. Both AIs were given the same prompt independently.

## Convergence — both AIs agree

1. **Diagnosis confirmed.** D3D11 stage linkage is register-position strict (semantic + register + mask must match), unlike D3D9's semantic-only matching. With independent VS and PS compilations, even correctly-paired asset HLSL sources can produce incompatible signatures. Flat-black character is the expected symptom when the PS executes but reads garbage/undefined values for missing interpolators and modulates against `COLOR0=0`.

2. **`id=343` × 565K is decisive.** `id=342` (missing semantic) = 0 confirms semantic NAMES match; register POSITIONS don't. Lack of `id=342` is NOT proof VS outputs all semantics — the debug layer may aggregate by family.

3. **Approach ranking:**
   | Approach | Codex | Cursor | Verdict |
   |----------|-------|--------|---------|
   | (1) VS recompile lane mirroring PS lane | Primary | Refined — see below | **PRIMARY (refined)** |
   | (5) Shared / per-pass interpolator-contract normalization | Fallback (narrow) | Endorsed as second layer | **FALLBACK** |
   | (2) `ID3D11Linker` / FLG | Reject — SM5-era | (not discussed) | **REJECT** |
   | (3) DXBC binary patch/remap | Reject — fragile/unsupported | (not discussed) | **REJECT** |
   | (4) Explicit register binding hints in PSRC | Reject — `packoffset` is cbuffer-only, no interpolator equivalent in SM2 | (not discussed) | **REJECT** |

## Cursor's critical correction (changes my original task #6 framing)

My original task #6 said "build a VS recompile lane mirroring Plan 17-02's PS lane." **VS recompile already exists** from Phase 11 — `Direct3d11_VertexShaderData.cpp` already retains VSRC and recompiles `//hlsl` VS via `D3DCompile`. And Phase 11's Iter-3 already solved a related `id=343` × 65K case for static Variant T by adding `buildHlslForVSOutputs()`:

```cpp
// Direct3d11_PixelShaderProgramData.cpp:467
// Build per-VS PS HLSL source from reflected outputs. Walks outputs
// sorted by Register so PS-input declaration order matches the VS
// output HW register layout exactly (HLSL compiler assigns PS input
// registers in declaration order -> v0==o0, v1==o1, ...).
```

That function builds a PS whose input struct mirrors the VS reflection register-for-register. It exists today and is used via `selectFallbackPSForVS`. The bind-time logic in `Direct3d11_StateCache.cpp:1107-1122`:

```cpp
if (ms_currentPSData && ms_currentPSData->getPixelShader()) {
    ctx->PSSetShader(ms_currentPSData->getPixelShader(), nullptr, 0);
}
else if (ID3D11PixelShader *fallback = selectFallbackPSForVS(ms_currentVSData)) {
    ctx->PSSetShader(fallback, nullptr, 0);
}
```

Plan 17-02 added `ms_currentPSData->getPixelShader()` non-null (real asset PS), which **wins the bind** — and the VS-matched fallback is skipped. The asset PS then fails the VS↔PS signature contract.

## The actual fix (synthesized from both)

**Pair-validate at bind/apply time, fall back gracefully.** Don't make Plan 17-02 the sole consumer of asset PS — make it conditional on signature compatibility:

```text
1. At PS-bind time (Direct3d11_StateCache::applyPreDrawState or equivalent):
   a. Reflect the bound VS's output signature.
   b. Compare register/mask/component-type/interpolation-mode against the
      asset PS's input signature (already cached by Plan 17-02's
      D3DReflect-once mechanism).
2. If signatures are compatible → use asset PS (Plan 17-02 happy path).
3. If signatures are incompatible → use selectFallbackPSForVS (existing
   buildHlslForVSOutputs-built PS that matches the VS layout).
4. Log the pair-key + shader names + mismatch detail (per-shader, not
   per-draw — log once per cache entry).
```

If asset PS proves incompatible with too many VSes (likely on this asset base), the **secondary layer (Approach 5)** kicks in:

```text
5. For asset-PS programs that consistently fail compat:
   a. Retrieve the VS reflection layout.
   b. Rewrite the asset PSRC source's input struct to match (insert a
      generated PSInput { ... } with VS-matched register order).
   c. Recompile via the existing tryCompilePixelShaderFromHlslNoFatal.
   d. Cache the rewritten variant separately from the original.
```

This is a per-shader-family adaptation, not a global mega-struct. Done only when needed.

## Codex's additional gotchas (folded into spec)

- **Mask matters**, not just semantic name. Validator must compare `D3D11_SIGNATURE_PARAMETER_DESC::Mask` AND `ReadWriteMask`.
- **Interpolation modifiers** (`linear`/`centroid`/`nointerpolation`/`sample`) must match. Different modifiers can cause linkage failures the debug layer reports differently than vanilla `id=343`.
- **`SV_Position` is system-value**, handle distinctly from user varyings — don't confuse the validator.
- **`COLOR0`/`COLOR1` are NOT magic D3D9 color registers in D3D11** — they're user semantics like `TEXCOORD`. Must match register-for-register, no auto-conversion.

## Plan 17-03 impact (both AIs converge)

**Plan 17-03 does NOT need redesign.** The reflected `SwgVertexConstants` cbuffer at `b0` size 400 vars 9 is the right cbuffer lane for material/texture-factor uploads. Once VS↔PS linkage is fixed (above), Plan 17-03's correct cbuffer values will become observable. Until then, judging Plan 17-03 visually is meaningless — the failure mode dominates.

**Open question for Plan 17-03 polish:** Codex flags "add VS cbuffer reflection too, because the VS recompile lane may reveal its own `b0`/matrix expectations." Plan 17-03's reflection is PS-side only. May need a symmetric VS-side cbuffer reflection (Plan 17-03.X or task #7 expansion). Defer until task #6 fix is in place — at that point we'll see whether `wroteDiffuse/Specular/Emissive` flags fire correctly OR whether the VS-side cbuffer has its own gap.

## Updated task structure

- **Task #6 (revised):** "Plan 17-04 — VS↔PS pair signature validation + asset-PS bind-time gate." NOT a new VS recompile lane (already exists). NOT a shared HLSL header. Instead: pair-validate before bind, use existing `selectFallbackPSForVS` when asset PS is incompatible. Add Approach #5 (per-shader PSRC source rewrite to match VS reflection) as a follow-on when Approach #1's fallback is too noisy.
- **Task #7 (existing):** Plan 17-03 `writeVarByName` for `SwgVertexConstants` schema (refine to look up `material[N]` array slots) — orthogonal to #6, can proceed independently.
- **Task #5 (existing):** gl05_d.dll rebuild texture-binding regression — pure forensic debug; consult input less critical.

## Files to read for the Plan 17-04 spec

- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.cpp:467-471` — existing `buildHlslForVSOutputs()` pattern
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_StateCache.cpp:1107-1122` — bind-time PS selection logic
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_StateCache.cpp:2290-2291` — `isTransformed()` check that explains why the VS-outputs log I cited was UI-only
- `Direct3d11_VertexShaderData.cpp` — existing VS recompile lane (for reference; do not add a parallel one)
- The 17-02-SUMMARY.md + Plan 17-02 commits — for the asset-PS recompile pattern that Plan 17-04 will gate on signature compat
