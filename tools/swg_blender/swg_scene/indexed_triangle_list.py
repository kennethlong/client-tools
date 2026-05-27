"""Indexed triangle list (IDTL) — Phase 12 collision mesh helper."""

from __future__ import annotations

from swg_iff.reader import IffReader
from swg_iff.tags import tag_from_str
from swg_iff.writer import make_chunk, make_chunk_float, make_chunk_float_vector, make_chunk_int32, make_form


def load_idtl_bytes(data: bytes) -> tuple[list[tuple[float, float, float]], list[int]]:
    reader = IffReader(data)
    return load_idtl_from_reader(reader)


def load_idtl_from_reader(reader: IffReader) -> tuple[list[tuple[float, float, float]], list[int]]:
    reader.enter_form(tag_from_str("IDTL"))
    try:
        reader.enter_form(tag_from_str("0000"))
        try:
            reader.enter_chunk(tag_from_str("VERT"))
            count = reader.chunk_bytes_remaining() // 12
            vertices = [reader.read_float_vector() for _ in range(count)]
            reader.exit_chunk(tag_from_str("VERT"))
            reader.enter_chunk(tag_from_str("INDX"))
            indices = [reader.read_int32() for _ in range(reader.chunk_bytes_remaining() // 4)]
            reader.exit_chunk(tag_from_str("INDX"))
            return vertices, indices
        finally:
            reader.exit_form(tag_from_str("0000"))
    finally:
        reader.exit_form(tag_from_str("IDTL"))


def write_idtl_bytes(
    vertices: list[tuple[float, float, float]],
    indices: list[int],
) -> bytes:
    vert_payload = b"".join(make_chunk_float_vector(*v) for v in vertices)
    indx_payload = b"".join(make_chunk_int32(i) for i in indices)
    body = make_chunk("VERT", vert_payload) + make_chunk("INDX", indx_payload)
    return make_form("IDTL", make_form("0000", body))
