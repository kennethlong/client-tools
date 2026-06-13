# CONSULT-43 — Cursor angle: floor/Y-handoff byte-level detail

First read `.planning/research/CONSULT-43-CONTEXT.md` (locked facts, banned frames, the
shared question) and skim `.planning/debug/cantina-corner-snap.md`.

YOUR ANGLE (stay in this lane — meticulous file:line detail is your strength):

Meticulously read the floor-following / ground-snapping / `Footprint` vertical-position
code and the Object cell↔world transform reframe (`Object::getTransform_o2w`,
`Object::setParentCell`, `attachToObject_w`/`detachFromObject`, and the snapToGround /
terrain-height path). Repo paths: `sharedObject/.../object/Object.cpp`, `sharedCollision`
(Footprint, floor), `sharedTerrain` if relevant.

At the interior→exterior door crossing, document EXACTLY (file:line) how and WHEN the
player's Y transitions from interior-cell floor (world-Y ≈ 5.1) to exterior terrain
(world-Y ≈ 1.06):
- Which field holds the player's Y at each step (cell-local pos, world pos, footprint pos)?
- Is there a single frame where the parent cell has already flipped to `world` but the Y
  still carries the interior value (or the reverse — world Y under an interior parent)?
- Does ground-snapping run BEFORE or AFTER the parent-cell reparent on the crossing frame,
  and does that ordering differ in any renderer-conditional code path?

Output per CONSULT-43-CONTEXT.md "Output format": mechanism hypothesis, file:line evidence,
one empirical discriminator.
