"""Phase 8.2 / 8.4 static mesh depth tests."""

from __future__ import annotations

from pathlib import Path

import pytest

from swg_iff.reader import validate_iff
from swg_scene.color_utils import rgba_float_to_packed
from swg_scene.index_sort import build_sorted_index_arrays, sort_triangles_by_direction
from swg_scene.mesh_static import load_static_mesh
from swg_scene.mesh_static_export import write_static_mesh_file
from swg_scene.model import SwgDirectionSortedIndices, SwgMesh, SwgMaterialRef, SwgScene
from tests.test_golden import _resolve_golden_msh


def _simple_mesh(*, alpha: float = 1.0) -> SwgMesh:
    color = rgba_float_to_packed(1.0, 0.0, 0.0, alpha)
    return SwgMesh(
        name="test",
        positions=[(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0)],
        normals=[(0.0, 0.0, 1.0)] * 4,
        uvs=[[(0.0, 0.0), (1.0, 0.0), (0.0, 1.0), (1.0, 1.0)]],
        colors0=[color] * 4,
        indices=[0, 1, 2, 0, 2, 3],
        material=SwgMaterialRef(shader_relpath="shader/test.sht"),
    )


def test_sort_triangles_preserves_index_count():
    mesh = _simple_mesh()
    sorted_indices = sort_triangles_by_direction(mesh.positions, mesh.indices, (0.0, 0.0, 1.0))
    assert len(sorted_indices) == len(mesh.indices)


def test_build_sorted_index_arrays_count():
    mesh = _simple_mesh()
    arrays = build_sorted_index_arrays(mesh.positions, mesh.indices)
    assert len(arrays) == 10
    assert all(len(indices) == len(mesh.indices) for _, indices in arrays)


def test_export_mesh_with_vertex_colors_and_sidx(tmp_path: Path):
    mesh = _simple_mesh(alpha=0.5)
    out = tmp_path / "alpha.msh"
    write_static_mesh_file(out, SwgScene(meshes=[mesh], rebuild_appearance=True))
    assert validate_iff(out.read_bytes())

    loaded = load_static_mesh(out)
    assert loaded.meshes[0].colors0
    assert loaded.meshes[0].sorted_indices
    assert len(loaded.meshes[0].sorted_indices) == 10


def test_sidx_roundtrip(tmp_path: Path):
    mesh = _simple_mesh(alpha=0.5)
    arrays = build_sorted_index_arrays(mesh.positions, mesh.indices)
    mesh.sorted_indices = [
        SwgDirectionSortedIndices(direction=direction, indices=indices)
        for direction, indices in arrays
    ]
    out = tmp_path / "roundtrip.msh"
    write_static_mesh_file(out, SwgScene(meshes=[mesh], rebuild_appearance=True))
    loaded = load_static_mesh(out)
    assert len(loaded.meshes[0].sorted_indices) == 10
    assert loaded.meshes[0].sorted_indices[0].indices == mesh.sorted_indices[0].indices


def test_golden_bed_still_has_no_sidx():
    scene = load_static_mesh(_resolve_golden_msh())
    mesh = scene.meshes[0]
    assert not mesh.sorted_indices
    assert not mesh.colors0

def test_multi_uv_roundtrip(tmp_path: Path):
    positions = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)]
    uv0 = [(0.0, 0.0), (1.0, 0.0), (0.0, 1.0)]
    uv1 = [(0.5, 0.5), (0.6, 0.5), (0.5, 0.6)]
    mesh = SwgMesh(
        name="uvtest",
        positions=positions,
        normals=[(0.0, 0.0, 1.0)] * 3,
        uvs=[uv0, uv1],
        indices=[0, 1, 2],
        material=SwgMaterialRef(shader_relpath="shader/test.sht"),
    )
    out = tmp_path / "multiuv.msh"
    write_static_mesh_file(out, SwgScene(meshes=[mesh], rebuild_appearance=True))
    loaded = load_static_mesh(out).meshes[0]
    assert len(loaded.uvs) == 2
    for orig, got in zip(uv0, loaded.uvs[0]):
        assert got[0] == pytest.approx(orig[0])
        assert got[1] == pytest.approx(orig[1])
    for orig, got in zip(uv1, loaded.uvs[1]):
        assert got[0] == pytest.approx(orig[0])
        assert got[1] == pytest.approx(orig[1])


def test_dot3_uv_roundtrip(tmp_path: Path):
    positions = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)]
    dot3 = [(1.0, 0.0, 0.0, 1.0), (1.0, 0.0, 0.0, 1.0), (1.0, 0.0, 0.0, 1.0)]
    mesh = SwgMesh(
        name="dot3",
        positions=positions,
        normals=[(0.0, 0.0, 1.0)] * 3,
        uvs=[[(0.0, 0.0), (1.0, 0.0), (0.0, 1.0)], dot3],
        indices=[0, 1, 2],
        material=SwgMaterialRef(
            shader_relpath="shader/test.sht",
            texture_coord_sets={"MAIN": 0, "DOT3": 1},
            has_dot3=True,
        ),
    )
    out = tmp_path / "dot3.msh"
    write_static_mesh_file(out, SwgScene(meshes=[mesh], rebuild_appearance=True))
    loaded = load_static_mesh(out).meshes[0]
    assert len(loaded.uvs) == 2
    assert len(loaded.uvs[1][0]) == 4
    assert loaded.uvs[1] == dot3
