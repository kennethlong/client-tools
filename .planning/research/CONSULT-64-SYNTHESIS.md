# CONSULT-64 SYNTHESIS — intermittent portal see-through (engine-side cell culling)

Five consultants (Codex call-graph, Cursor predicate byte-map, Sonnet dPVS internals,
Opus mechanism ranking, Fable blind-adversarial), fed the neutral evidence pack
(CONSULT-64-portal-culling-EVIDENCE.md). Outputs in the sibling .out files.

## Ground truth going in (RenderDoc, locked)

Broken frames are missing whole blocks of DrawIndexed calls (42 / 160 across the two
capture pairs) while every shared draw is byte-equal → cells never SUBMITTED; occlusion
buffer OFF on both platforms during all sightings (RenderWorld.cpp:905-925 gate,
cfg default "off") → occlusion subsystem eliminated.

## Architecture (Codex + Cursor, convergent)

GroundScene::draw → ClientWorld::draw → Camera::renderScene → RenderWorldCamera::
drawScene → RenderWorld::drawScene (RenderWorld.cpp:857): root cell =
`camera.getParentCell()` (:875) → `ms_dpvsCamera->resolveVisibility(commander, 8, 0)`
(:1093) → commander INSTANCE_VISIBLE → appearance->render(). The ROOT CELL and the
CAMERA POSITION are decided independently: FreeChaseCamera force-copies the PLAYER's
cell onto the camera every frame (FreeChaseCamera.cpp:615-617, portal transitions
disabled during the copy) while the camera's position is separately offset/zoomed/
collision-pulled — at a doorway they can straddle the portal plane (Cursor P9).
No hysteresis or caching anywhere in the containment path; dPVS portal creation only
happens at Portal::setNeighbor during PortalProperty::addToWorld pairing (Codex —
a real post-zone-in registration window).

## The two camps (a productive split)

**CAMP A — stale/wrong camera ROOT cell (containment desync).**
Opus M1 (55%), Fable (adversarial verdict), Cursor P9 as enabler.
- Cell tracking is an INCREMENTAL segment-vs-portal walk (CellProperty.cpp:115-185,
  early-out when no motion at :131); a camera path crossing a cell boundary THROUGH A
  WALL records no crossing → stale cell. The recent camera eases feed exactly that:
  shoulder ease explicitly accepts "brief wall clip" (FreeChaseCamera.cpp:681),
  cs_cameraPullInSpeed lets the camera linger at/behind colliding walls, dt-amplified
  by post-zone-in hitches.
- **Fable's archaeology:** commit a976f81e2 (2026-07-02, two days before the first
  sighting) removed the interior zoom cap whose own comment names "borderline portal
  cell-cull flip" at a PORTALLESS back wall — where a portal clip test cannot be the
  gate but a containment flip can. The bug likely PREDATES the recent fix wave;
  removing the cap re-exposed camera positions that trigger it.
- Fable: intermittency favors history-dependent state (the incremental tracker) over a
  memoryless clip test; and "small angle change restores it" discriminates NOTHING
  (orbit angle IS position; both camps predict instant restore).

