# CONSULT-54 (Cursor) — Exact geometry of the collision block + hunt for any keep-visible we lack

Read the shared evidence brief: `.planning/research/CONSULT-54-EVIDENCE-BRIEF.md`. Its facts are GIVEN.

## Your angle (most detailed code reader — file:line trace):
Two independent tasks.

### Task 1 — Characterize the exact geometric case in `FreeChaseCamera::alter`
In `D:\Code\swg-client-v2\src\engine\client\library\clientGame\src\shared\camera\FreeChaseCamera.cpp`
read the not-first-person collision block (the `ms_cameraSimpleCollision` path AND the 4-ray diamond
path) plus the shoulder-offset side-ray block just above it.
- The camera pivot is `getPosition_w()` (the player eye/offset position), and the camera swings to
  `start_w + rotate_o2w(negativeUnitZ * m_zoom)`.
- Precisely describe the case **"player standing near an interior wall, camera rotated to point AWAY
  from the wall"**: where is the pivot relative to the wall, where is the camera end-point, and what do
  the 4 diamond rays actually test? Identify the concrete reason a wall the camera passes through would
  NOT be caught: e.g. pivot already on/behind the wall plane; ray segment too short; the wall's
  collidable normal is back-faced to the ray; the near-plane offset (0.25) undershoot; the swing places
  the camera outside the pivot's cell so `getParentCell()` is the wrong cell for the query. Be specific
  about which of these is most likely and cite the exact lines.

### Task 2 — Does REFERENCE (`D:\Code\client-tools`) have ANY additional camera keep-visible path we lack?
The brief proves the camera *files* are identical. But scan the REFERENCE `clientGame` (and
`clientUserInterface`, `GroundScene.cpp`, `CockpitCamera`, DebugPortalCamera) for any OTHER mechanism
that keeps the camera off walls indoors that we might not be invoking the same way — e.g. a
near-plane wall push, a portal/cell-aware camera clamp, a `CollisionWorld` camera-specific query,
a config default (`ConfigClientGame`) that flips `cameraSimpleCollision` or a collision toggle, or a
DebugPortalCamera vs FreeChaseCamera selection difference indoors. Report file:line for anything found,
or state clearly "no additional path — behavior must be data/render/config."

Do NOT propose porting camera code (banned framing). Output = file:line findings + your single best
hypothesis for the root cause.
