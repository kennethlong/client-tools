"""Phase 8.3 index cache optimization tests."""

from __future__ import annotations

from pathlib import Path

from swg_iff.reader import validate_iff
from swg_scene.index_optimize import (
    estimate_cache_misses,
    optimize_triangle_list,
    triangle_sets_equal,
)
from swg_scene.mesh_static import load_static_mesh
from swg_scene.mesh_static_export import write_static_mesh_file
from swg_scene.model import SwgMaterialRef, SwgMesh, SwgScene
from tests.test_golden import _resolve_golden_msh


def _grid_mesh() -> SwgMesh:
    positions = [
        (0.0, 0.0, 0.0),
        (1.0, 0.0, 0.0),
        (0.0, 1.0, 0.0),
        (1.0, 1.0, 0.0),
        (0.0, 0.0, 1.0),
        (1.0, 0.0, 1.0),
        (0.0, 1.0, 1.0),
        (1.0, 1.0, 1.0),
    ]
    # Deliberately scrambled triangle order across the cube faces.
    indices = [
        0, 1, 2,
        5, 4, 7,
        3, 2, 1,
        6, 7, 4,
        0, 2, 4,
        3, 1, 5,
        2, 3, 6,
        5, 7, 1,
        0, 4, 6,
        7, 5, 3,
        4, 0, 1,
        2, 6, 3,
    ]
    return SwgMesh(
        name="cube",
        positions=positions,
        normals=[(0.0, 0.0, 1.0)] * 8,
        uvs=[[(0.0, 0.0)] * 8],
        indices=indices,
        material=SwgMaterialRef(shader_relpath="shader/test.sht"),
    )


def test_optimize_preserves_topology():
    mesh = _grid_mesh()
    optimized = optimize_triangle_list(mesh.indices)
    assert triangle_sets_equal(mesh.indices, optimized)


def test_optimize_improves_or_preserves_locality_on_scrambled_mesh():
    mesh = _grid_mesh()
    before = estimate_cache_misses(mesh.indices)
    optimized = optimize_triangle_list(mesh.indices)
    assert triangle_sets_equal(mesh.indices, optimized)
    assert estimate_cache_misses(optimized) <= before


def test_golden_bed_keeps_retail_index_order_when_optimize_requested(tmp_path: Path):
    original = load_static_mesh(_resolve_golden_msh())
    out = tmp_path / "optimized.msh"
    write_static_mesh_file(
        out,
        SwgScene(
            meshes=original.meshes,
            appearance_raw=original.appearance_raw,
            optimize_indices=True,
        ),
    )
    loaded = load_static_mesh(out)
    assert loaded.meshes[0].indices == original.meshes[0].indices


def test_roundtrip_without_optimize_preserves_indices(tmp_path: Path):
    original = load_static_mesh(_resolve_golden_msh())
    out = tmp_path / "same.msh"
    write_static_mesh_file(out, original)
    loaded = load_static_mesh(out)
    assert loaded.meshes[0].indices == original.meshes[0].indices