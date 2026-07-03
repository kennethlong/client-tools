# ILM_visuals.tre: Legends preference-changes vs retail — analysis & proposed optional-TRE split

**Date:** 2026-07-03 · **Status:** analysis complete, scope-down applied locally, SWGSource
contribution proposal drafted below.
**Audience:** us + the SWGSource maintainers (this doc is written to be shareable).

## TL;DR

`ILM_visuals.tre` (the "Improved Lighting Mod" pack shipped-but-unwired in the SWGSource v3.0
client distribution) mixes THREE distinct content classes that deserve different handling:

1. **Stub placeholders** (6,376 / 9,849 entries) — tiny 8x8 zero-alpha textures and stub
   `.sht`/`.prt`/`.lmg` that, wired as a search layer, SHADOW real base assets (breaks nebula
   skyboxes, planet shaders). Must never be mounted raw.
2. **JTL-required content** (~1,486 entries after our split) — ship chassis datatables, ship
   component visuals/LODs/cockpits, space stations, nebulae, space combat effects. In THIS
   distribution the player-ship data is ILM-exclusive (SWGSource dsrc Issue #23 / PR #415
   context): without it, player ships render as the default placeholder. **This class is
   effectively REQUIRED for JTL.**
3. **Legends aesthetic/preference changes to GROUND content** (~624 entries) — deliberate
   re-authoring of retail files: interior fog disabled game-wide, re-lit ground/terrain
   textures, character skin textures, faction palettes, sky/planet visuals, cutscene table.
   Opt-in taste, not fixes. **Wired silently, these change how the retail game looks/feels.**

**Proposal to SWGSource:** ship class 2 as a wired-by-default `ILM_space_required.tre` and
class 3 as an explicitly optional `ILM_ground_visuals.tre` (documented, commented-out cfg lines
like the existing live.cfg ILM block), and drop class 1 entirely.

## The proof case: interior fog (found 2026-07-03)

`datatables/interior/interior.iff` in ILM is retail's table (patch_16_00 vintage, 227 rows)
with **`Fog Enabled` flipped 1 -> 0 in 124 rows** — every fogged interior in the game: all
cantinas, Jabba's Palace, the Lucky Despot, Fort Tusken (all 13 cells), the Yavin temples,
Nightsister/clan strongholds, POI bunkers/dungeons, cloning/hospital/hotel/guild/capitol
buildings, the newbie hall, merchant tents — while fog density/color and all other columns stay
byte-identical, zero retail rows are dropped, and 38 new rows are added for Legends content
(Mustafar, gunboat, blackwing-facility POBs). One further change: Kashyyyk Myyydril caverns'
`lightningroom34` ambient swapped from `amb_myyydril_caverns_creepy.snd` to generic
`amb_cave_int_lp.snd`.

The surgical single-column consistency proves intent (an export bug produces noise, not this).
Result in a retail-behavior client: the cantina "smokey haze" and every interior fog effect
silently vanish. Our fix: a **merged** table (retail values for retail rows + ILM's 38 new rows
appended) — see `stage/override/datatables/interior/interior.iff` in this repo (commit
`8cd8c2d82`); that merged file is itself a candidate contribution (it is what
`ILM_space_required.tre` should carry instead of the fog-off variant).

## Quantified inventory (measured, not estimated)

Method: `stage/ilm_extract` (the de-stubbed 2,110-entry extraction of ILM's genuine content)
compared byte-wise against the retail-effective copy (last-wins across `data_*.tre` +
`patch_*.tre` in TOC order). Tooling: `swg-blender-plugin/swg_pipeline/tre_reader.py` + the
classifier at `.planning/research/ILM-SCOPEDOWN-classifier.py`; full per-file manifest with
per-rule rationale at `.planning/research/ILM-SCOPEDOWN-manifest.csv`.

- 2,110 genuine ILM entries = **1,119 ILM-exclusive** (additive; no retail counterpart) +
  **991 overlapping retail**, of which **990 differ** (the extraction kept exactly
  exclusives+diffs, so this is expected).
- The 990 diffs classified (rule counts in `ILM-SCOPEDOWN-run-report.txt`):
  - **KEEP / space-scoped (367):** ship assets incl. textures/LODs/cockpits (224), ship
    clientdata (38), space combat particle fx (40) + cefs (14), space datatables incl.
    missiles.iff (9), nebula/space-named skybox+station textures (38), space blast shaders (4).
  - **PARK / ground preference-changes (624):** terrain textures (483 — the dirt/grass/rock/
    sand/snow/forest blend-tile families across planets), sky+planet visuals (26 — `pln_*`,
    `planet_*.pln`, `grad_sky_*`; note ILM's `planet_tatooine.pln` was previously found
    actively WRONG — a Kessel variant), character skin/face textures (32), spaceport interior
    re-lights (16) + Tatooine starport meshes (16), interior textures (15) + shaders (6),
    palettes (6 — incl. stormtrooper stripes, rebel faction armor), pixel_program files (5 —
    **incl. `include/functions.inc` + `pixel_shader_constants.inc`, which are compile inputs
    for EVERY shader**), terrain `.trn` definitions (7 — dantooine/lok/yavin4), the interior
    fog datatable (1), cutscenes table, default shaders (2), force-power/lightsaber fx (4),
    intro textures (3), `e_particle_emisadd.eft` (shared base effect), flashspeeder windshield.
  - **JUDGE (1):** `shader/pt_add_smoke.sht` (shared thruster/ground smoke) — parked
    retail-faithful; restore first if space thruster smoke regresses.

## What we changed locally (reversible)

- The 624 PARK + 1 JUDGE files were **moved** (not deleted) from `stage/ilm_extract/` to
  `stage/ilm_extract_parked/` preserving relative paths. Restoring any file = moving it back.
- `stage/ilm_extract` now carries only: ILM-exclusives (1,119) + space-scoped diffs (367).
- The interior fog fix rides separately at `stage/override/datatables/interior/interior.iff`
  (priority 10) because it is a MERGE, not a pick-one.

## Verification bar

- Ground: cantina haze present (confirmed live 2026-07-03 via the override before the parking;
  re-confirm after), retail terrain/skin/palette look on Tatooine+Dantooine.
- Space: zone in, ship renders with proper chassis visuals (the reason the layer exists),
  nebula skyboxes intact, combat effects fire. Restore-first candidates on regression:
  `pt_add_smoke.sht`, `e_particle_emisadd.eft`, the `sky-planet-visual` group.

## Proposed SWGSource deliverable

1. `ILM_space_required.tre` — the 1,486-entry KEEP+exclusive set, wired by default in the
   shipped cfg (fixes dsrc Issue #23 placeholder ships out of the box), with interior.iff
   replaced by the retail-values+ILM-rows MERGED table.
2. `ILM_ground_visuals.tre` — the 624-entry preference set, shipped commented-out in the cfg
   (the existing live.cfg ILM block precedent), documented as "Legends re-lit ground visuals +
   interior fog removal; opt-in".
3. Never ship the 6,376 stub entries.
The per-file manifest CSV is the packing list for both TREs.
