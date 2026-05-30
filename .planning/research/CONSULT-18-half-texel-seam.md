# CONSULT: Phase 18 — confirm the central D3D11 half-pixel fix SITE and bias SIGN/MAGNITUDE for the load-screen centerline seam

You are a senior graphics/engine engineer doing an independent design review. I need a concrete,
implementable verdict — not a restatement of the problem. Be specific about WHERE the single central
correction belongs and WHAT the sign+magnitude should be. Flag wrong assumptions. You have NO repo
access; everything you need is inline below. Do not propose final code; describe the edit in prose and
give the math.

## System context

This is a Star Wars Galaxies (SOE) game client being ported from a D3D9 fixed-pipeline renderer to a
new D3D11 renderer plugin (`gl11_d.dll`). The renderer is a runtime-loaded DLL; the engine talks to it
through a C API vtable (`Gl*`). `rasterMajor=5` selects the legacy D3D9 plugin (`gl05`), `rasterMajor=11`
selects the new D3D11 plugin (`gl11`). The D3D9 path is the **byte-for-byte reference** — it is known-good.

This consult is about ONE isolated, deterministic, image-independent artifact: a faint **vertical seam
dead-center (x≈512)** on D3D11 **load screens** (the fullscreen splash blit). It is a pure 2D screen-space
path — **no pixel-shader asset pipeline, no 3D geometry, no skinning, no lighting** involved. It is the
cleanest possible 2D-sampling canary, which is exactly why we are fixing it first and centrally: the same
half-pixel convention will be leaned on later by the round-minimap work.

## Evidence the seam is a renderer-path artifact, not an image quirk (image-independence, CONFIRMED)

- The seam appears on **MANY different D3D11 load screens** (different splash images) and has **NEVER**
  been seen across ~20 D3D9 load-ins on various splashes. It tracks the *renderer*, not the *image*.
- It **persists at higher resolution** — it does not scale away — which says it is a fixed *path-level*
  half-pixel offset, not a per-image or per-resolution quirk.
- Therefore: this is a D3D9→D3D11 sampling/convention port bug (the classic D3D9 `-0.5` texel rule that
  was removed in D3D10+ center-sampling), not a defect in any one splash asset.

## Root-cause chain (the strong leading theory — please CONFIRM or REFUTE)

Under D3D9, 2D UI is drawn with **pre-transformed `D3DFVF_XYZRHW` vertices** (screen-space positions that
bypass the vertex transform). A live Phase-11 capture of the fullscreen load-screen quad recorded its
baked vertex positions as:

```
pos = (-0.5, -0.5) … (1023.5, 767.5)
```

That `-0.5` is NOT a mystery. It is a **live named engine constant**:

- `CuiManager::ms_pixelOffset = -0.5f`  — decl `CuiManager.h:150`, def `CuiManager.cpp:306`,
  accessor `getPixelOffset()` `CuiManager.h:177-179`.
- It is applied to **every** UI quad vertex via `getPixelOffset()` in `CuiLayerRenderer.cpp`
  (apply sites 189/241/286/333/382/434/489) and `CuiLayer_EngineCanvas.cpp` (275/426).

So the engine deliberately bakes `-0.5` into every 2D vertex position. Under D3D9's pre-D3D10 rasterizer
(which sampled with a `-0.5` texel rule) those baked positions land correctly. Under D3D11's
**center-sampling** rasterizer the same baked positions are off by half a pixel — and at the shared edge
of the fullscreen blit that misalignment shows as the centerline seam.

## How 2D screen-space → clip-space is done in BOTH plugins (the candidate central site)

Both plugins push a single renderer-internal constant — `viewportData`, register **c9** of the slot-0
cbuffer — that the engine-authored 2D vertex shaders read to convert the already-transformed screen-space
positions into NDC. The 2D VS does (documented in `Direct3d11_Device.cpp:662-693`, `transform2d()`):

```
ndc.x = pixel.x * viewportData.x + viewportData.z      // = pixel.x * (2/w) + (-1 - xOffset)
ndc.y = pixel.y * viewportData.y + viewportData.w      // = pixel.y * (-2/h) + (1 + yOffset)
```

