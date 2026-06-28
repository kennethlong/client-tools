# CONSULT-36 — cantina door-snap, NEUTRAL EVIDENCE (treat as given)

Hand this to consultants verbatim. These are MEASURED facts from a live x64 capture, not a
hypothesis. Do not re-derive them; reason forward from them.

## Build / repro
- x64 Debug client (`stage-x64/SwgClient_d.exe`), `rasterMajor=5` (gl05 / D3D9).
- Mos Eisley cantina, player walking OUT the MAIN DOOR (interior foyer cell -> exterior world/terrain).
- Visible bug: ~40% of door exits, the avatar SNAPS up ~4 m for one frame (and sometimes rubberbands).
  More frequent on FAST approaches (large per-frame move).
- Capture method: non-debugger OutputDebugString reader (DBWIN), so NO FPU-precision perturbation
  (Report.cpp:166 only fires under a real debugger). This is TRUE float behavior.

## The captured snap (the only large event in 642 player collision-rewinds)
The `_DEBUG` CORNERSNAP probes, frame 7353, object 127040355 = [player]:

PORTAL probe (CellProperty::Notification::positionChanged), same frame, 3 crossings, all world-Y 5.100:
    foyer1(1) -> world(0)  seg_w (3468.283,5.100,-4850.141)->(3468.458,5.100,-4850.233) len 0.197
    world(0) -> foyer1(1)  seg_w (3468.458,5.100,-4850.233)->(3468.280,5.100,-4850.140) len 0.200
    foyer1(1) -> world(0)  seg_w (3468.280,5.100,-4850.140)->(3468.458,5.100,-4850.233) len 0.200

RESOLVE probe (CollisionResolve::resolveCollisions, RR_Resolved branch), same frame:
    CORNERSNAP-RESOLVE: frame 7353 obj 127040355
        rewind (3468.458, 1.063, -4850.233) -> (3468.283, 5.100, -4850.141)  waypoints 2

CELLJUMP probe: NEVER fired in the whole session (0 of 642). The collision rewind never NET-changes
the parent cell. (Confirms the cell-membership ping-pong is self-correcting; a frame-scoped reversal
guard was already built + REVERTED, commit a6df32348 — do NOT propose it again.)

### What the numbers mean (given facts)
- The foyer1 floor sits at world-Y ~5.10; the terrain just outside the door is at world-Y ~1.06.
- Within frame 7353 the player crosses the portal to `world` and the position update drops them onto
  terrain: `getPosition_p()` = (3468.458, **1.063**, -4850.233).
- `CollisionResolve` then returns RR_Resolved and rewinds to
  `resetPos = moveSeg.getBegin(moveSeg.m_cellB)` = (3468.283, **5.100**, -4850.141)
  = the previous frame's position (foyer threshold) carried into the world cell.
- Net effect: player yanked from terrain (1.06) back UP to the interior-floor height (5.10) for one
  frame = the visible snap. dY = +4.037 m.
- All other 641 player rewinds this session are < 0.13 m (576 are < 0.01 m) — benign wall-graze
  micro-corrections. The door-snap is THIS specific cross-cell, large-vertical-delta RR_Resolved event.

## Relevant code (D:\Code\swg-client-v2)
`src/engine/shared/library/sharedCollision/src/shared/core/CollisionResolve.cpp`
- Move segment built from collider "last" state vs current:
    L265-280: moveSeg.m_cellA = colliderA->getLastCell(); m_pointA = colliderA->getLastPos_p();
              moveSeg.m_cellB = objectA->getParentCell(); m_pointB = objectA->getPosition_p();
              if lengthSquared()==0 return RR_NoCollision; result = resolveCollisions(colliderA, moveSeg, colliderList);
- Inner resolve + the rewind (RR_Resolved branch):
    L284-311: resolveCollisions(objectCell, tempExtent, ms_floorContactList, moveSeg, &ms_obstacleList, positions, footprint)
    L315-339: if(result==RR_Resolved){ ... Vector resetPos = moveSeg.getBegin(moveSeg.m_cellB);
              objectA->setPosition_p(resetPos); moveObjectAlong(objectA, positions); }
- Collider "last" state: `CollisionProperty.cpp` getLastPos_p L1217, getLastCell L1245/1259,
  setLastPos L1273 (m_lastCellObject = CellWatcher).
- `MoveSegment` (getBegin / getDelta / m_cellA / m_cellB) defined in CollisionResolve.cpp / .h
  (also CollisionUtils.cpp).

`src/engine/shared/library/sharedObject/src/shared/portal/CellProperty.cpp`
- L125-208: positionChanged portal walk; targetCell found -> object.setParentCell(targetCell); return false.

## Already FALSIFIED / REVERTED — do not re-propose
- "x64 / deterministic-SSE (or D3D11) fixes it" — FALSIFIED today: snap persists in x64 gl05/D3D9.
  So this is NOT a 32-bit float-codegen artifact; it is renderer- and bitness-independent.
- Frame-scoped cell-reversal guard in positionChanged (a6df32348) — REVERTED; pinned cell membership
  on the wrong side, dropped the interior to skybox. The cell ping-pong is SELF-CORRECTING.
- A prior CollisionResolve resetPos tweak (collision-independent) — REVERTED.
- `_fpreset`/MXCSR restore in endScene — only ~10% effect; REVERTED.

## Reference clean behavior
SWG Restoration x64 D3D9 (`D:\SWG Restoration\x64`) traverses this same door with NO snap.
SWGEmu client also clean. Same engine lineage; our door-exit path is byte-identical to pristine
SWG-Source per a prior audit.
