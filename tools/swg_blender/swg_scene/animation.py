"""KFAT/CKAT keyframe skeletal animation loader (KeyframeSkeletalAnimationTemplate 0002/0003)."""

from __future__ import annotations

from pathlib import Path

from swg_iff.reader import IffReader, IffError
from swg_iff.tags import (
    TAG_0002,
    TAG_0003,
    TAG_AROT,
    TAG_ATRN,
    TAG_CHNL,
    TAG_INFO,
    TAG_CKAT,
    TAG_KFAT,
    TAG_QCHN,
    TAG_SROT,
    TAG_STRN,
    TAG_XFIN,
    TAG_XFRM,
    tag_to_str,
)
from swg_scene.model import SwgClip, SwgQuatKey, SwgRealKey, SwgTransformTrack

# Channel component flags (KeyframeSkeletalAnimationTemplateDef.h)
SATCCF_xTranslation = 0x08
SATCCF_yTranslation = 0x10
SATCCF_zTranslation = 0x20
SATCCF_translationMask = SATCCF_xTranslation | SATCCF_yTranslation | SATCCF_zTranslation



def load_animation(path: str | Path) -> SwgClip:
    """Load `.ans` / animation IFF (KFAT or CKAT) into SwgClip IR."""
    p = Path(path)
    reader = IffReader.from_file(p)
    clip = _load_animation_reader(reader)
    clip.source_path = str(p)
    if not clip.name:
        clip.name = p.stem
    return clip


def load_animation_bytes(data: bytes, *, path: str = "<memory>") -> SwgClip:
    reader = IffReader(data, path)
    return _load_animation_reader(reader)


def _load_animation_reader(reader: IffReader) -> SwgClip:
    root = reader.current_name()
    if root == TAG_KFAT:
        return _load_kfat_reader(reader)
    if root == TAG_CKAT:
        from swg_scene.animation_compressed import _load_ckat_reader

        return _load_ckat_reader(reader)
    raise IffError(f"unsupported animation form {tag_to_str(root)}")

def load_kfat(path: str | Path) -> SwgClip:
    p = Path(path)
    reader = IffReader.from_file(p)
    clip = _load_kfat_reader(reader)
    clip.source_path = str(p)
    if not clip.name:
        clip.name = p.stem
    return clip


def load_kfat_bytes(data: bytes, *, path: str = "<memory>") -> SwgClip:
    reader = IffReader(data, path)
    return _load_kfat_reader(reader)


def _load_kfat_reader(reader: IffReader) -> SwgClip:
    reader.enter_form(TAG_KFAT)
    try:
        version_tag = reader.current_name()
        if version_tag == TAG_0002:
            clip = _load_0002(reader)
        elif version_tag == TAG_0003:
            clip = _load_0003(reader)
        else:
            raise IffError(f"unsupported KFAT version {tag_to_str(version_tag)}")
        clip.version = tag_to_str(version_tag)
        return clip
    finally:
        reader.exit_form(TAG_KFAT)


def _read_quaternion(reader: IffReader) -> tuple[float, float, float, float]:
    return (
        reader.read_float(),
        reader.read_float(),
        reader.read_float(),
        reader.read_float(),
    )


def _load_rotation_channel_0003(reader: IffReader) -> list[SwgQuatKey]:
    reader.enter_chunk(TAG_QCHN)
    try:
        key_count = reader.read_int32()
        keys: list[SwgQuatKey] = []
        for _ in range(key_count):
            frame = reader.read_int32()
            rotation = _read_quaternion(reader)
            keys.append(SwgQuatKey(frame=frame, rotation=rotation))
        return keys
    finally:
        reader.exit_chunk(TAG_QCHN)


def _load_translation_channel(reader: IffReader) -> list[SwgRealKey]:
    reader.enter_chunk(TAG_CHNL)
    try:
        key_count = reader.read_int32()
        keys: list[SwgRealKey] = []
        for _ in range(key_count):
            frame = reader.read_int32()
            value = reader.read_float()
            keys.append(SwgRealKey(frame=frame, value=value))
        return keys
    finally:
        reader.exit_chunk(TAG_CHNL)


def _load_transform_tracks_0003(reader: IffReader, transform_count: int) -> list[SwgTransformTrack]:
    reader.enter_form(TAG_XFRM)
    try:
        tracks: list[SwgTransformTrack] = []
        for _ in range(transform_count):
            reader.enter_chunk(TAG_XFIN)
            try:
                name = reader.read_string()
                has_animated_rotation = reader.read_int8() != 0
                rotation_channel_index = reader.read_int32()
                translation_mask = reader.read_uint32()
                x_index = reader.read_int32()
                y_index = reader.read_int32()
                z_index = reader.read_int32()
                tracks.append(
                    SwgTransformTrack(
                        name=name,
                        has_animated_rotation=has_animated_rotation,
                        rotation_channel_index=rotation_channel_index,
                        translation_mask=translation_mask,
                        x_translation_channel_index=x_index,
                        y_translation_channel_index=y_index,
                        z_translation_channel_index=z_index,
                    )
                )
            finally:
                reader.exit_chunk(TAG_XFIN)
        return tracks
    finally:
        reader.exit_form(TAG_XFRM)


