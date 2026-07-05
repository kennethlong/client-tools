# CONSULT-64 — evidence pack: intermittent portal see-through = engine-side cell culling

All items below are MEASURED or repo ground-truth. Treat as given; do not re-derive.

## Symptom (locked)

- Standing at/near the Mos Eisley cantina entrance, interior/adjacent-cell geometry
  intermittently does not render: either a wrong wall shows through the portal, or raw
  sky/terrain shows where the interior should be. A SMALL camera angle/position change
  restores correct rendering instantly. Self-clearing; no persistence.
- 3 sightings over 2 days + 2 deliberate captures: seen on **gl05/D3D9/Win32** AND
  **gl11/D3D11/x64** → renderer-agnostic AND bitness-agnostic.
- Sightings cluster shortly after zone-in (within ~seconds to minutes).
- No asset/portal/mesh errors in logs during any sighting session.

## RenderDoc capture-and-diff (locked — THE ground truth, 2 broken/good pairs)

Captures `stage-x64\Capture100-103.rdc` (gl11/x64, cantina, same session):
- Pair A: broken=249 draws/26,650 tris; good (slightly different angle)=291/32,842 —
  **42 draws missing**.
- Pair B: broken=74 draws/9,882 tris; good=234/47,959 — **160 draws missing**.
- In BOTH pairs, every draw the frames share diffs as EQUAL (same shaders, same
  counts); the broken frames are simply missing whole blocks of DrawIndexed calls.
- ⇒ **The missing cells' geometry is never SUBMITTED to the renderer.** The defect is
  in the engine's visibility determination, upstream of any graphics API call.

## Architecture anchors (repo ground truth — start here, not from scratch)

- **The visibility system is Umbra dPVS + a cell/portal graph.** FULL dPVS library
  source is vendored at `src/external/3rd/library/dpvs/` (implementation/sources +
  interface headers).
- Engine-side integration: `src/engine/client/library/clientGraphics/src/shared/`
  `RenderWorld.cpp`, `RenderWorldCamera.cpp`, `RenderWorldCommander.cpp`,
  `RenderWorldServices.cpp`, `RenderWorld_CellNotification.cpp`,
  `RenderWorld_OcclusionNotification.cpp`.
- Cell/portal model: `src/engine/shared/library/sharedObject/src/shared/portal/`
  (`CellProperty`, `Portal`, `PortalProperty`, `PortalPropertyTemplate`,
  `PortallizedSphereTree`).

## dPVS runtime status (verified 2026-07-05 — locked)

- dPVS is built from the vendored source for BOTH platforms (the dpvs vcxproj has real
  Win32 + x64 configs; dpvs.lib linked in all five SwgClient configs). All world
  visibility (view-frustum + cell/portal traversal) flows through DPVS::Camera on both.
- The occlusion-buffer feature is config-gated: RenderWorld.cpp:905-925 sets
  DPVS::Camera::OCCLUSION_CULLING from `[ClientGraphics/Dpvs] occlusionMode`
  (default "off"; "auto" = world-cell-only). NEITHER staged cfg arms it ⇒ **occlusion
  culling was OFF during every sighting, on both platforms** ⇒ the occlusion-buffer
  subsystem is ELIMINATED; the miscull is in the portal/frustum traversal or the
  engine-side cell/camera handling that feeds it.

## Historical context (locked facts, NOT hypotheses)

- A DETERMINISTIC see-through-wall at one cantina spot on gl05/32-bit was fixed
  2026-06-20 by adding `D3DCREATE_FPU_PRESERVE` at D3D9 device creation — the D3D9
  runtime had clamped x87 precision, degrading shared portal-visibility math. That fix
  is verified and still in place; the CURRENT bug reproduces on x64 (SSE math, no x87
  clamping) so it is a different instance, though possibly the same math neighborhood.
- The engine recently gained (2026-06→07, all verified fixes): camera pull-in rate
  limit (`cs_cameraPullInSpeed`), footprint seam-graze suppression
  (`cs_seamGrazeEpsilon`), interior zoom cap removal + gl11 collision-from-VB fix
  (CPU shadow), phased WorldSnapshot load, budgeted terrain preload, async-loader
  callback time budget. It is UNKNOWN whether the portal bug predates these (first
  sighting 2026-07-04; the client was not heavily played at the cantina entrance
  before then in this configuration).
- dPVS-era instrumentation from Phases 23/26 was removed after a verdict that DPVS
  itself was NOT the cause of an earlier (different) bug class.

## What is NOT known (open — these are the questions)

- Which gate in the camera→dPVS-query→render-queue path excluded the cells.
- Whether the camera's CONTAINING CELL was resolved wrong (camera thought to be in a
  different cell → different portal graph traversal) vs the portal clip test rejecting.
- What state differs between two frames with nearly identical cameras (dPVS occlusion
  buffer state? cell containment hysteresis? an occluder object present/absent?
  post-zone-in async state still settling?).
