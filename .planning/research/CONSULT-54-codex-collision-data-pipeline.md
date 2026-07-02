# CONSULT-54 (Codex) — Collision-DATA pipeline for interior walls (call-graph trace)

Read the shared evidence brief: `.planning/research/CONSULT-54-EVIDENCE-BRIEF.md`. Its facts are GIVEN.

## Your angle (repo tracer / call-graph — DATA, not algorithm):
The camera-collision *code* is identical between OURS (`D:\Code\swg-client-v2`) and REFERENCE
(`D:\Code\client-tools`). So focus entirely on **what geometry actually registers as camera-collidable
for a POB building interior**, and whether `ClientWorld::collide(... CF_allCamera)` could ever return a
hit for an interior wall in OUR tree.

Trace and report (with file:line, both trees where relevant):
1. `ClientWorld::collide(...)` → `CollisionWorld::*` → how `CF_interiorGeometry` / `CF_tangible`
   candidates are gathered. What object/appearance type carries an interior WALL as a camera-collidable
   surface inside a cell? (CellProperty / PortalProperty / Appearance collision extents / CollisionMesh?)
2. Where interior-cell collision geometry is BUILT/LOADED for a POB (portal system, `.pob`/appearance
   load path). Is there a spatial index (SpatialDatabase / SphereTree) the camera query walks, and does a
   cell's wall geometry get inserted into it as camera-collidable?
3. Compare OURS vs REFERENCE on that *data-registration* path — any file on the interior-collision load
   path that differs between the two trees (grep for diffs beyond the camera/floormesh files the brief
   already lists). Especially: sharedCollision extent builders, PortalProperty, CellProperty,
   sharedObject appearance-collision, or anything touching `CF_interiorGeometry`.
4. State plainly: for OUR tree, when the target is inside a cell and the camera end-point is on the far
   side of an interior wall, does the collide() query have any collidable surface to hit? If not, WHY
   (data not inserted? wrong flag? extents absent?) and where the fix would live.

Do NOT propose porting camera code (banned framing). Output = findings with file:line evidence.
