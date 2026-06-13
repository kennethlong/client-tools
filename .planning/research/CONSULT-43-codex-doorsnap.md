# CONSULT-43 — Codex angle: call-graph + transform-latch timing

First read `.planning/research/CONSULT-43-CONTEXT.md` (locked facts, banned frames, the
shared question) and skim `.planning/debug/cantina-corner-snap.md`.

YOUR ANGLE (stay in this lane — call-graph tracing is your strength):

Trace the COMPLETE per-frame call path that writes the PLAYER object's world position and
parent cell across a single building-door EXIT frame — from the controller/`alter` move,
through `positionChanged` / `setParentCell` / `attachToObject_w` / `getTransform_o2w`, to
the point where the renderable object-to-world transform is LATCHED/read for that frame's
draw. Map every site that writes the player's Y or its parent cell.

Key things to determine:
- At what point in the frame does each renderer backend READ the player/camera transform
  for drawing, relative to the writes above? Does the D3D9 backend latch it at a different
  point in the frame ordering than D3D11 (e.g. earlier/later in the alter→render sequence,
  or a per-object transform recompute on the D3D9 side)?
- Is there a write site that leaves the player's transform in a cell-frame-relative or
  stale state for the span between the parent-cell flip and the next floor-follow/ground-snap?

Output per CONSULT-43-CONTEXT.md "Output format": mechanism hypothesis, file:line evidence,
one empirical discriminator.
