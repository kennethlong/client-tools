## 1. Diagnosis: validate with one important nuance

**Yes — the core failure is a VS→PS interpolator / signature linkage mismatch, not a cbuffer wiring bug.** Plan 17-02 changed which PS gets bound; that exposed a constraint D3D9 never enforced.

The smoking gun is in `applyPreDrawState`: when Plan 17-02 succeeds, the asset PS wins and the Phase 11 per-VS dynamic PS path is skipped:

```1107:1122:src/engine/client/application/Direct3d11/src/win32/Direct3d11_StateCache.cpp
		if (ms_currentPSData && ms_currentPSData->getPixelShader())
		{
			ctx->PSSetShader(ms_currentPSData->getPixelShader(), nullptr, 0);
		}
		else if (ID3D11PixelShader *fallback = selectFallbackPSForVS(ms_currentVSData))
		{
			// ...
			ctx->PSSetShader(fallback, nullptr, 0);
		}
```

Phase 11 already knew D3D11 linkage is **register-position strict**, not semantic-only like D3D9. That’s why `buildHlslForVSOutputs()` generates a PS whose input struct mirrors the VS reflection register-for-register — the same lesson as Iter-3’s 65K `id=343` errors from static Variant T:

```467:471:src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.cpp
	// Build per-VS PS HLSL source from reflected outputs. Walks outputs
	// sorted by Register so PS-input declaration order matches the VS
	// output HW register layout exactly (HLSL compiler assigns PS input
	// registers in declaration order -> v0==o0, v1==o1, ...).
```

Plan 17-02 inverted that contract: asset PS keeps its native input layout; the VS side was never updated to match.

**Flat-black silhouette is a plausible symptom.** Character PS families modulate texture samples against `COLOR0` / vertex color. With broken linkage, those reads are undefined (often observed as zero) → zero product → black, while depth/rasterization stay fine.

### Nuance on the “VS outputs nothing” evidence

The log line you cited:

```text
VS-outputs(1): [TEXCOORD0 r=o1 mask=0x3 rwmask=0xC]
```

comes from a **transformed-vert (2D UI) diagnostic only**, not all draws:

```2290:2291:src/engine/client/application/Direct3d11/src/win32/Direct3d11_StateCache.cpp
	if (ms_currentVBFormat.isTransformed() && ms_currentVSData)
```

So “the only VS-output pattern in the entire log” may overfit UI/2D VSes (`2d_texture.vsh`, etc.), not character `h_*` / `a_*` VSes.

**However**, `id=343` × 565K on character draws is still decisive. And **`id=342 == 0` is a strong hint the failure mode is register-position mismatch, not wholly missing semantic names.** D3D11 may report `343` even when the underlying issue is “PS wants semantic X at register N, VS has something else (or nothing) at register N.” Do not treat zero `342`s as proof that VS outputs all required semantics at the wrong registers only — but do treat it as evidence the debug layer is classifying this as **hardware-register incompatibility**, which matches D3D11’s stricter model vs D3D9.

**Historical-context correction:** Phase 11 did not strip VS outputs for the magenta fallback. VS already recompiles from original `//hlsl` VSRC in `Direct3d11_VertexShaderData.cpp`. The mismatch is that **independent VS and PS recompiles can produce incompatible register layouts** even from correctly paired asset sources — a D3D9-era assumption that no longer holds on D3D11.

---

## 2. Ranked approaches (SM2 asset base)

