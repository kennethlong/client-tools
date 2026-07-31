# 2026-07-30 — HANDBACK: wsSetNodeTemplateName authored-node MISS fixed (engine erased authored rows on server replacement)

**Status:** DONE 2026-07-30 night, build-gated + boot smoke, exe restaged. **No contract change — still v25 / 147 names** (behavior-only fix, the occupancy-guard precedent).
**Request:** SWG-Toolkit `2026-07-30-CHANGE-REQUEST-wsSetNodeTemplateName-authored-lookup.md` + the MISS-REPORT filed to our inbox.

## 1. Root cause — your evidence was right, the mechanism was one layer deeper

The lookup was NEVER live-keyed: `ms_reader.find()` searches `m_networkIdNodeMap`, which the
parse populates with EVERY authored node (both load paths). Your byte-verified paradox (node in
the file 54s before the MISS) convicted something *erasing* the row mid-session:

**`GroundScene.cpp` SceneCreateObject handler:** when the server streams an object whose id
already exists as a client-cached (snapshot-spawned) object — exactly a static POB the player
is near, on any hybrid session — it deletes the client copy and called
`WorldSnapshot::removeObject(id)` ("mark the object so it never gets created again"), which
**tombstones AND erases the authored node from the reader map**. From that moment, for the rest
of the session:

- `wsSetNodeTemplateName(id)` → MISS (your defect), and
- `wsSaveSnapshot` **silently dropped that authored building (and its cells) from the saved
  .ws** (tombstone-skip) — a latent data-loss you hadn't hit yet, and
- the id allocator's map-miss free-test saw a still-authored id as FREE (latent collision hole).

Timing fits your log exactly: save-on-load fires at parse completion (row still present →
byte-scan finds it), the server's create for the building lands seconds later, every rebind
after that MISSes. Same on every wsUnload/wsLoad cycle — the server stream re-replaces each time.

## 2. The fix

New `WorldSnapshot::suppressObject(id)` — removes the node's sphere-tree handle (which is the
ENTIRE re-create prevention; the spawn set is the sphere tree) but leaves the authored row in
the map, un-tombstoned. The SceneCreateObject replacement path now calls it instead of
`removeObject` (whose semantics are unchanged — it is an advertised row).

Safety walked before landing: child duplicate-spawn is double-guarded (cell-fill requires
`!cellNode->isInWorld()`, and each child create is preceded by a NetworkIdManager id-exists
check — the server's copies occupy those ids); the walk-away delete path never consulted the
map and its `isClientCachedOnly` guard already refuses server objects; re-approach cannot
re-spawn (no sphere handle). Net behavior delta is exactly: authored data survives.

Also: the MISS log text now says `(not in authored map -- unknown id or editor-removed)` —
"(no live node)" was wrong and sent us both down the wrong alley. Editor-removed (wsRemoveNode)
ids still MISS, per spec — tombstone semantics unchanged.

## 3. Secondary ask — self-test hook

The hook was ALREADY config-gated default-off; what wrote your override dir every load was the
armed key `[ClientGame/WorldSnapshot] wsSelfTestSaveOnLoad=1` in `stage/client.cfg` (armed
2026-07-19 for the Goal-B save gates, long green). **Disarmed** (=0, with a comment). No load
mutates the override dir now.

- Your `stage/override/snapshot/tatooine.ws` (the resurrected copy) is untouched — it currently
  shadows stock with drifted content; consider deleting it to return to a stock baseline until
  a REAL save.
- The size-drift observation (1,380,222 → 1,400,231 per load-save cycle) is a real open item
  we'll chase separately (suspects: OTNL intern accumulation, cell-expansion rows). It does not
  block the rebind loop.

## 4. ⚠️ Expectation for your end-to-end smoke (hybrid vs editor scene)

On a HYBRID server session the server re-streams the building on every approach and its copy
supersedes the client-cached spawn — so even after rebind + save + reload, the **visible**
building interior is the SERVER's (stock) on hybrid sessions. The authored edit persists in the
.ws (and re-saves keep it, now that suppression preserves rows), but to SEE the derived
template + edited .ilf spawn, verify on an editor scene (`game::loadScene`) or any context
where the snapshot layer actually spawns the building. Rebind OK / save OK / byte-verify are
session-independent.

## 5. Gates

- Release/Win32 `/t:SwgClient`: 0 unresolved, exe restaged (clientGame WorldSnapshot.{h,cpp} +
  GroundScene.cpp — exe-side library only, no plugin ABI surface).
- `GetEngineHookPoints` ord-82 undecorated, 147==147 (untouched).
- Boot smoke on the staged exe: clean boot through Game::install into the frame loop at the
  login screen, zero new dumps; the smoke process then CLEAN-exited ~21s in (boot-trace line
  06 = the orderly shutdown path — consistent with the window being closed at the desk, and
  identical on the pre-fix exe, so not a fix regression). Kenny's next live login is the
  fuller pass, per the corrected smoke discipline.

## 6. Smoke steps (yours)

1. Same repro as tonight: hybrid session, cantina, hover decoration → Arm → rebind. Expect
   `[editor.ws] wsSetNodeTemplateName OK: id=1082874 <stock> -> <derived>`.
2. `wsSaveSnapshot` → byte-scan: building row now carries the derived template name; the
   building did NOT vanish from the save despite the server replacement (the §1(b) fix).
3. Editor scene reload → edited interior visible, subtree intact, other instances unchanged
   (the full model-D close-out).
