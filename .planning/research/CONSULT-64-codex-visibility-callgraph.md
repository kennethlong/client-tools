You are consulting on a rendering investigation. Read the evidence pack first:
D:\Code\swg-client-v2\.planning\research\CONSULT-64-portal-culling-EVIDENCE.md
Everything in it is measured ground truth — treat as given, do not re-derive.

Your task — a precise CALL-GRAPH TRACE of the frame's visibility/submission path,
with file:line citations, in the repo D:\Code\swg-client-v2:

1. From the ground scene's camera render entry down to individual appearance draws:
   who calls whom through RenderWorld/RenderWorldCamera/RenderWorldCommander
   (src/engine/client/library/clientGraphics/src/shared/) and the dPVS query
   (DPVS::Camera::resolveVisibility or equivalent — vendored dPVS interface headers at
   src/external/3rd/library/dpvs/interface). Where exactly does a visible object get
   enqueued for rendering (the commander callbacks)?
2. Enumerate EVERY gate on that path that can exclude an entire CELL's contents from
   submission in one frame: camera cell containment resolution, dPVS cell/portal
   objects and their enable/disable states, portal open/closed flags, clip/frustum
   tests done engine-side before or after dPVS, appearance-level active flags.
   For each gate: what INPUT DATA it depends on (camera transform, cell id, portal
   geometry, occlusion buffer) and which of those inputs could plausibly differ
   between two nearly-identical camera frames.
3. How do cells/portals get REGISTERED with dPVS (RenderWorld_CellNotification /
   PortalProperty creation) and is there any window (e.g. shortly after zone-in /
   async load) where a cell exists engine-side but its dPVS counterpart is absent,
   disabled, or has stale geometry?

Report: the call chain as a compact tree with file:line; then the gate list (each
with data dependencies); then any registration-window findings. Flag anything you
could not verify as UNVERIFIED. Mechanism only; do not propose fixes.
