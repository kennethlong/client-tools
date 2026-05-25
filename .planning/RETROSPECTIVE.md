# Project Retrospective

Living retrospective across milestones. Newest milestone first.

## Milestone: v2.0 — Modernisation

**Shipped:** 2026-05-25
**Phases:** 7–11 (5) | **Plans:** 29 tracked | **Audit:** tech_debt (functional PASS)

### What Was Built
A modern MSVC/C++20/MSBuild SWG client booting to character select and rendering a ground scene under both a D3D9 and a new D3D11 renderer (runtime-selectable via `rasterMajor`). Dead-code removal (designed), STLPort→MSVC STL migration (via wholesale adoption of Koogie's tree), DPVS profiling verdict, and the long-tail D3D11 renderer plugin.

### What Worked
- **Option D (Phase 9 pivot).** Rather than hand-migrate STLPort per-file, adopting Koogie's already-migrated MSBuild tree wholesale (merge `479d35df3`) collapsed ~30 commits of risky refactor into ~3. Pragmatic leverage of upstream community work.
- **Cross-AI consults (CODEX + Cursor).** Repeatedly unblocked opaque D3D11 walls (X4016 register collision, the WRITE_DISCARD-garbage cbuffer hypothesis, SRV0 binding, trianglefan index expansion) where textual rewrite had hit its limit.
- **Empirical "draw activity observable" vs "visible geometry" bar separation** (CODEX insight) kept the shader-binding arc honest about what each iteration actually proved.
- **Diagnostic infrastructure as first-class** — InfoQueue file sink, frame-stats, show-window helper, screenshot baselines — made fix-to-advance debugging tractable on a 20-year-old engine.
- **Fix-to-advance discipline.** Solo, ad-hoc, problem-driven cadence shipped a working renderer faster than a rigid plan would have.

### What Was Inefficient
- **Artifact-vs-reality drift.** Ad-hoc work + the Option-D base swap left planning docs (REQUIREMENTS/PROJECT) describing the abandoned CMake/whitengold tree while the active tree is MSBuild. The milestone audit surfaced this; docs were re-anchored at close. Accepted cost of pragmatic velocity.
- **Phase artifacts scattered across repos.** Phases 1–9 lived in the old whitengold repo; only 10–11 were here until the 2026-05-25 consolidation import.
- **D3D11 iteration sprawl.** Phase 11 ran ~45 iterations across many sub-plans (11-09.x) — the visual-parity cascade was hard to bound; the real blocker (asset PS pipeline) was only named late.

### Patterns Established
- Renderer selection via a single `Gl_api` loader (`Graphics.cpp`) multiplexing DLL families by `rasterMajor` — D3D9 and D3D11 coexist; D-05 (keep D3D9 buildable) as a standing invariant.
- D3D11 cbuffer matrices transposed at upload (col-vec engine vs row-vec bytecode).
- Matched D3D9-vs-D3D11 screenshot baselines as the visual-parity verification method (vs a formal VERIFICATION.md flow).

### Key Lessons
1. Adopting upstream work wholesale can beat re-deriving it — but it orphans the prior approach's artifacts; reconcile the docs at the boundary, not later.
2. On a legacy engine, a deterministic 2D repro (the load-screen half-texel seam) is worth more than a dozen messy in-world captures for isolating a class of bug.
3. Functional reality (builds/boots/renders) is the source of truth; planning docs are scaffolding, not contract.

### Cost Observations
- Model: Opus (quality profile). Cross-AI consults (CODEX/Cursor) used for opaque-error unblocks.
- Sessions: many; long-running solo hobby cadence over ~3 weeks (2026-05-05 → 05-25).

---

## Cross-Milestone Trends

| Milestone | Phases | Audit | Headline |
|-----------|--------|-------|----------|
| v2.0 Modernisation | 7–11 | tech_debt | MSVC/C++20/MSBuild client; D3D9+D3D11 selectable |
