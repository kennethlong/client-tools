# Utinni A-2.1 CRASHFIX HANDBACK — revert `systemMessageManager::receiveMessage` (world-load crash)

**Status:** DONE, build-gated, committed + pushed to `origin/master`. **Maintainer: re-sync to v11 + re-smoke.**
**Date:** 2026-06-28
**Commit:** `1693f8099` (`fix(enginehook): Utinni A-2.1 v11 -- REVERT systemMessageManager::receiveMessage`)
**Contract:** `ENGINE_HOOKPOINTS_VERSION` **10 → 11, 112 names** (−1 NAME REMOVE).

---

## What happened

Maintainer smoke crashed loading into the world (`c0000005`), early (region-enter). Dump
`stage/SwgClient_r.exe-unknown.0-20260629011718.{txt,mdmp}` symbolized (cdb `.ecxr; kb`) to:

```
SwgClient_r!CuiSystemMessageManager::sendFakeSystemMessage+0x5e  [CuiSystemMessageManager.cpp:119]
UtinniCore!utinni::hkReceiveMessage+0x3e                          ← consumer detour
  → Archive::AutoByteStream::addVariable → AV at 012ea775 (executing 0x0000 = garbage)
SwgClient_r!ClientRegionManager::checkCurrentRegion+0x42b         [ClientRegionManager.cpp:340]
SwgClient_r!ClientRegionManager::update → CuiManager::update → Game::runGameLoopOnce → hkMainLoop
```

(The SWG crash-`.txt` breadcrumb trail ended at an async `armor_marauder…belt.mgn` load, which
*looked* like the known async-`.mgn` race — a red herring. The real fault is the detour, only
visible in the minidump stack.)

## Root cause — a wrong `&` (my Bucket A v9 mapping)

Bucket A advertised `systemMessageManager::receiveMessage` → the **static**
`CuiSystemMessageManager::receiveSystemMessage(const ChatSystemMessage&)`. But the consumer's
`hkReceiveMessage` is written for the **`MessageDispatch::Receiver::receiveMessage(const Emitter&,
const MessageBase&)`** byte-stream pattern — it deserializes a network message (`Archive::
AutoByteStream` on the stack). Pointing that 2-arg `__thiscall` network-receiver detour at the 1-arg
`__cdecl` static UI handler misreads the arguments → garbage execution → AV.

`sendFakeSystemMessage` calls `receiveSystemMessage` internally [CuiSystemMessageManager.cpp:119], so
a normal **region-enter** system message (`ClientRegionManager::checkCurrentRegion`) triggered the
detour during world-load → deterministic-ish crash.

The actual receiver in this tree is the **file-local anon-namespace `Listener::receiveMessage`**
[CuiSystemMessageManager.cpp:51] (connects to `ChatSystemMessage`, deserializes, forwards to the
static) — it has **no external symbol** → un-advertisable.

## The fix — OMIT

Removed the row (a wrong `&` is worse than a missing row). Removing it restores graceful
degradation: the consumer can't resolve `receiveMessage` → its `hkReceiveMessage` detour doesn't
install → no crash. Now in the Bucket A OMIT/SKIP ledger (`engine_advertise.cpp`) as
`OMIT wrong-&`, with the consumer alternatives. Unused `CuiSystemMessageManager.h` include removed.

**Consumer alternatives** (maintainer's choice, none provider-blocking):
- **Observe:** vtable-resolve the `ChatSystemMessage` `Listener` receiver (the §2.C consumer-side
  pattern), since the engine receiver is file-local.
- **Inject:** the static `CuiSystemMessageManager::sendFakeSystemMessage(const Unicode::String&,
  bool)` IS cleanly advertisable as a CALLED row (`systemMessageManager::sendFakeSystemMessage`) —
  offered on request; it's an inject entry, not the observe-hook the consumer asked for.

## Untouched / still good

All other Bucket A / A-2 rows stand. In the *same* crash dump, `cuiHud::g_instance` →
`SwgCuiHudFactory::findMediatorForCurrentHud` was seen executing in the **normal** engine HUD-build
path (`SwgCuiHudFactory.cpp:176` → `CuiMediatorFactory_Constructor`) — working. The B-2 replay smoke
also logged `[utinni.replay] … played=yes` earlier in the same session.

> ⚠️ The other MISMATCH-name detour rows remain flagged for typedef-verify in their handbacks:
> `creatureObject::setTarget` → `setLookAtTarget`, and `messageQueue::appendMessage[Data]`. If any of
> those also fire a wrong-signature detour during smoke, same remedy (verify the consumer typedef vs
> the mapped symbol; OMIT if mismatched). The `systemMessageManager` case is the lesson:
> **a consumer `receiveMessage` hook means the `MessageDispatch::Receiver::receiveMessage(emitter,
> message)` virtual, not a static handler.**

## v11 re-sync sha256 (LF working-copy bytes)

| file | sha256 (LF) |
|---|---|
| `engine_hookpoints.h`   | `3de90c8970770f4b6f4dea2ec21567961ca684e0be7fc06d3127a20d2fbe062a` |
| `engine_hookpoints.inc` | `512335b029668e2e0127959909229fb690584cff1ef739ea443651c9bb10681b` |

## Build gate (passed)

Release/Win32 (serial — gl05/gl07 postbuild `d3dcompiler_47.dll` copy races under `/m`), **0**
unresolved, undecorated `GetEngineHookPoints` export, X-macro count gate **112 == 112**, staged
`SwgClient_r.exe` (2026-06-28 ~20:24).

## Maintainer smoke
Re-sync to v11, re-inject, load into the world (walk through a region boundary so
`sendFakeSystemMessage` fires) → **no `c0000005`**. `utinni_init` resolves **112/112**, version 11.

## Files touched (commit `1693f8099`)
- `engine_advertise.cpp` — row removed, OMIT-ledger note, include removed.
- `engine_hookpoints.h` — version 10→11 + note.
- `engine_hookpoints.inc` — name removed (−1).
