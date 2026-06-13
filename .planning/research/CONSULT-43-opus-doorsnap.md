# CONSULT-43 — Opus angle: coordinate-frame math & the "identical matrices" paradox

First read `.planning/research/CONSULT-43-CONTEXT.md` (locked facts, banned frames, the
shared question) and skim `.planning/debug/cantina-corner-snap.md`.

YOUR ANGLE (stay in this lane — transform math / spec reasoning is your strength):

Reason rigorously about the coordinate-frame reconciliation when EXITING a building portal:
interior cell space (building-local; floor ≈ 0.1 local, which maps to ≈ 5.1 world) versus
world/terrain space (≈ 1.06 world). The ~4 m delta must be reconciled across the cell→world
reparent.

Attack the central paradox:
- IF the renderable object-to-world and the camera world-to-view are truly identical across
  the D3D9 and D3D11 backends (LOCKED context lead — verify it), then a backend-specific
  snap is logically IMPOSSIBLE — so EITHER (a) the matrices are NOT actually identical at the
  portal frame in one backend, OR (b) one backend performs an extra/different transform
  recompute or applies the cell offset at a different stage.
- Determine which, with the math. Is there a frame where object-to-world is assembled from a
  MIXED frame — e.g. interior cell offset still applied while the parent is already the world
  cell, or a world-space position with a not-yet-cleared interior local offset? Walk the
  `getTransform_o2w` composition (parent-cell transform ∘ object-to-parent) at the reparent
  step and show where a 4 m Y term could appear/persist for exactly one frame.
- Quantify: does the predicted one-frame Y error equal the observed ~4 m (interior floor
  vs terrain)? If so, that's strong support.

Output per CONSULT-43-CONTEXT.md "Output format": mechanism hypothesis, file:line evidence
(+ the transform composition), one empirical discriminator. If you find the matrices are NOT
identical across backends, state it plainly with evidence.
