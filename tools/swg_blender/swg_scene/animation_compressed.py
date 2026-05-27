"""CKAT compressed keyframe animation loader (CompressedKeyframeAnimationTemplate 0001)."""

from __future__ import annotations

import math
from pathlib import Path

from swg_iff.reader import IffReader, IffError
from swg_iff.tags import (
    TAG_0001,
    TAG_AROT,
    TAG_ATRN,
    TAG_CHNL,
    TAG_CKAT,
    TAG_INFO,
    TAG_QCHN,
    TAG_SROT,
    TAG_STRN,
    TAG_XFIN,
    TAG_XFRM,
    tag_to_str,
)
from swg_scene.compressed_quaternion import expand_quaternion
from swg_scene.model import SwgClip, SwgQuatKey, SwgRealKey, SwgTransformTrack

_TRANSLATION_EPS = math.ulp(1.0) * 2.0


def load_ckat(path: str | Path) -> SwgClip:
    p = Path(path)
    reader = IffReader.from_file(p)
    clip = _load_ckat_reader(reader)
    clip.source_path = str(p)
    if not clip.name:
        clip.name = p.stem
    return clip


def load_ckat_bytes(data: bytes, *, path: str = "<memory>") -> SwgClip:
    reader = IffReader(data, path)
    return _load_ckat_reader(reader)


def _load_ckat_reader(reader: IffReader) -> SwgClip:
    reader.enter_form(TAG_CKAT)
    try:
        version_tag = reader.current_name()
        if version_tag != TAG_0001:
            raise IffError(f"unsupported CKAT version {tag_to_str(version_tag)}")
        return _load_0001(reader)
    finally:
        reader.exit_form(TAG_CKAT)


def _load_transform_tracks(reader: IffReader, transform_count: int) -> list[SwgTransformTrack]:
    reader.enter_form(TAG_XFRM)
    try:
        tracks: list[SwgTransformTrack] = []
        for _ in range(transform_count):
            reader.enter_chunk(TAG_XFIN)
            try:
                name = reader.read_string()
                has_animated_rotation = reader.read_int8() != 0
                rotation_channel_index = reader.read_int16()
                translation_mask = reader.read_uint8()
                x_index = reader.read_int16()
                y_index = reader.read_int16()
                z_index = reader.read_int16()
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


def _load_rotation_channel(reader: IffReader) -> list[SwgQuatKey]:
    reader.enter_chunk(TAG_QCHN)
    try:
        key_count = reader.read_int16()
        x_format = reader.read_uint8()
        y_format = reader.read_uint8()
        z_format = reader.read_uint8()
        keys: list[SwgQuatKey] = []
        last_compressed = 0
        for i in range(key_count):
            frame = reader.read_int16()
            compressed = reader.read_uint32()
            keep = (i == 0) or (i == key_count - 1) or not (i & 1)
            if keep and compressed != last_compressed:
                rotation = expand_quaternion(compressed, x_format, y_format, z_format)
                keys.append(SwgQuatKey(frame=frame, rotation=rotation))
            last_compressed = compressed
        return keys
    finally:
        reader.exit_chunk(TAG_QCHN)


def _load_translation_channel(reader: IffReader) -> list[SwgRealKey]:
    reader.enter_chunk(TAG_CHNL)
    try:
        key_count = reader.read_int16()
        midpoint = key_count // 2
        keys: list[SwgRealKey] = []
        last_value = float("inf")
        for i in range(key_count):
            frame = reader.read_int16()
            value = reader.read_float()
            if (
                i == 0
                or i == key_count - 1
                or i == midpoint
                or abs(value - last_value) > _TRANSLATION_EPS
            ):
                keys.append(SwgRealKey(frame=frame, value=value))
                last_value = value
        return keys
    finally:
        reader.exit_chunk(TAG_CHNL)


def _optional_form(reader: IffReader, tag: int) -> bool:
    block = reader._peek_block()
    if block.is_form and block.form_type == tag:
        reader.enter_form(tag)
        return True
    return False


def _optional_chunk(reader: IffReader, tag: int) -> bool:
    block = reader._peek_block()
    if not block.is_form and block.tag == tag:
        reader.enter_chunk(tag)
        return True
    return False


def _load_0001(reader: IffReader) -> SwgClip:
    reader.enter_form(TAG_0001)
    try:
        reader.enter_chunk(TAG_INFO)
        fps = reader.read_float()
        frame_count = reader.read_int16()
        transform_count = reader.read_int16()
        rotation_channel_count = reader.read_int16()
        static_rotation_count = reader.read_int16()
        translation_channel_count = reader.read_int16()
        static_translation_count = reader.read_int16()
        reader.exit_chunk(TAG_INFO)

        transforms = _load_transform_tracks(reader, transform_count)

        rotation_channels: list[list[SwgQuatKey]] = []
        if rotation_channel_count > 0:
            if not _optional_form(reader, TAG_AROT):
                raise IffError(f"CKAT INFO expects {rotation_channel_count} rotation channels")
            try:
                for _ in range(rotation_channel_count):
                    rotation_channels.append(_load_rotation_channel(reader))
            finally:
                reader.exit_form(TAG_AROT)

        static_rotations: list[tuple[float, float, float, float]] = []
        if static_rotation_count > 0:
            if not _optional_chunk(reader, TAG_SROT):
                raise IffError(f"CKAT INFO expects {static_rotation_count} static rotations")
            try:
                for _ in range(static_rotation_count):
                    x_format = reader.read_uint8()
                    y_format = reader.read_uint8()
                    z_format = reader.read_uint8()
                    compressed = reader.read_uint32()
                    static_rotations.append(
                        expand_quaternion(compressed, x_format, y_format, z_format)
                    )
            finally:
                reader.exit_chunk(TAG_SROT)

        translation_channels: list[list[SwgRealKey]] = []
        if translation_channel_count > 0:
            if not _optional_form(reader, TAG_ATRN):
                raise IffError(
                    f"CKAT INFO expects {translation_channel_count} translation channels"
                )
            try:
                for _ in range(translation_channel_count):
                    translation_channels.append(_load_translation_channel(reader))
            finally:
                reader.exit_form(TAG_ATRN)

        static_translations: list[float] = []
        if static_translation_count > 0:
            if not _optional_chunk(reader, TAG_STRN):
                raise IffError(
                    f"CKAT INFO expects {static_translation_count} static translations"
                )
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
            rotation_channels=rotation_channels,
            static_rotations=static_rotations,
            translation_channels=translation_channels,
            static_translations=static_translations,
            version="0001",
        )
    finally:
        reader.exit_form(TAG_0001)