---
name: swg-blender-pipeline
description: >-
  Import/export Star Wars Galaxies IFF assets via tools/swg_blender, Blender
  add-on, client test bundles, and TreeFile rsp manifests. Use when working on
  SWG meshes, animations, Blender MCP, export bundles, or client spawn validation.
---

# SWG Blender pipeline

## Startup

1. Read `.planning/handoff/swg-blender-import.md` if resuming prior work.
2. Read `AGENTS.md` gotchas (UTF-16 corruption on `tools/swg_blender/**/*.py`).
3. Run `cd tools/swg_blender && python -m pytest tests/ -q` before claiming done.

## Rules

- **Python encoding:** Do **not** use Cursor **`Write`** on `tools/swg_blender/**/*.py` — it can save UTF-16 on Windows (pytest: "null bytes"). Use **`StrReplace`** for edits, or PowerShell/Python UTF-8 writes for new files (see `AGENTS.md` → *Windows UTF-16 corruption*).
- **Do not** bake +90° X rotation into vertex coords; object rotation in `import_static.py` only.
- **CKAT** is default for `.ans` export; pass `format="kfat"` for KFAT round-trips.
- **TRE:** `tre_reader.py` supports `0004`/`0005`/`6000` listing; Restoration payloads may need `TreeFileExtractor` for byte extract.

## Blender add-on (Phase 15)

Install `tools/swg_blender/swg_blender_addon/` → Sidebar **SWG** tab.

| Panel | Purpose |
| --- | --- |
| **Export** | Type: static / creature bundle / mgn / skt / anim / building; author; CKAT vs KFAT; ignore textures/blend targets; pack TRE |
| **Hierarchy** | Lint Maya names (`mesh0`, `c0`, `pob`, `portals/`, …) |
| **Run Export** | Dispatches from scene `swg_export` settings |

Creature bundle: select skinned mesh + armature → **Creature bundle** or Export type **Creature bundle** → **Run Export**.

## Common tasks

| Task | Command / module |
| --- | --- |
| Dump IFF tree | `python -m swg_iff.dump file.msh` |
| Import static (MCP/Blender) | `swg_blender.import_static.import_static_mesh(path)` |
| Export client bundle | `python -m swg_pipeline.export_bundle static --mesh ... --output-dir ...` |
| Creature bundle (CLI) | `python -m swg_pipeline.batch_export skeletal --mgn ... --skt ... -outputdir ...` |
| Pack bundle → TRE | `python -m swg_pipeline.pack_pipeline D:\swg_dev_bundle --rebuild-rsp` |
| Build rsp files | `python -m swg_pipeline.rsp_builder <roots> --output-dir ...` |
| List TRE contents | `python -m swg_pipeline.tre_list -l file.tre` |
| Master TOC summary | `python -m swg_pipeline.tre_list --master file.toc` |
| Validate spawn (human E2E) | `tools/swg_blender/HUMAN_E2E_TEST_GUIDE.md` |
| Validate spawn (technical) | `tools/swg_blender/CLIENT_SPAWN_CHECKLIST.md` |

## Blender MCP snippet

```python
import sys
sys.path.insert(0, r"D:\Code\swg-client-v2\tools\swg_blender")
from swg_blender.import_static import import_static_mesh
import_static_mesh(r"path\to\mesh.msh")
```

## References

- `tools/swg_blender/PIPELINE.md` — CLI and rsp workflow
- `docs/research/blender-iff-interchange-PLAN.md` — phase roadmap
- `docs/research/maya-exporter-parity-checklist.md` — format parity