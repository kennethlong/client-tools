# Phase 18: Load-Screen Half-Texel Seam - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-30
**Phase:** 18-load-screen-half-texel-seam
**Areas discussed:** Fix site, Blast radius (validation bar), Evidence (image-independence)

---

## Fix site (getOneToOneUVMapping contradiction)

Scout finding presented to user: `getOneToOneUVMapping` has zero callers and its D3D11 entry is a `scaffold_fatal_stub`, so the ROADMAP-named fix would change nothing visible. The real seam is the D3D9 `D3DFVF_XYZRHW` pre-transformed fullscreen quad (half-pixel offset baked into vertex positions) per the Phase 11 consult.

| Option | Description | Selected |
|--------|-------------|----------|
| Redirect to real blit path | Treat criterion #3 as "fix the half-pixel convention centrally in the transformed-vertex→NDC path"; leave dead `getOneToOneUVMapping` alone | |
| Fix path + implement stub too | Fix the transformed-vertex path AND implement `getOneToOneUVMapping` now as Phase-20 future-proofing | |
| Investigate first, then decide | Make researcher confirm the exact seam source before locking the fix site | |
| **Other (free text)** | **"We could consult here with codex and cursor"** | ✓ |

**User's choice:** Free-text — resolve the fix-site contradiction via a CODEX + Cursor cross-AI consult before committing the edit.
**Notes:** Matches Kenny's established second-opinion workflow. Captured as D-01/D-01a: fix site is consult-gated; research/planning assembles the brief (Phase 11 XYZRHW diagnosis + dead-`getOneToOneUVMapping` finding + live transformed-vertex conversion sites + "central not scattered" constraint); outputs saved under `.planning/research/CONSULT-18-*`.

---

## Blast radius (validation bar)

Context presented: the transformed-vertex (XYZRHW) path drives ALL 2D UI (HUD, fonts, radial, menus), so a central convention fix touches everything 2D, not just the splash.

| Option | Description | Selected |
|--------|-------------|----------|
| Full 2D no-regression sweep | Seam-gone on multiple splashes AND no shift/blur regression on HUD/fonts/radial/char-select 2D | ✓ |
| Load-screen only | Validate only the splash seam; treat other 2D UI as out of scope | |
| Splash + char-select 2D | Splash plus the char-select 2D already captured each phase; skip broader sweep | |

**User's choice:** Full 2D no-regression sweep.
**Notes:** Captured as D-03. A "splash fixed but HUD blurry" outcome is a FAIL.

---

## Evidence (image-independence, criterion #2)

| Option | Description | Selected |
|--------|-------------|----------|
| Multi-image A/B in COMPARISON dir | Matched D3D9/D3D11 pairs across ≥3 splash variants into the Phase-17 D-05 COMPARISON convention | ✓ |
| Single before/after pair | One D3D11 before/after on Tatooine splash (0012), relying on prior COMPARISON.md image-independence finding | |
| You decide at execution | Planner/executor picks image count + capture format | |

**User's choice:** Multi-image A/B in the COMPARISON dir.
**Notes:** Captured as D-05. Demonstrates image-independence rather than relying solely on the prior observation.

---

## Claude's Discretion

- Exact conversion-site edit shape (shader-body passthrough vs CPU NDC conversion vs viewport convention) — decided by the D-01 consult outcome + planner/executor, subject to D-02 (central) and D-04 (D3D9 unregressed).
- Which 3 splash variants to capture, and the deterministic zone/config path to reach them.
- Any temporary vertex-dump instrumentation gating (off-by-default, D3D9 untouched).

## Deferred Ideas

- Implementing `getOneToOneUVMapping` for the Phase 20 minimap — deferred to Phase 20 against that phase's real call sites; do not pre-build the still-uncalled function here.
