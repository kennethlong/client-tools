"""Golden retail CKAT (compressed) animation loader tests."""

from __future__ import annotations

from pathlib import Path

import pytest

from swg_iff.reader import root_form_type, validate_iff
from swg_iff.tags import TAG_CKAT
from swg_scene.animation import load_animation, load_kfat
from swg_scene.animation_export import write_animation, write_animation_file
from swg_scene.skeleton import load_skeleton
from tests.test_golden import _resolve_golden_ckat, _resolve_golden_slod


def test_golden_ckat_loads():
    clip = load_animation(_resolve_golden_ckat())
    assert clip.version == "0001"
    assert clip.frame_count > 0
    assert len(clip.transforms) == 38
    assert clip.fps == pytest.approx(30.0)


def test_load_animation_dispatches_kfat_and_ckat():
    from tests.test_golden import _resolve_golden_ans

    kfat = load_animation(_resolve_golden_ans())
    assert kfat.version == "0003"
    ckat = load_animation(_resolve_golden_ckat())
    assert ckat.version == "0001"


def test_golden_ckat_matches_all_b_skeleton():
    clip = load_animation(_resolve_golden_ckat())
    skel = load_skeleton(_resolve_golden_slod(), lod_index=0)
    assert len(clip.transforms) == len(skel.joints)


def test_load_kfat_rejects_ckat():
    with pytest.raises(Exception):
        load_kfat(_resolve_golden_ckat())


def test_write_ckat_produces_valid_form():
    clip = load_animation(_resolve_golden_ckat())
    data = write_animation(clip, format="ckat")
    assert validate_iff(data)
    assert root_form_type(data) == TAG_CKAT
    assert data[8:12] == b"CKAT"


def test_golden_ckat_roundtrip(tmp_path: Path):
    original = load_animation(_resolve_golden_ckat())
    out = tmp_path / "roundtrip.ans"
    write_animation_file(out, original, format="ckat")
    reloaded = load_animation(out)

    assert reloaded.version == "0001"
    assert reloaded.frame_count == original.frame_count
    assert len(reloaded.transforms) == len(original.transforms)
    assert [t.name for t in reloaded.transforms] == [t.name for t in original.transforms]
    assert len(reloaded.rotation_channels) == len(original.rotation_channels)
    assert len(reloaded.translation_channels) == len(original.translation_channels)
    for orig_channel, reload_channel in zip(original.rotation_channels, reloaded.rotation_channels):
        assert len(reload_channel) == len(orig_channel)
        assert [key.frame for key in reload_channel] == [key.frame for key in orig_channel]
    for orig_channel, reload_channel in zip(original.translation_channels, reloaded.translation_channels):
        assert len(reload_channel) == len(orig_channel)
        assert [key.frame for key in reload_channel] == [key.frame for key in orig_channel]