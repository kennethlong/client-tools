"""SKMG DOT3 global pool + per-shader index helpers (Phase 9.4)."""

from __future__ import annotations

from swg_iff.reader import IffReader
from swg_iff.tags import TAG_DOT3
from swg_iff.writer import make_chunk, make_chunk_float, make_chunk_int32

Dot3Vector = tuple[float, float, float, float]


def _quantize4(value: Dot3Vector, *, places: int = 6) -> Dot3Vector:
    return tuple(round(c, places) for c in value)  # type: ignore[return-value]


def load_global_dot3(reader: IffReader) -> list[Dot3Vector]:
    block = reader._peek_block()
    if block.is_form or block.tag != TAG_DOT3:
        return []
    reader.enter_chunk(TAG_DOT3)
    try:
        count = reader.read_int32()
        vectors: list[Dot3Vector] = []
        for _ in range(count):
            vectors.append(
                (
                    reader.read_float(),
                    reader.read_float(),
                    reader.read_float(),
                    reader.read_float(),
                )
            )
        return vectors
    finally:
        reader.exit_chunk(TAG_DOT3)


def load_psdt_dot3_indices(reader: IffReader, shader_vert_count: int) -> list[int]:
    block = reader._peek_block()
    if block.is_form or block.tag != TAG_DOT3:
        return []
    reader.enter_chunk(TAG_DOT3)
    try:
        return [reader.read_int32() for _ in range(shader_vert_count)]
    finally:
        reader.exit_chunk(TAG_DOT3)


def resolve_shader_dot3(
    indices: list[int],
    pool: list[Dot3Vector],
) -> list[Dot3Vector]:
    return [pool[index] for index in indices]


def build_dot3_pools(
    shader_dot3: list[Dot3Vector],
) -> tuple[list[Dot3Vector], list[int]]:
    pool: list[Dot3Vector] = []
    index_map: dict[Dot3Vector, int] = {}
    indices: list[int] = []
    for coord in shader_dot3:
        key = _quantize4(coord)
        if key not in index_map:
            index_map[key] = len(pool)
            pool.append(coord)
        indices.append(index_map[key])
    return pool, indices


def write_global_dot3_chunk(vectors: list[Dot3Vector]) -> bytes:
    payload = make_chunk_int32(len(vectors))
    for x, y, z, w in vectors:
        payload += make_chunk_float(x) + make_chunk_float(y) + make_chunk_float(z) + make_chunk_float(w)
    return make_chunk("DOT3", payload)


def dot3_indices_for_pool(
    shader_dot3: list[Dot3Vector],
    pool: list[Dot3Vector],
) -> list[int]:
    index_map = {_quantize4(vector): index for index, vector in enumerate(pool)}
    indices: list[int] = []
    for coord in shader_dot3:
        key = _quantize4(coord)
        if key not in index_map:
            raise ValueError(f"shader DOT3 {key} not found in global pool")
        indices.append(index_map[key])
    return indices


def write_psdt_dot3_indices(indices: list[int]) -> bytes:
    payload = b"".join(make_chunk_int32(i) for i in indices)
    return make_chunk("DOT3", payload)


def shader_dot3_from_mesh_uvs(mesh) -> list[Dot3Vector] | None:
    """Extract per-shader DOT3 coordinates from mesh UV layers."""
    if not mesh.material.has_dot3:
        return None
    dot3_idx = mesh.material.texture_coord_sets.get("DOT3")
    if dot3_idx is not None and dot3_idx < len(mesh.uvs):
        layer = mesh.uvs[dot3_idx]
    elif len(mesh.uvs) >= 2:
        layer = mesh.uvs[1]
    else:
        return None
    if not layer or len(layer[0]) < 4:
        return None
    return [tuple(float(c) for c in coords[:4]) for coords in layer]