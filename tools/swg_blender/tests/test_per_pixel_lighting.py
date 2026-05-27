"""Phase 10.5-10.6 per-pixel lighting and light mesh tests."""

from __future__ import annotations

from types import SimpleNamespace

from swg_scene.light_mesh import enumerate_mesh_triangles
from swg_scene.per_pixel_lighting import shader_request_from_material


class _Poly:
    def __init__(self, verts, material_index=0):
        self.vertices = verts
        self.material_index = material_index


class _Mesh:
    def __init__(self):
        self.polygons = [
            _Poly([0, 1, 2], 0),
            _Poly([0, 2, 3, 4], 1),
        ]


def test_enumerate_mesh_triangles_fan():
    tris = enumerate_mesh_triangles(_Mesh(), material_slot_count=2)
    assert len(tris) == 3
    assert tris[0][0] == 0
    assert tris[1][0] == 1


def test_shader_request_from_material():
    mat = SimpleNamespace(
        get=lambda k, d=None: {"swg_dot3": True, "swg_dot3_uv": 1}.get(k, d),
    )
    req = shader_request_from_material(mat)
    assert req.use_dot3
    assert req.calculation_uv_set == 1
