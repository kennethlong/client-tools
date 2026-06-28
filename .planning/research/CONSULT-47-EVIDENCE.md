# CONSULT-47 — shared evidence pack (JTL space: real gaps vs version mismatch)

Treat ALL of the following as GIVEN (established by runtime crash dumps + TRE extraction on
2026-06-26). Do not re-derive; reason FROM it.

## Setup
- A modern MSBuild SWG client lives at `D:/Code/swg-client-v2` (project "SwgClient"). Its exe
  resource strings: ProductName "Star Wars Galaxies", FileDescription "SwgClient_r",
  Comments "Compiled for Stella Bellum Testing", FileVersion 2.0b.
- Game data (TRE files) is loaded from a SEPARATE directory: `D:/Code/SWGSource Client v3.0`
  (209 `.tre` files). Load config (`client.cfg`): base set enumerated via TOCs
  `sku0..3_client.toc` (file-dated Oct 2017); PLUS higher-priority `searchTree` overrides
  `disable_wayfar_dearic_snow.tre` and `swgsource_3.0.tre`; PLUS a loose dev override dir.
- So: client SOURCE and client DATA were obtained as two separate downloads ("latest version" of
  each, per the operator). The open question is whether they are the SAME publish/version.

## Observation A — Space HUD CodeData contract mismatch
Client code `SwgCuiHudSpace` ctor
(`src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHudSpace.cpp`) HARD-fetches
(FATAL if missing) 7 CodeData objects from UI page `/HudSpace`:
`DamagePage`(Page), `buttonEject`(Button), `buttonExitStation`(Button), `trainerButton`(Button),
`buttonEnterSpace`(Button), `missileLockOnYou`(Page), `missileLockOnYouDeformer`(Effector).

The space-HUD UI is defined in data file `ui/ui_hud_space.inc`. TWO copies exist in the data:
- `swgsource_3.0.tre` copy: has eject/exitStation/trainer; has **NO** `buttonEnterSpace` (no
  CodeData key, no widget).
- `ILM_ui.tre` copy: has the CodeData KEY `buttonEnterSpace='buttons.enterSpaceButton'` **BUT NO**
  `enterSpaceButton` widget in the page tree.
- BOTH copies: widgets `missileLockOnYou` (Page) and `missileLockOnYouDeformer` (DeformerWave)
  EXIST in the page tree, but NEITHER copy has a CodeData `<Data>` mapping entry for them.
- `swgsource_3.0.tre` (search priority 8) shadows `ILM_ui.tre` (priority 0-3) at runtime.
- Per the code comment, `buttonEnterSpace` is the "atmospheric flight / Enter Space" button.

## Observation B — Nebula lightning appearance fails to create
Client code `NebulaManagerClient`
(`src/engine/client/library/clientGame/src/shared/space/NebulaManagerClient.cpp`) requires each
space nebula's lightning appearance — `nebula->getLightningAppearance()` returns a NON-EMPTY asset
name — to be creatable via `AppearanceTemplateList::createAppearance()` +
`LightningAppearance::asLightningAppearance()`. For a `space_yavin4` nebula this returned NULL (the
named appearance asset is either absent from the loaded TREs, or is not a LightningAppearance type).

## What WORKED (same session, ~4 min in space before the nebula crash)
`terrain/space_yavin4.trn`, asteroid templates (`shared_asteroid_iron_medium_01`), a
`poi_yavn_death_star_wreckage` POI mesh, a stormtrooper NPC, `pt_nebulae_gas` shaders, the radar UI.

## The question (answer from YOUR assigned angle only)
Are observations A and B REAL gaps in SWGSource's JTL client support, or a VERSION MISMATCH between
the client source tree and the TRE data set (i.e. the operator got mismatched "latest" versions)?
If a mismatch: which side is newer, and what is the disambiguating test?
