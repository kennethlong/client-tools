# Provider Handback — Player lookAt-target id accessor (v16, `game::getPlayerLookAtTargetId`)

**From:** swg-client-v2 (provider) · **To:** Utinni (consumer) · **Date:** 2026-07-12
**Request:** [2026-07-09-utinni-lookattarget-accessor.md](2026-07-09-utinni-lookattarget-accessor.md) (rev. 2)
**Status:** DONE, build-gated (Release/Win32, 0 unresolved), staged. Contract **v15 → v16, 121 names**.

---

## What was delivered (exactly the §1 row, no deviations)

| Field | Value |
|-------|-------|
| Contract name | `game::getPlayerLookAtTargetId` |
| Provider symbol | `extern "C" __int64 __cdecl utinni_getPlayerLookAtTargetId(void)` |
| Defined in | `clientGame/CreatureObject.cpp` (tail, inside the existing `#if !defined(_WIN64)` block, next to `engine_creatureSetTargetRealEntry`) |
| Declared in | `SwgClient/src/win32/engine_creatureObject_forward.h` (exe-local) |
| Body | `CreatureObject const * const player = Game::getPlayerCreature(); return player ? player->getLookAtTarget().getValue() : 0;` |
| Row mechanism | constant `&fn` (NOT a dyn[] row — plain static-linkage C function, image-valid at load) |
| Returns | the player's lookAt-target `NetworkId` **value**, full 64 bits (cluster-id bits included); `0` = no player / no target |

**Placement note (the one mechanical deviation from the request's "shim body" sketch — location
only, body identical):** the shim could not live in `engine_advertise.cpp` because the exe TU
cannot include `clientGame/CreatureObject.h` (it transitively pulls
`sharedSkillSystem/SkillObjectArchive.h`, not on the exe include path — the documented Bucket A
constraint that already forced `engine_creatureSetTargetRealEntry` into `CreatureObject.cpp`). Same
solution: defined in CreatureObject's own TU, exe sees only the extern "C" declaration. Zero ABI
consequence — extern "C" linkage, no mangling, `&fn` taken in the exe TU.

**Semantics as requested:** strictly the lookAt/selection target (`m_lookAtTarget` — the slot the
advertised `creatureObject::setTarget` row writes). NOT `getIntendedTarget`. The row comment and
the `.h` changelog both pin this so a future re-point is structurally discouraged.

## Contract mechanics (all done)

- `ENGINE_HOOKPOINTS_VERSION` **15 → 16** (`engine_hookpoints.h`, with the standard changelog entry).
- 1 NAME ADD in `engine_hookpoints.inc`: `ENGINE_HOOKPOINT(game, getPlayerLookAtTargetId)` → **121 names**.
- Table row added in `engine_advertise.cpp` (after the sysmsg v15 shim row, its ABI-rule sibling).
- Compile-time count static_assert (121 == 121) holds; runtime `engine_verifyNoNullNoDup()`
  name-set-equality check governs as always (constant non-null row — nothing dynamic to fill).

## Re-sync (maintainer action)

Copy the two contract files **byte-identical** into `D:/Code/Utinni/UtinniCore/swg/`:

| File | SHA256 (v16) |
|------|--------------|
| `engine_hookpoints.h` | `74A2A5A8191B106C5D3E558DE30E1FAFFAA1EFBE66EBF17720A4F78FFD49A909` |
| `engine_hookpoints.inc` | `828CC1B9C8D42A44F4B2E06AC57860DE7A6B6872A053852899708A58F74FC177` |

Consumer-side (§2 of the request, for reference): bind the null-starting slot
`swg::game::getPlayerLookAtTargetId` (`int64_t(__cdecl*)()`), reroute
`Game::getPlayerLookAtTargetObject()` through the **v12 `network::getObjectById`** row (NOT the
wave-1-nulled `getCachedObjectById` path), keep the id in an `int64_t` local (never `swgptr`),
bump the 120/118 → 121/119 static_asserts/REQUIREs. The §2.3 blast-radius guards are already
landed consumer-side (`750d213`) per the request.

## Build gate

- Release/Win32 `/t:SwgClient`, `/nodeReuse:false`, fresh link (no stale exe): **PASS (exit 0)**
- `unresolved external symbol` count: **0** (/FORCE masks them as warnings — grepped the log explicitly)
- `dumpbin /exports`: `GetEngineHookPoints = _GetEngineHookPoints` present (ordinal 82).
- Staged: `stage/SwgClient_r.exe` (postbuild auto-copy, timestamp-verified).
- x64 unaffected by construction: the shim is `#if !defined(_WIN64)` in CreatureObject.cpp,
  matching the whole advertise body; `engine_advertise.cpp` compiles to nothing on x64.
- No shared-header ABI cascade: `engine_hookpoints.{h,inc}` + the forward header are exe-local;
  the CreatureObject.cpp addition is a free function (no class layout change) — gl0X plugins untouched.
- Boot smoke (2026-07-12): staged `SwgClient_r.exe` auto-login smoke, alive at 75s (in world,
  ~359MB working set), killed clean. The advertisement is inert without injection, but the boot
  gate holds.

## Maintainer smoke (request §3)

On the advertised NGE client, after v16 rebind:
1. Target an NPC → consumer log shows the id AND a non-null `Object*` from the v12-row resolve.
2. Inspector target readout populates on target change.
3. Un-target → clean 0/null degrade.
4. Target something, walk out of range → id non-zero, resolve null, no crash (staleness case).
5. Snapshot panel still shows no-node (expected — Goal B unchanged).
6. No crash across scene changes with a target held.
7. SWGEmu D3D9 smoke unchanged (D-00 — no SWGEmu code path touched).
