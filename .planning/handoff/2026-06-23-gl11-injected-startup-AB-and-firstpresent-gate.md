# gl11 injected-startup death — A/B revert + boot-trace + first-present gate (answer to the key question)

**Date:** 2026-06-23
**Status:** HANDBACK — A/B builds + observability staged; awaiting maintainer injected smoke.
**Audience:** maintainer (live inject owner) + Utinni dev.
**Source of truth:** this repo. No write to `D:/Code/Utinni`. No contract change.

## Answer to the key question

**Yes — §1's `displayModeChanged` runs on the initial/boot display-mode set, before the first present.**
The hook is wired during graphics init: `Graphics::install` calls
`Os::setDisplayModeChangedHookFunction(GraphicsNamespace::displayModeChanged)` at **Graphics.cpp:313**,
*inside* `SetupClientGraphics::install` (ClientMain.cpp:334) — which runs **before** `Game::run`
(ClientMain.cpp:387, where the first frame/present happens). So any `WM_SIZE` after :313 and before the
first present (the window show, an engine `SetWindowPos`, or — under injection — Utinni's
reparent/`SetWindowPos`) reaches the hook pre-present. §1 turned that into a synchronous
`Graphics::resize → device-lost/restored fan-out` before render state existed → the injected client
died in early startup (standalone never gets an early resize; intermittent = race between the WM_SIZE
timing and init progress; no dump = the crash bypasses the process exception filter under injection,
see below).

**This is exactly your proposed fix, and it's implemented (commit `feffb9ce2`):** gate ALL gl11 resize
work to **no-op until after the first successful present, independent of who triggers it.**
- `Graphics::displayModeChanged(gl11)` records the latest pending size and returns — no synchronous
  resize, regardless of trigger.
- `Graphics::beginScene` applies the pending resize only once `ms_firstPresentDone` (set by
  `Graphics::present`), at a frame boundary; `beginScene` runs only from the game loop, never mid-init.
- `Direct3d11 resize_impl` also guards on `Direct3d11_Device::isInstalled()`.

So a pre-first-present `WM_SIZE` from ANY source (boot window-show OR embed reparent) is now inert until
the first frame is up. Note this is **broader than the embed** — it also covers the plain boot
window-show `WM_SIZE`, which is the trigger that survives your Utinni-side reparent deferral.

## Why there was no dump (and the new artifact)

`writeMiniDumps` is **already true** (ClientMain.cpp:184) and the SEH filter installs **before** graphics
(`ClientMain.cpp:186` `SetupSharedFoundation::install → SetUnhandledExceptionFilter`, vs graphics at
:334). So a crash that REACHES the process filter writes a `.mdmp`/`.txt`/`.log`. "No dump" ⇒ the
injected crash **bypasses** that filter — most likely Utinni's injection replaces the process
`UnhandledExceptionFilter`, or it's a fast-fail / stack fault, or it's on the install chain (:334-386)
which isn't inside the `Game::run` `__try/__except` (ClientMain.cpp:387). 

**Added a crash-robust flushed boot-trace** (commit `feffb9ce2`, `ClientMain.cpp`):
`swgclient-boot-trace.log` in the cwd (the staging dir), one **immediately flushed** line per milestone:
```
00 ClientMain entry
01 pre SetupSharedFoundation::install
02 post SetupSharedFoundation (SEH filter + minidump installed)
03 pre SetupClientGraphics::install (gl11 device + displayModeChanged hook)
04 post SetupClientGraphics::install (gl11 device up, WM_SIZE hook now live)
05 pre Game::run (subsystems installed; entering frame loop -- first present happens here)
06 Game::run returned (clean exit / shutdown)
```
The **last line before death names the phase reached** — survives an SEH-bypassing crash. Bisection:
dies between 03 and 04 → gl11 device init; 04 but not 05 → post-graphics install chain; 05 but the
client never renders → in `Game::run` before/at first present. (The gl11 `displayModeChanged` one-shot
`WARNING` — "resize request … DEFERRED" — also lands in the engine log if a boot WM_SIZE fires.)

## A/B builds (deliverable §1 — the critical isolation)

Both are Release/Win32, **0 unresolved externals**, with the boot-trace:

- **`stage/`** = the **FIX** (deferral gate + boot-trace). The candidate.
- **`stage-revert-baseline/`** = the **pre-§1 REVERT** (§1's 4 files at `1d4c65028^`, i.e. NO resize
  reroute / NO deferral; **keeps** 38-07/08/09 and the boot-trace). The isolation baseline.

**A/B procedure (maintainer):**
1. Smoke `stage/` (the FIX) under injection. Expected: reaches first render + overlay installs; the
   boot-trace shows 00→06 and `Game::run` runs.
2. If you want the pure isolation A/B: back up `stage\SwgClient_r.exe` + `stage\gl11_r.dll`, copy
   `stage-revert-baseline\{SwgClient_r.exe,gl11_r.dll,*.pdb}` over them, and smoke. Expected: the
   pre-§1 client renders under injection (it did before §1) → **confirms §1 is the trigger**. Then
   restore the FIX binaries from your backup.
   - (PDBs are staged alongside each build so either dump symbolizes cleanly.)

## What to verify

- **FIX**: injected startup reaches first render (no early death); embed renders + crop fixed once the
  panel is sized; standalone gl11 + SWGEmu/D3D9 unchanged.
- **REVERT**: injected startup renders (isolation confirmation).
- If the **FIX still dies before render**: read `stage\swgclient-boot-trace.log` — the last milestone
  pinpoints the phase, and we go from there (the gate covers the resize path; a death elsewhere would
  mean a different §1-independent startup issue — the REVERT A/B tells us which).
- Confirm the runtime loads THESE builds (PE TS today), not a stale gl11.

## Build gate

- Release/Win32 both builds: 0 `unresolved external symbol`, Build succeeded; PDBs staged with each.
- Debug/Win32 of the fix built clean previously; the only new code since is the additive `ClientMain`
  boot-trace (plain C stdio) — low risk. (Rebuild Debug on request.)
- No shared-header/Gl_api ABI change → no plugin cascade.

## Commits

- `feffb9ce2` fix(dx11): defer gl11 resize until after first present + boot-trace observability.
- (§1 `1d4c65028` + the deferral are now both in HEAD; the revert baseline is built from `1d4c65028^`
  for the 4 §1 files only.)
