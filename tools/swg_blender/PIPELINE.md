# SWG asset pipeline (tools/swg_blender)

End-to-end flow from Blender edits to a bootable client test.

## Layout

| Path | Role |
| --- | --- |
| `swg_iff/` | IFF parse/dump |
| `swg_scene/` | Scene IR loaders/writers |
| `swg_blender/` | bpy import/export CLIs |
| `swg_blender_addon/` | Blender File menu + SWG sidebar |
| `swg_pipeline/` | TRE wrappers, rsp, bundles, validation |

## CLI reference

```powershell
cd tools/swg_blender
python -m pytest tests/ -q
# Skip headless Blender E2E (needs blender on PATH or BLENDER_EXE):
python -m pytest tests/ -q -m "not e2e"
# Headless import -> export round-trip (golden static .msh + skeletal .mgn):
python -m pytest tests/test_e2e_blender_roundtrip.py -q

# Human in-game validation (Phase 7, no Blender expertise required):
# See HUMAN_E2E_TEST_GUIDE.md

# Inspect IFF
python -m swg_iff.dump path/to/file.msh

# Import (dry-run stats without bpy)
python -m swg_blender.import_static tests/golden/frn_all_bed_sm_s1_l0.msh --dry-run

# Client test bundle (mesh + shader + rsp + cfg snippet)
python -m swg_pipeline.export_bundle phase7 --output-dir D:\swg_dev_bundle

# TreeFileRspBuilder-style manifests
python -m swg_pipeline.rsp_builder D:\swg_dev_bundle --output-dir D:\swg_dev_bundle\rsp --client-cfg D:\swg_dev_bundle\client_paths.cfg

# TRE listing (version-aware: 0004/0005=24-byte TOC, 6000=32-byte TOC)
python -m swg_pipeline.tre_list -l path/to/file.tre
python -m swg_pipeline.tre_list --master path/to/master.toc
python -m swg_pipeline.tre_list --toc   # sample COT2000 when SWG_SAMPLE_TRE_DIR set

# TRE extract (requires built TreeFileExtractor.exe — needed for encrypted v6000 payloads)
python -m swg_pipeline.tre_tools extract --tre path/to/file.tre --out extracted/

# Creature TRE project (M16): materialize full SAT graph into a workspace
python -m swg_pipeline.tre_project import-creature --workspace D:\swg_creature_ws --sat appearance/abyssin_m.sat
python -m swg_pipeline.tre_project import-creature --workspace D:\swg_creature_ws --sat appearance/abyssin_m.sat --tre path\to\data.tre --copy-textures
python -m swg_pipeline.tre_project import-creature --workspace D:\swg_creature_ws --sat appearance/abyssin_m.sat --blender
python -m swg_pipeline.tre_project export-creature --workspace D:\swg_creature_ws --blender
python -m swg_pipeline.tre_project pack --workspace D:\swg_creature_ws --rebuild-rsp

# Headless export needs import+export in one Blender session (--roundtrip); in-addon export uses the open scene.
python scripts/run_creature_roundtrip_check.py

# Multi-shader .mgn (e.g. abyssin_m_l0): several mesh objects share one swg_tre_relpath; export merges PSDTs.
# Creature import: per-.lmg child collection + LODs (l0 visible, l1+ in LODs coll, hidden); export walks all_objects.
# Skeletal .mgn uses from_pydata (duplicate triangles preserved, same as static .msh).

# Building TRE project (M17): POB -> cells -> static meshes
python -m swg_pipeline.tre_project import-building --workspace D:\swg_building_ws --pob appearance/echo_base_pob.pob --blender
python -m swg_pipeline.tre_project export-building --workspace D:\swg_building_ws --blender
# Headless full session: python scripts/run_building_roundtrip_check.py (materialize + Blender --roundtrip)
# Multi-shader .msh: import tags one building_mesh per cell; extras hidden; export merges all PSDTs per path
# Static import uses mesh.from_pydata so retail duplicate triangles are preserved (bmesh deduped them)
# Per cell: hidden wireframe floor0_{cell} from .flr (swg_kind=building_floor); export preserves BTRE/BEDG/PGRF
# POB: cell collection props; portals/ has portal{N} empty + portal{N}_idtl mesh (IDTL); export rewrites CELS+PRTS
# CELL rewrite preserves CMPT/PRTL/LGHT blocks; unchanged cells -> byte-identical .pob
# Fast smoke: python scripts/run_building_smoke.py  (blacksun 21 cells; --pob for echo_base)
# Creature client smoke: python scripts/run_creature_client_smoke.py [--blender] [--pack]
# Side-by-side vs io_scene_swg_msh: python scripts/compare_creature_io_scene.py --in-blender (MCP/Scripting)
#   or --workspace D:\path (spawns a second Blender GUI by default; use --background for -b)
# See docs/research/sat-lmg-ux-comparison.md and docs/research/skeletal-import-io-scene-parity.md

# PNG → DDS (Phase 8.7; DXT via texconv when available, else A8R8G8B8)
python -m swg_pipeline.dds path/to/texture.png path/to/texture.dds
python -m swg_pipeline.dds path/to/texture.dds --info
```

