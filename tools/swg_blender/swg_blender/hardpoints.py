"""Collect SWG hardpoints from Blender empty objects."""

from __future__ import annotations

from typing import Any

from swg_scene.coords import blender_to_engine_position, blender_to_engine_vector
from swg_scene.model import SwgHardpoint


def _hardpoint_name(obj: Any) -> str | None:
    custom = obj.get("swg_hardpoint")
    if custom:
        return str(custom)
    if obj.name.startswith("hp_"):
        return obj.name[3:]
    if obj.name.startswith("HP_"):
        return obj.name[3:]
    return None


def blender_matrix_to_engine_transform(matrix_local: Any) -> tuple[float, ...]:
    """Convert a Blender 4x4 local matrix to Iff floatTransform (3x4 row-major)."""
    cols = [matrix_local.col[i] for i in range(3)]
    basis = [
        blender_to_engine_vector(cols[0].x, cols[0].y, cols[0].z),
        blender_to_engine_vector(cols[1].x, cols[1].y, cols[1].z),
        blender_to_engine_vector(cols[2].x, cols[2].y, cols[2].z),
    ]
    ox, oy, oz = blender_to_engine_position(
        matrix_local.translation.x,
        matrix_local.translation.y,
        matrix_local.translation.z,
    )
    return (
        basis[0][0],
        basis[1][0],
        basis[2][0],
        ox,
        basis[0][1],
        basis[1][1],
        basis[2][1],
        oy,
        basis[0][2],
        basis[1][2],
        basis[2][2],
        oz,
    )


def collect_hardpoints_for_mesh(mesh_obj: Any, context_objects: list[Any] | None = None) -> list[SwgHardpoint]:
    """Find empties parented to ``mesh_obj`` (or in ``context_objects``) marked as hardpoints."""
    import bpy

    candidates = context_objects if context_objects is not None else list(bpy.context.scene.objects)
    hardpoints: list[SwgHardpoint] = []
    for obj in candidates:
        if obj.type != "EMPTY":
            continue
        name = _hardpoint_name(obj)
        if not name:
            continue
        if obj.parent == mesh_obj:
            matrix = obj.matrix_local.copy()
        elif obj.parent is None:
            matrix = mesh_obj.matrix_world.inverted() @ obj.matrix_world
        else:
            continue
        hardpoints.append(
            SwgHardpoint(name=name, matrix=blender_matrix_to_engine_transform(matrix))
        )
    hardpoints.sort(key=lambda hp: hp.name)
    return hardpoints
