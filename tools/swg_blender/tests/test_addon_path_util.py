"""Tests for swg_blender_addon.path_util (no bpy required)."""

from __future__ import annotations

import os
import sys

from swg_blender_addon.path_util import default_pipeline_root, ensure_pipeline_on_path


def test_default_pipeline_root():
    root = default_pipeline_root()
    assert (root / "swg_iff").is_dir()
    assert (root / "swg_scene").is_dir()


def test_ensure_pipeline_on_path_inserts_once():
    root = default_pipeline_root()
    path_str = str(root)
    sys.path[:] = [p for p in sys.path if p != path_str]
    returned = ensure_pipeline_on_path()
    assert returned.resolve() == root.resolve()
    assert sys.path[0] == path_str
    ensure_pipeline_on_path()
    assert sys.path.count(path_str) == 1


def test_swg_blender_root_env(monkeypatch, tmp_path):
    fake = tmp_path / "fake_pipeline"
    fake.mkdir()
    (fake / "swg_iff").mkdir()
    monkeypatch.setenv("SWG_BLENDER_ROOT", str(fake))
    sys.path[:] = [p for p in sys.path if p != str(fake)]
    from importlib import reload
    import swg_blender_addon.path_util as pu

    reload(pu)
    assert pu.pipeline_root().resolve() == fake.resolve()