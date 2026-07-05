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

## Consultant outputs

- CONSULT-64-visibility-callgraph-codex.out — call chain + 11 gates + registration windows.
- CONSULT-64-portal-cliptest-cursor.out — predicate byte-map P1-P13 + rank + probes.
- CONSULT-64-dpvs-internals-sonnet.out — dPVS traversal internals, clipVal<0.0f find,
  database settling, x64 clearances.
- CONSULT-64-mechanism-ranking-opus.out — M1-M7 ranking + one-frame classifier probe.
- CONSULT-64-adversarial-fable.out — containment counter-account, zoom-cap archaeology,
  the log-on-mismatch probe design, weakest-fact audit.
