"""Phase 8.9 index split tests."""

from __future__ import annotations

from swg_scene.index_split import MAX_SHADER_INDEX_COUNT, expand_meshes_for_export, split_mesh_by_index_limit
from swg_scene.mesh_static_export import write_static_mesh
from swg_pipeline.validate import load_static_mesh_from_bytes
from swg_scene.model import SwgMaterialRef, SwgMesh, SwgScene


def _big_mesh(triangles: int) -> SwgMesh:
    positions = [(float(i), 0.0, 0.0) for i in range(triangles * 3)]
    normals = [(0.0, 0.0, 1.0)] * len(positions)
    uvs = [[(0.0, 0.0)] * len(positions)]
    indices = list(range(triangles * 3))
    return SwgMesh(
        name="big",
        positions=positions,
        normals=normals,
        uvs=uvs,
        indices=indices,
        material=SwgMaterialRef(shader_relpath="shader/big.sht"),
    )


def test_split_mesh_creates_multiple_parts():
    mesh = _big_mesh(22000)
    parts = split_mesh_by_index_limit(mesh, max_indices=65535)
    assert len(parts) == 2
    assert all(len(part.indices) <= 65535 for part in parts)
    assert sum(len(part.indices) for part in parts) == len(mesh.indices)


def test_expand_meshes_for_export_preserves_triangle_count():
    mesh = _big_mesh(22000)
    expanded = expand_meshes_for_export([mesh])
    assert len(expanded) == 2
    assert sum(len(part.indices) for part in expanded) == len(mesh.indices)


def test_export_split_writes_multiple_sps_groups():
    mesh = _big_mesh(22000)
    scene = SwgScene(meshes=[mesh], rebuild_appearance=True)
    data = write_static_mesh(scene)
    loaded = load_static_mesh_from_bytes(data)
    assert len(loaded.meshes) == 2
    assert all(len(part.indices) <= MAX_SHADER_INDEX_COUNT for part in loaded.meshes)
