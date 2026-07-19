# 2026-07-19 — v20 HANDBACK: clientWorld::collideScreenRay + cuiRadialMenuManager::clear

**Status:** DONE 2026-07-19, build-gated + 50s boot smoke, exe restaged. **Contract v19 → v20, 140 → 142 names.**
**Request:** SWG-Toolkit `2026-07-19-CHANGE-REQUEST-advertised-ray-pick-accessor.md` (the Live World Editor
Slice-0 arc — cursor-on-ground placement + non-targetable pick). The pre-approved
`cuiRadialMenuManager::clear` rider (2026-07-18 positionchanged ANSWER: "rides the next version bump")
is INCLUDED in this bump.

## 1. The two rows

### 1a. `clientWorld::collideScreenRay` → `extern "C" int __cdecl utinni_collideScreenRay(int screenX, int screenY, int objectsOnly, __int64* outHitObjectId, float* outPoint3)`

Copy-out cursor ray-cast, engine-side (ABI RULE, the rider-4C camera-accessor shape): the consumer's
`collideCursorWithWorld` (Utinni `cui_hud.cpp:221`) semantics with **every NGE-unsafe piece kept in the
exe TU** — `CollisionInfo` (stack local, never crosses), `ClientWorld::collide` (which in THIS tree takes
an added `CollideParameters` param the consumer's 2002-era typedef doesn't model), and the camera
viewport math. Defined in `engine_advertise.cpp` next to the 4C camera accessors.

Semantics (mirrors the engine's OWN cursor pick — `SwgCuiHud.cpp` `findObjectByPolygon`:230 /
`hitReticle`:279 — not just Utinni's):

- **Ray:** `Game::getConstCamera()` position_w → through screen pixel `(screenX, screenY)` via
  `reverseProjectInScreenSpace` (**client-window pixels**; the engine subtracts the viewport origin
  internally — pass exactly what the WndProc/Present hook sees), normalized, length =
  `ConfigClientGame::getTargetingRange()` (the hud's own targeting viewDistance — NOT Utinni's raw
  `0x19488C8` static read).
- **Collide:** start cell = camera's parent cell (interiors correct), `CollideParameters::cms_default`,
  **player excluded** (the engine's `&self` — your own avatar never occludes the pick; null player at
  charselect degrades to no exclusion).
- **Flags:** the hud nine-flag pick set (`terrain | terrainFlora | tangible | tangibleNotTargetable |
  tangibleFlora | interiorObjects | interiorGeometry | skeletal | childObjects`).
  `objectsOnly=1` drops the three NON-OBJECT geometry classes (`terrain`, `terrainFlora`,
  `interiorGeometry`) — the ray then reports client objects only (and passes through terrain, so an
  object behind a rise is reachable; that is the documented meaning of the flag, use 0 for placement).
- **Returns:** `1` = hit → `outPoint3` = world x,y,z of the nearest hit; `0` = miss / no camera / null
  out-params (outs zeroed on every 0 path).
- **`outHitObjectId`:** the hit object's NetworkId value, with two deliberate wrinkles:
  - A **terrain hit returns 1 with id 0** — that is a VALID result (the place-at-cursor case); do not
    treat id==0 as failure when the return is 1.
  - A non-networked child part (e.g. a POB door piece under `CF_childObjects`) **walks up
    `Object::getParent()` to the nearest NETWORKED ancestor** so the id feeds straight into the
    existing `network::getObjectById` row; if no ancestor has a valid id, id stays 0 (point still valid).
- **Threading:** CALLED, game-thread-only, per-frame-safe (no allocation). Your Present-hook drain IS
  the game thread — calling it from the click handler inside `hkPresent` is correct. Do NOT call from
  the poll thread.

### 1b. `cuiRadialMenuManager::clear` → `(void *)&CuiRadialMenuManager::clear`

Plain constant `&fn` — public static `void clear()` [CuiRadialMenuManager.h:47], the `update`-row
sibling. Exactly as pre-approved in the 2026-07-18 positionchanged ANSWER.

## 2. Files changed (all exe-side; x64 untouched by construction — `#if !defined(_WIN64)`)

- `src/game/client/application/SwgClient/src/win32/engine_advertise.cpp` — shim + 2 table rows + 5
  includes (`ClientWorld.h`, `ConfigClientGame.h`, `CollideParameters.h`, `CollisionInfo.h`,
  `TerrainObject.h`).
- `src/game/client/application/SwgClient/src/shared/engine_hookpoints.h` — version 19 → **20** +
  changelog paragraph.
- `src/game/client/application/SwgClient/src/shared/engine_hookpoints.inc` — 2 `ENGINE_HOOKPOINT`
  rows (`clientWorld, collideScreenRay` / `cuiRadialMenuManager, clear`).

No plugin/shared-header ABI cascade: `engine_hookpoints.{h,inc}` are exe-internal contract files; the
gl0X plugins do not consume them.

## 3. Gates (all green, 2026-07-19 ~16:15 local)

- Release/Win32 `/t:SwgClient`, `/nodeReuse:false`, forced relink: exit 0, **0 `unresolved external
  symbol`**, 0 errors; exe auto-staged to `stage/SwgClient_r.exe` (16:14:57).
- `GetEngineHookPoints` export intact: ordinal 82, undecorated (`dumpbin /exports`).
- Count static_assert (table rows == .inc set) holds by compilation; 142 == 142.
- 50s boot smoke on the staged exe: no crash, no new dumps, boot money lines clean (incl.
  `Direct3d9_ShaderCache: preload complete (71 cached shaders in RAM)`).
  (A pre-existing `c0000005` dump at 16:00:41 local PREDATES this build — space-session breadcrumbs on
  the OLD exe; separate incident, not this change.)

## 4. Contract re-sync (maintainer)

Byte-identical re-sync of the v20 contract files, then rebuild the consumer bind on **v20 / 142**:

```
8a7deb647e6582a07ee46bd44e8442a9a78d9b937ea04f92913d03c41724b0f0  engine_hookpoints.h
a561d0cdc5c243b8d4148c1c16522cee21a40e36d5242f1b7c2e8519512b9321  engine_hookpoints.inc
```

Note the toolkit agent's `rva_table.cpp` was flagged (their pivot handoff) as pinned STALE ~v6 — the
v20 re-sync supersedes; version-assert against 20, count 142.

## 5. Smoke steps (consumer side)

1. Bind `clientWorld::collideScreenRay` + click in-world on open terrain → expect `1`, id 0, point ≈
   the ground under the cursor (feed to `wsAddObject` transform12 translation).
2. Click on a targetable NPC/object → expect `1`, id != 0, `network::getObjectById(id)` non-null,
   matches the object under the cursor.
3. Click inside a POB on a wall (objectsOnly=0) → expect `1`; on a door/child part the id should
   resolve to a networked ancestor.
4. Click at the sky (no geometry within targeting range) → expect `0`, outs zeroed.
5. `cuiRadialMenuManager::clear()` on editor teardown → no crash, radial state reset.

## 6. Still open (unchanged by this bump)

The v19 consumer bind+smoke item is now **superseded by v20** (one re-sync covers both — v19's rows
are unchanged; v20 is append-only + version bump). The §6 addendum + positionchanged ANSWER remain
required reading consumer-side. Kenny: push after review.
