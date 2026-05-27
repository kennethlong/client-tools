"""Build IFF bytes — mirrors sharedFile/Iff.cpp insertForm/insertChunk."""

from __future__ import annotations

import struct

from swg_iff.tags import TAG_FORM, tag_from_str


def _be_u32(value: int) -> bytes:
    return struct.pack(">I", value & 0xFFFFFFFF)


def _le_i32(value: int) -> bytes:
    return struct.pack("<i", value)


def _le_u32(value: int) -> bytes:
    return struct.pack("<I", value & 0xFFFFFFFF)


def _le_u16(value: int) -> bytes:
    return struct.pack("<H", value & 0xFFFF)


def _le_i16(value: int) -> bytes:
    return struct.pack("<h", value)


def _le_f32(value: float) -> bytes:
    return struct.pack("<f", value)


def make_chunk(chunk_tag: str, payload: bytes) -> bytes:
    t = tag_from_str(chunk_tag)
    return _be_u32(t) + _be_u32(len(payload)) + payload


def make_form(form_type: str, children: bytes) -> bytes:
    inner = _be_u32(tag_from_str(form_type)) + children
    return _be_u32(TAG_FORM) + _be_u32(len(inner)) + inner


def make_empty_form(form_type: str) -> bytes:
    return make_form(form_type, b"")


def make_chunk_int32(value: int) -> bytes:
    return _le_i32(value)


def make_chunk_uint32(value: int) -> bytes:
    return _le_u32(value)


def make_chunk_int16(value: int) -> bytes:
    return _le_i16(value)


def make_chunk_uint16(value: int) -> bytes:
    return _le_u16(value)


def make_chunk_bool8(value: bool) -> bytes:
    return bytes([1 if value else 0])


def make_chunk_int8(value: int) -> bytes:
    return struct.pack("<b", value)


def make_chunk_uint8(value: int) -> bytes:
    return struct.pack("<B", value & 0xFF)


def make_chunk_float(value: float) -> bytes:
    return _le_f32(value)


def make_chunk_float_vector(x: float, y: float, z: float) -> bytes:
    return _le_f32(x) + _le_f32(y) + _le_f32(z)


def make_chunk_quaternion(x: float, y: float, z: float, w: float) -> bytes:
    return _le_f32(x) + _le_f32(y) + _le_f32(z) + _le_f32(w)


def make_chunk_string(value: str) -> bytes:
    return value.encode("ascii", errors="replace") + b"\x00"


def make_info_chunk(ints: list[int]) -> bytes:
    payload = b"".join(_le_i32(v) for v in ints)
    return make_chunk("INFO", payload)


def make_mesh_shell(version: str = "0005") -> bytes:
    """Minimal MESH appearance skeleton (no real geometry)."""
    sps = make_form(
        "SPS ",
        make_form(
            version,
            make_chunk("CNT ", _le_i32(0)),
        ),
    )
    version_form = make_form(version, sps)
    return make_form("MESH", version_form)