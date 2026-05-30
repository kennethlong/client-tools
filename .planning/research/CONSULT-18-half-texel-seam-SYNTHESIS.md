# CONSULT-18 SYNTHESIS — Load-screen half-texel seam: confirmed fix site + bias sign/magnitude

**Synthesized:** 2026-05-30
**Reviewers:** CODEX (`-codex.out`) + Cursor (`-cursor.out`)
**Verdict:** UNANIMOUS — both reviewers converge on the identical site, sign, magnitude, and mechanism. No disagreement to reconcile.

> **Drift note for the 18-02 executor:** the line range below is verified as of 2026-05-30 but research artifacts have drifted before. **Re-read `Direct3d11_StateCache.cpp:setViewport` and re-confirm the c9 write range by reading the file** — do not trust the cited `1593-1602` blindly.

---

## 1. CONFIRMED central fix site

**`src/engine/client/application/Direct3d11/src/win32/Direct3d11_StateCache.cpp` — `setViewport()`, the c9 `viewportData` write (verified `1593-1602`).**

Both reviewers rank this **#1** over fix-paths A (HlslRewrite shader-body bias), B (CPU NDC pre-conversion), and C (passthrough VS substitution). Rationale (agreed):
- It is the single shared screen-space→NDC convention site every 2D XYZRHW draw reads via `transform2d()`.
- It mirrors D3D9's "convention lives in the constant" shape — D3D9's half-pixel behaviour lived in the *rasterizer* (removed in D3D10+); D3D11 must fold the equivalent into the *shared clip-map constant*.
- `Direct3d11_Device.cpp:beginScene` guarantees `setViewport`→c9 runs every frame before the first draw, so the correction is live from frame 0 (splash included).
- A/B/C are all higher blast-radius / wrong-layer / more invasive; **do not use them if the c9 correction suffices** (both reviewers).

Fix-path A/B/C are NOT needed. `getOneToOneUVMapping` is NOT the site.

## 2. CONFIRMED bias SIGN and MAGNITUDE (resolution-independent)

The leading hypothesis is **CONFIRMED correct (not inverted)** by both reviewers, derived from the baked `(-0.5,-0.5)` fingerprint (source: `CuiManager::ms_pixelOffset = -0.5f`, `CuiManager.cpp:306`) under the D3D11 center-sampling rule:

- **`.z` (index 2): `+= 1.0f / static_cast<float>(width)`**   (positive X clip offset)
- **`.w` (index 3): `-= 1.0f / static_cast<float>(height)`**  (negative Y clip offset)

i.e. after the existing D3D9-transcribed `viewportData[4]`:
```
viewportData[2] = (-1.0f - xOffset) + 1.0f/width;
viewportData[3] = ( 1.0f + yOffset) - 1.0f/height;
```

**Magnitude is `1/w`,`1/h` — NOT `0.5/w`,`0.5/h`.** Both reviewers show the algebra: the baked screen-space error is `Δp = 0.5` px; the c9 scale term is `2/w` (and `-2/h`), so the NDC correction is `0.5 · 2/w = 1/w` (and `0.5 · 2/h = 1/h`). `0.5/w` would only cancel a quarter-pixel and leave the seam attenuated-but-present. Indices 0/1 (the `2/w`, `-2/h` scale terms) are **unchanged**.

**Resolution-independent:** the correction MUST be a function of the live `width`/`height` already in `setViewport` — no hardcoded `512`/`1024`/`0.5f` pixel constants. The seam persisting at higher resolution is consistent with a fixed ½-px screen error (`1/w` in NDC, which does not vanish as `w` grows).

## 3. Dead stub — confirmed NOT implemented

Both reviewers confirm: implementing `getOneToOneUVMapping` (the `scaffold_fatal_stub` at `Direct3d11.cpp:1056`, zero callers) fixes nothing visible. If the splash routed through it, D3D11 would FATAL, not show a seam. **Leave it a fatal stub.**

## 4. Mechanism judgment (position-c9 vs UV/sampler-edge) — feeds the 18-03 fallback

