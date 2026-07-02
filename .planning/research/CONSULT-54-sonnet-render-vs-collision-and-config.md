# CONSULT-54 (fresh Sonnet 5) — Is it a PORTAL-RENDER miscull, not camera penetration? + config/build angle

Read the shared evidence brief: `.planning/research/CONSULT-54-EVIDENCE-BRIEF.md`. Its facts are GIVEN.

## Your angle (lateral / out-of-the-box):
The naive story is "the camera physically goes through the wall." Challenge it. Two lateral threads:

### Thread A — Render miscull vs. actual camera penetration
Our own zoom-cap comment + memory `project_d3d9_32bit_fpu_preserve_cantina_seethrough` say pressing
the indoor camera against a back wall re-triggers a **portal cell-cull flip** (exterior renders THROUGH
the wall, clears on look up/down) — a RENDER artifact, not the camera leaving the room. Investigate
whether the user's "see-through wall" is actually this portal-visibility miscull rather than the camera
end-point crossing the wall. What distinguishes them observationally? What in the portal/cell-cull path
(`PortalProperty`, `CellProperty`, cell visibility, the renderer's cell traversal) could make the
exterior draw through an interior wall when the camera is near-but-inside? Does the REFERENCE binary
avoid it because it renders the portal correctly (not because its camera collides differently)?
Consider: which renderer was OUR client running when observed (gl05/gl06/gl07/gl11), and does the
symptom depend on renderer? (If it's render-side, the fix is NOT camera collision at all.)

### Thread B — Config / build differences
The camera *code* is identical, so a behavioral gap can come from *inputs*:
- `ConfigClientGame` camera keys, `cameraSimpleCollision` default, `freeChaseCamera*` settings — diff
  OURS vs REFERENCE (`D:\Code\swg-client-v2` vs `D:\Code\client-tools`) and the shipped `.cfg` the
  reference binary uses vs our `stage*/client*.cfg`.
- `camera/freechasecamera.iff` data (zoom settings, DEFH offsets) — could the reference ship different
  camera IFF data that keeps the indoor framing tighter naturally?
- Any renderer / feature-flag difference that changes portal or collision behavior.

Deliverable: which thread (render-miscull vs actual-penetration vs config) best explains identical code
producing different behavior, with the reasoning. You have read access to both trees on disk. Do NOT
propose porting camera code (banned framing).

Write your final answer to `.planning/research/CONSULT-54-sonnet.out` (and also return it).
