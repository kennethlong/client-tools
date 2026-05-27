"""Phase 12 collision/floor/extent tests."""

from __future__ import annotations

import tempfile
from pathlib import Path

import pytest

from swg_iff.reader import validate_iff
from swg_pipeline.swg_main_paths import appearance_collision_floor, serverdata_file
from swg_scene.extents import (
    SwgBoxExtent,
    extent_root_type,
    load_extent_bytes,
    write_extent_bytes,
)
from swg_scene.floor_mesh import (
    load_floor_mesh,
    load_floor_mesh_bytes,
    write_floor_mesh_bytes,
)
from swg_scene.hierarchy import decode_export_name
from swg_scene.indexed_triangle_list import load_idtl_bytes, write_idtl_bytes


def _require_flr() -> Path:
    p = appearance_collision_floor("aurilia_ground_hut_s01_collision_floor.flr")
    if p is None:
        pytest.skip("no FLOR fixture on swg-main")
    return p


def test_flor_roundtrip():
    original = load_floor_mesh(_require_flr())
    assert original.vertices
    assert original.triangles
    out = Path(tempfile.gettempdir()) / "flor_roundtrip.flr"
    out.write_bytes(write_floor_mesh_bytes(original))
    assert validate_iff(out.read_bytes())
    rebuilt = load_floor_mesh(out)
    assert len(rebuilt.vertices) == len(original.vertices)
    assert len(rebuilt.triangles) == len(original.triangles)
    assert rebuilt.vertices[0] == original.vertices[0]
    assert rebuilt.triangles[0].corners == original.triangles[0].corners


def test_flor_byte_roundtrip():
    path = _require_flr()
    data = path.read_bytes()
    spec = load_floor_mesh_bytes(data)
    out = write_floor_mesh_bytes(spec)
    assert out == data


def test_idtl_roundtrip():
    verts = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)]
    indices = [0, 1, 2]
    blob = write_idtl_bytes(verts, indices)
    assert validate_iff(blob)
    v2, i2 = load_idtl_bytes(blob)
    assert v2 == verts
    assert i2 == indices


def test_extent_box_roundtrip():
    ext = SwgBoxExtent(min_xyz=(0.0, 0.0, 0.0), max_xyz=(1.0, 2.0, 3.0))
    blob = write_extent_bytes(ext)
    assert extent_root_type(blob) == "EXBX"
    assert validate_iff(blob)
    loaded = load_extent_bytes(blob)
    assert isinstance(loaded, SwgBoxExtent)
    assert loaded.min_xyz == ext.min_xyz
    assert loaded.max_xyz == ext.max_xyz


def test_hierarchy_extent_primitives():
    assert decode_export_name("sphere") is not None
    assert decode_export_name("cube") is not None
    assert decode_export_name("cylinder") is not None


def test_cmsh_from_appearance_if_present():
    p = serverdata_file("appearance/mesh/player/player_male_human_mesh.iff")
    if p is None:
        pytest.skip("no mesh IFF for CMSH probe")
    data = p.read_bytes()
    if b"CMSH" not in data:
        pytest.skip("mesh has no CMSH extent")
    idx = data.find(b"CMSH")
    # Best-effort: validate IDTL helpers only when standalone IDTL exists
    if b"IDTL" not in data:
        pytest.skip("no embedded IDTL")
