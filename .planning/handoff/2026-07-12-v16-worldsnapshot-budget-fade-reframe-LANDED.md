# 2026-07-12 — Utinni v16 shim · WorldSnapshot create-drain budget · scene-change fade reframe — all landed + pushed

**READ FIRST after restart.** One Sunday session, three landings, all
verified + pushed. `origin/master` = `54fcd54d4`:

- `75107abfd` feat(utinni): v16 `game::getPlayerLookAtTargetId` (121 names)
- `2869b3838` perf(worldsnapshot): create/delete drain TIME budget
- `54fcd54d4` perf(scene-change): non-blocking music fade handoff

**Bottom line for the perf arc:** the zone-in profile's two biggest numbers
are gone. The ~1.4s "crossfade mega-stall" (a blocking title-music fade
pump, not real work) → ~510-690ms of genuine one-time GroundScene-ctor
work; the 300ms WorldSnapshot object-creation burst → spread under a 6ms/
frame budget. Loading screen appears ~1s sooner. Kenny ear+feel verified
both renderers ("feels really smooth" / "Both sounded good").

---

## 1. Utinni v16 — lookAt-target id read shim (`75107abfd`)

The 2026-07-09 consumer request, delivered exactly as specced:
`game::getPlayerLookAtTargetId` → `extern "C" __int64 __cdecl
utinni_getPlayerLookAtTargetId(void)`, the READ twin of
`creatureObject::setTarget` (same `m_lookAtTarget` slot, NOT
`getIntendedTarget`). Defined in **CreatureObject.cpp** (exe TU cannot
include CreatureObject.h — the Bucket A precedent), declared in
`engine_creatureObject_forward.h`, constant `&fn` row.
`ENGINE_HOOKPOINTS_VERSION` **15→16, 121 names**.

Full handback with v16 sha256s + consumer §2 checklist:
[2026-07-12-utinni-lookattarget-accessor-HANDBACK.md](2026-07-12-utinni-lookattarget-accessor-HANDBACK.md).
**Kenny is running the consumer-side rebind in D:/Code/Utinni himself.**

## 2. WorldSnapshot create-drain time budget (`2869b3838`)

`WorldSnapshot::update`'s create/delete drain was count-capped only
(1000/frame — effectively unlimited; one building create = templates +
appearances + CollisionBuckets::build + portal/pathfinding off cold disk).
Sampler-convicted 300ms one-frame burst.

- New `[ClientGame/WorldSnapshot] createTimeBudgetMs` (**default 6, 0 = old
  behavior**). One shared timer spans both loops; checked top-of-iteration
  (the over-budget item completes — never a torn create); deletes always
  make ≥1 item of progress. Un-drained nodes re-enter next frame's diff
  nearest-first — `update()`'s early-out already yields to non-empty pending
  lists, so zero new bookkeeping. The unbudgeted same-cell loop is untouched
  (visible-cell interiors populate atomically, by design).
- Bounds the ACCUMULATION, not the worst single item (check runs between
  items — one cold building can still cost ~100-150ms).
- **Regression signature:** object pop-in at world entry / fast travel.
- Verified: zero `WorldSnapshot::update` frames in post-fix profiles.

## 3. Scene-change music-fade reframe (`54fcd54d4`, 4 files, renderer-agnostic)

**The "crossfade mega-stall" was never a visual crossfade.** SwgCuiManager's
scene-change listener pumped the title-music fade-out to COMPLETION with a
blocking 1s `Audio::alter`/`Sleep(5)` loop — needed when the load path
starved audio for seconds; obsolete after the CONSULT-59/60/68 budgeted-load
chain.

- **Part 1** (`SwgCuiManager.cpp`): pump skipped by default; the fade rides
  the main loop's per-frame `Audio::alter` through the loading screen.
  Clock's `minFrameRate` clamp (cfg 10 → 100ms max step) bounds fade steps
  (the VOLSTEP 2026-07-04 anti-snap lesson). Kill switch:
  `[ClientUserInterface] blockingSceneChangeMusicFade=true`.
- **Part 2** (the pump was ALSO a sequencer — Kenny's first listen caught
  the new track starting under the still-fading title music): new
  `CuiManager::isMusicPlaying()` (true through the `stopMusic` fade window)
  and `GameMusicManager::update` now DEFERS its track selection/start while
  the UI music is audible. State-gated, self-healing (Audio hard-stops the
  sound at fade end), deferred frames consume no state (first-played lists
  append and `ms_event` clears inside the gated block), inert in normal play.
- **Regression signatures:** zone music never starts (gate stuck → check
  `isMusicPlaying`/title stream); audible overlap returns (gate bypassed);
  fade audibly snaps (minFrameRate cfg raised past ~10).
- Verified by ear both renderers + sampler: mega-frame 1416/1365ms →
  510-690ms, all real work (cold texture loads, HUD creation,
  GameMusicManager install — one-time costs under the entry fade).

## 4. BUILD/STAGE/COMMIT STATE

- `master` == `origin/master` == `54fcd54d4` (fetched before each push).
- Staged this session: x64 + Win32 `SwgClient_r.exe`, Release, link-gated
  (0 unresolved, grepped per the /FORCE trap). CuiManager.h is a
  clientUserInterface header — exe-side libs only, **no gl0X plugin
  cascade**. Debug configs remain stale (canonical 5-target still due at
  next close-out, carried since 07-08).
- No cfg edits — all three features default ON in code; both platforms'
  cfgs stay in parity.
- x64 cfg has an **exit timer armed** (`Game.cpp` ms_exitTimer) — telemetry
  smokes self-terminate ~60s after launch; Win32 does not (kill manually).
- Untracked CONSULT-56/57/66 `.out` files still parked in
  `.planning/research/` (commit at a close-out).

## 5. WHERE TO RESUME — arc backlog after this session

1. **Texture pre-warm** — cold texture loads are now the biggest single
   items left in the mega frame AND the ~110-155ms singles class.
2. **gl05 bytecode-cache sibling** (Phase-32 port) — gl05 shows more cold
   singles than gl11 (no shader-cache preload there).
3. **preloadSomeAssets single-item overshoot** — AsynchronousLoader routing
   (consciously deferred; ~150ms once per zone-in).
4. **Driver-threading soak call** (Kenny) → flip ConfigDirect3d11 default.
5. **Probe/diag strip pass** after soaks (PortalCullProbe is chatty in the
   report log). Stack sampler stays — standing tool.
6. Carried: real-door trigger brittleness, ilm-extract audit,
   `_ITERATOR_DEBUG_LEVEL`, standalone TRE editor thread, Utinni v16
   consumer rebind (Kenny's side).
