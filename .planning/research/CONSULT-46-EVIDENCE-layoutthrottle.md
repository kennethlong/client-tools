# CONSULT-46 — should we throttle ClientInteriorLayoutManager? (risk / value / design)

## Context (treat as GIVEN)
Modern MSVC v145/C++20 rebuild of 2003 SWG client at D:\Code\swg-client-v2. We are reducing the
per-frame "jerk"/stutter when zoning into the Mos Eisley cantina interior (CONSULT-45). Already landed:
- **#1 disk VS bytecode shader cache** (`Direct3d9_ShaderCache`) — kills the runtime D3DCompile stall;
  first-ever zone-in is now a disk HIT after one warm run. VALIDATED (45 stores cold, 45 hits warm,
  first-load measurably smoother).
- **#4 elapsedTime clamp** — `[SharedFoundation] minFrameRate=10` (Clock caps the simulated frame step
  at 0.1s) to bound the camera/movement LURCH on any slow frame. (Does not speed the slow frame.)

**Residual:** a "small stutter" on the FIRST cantina entry of a session (re-entry within a session is
smooth). Crew (CONSULT-45) attributed the residual to: (a) the **interior-layout creation burst** and
(b) **texture create+upload** (D3DPOOL_MANAGED, device-affine, WDDM-amplified, one-time-per-session).

## The candidate fix #2 (UNDECIDED — this consult decides if/how)
`ClientInteriorLayoutManager::update()`
(`src/engine/client/library/clientGame/src/shared/object/ClientInteriorLayoutManager.cpp:62-130`)
iterates the visible cells and, for each not-yet-applied cell, creates **ALL** of that cell's
interior-layout objects **synchronously in one frame** (inner loop :103-128: per object
`ObjectTemplateList::createObject` :109 -> setParentCell/setTransform/addObjectNotifications/
endBaselines/addToWorld). It marks the cell **applied at :84 BEFORE** the inner create loop. A newly
visible cantina cell thus materializes its whole contents in a single frame (renderer-independent --
why the stutter shows on all renderers/bitness).

Reference pattern: `WorldSnapshot` already throttles creates with a per-frame cap
(`WorldSnapshot.cpp:914` `ms_maximumNumberOfCreatesPerFrame`, via a pending-create LIST of stable
buildout `Node*`). The interior-layout objects instead reference RUNTIME `CellProperty*` /
`TangibleObject*` (building) pointers that can be destroyed on cell/building unload.

## The three questions (this consult)
1. **RISK.** If we throttle (split a cell's burst across frames via resume-state or a pending queue),
   what breaks? Specifically: when a building/POB or cell unloads (player leaves, LOD eviction, fast
   in-out), what happens to in-progress interior-layout creation and to queued/pending creates? Is
   there a safe invalidation/teardown hook, or do we get dangling `CellProperty*`/`TangibleObject*`
   -> missing or duplicate interior objects? Failure mode of getting it wrong = silently missing
   cantina props (worse than the stutter).
2. **VALUE / should we even do it?** Given #1 and #4 already landed, how MUCH of the residual stutter
   is the layout burst vs the texture upload? Is throttling expected to give BIG returns, or is texture
   upload the real residual (making #2 low-ROI)? Are there cheaper/safer alternatives that capture most
   of the benefit? Recommend do / don't.
3. **DESIGN.** If worth it, the SAFEST throttle: resume-index vs pending-queue; how to guarantee no
   missing AND no duplicate objects; how to handle cell/building unload mid-creation; budget value.

Each consultant has one angle (below). Stay on it; convergence from independent angles is the signal.
