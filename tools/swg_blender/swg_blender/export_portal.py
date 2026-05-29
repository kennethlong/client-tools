"""Export Blender portal IDTL mesh to PRTS bytes."""

from __future__ import annotations

from typing import Any

from swg_blender.bpy_geometry import mesh_vertex_positions_engine
from swg_scene.indexed_triangle_list import write_idtl_bytes


def portal_object_to_idtl_bytes(obj: Any) -> bytes:
    mesh = obj.data
    if mesh is None:
        raise ValueError(f"object {obj.name!r} has no mesh")
    positions = mesh_vertex_positions_engine(obj)
    indices: list[int] = []
    for poly in mesh.polygons:
        if len(poly.vertices) != 3:
            raise ValueError(f"portal {obj.name!r}: non-triangle face")
        indices.extend(int(i) for i in poly.vertices)
    return write_idtl_bytes(positions, indices)