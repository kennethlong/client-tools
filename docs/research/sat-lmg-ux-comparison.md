# SAT / LMG authoring UX — io_scene_swg_msh vs tools/swg_blender

Reference: `D:\Code\io_scene_swg_msh` (Nick Rafalski) vs `tools/swg_blender` creature TRE project (M16).

Skeletal/armature parity detail: [skeletal-import-io-scene-parity.md](skeletal-import-io-scene-parity.md).

## Collection layout

| Topic | io_scene_swg_msh | swg_blender (M16) |
| --- | --- | --- |
| Root collection | Named after `.sat` stem | `SWG_Creature_{sat_stem}` |
| SAT metadata | Written on export from child `.lmg` collections | `sat_root` empty with `swg_kind=sat`, `swg_tre_relpath` |
| Per-part grouping | Child collection per `.lmg` (name ends with `.lmg`) | Child collection per `.lmg` under `SWG_Creature_*` (`swg_lmg_relpath` on collection) |
| LOD organization | Under `.lmg` collection via `import_lod` / export walks mesh objects | `LODs` sub-collection per part; l0 in `.lmg` coll; l1+ in `LODs` (hidden); export uses `all_objects` |
| Skeleton | `.skt` child collections on SAT collection | Single shared armature; `swg_kind=skeleton` |
| Multi-shader `.mgn` | One Blender object, multiple materials (`import_mgn`) | Separate objects per PSDT; merged on export via `swg_tre_relpath` + `swg_submesh_index` |

## Naming

| Topic | io_scene_swg_msh | swg_blender |
| --- | --- | --- |
| Mesh object names | Blender object name becomes `.mgn` stem on LMG export | Uses IFF mesh name when present (`abyssin_hair_aa7`); else `{stem}_{idx}` |
| LMG paths | `appearance/mesh/{collection.name}.lmg` from child collection name | Preserved from retail graph / manifest `lod_mgn_relpaths` |
| SAT MSGN | Hard-coded `appearance/mesh/{lmg}.lmg` on export | Rebuilt from manifest on export (`rebuild_creature_appearance_files`) |

## Export workflow

| Topic | io_scene_swg_msh | swg_blender |
| --- | --- | --- |
| SAT export | Active layer collection → `export_sat` writes SMAT + child LMGs | CLI / add-on: export meshes to stored paths, then rewrite SAT/LMG from graph |
| LMG export | Iterates mesh objects in `.lmg` collection; optional per-object `.mgn` | Per-part LOD chain from manifest; multi-object → one SKMG |
| Tangents / CKAT | Export flags on operators | Creature: shape keys optional; anim CKAT default in bundle export |

## Recommended alignment (future)

1. **Optional single-object multi-shader import** for `.mgn` (io_scene style) while keeping multi-object for TRE round-trip fidelity.
2. ~~Child collection per `.lmg`~~ — done in `tre_project_bpy.import_creature_from_workspace`.
3. ~~`LODs` collection~~ — done; toggle collection visibility to show/hide LOD1+.
4. ~~SAT/LMG rebuild in-addon~~ — **Rebuild SAT/LMG only** operator (`swg.rebuild_creature_sat_lmg`).

## Building contrast (M17)

| Topic | io_scene_swg_msh | swg_blender |
| --- | --- | --- |
| Root | POB filename collection | `SWG_Building_{pob_stem}` |
| Cells | Sub-collection per cell; `Appearance_{cell}` mesh | `{building}_{cell_name}` with `mesh0_{cell}` static mesh |
| Appearances | Resolves `.apt` / `.lod` / `.msh` at import via SWG_ROOT | Resolved at graph materialize; `primary_mesh_relpath` in manifest |
| POB portals | Full portal/floor import | Floor wire + `portal{N}_idtl` meshes; POB cell rewrite with CMPT/PRTL/LGHT preserved |
