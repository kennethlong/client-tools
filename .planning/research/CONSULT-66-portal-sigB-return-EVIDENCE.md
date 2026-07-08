# CONSULT-66 — portal see-through "Signature B" RETURNED (evidence pack)

Neutral evidence pack for a fresh diagnosis round. Treat everything in
"LOCKED ground truth" as measured fact — do not re-derive it.

## LOCKED ground truth

1. **Sighting** (2026-07-07 02:32 UTC, x64 Release client, gl11/rasterMajor=11,
   Mos Eisley, inside a POB building at approx 3457 5 -4847): player standing
   inside a building; the player's own cell renders (props visible at screen
   edge); EVERYTHING beyond every portal of that cell renders as skybox/stars
   — no terrain, no city, no neighbor cells. Screenshot stage-x64/screenshots/
   screenShot0025.jpg taken 02:32:27 UTC. The state persisted at least 16s
   (02:32:11 → 02:32:27) and was still present when the session ended
   (~02:32:3x). Unknown whether camera movement would have cleared it.

2. **Probe trace** (armed `[ClientGraphics] portalCullProbe`; probe code in
   `src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp`,
   reject counters exported from vendored dPVS
   `src/external/3rd/library/dpvs/implementation/sources/dpvsVisibilityQuery_Traverse.cpp`
   via `swgDpvsGetPortalRejects()`; DOOR lines from
   `sharedCollision/.../DoorObject.cpp` / `SpatialDatabase.cpp`).
   Full untruncated windows: `.planning/research/CONSULT-66-raw-trace.txt`
   (sick run + healthy contrast). Critical sequence (UTC):

   ```
   02:31:52  login (fresh session; buildings still streaming in)
   02:31:55  mass DOOR CLOSED wave as a nearby POB initializes (doors init
             CLOSED at building load), incl. hall/bedroom cells + a
             foyer1<->world pair (portal ptrs EFB85190/EFBB6930)
   02:32:08  DOORHIT-WAKE foyer1<->world passageAllowed=1 (x2 pairs)
             DOOR OPENED cell=foyer1 neighbor=world  portal=F5B4EA80 dpvs=yes
             DOOR OPENED cell=world  neighbor=foyer1 portal=EEFC9960 dpvs=yes
             (NOTE: different portal POINTERS from the 02:31:55 wave — cell
             names repeat per building / objects may be re-created; match by
             pointer)
   02:32:09  cell=foyer1 (CHANGED) portals 7->1 visCells 8->2 tested:3   (camera enters foyer1)
   02:32:09  cell=foyer1 portals 1->7 visCells 2->8 tested:14
   02:32:09  cell=foyer1 portals 7->8 visCells 8->9 tested:16
   02:32:10  cell=foyer1 portals 3->2 visCells 4->3 tested:3
   02:32:10  DOOR CLOSED cell=foyer1 neighbor=world portal=F5B4EA80 dpvs=yes camPos=3470.34 7.64 -4845.35
   02:32:10  DOOR CLOSED cell=world  neighbor=foyer1 portal=EEFC9960 dpvs=yes (same camPos)
   02:32:11  cell=foyer1 portals 1->0 visCells 2->1 camDelta=0.0783
             fwdDot=0.99995 rej=bf:0 ct:0 tt:0 rect:0 fr:0 ph:0 tested:0
             dbUpd:0 dbRem:0 dbAdd:0 pos=3464...
   -- no further probe lines this session (probe logs only count-flips /
      root-cell changes; a STUCK state emits nothing) --
   ```

   The anomaly line: **portals=0, visCells=1, tested=0, ALL dPVS reject
   counters zero** — the traversal tested no portal at all from a cell that
   has several interior portals (foyer1 connects deeper into the building),
   while the camera moved normally (camDelta 0.08/frame) and the player kept
   walking deeper into the building (pos 3470 -> 3464 -> 3457 at screenshot).

3. **Healthy contrast** (same building class, earlier session 2026-07-06
   21:37-21:38 UTC, in the raw-trace file): the same foyer1<->world door pair
   closes (21:38:09) and traversal stays healthy (`tested:2`, counts step
   normally, no collapse). Many prior sessions since the fix wave were clean,
   including two full verification passes on both renderers.

4. **History**: this matches "Signature B" from the CONSULT-64/65 arc
   (tested:0 while deep in-cell), which had NOT reappeared since the five
   landed fixes (commits 4dea2fdf3, 7577dc9a7, 458c7d386, 10821506b):
   dPVS dirty-node update hoisted above the stale-bounds frustum test;
   camera cell DERIVED from the camera's own final position (player->camera
   portal-graph walk, passableOnly=false); meshless doors never call
   Portal::setClosed; dPVS ImpMeshModel::backFaceCull EPSILON 0 -> -0.05;
   20cm hysteresis in the camera-cell derivation (a crossing whose portal
   plane sits within 20cm of the eye is not taken). Synthesis:
   `.planning/research/CONSULT-64-SYNTHESIS.md`. Known residual filed then:
   real-door trigger brittleness (SpatialDatabase capsule misses, one-shot
   wake, DoorObject cms_keepNoAlter self-unschedule).

