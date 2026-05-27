# SWG Blender pipeline (`tools/swg_blender`)

Python IFF/TRE tooling and Blender import/export for Star Wars Galaxies assets.

**Full workflow:** [PIPELINE.md](PIPELINE.md) · **Client testing:** [CLIENT_SPAWN_CHECKLIST.md](CLIENT_SPAWN_CHECKLIST.md)

## Quick start

```powershell
cd tools/swg_blender
python -m pytest tests/ -q
python -m swg_iff.dump path/to/file.msh
python -m swg_blender.import_static tests/golden/frn_all_bed_sm_s1_l0.msh --dry-run
```

## Phase 5 — pipeline integration

| Component | Role |
| --- | --- |
| `swg_pipeline/rsp_builder.py` | `data_*.rsp` manifests (TreeFileRspBuilder format) |
| `swg_pipeline/export_bundle.py` | Client test bundle + rsp + `client_search_paths.cfg` |
| `swg_blender_addon/` | Blender File menu + View3D sidebar (static/skeletal/animation) |
| `.cursor/skills/swg-blender-pipeline/` | Cursor agent skill for this workstream |

### Blender add-on

1. Preferences → Add-ons → Install → select `swg_blender_addon/`
2. Enable **SWG IFF Pipeline** (v0.2)
3. Optional prefs: pipeline root override, swg-main root (`SWG_MAIN`)
4. **File → Import/Export** or **View3D → Sidebar → SWG**

### RSP + bundle CLI

```powershell
python -m swg_pipeline.export_bundle --mesh tests/golden/frn_all_bed_sm_s1_l0.msh --output-dir D:\swg_dev_bundle
python -m swg_pipeline.rsp_builder D:\swg_dev_bundle --output-dir D:\swg_dev_bundle\rsp
```

Flags: `--no-rsp`, `--no-client-cfg`, `--no-copy-textures`

## Phase 1 — static mesh import

| Module | Role |
| --- | --- |
| `swg_scene/mesh_static.py` | `.msh` → `SwgScene` (SPS/VTXA/INDX) |
| `swg_scene/vertex_buffer.py` | VTXA 0003 decode |
| `swg_scene/coords.py` | Engine ↔ Blender X flip |
| `swg_blender/import_static.py` | Scene IR → bpy mesh + import rotation |

Engine positions are X-flipped to match MayaExporter. Static imports also set object rotation to `(90°, 0°, 0°)` — the same fix validated manually on furniture props.

## Blender MCP import

```python
import sys
sys.path.insert(0, r"D:\Code\swg-client-v2\tools\swg_blender")
from swg_blender.import_static import import_static_mesh
import_static_mesh(r"path\to\mesh.msh")
```

Golden fixture: `tests/golden/frn_all_bed_sm_s1_l0.msh` (139 verts, 201 tris).

See `docs/research/blender-iff-interchange-PLAN.md` for the full roadmap.