# Handoff: SWG Blender IFF pipeline

**Updated:** 2026-05-26 (Phase 11 — CMPA/DTLA + hierarchy lint)
**Branch:** `koogie-msvc-cpp20-base`
**Worktree:** `D:\Code\swg-client-v2`
**Status:** Phases 0–6, 8–11 done · Phase 7 client spawn user gate · **Next:** Phase 12 or Phase 7 validation.

---

## Pick up here

1. Read `AGENTS.md` (UTF-16 gotcha).
2. Read this file.
3. `cd tools/swg_blender; python -m pytest tests/ -q` — **150 passed** (ignore `test_skeletal_bpy_export` without bpy).
4. Plan: Phase 12 = collision/floor/extents.

---

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
| 0–11 | Done (7 = user client gate) |
| 12–15 | Not started |

---

## Test status

150 passed (`pytest tests/`)

---

## Next work

**A.** Phase 12 collision/floor/extents

**B.** Phase 7 in-game validation

**C.** Phase 13 POB/building

---

## Gotchas

See `AGENTS.md`.