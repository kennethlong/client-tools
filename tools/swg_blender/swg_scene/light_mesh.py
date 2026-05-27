"""Triangle enumeration per shader group (MayaLightMeshReader port, Phase 10.6)."""

from __future__ import annotations

from collections.abc import Callable
from typing import Any

TriangleCallback = Callable[[int, int, tuple[int, int, int]], bool]


def enumerate_mesh_triangles(
    mesh: Any,
    *,
    material_slot_count: int | None = None,
) -> list[tuple[int, int, tuple[int, int, int]]]:
    """Return (shader_index, poly_index, (v0, v1, v2)) face-relative vertex indices."""
    results: list[tuple[int, int, tuple[int, int, int]]] = []

    def collect(shader_index: int, poly_index: int, verts: tuple[int, int, int]) -> bool:
        results.append((shader_index, poly_index, verts))
        return True

    enumerate_triangles(mesh, material_slot_count=material_slot_count, callback=collect)
    return results


def enumerate_triangles(
    mesh: Any,
    *,
    material_slot_count: int | None = None,
    callback: TriangleCallback,
) -> bool:
    """Fan each polygon to triangles; shader_index from polygon material_index."""
    polygons = mesh.polygons
    slot_count = material_slot_count
    if slot_count is None:
        slot_count = len(getattr(mesh, "materials", []) or []) or 1

    for poly_index, poly in enumerate(polygons):
        shader_index = int(getattr(poly, "material_index", 0))
        if shader_index < 0:
            shader_index = 0
        if slot_count and shader_index >= slot_count:
            shader_index = 0
        vert_count = len(poly.vertices)
        if vert_count < 3:
            continue
        for last_index in range(2, vert_count):
            tri = (0, last_index, last_index - 1)
            if not callback(shader_index, poly_index, tri):
                return False
    return True
