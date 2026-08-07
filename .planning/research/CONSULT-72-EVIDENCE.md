# CONSULT-72 — EVIDENCE PACK (shared, identical for every consultant)

Repo: `D:/Code/swg-client-v2` — a Star Wars Galaxies client (C++, MSBuild, Win32 + x64).
You are one of four consultants given DIFFERENT angles on the same defect. Answer only your angle.

## The function in question

`ClientWorld::findClosestCellObjectFromWorldPosition(Vector const & position_w)` in
`src/engine/client/library/clientGame/src/shared/core/ClientWorld.cpp`.

It performs `ms_tangibleSphereTree.findInRange(position_w, 1.0f, objects)`, then walks `objects`
looking for one with a `PortalProperty` whose containing cell has a valid `NetworkId`. If it finds
none it returns the WORLD cell (the "outside" cell).

A diagnostic probe in that function reports, on every call:
`candidates` = `objects.size()` from that findInRange · `portals` = how many of those had a
PortalProperty · `rejectedForId` = how many had a containing cell whose owner had an invalid
NetworkId.

## LOCKED measurements — treat as given, do not re-derive

1. **OFFLINE (reproduces, every time).** A scene is entered by a custom shim that calls
   `Game::setSinglePlayer(true)` and then `Game::setScene(true, terrainFilename, playerFilename, nullptr)`.
   With no server connection. At world position `<3448.00, 4.00, -4824.00>` — a point PHYSICALLY
   INSIDE the Mos Eisley cantina building — the probe reads:

   ```
   [cellAtPos] WORLD pos=<3448.00,4.00,-4824.00> candidates=0 portals=0 idValid=0 rejectedForId=0
   ```

   `candidates=0` means the sphere-tree range query returned an **empty vector**. Nothing was found
   and rejected; nothing was found at all.

2. **SERVER-CONNECTED (does NOT reproduce).** Same client, same function, a session with a live
   server connection, at `<3442.00, 5.00, -5021.00>` inside a different building:

   ```
   [cellAtPos] HIT pos=<3442.00,5.00,-5021.00> candidates=2 portals=1 cell=insurance building=1106500
   ```

3. **A snapshot reload repairs it.** After the offline scene load, performing an unload-then-load of
   the world snapshot (`WorldSnapshot::unload()` then `WorldSnapshot::load(sceneId)`) makes the SAME
   offline position return `candidates=2 portals=1 cell=cantina building=1082874`.

4. **Physically walking through the building's door works correctly** — the interior renders, portal
   transitions behave. That code path does not call this function.

5. **Interior decoration objects DO appear** in the offline scene inside that same cantina (these are
   client-only objects created by `ClientInteriorLayoutManager` and parented to the building's cells).

6. The building at that position is a POB ("portalized object building") that comes from the world
   snapshot (`.ws`) file, and is a snapshot-created object in the offline case.

## BANNED framing — falsified by direct measurement, do NOT propose or re-derive it

The theory that this is caused by **the scene-load shim failing to destroy the outgoing scene**
(leaking it, and causing `ClientWorld::install()` to run a second time over a live world) is
**FALSIFIED**. It was tested directly by arranging for that teardown to actually run. The reading was
identical: `candidates=0 portals=0 idValid=0 rejectedForId=0`. The teardown omission was a real
separate defect and has been fixed; it is **not** the cause of this symptom. Do not spend any of your
answer on it, and do not build a mechanism that routes through it.

## Ground rules

- **Cite file:line for every claim.** Claims without a citation will be discarded.
- If you cannot establish something from the source, say "not established" — do not guess. A short
  answer with three solid citations beats a long one with a plausible story.
- Do not propose fixes. Establish mechanism.
- You are being cross-checked against three other consultants on different angles. Independent
  convergence is the signal; agreement reached by guessing is worse than useless.
