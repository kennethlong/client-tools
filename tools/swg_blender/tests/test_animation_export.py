"""Tests for KFAT animation IFF export."""

from __future__ import annotations

from pathlib import Path

import pytest

from swg_iff.reader import root_form_type, validate_iff
from swg_iff.tags import TAG_KFAT
from swg_scene.animation import load_kfat_bytes
from swg_scene.animation_export import write_kfat, write_animation_file
from swg_scene.model import SwgClip, SwgQuatKey, SwgRealKey, SwgTransformTrack

SATCCF_xTranslation = 0x08
SATCCF_yTranslation = 0x10
SATCCF_zTranslation = 0x20


def _sample_clip() -> SwgClip:
    static_rot_index = 0
    rot_channel_index = 0
    x_channel_index = 0
    static_x_index = 0

    root = SwgTransformTrack(
        name="root",
        has_animated_rotation=True,
        rotation_channel_index=rot_channel_index,
        translation_mask=0,
        x_translation_channel_index=-1,
        y_translation_channel_index=-1,
        z_translation_channel_index=-1,
    )
    child = SwgTransformTrack(
        name="bone01",
        has_animated_rotation=False,
        rotation_channel_index=static_rot_index,
        translation_mask=SATCCF_xTranslation | SATCCF_yTranslation | SATCCF_zTranslation,
        x_translation_channel_index=x_channel_index,
        y_translation_channel_index=x_channel_index + 1,
        z_translation_channel_index=x_channel_index + 2,
    )

    return SwgClip(
        name="test_anim",
        fps=30.0,
        frame_count=10,
        skeleton_template_path="appearance/skeleton/all_b.skt",
        transforms=[root, child],
        rotation_channels=[
            [
                SwgQuatKey(frame=0, rotation=(0.0, 0.0, 0.0, 1.0)),
                SwgQuatKey(frame=10, rotation=(0.0, 0.1, 0.0, 0.995)),
            ]
        ],
        static_rotations=[(0.0, 0.0, 0.0, 1.0)],
        translation_channels=[
            [SwgRealKey(frame=0, value=0.0), SwgRealKey(frame=10, value=1.0)],
            [SwgRealKey(frame=0, value=0.0), SwgRealKey(frame=10, value=0.5)],
            [SwgRealKey(frame=0, value=0.0), SwgRealKey(frame=10, value=-0.25)],
        ],
        static_translations=[0.0, 0.0, 0.0],
    )


def test_write_kfat_produces_valid_form():
    data = write_kfat(_sample_clip())
    assert validate_iff(data)
    assert root_form_type(data) == TAG_KFAT
    assert data[8:12] == b"KFAT"


def test_kfat_roundtrip_channels(tmp_path: Path):
    original = _sample_clip()
    out = tmp_path / "roundtrip.kfat"
    write_animation_file(out, original, format="kfat")
    loaded = load_kfat_bytes(out.read_bytes(), path=str(out))

    assert loaded.fps == pytest.approx(original.fps)
    assert loaded.frame_count == original.frame_count
    assert len(loaded.transforms) == len(original.transforms)
    assert [t.name for t in loaded.transforms] == [t.name for t in original.transforms]
    assert len(loaded.rotation_channels) == len(original.rotation_channels)
    assert len(loaded.rotation_channels[0]) == 2
    assert loaded.rotation_channels[0][0].rotation == pytest.approx(original.rotation_channels[0][0].rotation)
    assert len(loaded.translation_channels) == 3
    assert loaded.translation_channels[0][1].value == pytest.approx(1.0)


def test_kfat_rejects_unsupported_version():
    clip = _sample_clip()
    clip.version = "0002"
    with pytest.raises(ValueError, match="0003"):
        write_kfat(clip)