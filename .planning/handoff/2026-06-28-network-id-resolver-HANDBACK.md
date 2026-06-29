# Utinni A-3 HANDBACK — `network::getObjectById` (unblocks the target-change callback)

**Status:** DONE, build-gated, committed + pushed to `origin/master`. **Maintainer: re-sync v12 + smoke.**
**Date:** 2026-06-28
**Commit:** `901398013` (`feat(enginehook): Utinni A-3 v12 -- network::getObjectById`)
**Contract:** `ENGINE_HOOKPOINTS_VERSION` **11 → 12, 113 names** (+1 NAME ADD).

---

## Why

`creatureObject::setTarget` (mapped to the real entry of `CreatureObject::setLookAtTarget`) is
advertised + signature-matches (v9). But the consumer's `hkSetTarget` body resolves the new target's
`Object*` via `Network::getObjectById(id)` — a thin wrapper over a hardcoded SWGEmu RVA
(`idManagerGetObjectById` @ `0x00B380E0`) that was **not** advertised. On the advertised client that
call hit a stale RVA → crash (same class as the A-2.1 wrong-&). So target-change couldn't un-gate
until the id→Object resolver was advertised.

## Advertised (1) — constant `&fn`, no instance accessor needed

| Contract name | Real engine symbol | Mechanism |
|---|---|---|
| `network::getObjectById` | `NetworkIdManager::getObjectById(const NetworkId&)` static [NetworkIdManager.h:22 / .cpp:72] | constant `(void*)&fn` |

```cpp
static Object * NetworkIdManager::getObjectById(const NetworkId & source);   // [NetworkIdManager.h:22]
//   Consumer typedef: Object*(__cdecl*)(const NetworkId&)  -- exact match.
```

- **True static** → no `network::idManager` g_instance accessor needed (the singleton `ms_instance` is
  internal to the static; the public surface is already static). The world-pick `g_instance` pattern
  does **not** apply here.
- Group `network` matches the consumer's `Network::getObjectById` wrapper name.
- Out-of-line def (`.cpp:72`) → ODR address; `NetworkIdManager.h` is lightweight (only pulls
  `NetworkId.h`) → compiles in the exe TU; constant row → no `ensureDynamicRowsFilled` entry.

## Build gate (passed)

Release/Win32 (serial — gl05/gl07 postbuild `d3dcompiler_47.dll` copy races under `/m`), **0**
unresolved, undecorated `GetEngineHookPoints` export, X-macro count gate **113 == 113**, staged
`SwgClient_r.exe` (2026-06-28 ~20:56).

## v12 re-sync sha256 (LF working-copy bytes)

| file | sha256 (LF) |
|---|---|
| `engine_hookpoints.h`   | `61586631d0883f380004a370c3e8b7dd4aada31625338a01b956c060eb90699c` |
| `engine_hookpoints.inc` | `c68d55c72652e6fddcc7acd58994fe5355ba9809012f5f04530ab4e7981dcc83` |

## Maintainer smoke

Re-sync to v12, re-inject (`utinni_init` resolves **113/113**, version 12). Load into the world,
**select a target in-world** → the consumer's `hkSetTarget` fires, resolves the id via
`network::getObjectById`, and the `onTarget` callback gets a **non-null `Object*`** (no crash).

> This closes the last known blocker on the target-change editor callback. The remaining MISMATCH /
> detour rows still flagged for typedef-verify during smoke: `messageQueue::appendMessage[Data]`
> (INPUT-path). Same remedy if a wrong-signature detour appears (symbolize the `.mdmp`; OMIT if
> mismatched).

## Files touched (commit `901398013`)
- `engine_advertise.cpp` — `NetworkIdManager.h` include + the `network::getObjectById` row.
- `engine_hookpoints.h` — version 11→12 + note.
- `engine_hookpoints.inc` — +1 `ENGINE_HOOKPOINT(network, getObjectById)`.
