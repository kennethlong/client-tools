"""VTXA vertex buffer decode — port of clientGraphics/VertexBuffer.cpp (load_0003)."""

from __future__ import annotations

from dataclasses import dataclass

from swg_iff.reader import IffReader, IffError
from swg_iff.tags import TAG_0003, TAG_DATA, TAG_INFO, TAG_VTXA

F_POSITION = 1 << 0
F_TRANSFORMED = 1 << 1
F_NORMAL = 1 << 2
F_COLOR0 = 1 << 3
F_COLOR1 = 1 << 4
F_POINT_SIZE = 1 << 5

TC_COUNT_SHIFT = 8
TC_COUNT_MASK = 0xF
TC_DIM_BASE_SHIFT = 12
TC_DIM_PER_SET_SHIFT = 2
TC_DIM_MASK = 0x3


@dataclass
class VertexBufferData:
    flags: int
    positions: list[tuple[float, float, float]]
    normals: list[tuple[float, float, float]]
    uvs: list[list[tuple[float, float]]]
    colors0: list[int]

    @property
    def vertex_count(self) -> int:
        return len(self.positions)


def _tc_set_count(flags: int) -> int:
    return (flags >> TC_COUNT_SHIFT) & TC_COUNT_MASK


def _tc_set_dimension(flags: int, set_index: int) -> int:
    shift = TC_DIM_BASE_SHIFT + (set_index * TC_DIM_PER_SET_SHIFT)
    return ((flags >> shift) & TC_DIM_MASK) + 1


def read_vertex_buffer_vtxa(reader: IffReader) -> VertexBufferData:
    """Read a VTXA form (currently supports inner version 0003)."""
    reader.enter_form(TAG_VTXA)
    try:
        name = reader.current_name()
        if name != TAG_0003:
            raise IffError(
                f"unsupported VTXA version {reader._peek_block().form_type_str!r} "
                f"(only 0003 implemented)"
            )
        return _load_vtxa_0003(reader)
    finally:
        reader.exit_form(TAG_VTXA)


def _load_vtxa_0003(reader: IffReader) -> VertexBufferData:
    reader.enter_form(TAG_0003)
    try:
        reader.enter_chunk(TAG_INFO)
        flags = reader.read_uint32()
        number_of_vertices = reader.read_int32()
        if number_of_vertices <= 0:
            raise IffError("vertex buffer has zero vertices")
        reader.exit_chunk(TAG_INFO)

        tc_count = _tc_set_count(flags)
        tc_dims = [_tc_set_dimension(flags, j) for j in range(tc_count)]

        positions: list[tuple[float, float, float]] = []
        normals: list[tuple[float, float, float]] = []
        colors0: list[int] = []
        uvs: list[list[tuple[float, float]]] = [[] for _ in range(tc_count)]

        reader.enter_chunk(TAG_DATA)
        for _ in range(number_of_vertices):
            if flags & F_POSITION:
                positions.append(reader.read_float_vector())
            if flags & F_TRANSFORMED:
                reader.read_float()
            if flags & F_NORMAL:
                normals.append(reader.read_float_vector())
            if flags & F_POINT_SIZE:
                reader.read_float()
            if flags & F_COLOR0:
                colors0.append(reader.read_uint32())
            if flags & F_COLOR1:
                reader.read_uint32()
            for j, dim in enumerate(tc_dims):
                coords = [reader.read_float() for _ in range(dim)]
                if dim >= 4:
                    uvs[j].append(tuple(coords[:4]))
                elif dim >= 2:
                    uvs[j].append((coords[0], coords[1]))
                elif dim == 1:
                    uvs[j].append((coords[0], 0.0))
                else:
                    uvs[j].append((0.0, 0.0))
        if reader.chunk_bytes_remaining():
            raise IffError(
                f"VTXA DATA chunk has {reader.chunk_bytes_remaining()} trailing bytes"
            )
        reader.exit_chunk(TAG_DATA)

        if not positions:
            raise IffError("VTXA missing position channel")

        return VertexBufferData(
            flags=flags,
            positions=positions,
            normals=normals or [(0.0, 0.0, 1.0)] * len(positions),
            uvs=uvs,
            colors0=colors0,
        )
    finally:
        reader.exit_form(TAG_0003)


