"""Per-file static import/export diag (Blender)."""
from __future__ import annotations
import json, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

def main() -> int:
    import bpy
    from swg_blender.export_static import export_static_mesh
    from swg_blender.import_static import import_static_mesh
    from swg_pipeline.parity import compare_scenes
    from swg_scene.mesh_static import load_static_mesh
    path = Path(sys.argv[sys.argv.index("--") + 1]).resolve()
    before = load_static_mesh(path)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    objs = import_static_mesh(path)
    tris = sum(sum(1 for p in o.data.polygons if len(p.vertices)==3) for o in objs)
    out = path.parent / ("_dbg_" + path.name)
    export_static_mesh(out, objects=objs)
    after = load_static_mesh(out)
    diff = compare_scenes(before, after)
    print(json.dumps({"file": path.name, "import_tris": tris, "compare_ok": diff.ok, "errors": diff.errors}))
    return 0 if diff.ok else 1

if __name__ == "__main__":
    raise SystemExit(main())