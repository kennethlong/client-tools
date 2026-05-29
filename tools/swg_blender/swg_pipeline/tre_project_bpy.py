"""Headless Blender: import a materialized creature TRE workspace (M16.3)."""

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


def _get_or_create_collection(name: str) -> Any:
    import bpy

    coll = bpy.data.collections.get(name)
    if coll is None:
        coll = bpy.data.collections.new(name)
        bpy.context.scene.collection.children.link(coll)
    return coll


def _child_collection(parent: Any, name: str) -> Any:
    import bpy

    coll = bpy.data.collections.get(name)
    if coll is None:
        coll = bpy.data.collections.new(name)
        parent.children.link(coll)
    return coll


def _lmg_collection_name(lmg_rel: str, part_index: int) -> str:
    if lmg_rel:
        return Path(lmg_rel.replace("\\", "/")).name
    return f"part_{part_index}"


def _primary_skeleton_relpath(manifest: dict[str, Any]) -> str | None:
    graph = manifest.get("graph") or {}
    bindings = graph.get("skeleton_paths") or []
    if bindings:
        return str(bindings[0])
    parts = manifest.get("parts") or []
    for part in parts:
        sk = part.get("skeleton_relpath")
        if sk:
            return str(sk)
    return None


def import_creature_from_workspace(workspace: str | Path, *, build_skeleton: bool = True) -> dict[str, Any]:
    """Build ``SWG_Creature_<sat>`` collection from ``swg_tre_project.json``."""
    import bpy
    from swg_blender.import_skeletal import import_skeletal_mesh
    from swg_pipeline.tre_project import load_project_manifest

    workspace = Path(workspace).resolve()
    manifest = load_project_manifest(workspace)
    root_sat = str(manifest["root_sat"])
    sat_stem = Path(root_sat).stem
    coll_name = f"SWG_Creature_{sat_stem}"
    collection = _get_or_create_collection(coll_name)

    sat_root = bpy.data.objects.new("sat_root", None)
    sat_root.empty_display_type = "PLAIN_AXES"
    collection.objects.link(sat_root)
    if sat_root.name in bpy.context.scene.collection.objects:
        bpy.context.scene.collection.objects.unlink(sat_root)
    sat_root.select_set(True)
    bpy.context.view_layer.objects.active = sat_root
    sat_root["swg_kind"] = "sat"
    sat_root["swg_tre_relpath"] = root_sat

    skel_rel = _primary_skeleton_relpath(manifest)
    skel_path = workspace / skel_rel.replace("\\", "/") if skel_rel else None

    arm_obj = None
    imported: list[str] = []

    from swg_scene.mesh_lod import load_mesh_lod

    for part in manifest.get("parts") or []:
        lmg_rel = str(part.get("lmg_relpath") or "")
        lmg_path = workspace / lmg_rel.replace("\\", "/") if lmg_rel else None
        mgn_rels: list[str] = list(part.get("lod_mgn_relpaths") or [])
        if not mgn_rels and lmg_path and lmg_path.is_file():
            mgn_rels = [
                str(p).replace("\\", "/") for p in load_mesh_lod(lmg_path).level_paths
            ]
        if not mgn_rels:
            lod0 = part.get("lod0_mgn_relpath")
            if lod0:
                mgn_rels = [str(lod0)]

        base_name = str(part.get("object_name") or Path(lmg_rel).stem or "part")
        part_index = int(part.get("part_index", 0))
        lmg_name = _lmg_collection_name(lmg_rel, part_index)
        lmg_coll = _child_collection(collection, lmg_name)
        lmg_coll["swg_lmg_relpath"] = lmg_rel
        lmg_coll["swg_part_index"] = part_index
        lods_coll = _child_collection(lmg_coll, "LODs")

        for lod_index, mgn_rel in enumerate(mgn_rels):
            mgn_path = workspace / str(mgn_rel).replace("\\", "/")
            if not mgn_path.is_file():
                continue
            object_name = base_name if lod_index == 0 else mgn_path.stem
            mesh_coll = lmg_coll if lod_index == 0 else lods_coll
            objs = import_skeletal_mesh(
                mgn_path,
                skt_path=skel_path if (arm_obj is None and build_skeleton) else None,
                collection=mesh_coll,
                object_name=object_name,
                existing_armature=arm_obj,
            )
            for obj in objs:
                if obj.type == "ARMATURE" and arm_obj is None:
                    arm_obj = obj
                    if obj.name in mesh_coll.objects:
                        mesh_coll.objects.unlink(obj)
                    if obj.name not in collection.objects:
                        collection.objects.link(obj)
                    obj["swg_tre_relpath"] = skel_rel or ""
                    obj["swg_kind"] = "skeleton"
                elif obj.type == "MESH":
                    obj["swg_tre_relpath"] = str(mgn_rel)
                    obj["swg_lmg_relpath"] = lmg_rel
                    obj["swg_kind"] = "creature_part"
                    obj["swg_part_index"] = part_index
                    obj["swg_lod_index"] = lod_index
                    if lod_index > 0:
                        obj.hide_viewport = True
                        obj.hide_render = True
                    imported.append(obj.name)

    return {
        "ok": bool(imported),
        "collection": coll_name,
        "parts": imported,
        "skeleton": arm_obj.name if arm_obj else None,
    }