| Rank | Approach | Verdict |
|------|----------|---------|
| **1** | **VS recompile lane mirroring Plan 17-02** | **Partially right, but misnamed.** VS recompile **already exists**. Plan 17-04 needs **signature alignment**, not a new compile path. Recompiling both independently may work when paired VSRC/PSRC share identical I/O structs and FXC assigns matching registers — but that must be **verified per pair**, not assumed. |
| **2** | **Shared / generated interpolator contract (Approach 5 refined)** | **Best fallback and likely needed for some families.** Not one global mega-header for 100 shaders — a **per-pair bridge** synthesized from reflection. Treat **PS input signature as consumer contract**; normalize VS output struct (or PS input wrapper) at compile time. This is the inverse of existing `buildHlslForVSOutputs()`. |
| **3** | **`ID3D11Linker` / FLG paired compile** | **Poor fit.** Shader linking is SM5+ / Win8.1+ territory. Not the natural path for `vs_4_0` / `ps_4_0` on legacy SOE HLSL. High complexity, low confidence. |
| **4** | **Register hints via `packoffset` / source patching** | **Dead end for interpolators.** `packoffset` is cbuffer layout, not VS→PS varyings. Struct declaration order can influence allocation, but there is no reliable SM2 “pin interpolator to oN” knob. |
| **5** | **DXBC binary patch / remapping** | **Avoid.** Fragile, unsupported, nightmare to maintain across FXC/driver updates. |

**Tradeoff summary**

| Axis | #1 alone | #5 bridge | #2 linker | #3 DXBC patch |
|------|----------|-----------|-----------|---------------|
| Correctness | Medium until proven | High if source-level | High in theory | Low/fragile |
| Fits existing architecture | High | **Highest** (extends 11-09.13 pattern) | Low | Low |
| Fragility | Medium | Low–medium | Medium | Very high |
| Complexity | Low initial, unknown tail | Medium | High | High |
| 50–100 shader families | OK if pairs mostly self-align | **Best general pattern** | Overkill | Unmaintainable |

---

## 3. Recommendation: primary + fallback

### Primary (Plan 17-04)

**Pairwise signature validation + PS-input-driven VS normalization (Approach 5, narrowly scoped).**

Concrete shape:

```text
1. VS recompile + reflect outputs (already done in VertexShaderData).
2. PS recompile + reflect inputs (already done in Plan 17-02).
3. At StaticShader/pass bind: compare VS outputs vs PS inputs
   (semantic name/index, register, mask, component type, interpolation if present).
4. Log pair key: VS file, PS file, pass name, exact mismatch vector.
5. If compatible → draw with asset PS.
6. If incompatible → controlled fallback (magenta / per-VS scaffold), NOT silent black.
```

**First cheap experiment before building a bridge:** dump VS output reflection for the **actual character VS bytecode hashes** used at char-select (not the transformed-fan diagnostic). Compare against Plan 17-02 PS-input dumps for the bound pairs. Two outcomes:

- **A)** VS already outputs `COLOR0`, `TEXCOORD0..N` but at **wrong registers** vs PS → register-layout mismatch between independent compiles; bridge or unified struct injection required.
- **B)** VS truly outputs only `TEXCOORD0` for character draws → wrong VS bound, or VS source/rewrite is stripping outputs; fix pairing or VSRC handling before any bridge.

Do **not** assume (B) from UI-only diagnostics.

### Fallback (if independent recompile + shared struct injection still diverges)

**Per-pair “interpolator bridge” code generator:**

- Input: PS reflected input signature (consumer contract).
- Output: VS compile wrapper that declares a matching output struct and copies/computes fields from the original VS body (or reorders original struct members to match PS layout).
- Mirror the proven `buildHlslForVSOutputs()` pattern, but **PS-driven** instead of VS-driven.

This preserves the v2.2 “recompile, don’t decompile” reframe and scales without hand-fixing every `.sht`.

---

## 4. D3D11 linkage gotchas you might miss

1. **Register index is the hard requirement.** D3D9 matched by semantic name/index; D3D11 requires matching **hardware register slots** (`oN` ↔ `vN`). Your codebase already documented this at Iter-3/4.

2. **`id=342` vs `id=343`.** Zero `342`s does not mean “all semantics present.” Treat high-volume `343` as sufficient to block visual acceptance (`ROADMAP.md` already gates on both).

3. **Component masks matter.** VS `mask=0x3` (xy only) vs PS reading `xyzw` (`mask=0xF`) is incompatible for unread components. Compare **PS read mask**, not just semantic spelling.

4. **Packed registers (your `TEXCOORD0`/`TEXCOORD1` sharing reg 2).** Legal in HLSL only if **both stages agree** on the same packing. PS shows complementary masks on reg 2; VS must produce the same packed layout.