## Environment deltas since the last verified-clean state (relevance UNKNOWN — treat each as a candidate, none as the presumed cause)

- Exe rebuilt same day from the same master lineage plus ONE new commit
  (dde0c2a0a): a sharedFile change adding a per-SearchPath negative-lookup
  cache to TreeFile loose-path probing (misses-only hash set; TOC lookups
  unchanged). The FIRST post-rebuild session (21:37 UTC healthy contrast
  above) ran this exe cleanly.
- Config changes in stage-x64/client.cfg, cumulative order:
  1. `[SharedFile] asynchronousLoaderCallbackTimeBudgetMs=6` (per-frame
     wall-time budget for AsynchronousLoader::processCallbacks; defers
     surplus async-load completion callbacks to later frames). Present
     during the healthy 21:37 UTC session.
  2. `stallWatchdogMaxDumps` 6 -> 12. Present during the healthy session? NO —
     added together with (1) BEFORE the healthy session, so yes, present.
  3. `[Direct3d11] preventDriverInternalThreading=false` (re-enables NV
     driver internal threading; previously true on x64) and
     `[Direct3d11] censusLog=true` (per-frame CSV row). **FIRST SESSION with
     these two keys = the sick run.** (Win32 has soaked flag=false since
     2026-07-03 without a reported hole, but Win32 play time since then is
     limited.)
- The sighting happened ~19s after a fresh login, while buildings/assets were
  still streaming in (see the 02:31:55 door-init wave; also a texture-miss
  warning at 02:31:58).

## ADDENDUM (Kenny, post-sighting, LOCKED)

- The hole does NOT flicker — it stays SOLID while the camera holds position,
  and CLEARS when you move.
- It appears only at a PARTICULAR DISTANCE and within a SPECIFIC ANGLE WINDOW
  — i.e., it is a deterministic, positionally reproducible geometric
  condition, not a transient/stuck state. (This supersedes the "persists 16s"
  framing: it persisted because the camera stayed inside the window.)
- Consequence for ranking: any mechanism that is angle-independent (stale
  database node, stale camera-cell binding that survives movement, one-shot
  event corruption) must explain why the state tracks VIEW ANGLE and clears
  on movement. Mechanisms where the traversal's entry/first test fails inside
  a geometric window fit naturally — with the constraint that at failure the
  traversal never ITERATED portals (tested=0, all reject counters 0), rather
  than iterating and rejecting them.

## ADDENDUM 2 (post-round measurements, LOCKED)

- Stall watchdog (100ms threshold) for the sick session: stalls ONLY during
  zone-in settle (21:31:51-56 local, worst 924ms real-class) and at session
  exit (21:32:35). **NO main-thread stall at or near the collapse onset
  (21:32:10-11)** — the collapse occurred on smooth frames.
- gl11 census CSV (per-frame) same session: frame spikes align with the
  zone-in window only; steady-state clean. vsB0 (per-draw cbuffer updates)
  median 39/frame, p99 51 — separate perf datum, not implicated here.

## ADDENDUM 3 (Kenny, live experimentation at the repro spot, LOCKED)

- The hole clears by CAMERA LOOK ALONE: looking up a certain amount, down a
  certain amount, left a certain amount, OR right a certain amount each
  clears it — i.e., the broken state occupies a bounded angular window
  around a specific pose, healed by rotating far enough in ANY direction.
- Moving BACKWARD a certain distance also clears it.
- Walking FORWARD, the player passes THROUGH the affected portal normally
  (collision/containment fine) and then becomes INVISIBLE to the chase
  camera — the crossed-into cell is not in the camera's visible set.
- Adjudication: this kills the sealed-cell (B) and membership (M) classes
  (camera orbit cannot re-open a door or re-add a DB instance) and convicts
  the WRONG-BOX class W: the doorless foyer1→foyer2 portal is passable but
  its dPVS test volume/bounds sit in the wrong place; the frustum only
  intersects the misplaced box at certain poses. Leading origin story: the
  portal's DB instance was placed with a stale/uninitialized transform
  during the fresh-login streaming wave and — portals never move, dbUpd:0 —
  nothing ever re-places it (the healthy session's streaming order placed it
  correctly).

## What is wanted

The mechanism of the 02:32:11 collapse (portals=0 / tested=0 / all reject
counters 0, persistent), consistent with BOTH the sick timeline and the
healthy contrast — and the single cheapest experiment or probe extension that
would discriminate the top candidate mechanisms on the next sighting.