### Hardpoints (Phase 8.1)

Mark Blender **Empty** objects parented to the mesh:

- Custom property `swg_hardpoint` = engine hardpoint name (e.g. `foot`), or
- Name prefix `hp_` / `HP_` (e.g. `hp_foot` → `foot`)

Static export rebuilds `APPR/0003` with visual extent (`EXBX`) and `HPTS` blocks from those empties.
Round-trip import of appearance metadata is still raw-bytes only; hardpoint **export** is supported.

### Collision, floor, extents (Phase 12)

| IFF | Module | Notes |
| --- | --- | --- |
| `FLOR/0006` `.flr` | `swg_scene/floor_mesh.py` | VERT + TRIS (FloorTri 0002); preserves BTRE/BEDG/PGRF raw |
| `EXBX` / `EXSP` / `XCYL` / `CMSH` / `NULL` | `swg_scene/extents.py` | Standalone extent files + APPR collision slot |
| `IDTL` | `swg_scene/indexed_triangle_list.py` | Mesh collision extent (`CMSH/0000`) |

Hierarchy lint recognizes `collision`, `floor`, `extent`, `sphere`, `cube`, `cylinder` (MayaExporter naming).

Tests: `tests/test_phase12_collision.py` (retail `.flr` byte roundtrip on swg-main).

### Building / POB / portals (Phase 13)

| IFF | Module | Notes |
| --- | --- | --- |
| `PRTO/0004` `.pob` | `swg_scene/portal_property.py` | PRTS (IDTL portal geom), CELS (cells), PGRF, CRC |
| `APT/0000` `.apt` | `swg_scene/appearance_redirect.py` | Appearance path redirect for cell interiors |
| Assembly | `swg_scene/building_export.py` | `export_pob`, `export_apt` helpers |

Hierarchy lint: `pob`, `building`, `portals`, `portal`, `cells`.

Tests: `tests/test_phase13_building.py` (retail `echo_base_pob.pob` byte roundtrip).

### TRE production pipeline (Phase 14)

End-to-end: export bundle → `rsp/*.rsp` → `tre/*.tre` via `TreeFileBuilder.exe`.

```powershell
# Pack all rsp manifests in a dev bundle
python -m swg_pipeline.pack_pipeline D:\swg_dev_bundle --rebuild-rsp

# One-shot: export static + pack
python -m swg_pipeline.export_bundle static --mesh path\to\mesh.msh --output-dir D:\swg_dev_bundle --pack-tre

# TRE tools (pack / extract / list)
python -m swg_pipeline.tre_tools pack --rsp D:\swg_dev_bundle\rsp\data_compressed_other.rsp --output D:\patch.tre
python -m swg_pipeline.tre_tools list --tre D:\patch.tre --python-reader
python -m swg_pipeline.tre_extract --tre D:\patch.tre --out extracted\

# Batch CLI (Maya-style flags)
python -m swg_pipeline.batch_export static -node path\to\mesh.msh -outputdir D:\out -author "name"
python -m swg_pipeline.batch_export pack -outputdir D:\out --rebuild-rsp
```

- `swg_export_manifest.json` includes `pipeline_version` (`3.009`), `author`, optional `tre_files` after pack.
- Extended TRE tags `0006` / `5000` use 32-byte TOC stride; payloads may still need `TreeFileExtractor` (see `tre_decrypt.py`).

### Vertex colors + SIDX (Phase 8.4 / 8.2)

- Blender **Color** attribute (`Col` or active color layer) exports as VTXA `F_COLOR0`.
- Meshes with vertex alpha auto-generate **SIDX** direction-sorted index arrays (10 view directions) for transparency sorting.
- Loaded SIDX blocks round-trip on re-export.

### Index cache optimization (Phase 8.3)

Blender static export sets `optimize_indices=True`, reordering triangle indices for GeForce1/2-style vertex cache locality (same approach as MayaExporter `GenerateStrips` + `SetListsOnly(true)`). Topology is unchanged; round-trip reload without `optimize_indices` preserves original index order.

### Shader export from materials (Phase 8.8 / 10.1)

When no reference `.sht` is supplied, export builds **SSHT/0000** from the Blender Principled BSDF:

