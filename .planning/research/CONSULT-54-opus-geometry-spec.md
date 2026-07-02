# CONSULT-54 (fresh Opus) — Pure geometry: when does the identical 4-ray diamond MISS the wall?

Read the shared evidence brief: `.planning/research/CONSULT-54-EVIDENCE-BRIEF.md`. Its facts are GIVEN.

## Your angle (math / spec reasoning):
Given the camera-collision algorithm is IDENTICAL in both trees, the see-through must be either a
geometric edge case in that shared algorithm (triggered by our runtime state) or a non-camera cause.
Work the geometry precisely.

The relevant code (OURS, identical to REFERENCE) in `FreeChaseCamera::alter`:
- pivot `start_w = getPosition_w()` (player offset/eye position, inside the cell)
- camera target `end_w = start_w + rotate_o2w(negativeUnitZ * m_zoom)` (behind the player)
- 4 rays offset by ±0.25 in cell X and Y around that segment; `ClientWorld::collide(getParentCell(),
  interimStart_w, interimEnd_w, cms_default, result, CF_allCamera)`; on hit, pull `m_currentZoom` to the
  nearest hit fraction (minus a 0.25/len near-plane bias). Before that, a side-ray at
  `start_w + rotate_o2w(unitX*(m_desiredOffset.x+0.25))` zeroes the shoulder offset on hit.

Derive, as clean geometric conditions, EVERY way the camera end-point can end up on the far side of a
nearby interior wall while these rays report NO blocking hit. Candidates to evaluate rigorously:
1. Pivot already coincident with / on the far side of the wall plane (player hugging the wall so
   `start_w` is within ~0.25 m of it) → the segment barely straddles or starts past the wall.
2. Segment shorter than the near-plane bias `0.25/lineDistance` → the hit fraction clamps to 0 / is
   discarded.
3. The query cell (`getParentCell()`) — the CAMERA's parent cell — differs from the wall's owning cell,
   so the collide query is scoped to a cell whose geometry excludes that wall (portal boundary case).
4. Back-face: the wall's collidable surface normal faces away from the ray direction and the collide
   routine ignores back-facing hits.
5. Thin wall / diamond spacing: the ±0.25 rays straddle a thin pillar/edge and all four miss.

For the MOST LIKELY condition(s), specify the minimal, precise fix to `FreeChaseCamera::alter` (or the
query parameters) that closes the see-through **while preserving** the door-snap fixes described in the
brief (`cs_seamGrazeEpsilon` avatar-side; `cs_cameraPullInSpeed` inward pull-in rate-limit — the fix
must not remove or out-run the rate-limit, and must not reintroduce the instant shoulder-snap). If you
conclude the geometry CANNOT explain it (identical code can't diverge on identical geometry), say so and
name the non-camera cause you'd bet on.

Do NOT propose porting camera code (banned framing). Output = the geometric condition(s) + minimal fix
sketch, or the "not-geometry" verdict with reasoning.
