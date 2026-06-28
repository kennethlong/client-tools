---
created: 2026-06-26
title: Verify the space-HUD loose-override cfg fix + chase the nebula-lightning runtime null (JTL space shakedown)
area: client cfg / space (JTL) HUD + nebula / SWGSource data wiring
next_action: live re-test space load with searchPath_00_9 wired + band-aid retired — confirm the loose retail ui_hud_space.inc wins (no whack-a-mole), then decide whether to commit the cfg change + NebulaManagerClient guard. Separately, confirm whether the nebula WARNING repeats.
files:
  - stage/client.cfg  (FIX: added searchPath_00_9="D:/Code/SWGSource Client v3.0/" — wires the loose retail override layer; pri 9 > TREs 7/8)
  - stage/override/ui/ui_hud_space.inc.bak  (retired band-aid — the 3 hand edits; superseded by the loose retail file)
  - src/engine/client/library/clientGame/src/shared/space/NebulaManagerClient.cpp  (the :836 skip+WARNING guard, built+deployed, uncommitted)
  - src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHudSpace.cpp  (the ctor's 7 hard getCodeDataObject fetches)
  - loose retail data: "D:/Code/SWGSource Client v3.0/ui/ui_hud_space.inc" (COMPLETE, has enterSpaceButton, dated May 5) + appearance/pt_nebulae_white_02.ltn in data_sku1_00.tre
references:
  - memory: project_space_hud_buttonenterspace_tre_shadow  (corrected root-cause + CONSULT-47)
  - .planning/research/CONSULT-47-*  (4-AI crew: codex/cursor/sonnet/opus + EVIDENCE + ground-truth)
  - .planning/todos/pending/2026-06-13-test-jtl-space-rendering-post-v2.2.md  (JTL render-test counterpart)
status: research
priority: medium (cfg fix applied + unverified live; nebula root-cause still open)
---

## What this is (CORRECTED 2026-06-26 by CONSULT-47 — supersedes the original "upstream contract gap" framing)

Entering a space scene FATAL'd in the space-HUD ctor on `buttonEnterSpace`. The original read
("SWGSource data is incompletely authored, needs upstream coordination") was WRONG on the mechanism.
4-AI crew (codex=code-vintage, cursor=data-vintage, sonnet=history, opus=logic) + ground-truth probes
established:

- **`buttonEnterSpace` = atmospheric flight, shipped Game Update 21 (2011-09-15)**, 3mo before SWG
  shutdown. Our client SOURCE is final-retail NGE (has the GU21 code, pinned by net-proto stamp
  20100225). SWGSource lists GU21 as unimplemented server-side; `ILM_ui.tre` is a community UI mod
  (key-without-widget = modder started from a GU21 .inc, never authored the button).
- **The complete retail GU21 `ui_hud_space.inc` (with the real `enterSpaceButton` widget) WAS already
  in the install** — loose at `D:/Code/SWGSource Client v3.0/ui/` (dated May 5) — but our `client.cfg`
  had NO searchPath for that dir, so the older pre-GU21 TRE copy (`swgsource_3.0.tre`, pri 8) won.
  **The bug was a cfg wiring omission on OUR side, NOT a wrong-version download and NOT (for the HUD)
  a real SWGSource data gap.**
- The `missileLockOnYou`/`Deformer` "widgets-without-mapping" were a **false alarm**: `getCodeDataObject`
  falls back to `GetChild(name)` (CuiMediator.cpp:1521) when no CodeData property exists, and both are
  direct children → resolve fine. My 2 hand-added mappings were unnecessary.

## Fix applied (cfg, reversible)
Added `searchPath_00_9="D:/Code/SWGSource Client v3.0/"` (pri 9 > TREs) to wire the loose retail
override layer; retired the hand-authored `stage/override/ui/ui_hud_space.inc` to `.bak`. The loose
complete file now wins → real `enterSpaceButton` present. `NebulaManagerClient` guard built+deployed.

## Open items
1. **Live re-test** the space load: confirm the loose retail HUD loads cleanly (no whack-a-mole on
   other space mediators) with the band-aid gone. The `searchPath_00_9` now exposes the WHOLE install
   root as a loose-override layer (intended SWGSource client behavior, but broad — first suspect if
   anything else regresses).
2. **Nebula null (separate, NOT a data gap):** the yavin4 nebula's `appearance/pt_nebulae_white_02.ltn`
   EXISTS in `data_sku1_00.tre` (wired into sku1_client.toc, loaded) and the `.ltn` type IS registered
   (SetupClientParticle → LightningAppearanceTemplate::install → assignBinding). So `createAppearance()`
   returning null is a RUNTIME anomaly (likely transient/async load race), not a missing file. Confirm
   on next space run whether the WARNING (`pt_nebulae_white_02.ltn`) repeats: deterministic = a
   load-order/timing bug to chase (cf. ClientNebula.cpp:127 guards with TreeFile::exists; the crashing
   NebulaManagerClient.cpp:834 did not); one-off = transient, guard suffices.
3. **Commit** the cfg change + the NebulaManagerClient guard once the re-test passes (stage explicit
   paths per the dirty-repo rule).

## Done when
- A clean live JTL space load completes past full HUD construction via the loose retail UI (not the
  hand-edited band-aid);
- the nebula WARNING's repeat-vs-one-off behavior is characterized (and chased if deterministic);
- cfg + guard committed.
