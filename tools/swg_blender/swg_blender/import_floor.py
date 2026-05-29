"""Import SWG floor mesh (.flr) into Blender for building workflow."""

from __future__ import annotations

import math
import sys
from pathlib import Path
from typing import Any

from swg_scene.coords import engine_to_blender_position
from swg_scene.floor_mesh import SwgFloorMesh, load_floor_mesh

IMPORT_ROTATION_EULER = (math.pi / 2.0, 0.0, 0.0)


def _ensure_repo_on_path() -> None:
    root = Path(__file__).resolve().parents[1]
    root_str = str(root)
    if root_str not in sys.path:
        sys.path.insert(0, root_str)


def floor_to_blender_object(
    spec: SwgFloorMesh,
    *,
    name: str,
    collection: Any,
    apply_import_rotation: bool = True,
) -> Any:
    import bpy

    positions = [engine_to_blender_position(*v) for v in spec.vertices]
    faces = [tuple(t.corners) for t in spec.triangles]
    me = bpy.data.meshes.new(name)
    me.from_pydata(positions, [], faces)
    me.update()
    obj = bpy.data.objects.new(name, me)
    if apply_import_rotation:
        obj.rotation_euler = IMPORT_ROTATION_EULER
    collection.objects.link(obj)
    if obj.name in bpy.context.scene.collection.objects:
        bpy.context.scene.collection.objects.unlink(obj)
    return obj


def import_floor_mesh(
    filepath: str | Path,
    *,
    collection: Any | None = None,
    object_name: str | None = None,
    apply_import_rotation: bool = True,
    hide_viewport: bool = True,
) -> Any | None:
    """Import one `.flr` file; returns the Blender object or None if missing."""
    import bpy

    _ensure_repo_on_path()
    path = Path(filepath)
    if not path.is_file():
        return None

    spec = load_floor_mesh(path)
    if collection is None:
        collection = bpy.context.collection
    name = object_name or f"floor0_{path.stem}"
    obj = floor_to_blender_object(
        spec,
        name=name,
        collection=collection,
        apply_import_rotation=apply_import_rotation,
    )
    obj["swg_kind"] = "building_floor"
    obj["swg_floor_tri_count"] = len(spec.triangles)
    if hide_viewport:
        obj.hide_viewport = True
        obj.hide_render = True
        obj.display_type = "WIRE"
    return obj