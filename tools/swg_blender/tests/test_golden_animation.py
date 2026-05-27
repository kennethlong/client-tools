"""Golden retail animation fixture tests."""

from __future__ import annotations

from pathlib import Path

import pytest

from swg_iff.tags import TAG_KFAT
from swg_scene.animation import load_kfat
from swg_scene.animation_export import write_animation_file
from swg_scene.skeleton import load_skeleton
from tests.test_golden import _resolve_golden_ans, _resolve_golden_slod


def test_golden_kfat_loads_humanoid_clip():
    clip = load_kfat(_resolve_golden_ans())
    assert clip.version == "0003"
    assert clip.frame_count == 23
    assert len(clip.transforms) == 38
    assert clip.fps == pytest.approx(30.0)


def test_golden_kfat_roundtrip(tmp_path: Path):
    original = load_kfat(_resolve_golden_ans())
    out = tmp_path / "roundtrip.ans"
    write_animation_file(out, original, format="kfat")
    reloaded = load_kfat(out)
    assert reloaded.frame_count == original.frame_count
    assert len(reloaded.transforms) == len(original.transforms)
    assert [t.name for t in reloaded.transforms] == [t.name for t in original.transforms]
    assert reloaded.fps == pytest.approx(original.fps)


def test_golden_animation_matches_all_b_skeleton():
    clip = load_kfat(_resolve_golden_ans())
    skel = load_skeleton(_resolve_golden_slod(), lod_index=0)
    assert len(clip.transforms) == len(skel.joints)