"""Headless Blender import -> export round-trip (run inside ``blender -b --python``)."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any


def _ensure_repo_on_path() -> Path:
    root = Path(__file__).resolve().parents[1]
    root_str = str(root)
    if root_str not in sys.path:
        sys.path.insert(0, root_str)
    return root


def _clear_scene() -> None:
    import bpy

    bpy.ops.wm.read_factory_settings(use_empty=True)


def _result(ok: bool, **payload: Any) -> int:
    print(json.dumps({"ok": ok, **payload}))
    return 0 if ok else 1


def _compare_static(source: Path, exported_scene) -> dict[str, Any]:
    from swg_pipeline.parity import compare_scene_geometry
    from swg_scene.mesh_static import load_static_mesh

    reference = load_static_mesh(source)
    diff = compare_scene_geometry(reference, exported_scene)
    return {
        "ok": diff.ok,
        "errors": diff.errors,
        "warnings": diff.warnings,
        "stats": diff.stats,
    }


def _compare_skeletal(source: Path, exported_scene) -> dict[str, Any]:
    from swg_pipeline.parity import compare_scene_geometry
    from swg_scene.mesh_skeletal import load_skeletal_mesh
    from swg_scene.model import SwgScene

    reference = load_skeletal_mesh(source)
    # .mgn IR does not embed skeleton joints; compare mesh geometry only.
    ref_mesh = SwgScene(meshes=reference.meshes)
    cand_mesh = SwgScene(meshes=exported_scene.meshes)
    diff = compare_scene_geometry(ref_mesh, cand_mesh)
    for mi, (before, after) in enumerate(
        zip(reference.meshes, exported_scene.meshes, strict=True)
    ):
        if len(before.bone_weights) != len(after.bone_weights):
            diff.ok = False
            diff.errors.append(
                f"mesh[{mi}]: bone_weights vertex count {len(before.bone_weights)} != {len(after.bone_weights)}"
            )
        if before.transform_names != after.transform_names:
            diff.ok = False
            diff.errors.append(f"mesh[{mi}]: transform_names differ")
    return {
        "errors": diff.errors,
        "warnings": diff.warnings,
        "stats": diff.stats,
        "ok": diff.ok,
    }


def run_static(source: Path, out: Path) -> int:
    import bpy

    from swg_blender.export_static import mesh_object_to_swg_mesh
    from swg_blender.import_static import import_static_mesh
    from swg_scene.mesh_static import load_static_mesh
    from swg_scene.mesh_static_export import write_static_mesh_file
    from swg_scene.model import SwgScene

    _clear_scene()
    objs = import_static_mesh(source)
    mesh_objs = [o for o in objs if o.type == "MESH"]
    if not mesh_objs:
        return _result(False, kind="static", error="no mesh objects imported")

    meshes = [mesh_object_to_swg_mesh(o) for o in mesh_objs]
    scene = SwgScene(
        meshes=meshes,
        rebuild_appearance=False,
        optimize_indices=False,
    )
    write_static_mesh_file(out, scene)
    exported = load_static_mesh(out)
    cmp = _compare_static(source, exported)
    ok = bool(cmp.pop("ok", False))
    return _result(ok, kind="static", source=str(source), output=str(out), **cmp)


def run_skeletal(source: Path, out: Path, skt: Path | None) -> int:
    import bpy

    from swg_blender.export_skeletal import export_skeletal_mesh
    from swg_blender.import_skeletal import import_skeletal_mesh
    from swg_scene.mesh_skeletal import load_skeletal_mesh

    _clear_scene()
    objs = import_skeletal_mesh(source, skt_path=skt)
    mesh_objs = [o for o in objs if o.type == "MESH"]
    arm_objs = [o for o in objs if o.type == "ARMATURE"]
    if not mesh_objs:
        return _result(False, kind="skeletal", error="no mesh objects imported")

    arm = arm_objs[0] if arm_objs else None
    skel_templates = None
    ref = load_skeletal_mesh(source)
    if ref.meshes and ref.meshes[0].skeleton_template_names:
        skel_templates = list(ref.meshes[0].skeleton_template_names)

    exported = export_skeletal_mesh(
        out,
        objects=mesh_objs,
        armature=arm,
        skeleton_template_names=skel_templates,
        ignore_blend_targets=True,
    )
    cmp = _compare_skeletal(source, exported)
    ok = bool(cmp.pop("ok", False))
    return _result(ok, kind="skeletal", source=str(source), output=str(out), **cmp)


def _script_argv() -> list[str]:
    """Arguments after Blender's ``--python script.py --`` separator."""
    if "--" in sys.argv:
        return sys.argv[sys.argv.index("--") + 1 :]
    argv = sys.argv[1:]
    if argv and argv[0].endswith(".py"):
        return argv[1:]
    return argv


def main(argv: list[str] | None = None) -> int:
    _ensure_repo_on_path()
    if argv is None:
        argv = _script_argv()
    parser = argparse.ArgumentParser(description="SWG Blender E2E round-trip")
    parser.add_argument("kind", choices=("static", "skeletal"))
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--skt", type=Path, default=None)
    args = parser.parse_args(argv)

    if args.kind == "static":
        return run_static(args.source, args.output)
    return run_skeletal(args.source, args.output, args.skt)


if __name__ == "__main__":
    raise SystemExit(main())
