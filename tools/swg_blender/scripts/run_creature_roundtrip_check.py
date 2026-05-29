"""Materialize abyssin, Blender import+export, compare all LOD .mgn geometry."""
from __future__ import annotations
import shutil, sys, tempfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from swg_pipeline.parity import compare_scene_geometry
from swg_pipeline.tre_project import export_creature_project, import_creature_project, load_project_manifest
from swg_scene.mesh_skeletal import load_skeletal_mesh

def main() -> int:
    ws = Path(tempfile.mkdtemp(prefix="swg_creature_rt_"))
    backup = ws / "_backup"
    print("workspace:", ws)
    imp = import_creature_project(ws, "appearance/abyssin_m.sat")
    print("materialized:", len(imp.materialized), "missing:", len(imp.missing))
    if imp.missing:
        return 1
    manifest = load_project_manifest(ws)
    backup.mkdir()
    for part in manifest.get("parts") or []:
        rels = list(part.get("lod_mgn_relpaths") or [])
        if not rels:
            lod0 = part.get("lod0_mgn_relpath")
            if lod0:
                rels = [str(lod0)]
        for rel in rels:
            src = ws / str(rel).replace("\\", "/")
            if src.is_file():
                shutil.copy2(src, backup / src.name)
    exp = export_creature_project(ws, use_blender=True)
    print("exported mgn:", len(exp.exported_mgn))
    failed = 0
    checked: set[str] = set()
    for part in manifest.get("parts") or []:
        rels = list(part.get("lod_mgn_relpaths") or [])
        if not rels:
            lod0 = part.get("lod0_mgn_relpath")
            if lod0:
                rels = [str(lod0)]
        for rel in rels:
            rel = str(rel).replace("\\", "/")
            if rel in checked:
                continue
            checked.add(rel)
            name = Path(rel).name
            before, after = backup / name, ws / rel
            if not before.is_file() or not after.is_file():
                print(name, "SKIP")
                continue
            diff = compare_scene_geometry(load_skeletal_mesh(before), load_skeletal_mesh(after))
            if not diff.ok:
                failed += 1
            print(name, "OK" if diff.ok else "FAIL", diff.errors[0] if diff.errors else "match")
    print("summary failed:", failed)
    return 1 if failed else 0
if __name__ == "__main__":
    raise SystemExit(main())

