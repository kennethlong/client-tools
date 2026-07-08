# CONSULT-66 SYNTHESIS — portal Signature B return (2026-07-07 sighting)

Round: Codex + Cursor + fresh Opus + fresh Sonnet, adversarial Fable close.
Evidence: CONSULT-66-portal-sigB-return-EVIDENCE.md (+2 addenda) + raw-trace.

## Verdict state after the round

**Killed:**
- S2 stuck per-render exclude-list disable (Fable call-graph: the exclude
  machinery — FreeChaseCamera/Cockpit/Hyperspace/EnvMap/TangibleObject
  CVF_gm_only — never touches a portal's dPVS object).
- C stale/wrong camera root cell (same cell binding produced tested:3/14/16
  for a full second pre-collapse; pose-independent latch contradicts the
  deterministic angle+distance window that clears on movement).
- B sealed cell AS A COMPLETE MECHANISM (killer fact from the healthy trace:
  foyer1→foyer2 crossed with ZERO door lines ⇒ foyer1 has a DOORLESS,
  permanently-enabled portal; at collapse the camera faced it nearly dead-on
  from ~8m and it was still never tested). B survives as the BACKGROUND
  CONDITION: the 02:32:10 front-door close legitimately reduced foyer1's
  enabled portal set to exactly that one doorless portal — the close unmasked
  the defect, it did not zero the traversal itself.
- A "stale bounds" in its original form (post-CONSULT-65, updateDirtyNode runs
  BEFORE the VF test — bounds are freshly recomputed; and the healthy session
  ALSO closed this pair with the camera inside foyer1, no collapse).

**Source-verified mechanics (Fable, vendored dPVS):**
1. Node-level NODE_HIDDEN (dpvsDatabase.cpp:3035-3044) touches ONLY
   DPVS_PROFILE statistics (compiled into Release —
   dpvsPrivateDefs.hpp:306), never the probe's reject counters. Silent zero.
2. ImpCell::enableObject (dpvsImpCell.cpp:243-249) REMOVES a disabled
   portal's object from the cell DB (re-adds on enable). A truly sealed cell
   yields tested:0 all-zeros.
3. THIRD silent route (new): object-level VF cull dpvsDatabase.cpp:3214-3218
   (+exact-OBB 3234-3238) skips an ENABLED, ENUMERATED portal with `continue`
   BEFORE isObjectVisible_INTERNAL — tested increments only inside it
   (dpvsVisibilityQuery_Test.cpp:57-59). ⇒ Opus's "tested:0 proves zero
   enabled portals" is INVALID; Cursor's "enabled portals would hit
   isObjectVisible" equally wrong for an AABB that fails the frustum test.

**Surviving field (two classes):**
- **W — wrong-box:** the lone enabled (doorless foyer2) portal is in the DB
  but its LEAF BOUNDS (A′) or its OWN object AABB (D) is wrong/degenerate ⇒
  silently VF-culled via routes 1 or 3. Predicts: sharp deterministic
  distance+angle window; clears on ANY small camera motion; re-enterable at
  the same pose with no door edges. ← matches Kenny's locked addendum
  naturally.
- **M — membership:** portal enabled-flagged but ABSENT from the DB
  (removeObject without re-add during streaming / duplicate-cell shuffle).
  Predicts: hole whenever an aperture is on screen, at all poses in the cell,
  after doors reduce the set to the broken portal; clears ONLY via door
  re-trigger or cell crossing.

## The adjudicators

**Zero-code (Kenny, next sighting):** without moving the character, orbit the
camera a few degrees — does it clear? If not, one zoom tick? If it only
clears when WALKING — did the step pass near a doorway? After clearing, does
returning to the exact pose re-trigger it with no door edges in between?
Camera-only-orbit clears + pose-re-entry re-triggers ⇒ W. Walk-only clears ⇒ M/B.

**STUCK0/CLEAR0 probe (implemented this session — see below):** after
resolveVisibility, when cameraCell is interior AND rejects[6]==0: 1 Hz STUCK0
line + a CLEAR0 edge line when it heals, bypassing the count-flip gate
(Sonnet: the gate made stuck and healed indistinguishable). Fields: cell name
+ ptr + dpvs cell ptr, per-portal {isClosed, dpvsEnabled, inDatabase}, portal
count, per-query DPVS profile-statistic deltas (nodes VF-culled, objects
traversed, objects VF/exact-culled), camera cell-space pos + fwd.
Conviction table:
- all portals c1/e0 ................ B truly sealed (would also disprove the
                                      doorless-portal read — reopens B)
- any c0/e0 ........................ S2-class desync (resurrect the hunt)
- nPort==0 / wrong ptr / any e1+db0 . M membership class
- e1+db1, dVFnode>0, dObTrav==0 .... A′ leaf-bounds hide
- e1+db1, dObTrav>0, dObCull>0 ..... D portal object-AABB cull
- e1+db1, all deltas 0 ............. timestamp-skip path (dpvsDatabase.cpp:
                                      3057-3069) — escalate

## ROUND 2 — STUCK0 probe field results (2026-07-07 ~03:19-03:25 UTC, 7 relogs)

Kenny reproduced (took ~7 relogs — a per-SESSION component exists: whether a
login has the broken state varies; once present it is geometrically
consistent). Probe verdicts:

- **M (membership) and B (sealed) are DEAD by measurement**: every hole
  episode shows the foyer2 portal `{c0 e1 db1}` — open, enabled, IN the
  database. Cell/dpvsCell pointers stable.
- **"Wrong box" is DEAD in its literal form**: `aabb0 = 35.63,-0.19,-10.47 ..
  35.63,4.39,-3.64` — a correct flat portal quad exactly at the aperture.
  The BOX is right; the TEST kills it.
- **The defect, measured**: e.g. 03:25:35-40 — camera posC (44.9→41.7, 3.2,
  -3.4..-4.1), fwd (-0.882,-0.198,0.427), staring near head-on at the enabled
  in-DB portal from 6-9m: `tested:0` for 5+ seconds, while dObTrav≈34-42 and
  dObVF≈7-14 (the traversal IS running and IS VF-culling objects). The portal
  dies in one of the SILENT VF stages. Same failure mirrored from the foyer2
  side. CLEAR0 fired on a 10cm camera move (cm-scale threshold).
- Remaining ambiguity: WHICH silent stage (node VF-cull vs object AABB test
  vs exact OBB test vs node timestamp-skip/occlusion). → Round-2 probe:
  g_swgDpvsPortalRejects extended [10..13] = portal-attributed kills at each
  site (dpvsDatabase.cpp object AABB 3214→[10], exact OBB 3234→[11], node
  VF-cull→[12], node timestamp-skip/occlusion→[13]); STUCK0 line now prints
  pObVF/pObXVF/pNodeVF/pNodeSkip. Next repro names the guilty stage directly.

## ROUND 3 — total-capture probe set (2026-07-07, repro now ~1-in-12 relogs)

Kenny ran 12 dry relogs — repro is rare, so the probes now capture everything
in one hit AND detect the broken state without the visual window:

- **CELLSTATE** (RenderWorld, on entering any interior cell): one line per
  portal — {c,e,db} + the portal's cell-space box + its owning BSP NODE's
  getTestBounds (m_tightBounds ? *m_tightBounds : m_bounds — two sources,
  either can be stale) + leaf/dirty/instanceCount/nodesInstancedIn. If the
  per-session defect is bounds-state, every walk-in classifies the session
  broken/healthy — dry runs become the healthy-baseline corpus.
- **KILLDETAIL** (dpvsDatabase kill sites → static string export
  swgDpvsGetLastPortalKillString): on any portal-attributed cull while
  STUCK, prints site (obVF/obXVF/nodeVF/nodeSkip/nodeOcc) + tested box +
  active clip mask + every active frustum plane equation — the failing
  test's exact operands.
- **dpvsPosC** on STUCK0/CLEAR0: dPVS's own cameraToCell translation vs the
  engine's posC — a mismatch convicts the camera-transform hand-off.
- New dpvs exports: swgDpvsGetObjectNodeInfo (node bounds + flags),
  swgDpvsGetLastPortalKillString. All config-gated under portalCullProbe.

Field protocol: play normally; walk into the building once per login (feeds
the state diff); if the hole shows, linger 2s then clear. Win32 caveat: dpvs
is a DLL there — rebuild + hand-stage stage/dpvs.dll before any Win32 session.

## ROUND 3 FIELD RESULT — CONVICTED + FIXED (2026-07-07, cold-disk-cache repro)

Kenny hit it on a cold-cache start (log archived:
stage-x64/logs-archive/2026-07-07-consult66-CONVICTION/). The probes told the
whole story in one session:

- **KILLDETAIL: `site=nodeVF box=35.63,4.34,-10.59..150.46,165.50,114.44`** —
  the portal's NODE test bounds are a phantom box floating ABOVE the room
  (portal spans y -0.19..4.39; its node starts at y=4.34 and reaches
  y=165/x=150/z=114). The frustum only sweeps that elevated volume when
  looking UP / backing away — Kenny's clear thresholds, exactly.
- **CELLSTATE**: foyer1 P1 box correct, `nNodes=1` — the portal held ONE
  instance, in a top-sliver leaf whose region barely grazes it; its healthy
  twin (same aperture, foyer2 side) held 2+. The cantina's portal nodes ALSO
  carry world-scale bounds (y up to 2891!) but those giant boxes CONTAIN the
  camera → always pass VF → no visible hole there. `dirty0` everywhere —
  bounds are stable, not stale-pending.
- Mechanics (source): `calculateTightBounds` clamps to the node REGION
  (dst.clamp(m_bounds)) — so a single-sliver instance yields exactly the
  observed phantom test box. The instance-coverage killer is
  **`Database::splitInstance`**: for STATIC objects straddling a split it
  refines the box-based child mask with an EXACT triangle-vs-AABB test —
  zero-epsilon against box faces (vendor comment concedes "extremely nasty
  floating-point accuracy errors"; a both-children-fail path can even
  deleteInstance). A zero-thickness portal quad coplanar with a region
  boundary face is a coin flip → the child holding most of the portal loses
  its instance. Split placement depends on the streaming-time object
  population → per-session (cold cache shifts it), deterministic afterwards.

**FIX (dpvsDatabase.cpp splitInstance): portals never take the exact-mesh
refinement — `if (ob->isStatic() && !ob->isPortal())` — keeping the
conservative box-based child mask (guaranteed instance coverage of every
child the box overlaps; cost = at most a few extra flat-quad instances).**
Same defect family as the CONSULT-65 backface epsilon (zero-epsilon boundary
math on flat portals).

Post-fix verify signal: CELLSTATE on walk-ins — a portal's printed node box
should never exclude the portal's own box when nNodes=1; the hole should be
unreproducible; KILLDETAIL pNodeVF with phantom boxes should never fire.
Probes stay armed as soak tripwires.

## Also learned / corrected

- Sonnet delta grades: negative cache LOW (clean prior session), async budget
  LOW-MEDIUM (present in healthy run), driver-threading flag MEDIUM on timing
  only — and the stall-cascade hop is now DEAD (watchdog + census show NO
  main-thread stall at collapse onset). No cfg was reverted; no delta is
  convicted.
- Evidence-pack correction (Fable): the "healthy contrast = camera in world"
  framing undersold — the healthy session also closed this pair with the
  camera inside foyer1, cleanly.
- Sonnet probe-hygiene point (adopted): count-flip gating cannot prove
  persistence; STUCK0/CLEAR0 heartbeat added.
- vsB0 census (separate, for the cbuffer-ring work): median 39, p99 51
  updates/frame.