**Primary root cause = global XYZRHW position-convention mismatch** (baked `-0.5` vertices + verbatim D3D9 c9 + D3D11 center-sampling). Both reviewers agree the COMPARISON §5 split-texture join is a **visibility amplifier**, not an alternate root cause: the global ½-px misalignment is invisible on a continuous quad but becomes a sharp 1-column discontinuity at the hard join of two side-by-side splash halves at screen center. This explains *where* the seam shows (x≈512), not *why* D3D11 differs from D3D9.

**Why the "global fix shifts all UI without fixing the join" failure mode does NOT apply here:** that failure mode requires the vertices to be *already correct for D3D11* with a *purely UV-local* bug. They are not — the vertices are provably D3D9-biased (`ms_pixelOffset=-0.5` everywhere). So the c9 correction is *restoration to D3D9 parity*, not a blind global shift that trades a seam for blur.

**Secondary fallback (only if c9 fails — for the 18-03 human gate):** a half-texel UV inset local to the splash split draws (left half: shrink U-max by `0.5/texWidth`; right half: expand U-min by `0.5/texWidth`), in the D3D11 splash/blit path that emits the two halves — NOT a substitute for c9. Disproof signals:
- c9 applied → center seam gone on splashes BUT a non-split single-quad HUD element still shows ½-px mismatch vs D3D9 → c9 over/under-corrects (double-compensation elsewhere); investigate.
- c9 applied → seam persists *unchanged* at center while non-split 2D now aligns sharper vs D3D9 → primary was c9 (fixed), residual is the split UV edge → apply the secondary UV inset at split draws only.

## 5. Regression-risk notes to carry into the 18-03 no-regression sweep (D-03)

A central c9 edit moves **every** D3D11 2D XYZRHW draw ≈½ px toward D3D9 parity (intended). Watch for these failure modes (both reviewers):
- **Double-correction:** if some other D3D11 path already added a local +0.5 fudge, the c9 edit shifts it a *full* pixel vs D3D9 → text blur / ~1px drift / UI clipped at screen edges.
- **Wrong sign** (`z -= 1/w` / `w += 1/h`): systematic shift opposite D3D9; seam *worsens* — the 18-02 cheap wrong-sign detector catches this.
- **Wrong magnitude** (`0.5/w`): seam attenuated but not gone; UI still soft.
- **Viewport-origin / sub-viewport:** correction must land on the base `.z`/`.w`, not bleed into `xOffset`/`yOffset`; test scissored subregions (inventory panes, nested viewports) and nonzero-origin `setViewport` calls.
- **3D pollution:** verify no 3D VS reads c9 (should be guarded by `transform2d()` scope, but confirm).

**18-03 sweep targets:** (a) center seam gone on ≥3 splashes + at higher res; (b) HUD/chat/tooltip text crispness — no extra blur, no systematic 1px drift vs D3D9; (c) full-bleed UI still flush to borders; (d) radial menu / char-select 2D still centered, no oval distortion (scale terms unchanged); (e) scissored subregions aligned vs D3D9. **"Splash fixed but HUD now soft/shifted" ⇒ suspect sign/magnitude/double-correction, NOT that c9 was the wrong layer.**

## 6. Reviewer disagreement

**None.** CODEX and Cursor independently produced identical site (c9 `.z/.w` in `setViewport`), identical sign (`+1/w`, `-1/h`), identical magnitude (`1/w`,`1/h`, both rejecting `0.5/w`), identical mechanism judgment (position-c9 primary, split-join as amplifier, UV inset as secondary fallback), and identical dead-stub verdict. Confidence: HIGH.

---

## Hand-off to 18-02

Implement ONE central edit in `Direct3d11_StateCache.cpp:setViewport`: after the existing D3D9-identical `viewportData` computation, add `+ 1.0f/width` to index 2 (`.z`) and subtract `1.0f/height` from index 3 (`.w`), expressed via `1.0f / static_cast<float>(width|height)`. Leave indices 0/1 unchanged. No `…/Direct3d9/…` edits (D-04). No per-draw +0.5 scatter (D-02). Leave `getOneToOneUVMapping` stubbed (D-01). Then the 18-02 one-screenshot wrong-sign detector + the 18-03 ≥3-variant no-regression sweep.
