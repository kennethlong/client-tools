"""Headless Blender: import/export building TRE workspace (M17)."""

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


def import_building_from_workspace(workspace: str | Path) -> dict[str, Any]:
    import bpy
    from swg_blender.import_static import import_static_mesh
    from swg_pipeline.tre_project import load_project_manifest

    workspace = Path(workspace).resolve()
    manifest = load_project_manifest(workspace)
    root_pob = str(manifest["root_pob"])
    pob_stem = Path(root_pob).stem
    coll_name = f"SWG_Building_{pob_stem}"
    root_coll = _get_or_create_collection(coll_name)

    pob_root = bpy.data.objects.new("pob_root", None)
    pob_root.empty_display_type = "PLAIN_AXES"
    root_coll.objects.link(pob_root)
    if pob_root.name in bpy.context.scene.collection.objects:
        bpy.context.scene.collection.objects.unlink(pob_root)
    pob_root.select_set(True)
    bpy.context.view_layer.objects.active = pob_root
    pob_root["swg_kind"] = "pob"
    pob_root["swg_tre_relpath"] = root_pob

    imported: list[str] = []
    for cell in manifest.get("cells") or []:
        mesh_rel = cell.get("primary_mesh_relpath")
        if not mesh_rel:
            continue
        mesh_path = workspace / str(mesh_rel).replace("\\", "/")
        if not mesh_path.is_file():
            continue

        cell_name = str(cell.get("name") or f"cell_{cell.get('cell_index', 0)}")
        cell_coll = bpy.data.collections.get(f"{coll_name}_{cell_name}")
        if cell_coll is None:
            cell_coll = bpy.data.collections.new(f"{coll_name}_{cell_name}")
            root_coll.children.link(cell_coll)
        cell_coll["swg_cell_index"] = int(cell.get("cell_index", 0))
        cell_coll["swg_cell_name"] = cell_name
        cell_coll["swg_appearance_relpath"] = str(cell.get("appearance_relpath") or "")
        cell_coll["swg_floor_relpath"] = str(cell.get("floor_relpath") or "")
        cell_coll["swg_can_see_parent"] = bool(cell.get("can_see_parent"))
        cell_coll["swg_portal_count"] = int(cell.get("portal_count", 0))

        object_name = f"mesh0_{cell_name}"
        objs = import_static_mesh(mesh_path, collection=cell_coll, object_name=object_name)

        floor_rel = cell.get("floor_relpath")
        if floor_rel:
            from swg_blender.import_floor import import_floor_mesh

            floor_path = workspace / str(floor_rel).replace("\\", "/")
            floor_obj = import_floor_mesh(
                floor_path,
                collection=cell_coll,
                object_name=f"floor0_{cell_name}",
            )
            if floor_obj is not None:
                floor_obj["swg_tre_relpath"] = str(floor_rel).replace("\\", "/")
        mesh_rel_str = str(mesh_rel)
        tagged = False
        for obj in objs:
            if obj.type != "MESH":
                continue
            obj["swg_tre_relpath"] = mesh_rel_str
            obj["swg_cell_index"] = int(cell.get("cell_index", 0))
            obj["swg_cell_name"] = cell_name
            obj["swg_appearance_relpath"] = str(cell.get("appearance_relpath") or "")
            if not tagged:
                obj["swg_kind"] = "building_mesh"
                imported.append(obj.name)
                tagged = True
            else:
                obj["swg_kind"] = "building_mesh_extra"
                obj.hide_viewport = True
                obj.hide_render = True

    pob_path = workspace / str(root_pob).replace("\\", "/")
    portal_count = 0
    if pob_path.is_file():
        from swg_scene.portal_property import load_portal_property

        pob_spec = load_portal_property(pob_path)
        portal_count = pob_spec.portal_count
        portals_coll_name = f"{coll_name}_portals"
        portals_coll = bpy.data.collections.get(portals_coll_name)
        if portals_coll is None:
            portals_coll = bpy.data.collections.new(portals_coll_name)
            root_coll.children.link(portals_coll)
        from swg_blender.import_portal import import_portal_idtl

        for index in range(portal_count):
            portal_name = f"portal{index}"
            empty = bpy.data.objects.get(portal_name)
            if empty is None:
                empty = bpy.data.objects.new(portal_name, None)
                empty.empty_display_size = 0.25
                empty["swg_kind"] = "building_portal"
                empty["swg_portal_index"] = index
                portals_coll.objects.link(empty)
                if empty.name in bpy.context.scene.collection.objects:
                    bpy.context.scene.collection.objects.unlink(empty)
            if index < len(pob_spec.portal_geometry_raw):
                mesh_name = f"{portal_name}_idtl"
                if mesh_name not in bpy.data.objects:
                    import_portal_idtl(
                        pob_spec.portal_geometry_raw[index],
                        name=mesh_name,
                        collection=portals_coll,
                        portal_index=index,
                        parent=empty,
                        hide_viewport=False,
                    )

    return {
        "ok": bool(imported),
        "collection": coll_name,
        "imported": imported,
        "cell_count": len(manifest.get("cells") or []),
        "portal_count": portal_count,
    }