### D3D9 reference (READ-ONLY — this is what we must match) — `Direct3d9.cpp:3238-3244`
```cpp
#ifdef VSPS
    // let the vertex shader know this information for 2d stuff
    float xOffset = ((x * 2.0f) / static_cast<float>(viewportWidth));
    float yOffset = ((y * 2.0f) / static_cast<float>(viewportHeight));
    float const viewportData[4] = { 2.0f / static_cast<float>(viewportWidth), -2.0f / static_cast<float>(viewportHeight), -1.0f - xOffset,  1.0f + yOffset };
    Direct3d9_StateCache::setVertexShaderConstants(VSCR_viewportData, viewportData, 1);
#endif
```
Note: this formula has **NO `+0.5` half-pixel term**. Under D3D9 the half-pixel behaviour came from the
rasterizer's `-0.5` rule + the baked `ms_pixelOffset` positions, not from this constant.

### D3D11 PRIMARY CANDIDATE — `Direct3d11_StateCache.cpp:setViewport`, c9 write at lines **1593-1602**
A verbatim transcription of the D3D9 formula above:
```cpp
    float const xOffset = (static_cast<float>(x) * 2.0f) / static_cast<float>(width);
    float const yOffset = (static_cast<float>(y) * 2.0f) / static_cast<float>(height);
    float const viewportData[4] = {
         2.0f / static_cast<float>(width),
        -2.0f / static_cast<float>(height),
        -1.0f - xOffset,
         1.0f + yOffset
    };
    constexpr int kVSCR_viewportData = 9;
    setVSConstants(kVSCR_viewportData, viewportData, 1);
```
`Direct3d11_Device.cpp:662-693` (beginScene) guarantees this `setViewport`→c9 write runs every frame
before the first draw, so any correction folded into the `.z`/`.w` terms is live from frame 0 (splash
included). A correction folded into `.z`/`.w` is **one central edit consumed by every 2D draw** — which is
the shape we want.

## The three candidate fix paths (rank these)

- **Path A — shader-body bias** in `Direct3d11_HlslRewrite.cpp` (a textual SM2→SM4 VS rewriter; today it
  does reserved-word suffixing / register-strip / `#pragma def` strip and does NOT touch position math).
  Inject a half-pixel term into the rewritten VS body. Higher blast radius (textual injection into every
  VS), and does not match D3D9's "convention lives in the constant" shape.
- **Path B — CPU-side NDC pre-conversion** of the transformed vertex stream, near
  `Direct3d11_VertexBufferDescriptorMap.cpp:217-232` (where the XYZRHW `isTransformed()` input layout is
  emitted). Most invasive; least aligned with "central, not scattered."
- **Path C — passthrough VS substitution** (replace the 2D VS with a passthrough that bakes the convention).

