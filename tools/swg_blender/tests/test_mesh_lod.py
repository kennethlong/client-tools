"""MLOD (.lmg) mesh LOD template tests."""

from __future__ import annotations

import tempfile
from pathlib import Path

import pytest

from swg_pipeline.swg_main_paths import appearance_mesh
from swg_scene.mesh_lod import load_mesh_lod, write_mesh_lod


def _resolve_body_lmg() -> Path:
    golden = Path(__file__).parent / "golden" / "abyssin_m_body.lmg"
    if golden.is_file():
        return golden
    p = appearance_mesh("abyssin_m_body.lmg")
    if p is not None:
        return p
    pytest.skip("No abyssin_m_body.lmg in tests/golden/ or swg-main")


def test_load_abyssin_body_lmg():
    lod = load_mesh_lod(_resolve_body_lmg())
    assert len(lod.level_paths) == 4
    assert lod.level_paths[0].endswith("abyssin_m_body_l0.mgn")
    assert lod.level_paths[-1].endswith("abyssin_m_body_l3.mgn")


def test_mesh_lod_byte_roundtrip():
    path = _resolve_body_lmg()
    original = path.read_bytes()
    lod = load_mesh_lod(path)
    data = write_mesh_lod(lod)
    assert data == original

    tmp = Path(tempfile.gettempdir()) / "mesh_lod_roundtrip.lmg"
    tmp.write_bytes(data)
    reloaded = load_mesh_lod(tmp)
    assert reloaded.level_paths == lod.level_paths
