---
phase: 18
reviewers: [codex, cursor]
reviewed_at: 2026-05-30T22:11:35Z
plans_reviewed: [18-01-PLAN.md, 18-02-PLAN.md, 18-03-PLAN.md]
self_skipped: claude (running inside Claude Code — skipped for independence)
---

# Cross-AI Plan Review — Phase 18: Load-Screen Half-Texel Seam

Two independent reviewers: **Codex** (`gpt-5.5`, read-only sandbox, verified claims against the live tree)
and **Cursor** (`cursor-agent --mode ask`, reads the live tree). Both rated the plan set MEDIUM /
MEDIUM-HIGH. They **converge strongly** on a likely **inverted bias sign**, a **mechanism-coverage gap**,
and the absence of a **visual seam check in 18-02**. The orchestrator verified the two highest-value
findings against live source (see Consensus → Orchestrator Verification).

---

## Codex Review

**Summary**

The three-wave sequence is mostly sound: consult first because the sign/site risk is real, then one central D3D11 edit, then evidence-heavy validation. Verified the main source claims in the live tree: `Direct3d11_StateCache.cpp` writes `viewportData` at c9 in `setViewport` around lines 1571-1602; `Direct3d11_Device.cpp` calls `Direct3d11_StateCache::setViewport(0, 0, ms_width, ms_height, ...)` at line 693 before early draws; D3D9 has the matching formula at `Direct3d9.cpp:3238-3243`; `Direct3d11.cpp:1056` still stubs `getOneToOneUVMapping`; repo-wide `rg getOneToOneUVMapping` shows no real call sites beyond declaration/dispatch/plugin plumbing and D3D9 implementation. The major weakness is not process overhead, it is that the likely c9 fix has a sign trap and a global 2D blast radius.

**Strengths**

- The plan correctly rejects `getOneToOneUVMapping` as the fix site. Live source supports this: D3D11 still routes it to `STUB(getOneToOneUVMapping)`, and no real caller appears in repo grep.
- The consult-gate is justified. This is not just "one constant"; it changes the central transformed-vertex convention for all D3D11 2D.
- The D-03 full-2D sweep is necessary and correctly framed. c9 is shared by splash, HUD, fonts, radial, char-select UI, and likely post/RT blits.
- The build gate includes the `/FORCE` unresolved-symbol grep, which is essential for this repo.
- Separating "fix" from "evidence" is reasonable because screenshot capture is host/human dependent.

**Concerns**

- **HIGH: The half-pixel sign shown in the pattern map is probably reversed.** With `ndc.x = pixel.x * (2/w) + z`, a baked D3D9 position of `x=-0.5` currently maps to `-1 - 1/w`. To make that behave like `x=0` in D3D11, `z` should move by `+1/w`, not `-1/w`. For Y, `ndc.y = pixel.y * (-2/h) + wTerm`; `y=-0.5` maps to `1 + 1/h`, so the central correction should likely be `wTerm -= 1/h`, not `+= 1/h`. The plan says sign is consult-dependent, but the embedded "adjust by `-(1/width)` / `+(1/height)`" example is dangerous enough to mislead execution.
- **HIGH: 18-02 says `files_modified: ONLY Direct3d11_StateCache.cpp`, but also says implement an alternate site if consult rejects c9.** Those two instructions conflict. If the consult chooses shader rewrite or CPU transformed-vertex conversion, the executor either violates file scope or ignores the consult.
- **HIGH: The theory needs a sharper proof that a global viewport constant fixes a centerline seam rather than just translating all 2D by half a pixel.** The plan assumes the seam is due to baked `XYZRHW` coordinates crossing D3D9/D3D11 sampling conventions. Plausible, but a uniform position bias changes raster coverage/interpolation globally. It may fix fullscreen splashes while making fonts or 1px UI softer. D-03 catches this after the fact, but 18-02 acceptance could still declare "fix landed" before visual proof.
- **MEDIUM: No explicit odd-size / non-power-of-two viewport acceptance.** The formula is resolution-independent, but center seams are especially suspect at odd viewport widths or scaled render targets. "Higher resolution" is not enough; include at least one non-1024/1920 width or odd-ish UI scale if available.
- **MEDIUM: Render-target vs backbuffer sizing is under-specified.** `setViewport` is called by post-processing and render-target paths with destination sizes, not only the backbuffer. A c9 bias based on current viewport width/height is probably right, but validation should include at least one UI/render-target path, not only world-load splash.
- **MEDIUM: 18-03 assumes ≥3 splash variants are practically capturable but does not say how to force them.** If load screens are randomized or content-dependent, this can waste a cycle. The plan needs a deterministic capture recipe or an accepted fallback list of forced destinations/assets.
- **MEDIUM: Boot gate wording is weaker than the invariant.** 18-02 says rebuild the Direct3d11 plugin and smoke both renderers, but does not clearly state Debug vs Release or whether Release `client.cfg` must be preserved. If this phase only gates Debug, say so; otherwise build/smoke both relevant configs.
- **LOW: Grep acceptance "no literal `0.5f/512/1024` bias" is easy to satisfy while still using an incorrect hardcoded decimal.** Better assert the code uses `1.0f / static_cast<float>(width)` and `1.0f / static_cast<float>(height)` or named variables derived from them.
- **LOW: 18-03 `files_modified` should explicitly allow image files and forbid committing config changes.** The plan mentions image assets but should require `client_d.cfg` / `client.cfg` restored or excluded from diff.

