"""Blender mesh geometry in SWG engine space (world-aware)."""

from __future__ import annotations

from typing import Any

from swg_scene.coords import blender_to_engine_position, blender_to_engine_vector


def _vec3_xyz(v: Any) -> tuple[float, float, float]:
    if hasattr(v, "x"):
        return float(v.x), float(v.y), float(v.z)
    return float(v[0]), float(v[1]), float(v[2])


class _IdentityMatrix:
    """Fallback when bpy/mathutils unavailable (unit tests with mocks)."""

    def __matmul__(self, other: Any) -> Any:
        return other

    def to_3x3(self) -> "_IdentityMatrix":
        return self

    def inverted(self) -> "_IdentityMatrix":
        return self

    def transposed(self) -> "_IdentityMatrix":
        return self


def _mesh_world_matrix(obj: Any):
    mw = getattr(obj, "matrix_world", None)
    if mw is not None:
        if hasattr(mw, "copy"):
            return mw.copy()
        return mw
    return _IdentityMatrix()


def _normal_transform_matrix(mw: Any):
    m3 = mw.to_3x3()
    inv = m3.inverted()
    transposed = getattr(inv, "transposed", None)
    if transposed is not None and not callable(transposed):
        return transposed
    if callable(transposed):
        return transposed()
    return inv


def mesh_vertex_positions_engine(obj: Any) -> list[tuple[float, float, float]]:
    """Mesh vertex positions in engine space, including object transform."""
    mesh = obj.data
    if mesh is None:
        raise ValueError(f"object {obj.name!r} has no mesh data")
    mw = _mesh_world_matrix(obj)
    return [
        blender_to_engine_position(*_vec3_xyz(mw @ v.co))
        for v in mesh.vertices
    ]


def mesh_vertex_normals_engine(obj: Any) -> list[tuple[float, float, float]]:
    mesh = obj.data
    if mesh is None or not mesh.vertices:
        return []
    if hasattr(mesh.vertices[0], "normal"):
        pass
    elif mesh.polygons:
        mesh.calc_normals()
    else:
        return [(0.0, 0.0, 1.0)] * len(mesh.vertices)
    mw = _mesh_world_matrix(obj)
    normal_matrix = _normal_transform_matrix(mw)
    out: list[tuple[float, float, float]] = []
    for v in mesh.vertices:
        raw = normal_matrix @ v.normal
        n = raw.normalized() if hasattr(raw, "normalized") else raw
        nx, ny, nz = _vec3_xyz(n)
        out.append(blender_to_engine_vector(nx, ny, nz))
    return out


def armature_bind_positions_engine(arm_obj: Any) -> list[tuple[float, float, float]]:
    mw = _mesh_world_matrix(arm_obj)
    return [
        blender_to_engine_position(*_vec3_xyz(mw @ bone.head_local))
        for bone in arm_obj.data.bones
    ]
