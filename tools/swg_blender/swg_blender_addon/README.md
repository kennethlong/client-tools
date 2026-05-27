# SWG Blender add-on

Thin UI over `tools/swg_blender`. Install this folder as a Blender add-on.

## Install

1. Blender 3.x or 4.x → Edit → Preferences → Add-ons → Install
2. Select this directory (`swg_blender_addon`) or zip it first
3. Enable **SWG IFF Pipeline**

The add-on expects `tools/swg_blender` as its parent directory. If you copy only
the add-on elsewhere, set **Pipeline root** in add-on preferences.

## Preferences

| Setting | Purpose |
| --- | --- |
| Pipeline root | Path to `tools/swg_blender` when not co-located |
| swg-main root | Sets `SWG_MAIN` for shader/texture resolution during bundle export |

## Sidebar (Phase 15)

**SWG → Export** — choose export type, author/notes, texture/blend options, CKAT/KFAT, pack TRE.

**SWG → Run Export** — runs the selected export type (file or folder picker).

**SWG → Hierarchy** — validate Maya-style node names (`mesh0`, `c0`, `l0`, `pob`, …).

## Operators

**Import:** `.msh` static mesh, `.mgn` skeletal mesh (auto-resolves `.skt` when possible)

**Export:** `.msh`, `.mgn`, `.skt`, `.ans`, static client bundle, **creature bundle** (`.mgn` + `.skt` + `.sat` + optional `.ans`)

Manifest `swg_export_manifest.json` records `pipeline_version` **3.009**, author, and notes.

See `../PIPELINE.md` and `../CLIENT_SPAWN_CHECKLIST.md`.