- **MAIN** ← Base Color image (or `swg_texture_MAIN` / `texture/{name}.dds`)
- **SPEC** ← Specular map when linked → `effect\a_specmap.eft` + KCLB/CEPS blocks
- **EMIS**, **CNRM**, **ENVM**, **MASK**, **HUEB** via `swg_texture_*` custom props or linked Principled inputs
- Alpha / punchout heuristics → `effect\a_alpha.eft`, `effect\a_punchout.eft`, etc. (`swg_pipeline/shader_effects.py`)
- Load retail `.sht` → `ShaderBuildSpec` via `swg_pipeline/shader_import.py` for roundtrip tests

Reference stub copy still works via `shader_stub_reference` for retail parity templates.

`export_static_bundle(..., build_shaders_from_scene=True)` or CLI `--build-shaders-from-scene` writes shaders from scene material slots without a reference `.sht`.

### Texture bundle copy (Phase 10.7)

- `swg_pipeline/texture_bundle.py` — copy DDS paths referenced by SSHT into a dev bundle
- Resolves loose files under `SWG_MAIN/serverdata` (retail textures in TREs are not extracted automatically)
- Wired into `export_static_bundle` / `export_skeletal_bundle_from_files` when `copy_textures=True` (default)
- CLI: omit `--no-copy-textures` on `export_bundle static` / `skeletal`

### Multi-UV + DOT3 (Phase 8.5)

- Exports up to **8 UV layers** from Blender (`mesh.uv_layers`).
- Normal-map materials add a **DOT3** tangent basis UV set (4 floats: tangent xyz + handedness).
- Shader **TCSS** maps `MAIN` → UV0, `DOT3` → tangent set index when present.


### Normal map bake (Phase 8.6)

- `swg_pipeline/normal_map.py` ports Maya `NormalMapUtility` scratch-height → normal and UV-space mesh baking.
- `bake_mesh_normal_map()` / `bake_swgmesh_normal_map()` rasterize interpolated vertex normals into BGR888 tangent normals.
- `export_static_bundle(..., bake_normal_maps=True)` writes `texture/{mesh}_norm.dds` for DOT3 materials.

### Per-shader 65535 index split (Phase 8.9)

Static export auto-splits any mesh whose triangle index list exceeds **65535** entries into multiple SPS shader groups (`0001`, `0002`, …) sharing the same `.sht` path. Validation rejects unsplit meshes over the limit.

### Byte-compare harness (Phase 8.10)

- `swg_pipeline/parity.py` — APPR byte compare + semantic scene diff + `parity_report()`.
- `tests/test_maya_static_parity.py` — golden bed mesh load→export→reload checks (appearance preserved, topology stable).

## TreeFileRspBuilder workflow

Retail packing uses `.rsp` files listing `treefile_path @ filesystem_path` lines.
`rsp_builder.py` mirrors the C++ tool buckets (`.msh`, `.mgn`, `.ans`, `.dds`, …).

Typical dev loop:

1. Export a bundle under `D:\swg_dev_bundle` (add-on or `export_bundle` CLI).
2. Add `searchPath0=D:\swg_dev_bundle` to `client_d.cfg` (see `client_search_paths.cfg`).
3. Validate in-game — `CLIENT_SPAWN_CHECKLIST.md`.
4. Optional: regenerate rsp from the same folder for `TreeFileBuilder.exe`.

Engine tool source: `src/engine/shared/application/TreeFileRspBuilder/`.

## Blender add-on (Phase 15)

Install folder `swg_blender_addon/` via Blender Preferences → Add-ons.

- **Preferences:** optional `tools/swg_blender` override, `swg-main` root for shaders
- **Sidebar → SWG:** **Run Export** + Export/Hierarchy sub-panels
- **Export types:** static `.msh`, static bundle, `.mgn`, `.skt`, `.ans` (CKAT/KFAT), creature bundle, building (hierarchy lint + CLI guidance)
- **Flags:** ignore textures, ignore blend targets, pack TRE, author/notes in manifest
- **File → Import/Export:** same operators as quick actions

Environment override: `SWG_BLENDER_ROOT`, `SWG_MAIN`.

## Agent / Cursor

Project skill: `.cursor/skills/swg-blender-pipeline/SKILL.md`

Handoff: `.planning/handoff/swg-blender-import.md`

### Skeletal appearance `.sat` (Phase 9.1)

- `swg_scene/skeletal_appearance.py` - SMAT load/export for versions **0001**, **0002**, **0003**
- Chunks: `INFO`, `MSGN` (mesh generator paths), `SKTI` (skeleton + attachment transform), optional `LATX` (skeleton -> LAT map), `LDTB`/`SFSK`/`APAG`
- Golden roundtrip: `tests/test_skeletal_appearance.py` (`abyssin_m.sat` from swg-main or `tests/golden/`)

### Skeletal bundle + `.sat` (Phase 9.2)

