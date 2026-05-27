# Handoff: SWG Blender IFF pipeline

**Updated:** 2026-05-26 (Phase 15 — add-on UX polish)
**Branch:** `koogie-msvc-cpp20-base`
**Worktree:** `D:\Code\swg-client-v2`
**Status:** Phases 0–15 + **Phase 7 client validation** done · **Next:** parity gaps / SAT bundle / commit E2E work.

---

## Pick up here

1. Read `AGENTS.md` (UTF-16 gotcha).
2. Read this file.
3. `cd tools/swg_blender; python -m pytest tests/ -q` — **164+ passed** (ignore `test_skeletal_bpy_export` without bpy).
4. Phase 7 closed — see `tools/swg_blender/PHASE7_VALIDATION.md`.

---

## Phase 15 (done)

| Task | Module |
| --- | --- |
| P15.1 Export type panel | `swg_blender_addon/properties.py`, `panel.py` |
| P15.2 CKAT/KFAT toggle | scene `anim_format` |
| P15.3 Ignore textures / blend targets | scene flags → bundle + `export_skeletal` |
| P15.4 Author + notes | `export_manifest.apply_manifest_author_notes` |
| P15.5 Hierarchy panel | `SWG_PT_hierarchy` + `swg.validate_hierarchy` |
| P15.6 Skill/docs | `.cursor/skills/swg-blender-pipeline/SKILL.md` |

Creature bundle: `swg.export_creature_bundle` or **Run Export** with type Creature bundle.

## Phase 14 (done)

| Task | Module |
| --- | --- |
| P14.1 TreeFileBuilder wrapper | `swg_pipeline/tre_tools.py` (`pack`/`extract`/`list`) |
| P14.2 Bundle → rsp → tre | `swg_pipeline/pack_pipeline.py` |
| P14.3 v6000 decrypt notes | `swg_pipeline/tre_decrypt.py` |
| P14.4 TRE tags 0006/5000 | `tre_reader.py` (32-byte TOC stride) |
| P14.5 Export manifest | `swg_pipeline/export_manifest.py` (`pipeline_version` 3.009) |
| P14.6 Batch CLI | `swg_pipeline/batch_export.py` (`-node`, `-outputdir`, `-author`, `-silent`) |
| P14.7 `--pack-tre` on export_bundle | `export_bundle.py` |

Pack tests skip without built `TreeFileBuilder.exe`.

## Phase 13 (done)

| Task | Module |
| --- | --- |
| P13.1 PRTO load/write | `swg_scene/portal_property.py` |
| P13.2 Portal/cell graph (raw preserve) | same (PRTS IDTL + CELS) |
| P13.3 APT redirect | `swg_scene/appearance_redirect.py` |
| P13.4 Export helpers | `swg_scene/building_export.py` |
| P13.5 Path helpers | `appearance_pob`, `appearance_apt` in `swg_main_paths.py` |
| P13.6 Hierarchy | `pob`, `building`, `portal`, `cells` in `hierarchy.py` |
| P13.7 Tests | `tests/test_phase13_building.py` |

Retail roundtrip: `appearance/echo_base_pob.pob` (byte-identical), `appearance/abyssin_m_arms.apt`.

## Phase 12 (done)

| Task | Module |
| --- | --- |
| P12.1 Extents load/write | `swg_scene/extents.py` |
| P12.2 IDTL (CMSH) | `swg_scene/indexed_triangle_list.py` |
| P12.3 FLOR/0006 floor mesh | `swg_scene/floor_mesh.py` |
| P12.4 Path helper | `swg_pipeline/swg_main_paths.py` (`appearance_collision_floor`) |
| P12.5 Hierarchy primitives | `swg_scene/hierarchy.py` |
| P12.6 Tests | `tests/test_phase12_collision.py` |

Retail roundtrip: `appearance/collision/aurilia_ground_hut_s01_collision_floor.flr` (byte-identical).

## Phase 11 (done)

| Task | Module |
| --- | --- |
| P11.1–P11.2 CMPA load/write + parts | `swg_scene/component_appearance.py` |
| P11.3 RADR placeholder on write | same (preserve `radar_raw` on roundtrip) |
| P11.4–P11.7 DTLA load/write + PIVT/LOD bands | `swg_scene/detail_appearance.py` |
| P11.8 Hierarchy decode/lint | `swg_scene/hierarchy.py` |
| P11.9 Golden tests | `test_phase11_appearances.py` (swg-main) |
| P11.10 Add-on validator | `swg.validate_hierarchy` operator |

Retail roundtrips: `appearance/component/barc_speeder_destroyed_l0.cmp`, `appearance/lod/abyssin_m_arms.lod`.

---

## Pipeline status

| Phase | Status |
| --- | --- |
| 0–15 | Done |
| 7 (client gate) | Done (2026-05-26) |

---

## Test status

171 passed (`pytest tests/`, includes Blender E2E when Blender on PATH)

---

## Next work

1. **Commit** — E2E (`e2e_blender_roundtrip.py`, `bpy_geometry.py`), `HUMAN_E2E_TEST_GUIDE.md`, export matrix_world fix.
2. **Optional** — Phase 7 anim play (`.ans` on humanoid); SAT single-export creature bundle.
3. **Parity** — byte-compare vs MayaExporter where still 🟡 in `maya-exporter-parity-checklist.md`.

---

## Gotchas

See `AGENTS.md`.