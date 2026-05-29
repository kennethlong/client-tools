# Handoff: SWG Blender IFF pipeline

**Updated:** 2026-05-29 (M16 tre_project ready to commit; roundtrip smoke green)
**Branch:** koogie-msvc-cpp20-base (ahead 63, **swg_blender WIP staged for commit**)
**Worktree:** D:\Code\swg-client-v2
**Status:** Phases 0–15 + Phase 7 done · Creature **io-parity compare validated** (abyssin_m) · **M16 tre_project roundtrip smoke PASS** (creature + building Blender) · MCP **blender-mcp 1.5.6** / Blender **5.1.2**

---

## Pick up here (restart)

1. Read AGENTS.md — **never use Cursor Write on `tools/swg_blender/**/*.py` or `.planning/handoff/*.md`** (UTF-16 corruption on Windows).
2. Read this file; if resuming D3D11 char-select work read [phase-17-char-select.md](phase-17-char-select.md).
3. `cd tools/swg_blender; python -m pytest tests/ -q` — last run **191 passed, 8 skipped** (~165s).
4. **Roundtrip smoke (host, 2026-05-29):**
   - `python scripts/run_creature_roundtrip_check.py` — 12/12 LOD `.mgn` OK
   - `python scripts/run_building_roundtrip_check.py` — 41/41 `.msh` Python + Blender OK (~45s)
5. **M16 modules** (commit as one slice):
   - `swg_pipeline/creature_graph.py`, `building_graph.py`, `tre_project*.py` — workspace materialize + export
   - Addon **Creature project** / **Building project** panels + operators in `swg_blender_addon/`
   - `load_skeletal_mesh_io_parity()` + compare scripts under `tools/swg_blender/scripts/`
   - Spec: `docs/research/tre-project-roundtrip-SPEC.md`
5. **Creature compare** (visual parity validated):
   - Workspace: `C:\Users\kenne\AppData\Local\Temp\swg_creature_compare_oubiscla`
   - Script: `tools/swg_blender/scripts/compare_creature_io_scene_bpy.py`
   - io_scene (patched): `D:\Code\io_scene_swg_msh\io_scene_swg\`
   - User confirmed **matching** after NIDX fix + Y-offset + zero blend shapes
6. **MCP:** Cursor mcp.json uses `cmd /c uvx blender-mcp` + DISABLE_TELEMETRY. PyPI **1.5.6**. Chunk heavy imports — do not one-shot full runpy over MCP.

---

## Session 2026-05-29 — M16 TRE project (WIP, uncommitted)

### Goal

Vertical slice: extract creature/building DAG from TRE → Blender workspace (`swg_tre_project.json`) → edit → export back → optional pack.

### New modules (untracked / modified)

| Area | Files |
| --- | --- |
| Creature graph | `swg_pipeline/creature_graph.py`, `tre_project.py`, `tre_project_bpy.py` |
| Building graph | `swg_pipeline/building_graph.py`, `tre_project_building.py`, `tre_project_bpy_building.py` |
| Floor/portal import | `swg_blender/import_floor.py`, `import_portal.py`, `export_floor.py`, `export_portal.py` |
| POB cells | `swg_scene/pob_cells.py` |
| Addon UI | `operators.py` — `SWG_OT_import/export_creature_project`, building project ops |
| Tests | `test_creature_graph.py`, `test_building_graph.py`, `test_tre_project.py`, `test_skeletal_io_scene_parity.py` |
| Smoke scripts | `scripts/run_creature_roundtrip_check.py`, `run_building_roundtrip_check.py`, `run_creature_client_smoke.py` |

### CLI (from SKILL.md)

```powershell
python -m swg_pipeline.tre_project import-creature --workspace W --sat appearance/abyssin_m.sat [--tre X.tre] [--blender]
python -m swg_pipeline.tre_project export-creature --workspace W [--blender] [--rebuild-rsp]
python -m swg_pipeline.tre_project import-building --workspace W --pob appearance/echo_base_pob.pob [--blender]
```

Golden refs: `appearance/abyssin_m.sat` (creature), `appearance/echo_base_pob.pob` (building).

### Next for M16

1. ~~Commit once roundtrip smoke passes~~ **done** (2026-05-29).
2. **M16 client smoke** — pack creature TRE, load in client (Blender geometry parity already validated).
3. Decide addon **creature import** UX: SAT+rotation vs io-parity MGN path (`docs/research/sat-lmg-ux-comparison.md`).
4. **`rebuild_appearance=True` hang** on some building `.msh` (e.g. `echo_base_pob_r11_r11`) — building Blender export uses `rebuild_appearance=False` to match parity tests; root cause in appearance rebuild still open.

---

## Session 2026-05-29 — abyssin_m compare (swg_blender vs io_scene_swg_msh)

### Default compare behavior

| Flag | Meaning |
| --- | --- |
| *(default)* | **io-parity MGN** — 3 LOD0 files (`abyssin_m_l0`, `body_l0`, `arms_l0`) |
| `--use-sat-import` | Old full SAT + armature path |
| `--torso-only` | Only `abyssin_m_body_l0.mgn` |
| `--offset 2.5` | **Y-axis** separation (swg at -Y, io at +Y) |

Background (stable, no MCP):

```powershell
blender -b --python D:\Code\swg-client-v2\tools\swg_blender\scripts\compare_creature_io_scene_bpy.py -- --workspace C:\Users\kenne\AppData\Local\Temp\swg_creature_compare_oubiscla --io-scene-dir D:\Code\io_scene_swg_msh\io_scene_swg --offset 2.5
```

### Root causes fixed

| Symptom | Fix |
| --- | --- |
| Opposite facing | Offset on **Y** not X |
| io "fat" belly | `_zero_blend_shape_keys()` on io side |
| Geometry mismatch | Default **io-parity** loader (not SAT) |
| Splotchy swg shading | **NIDX** in `load_skeletal_mesh_io_parity()` |
| MCP disconnect | Chunked MCP or `blender -b` |

### Verification

Per-mesh evaluated vertex delta = **0.0** for `abyssin_m_l0`, `abyssin_m_body_l0`, `abyssin_m_arms_l0`.

### Code touched

| File | Change |
| --- | --- |
| `scripts/compare_creature_io_scene_bpy.py` | io-parity default, Y offset, neutral mats, zero blends |
| `swg_scene/mesh_skeletal.py` | `load_skeletal_mesh_io_parity()`, IoParityFace + NIDX |

---

## Blender MCP stability

| Item | Detail |
| --- | --- |
| **PyPI** | blender-mcp **1.5.6** |
| **Cursor config** | `C:\Users\kenne\.cursor\mcp.json` — `cmd /c uvx blender-mcp`, DISABLE_TELEMETRY=true |
| **Blender** | **5.1.2** |
| **Rule** | Only one MCP instance; chunk heavy imports |

---

## Pipeline status

| Phase | Status |
| --- | --- |
| 0–15 | Done |
| 7 (client gate) | Done |
| M16 tre_project | **Roundtrip smoke PASS** — client smoke next |

---

## Test status

Run: `cd tools/swg_blender; python -m pytest tests/ -q`

**191 passed, 8 skipped** (2026-05-29). Skips: pack tests without TreeFileBuilder, swg-main retail paths without SWG_MAIN.

---

## Gotchas

See AGENTS.md (UTF-16, VTXA tag, use_auto_smooth removed in Blender 4+, import rotation on **object** not vertices).