- `export_skeletal_bundle_from_files(..., write_appearance=True)` writes `appearance/{name}.sat` alongside `.mgn`/`.skt`/`.ans`
- SAT references bundle-relative paths (`appearance/mesh/...`, `appearance/skeleton/...`)
- When animations are included, a default LATX entry maps skeleton -> `appearance/lat/{skt_stem}.lat` (override with `skeleton_lat_pairs`)

### Blend targets / shape keys (Phase 9.3)

- swg_scene/blend_targets.py — BLT/BLTS load + write (positions, normals, optional DOT3/HPTS)
- SKMG block order matches Maya/engine: BLTS + occlusion chunks before PSDT
- swg_blender/shape_keys.py — Blender shape keys -> sparse bind-pool deltas (lend_* names)
- Skeletal export auto-includes shape keys when present on the mesh object
- Golden coverage: byssin_m_body_l0.mgn (lend_muscle, lend_fat, lend_skinny)

### SKMG DOT3 (Phase 9.4)

- Global bind-pose DOT3 chunk (count + float4 tangent vectors) after NORM
- Per-shader PSDT DOT3 chunk: int32 index per shader vertex into the global pool
- swg_scene/skmg_dot3.py — load/write helpers + pool dedupe
- Skeletal load resolves indices into a second UV layer (4 floats); export writes pool + indices
- Blender skeletal export reuses compute_dot3_coordinates() when material has DOT3

### SKMG occlusion zones (Phase 9.5)

- `swg_scene/occlusion.py` — OZN/FOZC/OZC/ZTO load + write; OITL per-triangle zone combo indices
- Loader fills `SwgScene.occlusion` and `SwgMesh.occlusion_triangles`; export writes structured chunks (not raw pre-PSDT bytes)
- Golden coverage: `abyssin_m_body_l0.mgn` (15 zones, 14 combinations, 1180 OITL tris)

### Mesh LOD `.lmg` (Phase 9.6)

- `swg_scene/mesh_lod.py` — MLOD 0000 load/write (INFO detail count + NAME paths per level)
- Golden byte roundtrip: `abyssin_m_body.lmg` → l0–l3 `.mgn` paths

### Skeleton LOD SLOD export (Phase 9.7)

- `swg_scene/skeleton_lod_export.py` — write nested SKTM detail levels under SLOD 0000
- `skeleton_lod.load_slod_all_levels()` loads every LOD for roundtrip tests
- Golden semantic roundtrip: `all_b.skt` (38/30/27/21 joints per level)

### Extended SSHT slots + retail import (Phase 10.1)

- `swg_blender/materials.py` — Principled BSDF + `swg_texture_*` props → full slot map (MAIN/SPEC/CNRM/EMIS/ENVM/MASK/HUEB)
- `swg_pipeline/shader_effects.py` — effect-path inference aligned with io_scene_swg / Maya heuristics
- `swg_pipeline/shader_import.py` — retail `.sht` → `ShaderBuildSpec` (TCSS/TFNS/ARVS parse)
- Tests: `test_shader_effects.py`, `test_shader_import.py` (specmap + emismap + alpha retail roundtrips)

### Component & detail appearances (Phase 11)

- `swg_scene/component_appearance.py` — CMPA load/write (versions 0001–0005; export writes 0005)
- `swg_scene/detail_appearance.py` — DTLA load/write (versions 0001–0008; export writes 0008)
- `swg_scene/hierarchy.py` — Maya-style name decode (`mesh0`, `c0`, `l1`, `radar`, …) + lint
- Path helpers: `appearance_component()`, `appearance_lod()` in `swg_pipeline/swg_main_paths.py`
- Add-on: **Validate hierarchy names** (Sidebar → SWG)
- Tests: `tests/test_phase11_appearances.py` (retail `barc_speeder_destroyed_l0.cmp`, `abyssin_m_arms.lod`)

Blender export naming (under selection root): `mesh/` static meshes, `c<N>/` component parts → `.cmp`, `l<N>/` detail LOD bands → `.lod`.

### Extended shader templates (Phase 10.2–10.6)

- `swg_pipeline/shader_extended.py` — CSHD / SWTS / OPST load + write roundtrip
- `swg_scene/light_mesh.py` — per-shader triangle enumeration (MayaLightMeshReader port)
- `swg_scene/per_pixel_lighting.py` — dot3 request + `compute_mesh_dot3()` for materials with `swg_dot3`
- Blender material flags: `swg_customizable`, `swg_animating_texture`, `swg_skin_swap`, `swg_dot3`
- Tests: `test_shader_extended.py`, `test_per_pixel_lighting.py`

### Texture bundle copy (Phase 10.7)

- `swg_pipeline/texture_bundle.py` — dedupe + copy shader texture paths into bundle tree
- `test_texture_bundle.py` — unit test with mocked `serverdata_file` resolution

