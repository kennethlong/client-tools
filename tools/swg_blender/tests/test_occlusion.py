"""SKMG occlusion zone + OITL tests."""

from __future__ import annotations

import tempfile
from pathlib import Path

import pytest

from swg_pipeline.swg_main_paths import appearance_mesh
from swg_scene.mesh_skeletal import load_skeletal_mesh
from swg_scene.mesh_skeletal_export import write_skeletal_mesh


def _resolve_body_mgn() -> Path:
    golden = Path(__file__).parent / "golden" / "abyssin_m_body_l0.mgn"
    if golden.is_file():
        return golden
    p = appearance_mesh("abyssin_m_body_l0.mgn")
    if p is not None:
        return p
    pytest.skip("No abyssin_m_body_l0.mgn in tests/golden/ or swg-main")


def test_load_abyssin_body_occlusion():
    scene = load_skeletal_mesh(_resolve_body_mgn())
    assert len(scene.occlusion.zone_names) == 15
    assert len(scene.occlusion.zone_combinations) == 14
    assert len(scene.occlusion.zones_this_occludes) == 13
    assert scene.occlusion.occlusion_layer == 0
    assert len(scene.meshes[0].occlusion_triangles) == 1180
    assert scene.meshes[0].occlusion_triangles[0][0] == 0


def test_skmg_occlusion_roundtrip():
    original = load_skeletal_mesh(_resolve_body_mgn())
    data = write_skeletal_mesh(original)
    tmp = Path(tempfile.gettempdir()) / "skmg_occlusion_roundtrip.mgn"
    tmp.write_bytes(data)
    reloaded = load_skeletal_mesh(tmp)

    assert reloaded.occlusion.zone_names == original.occlusion.zone_names
    assert reloaded.occlusion.zone_combinations == original.occlusion.zone_combinations
    assert (
        reloaded.occlusion.fully_occluded_zone_combination
        == original.occlusion.fully_occluded_zone_combination
    )
    assert reloaded.occlusion.zones_this_occludes == original.occlusion.zones_this_occludes
    assert (
        reloaded.meshes[0].occlusion_triangles
        == original.meshes[0].occlusion_triangles
    )
