# CONSULT: GAP-6 fix — D3D11 VS recompile must replicate D3D9's per-mesh texcoord-set remap

You are a senior graphics/engine engineer doing an independent design review. I need a concrete
verdict on a proposed fix — is the approach right, is the scope right, what will bite us. Be specific,
flag wrong assumptions, propose a simpler path if one exists. No repo access; everything is below.

## System context

Star Wars Galaxies (SOE) client being ported from a D3D9 fixed-function/VSPS renderer to a new D3D11
renderer plugin (`gl11_d.dll`, runtime-loaded via a C API vtable). We are proving the "asset pixel
shader pipeline" on the deterministic character-select screen as a beachhead. Vertex/pixel shaders ship
as IFF assets carrying their ORIGINAL D3D9-era HLSL (`//hlsl vs_2_0` etc.) in a retained chunk; our lane
RECOMPILES that HLSL to `vs_4_0`/`ps_4_0` at load via `D3DCompile` (with backwards-compat). The lighting
+ asset-PS pipeline (prior gaps GAP-3/4/5) is PROVEN; ~95% of the character renders correctly. The
remaining defect is the **bump-shader parts (sleeves/hands)** rendering wrong (NaN vertex colors).

## What is CONFIRMED (RenderDoc + in-engine input-layout dumps this session — do NOT re-derive)

The bump skeletal mesh part is drawn with a recompiled `vs_4_0` whose **input signature** (from
`D3DReflect`) is:

```
TEXCOORD0  float2   (textureCoordinateSetMAIN  — diffuse UV)
TEXCOORD1  float2   (textureCoordinateSetNRML  — normal-map UV; USED: output to PS)
TEXCOORD2  float4   (textureCoordinateSetDOT3  — per-vertex tangent/DOT3; USED for per-pixel light)
```

The bump VS body (the asset `//hlsl`, paraphrased):

```hlsl
outputVertex.tcs_MAIN = inputVertex.textureCoordinateSetMAIN;   // TEXCOORD0
outputVertex.tcs_NRML = inputVertex.textureCoordinateSetNRML;   // TEXCOORD1  (used)
outputVertex.directionToLight_t = calculateDot3LightDirection_t(normal, inputVertex.textureCoordinateSetDOT3); // TEXCOORD2
outputVertex.halfAngle_t        = calculateHalfAngle_t(position, normal, inputVertex.textureCoordinateSetDOT3); // TEXCOORD2
```

