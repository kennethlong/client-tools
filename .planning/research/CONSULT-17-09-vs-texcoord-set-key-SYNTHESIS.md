# SYNTHESIS — CONSULT-17-09 (D3D11 VS per-key texcoord-set remap)

**Both CODEX and Cursor independently converged: implement Fix A, reject Fix B as the production fix.**
The missing piece is per-key VS recompilation with D3D9 remap macros — NOT layout logic (the layout/global
counter already mirrors D3D9 and is correct; the bug is VS-side, pre-compilation).

## Converged design (both agree)

1. **`Direct3d11_VertexShaderData`** keeps one instance per VS asset; add a **lazy
   `std::unordered_map<uint32 key, Variant>`** where `Variant = {ComPtr<ID3D11VertexShader>,
   ComPtr<ID3DBlob> bytecode, uint64 bytecodeHash, vector<ReflectedVSInput>, vector<ReflectedVSOutput>,
   uint32 outputSigHash}`. Expose `getVariant(uint32 key)` (compiles lazily). This matches engine
   ownership (VS-data created once per asset) and D3D9 semantics (same asset, different keys).
2. **Parse texcoord-set tags** from VS source once (mirror `Direct3d9_VertexShaderData`: BOTH
   `textureCoordinateSet*` AND `vTextureCoordinateSet*` spellings). `getTextureCoordinateSetTags()`.
3. **`Direct3d11_StaticShaderData::Pass`** computes + stores `m_textureCoordinateSetKey` at construct,
   literally mirroring `Direct3d9_StaticShaderData::Pass::construct:550-578`
   (`shader.getTextureCoordinateSet(tag,&idx)`, 3 bits/tag, same missing-tag fallback).
4. **Macro injection at compile** (mirror `Direct3d9_VertexShaderData::createVertexShader:344-420`):
   per-tag `#define textureCoordinateSet<TAG> textureCoordinateSet<set>` + accumulate
   `DECLARE_textureCoordinateSets` emitting **unique** set indices only (`floatD ...Set<set> :
   TEXCOORD<set> : register(v<base+set>)`), DOT3 dim=4 else 2. Add to the `D3D_SHADER_MACRO` list (rides
   the existing FNV-1a source+defines cache key → per-key bytecode caches naturally).
5. **EXPLICIT key plumbing — both stressed this.** Do NOT read key from ambient StateCache "current"
   state for anything cached/async. Pass `(vsData, key)` (or the resulting bytecodeHash) explicitly into
   `getOrCreate*` layout creation and PS reconstruction. Capture key/hash at request/enqueue time. Single-
   threaded immediate context means no race *today*, but lazy layout-cache misses are about a specific
   pass, not "whatever key is current."
6. **Input-layout cache** stays keyed by `(VBFormat, vsBytecodeHash)` — per-key variants have distinct
   bytecode/hash, so it composes with zero cache-key change.
7. **PS reconstruction**: VS *output* signature is unchanged by input remap (outputs = fixed struct
   semantics). `computeOutputSignatureHash` must hash OUTPUTS ONLY. PS lane fetches the same key's variant.
8. **Bump `D3D11_REWRITE_VERSION`** (invalidates stale `.cso`; expect cache churn). Add per-variant
   compile logging: VS name, key, tag list, tag→set map, reflected input sig.

## Cursor's optional scope-trim (noted, not required)
Compile only the pass's key **eagerly** in `Pass::construct` and store the variant on the Pass — avoids
widening getters to key-based API. Fall back to the lazy map only if a VS asset is shared across passes
with different keys. **Decision: keep the lazy `map<key,Variant>` in VertexShaderData** (robust to asset
reuse; both endorse it) — but Pass owns the key and drives the lookup.

## Top 3 risks (both lists merged)
1. **D3D9 macro/tag/key parity drift** (biggest): tag scan order, DOT3 dim=4, **aliased/duplicate-set
   collapse** (two tags→set0 must emit ONE `textureCoordinateSet0` decl, not duplicates), `register(v
   base+set)` base, missing-tag fallback. Mitigation: port D3D9 logic literally; golden-test the input
   signature for the identity key AND the bump-alias key.
2. **Key lifetime in cached/async paths**: layout/PS jobs reading stale "current" key. Mitigation:
   explicit `(vsData,key)`/hash captured at enqueue; never implicit global for cache misses.
3. **Variant-selection regression during rollout**: callers still using a no-key getter compile identity
   macros and silently break aliased meshes again. Mitigation: make the keyed path the only path; assert
   in debug if a VS with tags is bound without a key; bump REWRITE_VERSION.

## Blast radius
Identity-mapping shaders (UI 2D, non-bump skeletal, static props with 1:1 UV sets) → macros reduce to the
default declaration → bytecode should match the existing working path (cache churn only). Aliasing keys
(the bug) → fewer/different VS inputs → different bytecode = the fix, not a regression.

**Verdict: proceed with Fix A as specified above.**
