"""Phase 8.6 normal map bake tests."""

from __future__ import annotations

import pytest

from swg_pipeline.normal_map import (
    bake_mesh_normal_map,
    pack_bgr_normal,
    process_scratch_height_map,
    scratch_height_to_bgr,
    unpack_bgr_normal,
)


def test_pack_unpack_bgr_normal_roundtrip():
    normal = (0.0, 0.0, 1.0)
    packed = pack_bgr_normal(normal)
    back = unpack_bgr_normal(packed)
    assert back[0] == pytest.approx(0.0, abs=0.01)
    assert back[1] == pytest.approx(0.0, abs=0.01)
    assert back[2] == pytest.approx(1.0, abs=0.02)


def test_scratch_height_flat_is_up():
    height = [128] * 16
    normals = process_scratch_height_map(height, 4, 4, scale=1.0)
    for nx, ny, nz in normals:
        assert nz == pytest.approx(1.0, abs=0.05)


def test_bake_plane_normal_map_points_up():
    positions = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)]
    normals = [(0.0, 0.0, 1.0)] * 3
    uvs = [[(0.0, 0.0), (1.0, 0.0), (0.0, 1.0)]]
    bgr = bake_mesh_normal_map(positions, normals, uvs, [0, 1, 2], width=8, height=8)
    assert len(bgr) == 8 * 8 * 3
    cx = unpack_bgr_normal(bgr[(4 * 8 + 4) * 3 : (4 * 8 + 4) * 3 + 3])
    assert cx[2] == pytest.approx(1.0, abs=0.05)


def test_scratch_height_to_bgr_length():
    bgr = scratch_height_to_bgr([0, 255, 128, 64] * 4, 4, 4)
    assert len(bgr) == 48
