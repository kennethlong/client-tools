"""SKMG DOT3 global pool + per-shader index tests."""

from __future__ import annotations

import tempfile
from pathlib import Path

import pytest

from swg_pipeline.swg_main_paths import appearance_mesh
from swg_scene.mesh_skeletal import load_skeletal_mesh
from swg_scene.mesh_skeletal_export import write_skeletal_mesh
from swg_scene.skmg_dot3 import build_dot3_pools


def _resolve_arms_mgn() -> Path:
    golden = Path(__file__).parent / "golden" / "abyssin_m_arms_l0.mgn"
    if golden.is_file():
        return golden
    p = appearance_mesh("abyssin_m_arms_l0.mgn")
    if p is not None:
        return p
    pytest.skip("No abyssin_m_arms_l0.mgn in tests/golden/ or swg-main")


def test_load_abyssin_arms_dot3():
    scene = load_skeletal_mesh(_resolve_arms_mgn())
    assert len(scene.dot3_vectors) == 96
    mesh = scene.meshes[0]
    assert mesh.material.has_dot3 is True
    assert len(mesh.uvs) >= 2
    assert len(mesh.uvs[1][0]) == 4


def test_skmg_dot3_roundtrip():
    original = load_skeletal_mesh(_resolve_arms_mgn())
    data = write_skeletal_mesh(original)
    tmp = Path(tempfile.gettempdir()) / "skmg_dot3_roundtrip.mgn"
    tmp.write_bytes(data)
    reloaded = load_skeletal_mesh(tmp)
    assert len(reloaded.dot3_vectors) == len(original.dot3_vectors)
    assert reloaded.meshes[0].material.has_dot3 is True
    for a, b in zip(original.dot3_vectors, reloaded.dot3_vectors):
        assert a == pytest.approx(b)
    for a, b in zip(original.meshes[0].uvs[1], reloaded.meshes[0].uvs[1]):
        assert a == pytest.approx(b)


def test_build_dot3_pools_dedupes():
    pool, indices = build_dot3_pools(
        [
            (1.0, 0.0, 0.0, 1.0),
            (1.0, 0.0, 0.0, 1.0),
            (0.0, 1.0, 0.0, 1.0),
        ]
    )
    assert len(pool) == 2
    assert indices == [0, 0, 1]