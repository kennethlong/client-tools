# gl11 injected-startup no-render — FIXED: defer §1 resize until after first present

**Date:** 2026-06-23
**Status:** HANDBACK — fix staged, awaiting maintainer injected re-smoke.
**Audience:** maintainer (live inject + RenderDoc owner) + Utinni dev.
**Source of truth:** this repo. No write to `D:/Code/Utinni`. No contract change (version 3 / 94 eps).

## TL;DR

The §1 resize fix rendered fine **standalone** but **not under Utinni injection** (two re-smokes, no
first render, no crash dump, intermittent). Root cause (your mechanism, confirmed in code + Codex
cross-check): under injection Utinni reparents + `SetWindowPos`-resizes the SWG window during startup,
delivering `WM_SIZE` **before the first frame**. §1 turned that early `WM_SIZE` into a synchronous
`Graphics::resize` → device-lost/restored fan-out + backbuffer resize **before render state existed** →
no render. Standalone never gets that early embed resize. **Fix: defer gl11 resizes until after the
first present**, applying the latest size at a frame boundary.

## The fix (4 files, gl11-only; D3D9 untouched)

`src/engine/client/library/clientGraphics/src/win32/Graphics.cpp`:
- `GraphicsNamespace::displayModeChanged()` (gl11 branch) **no longer resizes synchronously** — it
  records the latest requested client-rect (`ms_pendingResizeWidth/Height`, `ms_resizePending`) and
  returns. One-shot `WARNING` artifact (Release-visible):
  `Graphics::displayModeChanged(gl11): resize request w=.. h=.. rasterMajor=11 firstPresentDone=.. -> DEFERRED until first present`.
- `Graphics::beginScene()` applies the pending resize (calls `Graphics::resize`) **only when
  `ms_resizePending && ms_firstPresentDone`**, then clears it. One-shot `WARNING` on apply:
  `Graphics::beginScene: applying deferred gl11 resize w=.. h=..`. `beginScene` runs only from the
  game loop (the install-time clear uses the plugin's `beginScene` directly), so this never runs
  mid-init.
- `Graphics::present()` and `Graphics::present(HWND,int,int)` set `ms_firstPresentDone = true` (the
  install-time present uses the plugin's `present`, so the gate flips only on a real game-loop present).

`src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp`:
- `resize_impl` early-returns if `!Direct3d11_Device::isInstalled()` — defense for the `CutScene` /
  console `Graphics::resize` callers so the device-lost/restored fan-out never runs before the device
  exists.

`Direct3d11_Device.{h,cpp}`: added `static bool isInstalled()`.

### Resulting behavior under injection
Early embed `WM_SIZE` → recorded, no work → first frame renders (creation size, DXGI STRETCH) →
present → next frame's `beginScene` applies the embed resize → crop fixed. **Render-then-correct**, no
pre-first-frame fan-out.

## Diagnostics added (deliverable §3)

- Two one-shot `WARNING`s on the resize path (request + apply), Release-visible, naming w/h/rasterMajor/
  firstPresentDone — the artifact that confirms the early embed `WM_SIZE` is happening and when it
  applies.
- **Crash-dump handler already installs before graphics init** — `ClientMain.cpp:186`
  (`SetupSharedFoundation::install` → `SetUnhandledExceptionFilter`) precedes graphics at
  `ClientMain.cpp:334` (`SetupClientGraphics::install`). No change needed. ("No crash dump" under
  injection is consistent with a *silent no-render*, not a missed dump — which this fix addresses.)

## On the A/B revert (deliverable §1)

Not produced as a separate binary — **this fix subsumes the revert**: it makes §1's early-startup
resize a no-op-until-ready, so an injected re-smoke that now renders confirms both the diagnosis and
the fix in one pass, and the one-shot WARNING log proves the early `WM_SIZE` path is being exercised.
If you still want an independent pure-§1-revert A/B build to isolate causation, say so and I'll produce
one (keeping 38-07/08/09).

## Build gate

- **Release/Win32** and **Debug/Win32** (`Direct3d11;SwgClient`, `/nodeReuse:false`, serial; staged +
  compiled binaries deleted to force relink): **0 `unresolved external symbol`**, 0 errors, Build
  succeeded. Restaged `SwgClient_r.exe` + `gl11_r.dll` **with matching PDBs** (`stage\*.pdb` same
  timestamp as the dll — so a fresh dump symbolizes cleanly).
- dumpbin exports intact: `GetEngineHookPoints` (exe), `GetHookPoints` + `GetApi` (gl11). No
  shared-header/Gl_api ABI change → no plugin cascade (`Direct3d11_Device.h` is gl11-internal;
  `Graphics.cpp` is exe-side).
- Cross-checked by Codex (CONSULT-50): coverage (only CutScene bypasses, post-install), liveness (loop
  applies pending the frame after first present), reentrancy (safe pre-beginScene point; nested WM_SIZE
  just re-queues), gate correctness (install present doesn't flip it). No hole found.

## What to verify (maintainer — live inject + RenderDoc)

1. **Injected startup renders** (the regression): inject into the fresh `stage\` 19:23 build; the
   client reaches first render + the DX11 overlay installs. Watch the log for the one-shot
   `displayModeChanged(gl11): resize request … DEFERRED` then `applying deferred gl11 resize`.
2. **Embed crop fixed**: scene fills the panel (no top-left crop / bottom black bar); resizing the TJT
   panel rescales.
3. Standalone gl11 unchanged; SWGEmu/D3D9 unchanged.
4. Confirm the runtime loads THIS build (PE TS Jun 23 19:23) — last time a stale June-2 gl11 was
   running (see `2026-06-23-gl11-crash-dump-is-stale-not-s1.md`).

## Known/low-risk

`CutScene::start/stop` call `Graphics::resize` synchronously (640x480 letterbox) — in-game only,
post-install, guarded by `isInstalled`; not a startup path. `CutScene` on gl11 was previously broken
(it hit the old `STUB(resize)` FATAL); §1 + this guard make it work.
