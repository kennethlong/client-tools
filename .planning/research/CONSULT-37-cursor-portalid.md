# Task for Cursor (most detailed code/data reader)

Read `.planning/research/CONSULT-37-doorsnap-fix-EVIDENCE.md` first — treat it as given ground truth.

You are working in this repo (read-only, ask mode). Your angle: **where does a floor edge's
`portalId` come from, and does the doorway seam edge the footprint CIRCLE hits actually carry one?**
This is THE empirical unknown that decides whether candidate fix #4 works.

Trace with file:line precision (focus `sharedCollision/.../FloorMesh.cpp`, `FloorTri.*`,
`FloorMeshReadWrite` / IFF load, `Floor.cpp`, `.flr` asset loading):
1. How is `FloorTri::getPortalId(edgeId)` populated at load? Trace from the IFF/.flr chunk read
   (look near FloorMesh.cpp:843-892 crossable serialization, and any `PortalId`/`portal` chunk) to the
   per-edge field. Is portalId stored per-edge in the asset, or derived?
2. `flagCrossableEdges()` (FloorMesh.cpp:791) — confirm it ONLY uses within-mesh triangle adjacency
   and does NOT consult portalId. Quote the code.
3. Geometric question: when the footprint has radius (~0.5 m) and the CENTER crosses the portal seam
   cleanly, can the CIRCLE clip a DIFFERENT boundary edge of the same tri than the center's edge —
   one whose `getPortalId` may be -1 even though the center's edge has a valid portalId? Trace
   `intersectEdge` / `TestExitTri` / `canExitEdge(...,useRadius)` (FloorMesh.cpp:1836 vs :1913) to
   show how radius changes WHICH edge is hit.
4. Net: for fix #4 to work, the hit edge must have `getPortalId != -1`. From the asset/load code, can
   you tell whether a cantina doorway boundary edge carries a portalId on the seam edges adjacent to
   the portal, or only on the single portal-crossing edge? If undecidable statically, specify the
   exact runtime probe (which variable to log in pathWalkPoint).

Output: a byte/field-level account of portalId provenance + a yes/no/undecidable on whether the
circle's hit edge carries a portalId, with the probe spec if undecidable.
