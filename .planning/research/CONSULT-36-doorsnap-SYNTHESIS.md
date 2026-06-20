# CONSULT-36 SYNTHESIS — cantina door-snap root cause (4-AI crew + live x64 capture)

Date: 2026-06-19. Evidence: `CONSULT-36-EVIDENCE-doorsnap.md` (frame 7353, non-debugger DBWIN capture).
Crew outputs: `CONSULT-36-{codex,cursor}.out` (door-snap run); Opus/Sonnet inline.

## ROOT CAUSE (convergent)
The visible door-snap is `CollisionResolve::resolveCollisions` resolving the player against the
**stale CANTINA FOYER FLOOR contact** on the single frame the player crosses the portal from foyer1
(floor world-Y ~5.10) to the world/terrain cell (world-Y ~1.06).

Mechanism, step by step (all four consultants agree on this chain):
1. During `alter`, `CellProperty::positionChanged` portal-walks the player to `parentCell=world` and
   the position update legitimately drops them onto terrain at world-Y 1.063. (PORTAL probe; CELLJUMP
   never fires — cell membership is correct and self-correcting. Do NOT touch portal cells.)
2. In the post-alter `CollisionWorld::updateStep`, the resolver runs. `explodeFootprint` dumps ALL
   currently-attached floor contacts into `ms_floorContactList` — INCLUDING the foyer1 floor contact,
   which is still attached this frame (it is only pruned later in `updatePostResolve`/`sweepContacts`,
   AFTER the resolver already consumed it). [Sonnet Hyp-A; CollisionWorld updateStep ordering]
3. `findFirstContact` -> `findContactWithFloor` (CollisionResolve.cpp:741-815) tests the move against
   that foyer floor: it zeroes vertical motion (`delta.y=0`, L762) and runs `floor->canMove`. Because
   the player has walked off the foyer mesh edge, it returns a walk-edge hit (PWR_HitEdge class) ->
   `RR_Resolved`. `isFloorWithinThreshold` does NOT reject it: the foyer floor sits ABOVE the new
   position, and that test only rejects floors >0.5 m BELOW. [Codex Q1; Sonnet Hyp-A]
4. The RR_Resolved branch (CollisionResolve.cpp:315-339) rewinds:
   `resetPos = moveSeg.getBegin(moveSeg.m_cellB)` = last-frame foyer-threshold position carried into
   world space = (…, 5.100, …) [Cursor + Opus: getBegin carries cellA's floor identity, not cellB's;
   only "accidentally correct" when m_cellA==m_cellB]. `setPosition_p(resetPos)` then
   `moveObjectAlong(positions)`.
5. The replay does NOT descend back to terrain: `positions` (waypoints=2) are BOTH anchored in the
   foyer contact cell at ~5.1 — waypoint0 = the foyer floor contact point (L423, contactCell=foyer1),
   waypoint1 = the post-slide endpoint with both ends in that same contactCell (L459-463). [Codex Q2]
   -> player ends the frame elevated at ~5.1 = the visible +4.04 m snap (+ rubberband when the next
   alter corrects from the stored elevated state).

## DIVERGENCE THAT SHARPENED IT (the productive split)
- **Opus** proposed: stop using `getBegin(m_cellB)`; seed the rewind from the resolver's own first
  valid waypoint `positions[0]`. **REFUTED by Codex + the capture:** `positions[0]` is itself the
  FOYER FLOOR contact point at ~5.1, not terrain — so seeding from it still leaves the player
  elevated. The waypoints are floor-consistent *with the wrong floor*. This rules out a
  "use-the-resolver's-own-anchor" fix.
- This refutation isolates the true defect: the problem is UPSTREAM of the rewind — a floor contact
  belonging to a cell the player has ALREADY LEFT (foyer1) is being resolved against at all on the
  exit frame.

## CANDIDATE FIX (cleanest, surgical — converged Codex+Sonnet, geometry-blessed by Opus)
Gate floor-contact resolution on the contact's cell matching the object's CURRENT parent cell:
in `findContactWithFloor` (or where floor contacts are admitted to `ms_floorContactList`), skip /
return no-contact for any floor whose `getCell()` != the object's current `getParentCell()`. On the
exit frame this excludes the foyer floor (object is already `world`), so the resolver does not fight
the legitimate terrain drop the portal walk already applied. Same-cell wall/floor resolution (the 641
benign grazes) is unaffected.

Why NOT the alternatives:
- Top-level `if (moveSeg.m_cellA != moveSeg.m_cellB) return RR_NoCollision`: blunter — also
  suppresses legitimate obstacle collisions that co-occur on a cross-cell frame.
- Opus's `positions[0]` seed: refuted above (foyer-anchored waypoints).
- Reverted dead-ends (do NOT revisit): cell-reversal guard in positionChanged (a6df32348), prior
  CollisionResolve resetPos tweak, _fpreset/MXCSR.

## CONFIRM BEFORE CODING (Sonnet's minimal probe)
Add a `DEBUG_REPORT_LOG` in `findContactWithFloor` printing the floor contact's cell vs the object's
current parent cell. Re-capture a door exit. Expected on a snap frame: floor cell = `foyer1`, object
parent cell = `world`. That nails the stale-cross-cell-floor-contact as the direct trigger and proves
the gate is correct before we touch shipped collision-response code (high blast radius — affects ALL
movement). Probe is reversible; build + one in-world door run confirms.

## STATUS
Phase 36 VERIFY-01 = FAILED (snap real). 36-02 (probe strip) stays BLOCKED — the CORNERSNAP harness
stays in until the snap is actually fixed and re-verified.