def export_creature_from_workspace(
    workspace: str | Path, *, ignore_blend_targets: bool = False
) -> dict[str, Any]:
    """Write .mgn/.skt from SWG_Creature_* collection using swg_tre_relpath props."""
    import bpy
    from swg_blender.export_skeletal import export_skeleton, export_skeletal_mesh
    from swg_pipeline.tre_project import load_project_manifest

    workspace = Path(workspace).resolve()
    manifest = load_project_manifest(workspace)
    sat_stem = Path(str(manifest["root_sat"])).stem
    coll_name = f"SWG_Creature_{sat_stem}"
    collection = bpy.data.collections.get(coll_name)
    if collection is None:
        raise RuntimeError(f"collection not found: {coll_name}")

    skel_rel = _primary_skeleton_relpath(manifest)
    arm_obj = None
    for obj in collection.all_objects:
        if obj.type == "ARMATURE":
            arm_obj = obj
            break

    exported_mgn: list[str] = []
    if arm_obj is not None and skel_rel:
        skel_path = workspace / skel_rel.replace("\\", "/")
        skel_path.parent.mkdir(parents=True, exist_ok=True)
        export_skeleton(skel_path, armature=arm_obj)
        arm_obj["swg_tre_relpath"] = skel_rel

    by_relpath: dict[str, list[Any]] = {}
    for obj in collection.all_objects:
        if obj.type != "MESH":
            continue
        if obj.get("swg_kind") != "creature_part":
            continue
        relpath = obj.get("swg_tre_relpath")
        if not relpath:
            continue
        by_relpath.setdefault(str(relpath), []).append(obj)

    for relpath, objs in sorted(by_relpath.items()):
        objs.sort(key=lambda o: int(o.get("swg_submesh_index", 0)))
        out_path = workspace / relpath.replace("\\", "/")
        out_path.parent.mkdir(parents=True, exist_ok=True)
        export_skeletal_mesh(
            out_path,
            objects=objs,
            armature=arm_obj,
            ignore_blend_targets=ignore_blend_targets,
        )
        exported_mgn.append(relpath)

    return {
        "ok": bool(exported_mgn),
        "collection": coll_name,
        "exported_mgn": exported_mgn,
        "skeleton_relpath": skel_rel,
    }


def _script_argv() -> list[str]:
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
    parser = argparse.ArgumentParser(
        description="Import/export creature TRE workspace in Blender"
    )
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--export", action="store_true")
    parser.add_argument("--ignore-blend-targets", action="store_true")
    parser.add_argument(
        "--roundtrip",
        action="store_true",
        help="import then export in one Blender session",
    )
    args = parser.parse_args(argv)

    _clear_scene()
    try:
        if args.roundtrip:
            imp = import_creature_from_workspace(args.workspace)
            result = export_creature_from_workspace(
                args.workspace,
                ignore_blend_targets=args.ignore_blend_targets,
            )
            print(json.dumps({"import": imp, **result}))
            return 0 if result.get("ok") else 1
        if args.export:
            result = export_creature_from_workspace(
                args.workspace,
                ignore_blend_targets=args.ignore_blend_targets,
            )
        else:
            result = import_creature_from_workspace(args.workspace)
    except Exception as exc:
        print(json.dumps({"ok": False, "error": str(exc)}))
        return 1

    print(json.dumps(result))
    return 0 if result.get("ok") else 1


if __name__ == "__main__":
    raise SystemExit(main())