def _sync_manifest_cells_from_scene(
    workspace: Path, manifest: dict[str, Any], coll_name: str
) -> None:
    import bpy
    from swg_pipeline.creature_graph import normalize_tre_relpath
    from swg_pipeline.tre_project import PROJECT_MANIFEST

    for cell in manifest.get("cells") or []:
        cell_name = str(cell.get("name") or "")
        cell_coll = bpy.data.collections.get(f"{coll_name}_{cell_name}")
        if cell_coll is None:
            continue
        if cell_coll.get("swg_appearance_relpath"):
            cell["appearance_relpath"] = normalize_tre_relpath(
                str(cell_coll["swg_appearance_relpath"])
            )
        if cell_coll.get("swg_floor_relpath"):
            cell["floor_relpath"] = normalize_tre_relpath(str(cell_coll["swg_floor_relpath"]))
        if "swg_can_see_parent" in cell_coll:
            cell["can_see_parent"] = bool(cell_coll["swg_can_see_parent"])
        if cell_coll.get("swg_cell_name"):
            cell["name"] = str(cell_coll["swg_cell_name"])

    (workspace / PROJECT_MANIFEST).write_text(
        json.dumps(manifest, indent=2), encoding="utf-8"
    )


def export_building_from_workspace(workspace: str | Path) -> dict[str, Any]:
    import bpy
    from swg_blender.export_static import export_static_mesh
    from swg_pipeline.tre_project import load_project_manifest

    workspace = Path(workspace).resolve()
    manifest = load_project_manifest(workspace)
    pob_stem = Path(str(manifest["root_pob"])).stem
    coll_name = f"SWG_Building_{pob_stem}"
    root_coll = bpy.data.collections.get(coll_name)
    if root_coll is None:
        raise RuntimeError(f"collection not found: {coll_name}")

    _sync_manifest_cells_from_scene(workspace, manifest, coll_name)

    portal_geometry_raw: list[bytes] | None = None
    pob_path = workspace / str(manifest["root_pob"]).replace("\\", "/")
    if pob_path.is_file():
        from swg_blender.export_portal import portal_object_to_idtl_bytes
        from swg_scene.portal_property import load_portal_property

        pob_spec = load_portal_property(pob_path)
        portal_geometry_raw = list(pob_spec.portal_geometry_raw)
        for obj in sorted(
            (
                o
                for o in root_coll.all_objects
                if o.type == "MESH" and o.get("swg_kind") == "building_portal_mesh"
            ),
            key=lambda o: int(o.get("swg_portal_index", 0)),
        ):
            index = int(obj["swg_portal_index"])
            if 0 <= index < len(portal_geometry_raw):
                portal_geometry_raw[index] = portal_object_to_idtl_bytes(obj)

    groups: dict[str, list[Any]] = {}
    for obj in root_coll.all_objects:
        if obj.type != "MESH":
            continue
        if obj.get("swg_kind") not in ("building_mesh", "building_mesh_extra"):
            continue
        relpath = obj.get("swg_tre_relpath")
        if not relpath:
            continue
        groups.setdefault(str(relpath), []).append(obj)

    exported_msh: list[str] = []
    for relpath in sorted(groups):
        objs = groups[relpath]
        objs.sort(
            key=lambda o: (0 if o.get("swg_kind") == "building_mesh" else 1, o.name)
        )
        out_path = workspace / relpath.replace("\\", "/")
        out_path.parent.mkdir(parents=True, exist_ok=True)
        export_static_mesh(
            out_path,
            objects=objs,
            rebuild_appearance=False,
            optimize_indices=False,
        )
        exported_msh.append(relpath)

    exported_flr: list[str] = []
    for obj in root_coll.all_objects:
        if obj.type != "MESH" or obj.get("swg_kind") != "building_floor":
            continue
        relpath = obj.get("swg_tre_relpath")
        if not relpath:
            continue
        from swg_blender.export_floor import export_floor_mesh

        out_path = workspace / str(relpath).replace("\\", "/")
        export_floor_mesh(out_path, objects=[obj], preserve_metadata_from=out_path)
        exported_flr.append(str(relpath))

    from swg_pipeline.tre_project_building import rewrite_building_pob

    rewrite_building_pob(
        workspace,
        manifest,
        portal_geometry_raw=portal_geometry_raw,
    )

    return {
        "ok": bool(exported_msh),
        "collection": coll_name,
        "exported_msh": exported_msh,
        "exported_flr": exported_flr,
        "exported_portals": len(portal_geometry_raw or []),
        "pob_rewritten": True,
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
    parser = argparse.ArgumentParser(description="Import/export building TRE workspace")
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--export", action="store_true")
    parser.add_argument("--roundtrip", action="store_true")
    args = parser.parse_args(argv)

    _clear_scene()
    try:
        if args.roundtrip:
            imp = import_building_from_workspace(args.workspace)
            result = export_building_from_workspace(args.workspace)
            print(json.dumps({"import": imp, **result}))
            return 0 if result.get("ok") else 1
        if args.export:
            result = export_building_from_workspace(args.workspace)
        else:
            result = import_building_from_workspace(args.workspace)
    except Exception as exc:
        print(json.dumps({"ok": False, "error": str(exc)}))
        return 1

    print(json.dumps(result))
    return 0 if result.get("ok") else 1


if __name__ == "__main__":
    raise SystemExit(main())