**Suggestions**

- In 18-01, make the algebra an explicit reviewer question with both candidate signs: `z = -1 - xOffset + 1/width` and `w = 1 + yOffset - 1/height` versus the opposite. Require reviewers to explain using the `(-0.5,-0.5)` fingerprint.
- In 18-02, change file scope to: "Expected only `Direct3d11_StateCache.cpp`; if consult selects another site, update `files_modified` and acceptance before execution."
- Add a pre/post diagnostic capture in 18-02 or 18-03 that logs/dumps c9 for a known 1024x768 or 1920x1080 viewport. This makes sign mistakes obvious numerically.
- Strengthen 18-03 with deterministic splash selection steps: named planets/scenes/assets, exact config toggles, and a fallback if only one splash is reachable.
- Add validation for at least one non-default viewport/render-target path and one non-standard resolution, ideally not only "higher res."
- Require final diff checks: `git diff --name-only` contains no D3D9 files, no config files, and only the approved D3D11 source file for 18-02.
- Keep the consult, but make it technical rather than ceremonial: one blocking consult is justified; merging 18-01 and 18-02 would be premature unless the sign/site is already independently proven.

**Risk Assessment: MEDIUM**

The plan structure is disciplined and the live source supports the proposed primary site. The risk is concentrated in the sign and global 2D convention change. A wrong sign or overbroad c9 adjustment could either leave the seam, shift all UI by half a pixel, or create font/HUD blur. The planned D-03 sweep should catch that, but 18-02 should not be treated as successful until visual evidence confirms both seam removal and no 2D regression.

---

## Cursor Review

### What Cursor Verified Against Live Source

| Claim | Verdict | Live location |
|---|---|---|
| c9 `viewportData` is verbatim D3D9 formula | ✅ Confirmed | `Direct3d11_StateCache.cpp:1593-1602` |
| D3D9 reference formula (no +0.5 in viewportData) | ✅ Confirmed | `Direct3d9.cpp:3238-3244` |
| `getOneToOneUVMapping` D3D11 entry is `STUB` → fatal | ✅ Confirmed | `Direct3d11.cpp:1056` |
| Zero real callers of `getOneToOneUVMapping` | ✅ Confirmed | Only decl/dispatch/impl/stub in `src/` |
| `beginScene` guarantees c9 write before first draw | ✅ Confirmed | `Direct3d11_Device.cpp:662-693` |
| Drift: stub was ~995, now 1056 | ✅ Confirmed | Plans carry drift flag correctly |
| Baked `(-0.5,-0.5)…` fingerprint | ⚠️ Not source-verifiable | Plan correctly labels as Phase 11 capture |
| **Missing from plans:** explicit source of `-0.5` | ⚠️ Found in repo, not in plan set | `CuiManager::ms_pixelOffset = -0.5f` applied to all UI quads (`CuiLayerRenderer.cpp`) |

### Plan Set Summary

