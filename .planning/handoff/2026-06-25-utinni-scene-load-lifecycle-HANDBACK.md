# Provider Handback — editor scene-load: confirmed + `game::loadScene` added (Option A)

**From:** swg-client-v2 (provider) · **To:** Utinni (consumer) · **Date:** 2026-06-25
**Re:** `2026-06-25-utinni-scene-load-lifecycle-consult.md`. DONE + build-gated.
**Contract:** `ENGINE_HOOKPOINTS_VERSION` **5 → 6** (1 NAME ADD). Not committed yet. Did NOT touch Utinni.

**Re-sync** (byte-identical into `D:/Code/Utinni/UtinniCore/swg/`, sha256-verify):
```
engine_hookpoints.h    sha256 b869747687dc1bef8490129eec30c3b1ca9a4b256b4f597f9b92ec6ae679afc0
engine_hookpoints.inc  sha256 12143e1f1459aef663028edb8b46030a343ea9474bfd1576b07d1801cca36544
```

---

## 1. Lifecycle read — CONFIRMED correct

`Game::_setScene(Scene*)` [Game.cpp:1386] only does `ms_scene = newScene` (+ a little player-path
bookkeeping). It **skips the entire SceneCreator lifecycle**. The real load is (verified in source):
```
Game::setScene(immediately, terrain, player, customized)   [Game.cpp:1290]
  -> new SinglePlayerSceneCreator(terrain, player, customized)
  -> Game::_setScene(SceneCreator&, immediately)            [Game.cpp:1320]  // ms_nextScene, cutscene check,
       |                                                                     // CuiLoadingManager, deferred-creation
       -> Game::_startScene()                               [Game.cpp:1371]
            -> sc->create()  == new GroundScene(...)         [Game.cpp:221]   // build
            -> Game::_setScene(Scene*)                       [Game.cpp:1386]  // activate
            -> sc->endDeferredCreation(); delete sc                          // integrate / handoff
```
So handing the engine a pre-built `GroundScene` and only `_setScene(it)` runs **just the last `ms_scene =`
step** — it skips the `_setScene(SceneCreator&)` preamble (loading-manager + deferred-creation handshake,
`ms_nextScene` management) and the `endDeferredCreation()` after. The scene is left half-integrated →
next-frame throw. Your read is exactly right: **the SceneCreator/string path is required; `_setScene(Scene*)`
is insufficient, and a pre-built `GroundScene` is the wrong shape on this client.**

Note: `SinglePlayerSceneCreator::create()` is literally `new GroundScene(terrain, player, customized)` — the
**same ctor** your `groundScene::ctor` calls. So the build was never the problem; the *surrounding lifecycle*
is. Don't build a `GroundScene` on the advertised client — let the engine do it.

## 2. Entry — Option A (your preferred), shipped

Advertised a new row **`game::loadScene`**:
```cpp
static void __cdecl utinni_gameLoadScene(const char * terrainFilename, const char * playerFilename)
{
    Game::setScene(true, terrainFilename, playerFilename, nullptr);
}
// row: { "game::loadScene", (void *)&utinni_gameLoadScene }
```
- **Consumer:** bind `game::loadScene → &utinni_gameLoadScene`, call it `(terrainFilename, avatarFilename)`
  on the advertised client's "Load scene" — **stop building a GroundScene** for this path. ABI:
  `void(__cdecl*)(const char* terrain, const char* player)`.
- `game::setupScene` is **left as-is** (the `_setScene(Scene*)` thunk) — it's the valid low-level
  "set-pre-built-scene-active" primitive; just don't use it for a full load. Not reverted, not removed.

## 3. `immediately` value + `customizedPlayer` — answered from the engine's own loaders

- **`immediately = true`.** `!immediately` makes `_setScene(SceneCreator&)` look up the terrain's **intro
  cutscene** and defer the load behind it + a fullscreen loading screen [Game.cpp:1338-1361]; `true` skips
  that → `_startScene()` runs the load now. This is exactly what every in-engine "load scene by name" UI
  uses: **`SwgCuiSceneSelection.cpp:264`, `SwgCuiLocations.cpp:179`, `SwgCuiCommandParserScene.cpp:288` all
  call `setScene(true, …)`.** Right choice for an editor "load this now".
- **`customizedPlayer = nullptr` — correct.** When null, the `GroundScene` ctor loads the avatar
  **synchronously from `playerFilename`** (disables the async loader, `ObjectTemplate::createObject(playerFilename)`)
  — the normal single-player offline path [GroundScene.cpp:930-945]. `SwgCuiLocations.cpp:179` and
  `SwgCuiCommandParserScene.cpp:288` both pass `0` here. ⚠️ **`playerFilename` must be a loadable object
  template** (a player creature `.iff`) or the ctor `FATAL`s [GroundScene.cpp:942] — pass a real avatar
  template, not an empty/placeholder string.

## 4. Gate + scope

- **Release/Win32**, forced relink. Exit 0; 0 unresolved; 0 errors; undecorated `GetEngineHookPoints`
  present; restaged with matching PDB (2026-06-25 14:59). `static_assert` table-count == .inc count (now
  **99**) holds; no dup names; the 29 static-init-race placeholders are unchanged (the new row is a constant
  `&fn`, independent of that fix).
- Changed: `engine_advertise.cpp` (+thunk +row), `engine_hookpoints.inc` (+`game::loadScene`),
  `engine_hookpoints.h` (v5→6). **Re-sync the `.h`/`.inc`** (sha256 above).
- **Maintainer live smoke:** in-world → TJT Scene panel → pick terrain → **Load** → scene loads and is
  renderable, **no Fatal/throw ~1s later**. (Carries the static-init-race fix + setupScene re-map too.)
