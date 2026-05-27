"""SLOD skeleton LOD export tests."""

from __future__ import annotations

import tempfile
from pathlib import Path

import pytest

from swg_pipeline.swg_main_paths import appearance_skeleton
from swg_scene.skeleton import load_skeleton
from swg_scene.skeleton_lod import load_slod_all_levels
from swg_scene.skeleton_lod_export import write_slod


def _resolve_all_b_skt() -> Path:
    golden = Path(__file__).parent / "golden" / "all_b.skt"
    if golden.is_file():
        return golden
    p = appearance_skeleton("all_b.skt")
    if p is not None:
        return p
    pytest.skip("No all_b.skt in tests/golden/ or swg-main")


def test_load_all_b_slod_levels():
    skeletons = load_slod_all_levels(_resolve_all_b_skt())
    assert [len(s.joints) for s in skeletons] == [38, 30, 27, 21]


def test_slod_semantic_roundtrip():
    path = _resolve_all_b_skt()
    original = load_slod_all_levels(path)
    data = write_slod(original)
    tmp = Path(tempfile.gettempdir()) / "slod_roundtrip.skt"
    tmp.write_bytes(data)

    for lod_index, skel in enumerate(original):
        reloaded = load_skeleton(tmp, lod_index=lod_index)
        assert len(reloaded.joints) == len(skel.joints)
        assert [j.name for j in reloaded.joints] == [j.name for j in skel.joints]
        assert [j.parent_index for j in reloaded.joints] == [
            j.parent_index for j in skel.joints
        ]
