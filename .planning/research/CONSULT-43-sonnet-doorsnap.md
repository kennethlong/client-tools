# CONSULT-43 — Sonnet angle: lateral / render-backend timing & latching

First read `.planning/research/CONSULT-43-CONTEXT.md` (locked facts, banned frames, the
shared question) and skim `.planning/debug/cantina-corner-snap.md`.

YOUR ANGLE (stay in this lane — lateral / non-obvious mechanisms are your strength):

Open question: what could make a one-frame POSITIONAL discontinuity at a building doorway
VISIBLE under one rendering backend (D3D9: gl05/gl07) but NOT another (D3D11: gl11), when
the simulation and (believed) camera matrix are shared?

Consider ANY mechanism, especially non-obvious ones:
- render-thread vs sim-thread timing / when each backend samples object transforms
- frame-data latching / double-buffering of object or camera transforms (does one backend
  use last-frame's transform for one object for one frame?)
- position interpolation/extrapolation (client-side movement smoothing) that one backend
  path engages and the other doesn't
- a D3D9-only per-object transform recompute or render hook
- the chase camera's OWN cell handling / its transform at the portal
- DPVS / cell-visibility traversal differences between backends that change which cell's
  geometry+transform is used for the crossing frame

Propose the 2-3 MOST plausible mechanisms and, for each, the single observation that would
distinguish it from the others. Don't converge prematurely — divergent framings are wanted.

Output per CONSULT-43-CONTEXT.md "Output format": mechanism hypothesis(es), file:line
evidence, one empirical discriminator each.
