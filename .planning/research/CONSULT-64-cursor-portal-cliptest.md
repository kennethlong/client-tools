You are consulting on a rendering investigation. Read the evidence pack first:
.planning/research/CONSULT-64-portal-culling-EVIDENCE.md
Everything in it is measured ground truth — treat as given.

Your task — a DETAILED file:line byte-map of the geometric tests that decide
cell-to-cell visibility, in this repo:

1. `src/engine/shared/library/sharedObject/src/shared/portal/` — Portal.cpp,
   PortalProperty.cpp, PortalPropertyTemplate.cpp, CellProperty.cpp: map every
   geometric predicate: portal plane construction (winding, normal direction),
   point-vs-plane tests and their epsilons (or absence of epsilons), the camera/point
   cell-containment resolution (how the engine decides WHICH CELL the camera is in —
   trace it end to end, including the world-cell fallback), portal open/closed and
   passable logic, and any hysteresis or caching of containment results.
2. `RenderWorldCamera.cpp` + `RenderWorld.cpp` (clientGraphics/src/shared): how the
   camera's cell is handed to dPVS each frame; any engine-side portal clipping done
   in addition to dPVS.
3. For EACH predicate: enumerate its boundary/degenerate cases — camera exactly on or
   within epsilon of a portal plane, camera in the portal doorway volume, portal quad
   containment vs plane-only tests, transforms applied in the wrong space, NaN/denorm
   propagation — and say what the OBSERVED failure would look like for each (whole
   cell missing? wrong wall visible? sky visible?).
4. Rank the predicates by consistency with the locked facts: whole cells missing from
   submission, restored by a tiny camera move, at a building ENTRANCE portal
   (world↔interior boundary), cross-platform, clustering shortly after zone-in.

file:line everything. Mechanism analysis only; no fixes.
