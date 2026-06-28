# CONSULT-48 — shared evidence pack (Does this client+data fully support JTL space, correctly configured?)

Treat ALL of the following as GIVEN (measured on 2026-06-26 — runtime + TRE inventory). Do not
re-derive; reason FROM it. Investigate ONLY your assigned angle (below).

## LOCKED ground truth (measured this session — do NOT contradict)
- Client repo: `D:/Code/swg-client-v2` (MSBuild SWG client, "Compiled for Stella Bellum Testing").
- Game data: `D:/Code/SWGSource Client v3.0` (209 `.tre`, ~150,575 logical files).
- **Real server space entry**: operator launched a SHIP from a station launch terminal (connected to
  the swg-main VM at 192.168.1.200:44453) and travelled to space — NOT the Utinni offline harness.
- Renderer: **DX9 / gl05 (rasterMajor=5)** — NOT D3D11.
- Symptoms in space: the player's SHIP renders as the **default/missing-model placeholder** (its
  appearance didn't load); the space environment renders broken ("planets" as reflective chrome
  spheres, green/teal wash, floating debris squares, garbage radar) — same look as earlier offline
  screenShot0407/0408. The space **reticle/HUD is present** (targeting works).

## BANNED framings (already FALSIFIED — do not propose these)
- "D3D11 rendering parity bug" — FALSE: this is gl05/D3D9.
- "Utinni offline-harness artifact" — FALSE: this is a real server space entry with a ship.
- "JTL data is entirely missing" — FALSE: see inventory below.

## Measured data inventory (JTL data is broadly PRESENT)
- **17 space terrains** incl. `terrain/space_tatooine.trn`, `space_yavin4.trn`, naboo/corellia/etc.
- **573 `datatables/space/`** entries (asteroidfield fieldstyles, debris, etc.).
- **2600 ship object templates** (`object/.../ship/...iff`).
- **306 ship appearances** (`appearance/*ship*` .apt/.lod/.msh).
- Sampled assets (space_tatooine.trn, a ship appearance, a ship pcd template) are all in
  TOC-WIRED TREs (verified loadable).

## THE WIRING ANOMALY (prime suspect — verify and explain)
cfg load order (`stage/client.cfg`): `searchTOC` sku0..3 (enumerate **198** TREs) + `searchTree`
`swgsource_3.0.tre` + `disable_wayfar_dearic_snow.tre` + loose install-root `searchPath` + `stage/override`.

**9 TREs are present on disk but NOT referenced by any TOC or searchTree (so NOT loaded):**
`gu8.tre`, `hotfix_56_client_00.tre`, `hotfix_56_shared_00.tre`,
`ILM_animation.tre`, `ILM_maps.tre`, `ILM_music.tre`, `ILM_sound.tre`, `ILM_ui.tre`, `ILM_visuals.tre`.

(Note: a guessed-path space-zone datatable `datatables/space/zone/space_tatooine.iff` was NOT found —
path may be wrong; the real space-zone/environment config table location is unknown and worth finding.)

### Contents of the unwired TREs (measured) — the ILM_* family holds JTL space VISUALS
- **`ILM_visuals.tre` = 9,849 files** incl. **749 space/ship assets**: ship LODs
  (`appearance/lod/ship_*`, `ship_component_*`), `spacestation_*` appearances, **nebula datatables**
  (`datatables/space/nebula/space_corellia_2.iff`), ship chassis tables, ship clientdata `.cdf`,
  planetoid textures. (5812 appearance + 3361 texture + 213 shader + 168 datatables total.)
- **`ILM_ui.tre`** = space HUD UI/textures (`ui/ui_hud_space_*`, `ui_space_hud.dds`, `ui_palette_space.inc`).
- **`ILM_sound.tre`** = space SFX (`ship_*_gun`, `shp_player_hyperspace_*`, `amb_space_station_lp`).
- **`ILM_music.tre`** = space battle music (`mus_space_battle_*`).
- **`ILM_animation.tre`** = 76 appearances. **`ILM_maps.tre`** = planet/space-hyperspace map UI + textures.
- `gu8.tre` = 722 GU8 (ground-era) objects/appearances/textures, NO space content.
  `hotfix_56_*` = EMPTY (0 files).

So the leading-evidence pattern: the **ILM_* community visual/content pack holds much of the JTL
space VISUAL layer (ship models, station/planet/nebula visuals, space HUD/SFX/music) and is NOT
wired into the cfg**. OPEN PUZZLE for the crew: some nebula data appeared to load at runtime earlier
even though its datatable is in unwired ILM_visuals — so either ILM IS loaded by a mechanism not
captured here, or those assets are duplicated in a loaded TRE. Resolve whether ILM is a REQUIRED
base layer (unwiring it breaks ship/space visuals) or an OPTIONAL enhancement (base assets live
elsewhere and the breakage is something else), and prescribe the correct fix.

## The question (answer from YOUR assigned angle only)
Is JTL/space FULLY SUPPORTED and CORRECTLY CONFIGURED in this client+data, or is something
missing/misconfigured? Specifically: why does the SHIP render as the default placeholder and the
space ENVIRONMENT render broken when the JTL data is present? Are the 9 unwired TREs (esp. the ILM_*
family / gu8 / hotfix_56) SUPPOSED to be loaded — is that the gap? What is the complete/correct
TOC+searchTree+cfg manifest a SWGSource client needs for JTL space, and what's missing here?
Give a decisive disambiguating test.
