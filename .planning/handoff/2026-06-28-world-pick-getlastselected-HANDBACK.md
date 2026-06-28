# Utinni Bucket A-2 HANDBACK — world-pick / HUD-target (`cuiHud::getTarget` + `cuiHud::g_instance`)

**Status:** DONE, build-gated, committed + pushed to `origin/master`. **Awaiting maintainer inject-smoke.**
**Date:** 2026-06-28
**Commit:** `cc1933001` (`feat(enginehook): Utinni Bucket A-2 v10 -- world-pick / HUD-target`)
**Contract:** `ENGINE_HOOKPOINTS_VERSION` **9 → 10, 113 names** (+2 NAME ADDs).

Closes the one genuine §2.A gap the Bucket A handback OMIT'd: world-pick / HUD-target. Bucket A
OMIT'd `cuiHud::getTarget` as "absent" (no `SwgCuiHud::getTarget`) and only *offered*
`SwgCuiHud::getLastSelectedObject() const` as the replacement without advertising it. The world-pick
editor needs it — now advertised, with the live-instance accessor it must be called on.

---

## v10 re-sync sha256 (LF working-copy bytes — copy the working tree, not a CRLF checkout)

Re-copy these two byte-identical into `D:/Code/Utinni/UtinniCore/swg/` and sha256-verify:

| file | sha256 (LF) |
|---|---|
| `engine_hookpoints.h`   | `32ad9b67b4493489d016289323b255768857399ba505d3289614a724cdc255e7` |
| `engine_hookpoints.inc` | `e3e9313307cfc82e03403b69156e654bfe1723b9b80bbfdfa848653f794eec3a` |

(`engine_advertise.cpp` is provider-side only — not re-synced.) Both new rows are constant
(`&thunk` / `&fn`) → **no** `ensureDynamicRowsFilled` / `dyn[]` entry.

---

## ADVERTISED (2) — name → real engine symbol → mechanism

| Contract name | Real engine symbol (file:line) | Mechanism |
|---|---|---|
| `cuiHud::getTarget` | `SwgCuiHud::getLastSelectedObject() const` [SwgCuiHud.h:95 / .cpp:2211] *(MISMATCH name)* | `__fastcall` call-through thunk `utinni_hudGetLastSelectedObject` → constant `(void*)&thunk` |
| `cuiHud::g_instance` | `SwgCuiHudFactory::findMediatorForCurrentHud()` static [SwgCuiHudFactory.h:24 / .cpp:200] | constant `(void*)&fn` |

### Exact signatures
```cpp
// real method (read the world-picked object; m_lastSelectedObject is a Watcher<Object>)
Object * SwgCuiHud::getLastSelectedObject() const;            // [SwgCuiHud.h:95]

// provider thunk advertised as cuiHud::getTarget (defined in engine_advertise.cpp)
static Object * __fastcall utinni_hudGetLastSelectedObject(SwgCuiHud * pThis, int /*edx*/)
{ return pThis->getLastSelectedObject(); }
//   Consumer typedef: Object*(__thiscall*)(SwgCuiHud*)

// active-hud accessor advertised as cuiHud::g_instance
static SwgCuiHud * SwgCuiHudFactory::findMediatorForCurrentHud();   // [SwgCuiHudFactory.h:24]
//   Consumer typedef: SwgCuiHud*(__cdecl*)()
```

### Why this shape
- **`SwgCuiHud` is MULTIPLE-INHERITANCE** (`: public CuiMediator, public UIEventCallback`
  [SwgCuiHud.h:41-43]) → a raw `&SwgCuiHud::getLastSelectedObject` PMF is inflated and trips the
  `pmfToVoid` sizeof guard (C2338). This is a **CALLED row** (the consumer *reads* the pick by
  invoking it on the live hud), so the right shape is a `__fastcall(pThis /*ECX*/, int /*EDX*/)` ==
  `__thiscall` call-through thunk — exactly the `cuiChatWindow::writeToAllTabs` / `groundScene::*`
  CALLED-thunk pattern. (`pmfRealEntry` is only for *detoured* rows; getter reads are calls.) The
  consumer passes the live `SwgCuiHud*` (from `cuiHud::g_instance`) in ECX; `getLastSelectedObject`
  is `SwgCuiHud`'s own method so `this` is already the `SwgCuiHud` subobject — no adjustment.
  `getLastSelectedObject` is **public**, so the thunk names it directly (no friend grant).
