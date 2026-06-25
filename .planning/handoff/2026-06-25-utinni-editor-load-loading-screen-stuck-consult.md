# Provider Consult — editor `loadScene` works, but the loading screen never dismisses (no PlayerObject offline)

**From:** Utinni (consumer) · **To:** swg-client-v2 (provider) · **Date:** 2026-06-25
**Status:** CONSULT (root-caused; needs a provider fix-direction decision). Follows the v6
`game::loadScene` handback — that entry is correct and shipped; this is the *next* lifecycle gap it
exposed on the **offline advertised client**.
**Re:** `2026-06-25-utinni-scene-load-lifecycle-HANDBACK.md`.

---

## 0. TL;DR

v6 `game::loadScene` (`Game::setScene(true, terrain, player, nullptr)`) is wired on the consumer and
**works**: the SceneCreator lifecycle runs, the thunk returns cleanly (~3 s, no Fatal/throw/AV), and the
engine renders the terrain. **But the fullscreen terrain loading screen never dismisses** — the client
sits on the spinning load/cut screen forever (engine loop healthy and pegged; frames progress through
render→UI phases; it is NOT a hang, NOT a perf cliff, NOT present/composite).

**Root cause (traced in engine source):** the loading-screen teardown is gated on
`isFinishedLoading()`, which **unconditionally requires a non-null `PlayerObject` (ghost)**. The offline
editor load builds only a `CreatureObject` from the avatar `.iff` — the ghost is normally minted from
**server character data**, which the advertised editor client (no connection) never receives. So
`isFinishedLoading()` is structurally false forever → loading screen stays up.

This is a **provider/engine** concern: the consumer only calls the advertised thunk and cannot influence
player setup. The consumer-side v6 wiring stays staged and **uncommitted** pending this.

---

## 1. Evidence (engine source, this repo)

**Teardown site —** `GroundScene::updateLoading()`
(`src/engine/client/library/clientGame/src/shared/scene/GroundScene.cpp`):
```
2087   if (m_sentSceneChannel && finishedLoading)
2089       CuiLoadingManager::setFullscreenLoadingEnabled (false);   // the dismiss
2090       _onFinishedLoading();                                     // sets m_loading = false
```
`m_sentSceneChannel` is set at 2063 (inside the 2059 gate) and also requires `finishedLoading` on a
client. So **every dismiss path needs `finishedLoading == true`.**

**The blocker —** `GroundScene::isFinishedLoading()` (1825):
```
1837   bool const hasPlayerObject = (Game::getPlayerObject() != NULL);
1839   return (cachedFileManagerDone && spacePreloadedAssetManagerDone && worldSnapshotDone
1843           && loaderIsIdle && terrainGenerationStabilized && hasPlayerObject);
```

**Why it's NULL offline —** `Game::getPlayerObject()` (`core/Game.cpp:1758`):
```
1770   result = creatureObject->getPlayerObject();   // the contained player-data "ghost"
```
The single-player `GroundScene` ctor (929-963) creates **only the creature**
(`ObjectTemplate::createObject(playerFilename)` at 941) — no ghost is attached. The ghost is a
server-driven contained object; offline it never exists.

**Why the handback's cited callers worked —** `SwgCuiLocations.cpp:179`,
`SwgCuiCommandParserScene.cpp:288`, `SwgCuiSceneSelection.cpp:264` all call `setScene(true, …)` from a
**connected session where a ghost already exists**, so `hasPlayerObject` is already true. The editor is
the first caller to drive `setScene` with **no connected player**.

**Why "just use single-player mode" doesn't fix it on its own —**
- The legacy offline loaders (particle/menu/single-player ground-scene: `ms_singlePlayer = true;
  setScene(false, groundScene, avatar, 0)`) are **all `#if 0`'d out** (Game.cpp:849, 926-953). There is
  **no live shipping path** that does an offline scene-load with a dismissable loading screen.
- `ms_singlePlayer` is still a live flag, but the loading code only consults it to **skip the network
  `clientReady` send** (GroundScene.cpp:2103). `isFinishedLoading()` requires `hasPlayerObject`
  **regardless of single-player**, so setting the flag alone still sticks.

---

## 2. The question / fix-space (all provider-side)

The editor needs `loadScene` to leave the client in a **renderable, loading-screen-dismissed** state with
no connection. Options, roughly increasing fidelity / effort:

**Option A — editor/single-player completion bypass (smallest engine change).**
Make the loading path treat the ghost as satisfied when offline. Minimal shape:
- Thunk: `utinni_gameLoadScene` sets single-player before the load —
  `Game::setSinglePlayer(true); Game::setScene(true, terrain, player, nullptr);`
- Engine: relax the gate to honor the existing flag —
  `bool const hasPlayerObject = (Game::getPlayerObject() != NULL) || Game::getSinglePlayer();`
  (isFinishedLoading, GroundScene.cpp:1837).
This reuses the flag the loading code already partly respects (2103). **Risk:** `getPlayerObject()` stays
NULL, so any post-load code that assumes a non-null ghost can crash downstream (same class as the §4
"editor exercises a path that assumes a connected player" hazard — e.g. `generateHighestId`). Likely
surfaces per-feature, guard as they appear.

**Option B — mint a client-side PlayerObject (ghost) offline (cleanest, most work).**
Attach a real ghost to the loaded creature so `isFinishedLoading()` completes naturally and downstream
`getPlayerObject()` consumers are satisfied. No existing offline ghost-creation path survives (all the
single-player setup is `#if 0`'d), so this means building/finding one.

**Option C — dedicated editor-load completion (scoped, explicit).**
A provider editor-only entry that, after the scene builds, explicitly force-finishes the editor load
(`CuiLoadingManager::setFullscreenLoadingEnabled(false)` + the `_onFinishedLoading` bookkeeping) guarded
to the advertised/editor client, leaving SWGEmu untouched. Pragmatic; sidesteps the ghost entirely but
must replicate whatever `_onFinishedLoading`/scene-ready does that later code depends on.

**Three things to confirm:**
1. Is the root cause read correct — `isFinishedLoading()`'s `hasPlayerObject` gate is THE blocker for the
   offline editor load (others — terrain/worldSnapshot/loader — do complete offline)?
2. Which direction do you want (A relax-the-gate, B mint-a-ghost, C editor-completion entry)?
3. For A/C: are there known post-load `getPlayerObject()` derefs that will immediately crash an offline
   editor scene, or do we guard those per-feature as they appear (the §4 pattern)?

---

## 3. Consumer state (no action needed there)

v6 `loadScene` consumer wiring is complete, built, and `[endpoints]` green (97/97), staged but
**uncommitted** (the 12-file batch) pending a green in-editor load smoke. The `hkMainLoop` per-target
change calls `swg::game::loadScene(terrain, avatar)` on the advertised client; SWGEmu keeps
`setupScene(GroundScene::ctor(...))`. Nothing to change consumer-side for this gap.

## 4. Pointers
- Stuck symptom: in-editor Scene panel → Load naboo → terrain loads + renders behind a load/cut screen
  that never dismisses. Engine loop healthy (thread-0 sampled in `RenderWorld::drawScene`→dpvs and in
  `CuiManager::update`→`SendHeartbeats` across 2 s — progressing, not hung).
- Engine: `GroundScene::isFinishedLoading` (1825), `updateLoading` (2049-2090), single-player ctor
  (929-963); `Game::getPlayerObject` (core/Game.cpp:1758); `#if 0`'d offline loaders (Game.cpp:849,
  926-953).
- Prior: `2026-06-25-utinni-scene-load-lifecycle-HANDBACK.md` (the v6 `loadScene` add).
