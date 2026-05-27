"""DOT3 tangent basis helpers for static mesh export (Phase 8.5)."""

from __future__ import annotations

from typing import Any

from swg_scene.coords import blender_to_engine_vector


def compute_dot3_coordinates(mesh: Any, *, uv_layer_name: str | None = None) -> list[tuple[float, float, float, float]] | None:
    """Return per-vertex DOT3 coordinates (du.xyz + handedness w) in engine space."""
    if not mesh.uv_layers:
        return None
    layer_name = uv_layer_name or (mesh.uv_layers.active.name if mesh.uv_layers.active else mesh.uv_layers[0].name)
    try:
        mesh.calc_tangents(uvmap=layer_name)
    except RuntimeError:
        return None

    per_vertex: list[tuple[float, float, float, float]] = [(0.0, 0.0, 1.0, 1.0)] * len(mesh.vertices)
    counts = [0] * len(mesh.vertices)
    for loop in mesh.loops:
        tangent = loop.tangent
        tx, ty, tz = blender_to_engine_vector(tangent.x, tangent.y, tangent.z)
        handedness = float(loop.bitangent_sign)
        vi = loop.vertex_index
        ox, oy, oz, ow = per_vertex[vi]
        n = counts[vi]
        per_vertex[vi] = (
            (ox * n + tx) / (n + 1),
            (oy * n + ty) / (n + 1),
            (oz * n + tz) / (n + 1),
            handedness,
        )
        counts[vi] += 1
    return per_vertex