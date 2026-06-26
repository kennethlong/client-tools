# Editor scene-load arc — COMPLETE + live-verified (2026-06-25)

The advertised-DX11-client editor-unlock arc this session: **the editor can now load a scene into the
live `SwgClient_r.exe` and it renders, in-world.** loadScene wiring + Option A (loading-screen dismiss) +
Enter-mask all landed, pushed, and maintainer-smoke-verified. **Nothing pending on this arc.**

## What landed (all committed + pushed)

**Provider — `swg-client-v2` (origin/master, kennethlong/client-tools):**
- `6de8ed5` `feat(harness): editor offline scene-load completes -- single-player gate (Option A)` —
  `utinni_gameLoadScene` marks `Game::setSinglePlayer(true)` before `Game::setScene`; `isFinishedLoading()`
  accepts `getSinglePlayer()` in lieu of the (offline-absent) PlayerObject ghost. No-op for SWGEmu.
- `b01371b4` `docs(handoff): editor loadScene loading-screen-stuck consult (root cause + Option A)`.

**Consumer — `Utinni` (origin/master, kennethlong/Utinni):**
- `e99e27c` `feat(24): wire v6 game::loadScene + GAME-subsystem advertised endpoints` — contract v6/99,
  bound in lockstep (asserts 99/97), `[endpoints]` 97/97; hkMainLoop drives the full SceneCreator string-load
  on the advertised client (SWGEmu keeps `setupScene(GroundScene::ctor)`).
- `0e4f2b3` `feat(24): advertised-client embed clamp + offline Repository-walk guard`.
- `07f39d1` `docs(window): capture D3D11 advertised-client Enter/fullscreen-restyle repro`.
- `505d2da` `feat(24): advertised-client Enter-mask -- filter Enter from the DI keyboard buffer` —
  hooks buffered `GetDeviceData` (vtbl[10]; SWG uses buffered, not GetDeviceState), drops
  `DIK_RETURN`/`DIK_NUMPADENTER` on the keyboard device, advertised-client-gated.

## Smokes (maintainer, green)
- Editor Scene panel → Load naboo → **scene loads + renders, loading screen dismisses, no Fatal.**
- In-world **Enter no longer bounces the embed**; WASD/movement unaffected.

## Root causes (for context)
- **Loading-screen stuck:** offline editor load builds only a `CreatureObject`, never the server-driven
  PlayerObject ghost → `GroundScene::isFinishedLoading()` (`hasPlayerObject`, GroundScene.cpp:1837) never
  true → `setFullscreenLoadingEnabled(false)` (2089) never fires. Fixed by Option A (single-player gate).
- **Enter bounce:** SWG-native in-world Enter → chat-open `enableTextInput` → window-level fullscreen
  restyle (the known `swg-window-resize-fullscreen-edge-cases.md` cluster, now on D3D11). Suppressed the
  *trigger* via the DI Enter-mask (WM consumption is ineffective — SWG polls DI, not WM_KEYDOWN).

## OPEN follow-ups (NOT this arc — separate per-subsystem unlocks behind their own smokes)
- **Chat / MISC-INPUT subsystems** still wholesale-locked on the advertised client (`createDetours()`
  `!advertised` skips). Chat won't work until that unlock. In-world exit today = Scene panel **Unload**.
- **Window-management cluster** (the fullscreen-restyle root + RT-space mouse mapping) — Utinni
  `.planning/todos/pending/swg-window-resize-fullscreen-edge-cases.md` (updated with the D3D11 repro).
  The Enter-mask suppressed the trigger; the underlying restyle path is still that todo's domain.
- **Enter-mask revisit** when MISC/INPUT unlocks and in-world Enter regains a real use (flagged in-code).

## ⚠️ REPO-STATE CAVEAT for the next session — do NOT clobber
A **separate in-flight follow-up wave** is active in the **Utinni** working tree (NOT this arc):
- **2 unpushed Utinni commits:** `6ccb6c3` `docs(24): follow-up wave plan + Smoke-1 root-cause (WS-0/WS-1
  reverted)`, `499e16c` `docs(24): mark editor-unlock handoff superseded -> point to follow-ups plan`.
- **Uncommitted Utinni WIP:** `endpoints.{cpp,h}`, `endpoints_bindings.cpp`, `endpoints_tests.cpp`,
  `scene/world_snapshot.cpp`, `.planning/phases/24-.../24-FOLLOWUPS-PLAN.md`, + `Generated/UtinniCore.cs`
  (CppSharp churn — keep reverted, never commit).
- **Forward pointer for that wave = Utinni `24-FOLLOWUPS-PLAN.md`.** Leave its commits/WIP alone; this arc's
  work is already pushed and self-contained.

## Build/run pointers
- Utinni: `MSBuild Utinni.sln -t:UtinniCore;UtinniCore_Tests -p:Configuration=Release -p:Platform=x86
  -nodeReuse:false` (VS18 Community MSBuild); run `[endpoints]` tests; always
  `git checkout -- UtinniCoreDotNet/Generated/UtinniCore.cs` after (CppSharp churn).
- Provider: `$env:MSBUILD ...\swg.sln /t:SwgClient /p:Configuration=Release /p:Platform=Win32
  /nodeReuse:false`; delete the exe to force relink; grep log `unresolved external symbol` (must be 0);
  postbuild stages `SwgClient_r.exe` → `stage/`.
- Smoke = maintainer-only (Utinni `Launcher.exe` injects `SwgClient_r.exe`; `ut.ini
  swgClientPath=D:\Code\swg-client-v2\stage\`; read `bin/Release/utinni.log`).
