"""Tests for Blender action to KFAT export helpers (no bpy required)."""

from __future__ import annotations

import pytest

from swg_blender.export_animation import (
    JointSample,
    _add_quat_key,
    _add_real_key,
    _quat_delta,
    joint_samples_to_swg_clip,
)
from swg_scene.animation import SATCCF_xTranslation, SATCCF_yTranslation, SATCCF_zTranslation
from swg_scene.animation_export import write_kfat
from swg_scene.model import SwgQuatKey, SwgRealKey
from swg_iff.reader import validate_iff


def test_quat_delta_identity():
    bind = (0.0, 0.0, 0.0, 1.0)
    current = (0.0, 0.1, 0.0, 0.995)
    delta = _quat_delta(current, bind)
    assert delta == pytest.approx(current)


def test_add_real_key_inserts_hold_key():
    keys: list[SwgRealKey] = []
    _add_real_key(keys, 0, 0.0, last_frame=False)
    _add_real_key(keys, 5, 1.0, last_frame=False)
    _add_real_key(keys, 10, 1.0, last_frame=True)
    assert [key.frame for key in keys] == [0, 4, 5, 10]
    assert keys[1].value == pytest.approx(0.0)


def test_joint_samples_to_swg_clip_static_and_animated():
    joint = JointSample(
        name="root",
        rotation_keys=[
            SwgQuatKey(frame=0, rotation=(0.0, 0.0, 0.0, 1.0)),
            SwgQuatKey(frame=10, rotation=(0.0, 0.2, 0.0, 0.98)),
        ],
        x_translation_keys=[SwgRealKey(frame=0, value=0.0)],
        y_translation_keys=[SwgRealKey(frame=0, value=0.0)],
        z_translation_keys=[SwgRealKey(frame=0, value=0.0)],
    )
    clip = joint_samples_to_swg_clip([joint], fps=30.0, frame_count=10, name="idle")
    assert clip.transforms[0].has_animated_rotation is True
    assert clip.transforms[0].translation_mask == 0
    assert len(clip.rotation_channels) == 1
    assert len(clip.static_translations) == 3
    data = write_kfat(clip)
    assert validate_iff(data)


def test_joint_samples_to_swg_clip_animated_translation_axes():
    joint = JointSample(
        name="bone01",
        rotation_keys=[SwgQuatKey(frame=0, rotation=(0.0, 0.0, 0.0, 1.0))],
        x_translation_keys=[
            SwgRealKey(frame=0, value=0.0),
            SwgRealKey(frame=10, value=1.0),
        ],
        y_translation_keys=[SwgRealKey(frame=0, value=0.0)],
        z_translation_keys=[SwgRealKey(frame=0, value=0.0)],
    )
    clip = joint_samples_to_swg_clip([joint], frame_count=10)
    track = clip.transforms[0]
    assert track.has_animated_rotation is False
    assert track.translation_mask == SATCCF_xTranslation
    assert track.x_translation_channel_index == 0
    assert track.y_translation_channel_index == 0
    assert track.z_translation_channel_index == 1
    assert len(clip.translation_channels) == 1