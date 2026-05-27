# Handoff: SWG Blender IFF pipeline

**Updated:** 2026-05-26 (Phase 10 complete — CSHD/SWTS/OPST + PPL/light mesh)
**Branch:** `koogie-msvc-cpp20-base`
**Worktree:** `D:\Code\swg-client-v2`
**Status:** Phases 0–6 done · Phase 7 pytest done (client spawn: user) · Phase 8 done · Phase 9 done · **Phase 10 done**. **Next:** Phase 7 in-game validation or Phase 11+.

---

## Pick up here

1. Read `AGENTS.md` (session startup + **UTF-16 gotcha**).
2. Read this file.
3. Verify: `cd D:\Code\swg-client-v2\tools\swg_blender; python -m pytest tests/ -q` — expect **146+ passed** (3 bpy skeletal tests may skip/fail without Blender).
4. Read plan: `docs/research/blender-iff-interchange-PLAN.md` (Phase 11 = CMPA/DTLA).
5. CLI / workflow: `tools/swg_blender/PIPELINE.md`.

Pipeline code lives under `tools/swg_blender/` (**untracked in git** as of this handoff).

---

## Phase 10 (done)

| Task | Module | Tests |
| --- | --- | --- |
| P10.1 SSHT slots | `shader_effects.py`, `shader_import.py`, `materials.py` | `test_shader_effects.py`, `test_shader_import.py` |
| P10.2 CSHD | `shader_extended.py` | retail `abyssin_body.sht` roundtrip |
| P10.3 SWTS | `shader_extended.py` | `anim_bank_side_screen.sht` roundtrip |
| P10.4 OPST | `shader_extended.py` | `arms.sht` roundtrip |
| P10.5 PPL | `per_pixel_lighting.py`, `export_static.py` | `test_per_pixel_lighting.py` |
| P10.6 light mesh | `light_mesh.py` | `test_per_pixel_lighting.py` |
| P10.7 textures | `texture_bundle.py` | `test_texture_bundle.py` |

---

## Pipeline status

| Phase | Status |
| --- | --- |
| 0–6, 8–10 | Done |
| 7 | pytest done; client spawn user gate |
| 11–15 | Not started |

---

## Test status

146 passed (`pytest tests/`); 3 `test_skeletal_bpy_export` failures when bpy mock incomplete.

---

## Next work

**A.** Phase 7 in-game validation (`export_bundle phase7`)

**B.** Phase 11 CMPA/DTLA wearables

**C.** Phase 12+ collision/floor/building

---

## Gotchas

See `AGENTS.md` (UTF-16, VTXA, import rotation).
