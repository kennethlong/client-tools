"""CKAT compressed keyframe animation writer (CompressedKeyframeAnimationTemplate 0001)."""

from __future__ import annotations

from swg_iff.writer import (
    make_chunk,
    make_chunk_bool8,
    make_chunk_float,
    make_chunk_int16,
    make_chunk_string,
    make_chunk_uint32,
    make_chunk_uint8,
    make_form,
)
from swg_scene.compressed_quaternion import (
    compress_quaternion,
    compress_rotations,
    get_optimal_compression_format_for_rotations,
)
from swg_scene.model import SwgClip, SwgQuatKey, SwgRealKey


def _stream_key_count(sparse_key_count: int, frame_count: int) -> int:
    if sparse_key_count <= 1:
        return sparse_key_count
    return frame_count + 1


def _quaternion_at_frame(keys: list[SwgQuatKey], frame: int) -> tuple[float, float, float, float]:
    if not keys:
        return (0.0, 0.0, 0.0, 1.0)
    lookup = {key.frame: key.rotation for key in keys}
    if frame in lookup:
        return lookup[frame]
    prior_frames = [f for f in lookup if f <= frame]
    if prior_frames:
        return lookup[max(prior_frames)]
    return keys[0].rotation


def _value_at_frame(keys: list[SwgRealKey], frame: int) -> float:
    if not keys:
        return 0.0
    lookup = {key.frame: key.value for key in keys}
    if frame in lookup:
        return lookup[frame]
    prior_frames = [f for f in lookup if f <= frame]
    if prior_frames:
        return lookup[max(prior_frames)]
    return keys[0].value


def _expand_rotation_stream(
    keys: list[SwgQuatKey],
    frame_count: int,
    x_format: int,
    y_format: int,
    z_format: int,
) -> list[tuple[int, int]]:
    key_count = _stream_key_count(len(keys), frame_count)
    if key_count == 0:
        return []
    if key_count == 1:
        rotation = _quaternion_at_frame(keys, 0)
        compressed = compress_quaternion(*rotation, x_format, y_format, z_format)
        return [(0, compressed)]

    dense_rotations = [_quaternion_at_frame(keys, frame) for frame in range(key_count)]
    compressed_values = compress_rotations(dense_rotations, x_format, y_format, z_format)
    return list(enumerate(compressed_values))


def _expand_translation_stream(keys: list[SwgRealKey], frame_count: int) -> list[tuple[int, float]]:
    key_count = _stream_key_count(len(keys), frame_count)
    if key_count == 0:
        return []
    return [(frame, _value_at_frame(keys, frame)) for frame in range(key_count)]


def _write_rotation_channel(
    keys: list[SwgQuatKey],
    frame_count: int,
) -> bytes:
    if not keys:
        return make_chunk("QCHN", make_chunk_int16(0))

    sample_rotations = [_quaternion_at_frame(keys, frame) for frame in range(_stream_key_count(len(keys), frame_count))]
    x_format, y_format, z_format = get_optimal_compression_format_for_rotations(sample_rotations)
    dense_keys = _expand_rotation_stream(keys, frame_count, x_format, y_format, z_format)

    payload = (
        make_chunk_int16(len(dense_keys))
        + make_chunk_uint8(x_format)
        + make_chunk_uint8(y_format)
        + make_chunk_uint8(z_format)
    )
    for frame, compressed in dense_keys:
        payload += make_chunk_int16(frame) + make_chunk_uint32(compressed)
    return make_chunk("QCHN", payload)


def _write_translation_channel(keys: list[SwgRealKey], frame_count: int) -> bytes:
    dense_keys = _expand_translation_stream(keys, frame_count)
    payload = make_chunk_int16(len(dense_keys))
    for frame, value in dense_keys:
        payload += make_chunk_int16(frame) + make_chunk_float(value)
    return make_chunk("CHNL", payload)


def write_ckat(clip: SwgClip, *, version: str | None = None) -> bytes:
    version = version or clip.version or "0001"
    if version != "0001":
        raise ValueError(f"unsupported CKAT export version {version!r}; only 0001 is implemented")

    info_payload = (
        make_chunk_float(clip.fps)
        + make_chunk_int16(clip.frame_count)
        + make_chunk_int16(len(clip.transforms))
        + make_chunk_int16(len(clip.rotation_channels))
        + make_chunk_int16(len(clip.static_rotations))
        + make_chunk_int16(len(clip.translation_channels))
        + make_chunk_int16(len(clip.static_translations))
    )
    info = make_chunk("INFO", info_payload)

    xfin_chunks: list[bytes] = []
    for track in clip.transforms:
        xfin_payload = (
            make_chunk_string(track.name)
            + make_chunk_bool8(track.has_animated_rotation)
            + make_chunk_int16(track.rotation_channel_index)
            + make_chunk_uint8(track.translation_mask & 0xFF)
            + make_chunk_int16(track.x_translation_channel_index)
            + make_chunk_int16(track.y_translation_channel_index)
            + make_chunk_int16(track.z_translation_channel_index)
        )
        xfin_chunks.append(make_chunk("XFIN", xfin_payload))
    xfrm = make_form("XFRM", b"".join(xfin_chunks))

    qchn_chunks = [
        _write_rotation_channel(channel, clip.frame_count) for channel in clip.rotation_channels
    ]
    arot = make_form("AROT", b"".join(qchn_chunks)) if qchn_chunks else b""

    srot_payload = b""
    for rotation in clip.static_rotations:
        x_format, y_format, z_format = get_optimal_compression_format_for_rotations([rotation])
        compressed = compress_quaternion(*rotation, x_format, y_format, z_format)
        srot_payload += (
            make_chunk_uint8(x_format)
            + make_chunk_uint8(y_format)
            + make_chunk_uint8(z_format)
            + make_chunk_uint32(compressed)
        )
    srot = make_chunk("SROT", srot_payload) if srot_payload else b""

    chnl_chunks = [
        _write_translation_channel(channel, clip.frame_count)
        for channel in clip.translation_channels
    ]
    atrn = make_form("ATRN", b"".join(chnl_chunks)) if chnl_chunks else b""

    strn_payload = b"".join(make_chunk_float(value) for value in clip.static_translations)
    strn = make_chunk("STRN", strn_payload) if strn_payload else b""

    body = info + xfrm + arot + srot + atrn + strn
    return make_form("CKAT", make_form("0001", body))