def read_index_buffer_uint16(reader: IffReader) -> list[int]:
    """INDX chunk: int32 count followed by uint16 indices."""
    count = reader.read_int32()
    return [reader.read_uint16() for _ in range(count)]



def build_vtxa_flags(
    uv_sets: list[list[tuple[float, float]]],
    *,
    has_color0: bool = False,
) -> int:
    flags = F_POSITION | F_NORMAL
    if has_color0:
        flags |= F_COLOR0
    tc_count = len(uv_sets)
    if tc_count:
        flags |= (tc_count & TC_COUNT_MASK) << TC_COUNT_SHIFT
        for index, layer in enumerate(uv_sets):
            if layer and len(layer[0]) == 4:
                dim = 4
            elif layer:
                dim = 2
            else:
                dim = 1
            flags |= ((dim - 1) & TC_DIM_MASK) << (
                TC_DIM_BASE_SHIFT + index * TC_DIM_PER_SET_SHIFT
            )
    return flags


def write_vertex_buffer_vtxa(
    positions: list[tuple[float, float, float]],
    normals: list[tuple[float, float, float]],
    uvs: list[list[tuple[float, float]]],
    colors0: list[int] | None = None,
) -> bytes:
    from swg_iff.writer import (
        make_chunk,
        make_chunk_float,
        make_chunk_float_vector,
        make_chunk_int32,
        make_chunk_uint32,
        make_form,
    )

    from swg_scene.color_utils import colors_are_default

    if not positions:
        raise ValueError("VTXA requires at least one position")
    if len(normals) != len(positions):
        raise ValueError("normal count must match position count")
    uv_sets = uvs or [[]]
    for layer in uv_sets:
        if len(layer) not in (0, len(positions)):
            raise ValueError("each UV layer must match vertex count")

    use_colors = colors0 is not None and not colors_are_default(colors0)
    if use_colors and len(colors0) != len(positions):
        raise ValueError("color0 count must match position count")

    flags = build_vtxa_flags(uv_sets, has_color0=use_colors)
    tc_dims = [_tc_set_dimension(flags, j) for j in range(_tc_set_count(flags))]

    info = make_chunk_uint32(flags) + make_chunk_int32(len(positions))
    data = b""
    for index in range(len(positions)):
        data += make_chunk_float_vector(*positions[index])
        data += make_chunk_float_vector(*normals[index])
        if use_colors:
            data += make_chunk_uint32(colors0[index])
        for set_index, dim in enumerate(tc_dims):
            uv = uv_sets[set_index][index] if uv_sets[set_index] else (0.0, 0.0)
            if dim >= 4:
                data += make_chunk_float(uv[0]) + make_chunk_float(uv[1]) + make_chunk_float(uv[2]) + make_chunk_float(uv[3])
            elif dim >= 2:
                data += make_chunk_float(uv[0]) + make_chunk_float(uv[1])
            elif dim == 1:
                data += make_chunk_float(uv[0])

    inner = make_chunk("INFO", info) + make_chunk("DATA", data)
    return make_form("VTXA", make_form("0003", inner))


def write_index_buffer_uint16(indices: list[int]) -> bytes:
    import struct
    from swg_iff.writer import make_chunk_int32

    payload = make_chunk_int32(len(indices))
    payload += b"".join(struct.pack("<H", i & 0xFFFF) for i in indices)
    return payload


def read_sorted_index_buffer(reader: IffReader) -> list[tuple[tuple[float, float, float], list[int]]]:
    array_count = reader.read_int32()
    arrays: list[tuple[tuple[float, float, float], list[int]]] = []
    for _ in range(array_count):
        direction = reader.read_float_vector()
        indices = read_index_buffer_uint16(reader)
        arrays.append((direction, indices))
    return arrays


def write_sorted_index_buffer(
    arrays: list[tuple[tuple[float, float, float], list[int]]],
) -> bytes:
    from swg_iff.writer import make_chunk_float_vector, make_chunk_int32

    payload = make_chunk_int32(len(arrays))
    for direction, indices in arrays:
        payload += make_chunk_float_vector(*direction)
        payload += write_index_buffer_uint16(indices)
    return payload