- **`cuiHud::g_instance`** answers "how does the consumer reach the live hud?" `SwgCuiHudFactory::
  findMediatorForCurrentHud()` is a ready-made static returning the current `SwgCuiHud*` — it
  resolves whichever hud is active (concrete `HudGround` / `HudSpace`, both deriving from
  `SwgCuiHud`; registered `/GroundHUD` and `/HudSpace`). This mirrors the existing
  `cuiIo::g_instance → CuiManager::getIoWin` accessor pattern. **A clean accessor already existed —
  no new one was fabricated.** Both `SwgCuiHud.h` and `SwgCuiHudFactory.h` compile in the exe TU
  (lightweight; the `swgClientUserInterface/include/public` dir is already on the path via the
  `SwgCuiChatWindow.h` include), so both live entirely in `engine_advertise.cpp` — no new file, no
  shared-header change.

`cuiManager::findObjectUnderCursor` **stays OMIT** — there is no such symbol in this tree (retail
collapsed it + `getTarget` onto one world-pick RVA); the world-pick value is now delivered by the two
`cuiHud` rows above.

---

## Build gate (passed)

- **Release/Win32**, `$env:MSBUILD`, `/nodeReuse:false`, exe deleted to force relink.
- **0** `unresolved external symbol`, **0** LNK1120/LNK1181, **0** `error C…`.
- X-macro count gate (`UTINNI_REQUIRED_COUNT` static_assert): **113 == 113** (table rows == `.inc` names).
- Undecorated export confirmed: `GetEngineHookPoints = _GetEngineHookPoints`.
- Staged `SwgClient_r.exe` (28,456,960 bytes, 2026-06-28 ~18:26). Advertise TU is **32-bit only**.

> **Build note:** built **serially** (no `/m`). With `/m`, the gl05 + gl07 (`Direct3d9` /
> `Direct3d9_vsps`) postbuilds race copying `d3dcompiler_47.dll` into `stage/` → `MSB3073` aborts the
> build before `SwgClient` even compiles. Serial avoids the race. (Known: memory
> `project_32bit_largeaddressaware_debug_runnable`.)

---

## Maintainer inject-smoke

1. Inject into the staged `SwgClient_r.exe`; confirm `utinni_init` resolves the full table
   (**113/113**) and `GetEngineHookPoints` reports version **10**.
2. Load a world (so `findMediatorForCurrentHud()` returns a live hud).
3. The consumer calls `cuiHud::g_instance` → gets the live `SwgCuiHud*`.
4. **Select / click a world object**, then call `cuiHud::getTarget` on that instance →
   it returns the picked `Object*` (the last-selected object). Clear selection → returns null.
5. Space + ground both work (the accessor resolves `HudSpace` / `HudGround` respectively).

> Live inject-smoke is **maintainer-side**. Provider claim is scoped to: contract populated
> (113/113), built + staged + exported, each row maps to a real entry. **Do not** touch
> `D:/Code/Utinni`.

---

## Files touched (commit `cc1933001`)

- `src/game/client/application/SwgClient/src/win32/engine_advertise.cpp` — `SwgCuiHud`/`SwgCuiHudFactory` includes, the `utinni_hudGetLastSelectedObject` thunk, 2 rows, OMIT-ledger update.
- `src/game/client/application/SwgClient/src/shared/engine_hookpoints.h` — version 9→10 + note.
- `src/game/client/application/SwgClient/src/shared/engine_hookpoints.inc` — +2 `ENGINE_HOOKPOINT` rows.

(No new file, no engine-library change — both targets are reachable from the exe TU.)