**CAMP B — dPVS portal clip/backface degenerate (memoryless float boundary).**
Cursor P10/P11 (his #1), Sonnet's internals find.
- Portal backface cull with hard `EPSILON = 0.0f` (dpvsImpMeshModel.cpp:482-519);
  camera position for portals is translation-only object-space (ImpObject.cpp:1829).
- Portal screen rectangle: `getClippedRectangle` classifies vertices with zero-epsilon
  `clipVal < 0.0f` (dpvsClipPolygon.cpp:51) — **the Umbra author's own in-source doubt:
  `// should this be <= ?`** — degenerate at grazing angles → getTestRectangle fails →
  traversePortal silently rejects the whole downstream transition
  (dpvsVisibilityQuery_Traverse.cpp:557-563).
- Frame-fresh recompute (no persisted state) — a tiny nudge flips a boundary vertex.

**Secondary (both camps agree, lower):** post-zone-in settling — per-cell dPVS
Database rebalancing self-throttles to 2.5% frame time / 1/64 nodes per pass
(dpvsDatabase.cpp:3580/:922) → insertion bursts take many frames to converge; and the
Portal::setNeighbor registration window. Explains CLUSTERING, not the steady-state
sightings. Door/passability and object-to-cell assignment are effectively eliminated
(camera-independent state cannot heal on a 1° nudge — Fable's argument #6).

## Eliminations this round

- Occlusion buffer + its node caching (Sonnet verified isNodeVisible short-circuits
  to true with the flag off — deeper corroboration of the config elimination).
- dPVS x64 pointer-hash truncation (Hash::get re-verifies full 64-bit keys).
- Any x64-only precision/pointer path in the portal walk (plain float32 math, no arch
  branches — consistent with cross-bitness repro).
- `canSeeParentCell` (loaded but has no runtime consumer).

## THE PROBE (landed, config-gated `[ClientGraphics] portalCullProbe=true`)

RenderWorld::drawScene, immediately after resolveVisibility: log-on-anomaly only —
(a) every camera ROOT-CELL change (rare; from→to names = the containment history),
(b) portalsCrossed / visibleCells count flips while the camera moved < 0.10m
(the boundary-flip signature, fires on both break and heal).
Line: `[PortalCullProbe] cell=NAME portals a->b visCells a->b camDelta pos`.
→ SwgClient_report.log; near-zero cost; armed for days.

**Decision rule at the next sighting (screenshot key timestamps correlate):**
- Broken window shows root cell = world/wrong cell (or a missed transition around the
  moment) → CAMP A convicted → fix = camera-cell re-derivation robustness (and/or
  reconcile the force-copy with the collision-adjusted position).
- Broken window shows CORRECT root cell + portalsCrossed/visCells collapse at
  camDelta≈0 → CAMP B convicted → fix = epsilon in the dPVS portal rectangle/backface
  path (the author's own `<=` doubt) or engine-side portal-plane guard band.
- Optional Fable confirm either way: re-arm the old interior zoom cap for one soak
  (its removal is the exposure-change candidate).

## ROUND 2 (2026-07-05 ~09:00) — probe FIRED on first session; BOTH signatures observed

Kenny armed the probe, logged in at the cantina, and hit the bug in TWO places within
90 seconds (screenshots 0009-0012, stage-x64; log times UTC = local+5).

**Sighting 1 (screenshot 0009 @ 13:58:42Z, cantina entrance, pos ~3468 8 -4850):**
view = raw terrain, NO building at all. Probe: camera ROOT CELL OSCILLATING
world↔foyer1 SIX times in ~12s (13:58:43-55) at the same spot (deltas 0.03-0.27m).
When tagged foyer1 with the eye actually outside, the world is only reachable through
the doorway's shrunken portal frustum → the surrounding building-exterior (world-cell
object) culls away; terrain (non-dPVS) survives = exactly the screenshot.
⇒ **CAMP A signature: root/eye split at the entrance (P9).**

**Sighting 2 (screenshot 0011 @ 13:59:00Z, INSIDE foyer1, pos ~3460-3465):**
foyer floor/props/doorframe render (root CORRECT and STABLE at foyer1); the wall
ahead is open starfield. Probe: `portals 1->0 visCells 2->1` at 13:58:58 (the break),
heal `0->4->5` at 13:59:05 with camDelta=0.0037 (4 MILLIMETERS), re-break `5->0` at
13:59:08 with camDelta=0.0061. Whole portal chain collapsing/rebuilding under an
essentially frozen camera with the correct root.
⇒ **CAMP B signature: portal-clip collapse at a near-static camera (P10/P11 /
clipVal<0.0f).** Residual confound: probe round-1 gated on POSITION delta only —
rotation could zero portal counts legitimately (screenshot facing argues against it:
Kenny is looking INTO the missing wall).

**Also noted:** pre-spawn loading window (13:58:28-32, camDelta exactly 0, camera
parked) shows portals 0→3→4→3→2→0→2→5→6→4→3→4→6 flip-flops — either benign
streaming-registration churn or the same instability during load. Unclassified.

**Round-2 probe upgrade (landed):** line now carries fwdDot (camera forward dot
previous frame — 1.0 = no rotation) + the forward vector, closing the rotation
confound. Next sighting decides: count-collapse with fwdDot≈1 = camp B convicted
beyond argument; oscillating root at the doorway remains camp A's (both fixes may
be needed — the two sightings may genuinely be TWO defects sharing a symptom).

## ROUND 3 (2026-07-05 ~09:10) — rotation confound CLOSED; DOOR mechanism promoted to prime suspect

Second armed session (screenshots 0013): the conviction line —
`140910 cell=foyer1 (CHANGED) portals 6->6 ... fwdDot=1.00000` (healthy transition)
followed same-second by `portals 6->0 visCells 7->1 camDelta=0.0000 fwdDot=1.00000`:
the WHOLE portal chain vanished with a BIT-STILL camera (no motion, no rotation),
one frame after a clean root-cell change; healed on 2.7cm. Second collapse at 140916
(4->0, fwdDot 0.99993) immediately precedes Kenny's screenshot (inside foyer1, wall =
open starfield, an NPC NAMEPLATE floating in the void — object culled with its cell,
screen-space UI not). Also decoded: the camDelta=0.0000/fwdDot=0.92388 flip-flop
blocks are the planet-entry SPINNING preview camera (22.5°/frame) — legit, ignore.

