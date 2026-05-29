"""Import SWG portal geometry (IDTL in PRTS) into Blender."""

from __future__ import annotations

import math
import sys
from pathlib import Path
from typing import Any

from swg_scene.coords import engine_to_blender_position
from swg_scene.indexed_triangle_list import load_idtl_bytes

IMPORT_ROTATION_EULER = (math.pi / 2.0, 0.0, 0.0)


def import_portal_idtl(
    data: bytes,
    *,
    name: str,
    collection: Any,
    portal_index: int,
    parent: Any | None = None,
    apply_import_rotation: bool = True,
    hide_viewport: bool = False,
) -> Any:
    import bpy

    vertices, indices = load_idtl_bytes(data)
    positions = [engine_to_blender_position(*v) for v in vertices]
    faces = [tuple(indices[i : i + 3]) for i in range(0, len(indices), 3)]
    me = bpy.data.meshes.new(name)
    me.from_pydata(positions, [], faces)
    me.update()
    obj = bpy.data.objects.new(name, me)
    if apply_import_rotation:
        obj.rotation_euler = IMPORT_ROTATION_EULER
    collection.objects.link(obj)
    if obj.name in bpy.context.scene.collection.objects:
        bpy.context.scene.collection.objects.unlink(obj)
    if parent is not None:
        obj.parent = parent
        obj.matrix_parent_inverse = parent.matrix_world.inverted()
    obj["swg_kind"] = "building_portal_mesh"
    obj["swg_portal_index"] = int(portal_index)
    if hide_viewport:
        obj.hide_viewport = True
        obj.hide_render = True
        obj.display_type = "WIRE"
    return obj