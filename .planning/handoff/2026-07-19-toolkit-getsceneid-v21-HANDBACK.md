# 2026-07-19 — v21 HANDBACK: game::getSceneId copy-out

**Status:** DONE 2026-07-19 evening, build-gated + 45s boot smoke, exe restaged.
**Contract v20 → v21, 142 → 143 names.**
**Request:** SWG-Toolkit `2026-07-19-CHANGE-REQUEST-advertised-getSceneId.md` (one-click
"Reload current scene" + .ws auto-naming; motivated by the `tatooine-2` unload-into-nothing
footgun).

## 1. The row

`game::getSceneId` → `extern "C" int __cdecl utinni_getSceneId(char* buf, int cap)`

- **Why a shim (not `&Game::getSceneId`):** the engine getter is INLINE (no ODR address) and
  returns `const std::string&` — both un-advertisable (ABI RULE / the v14 sysmsg lesson).
- **Convention:** exactly `wsGetSavePath` — returns needed length **INCLUDING the NUL**
  (size-first: call `(nullptr, 0)` to size, then with a buffer; a short buffer gets a
  truncated copy but the return is always the full needed length); **0 = no scene loaded**
  (`ms_sceneId` empty — charselect / pre-world).
- The returned id (e.g. `"tatooine"`) is the SAME string `WorldSnapshot::load()` /
  `wsSaveSnapshot()` key the .ws filename on — `wsUnloadSnapshot()` + `load(getSceneId())`
  is the correct one-click reload and picks up a just-saved override .ws.
- CALLED, game-thread-only, per-frame-safe.

Defined in `engine_advertise.cpp` next to the v20 shims (Game.h already in the TU).

## 2. Gates (all green, 2026-07-19 ~18:32 local)

- Release/Win32 `/t:SwgClient` forced relink: exit 0, **0 unresolved external symbol**;
  exe auto-staged 18:31:49.
- `GetEngineHookPoints` export intact (ordinal 82, undecorated).
- 143 == 143 count static_assert holds by compilation.
- 45s boot smoke on the staged exe: alive, no new dumps.
- x64 untouched by construction (`#if !defined(_WIN64)`).

## 3. Contract re-sync (maintainer)

```
5a3694f4dad3fdda38e5616f68ee0c244e2a5b6ca464d503d8fe9f8f236f5e47  engine_hookpoints.h
e38ecea556d4fecb462fdf6fd81f7318bef5f2e47d9f75ddc21e16cc67dbd0c1  engine_hookpoints.inc
```

Version-assert 21, count 143. Supersedes the v20 sync the same way v20 superseded v19
(append-only): one re-sync covers everything.

## 4. Smoke steps (consumer)

1. In-world: `getSceneId(nullptr, 0)` → e.g. 9 (`"tatooine"` + NUL); with a buffer →
   `"tatooine"`.
2. At charselect (no scene): → 0.
3. The reload button: save → `wsUnloadSnapshot()` → `load(getSceneId())` → placements table
   repopulates from the just-saved override .ws (generation bump observed).