**Camp B's "clip test" cannot explain a collapse with an IDENTICAL camera pose** —
the traversal inputs were bit-equal, so some OTHER state changed. The one gate that
is camera-independent AND heals on player movement: **the door hook.**
`DoorObject::alter` (sharedCollision/DoorObject.cpp:298-307) sets
`m_portal->setClosed(!open)` from the door ANIMATION position every alter;
`Portal::setClosed` is edge-triggered (Portal.cpp:373-381) into
`closedStateChangedHookFunction` (RenderWorld.cpp:755-763) which sets the dPVS
portal ENABLED=false → everything beyond culls. Explains: frozen-pose collapse
(door timeline independent of camera), heal-on-tiny-move (proximity trigger
re-entered), post-zone-in clustering (doors initialize closed until first trigger),
sky through an APPARENTLY open archway (doorway with no/invisible drawn door mesh
still closes its portal), the nameplate-in-void. Opus's 2% elimination of doors
("closed would persist") was wrong for PROXIMITY doors — they heal exactly like
this bug heals.

**Round-3 probe (landed):** the closed-state hook now logs every portal
OPEN/CLOSED transition (edge-triggered = rare) with portal ptr, cell, neighbor
cell, dpvs-object presence, camera pos — same [PortalCullProbe] tag, same cfg key.
Decision at next sighting: DOOR CLOSED line at the collapse timestamp for a
foyer1/world portal → door mechanism CONVICTED (fix conversation: why these
doorways close their portals / alter-starvation widening windows / alwaysOpen
data); no DOOR lines at collapse → back to dPVS-internal state (traversal-order /
database settling — Sonnet secondaries).

## ROUNDS 4-5 (2026-07-05 ~15:30 session) — door story REFINED; the naked defect is
## a bistable dPVS traversal flicker at CONSTANT inputs; library instrumented

**Door findings (real, contributing, NOT the core):** DOOR edge logging showed doors
close legitimately behind the player (e.g. 14:36:03, ~1m past the doorway — retail-
normal), buildings initialize ALL portals closed on load (the 14:36:23 whole-building
batch), and NO door mesh renders in any hole screenshot — doorways that close their
portals with no visible door make every legit close a see-through window. One
entrance pair (14:31:05) never re-opened all session — wake starvation is real too.
DOORHIT probes: only 4 lines all session (one wake moment, passage allowed) — during
the standing-in-the-hole windows hitBy NEVER fired (collision trigger silent).

**THE NAKED DEFECT (15:30:50-15:31:26):** player standing still INSIDE foyer1
(1.5cm jitter, fwdDot=1.00000), NO door edges for a minute, root cell stable —
and the traversal flickers portals 0<->4-5 every couple of seconds. Constant
portal-enabled states + constant camera + constant root, alternating full-chain /
zero-chain results ⇒ **the instability is INSIDE DPVS::Camera::resolveVisibility**
(Sonnet's secondaries: non-stable traversal priority queue with first-portal-wins
re-traversal drop; lazily rebalancing per-cell Database). Kenny's "I disappeared
behind the sky" = avatar in a culled neighbor cell (or zoom-in exclusion) while the
flicker was active.

