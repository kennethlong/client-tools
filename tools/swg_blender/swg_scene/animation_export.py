"""Keyframe skeletal animation writer (KFAT 0003 / CKAT 0001)."""

from __future__ import annotations

from pathlib import Path

from swg_iff.writer import (
    make_chunk,
    make_chunk_bool8,
    make_chunk_float,
    make_chunk_int32,
    make_chunk_quaternion,
    make_chunk_string,
    make_chunk_uint32,
    make_form,
)
from swg_scene.animation_compressed_export import write_ckat
from swg_scene.model import SwgClip


def write_kfat(clip: SwgClip, *, version: str | None = None) -> bytes:
    version = version or clip.version or "0003"
    if version != "0003":
        raise ValueError(f"unsupported KFAT export version {version!r}; only 0003 is implemented")

    info_payload = (
        make_chunk_float(clip.fps)
        + make_chunk_int32(clip.frame_count)
        + make_chunk_int32(len(clip.transforms))
        + make_chunk_int32(len(clip.rotation_channels))
        + make_chunk_int32(len(clip.static_rotations))
        + make_chunk_int32(len(clip.translation_channels))
        + make_chunk_int32(len(clip.static_translations))
    )
    info = make_chunk("INFO", info_payload)

    xfin_chunks: list[bytes] = []
    for track in clip.transforms:
        xfin_payload = (
            make_chunk_string(track.name)
            + make_chunk_bool8(track.has_animated_rotation)
            + make_chunk_int32(track.rotation_channel_index)
            + make_chunk_uint32(track.translation_mask)
            + make_chunk_int32(track.x_translation_channel_index)
            + make_chunk_int32(track.y_translation_channel_index)
            + make_chunk_int32(track.z_translation_channel_index)
        )
        xfin_chunks.append(make_chunk("XFIN", xfin_payload))
    xfrm = make_form("XFRM", b"".join(xfin_chunks))

    qchn_chunks: list[bytes] = []
    for channel in clip.rotation_channels:
        payload = make_chunk_int32(len(channel))
        for key in channel:
            payload += make_chunk_int32(key.frame) + make_chunk_quaternion(*key.rotation)
        qchn_chunks.append(make_chunk("QCHN", payload))
    arot = make_form("AROT", b"".join(qchn_chunks))

    srot_payload = b"".join(make_chunk_quaternion(*rotation) for rotation in clip.static_rotations)
    srot = make_chunk("SROT", srot_payload)

    chnl_chunks: list[bytes] = []
    for channel in clip.translation_channels:
        payload = make_chunk_int32(len(channel))
        for key in channel:
            payload += make_chunk_int32(key.frame) + make_chunk_float(key.value)
        chnl_chunks.append(make_chunk("CHNL", payload))
    atrn = make_form("ATRN", b"".join(chnl_chunks))

    strn_payload = b"".join(make_chunk_float(value) for value in clip.static_translations)
    strn = make_chunk("STRN", strn_payload)

    body = info + xfrm + arot + srot + atrn + strn
    return make_form("KFAT", make_form("0003", body))


def write_animation(clip: SwgClip, *, format: str = "ckat", version: str | None = None) -> bytes:
    if format == "ckat":
        return write_ckat(clip, version=version or "0001")
    if format == "kfat":
        return write_kfat(clip, version=version or "0003")
    raise ValueError(f"unsupported animation export format {format!r}; use 'ckat' or 'kfat'")


def write_animation_file(
    path: str | Path,
    clip: SwgClip,
    *,
    format: str = "ckat",
    version: str | None = None,
) -> None:
    Path(path).write_bytes(write_animation(clip, format=format, version=version))