The leading candidate is **none of A/B/C directly** but the **c9 `viewportData` constant** in
`StateCache.cpp:1593-1602` — i.e. fold the correction into the offset terms of the shared constant
(closest to D3D9's "convention in the constant" shape, single site). Please confirm whether the c9
constant is the right central site, or whether one of A/B/C is genuinely required instead.

## ALTERNATE MECHANISM you must weigh (do not dismiss it)

Our Phase-12 baseline doc frames the seam differently: the splash may be drawn as **two side-by-side
halves** (a large splash exceeding max texture width is split into two textures), and the sample at the
**shared edge** is off by a half-texel — i.e. a **texture-coordinate / sampler-edge** artifact local to the
join, NOT a global vertex-position bias. This matters enormously for the fix:

- If the true mechanism is the **global position c9 bias**, a c9 `.z/.w` correction fixes it cleanly.
- If the true mechanism is the **UV/sampler-edge** at the split-texture join, a **global** c9 bias would
  shift **ALL** 2D content by half a pixel (HUD, fonts, radial menu, char-select 2D) **without** actually
  fixing the centerline join — a net regression that trades a faint seam for blurred/shifted UI everywhere.

Please judge which mechanism is more likely given the evidence (image-independent, resolution-independent,
exactly centerline, only under D3D11), and explicitly flag the failure mode above if the c9 route is wrong.

## The dead-end we must rule out (please confirm)

ROADMAP success-criterion #3 literally names a function `getOneToOneUVMapping` as the fix. But:
- `Direct3d11.cpp:1056` is `STUB(getOneToOneUVMapping)`, where `STUB` expands to assign the slot to
  `scaffold_fatal_stub` (a `[[noreturn]]` fatal handler).
- A whole-tree search found **zero real callers** — only self-plumbing (a header decl, a facade dispatch in
  `Graphics.cpp:1675-1679`, an fn-ptr table slot, and the D3D9 impl which is *also* uncalled).
- So if the load screen ever routed through it under D3D11, the client would **FATAL**, not show a seam —
  proving the splash does NOT route through it.

**Question:** Confirm that implementing the `scaffold_fatal_stub` at `Direct3d11.cpp:1056` would fix
nothing visible (the seam is the c9/XYZRHW convention, not a missing UV-mapping helper). We intend to leave
it a fatal stub.

## Hard constraints (verbatim, non-negotiable)

- **D-02 — Central, not scattered.** The half-pixel convention must be corrected **once** at the shared
  screen-space/viewport convention site — NOT as per-draw `+0.5` fudge factors sprinkled across call sites.
- **D-04 — gl11-only, D3D9 byte-for-byte unregressed.** Only files under
  `…/Direct3d11/…` are editable. No file under `…/Direct3d9/…` may change. The `rasterMajor=5` reference
  path must remain byte-for-byte identical.

## Design questions — answer each concretely

1. **SIGN (first-class).** The engine bakes `-0.5` into every 2D vertex via
   `CuiManager::ms_pixelOffset = -0.5f`, producing the `(-0.5,-0.5)` fingerprint. With
   `ndc.x = pixel.x*(2/w) + z`, a baked `pixel.x = -0.5` maps to `-1 - 1/w`; to behave like `pixel.x = 0`
   the c9 correction would be `z += 1.0f/width`. Symmetrically with `ndc.y = pixel.y*(-2/h) + w`, a baked
   `pixel.y = -0.5` maps to `1 + 1/h`, suggesting `w -= 1.0f/height`. **Is this derived sign correct, or is
   it the opposite?** Justify from the `(-0.5,-0.5)` fingerprint and the D3D11 center-sampling rule. (Do NOT
   assume my derivation is right — it is the hypothesis under test.)

2. **MAGNITUDE.** Confirm the magnitude is exactly one half-pixel expressed resolution-independently — i.e.
   `1.0f/width` and `1.0f/height` (since the baked offset is `0.5` pixel and the c9 scale is `2/w`/`-2/h`,
   half a pixel in clip space is `0.5 * 2/w = 1/w`). Is `1/w`,`1/h` the right magnitude, or should it be
   `0.5/w`,`0.5/h`, or something else? Show the algebra.

3. **SITE.** Is folding the bias into the c9 `viewportData.z/.w` in `StateCache.cpp:setViewport` the correct
   central site (matching D3D9's "convention in the constant" shape), or is one of fix-paths A/B/C actually
   required? Rank c9 vs A vs B vs C.

4. **MECHANISM.** Position-c9 global bias vs the COMPARISON §5 UV/sampler-edge artifact at the split-texture
   join — which is the true cause, and what observation would disprove the c9 theory? If c9 is wrong, what
   is the minimal correct fix and where?

5. **RESOLUTION-INDEPENDENCE.** Confirm the correction must be a function of `1.0f/width`,`1.0f/height`
   (no hardcoded 512/1024/0.5 pixel constants), consistent with the seam persisting at higher resolution.

6. **REGRESSION RISK.** Name any failure mode where a central c9 edit would shift/blur the HUD, fonts,
   radial menu, or char-select 2D under D3D11 (since the c9 constant drives ALL 2D XYZRHW draws, not just the
   splash). What should we watch for in the no-regression sweep?

7. **DEAD STUB.** Confirm implementing `getOneToOneUVMapping` (the `scaffold_fatal_stub` at
   `Direct3d11.cpp:1056`) fixes nothing visible and should stay unimplemented.

Give your recommended **central site + sign + magnitude**, your **mechanism judgment** (position vs
UV/sampler-edge), the **regression risks**, and the **dead-stub confirmation** — concisely and concretely.