**Round-5 instrument (landed):** the vendored dPVS build now exports per-frame
portal-rejection reason counters (g_swgDpvsPortalRejects[5]: backface /
calculateTransition / testTransition(recursion solver) / getTestRectangle /
createFrustumFromRectangle), reset before each resolveVisibility and appended to
every [PortalCullProbe] line as `rej=bf:x ct:x tt:x rect:x fr:x`. The 0-portal
flicker frames will now NAME the check that killed the traversal. tt: spikes =
recursion-solver first-portal-wins (Sonnet #6); rect:/fr: spikes = the zero-epsilon
rectangle path (Sonnet #1); bf: spikes = plane-side flip; all-zero on a 0-frame =
the portals were never REACHED (cell/object-level gate upstream).

## ROUND 7 + CONSULT-65 (2026-07-06) — THE FIX

Round-6 (three runs): every collapse frame = `tested:0, ph:0`, all reject counters
silent — the cell database never ENUMERATES the portals. Flicker cadence ≈ dPVS
g_timeUntilStatic (1.0s static-recheck heartbeat).

CONSULT-65 (Codex trace + Opus mechanism/fix): two candidate families —
(A) Database::updateObject → removeObject on transiently-bad bounds (Codex #1;
predicts dbRem>0 on collapse), (B) traverseNode ORDERING HAZARD: the node is
frustum-tested against STALE getTestBounds() (:3018-3022) BEFORE the dirty-node
refresh (updateDirtyNode, was :3090) — a straddling leaf due for a bounds-grow is
transiently NODE_HIDDEN for exactly one query, silently dropping every instance
inside (Opus #1; predicts dbRem:0). Both consultants: run the round-7 counters
before fixing — the families need OPPOSITE fixes.

**Round-7 discriminator run (Kenny): every collapse frame = dbUpd:0 dbRem:0
dbAdd:0.** Family A DEAD (the portal never leaves the database). **Family B
CONVICTED.**

**FIX (landed in vendored dPVS, dpvsDatabase.cpp traverseNode):** hoist the
existing `if (v->isDirty() && !updateDirtyNode(v)) return NODE_KILLED;` block
ABOVE the view-frustum test — dirty nodes refresh their bounds BEFORE being
frustum-gated. Pure reordering of work the function already did; NODE_KILLED
handled identically by the caller; only dirty nodes pay the update marginally
earlier (Opus: regression risk LOW).

**Verification protocol:** stand bit-still at the flicker spots 60s+ per spot —
flicker gone; probe lines (if any) show tested:1-3 steady, no portals→0 events at
camDelta≈0/fwdDot≈1, no dbRem/dbAdd churn, frame time flat.

**Remaining, separate (parked as own todos):** door-portal layer — doorways close
portals with NO visible door mesh (every legit close = see-through window until
re-trigger) + one observed whole-session door wake starvation (hitBy silent).
These are engine-side door/data issues, NOT the dPVS flicker.

## SOAK VERDICT + FIX 2 (2026-07-06) — two defects confirmed, both now fixed

Kenny's soak after the dPVS traverseNode reorder (4dea2fdf3): **the bit-still
flicker is GONE** (state no longer jumps while standing still — defect 1 CURED).
Residual: holes now flip ONLY on movement through portals, are STABLE while
still, and — Kenny's decisive observation — running through a doorway leaves the
avatar invisible while the chase camera does NOT pull in (its collision ray sees
the OPEN doorway, nothing wrong).

That is defect 2, the round-2 CAMP A account, now cleanly separated:
**FreeChaseCamera force-copies the PLAYER's cell onto the camera every frame
(FreeChaseCamera.cpp:615-617) while the camera's POSITION lags meters behind** —
crossing a doorway opens a window where the camera is tagged in the new cell
with its eye still in the old one → dPVS roots visibility at the wrong cell for
the eye → the doorway portal backface-culls → whole cells (avatar included)
vanish until the camera catches up. Stable-when-still because nothing re-derives.

**FIX 2 (FreeChaseCamera.cpp, after final camera positioning):** derive the
camera's cell from its own FINAL position by walking the short player→camera
segment through the portal graph (the exact loop
CellProperty::Notification::positionChanged uses; passableOnly=false — sight,
not passage; player's cell = authoritative anchor so per-frame camera teleports
cannot desync). On a cell change, preserve the world pose via setTransform_o2w.

Verify: run in/out through the cantina doorway repeatedly — avatar stays
visible, no holes, camera renders the cell it is actually in; back-room and
door-snap behaviors unchanged (the fix only re-tags the cell, never moves the
camera).

## Consultant outputs

- CONSULT-64-visibility-callgraph-codex.out — call chain + 11 gates + registration windows.
- CONSULT-64-portal-cliptest-cursor.out — predicate byte-map P1-P13 + rank + probes.
- CONSULT-64-dpvs-internals-sonnet.out — dPVS traversal internals, clipVal<0.0f find,
  database settling, x64 clearances.
- CONSULT-64-mechanism-ranking-opus.out — M1-M7 ranking + one-frame classifier probe.
- CONSULT-64-adversarial-fable.out — containment counter-account, zoom-cap archaeology,
  the log-on-mismatch probe design, weakest-fact audit.
