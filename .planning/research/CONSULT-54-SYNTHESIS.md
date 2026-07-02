# CONSULT-54 — SYNTHESIS: indoor chase-camera see-through-wall

## Locked facts (verified on-disk, not hypotheses)
- Camera-collision SOURCE is byte-identical OURS (`swg-client-v2`) vs SWG-Source reference
  (`client-tools`): `FreeChaseCamera.cpp` differs only by OUR two additions (interior zoom-cap +
  door-snap rate-limiting); `ClientWorld.cpp` collide, `CollisionWorld.cpp`, `CollideParameters.cpp` =
  0 diff.
- Collision-DATA build path is byte-identical too: `ShaderPrimitiveSetTemplate.cpp` (collisionSplit),
  `MeshAppearance.cpp` (collide), `StaticShaderTemplate.cpp`, `ShaderEffect.cpp`, `PortalProperty.cpp`
  = 0 diff. Same game-data TREs. → identical collidable wall triangles in both trees, any renderer.
- `CF_interiorGeometry` is DEAD: defined + in `CF_allCamera` but never consumed by `ClientWorld::collide`
  (Codex + Cursor). Both clients keep the camera inside purely via the cell-appearance mesh + the
  instant inward snap.
- **Renderer confound (Sonnet):** the reference binary is D3D9-only (gl05, no gl11.dll). OUR
  `stage\client.cfg` is currently forced to `rasterMajor=11` (D3D11) — stale RenderDoc leftover. So a
  recent repro on our Release exe ran gl11 vs the reference's gl05. NOT apples-to-apples.

## Live hypotheses after cross-check (Codex's data-divergence REFUTED by 0-diff build path)
1. **Our door-snap rate-limit (Opus + Cursor-C), renderer-agnostic.** `FreeChaseCamera.cpp:782-787`
   rate-limits the inward collision pull-in to `cs_cameraPullInSpeed`=8 m/s. Yawing the boom through a
   wall wants a big one-frame zoom drop; the limit refuses → for the ease frames the boom stays long →
   end-point beyond the wall = see-through. Reference snaps instantly, never lags. Our 3 m zoom-cap
   masks it. Would reproduce on gl05 too.
2. **gl11-only render/portal artifact (Cursor-B + Sonnet).** Camera world-pos exits the cell hull while
   `getParentCell()`/DPVS still treat it as interior (`RenderWorld.cpp:872-889`) → portal visibility
   flips → exterior draws through. The old gl05/32-bit FPU version of this is fixed and x64/gl11 is
   "immune" per Direct3d9.cpp comment — but a distinct gl11 depth/portal artifact fits the project's
   gl11-exclusive-bug history. Would NOT reproduce on gl05.

## Decisive experiment (cheap, adjudicates 1 vs 2)
Force `rasterMajor=5` on OUR client, re-run the exact repro:
- **See-through GONE on gl05** → gl11-only render bug → RenderDoc D3D11 capture at the wall; camera code
  is fine; Opus's fix is irrelevant.
- **Still see-through on gl05** → renderer-agnostic → it's our rate-limit → apply Opus's fix:
  bound the collision *penetration* (`m_currentZoom = min(m_currentZoom, collisionZoom + ~0.25m)`)
  instead of the pull-in *rate*; then delete the zoom-cap band-aid
  (`freeChaseCameraInteriorMaximumZoom=0`). Preserves `cs_seamGrazeEpsilon` + the pull-in ease.

Trigger tell: rate-limit (H1) shows up on FAST camera rotation only; a data/render cause shows on slow
back-into-wall too.

## Per-consultant outputs
- Codex: `CONSULT-54-codex.out` (data pipeline; CF_interiorGeometry dead; shader-collidability path)
- Cursor: `CONSULT-54-cursor.out` (geometry ranking A/B/C; no hidden reference keep-visible path)
- Opus:  `CONSULT-54-opus.out` (not-geometry verdict; rate-limit; penetration-bound fix)
- Sonnet: `CONSULT-54-sonnet.out` (renderer confound; reference is D3D9-only; force gl05 to test)
