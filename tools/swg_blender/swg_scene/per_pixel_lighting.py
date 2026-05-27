"""Per-pixel lighting helpers (MayaPerPixelLighting port, Phase 10.5)."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any

from swg_scene.dot3 import compute_dot3_coordinates
from swg_scene.light_mesh import enumerate_mesh_triangles


@dataclass
class ShaderRequestInfo:
    use_dot3: bool = False
    calculation_uv_set: int = 0


def shader_request_from_material(material: Any | None) -> ShaderRequestInfo:
    if material is None:
        return ShaderRequestInfo()
    use_dot3 = bool(material.get("swg_dot3") or material.get("DOT3"))
    uv_set = int(material.get("swg_dot3_uv", material.get("swg_uv_CNRM", 0)) or 0)
    if not use_dot3:
        for tag in ("CNRM", "NRML"):
            if material.get(f"swg_texture_{tag}"):
                use_dot3 = True
                uv_set = int(material.get(f"swg_uv_{tag}", uv_set) or 0)
                break
    return ShaderRequestInfo(use_dot3=use_dot3, calculation_uv_set=uv_set)


def shader_request_vector_for_mesh(mesh: Any, *, slot_count: int | None = None) -> list[ShaderRequestInfo]:
    materials = list(getattr(mesh, "materials", []) or [])
    count = slot_count if slot_count is not None else max(len(materials), 1)
    requests: list[ShaderRequestInfo] = []
    for index in range(count):
        mat = materials[index] if index < len(materials) else None
        requests.append(shader_request_from_material(mat))
    return requests


def compute_mesh_dot3(
    mesh: Any,
    *,
    uv_layer_name: str | None = None,
    material: Any | None = None,
) -> list[tuple[float, float, float, float]] | None:
    """Compute per-vertex DOT3 coordinates when the material requests dot3."""
    mat = material
    if mat is None and getattr(mesh, "materials", None):
        active = getattr(mesh, "materials", None)
        if active:
            mat = active[0]
    request = shader_request_from_material(mat)
    if not request.use_dot3:
        return None
    layers = list(getattr(mesh, "uv_layers", []) or [])
    layer_name = uv_layer_name
    if layer_name is None and layers:
        idx = request.calculation_uv_set
        if 0 <= idx < len(layers):
            layer_name = layers[idx].name
        else:
            layer_name = layers[0].name
    return compute_dot3_coordinates(mesh, uv_layer_name=layer_name)


def dot3_key_count(mesh: Any, requests: list[ShaderRequestInfo] | None = None) -> int:
    """Count unique (position, normal, shader) triples that request dot3."""
    req = requests or shader_request_vector_for_mesh(mesh)
    keys: set[tuple[int, int, int]] = set()
    mesh.calc_loop_triangles()
    for tri in mesh.loop_triangles:
        shader_index = int(mesh.polygons[tri.polygon_index].material_index)
        if shader_index < 0 or shader_index >= len(req):
            shader_index = 0
        if not req[shader_index].use_dot3:
            continue
        poly = mesh.polygons[tri.polygon_index]
        for loop_index in tri.loops:
            vert_index = mesh.loops[loop_index].vertex_index
            keys.add((vert_index, vert_index, shader_index))
    if not keys:
        for shader_index, _poly_index, tri in enumerate_mesh_triangles(mesh):
            if shader_index < len(req) and req[shader_index].use_dot3:
                keys.add((tri[0], tri[0], shader_index))
    return len(keys)