The three-wave sequence (18-01 consult → 18-02 single gl11-only c9 edit → 18-03 evidence + human gate) is structurally sound for a convention fix with high blast radius: it correctly rejects the dead `getOneToOneUVMapping` stub, targets the shared XYZRHW→NDC path, keeps D3D9 byte-for-byte untouched, and gates UI-01 with multi-image captures plus a mandatory full-2D regression sweep. The primary weakness is not process overhead but **mechanism ambiguity**: COMPARISON.md emphasizes a texture-sampling seam at a split-texture edge, while the plans bet entirely on a global c9 `.z/.w` position bias — and the proposed sign/magnitude is algebraically plausible but **not uniquely determined** from the embedded material alone.

### Plan 18-01 — Consult Gate

**Concerns:**
- **HIGH — Consult brief omits the actual `-0.5` source.** The baked fingerprint is not mysterious: `CuiManager::ms_pixelOffset = -0.5f` is applied to every UI quad in `CuiLayerRenderer.cpp`. Without this in the brief, reviewers may mis-derive sign/magnitude or recommend the wrong path.
- **MEDIUM — Blocking human-action for dual CLI consult may be heavy** for what is likely a 2-line edit, but sign ambiguity partially justifies it.
- **MEDIUM — Stale project docs contradict D-01.** `.planning/research/SUMMARY.md:14` and `.planning/STATE.md:76,120` still say "implement `getOneToOneUVMapping` stub at Direct3d11.cpp:995." Executor could follow stale guidance despite phase CONTEXT.
- **LOW — Task 2 acceptance is weak on quality.** "Non-empty + contains fix-site recommendation" doesn't require agreement on sign/magnitude.

**Suggestions:** add `CuiManager::ms_pixelOffset` + `CuiLayerRenderer` to the mandatory brief sections; add a consult question "given engine applies `-0.5` at vertex emit, should c9 compensate with `+1/width` or `-1/width` in `.z`?"; Task 3 synthesis AC should require BOTH sign and magnitude; flag/update the stale SUMMARY.md/STATE.md.

**Risk: MEDIUM** — good gate design undermined by incomplete root-cause chain in the brief and stale doc drift.

### Plan 18-02 — Central Fix + Dual Boot Gate

**Strengths:** excellent scope discipline (one Direct3d11 `.cpp`, zero Direct3d9, stub stays dead); resolution-independent form correct; correct build/runtime gotchas (`client_d.cfg`, `/FORCE` grep, plugin-only relink); explicit escape hatch if consult picks non-c9 site.

**Concerns:**
- **HIGH — Proposed default sign in PATTERNS may be wrong.** A half-pixel shift = `±(1/w)` in NDC. Vertices already shifted left/up by `-0.5` via `CuiManager::ms_pixelOffset`; compensating typically requires **adding `+1/w` to `.z`** (shift right), not subtracting. PATTERNS proposes `-(1.0f/width)` — opposite direction. Sign is genuinely consult-dependent; the primary-candidate bias is not self-evidently correct.
- **HIGH — No seam verification in 18-02.** Boot-smoke only checks crash/no-FATAL. Wrong sign ships cleanly to 18-03; wasted cycle if human gate rejects.
- **MEDIUM — Global c9 shift is the theory; COMPARISON §5 emphasizes UV/sampling at split texture edge.** If the seam is primarily a texture-coordinate/sampler artifact at the center join, a position-only c9 fix may not eliminate it. Plans have no fallback if seam persists post-c9.
- **MEDIUM — Sub-viewport correctness untested.** `setViewport` is called with partial rects from `ShaderPrimitiveSorter`, `PostProcessingEffectsManager`, letterboxed `GroundScene` paths; 18-03 doesn't explicitly test letterboxed/compositing sub-viewports.
- **LOW — "Byte-for-byte D3D9 unregressed" verified only by git diff scope**, not visual diff; 18-03 should include at least one D3D9 re-capture, not just assertion.

**Suggestions:** add a quick seam sanity capture to 18-02 Task 2 (one D3D11 load-screen screenshot before handing to 18-03) as a cheap wrong-sign detector; document an explicit sign-flip procedure; reference `CuiManager::ms_pixelOffset` in the comment; consider a consult-gated 18-02b fallback (sampler/UV-edge) named in 18-03's FAIL path.