def _load_0003(reader: IffReader) -> SwgClip:
    reader.enter_form(TAG_0003)
    try:
        reader.enter_chunk(TAG_INFO)
        fps = reader.read_float()
        frame_count = reader.read_int32()
        transform_count = reader.read_int32()
        rotation_channel_count = reader.read_int32()
        static_rotation_count = reader.read_int32()
        translation_channel_count = reader.read_int32()
        static_translation_count = reader.read_int32()
        reader.exit_chunk(TAG_INFO)

        transforms = _load_transform_tracks_0003(reader, transform_count)

        rotation_channels: list[list[SwgQuatKey]] = []
        if rotation_channel_count > 0:
            reader.enter_form(TAG_AROT)
            try:
                for _ in range(rotation_channel_count):
                    rotation_channels.append(_load_rotation_channel_0003(reader))
            finally:
                reader.exit_form(TAG_AROT)

        static_rotations: list[tuple[float, float, float, float]] = []
        if static_rotation_count > 0:
            reader.enter_chunk(TAG_SROT)
            try:
                for _ in range(static_rotation_count):
                    static_rotations.append(_read_quaternion(reader))
            finally:
                reader.exit_chunk(TAG_SROT)

        translation_channels: list[list[SwgRealKey]] = []
        if translation_channel_count > 0:
            reader.enter_form(TAG_ATRN)
            try:
                for _ in range(translation_channel_count):
                    translation_channels.append(_load_translation_channel(reader))
            finally:
                reader.exit_form(TAG_ATRN)

        static_translations: list[float] = []
        if static_translation_count > 0:
            reader.enter_chunk(TAG_STRN)
            try:
                for _ in range(static_translation_count):
                    static_translations.append(reader.read_float())
            finally:
                reader.exit_chunk(TAG_STRN)

        # Skip optional MSGS / LOCT / LOCR chunks for MVP.
        while not reader.at_end_of_form():
            reader.skip_block()

        return SwgClip(
            fps=fps,
            frame_count=frame_count,
            transforms=transforms,
            rotation_channels=rotation_channels,
            static_rotations=static_rotations,
            translation_channels=translation_channels,
            static_translations=static_translations,
        )
    finally:
        reader.exit_form(TAG_0003)


def _load_transform_tracks_0002(reader: IffReader, transform_count: int) -> list[SwgTransformTrack]:
    reader.enter_form(TAG_XFRM)
    try:
        tracks: list[SwgTransformTrack] = []
        animated_rotation_count = 0
        static_rotation_count = 0
        for _ in range(transform_count):
            reader.enter_chunk(TAG_XFIN)
            try:
                name = reader.read_string()
                rotation_mask = reader.read_uint32()
                x_rot = reader.read_int32()
                y_rot = reader.read_int32()
                z_rot = reader.read_int32()
                has_animated_rotation = (rotation_mask & 0x07) != 0
                if has_animated_rotation:
                    rotation_channel_index = animated_rotation_count
                    animated_rotation_count += 1
                else:
                    rotation_channel_index = static_rotation_count
                    static_rotation_count += 1

                translation_mask = reader.read_uint32()
                x_index = reader.read_int32()
                y_index = reader.read_int32()
                z_index = reader.read_int32()
                tracks.append(
                    SwgTransformTrack(
                        name=name,
                        has_animated_rotation=has_animated_rotation,
                        rotation_channel_index=rotation_channel_index,
                        translation_mask=translation_mask,
                        x_translation_channel_index=x_index,
                        y_translation_channel_index=y_index,
                        z_translation_channel_index=z_index,
                    )
                )
            finally:
                reader.exit_chunk(TAG_XFIN)
        return tracks
    finally:
        reader.exit_form(TAG_XFRM)


def _load_0002(reader: IffReader) -> SwgClip:
    """Load legacy Euler KFAT 0002.

    Rotation keys remain in Euler float channels until converted; quaternion
    channels are not populated for animated rotations in this format.
    """
    reader.enter_form(TAG_0002)
    try:
        reader.enter_chunk(TAG_INFO)
        fps = reader.read_float()
        frame_count = reader.read_int32()
        transform_count = reader.read_int32()
        rotation_channel_count = reader.read_int32()
        static_rotation_count = reader.read_int32()
        translation_channel_count = reader.read_int32()
        static_translation_count = reader.read_int32()
        reader.exit_chunk(TAG_INFO)

        transforms = _load_transform_tracks_0002(reader, transform_count)

        if rotation_channel_count > 0:
            reader.enter_form(TAG_AROT)
            try:
                for _ in range(rotation_channel_count):
                    _load_translation_channel(reader)
            finally:
                reader.exit_form(TAG_AROT)

        static_rotations: list[tuple[float, float, float, float]] = []
        if static_rotation_count > 0:
            reader.enter_chunk(TAG_SROT)
            try:
                for _ in range(static_rotation_count):
                    # 0002 stores one float per static rotation channel (Euler component)
                    reader.read_float()
            finally:
                reader.exit_chunk(TAG_SROT)

        translation_channels: list[list[SwgRealKey]] = []
        if translation_channel_count > 0:
            reader.enter_form(TAG_ATRN)
            try:
                for _ in range(translation_channel_count):
                    translation_channels.append(_load_translation_channel(reader))
            finally:
                reader.exit_form(TAG_ATRN)

        static_translations: list[float] = []
        if static_translation_count > 0:
            reader.enter_chunk(TAG_STRN)
            try:
                for _ in range(static_translation_count):
                    static_translations.append(reader.read_float())
            finally:
                reader.exit_chunk(TAG_STRN)

        while not reader.at_end_of_form():
            reader.skip_block()

        return SwgClip(
            fps=fps,
            frame_count=frame_count,
            transforms=transforms,
            translation_channels=translation_channels,
            static_translations=static_translations,
        )
    finally:
        reader.exit_form(TAG_0002)