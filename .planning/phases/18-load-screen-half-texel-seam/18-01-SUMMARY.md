---
phase: 18-load-screen-half-texel-seam
plan: 01
status: complete
completed: 2026-05-30
requirements: [UI-01]
autonomous: false
---

# 18-01 SUMMARY — D-01 cross-AI consult: confirmed fix site + bias sign/magnitude

## What was done

Ran the D-01 CODEX + Cursor cross-AI consult that confirms — BEFORE any code edit — the exact central D3D11 half-pixel fix site AND the bias sign/magnitude for the load-screen centerline seam. Produced the self-contained consult brief, both reviewers' raw outputs (decoded/sliced), a reconciled synthesis, and corrected the stale `getOneToOneUVMapping`-stub guidance in `research/SUMMARY.md`.

## Confirmed result (input for 18-02) — UNANIMOUS consult verdict

- **Fix site:** `src/engine/client/application/Direct3d11/src/win32/Direct3d11_StateCache.cpp` — the c9 `viewportData` write in `setViewport()` (verified `1593-1602`; 18-02 must re-confirm the range by reading the file — drift flag). Both reviewers rank this #1 over fix-paths A (HlslRewrite), B (CPU NDC), C (passthrough VS).
- **Bias sign (leading hypothesis CONFIRMED, not inverted):**
  - `.z` (index 2): `+= 1.0f / static_cast<float>(width)`
  - `.w` (index 3): `-= 1.0f / static_cast<float>(height)`
- **Magnitude:** exactly `1/w`, `1/h` — both reviewers explicitly reject `0.5/w` as a quarter-pixel under-correction. Resolution-independent (function of live `width`/`height`; no `512`/`1024`/`0.5f` literals). Indices 0/1 (scale terms `2/w`, `-2/h`) unchanged.
- **Dead stub:** `getOneToOneUVMapping` (`scaffold_fatal_stub`, `Direct3d11.cpp:1056`, zero callers) stays unimplemented — confirmed fixes nothing visible.

## Mechanism judgment (feeds 18-03 fallback)

Primary root cause = **global XYZRHW position-convention mismatch** (baked `-0.5` via `CuiManager::ms_pixelOffset` + verbatim D3D9 c9 + D3D11 center-sampling). The COMPARISON §5 split-texture join is a **visibility amplifier** (explains *where* the seam shows at x≈512, a 1-column discontinuity at the hard join), NOT an alternate root cause. The "global fix shifts all UI without fixing the join" failure mode does NOT apply because the vertices are provably D3D9-biased — the c9 edit is *restoration to D3D9 parity*, not a blind shift. Secondary fallback (only if c9 fails): half-texel UV inset local to the splash split draws — not a substitute for c9.

## Reviewer disagreement

**None.** CODEX and Cursor independently produced identical site / sign / magnitude / mechanism / dead-stub verdicts. Confidence HIGH.

## Regression-risk notes to carry into 18-03 (D-03 full-2D sweep)

- Double-correction (some path already added a local +0.5 → ~1px shift / text blur / edge clipping).
- Wrong-sign (`z -= 1/w`/`w += 1/h`) → seam worsens (18-02 one-screenshot detector catches this).
- Wrong-magnitude (`0.5/w`) → seam attenuated but not gone.
- Viewport-origin / sub-viewport: correction must land on base `.z`/`.w`, not `xOffset`/`yOffset`; test scissored subregions + nonzero-origin viewports.
- 3D pollution: verify no 3D VS reads c9.
- Sweep targets: ≥3 splashes + higher res (seam gone); HUD/chat/tooltip text crispness (no blur/1px drift vs D3D9); full-bleed UI flush to borders; radial/char-select centered (no oval distortion); scissored subregions aligned. "Splash fixed but HUD soft/shifted" ⇒ suspect sign/magnitude/double-correction, not wrong layer.

## Planning hygiene

- Reconciled `.planning/research/SUMMARY.md` at all six stale occurrences (lines ~14, 49, 79, 128, 205, 236): removed the dead-end "implement getOneToOneUVMapping stub at Direct3d11.cpp:995" guidance; corrected to the D-01 reality (dead fatal stub at `:1056`; fix = c9 convention at `StateCache.cpp:1593-1602`, sign consult-gated). Corrected the stale `StateCache.cpp:1504-1511` cite to verified `1593-1602`.
- **STATE.md:76,120 flag (for later gsd-managed reconciliation):** `.planning/STATE.md` carries the same stale "getOneToOneUVMapping stub at Direct3d11.cpp:995" phrasing. NOT edited here (STATE.md is gsd-managed). Flagged for the orchestrator's tracking-update pass / a later gsd reconciliation.

## Artifacts created

- `.planning/research/CONSULT-18-half-texel-seam.md` (brief, 183 lines)
- `.planning/research/CONSULT-18-half-texel-seam-codex.out` (CODEX review)
- `.planning/research/CONSULT-18-half-texel-seam-cursor.out` (Cursor review)
- `.planning/research/CONSULT-18-half-texel-seam-SYNTHESIS.md` (reconciled verdict, 73 lines)
- `.planning/research/SUMMARY.md` (reconciled — edit only)

## Verification

- Brief self-contained, ≥60 lines, carries `(-0.5,-0.5)` fingerprint traced to `ms_pixelOffset`, dead-stub finding (`scaffold_fatal_stub`/`1056`), ranked sites, UV/sampler-edge alternate mechanism, first-class sign question, D-02/D-04 constraints verbatim, no pre-locked sign, no proposed fix code. ✓
- Both `.out` non-empty with explicit fix-site recommendation + stated bias sign. ✓
- Synthesis ≥30 lines (73), names confirmed site + symbol, states confirmed sign AND resolution-independent magnitude, no-stub confirmation, mechanism judgment, HUD/fonts/radial risk notes. ✓
- No code under `…/Direct3d11/…` or `…/Direct3d9/…` modified by this plan (git diff scope: `.planning/research/` only). ✓

## Self-Check: PASSED