**Risk: MEDIUM-HIGH** — low implementation risk, high wrong-fix-site or wrong-sign risk.

### Plan 18-03 — Evidence + Human Gate

**Strengths:** D-03 full-2D sweep directly addresses the c9 blast radius; matched pairs + retained BEFORE artifact mitigate the Iter-44B over-claim; blocking human gate with explicit reject/resume; validation-only scope is clean.

**Concerns:**
- **HIGH — False-negative pass risk on subtle half-pixel regression.** A ±0.5px shift on HUD/fonts may be invisible in casual screenshot review but visible on thin text/radial edges. Qualitative-only review is insufficient.
- **MEDIUM — "≥3 distinct splash variants" under-specified.** No deterministic routing table (planet/zone paths); reproducibility across sessions/agents is weak.
- **MEDIUM — D3D9 "unregressed" is asserted, not evidenced.** Task 2 only requires stating it in markdown — no mandatory D3D9 re-capture in AC.
- **MEDIUM — Higher-res capture lacks a defined target resolution** (e.g. 1440p/4K/config key) — the resolution-independent claim needs a recorded value.
- **LOW — COMPARISON §5 hypothesis isn't validated** — even if seam is gone, root cause isn't proven; weak for "establishes convention for Phase 20 minimap."

**Suggestions:** require pixel-aligned diff or zoom-crop on HUD text + radial edges; predefine 3 splash routes; mandate one D3D9 post-18-02 re-capture for a known HUD surface; record exact higher-res resolution in COMPARISON.md.

**Risk: MEDIUM** — good human gate, but qualitative-only regression detection may miss the exact D-03 failure mode.

### Cursor Overall: **MEDIUM-HIGH**

The phase goal hinges on a single signed constant whose correct value depends on interacting conventions (vertex `-0.5`, D3D9 raster rules, D3D11 center sampling) and whose failure mode (global 2D shift) is subtle. The consult gate is warranted — but only if the brief includes the full causal chain through `CuiManager::ms_pixelOffset`.

---

## Consensus Summary

### Agreed Strengths (both reviewers)
- Correctly **rejects the dead `getOneToOneUVMapping` stub** — both verified zero callers + fatal-stub in live source.
- **c9 `viewportData` is the right primary candidate** for a central, shared, D3D9-shaped fix (both verified the formula is a verbatim D3D9 transcription).
- **Consult gate (18-01) is justified, not over-process** — the sign/blast-radius risk is real; both explicitly reject merging 18-01+18-02.
- **D-03 full-2D sweep is necessary and correctly framed**; gl11-only / D3D9-byte-for-byte (D-04) and central-not-scattered (D-02) constraints are strong.
- The `/FORCE` unresolved-symbol grep gate and the line-drift "re-read the file" hygiene are good.

### Agreed Concerns — priority order (raised by both unless noted)

