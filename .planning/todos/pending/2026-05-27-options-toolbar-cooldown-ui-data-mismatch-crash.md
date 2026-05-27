---
created: 2026-05-27
title: Options window FATAL — checkShowToolbarCommandCooldownTimer CodeData property missing from GroundHUD options .ui
area: client UI / CuiMediator CodeData binding / GroundHUD options layout
next_action: add the checkShowToolbarCommandCooldownTimer TUICheckbox to the GroundHUD options .ui layout (or guard the lookup) so opening Options does not FATAL
files:
  - src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiOptUi.cpp  (line 219 — getCodeDataObject(TUICheckbox, checkbox, "checkShowToolbarCommandCooldownTimer"))
  - src/engine/client/library/clientUserInterface/src/shared/core/CuiMediator.cpp  (line 1487 — FATAL on missing CodeData property)
  - GroundHUD options .ui layout — the [/GroundHUD.OptMain.comp.target.ui] component (loaded at runtime from ui_*.tre / the ui data tree; needs the checkShowToolbarCommandCooldownTimer checkbox widget)
references:
  - commit d1b3c0eaf  "Add Toolbar Cooldown Timer Display Support"  (introduced the SwgCuiOptUi.cpp:219 code reference; the matching .ui widget was never added to the loaded layout)
  - .planning/phases/16-v2-1-tech-debt-cleanup/16-03-SUMMARY.md  (discovered + ruled out as a phase-16 regression during the D-08 boot smoke)
  - stage/SwgClient_d.exe-unknown.0-20260527171329.txt  (crash dump with full call stack)
status: pre_existing_defect
priority: medium (FATALs the client whenever the Options window is opened in-world under this build)
---

## What this is

Opening the **Options window** in-world FATALs the client:

```
CuiMediator.cpp(1487) : FATAL 3a8ab9d0: Unable to find CodeData property
  'checkShowToolbarCommandCooldownTimer' from [/GroundHUD.OptMain.comp.target.ui] for [SwgCuiOptUi]
```

`SwgCuiOptUi.cpp:219` does:

```cpp
getCodeDataObject(TUICheckbox, checkbox, "checkShowToolbarCommandCooldownTimer");
```

i.e. the C++ Options UI **requires** a checkbox named `checkShowToolbarCommandCooldownTimer`
in the GroundHUD options `.ui` layout (`/GroundHUD.OptMain.comp.target.ui`). The loaded
`.ui` data does **not** define that widget, so the CodeData lookup hits the unconditional
FATAL in `CuiMediator`.

## Root cause

A **code↔UI-data mismatch**. Feature commit `d1b3c0eaf "Add Toolbar Cooldown Timer Display
Support"` added the `SwgCuiOptUi.cpp:219` lookup but the corresponding checkbox widget was
never added to the GroundHUD options `.ui` layout that ships in the loaded ui data tree
(stage). So the code expects a widget the data doesn't have.

## Why it is NOT a phase-16 (Decruft) regression

Discovered during the Phase 16 D-08 boot smoke; ruled out conclusively:
- The entire phase-16 diff is 4 files (`CuiPreferences.cpp/.h`, `SwgGodClient.vcxproj`,
  `SwgCuiHudAction.cpp`) — none is `SwgCuiOptUi` / `SwgCuiOpt` / `CuiMediator` / any `.ui`.
- SwgClient relinked Debug+Release with **0 unresolved externals** — no live symbol was
  removed; the voice-volume API that phase 16 deleted had zero callers (incl. the Options UI).
- The `d1b3c0eaf` feature predates phase 16.

## Fix options (when a UI phase picks this up)

1. **Preferred:** add the `checkShowToolbarCommandCooldownTimer` `TUICheckbox` to the
   GroundHUD options `.ui` layout so code + data match (this is what the feature intended).
2. **Defensive:** make the `SwgCuiOptUi.cpp:219` lookup non-fatal (optional widget) so a
   missing checkbox degrades gracefully instead of FATAL-ing the client.

## Reproduction

Boot `stage/SwgClient_d.exe` (rasterMajor=11), reach a character in-world, open the Options
window (e.g. via the toolbar/options command path through the chat/console parser). FATALs
immediately on the GroundHUD options component load.
