"""Direction-sorted triangle index buffers (SIDX) — Phase 8.2."""

from __future__ import annotations

import math
from typing import Sequence

# ShaderPrimitiveSetWriter.cpp gSortDirs / gkNumSortDirs
SORT_DIRECTIONS: tuple[tuple[float, float, float], ...] = (
    (1.0, 0.0, 0.0),
    (-1.0, 0.0, 0.0),
    (0.0, 1.0, 0.0),
    (0.0, -1.0, 0.0),
    (0.0, 0.0, 1.0),
    (0.0, 0.0, -1.0),
    (1.0, 0.0, 1.0),
    (-1.0, 0.0, 1.0),
    (1.0, 0.0, -1.0),
    (-1.0, 0.0, -1.0),
)


def _normalize(direction: Sequence[float]) -> tuple[float, float, float]:
    x, y, z = direction
    length = math.sqrt(x * x + y * y + z * z)
    if length <= 1e-8:
        return (0.0, 0.0, 1.0)
    return (x / length, y / length, z / length)


def _triangle_sort_key(
    positions: list[tuple[float, float, float]],
    indices: list[int],
    tri_index: int,
    direction: tuple[float, float, float],
) -> float:
    base = tri_index * 3
    keys = []
    for offset in (0, 1, 2):
        vx, vy, vz = positions[indices[base + offset]]
        keys.append(-(vx * direction[0] + vy * direction[1] + vz * direction[2]))
    return min(keys)


def sort_triangles_by_direction(
    positions: list[tuple[float, float, float]],
    indices: list[int],
    direction: Sequence[float],
) -> list[int]:
    """Stable sort of triangle indices along a view direction (Maya getKey heuristic)."""
    if len(indices) % 3 != 0:
        raise ValueError("indices length must be a multiple of 3")
    if not indices:
        return []

    sort_dir = _normalize(direction)
    tri_count = len(indices) // 3
    order = sorted(
        range(tri_count),
        key=lambda tri: _triangle_sort_key(positions, indices, tri, sort_dir),
    )
    out: list[int] = []
    for tri in order:
        base = tri * 3
        out.extend(indices[base : base + 3])
    return out


def build_sorted_index_arrays(
    positions: list[tuple[float, float, float]],
    indices: list[int],
) -> list[tuple[tuple[float, float, float], list[int]]]:
    """Build the 10 direction-sorted index arrays used by SIDX."""
    if not indices:
        return []
    return [
        (_normalize(direction), sort_triangles_by_direction(positions, indices, direction))
        for direction in SORT_DIRECTIONS
    ]


def mesh_has_vertex_alpha(colors0: list[int]) -> bool:
    if not colors0:
        return False
    return any(((color >> 24) & 0xFF) < 255 for color in colors0)