1. **HIGH — Bias SIGN is probably inverted.** Both independently derived that compensating the baked `-0.5` needs `z += 1/w` and `w -= 1/h` — the **opposite** of the PATTERNS.md embedded example (`-(1/width)` / `+(1/height)`). *Orchestrator-corroborated:* the stale `SUMMARY.md:14` also says "D3D11 must add **+0.5**" → three independent sources point positive. The embedded example could mislead execution.
2. **HIGH — Mechanism-coverage gap / no fallback.** A global viewport-constant position bias may translate ALL 2D by half a pixel rather than fix a *centerline-join* seam; COMPARISON §5 frames it as a texture-sampling artifact at the split edge. The plans bet entirely on the position-c9 path with **no fallback task** (sampler/UV-edge) if c9 alone doesn't remove the seam.
3. **HIGH — 18-02 has no visual seam check.** Boot-smoke only gates crash/no-FATAL, so a wrong sign (#1) or wrong mechanism (#2) ships cleanly into the expensive 18-03 evidence wave before anyone looks at a pixel. Both want a cheap one-screenshot seam sanity check inside 18-02.
4. **HIGH (Codex) — 18-02 file-scope contradiction.** `files_modified: ONLY Direct3d11_StateCache.cpp` conflicts with "implement an alternate site if the consult rejects c9." Fix: phrase scope as "expected only StateCache.cpp; if the consult selects another site, update `files_modified` + AC before execution."
5. **HIGH (Cursor) — Consult brief omits the real `-0.5` source.** `CuiManager::ms_pixelOffset = -0.5f` (*orchestrator-verified* at `CuiManager.cpp:306`) is the named engine constant behind the baked fingerprint. Add it (and its apply path) to the 18-01 brief + synthesis questions so reviewers reason from the real causal chain, not a "mystery capture."
6. **MEDIUM (both) — ≥3 splash variants are under-specified / not deterministic.** Predefine named routes (planets/scenes/config toggles) + a fallback if only one splash is reachable.
7. **MEDIUM (both) — Regression detection is qualitative.** A ±0.5px HUD/font shift can pass a casual eyeball. Require zoom-crop / pixel-aligned diff on thin text + radial edges; mandate at least one D3D9 post-18-02 re-capture (don't just assert "unregressed").
8. **MEDIUM (Codex) — Edge cases under-specified:** odd / non-power-of-two viewport widths; render-target vs backbuffer sizing (post-processing/RT `setViewport` calls); Debug-vs-Release boot-gate wording (preserve `client.cfg`).
9. **MEDIUM (Cursor, orchestrator-verified+expanded) — Stale docs contradict D-01.** `SUMMARY.md` (lines 14, 49, 79, 128, 205, 236) and `STATE.md` (76, 120) still instruct "implement the `getOneToOneUVMapping` stub at Direct3d11.cpp:995" and cite `StateCache.cpp:1504-1511`. A parallel agent could follow the stale guidance straight into the dead end D-01 warns against. Reconcile them.
10. **LOW (both) — Strengthen the AC positively:** assert the corrected terms USE `1.0f/static_cast<float>(width)` / `…(height)` form (not just "no literal 0.5f/512/1024"); 18-03 should forbid committing `client_d.cfg`/`client.cfg` changes (restore or exclude from diff).

### Divergent / Unique Views
- **Cursor only** (live-tree advantage): found `CuiManager::ms_pixelOffset` as the concrete `-0.5` source, and the stale SUMMARY.md/STATE.md stub guidance. Both orchestrator-verified as real.
- **Codex only:** render-target-vs-backbuffer sizing, odd/NPOT viewport, Debug-vs-Release boot-gate config preservation, and the numeric c9-dump diagnostic idea (dump c9 for a known 1024×768 / 1920×1080 viewport to make a sign mistake obvious before capture).
- **No true disagreement** — the two reviews are complementary; Codex is stronger on viewport/RT edge cases, Cursor is stronger on the live root-cause chain and doc drift. Both rate the set MEDIUM / MEDIUM-HIGH and endorse the wave structure.

### Orchestrator Verification (of the two highest-value findings)
- ✅ `CuiManager::ms_pixelOffset = -0.5f` exists (`CuiManager.cpp:306`, decl `.h:150`, accessor `.h:179/186`). Cursor's apply-site cite `CuiLayerRenderer.cpp:189-205` did **not** match grep (offset applied via `getPixelOffset()` elsewhere) — mechanism real, exact apply-site to be pinned in the replan.
- ✅ Stale stub/line guidance confirmed in `SUMMARY.md` (6 occurrences incl. the "+0.5" note that corroborates the sign correction) and `STATE.md` (2 occurrences).

### Recommended disposition for `/gsd:plan-phase 18 --reviews`
Replan should: (a) **drop the embedded `-(1/width)` sign example** from 18-02/PATTERNS and make the sign a first-class consult question framed around `ms_pixelOffset = -0.5`; (b) **add `CuiManager::ms_pixelOffset` + its apply path to the 18-01 brief**; (c) **add a one-screenshot seam sanity check to 18-02** before 18-03; (d) **resolve the 18-02 file-scope contradiction**; (e) **name a c9/UV-sampler fallback** in 18-03's FAIL path; (f) **predefine ≥3 deterministic splash routes + a target higher-res value** and require zoom-crop/pixel diff + a D3D9 re-capture; (g) **reconcile the stale SUMMARY.md/STATE.md stub guidance** (planning hygiene). None of these block the wave structure — they harden a fundamentally sound plan against the one real failure mode: a wrong-sign / wrong-mechanism fix sailing through to the expensive evidence wave.
