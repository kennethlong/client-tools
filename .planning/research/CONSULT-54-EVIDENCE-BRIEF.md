# CONSULT-54 — Indoor chase-camera see-through-wall: shared evidence brief

## Treat everything in this section as GIVEN (do not re-derive; do not re-diff to "confirm")

Two source trees on disk (both are full SWG client source):
- **OURS** = `D:\Code\swg-client-v2` — our fork; the client that exhibits the symptom.
- **REFERENCE** = `D:\Code\client-tools` — the SWG-Source client source (`github.com/SWG-Source/client-tools`). The user played a binary built from this lineage last night.

The ground third-person chase camera is `FreeChaseCamera`
(`src/engine/client/library/clientGame/src/shared/camera/FreeChaseCamera.cpp` in both trees).

### MEASURED diffs OURS-vs-REFERENCE (already run, given as fact):
- `FreeChaseCamera.cpp`: the ONLY differences are **additions in OUR tree** — (a) an interior zoom
  cap `ms_interiorMaximumZoom = 3.0f` that clamps `m_zoom` when the target is in a non-world cell,
  and (b) door-snap rate-limiting (`cs_cameraPullInSpeed` easing the shoulder-offset zeroing and the
  inward collision pull-in). **The collision/ray-trace block itself is byte-identical in both trees** —
  the 4-ray diamond `ClientWorld::collide(getParentCell(), start_w, end_w, CollideParameters::cms_default,
  result, ClientWorld::CF_allCamera)`, and the `ms_cameraSimpleCollision` single-ray path.
- `ClientWorld.cpp` (defines `ClientWorld::collide`): **0 diff.**
- `CollisionWorld.cpp`: **0 diff.**  `CollideParameters.cpp`: **0 diff.**
- `FloorMesh.cpp`: differs ONLY by our avatar floor-seam-graze fix (`cs_seamGrazeEpsilon`) — that is
  AVATAR movement collision, not camera.
- `CF_allCamera = CF_terrain | CF_tangible | CF_tangibleNotTargetable | CF_interiorGeometry | CF_childObjects`
  (identical in both trees).
- Camera-class inventory identical (FreeChaseCamera, CockpitCamera, DebugPortalCamera, FreeCamera,
  FlyByCamera, ShipTurretCamera, StructurePlacementCamera).

### SYMPTOM (observed at runtime):
- **OUR client:** standing near an interior wall of a building (POB / player-occupiable structure) and
  rotating the camera to point AWAY from the wall lets the chase camera end up on the FAR side of the
  wall — you see through/past it. Our band-aid: the interior zoom cap (holds indoor camera within 3 m
  so it never reaches the walls).
- **REFERENCE client:** indoor chase camera does NOT clip through walls, stays framed on the player,
  with NO interior zoom-in.

### GOAL:
Remove our interior-zoom-cap band-aid and make the camera collision / keep-visible behave like the
reference (no see-through) **without regressing** our door-snap-prevention fixes
(`cs_seamGrazeEpsilon` avatar fix + `cs_cameraPullInSpeed` camera pull-in rate-limit).

### BANNED framing (falsified — do NOT propose):
"Port the reference camera collision/ray-trace code" / "we are missing the reference's collision
algorithm." The camera-collision SOURCE is proven identical across both trees. The behavioral
difference must originate **elsewhere** (collidable interior data, portal/cell-cull render, config,
build/renderer, or a subtle geometric edge in the shared algorithm). Find WHERE.

### Relevant local history (context, not a conclusion):
- Memory `project_d3d9_32bit_fpu_preserve_cantina_seethrough`: on gl05/32-bit, a MISSING
  `D3DCREATE_FPU_PRESERVE` degraded shared portal-visibility math → a *deterministic see-through-wall*
  at one cantina spot (a RENDER cell-cull flip, later fixed). Our own zoom-cap comment says pressing
  against a back wall "re-triggers the borderline portal cell-cull flip (exterior shows through the
  wall, clears on look up/down)."
- Memory `project_backroom_camera_interior_zoom_cap` + `project_cantina_corner_snap_engine_quirk`:
  the zoom cap and the door-snap fixes (`cs_seamGrazeEpsilon`, `cs_cameraPullInSpeed`) are OUR additions.
