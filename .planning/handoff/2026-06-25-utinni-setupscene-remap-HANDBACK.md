# Provider Handback — game::setupScene re-mapped to the pre-built-scene setter

**From:** swg-client-v2 (provider) · **To:** Utinni (consumer) · **Date:** 2026-06-25
**Re:** `2026-06-25-utinni-setupscene-remap.md`. DONE + build-gated. **No contract change** (same-name
address re-map → no re-sync, no version bump). Not committed yet. Did NOT touch `D:/Code/Utinni`.

---

## TL;DR

`game::setupScene` now points at a `__cdecl(void*)` thunk over **`Game::_setScene(Scene*)`** (the
pre-built-scene setter), replacing the strings-overload `Game::setScene(bool,terrain,player,customized)`
that was crashing the editor's "Load scene". **Consumer side: zero change** — your
`setupScene(GroundScene::ctor(...))` call and `hkSetScene` detour both already use the `(GroundScene*)`
shape. Rebuild + re-stage only.

## The fix

```cpp
static void __cdecl utinni_gameSetupScene(void * groundScene)
{
    Game::_setScene(static_cast<Scene *>(reinterpret_cast<GroundScene *>(groundScene)));
}
// row: { "game::setupScene", (void *)&utinni_gameSetupScene }   // was static_cast<...>(&Game::setScene)
```

- **Why it was crashing:** the old row advertised the *strings-builder* overload. Your editor passes a
  GroundScene* (build-then-set), which the builder read as its `bool immediately` + garbage filename args →
  built a scene with a garbage `player` → `0xC0000005 WRITE` in `GroundScene::GroundScene`
  (`player->setPosition_p`, GroundScene.cpp:948). Exactly your diagnosis.
- **Target:** `Game::_setScene(Scene*)` [Game.h:126 / Game.cpp:1386, `ms_scene = newScene`] — the modern
  "set this pre-built scene active" entry, equivalent to SWGEmu `setupScene(GroundScene*)`.
- **MI cast:** GroundScene is multiple-inheritance (`NetworkScene : public Scene, public
  MessageDispatch::Receiver`), so the `Scene` base sits at a **non-zero offset**. The `static_cast<Scene*>`
  applies the correct this-adjustment; a raw reinterpret would pass the wrong subobject. ABI:
  `void(__cdecl*)(void*)` ≡ your `swg::game::setupScene` typedef `void(__cdecl*)(GroundScene*)`.

## ⚠️ One correction vs the request

The request assumed `Game::_setScene` is **private** ("a friend/in-TU forwarder is needed exactly like
utinni_groundScene*"). **In this tree it is PUBLIC** (Game.h:126, under the `public:` at Game.h:92) — so the
thunk calls `Game::_setScene` **directly** from `engine_advertise.cpp`, no friend declaration and no in-TU
forwarder. Simpler than the SWGEmu-layout-based suggestion; same result. (Added an explicit
`#include "clientGame/Scene.h"` so `Scene` is a complete type for the upcast.)

## Contract / scope

- **Same name `game::setupScene` → pure address re-map.** `engine_hookpoints.{h,inc}` byte-identical, version
  unchanged (v5/98), POD unchanged → **no re-sync**. Verified only `engine_advertise.cpp` changed.
- The re-mapped row is a compile-time constant (`(void*)&utinni_gameSetupScene`), so it's **independent of
  the 2026-06-25 static-init race fix** — the 29 placeholder rows + lazy fill are unchanged (verified intact).
  (You need that race-fix build too: both land in the same staged exe.)

## Gate + acceptance

- **Release/Win32**, forced relink. Exit 0; 0 unresolved; 0 errors; the MI `static_cast` upcast compiled
  (valid, unambiguous base); undecorated `GetEngineHookPoints` present; restaged with matching PDB
  (2026-06-25 10:16).
- **Maintainer live smoke:** in-world → TJT Scene panel → pick terrain → **Load** → scene loads (or honestly
  degrades) with **no `0xC0000005` in `GroundScene::GroundScene`**. (`cleanupScene` already worked; this is
  the `setupScene` leg.)
