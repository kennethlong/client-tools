"""Phase 1 static mesh loader tests."""

from __future__ import annotations

import math

import pytest

from swg_scene.coords import blender_to_engine_position, engine_to_blender_position
from swg_iff.reader import validate_iff
from swg_scene.mesh_static import load_static_mesh, scene_to_blender_coords
from swg_scene.mesh_static_export import write_static_mesh_file
from swg_blender.import_static import IMPORT_ROTATION_EULER
from tests.test_golden import _resolve_golden_msh

# Golden mesh topology (frn_all_bed_sm_s1_l0.msh from swg-main serverdata)
GOLDEN_VERTS = 139
GOLDEN_TRIS = 201
GOLDEN_SHADER = "shader/frn_all_bed_sm_s1_as9.sht"


def test_load_golden_static_mesh():
    path = _resolve_golden_msh()
    scene = load_static_mesh(path)
    assert scene.root_form == "MESH"
    assert len(scene.meshes) == 1

    mesh = scene.meshes[0]
    assert len(mesh.positions) == GOLDEN_VERTS
    assert len(mesh.normals) == GOLDEN_VERTS
    assert len(mesh.indices) == GOLDEN_TRIS * 3
    assert mesh.material.shader_relpath == GOLDEN_SHADER
    assert max(mesh.indices) < GOLDEN_VERTS


def test_golden_uv_layer():
    path = _resolve_golden_msh()
    mesh = load_static_mesh(path).meshes[0]
    assert len(mesh.uvs) >= 1
    assert len(mesh.uvs[0]) == GOLDEN_VERTS


def test_engine_to_blender_flips_x():
    bx, by, bz = engine_to_blender_position(1.0, 2.0, 3.0)
    assert (bx, by, bz) == (-1.0, 2.0, 3.0)


def test_blender_engine_position_roundtrip():
    ex, ey, ez = 0.5, 1.2, -0.3
    bx, by, bz = engine_to_blender_position(ex, ey, ez)
    assert blender_to_engine_position(bx, by, bz) == (ex, ey, ez)


def test_scene_to_blender_coords():
    path = _resolve_golden_msh()
    raw = load_static_mesh(path)
    scene = scene_to_blender_coords(raw)
    mesh = scene.meshes[0]
    ex, ey, ez = raw.meshes[0].positions[0]
    assert mesh.positions[0] == engine_to_blender_position(ex, ey, ez)


def test_import_rotation_euler():
    assert IMPORT_ROTATION_EULER == (math.pi / 2.0, 0.0, 0.0)


def test_import_static_dry_run_cli():
    path = _resolve_golden_msh()
    from swg_blender.import_static import main

    assert main([str(path), "--dry-run"]) == 0
def test_roundtrip_golden_static_mesh(tmp_path):
    path = _resolve_golden_msh()
    original = load_static_mesh(path)
    out_path = tmp_path / "roundtrip.msh"
    write_static_mesh_file(out_path, original)
    roundtrip = load_static_mesh(out_path)

    assert validate_iff(out_path.read_bytes())
    assert len(roundtrip.meshes) == len(original.meshes)
    for before, after in zip(original.meshes, roundtrip.meshes):
        assert len(after.positions) == len(before.positions)
        assert len(after.indices) == len(before.indices)
        assert after.material.shader_relpath == before.material.shader_relpath
        for p0, p1 in zip(before.positions, after.positions):
            assert abs(p0[0] - p1[0]) < 1e-5
            assert abs(p0[1] - p1[1]) < 1e-5
            assert abs(p0[2] - p1[2]) < 1e-5
        assert after.indices == before.indices


def test_roundtrip_preserves_appearance_raw(tmp_path):
    path = _resolve_golden_msh()
    original = load_static_mesh(path)
    assert original.appearance_raw is not None
    out_path = tmp_path / "roundtrip.msh"
    write_static_mesh_file(out_path, original)
    roundtrip = load_static_mesh(out_path)
    assert roundtrip.appearance_raw == original.appearance_raw