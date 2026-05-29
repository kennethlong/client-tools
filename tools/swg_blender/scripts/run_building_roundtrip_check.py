"""Materialize echo_base POB workspace, optional Blender round-trip, compare primary .msh geometry."""
from __future__ import annotations

import shutil
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from swg_pipeline.parity import compare_scenes, roundtrip_scene
from swg_pipeline.tre_project import load_project_manifest
from swg_pipeline.tre_project_building import (
    _parse_blender_json,
    _resolve_blender_exe,
    import_building_project,
    run_blender_building_roundtrip,
)
from swg_scene.mesh_static import load_static_mesh


def main() -> int:
    ws = Path(tempfile.mkdtemp(prefix="swg_building_rt_"))
    print("workspace:", ws)
    imp = import_building_project(ws, "appearance/echo_base_pob.pob")
    print("materialized:", imp.as_dict().get("materialized_count"), "missing:", len(imp.missing))
    if imp.missing:
        return 1

    manifest = load_project_manifest(ws)
    cells = manifest.get("cells") or []
    primary = [c["primary_mesh_relpath"] for c in cells if c.get("primary_mesh_relpath")]
    print("cells:", len(cells), "primary_msh:", len(primary))

    backup = ws / "_backup"
    backup.mkdir()
    for rel in primary:
        rel_str = str(rel).replace("\\", "/")
        src = ws / rel_str
        if src.is_file():
            dest = backup / rel_str
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dest)

    failed = 0
    for rel in primary:
        path = ws / str(rel).replace("\\", "/")
        if not path.is_file():
            print(rel, "SKIP missing")
            failed += 1
            continue
        original = load_static_mesh(path)
        rt = roundtrip_scene(path, rebuild_appearance=False, optimize_indices=False)
        diff = compare_scenes(original, rt)
        if not diff.ok:
            failed += 1
            print(path.name, "FAIL", (diff.errors or diff.warnings)[:2])
        else:
            print(path.name, "OK")

    print("python roundtrip failed:", failed)
    if failed:
        return 1

    try:
        blender_exe = _resolve_blender_exe()
    except Exception:
        print(
            "Blender not configured; skip headless import/export. "
            "Set BLENDER_EXE or install Blender (see tools/swg_blender/PIPELINE.md)."
        )
        return 0
    print("blender:", blender_exe)

    proc = run_blender_building_roundtrip(ws)
    if proc.returncode != 0:
        print((proc.stderr or proc.stdout or "")[-2000:], file=sys.stderr)
        return proc.returncode
    payload = _parse_blender_json(proc.stdout or "")
    exported = list(payload.get("exported_msh") or [])
    imported = list((payload.get("import") or {}).get("imported") or [])
    print("blender imported:", len(imported), "exported:", len(exported))
    if len(exported) != len(primary):
        print("FAIL export count", len(exported), "!= primary", len(primary))
        return 1

    post_fail = 0
    for rel in primary:
        rel_str = str(rel).replace("\\", "/")
        after_path = ws / rel_str
        before_path = backup / rel_str
        if not after_path.is_file() or not before_path.is_file():
            post_fail += 1
            continue
        diff = compare_scenes(load_static_mesh(before_path), load_static_mesh(after_path))
        if not diff.ok:
            post_fail += 1
            print(Path(rel_str).name, "blender FAIL", (diff.errors or [])[:2])
    print("blender geometry failed:", post_fail)
    return 1 if post_fail else 0


if __name__ == "__main__":
    raise SystemExit(main())
