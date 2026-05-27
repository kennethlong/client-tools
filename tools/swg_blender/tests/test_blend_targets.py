"""BLT/BLTS blend target load + export tests."""

from __future__ import annotations

import tempfile
from pathlib import Path

import pytest

from swg_iff.reader import validate_iff
from swg_pipeline.swg_main_paths import appearance_mesh
from swg_scene.blend_targets import build_blend_targets_from_pool_deltas
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


def test_load_abyssin_body_blend_targets():
    scene = load_skeletal_mesh(_resolve_body_mgn())
    assert len(scene.blend_targets) == 3
    names = {target.name for target in scene.blend_targets}
    assert names == {"blend_muscle", "blend_fat", "blend_skinny"}


def test_blend_targets_roundtrip():
    path = _resolve_body_mgn()
    original = load_skeletal_mesh(path)
    data = write_skeletal_mesh(original)
    assert validate_iff(data)
    tmp = Path(tempfile.gettempdir()) / "blend_roundtrip.mgn"
    tmp.write_bytes(data)
    reloaded = load_skeletal_mesh(tmp)
    assert len(reloaded.blend_targets) == len(original.blend_targets)
    for src, dst in zip(original.blend_targets, reloaded.blend_targets):
        assert src.name == dst.name
        assert len(src.position_deltas) == len(dst.position_deltas)
        assert len(src.normal_deltas) == len(dst.normal_deltas)
        assert len(src.dot3_deltas) == len(dst.dot3_deltas)


def test_build_blend_targets_from_pool_deltas_sparse():
    bind_pos = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0)]
    bind_norm = [(0.0, 0.0, 1.0), (0.0, 0.0, 1.0)]
    targets = build_blend_targets_from_pool_deltas(
        names=["blend_test"],
        bind_positions=bind_pos,
        bind_normals=bind_norm,
        shape_positions=[[(0.001, 0.0, 0.0), (1.0, 0.0, 0.0)]],
        shape_normals=[bind_norm],
    )
    assert len(targets) == 1
    assert len(targets[0].position_deltas) == 1
    assert targets[0].position_deltas[0].index == 0