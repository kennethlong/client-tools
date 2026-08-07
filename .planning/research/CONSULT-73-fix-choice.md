# CONSULT-73 — fix choice for the WorldSnapshot sphere-handle tombstone

Repo: `D:/Code/swg-client-v2` (Star Wars Galaxies client, C++, MSBuild). You are one of two
consultants on a **design choice**, not a diagnosis. The diagnosis is done and confirmed by
experiment — do not re-litigate it.

## LOCKED — established, confirmed by a pre-registered experiment. Do not re-derive.

`WorldSnapshot` keeps a proximity index of `.ws` nodes, `ms_sphereTree`
(`src/engine/client/library/clientGame/src/shared/core/WorldSnapshot.cpp`). A node is only ever
created into the world if `ms_sphereTree.findInRange(position_w, 1.f, ms_queryList)` returns it
(`:1217`). Each node holds a `SpatialSubdivisionHandle`; handle == 0 means it is NOT in the index and
can never be created.

**The defect:** during a server-connected session, snapshot nodes get their handles stripped
permanently, for the life of the process:

1. `GroundScene.cpp:2528-2544` — on `SceneCreateObject` for a POB the snapshot already spawned, the
   client deletes its copy and calls `WorldSnapshot::suppressObject` (`:1606-1619`), which strips the
   node's handle. Comment: *"mark the object so it never gets created again."* Correct for that
   session — the server copy supersedes it.
2. `WorldSnapshot.cpp:1373-1375` — any failed create (e.g. `CEC_objectAlreadyExists`) also strips.

Then `game::loadScene` (an offline "editor scene", our shim `engine_gameLoadScene` in
`src/game/client/application/SwgClient/src/win32/engine_advertise.cpp`) loads the SAME terrain name.
`WorldSnapshot::load` **early-returns at `:676-680` before any re-parse** when `ms_sceneName` still
matches — and `ms_sceneName` is only cleared by `engine_wsUnloadSnapshot` (`:2946-2955`). So the
stripped handles survive into the editor scene and those buildings **cannot** be created.

**Confirmed by experiment:** fresh client process → editor scene immediately, never logging in →
Mos Eisley and the cantina render correctly with all interior edits. Log in first, then editor scene
→ broken, buildings absent. The connected phase is the inflictor. A manual `wsUnloadSnapshot`+load
repairs it (it forces the re-parse).

Note `SwgGodClient`, SOE's own world editor, loads scenes as `load("")` then `load(sceneId)`
(`BuildoutAreaListView.cpp:101-102`) — a forced full re-parse. Precedent exists.

## THE CHOICE

**(a) Force a genuine re-parse on `loadScene`** — clear `ms_sceneName` (or equivalent) so `load()`
rebuilds `ms_sphereTree` from scratch. Matches SwgGodClient. Simple. **Costs a full `.ws` re-parse
(~3.1 s measured) on every editor scene load** — a real regression to the tool's edit loop.

**(b) Re-arm stripped handles without re-parsing** — walk `ms_reader`'s nodes and re-add any with a
zero handle to `ms_sphereTree`. Near-free, no parse. **The stated worry:** a handle is zero both for
*tombstoned* nodes AND for nodes the `:1034-1040` gate deliberately never added — blindly re-arming
the second class could resurrect content the engine intentionally excluded.

## QUESTIONS

1. **Is the worry in (b) real?** Read the population gate at `WorldSnapshot.cpp:1020-1045`. Can a
   zero-handle node that was *deliberately never added* be distinguished at re-arm time from one that
   was *added then stripped*? Note the gate predicate appears re-evaluable per node. If it is
   re-evaluable, does that dissolve the objection — and what, exactly, would the correct re-arm
   predicate be? Be precise; this is the crux.
2. **What breaks under (b)?** Consider at least: a re-armed node whose object still exists (duplicate
   creation / `CEC_objectAlreadyExists`), interaction with `ms_loadedList` bookkeeping, the
   `ms_parsePending` window, and whether re-arming during a live scene (rather than at load) is safe.
3. **What breaks under (a)?** Beyond the 3.1 s cost. Consider re-entrancy, the ordering against the
   outgoing-scene teardown our shim now performs, and whether a re-parse discards anything the
   consumer cares about (they hold unpersisted in-session edits as snapshot nodes).
4. **Is there a third option either of us has missed?** E.g. scoping the repair, making suppression
   scene-scoped rather than process-scoped, or recording suppression separately so it can be undone.
   If a better option exists, say so — the two on the table are not sacred.
5. **Recommend one**, with the failure mode you would most fear from your own recommendation.

## RULES

- Cite `file:line` for every claim. "Not established" is a valid and valued answer.
- This is a real change to shipping code; prefer the boring correct option over the clever one.
- The investigator has been wrong three times today by reasoning from a function's *name or shape*
  instead of reading what it does. Read the code.