The skeletal mesh provides only **two** vertex texcoord sets: one 2D UV + one 4D DOT3 (tangent). The
D3D11 input-layout builder maps mesh texcoord sets to consecutive `TEXCOORD` semantic indices (a global
running counter that already mirrors D3D9's `Direct3d9_VertexDeclarationMap` `textureCoordinate++`). So
the layout the engine feeds is:

```
TEXCOORD0  float2  <- mesh UV        (slot 0)
TEXCOORD1  float4  <- mesh DOT3      (slot 1)     << the 4D tangent lands here
TEXCOORD2  float4  <- (nothing) -> phantom zero-buffer (slot 15)   << VS reads tangent HERE
```

RenderDoc on the failing draw confirms: at a vertex, `TEXCOORD1` holds the tangent (non-zero 4D) and
`TEXCOORD2` is all `0.0`. So the recompiled VS reads its DOT3 tangent from `TEXCOORD2`, which is
phantom-zeroed -> `normalize(0)` -> NaN -> `COLOR0/COLOR1` NaN -> wrong sleeves/hands. (A NaN guard keeps
it from going fully black; the part renders underlit/"purple".)

## THE ROOT CAUSE (newly found — this is the crux)

D3D9 does **not** use a fixed `TEXCOORD0/1/2` mapping. `Direct3d9_VertexShaderData::createVertexShader`
**recompiles the VS per-mesh** from a `textureCoordinateSetKey` (each shader texcoord TAG -> the mesh set
index that feeds it, 3 bits per tag). It emits remap macros, e.g.:

```cpp
// for each tag i:
int const set = (key >> (i*3)) & 7;
int const dim = (tag[i] == TAG_DOT3) ? 4 : 2;
// #define textureCoordinateSet<TAG>  textureCoordinateSet<set>
// and accumulate DECLARE_textureCoordinateSets:
//   floatD textureCoordinateSet<set> : TEXCOORD<set> : register(v<base+set>);
```

The key is computed in `Direct3d9_StaticShaderData::Pass::construct` via
`shader.getTextureCoordinateSet(tag, &setIndex)` (the StaticShader knows the shader-tag -> mesh-set
mapping), then `vertexShaderData->getVertexShader(key)`.

For our 1-UV + DOT3 mesh, D3D9's key maps MAIN->set0, **NRML->set0 (aliased to the same UV)**, DOT3->set1.
So D3D9 emits a VS that declares only **two** real inputs — `TEXCOORD0` (2D, used by BOTH MAIN and NRML)
and `TEXCOORD1` (4D DOT3) — and reads the tangent from `TEXCOORD1`. That matches the compacted vertex
declaration exactly. **That is why D3D9 renders these arms correctly.**

Our D3D11 asset-PS VS recompile **skips this entirely**. The code has a literal comment:
`Direct3d11_VertexShaderData.cpp` — *"We do NOT yet add textureCoordinateSet defines (Plan 11-06 …)."*
It compiles the `//hlsl` with the source's DEFAULT macros (MAIN->0, NRML->1, DOT3->2), producing the
3-input signature that the 2-set mesh can't satisfy. There is **no** `textureCoordinateSetKey` anywhere
in the D3D11 plugin, and the D3D11 VS is compiled **once** (not per-key).

## PROPOSED FIX A (the one I want reviewed): replicate D3D9's per-key VS recompile in D3D11

Current `Direct3d11_VertexShaderData` compiles once in its constructor and holds a single
`{ID3D11VertexShader, ID3DBlob bytecode, uint64 bytecodeHash, vector<ReflectedVSInput>,
vector<ReflectedVSOutput>}`. Proposed:

1. **Parse texcoord-set tags** from the VS source at construction (scan for `textureCoordinateSet<TAG>` /
   `vTextureCoordinateSet<TAG>` tokens; mirror `Direct3d9_VertexShaderData`). Store the ordered tag list;
   add `getTextureCoordinateSetTags()`.
2. **Restructure to per-key variants:** `std::map<uint32 key, Variant>` where `Variant` =
   `{ComPtr<ID3D11VertexShader>, ComPtr<ID3DBlob>, uint64 hash, vector<ReflectedVSInput>,
   vector<ReflectedVSOutput>}`. Compile lazily on first request of a key. The remap macros (per-tag
   `#define` + `DECLARE_textureCoordinateSets`, mirroring D3D9) are injected into the `D3DCompile`
   `D3D_SHADER_MACRO` list (which already feeds an FNV-1a source+defines cache key, so different keys
   naturally cache as different bytecode).
3. **Getters take a `key`:** `getVertexShader(key)`, `getBytecode(key)`, `getBytecodeHash(key)`,
   `getReflectedInputs(key)`, `getReflectedOutputs(key)`, `computeOutputSignatureHash(key)`.
4. **Compute the key in `Direct3d11_StaticShaderData`** (mirror `Direct3d9_StaticShaderData::Pass::
   construct`: loop the VS's tags, `shader.getTextureCoordinateSet(tag,&idx)`, pack 3 bits each). Store
   `m_textureCoordinateSetKey`.
5. **Plumb the key to the bind path:** `Direct3d11_StateCache` tracks the current key alongside the
   current VS-data pointer (set when the pass binds), and passes it to (a) the getters and (b) the
   input-layout cache `getOrCreate*` (which calls `getBytecode/Hash/ReflectedInputs`). The input-layout
   cache is already keyed by `(VBFormat flags, vsBytecodeHash)`; since per-key variants have distinct
   bytecode hashes, this composes without a cache-key change.
6. **PS reconstruction** (`Direct3d11_PixelShaderProgramData`, our GAP-3 lane) consumes the paired VS's
   reflected OUTPUTS + `computeOutputSignatureHash` to reconstruct the PS input signature; it must use
   the same key's variant.
7. Bump `D3D11_REWRITE_VERSION` to invalidate stale cached `.cso`.

All six files are **inside the gl11 plugin** (no shared-header/cross-DLL ABI cascade): `Direct3d11_
VertexShaderData.{h,cpp}`, `Direct3d11_StaticShaderData.{h,cpp}`, `Direct3d11_StateCache.cpp`,
`Direct3d11_VertexDeclarationMap.cpp`, `Direct3d11_PixelShaderProgramData.cpp`.

**Net effect:** the recompiled VS reads DOT3 at `TEXCOORD1`, MAIN/NRML aliased to `TEXCOORD0` — byte-
identical input behavior to D3D9, for every texcoord-tag arrangement. The vertex layout + the global
counter need ZERO change.

## The alternative I rejected (FIX B) — tell me if I'm wrong to reject it

Make the D3D11 input-layout builder reflection-aware: bind the mesh's 4D DOT3 element to the VS's 4D
texcoord input index (`TEXCOORD2`), and alias the 2D UV element to the VS's 2D inputs (`TEXCOORD0` +
`TEXCOORD1`). gl11-layout-only, ~1 file, no per-key recompile. It renders THIS case correctly. I rejected
it as a heuristic that compensates for an un-remapped VS (diverges from D3D9; positional 2D assignment may
misorder for shaders with multiple distinct UV sets / DTLA-DTLB-MASK tag sets).

## Questions (answer each concretely)

1. **Is Fix A the right call over Fix B**, or is the layout-side compensation actually sufficient and far
   cheaper for a beachhead? Where does the dimension/positional heuristic of Fix B actually break?
2. **Per-key variant map vs other structures.** D3D9 keeps only a 1-entry "last key" cache and recompiles
   on key change. D3D11 has downstream caches (input-layout keyed by bytecode-hash; PS-reconstruction
   keyed by VS-output-sig hash) that need bytecode/reflection to be STABLE per key. Is a per-key map the
   right structure, or is there a cleaner ownership model (e.g., key folded into VS-data identity at
   creation — note the engine creates VS-data once per VS asset, not per material/key)?
3. **Key plumbing.** Tracking "current key" in the StateCache (set at pass bind, read by async layout-
   cache misses + PS reconstruction) — is that race-free given the single-threaded D3D11 immediate
   context, or is there a hazard (e.g., layout creation deferred past the next pass bind)? Safer to pass
   the key explicitly through `getOrCreate*` call args (which I plan) vs StateCache state?
4. **PS-reconstruction coupling.** The PS input-sig reconstruction depends on the VS OUTPUT signature.
   Does changing the VS INPUT remap (texcoord set indices) change the VS OUTPUT signature at all? (The
   outputs are `tcs_MAIN/tcs_NRML/directionToLight_t/halfAngle_t/COLOR0/COLOR1` — fixed semantics.) If
   outputs are unaffected, the PS lane only needs the right key to fetch the same outputs — confirm.
5. **Anything simpler I'm missing** — e.g., a way to make `D3DCompile` honor the asset's own
   `DECLARE_textureCoordinateSets` such that the unused/aliased inputs collapse without us reconstructing
   the key? Or a vs_4_0 input-signature rewrite (we already rewrite PS input sigs in this codebase)?
6. **Failure modes / blast radius:** with the per-key recompile live, what regresses for the
   already-working shaders (UI 2D passthrough; non-bump skeletal with no DOT3; static props)? Their key
   would map tags 1:1 to sets — does the macro path reduce to identity for them, or do we risk perturbing
   working bytecode?

Give a recommended approach (A vs B vs other), the cleanest ownership/plumbing model, and the top 3 risks.