5. **`ReadWriteMask` vs `Mask`.** VS may declare a semantic with `rwmask` showing partial writes (your `rwmask=0xC` = zw unwritten). PS reads of those components are undefined — can cause black or garbage even when `id=343` count drops.

6. **Interpolation qualifiers.** `linear`, `centroid`, `nointerpolation`, `sample` must match where used. May not always surface as `343`; include in validator.

7. **`SV_POSITION` is not part of the interpolator problem.** Your PS dump correctly filters `SystemValueType != UNDEFINED`. Don’t conflate position linkage with varying linkage.

8. **Separate compile contexts (VS in `VertexShaderData`, PS in `PixelShaderProgramData`).** Even identical struct text in VSRC and PSRC can diverge if:
   - rewrite/version macros differ (`D3D11_REWRITE_VERSION` is **21** for PS, **15** for VS today),
   - profile strings differ (`vs_4_0` vs `ps_4_0`),
   - includes resolve differently,
   - preprocessor defines differ.

   **Align compile contracts** before declaring “source structs match.”

9. **Undefined ≠ guaranteed zero.** Black is still the expected *observed* failure mode for `COLOR0`-modulated paths, but validation should not assume zero-fill behavior.

10. **The old per-VS dynamic PS masked this entirely.** Any test that bound asset PS without VS alignment was guaranteed to fail on D3D11 — Phase 11 just made it visible as magenta instead of black.

---

## 5. Plan 17-03 (cbuffer) impact

**No conceptual redesign needed, but do not judge 17-03 visually until linkage is fixed.**

Your own Phase 17 artifacts already flag this:

- `SwgVertexConstants @ b0, size 400, 9 vars` is the **VS-side constants cbuffer** (from `Direct3d11_HlslRewrite` Rule D wrapping `vertex_shader_constants.inc`), reflected on PS because PS `#include`s the same header — **not** a dedicated “material cbuffer.”
- Plan 17-03’s `materialDiffuse` / `materialSpecular` name lookups may **silently miss** `material[5]` array slots in `SwgVertexConstants` (documented in `17-03-SUMMARY.md`).
- **Material tint for character body may live in `COLOR0` interpolators**, not PS cbuffers — so even perfect cbuffer wiring won’t fix black if `COLOR0` linkage is broken.

**After linkage fix**, failure modes shift from “flat black” to wrong tint/spec/bump/texture response — that’s when cbuffer correctness matters visually.

**Recommended 17-03 adjustments (post-linkage, not blocking Wave 3):**

1. Add **VS cbuffer reflection** alongside PS (matrices/lights vs material).
2. Gate material uploads on **reflected variable names** (`material[1]`, etc.), not assumed PerMaterial names.
3. Consider **skipping PS `SwgVertexConstants` upload** if that cbuffer is VS-owned and already fed via VS constant path — avoid zeroing WVP/light data (17-03-SUMMARY caveat).
4. Keep **`id=342==0 && id=343==0`** as the acceptance gate before CHAR visual A/B (already in roadmap/17-03 docs).

---

## Bottom line

| Question | Answer |
|----------|--------|
| Root cause? | **Yes** — VS/PS signature mismatch after Plan 17-02 binds real asset PS; D3D11 register-strict linkage vs D3D9 semantic-only pairing. |
| Best primary path? | **Pairwise reflect + validate; PS-input-driven VS normalization (Approach 5 narrow), building on existing recompile/reflection infra.** |
| Best fallback? | **Per-pair interpolator bridge generator** (inverse of `buildHlslForVSOutputs`), with controlled fallback on mismatch. |
| Plan 17-03? | **Proceed structurally; defer visual judgment; fix name/layout assumptions after linkage.** |

**Immediate diagnostic for Plan 17-04 Task 0:** emit VS-output reflection dumps for char-select 3D VS hashes (same dual-route as Plan 17-02 PS dumps), then diff pairwise against cached PS-input signatures. That one table will tell you whether you need register reordering (common) or VS output restoration (harder) — and it will correct the “only TEXCOORD0” hypothesis if it came from UI-only logging.
