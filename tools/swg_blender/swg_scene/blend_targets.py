"""SKMG blend targets (BLT/BLTS) load + export."""

from __future__ import annotations

from swg_iff.reader import IffReader
from swg_iff.tags import TAG_BLT, TAG_BLTS, TAG_DOT3, TAG_HPTS, TAG_INFO, TAG_NORM, TAG_POSN
from swg_iff.writer import (
    make_chunk,
    make_chunk_float,
    make_chunk_float_vector,
    make_chunk_int16,
    make_chunk_int32,
    make_chunk_string,
    make_form,
)
from swg_scene.model import SwgBlendTarget, SwgBlendTargetHardpoint, SwgBlendVector

BLEND_EPSILON = 1e-4


def _load_blend_vectors(reader: IffReader, count: int) -> list[SwgBlendVector]:
    return [
        SwgBlendVector(index=reader.read_int32(), delta=reader.read_float_vector())
        for _ in range(count)
    ]


def load_blend_target(reader: IffReader) -> SwgBlendTarget:
    reader.enter_form(TAG_BLT)
    try:
        reader.enter_chunk(TAG_INFO)
        position_count = reader.read_int32()
        normal_count = reader.read_int32()
        name = reader.read_string()
        reader.exit_chunk(TAG_INFO)

        position_deltas: list[SwgBlendVector] = []
        if position_count > 0:
            reader.enter_chunk(TAG_POSN)
            position_deltas = _load_blend_vectors(reader, position_count)
            reader.exit_chunk(TAG_POSN)

        normal_deltas: list[SwgBlendVector] = []
        if normal_count > 0:
            reader.enter_chunk(TAG_NORM)
            normal_deltas = _load_blend_vectors(reader, normal_count)
            reader.exit_chunk(TAG_NORM)

        dot3_deltas: list[SwgBlendVector] = []
        if not reader.at_end_of_form():
            block = reader._peek_block()
            if not block.is_form and block.tag == TAG_DOT3:
                reader.enter_chunk(TAG_DOT3)
                dot3_count = reader.read_int32()
                dot3_deltas = _load_blend_vectors(reader, dot3_count)
                reader.exit_chunk(TAG_DOT3)

        hardpoint_targets: list[SwgBlendTargetHardpoint] = []
        if not reader.at_end_of_form():
            block = reader._peek_block()
            if not block.is_form and block.tag == TAG_HPTS:
                reader.enter_chunk(TAG_HPTS)
                hp_count = reader.read_int16()
                for _ in range(hp_count):
                    hardpoint_targets.append(
                        SwgBlendTargetHardpoint(
                            dynamic_hardpoint_index=reader.read_int16(),
                            delta_position=reader.read_float_vector(),
                            delta_rotation=reader.read_float_vector() + (reader.read_float(),),
                        )
                    )
                reader.exit_chunk(TAG_HPTS)

        return SwgBlendTarget(
            name=name,
            position_deltas=position_deltas,
            normal_deltas=normal_deltas,
            dot3_deltas=dot3_deltas,
            hardpoint_targets=hardpoint_targets,
        )
    finally:
        reader.exit_form(TAG_BLT)


def load_blts(reader: IffReader, count: int) -> list[SwgBlendTarget]:
    if count <= 0:
        return []
    reader.enter_form(TAG_BLTS)
    try:
        return [load_blend_target(reader) for _ in range(count)]
    finally:
        reader.exit_form(TAG_BLTS)


def _write_blend_vectors(vectors: list[SwgBlendVector]) -> bytes:
    payload = b""
    for item in vectors:
        payload += make_chunk_int32(item.index) + make_chunk_float_vector(*item.delta)
    return payload


def write_blend_target(target: SwgBlendTarget) -> bytes:
    info_payload = (
        make_chunk_int32(len(target.position_deltas))
        + make_chunk_int32(len(target.normal_deltas))
        + make_chunk_string(target.name)
    )
    body = make_chunk("INFO", info_payload)
    if target.position_deltas:
        body += make_chunk("POSN", _write_blend_vectors(target.position_deltas))
    if target.normal_deltas:
        body += make_chunk("NORM", _write_blend_vectors(target.normal_deltas))
    if target.dot3_deltas:
        body += make_chunk(
            "DOT3",
            make_chunk_int32(len(target.dot3_deltas)) + _write_blend_vectors(target.dot3_deltas),
        )
    if target.hardpoint_targets:
        hp_payload = make_chunk_int16(len(target.hardpoint_targets))
        for hp in target.hardpoint_targets:
            hp_payload += make_chunk_int16(hp.dynamic_hardpoint_index)
            hp_payload += make_chunk_float_vector(*hp.delta_position)
            x, y, z, w = hp.delta_rotation
            hp_payload += make_chunk_float_vector(x, y, z) + make_chunk_float(w)
        body += make_chunk("HPTS", hp_payload)
    return make_form("BLT ", body)


def write_blts(targets: list[SwgBlendTarget]) -> bytes:
    if not targets:
        return b""
    return make_form("BLTS", b"".join(write_blend_target(t) for t in targets))


def _significant(delta: tuple[float, float, float], epsilon: float = BLEND_EPSILON) -> bool:
    return any(abs(v) >= epsilon for v in delta)


def build_blend_targets_from_pool_deltas(
    *,
    names: list[str],
    bind_positions: list[tuple[float, float, float]],
    bind_normals: list[tuple[float, float, float]],
    shape_positions: list[list[tuple[float, float, float]]],
    shape_normals: list[list[tuple[float, float, float]]],
) -> list[SwgBlendTarget]:
    """Build sparse BLT entries from bind vs shape-key pool arrays (engine coords)."""
    targets: list[SwgBlendTarget] = []
    pool_count = len(bind_positions)
    for name, positions, normals in zip(names, shape_positions, shape_normals):
        pos_deltas: list[SwgBlendVector] = []
        norm_deltas: list[SwgBlendVector] = []
        for index in range(pool_count):
            pd = (
                positions[index][0] - bind_positions[index][0],
                positions[index][1] - bind_positions[index][1],
                positions[index][2] - bind_positions[index][2],
            )
            if _significant(pd):
                pos_deltas.append(SwgBlendVector(index=index, delta=pd))
            if index < len(bind_normals) and index < len(normals):
                nd = (
                    normals[index][0] - bind_normals[index][0],
                    normals[index][1] - bind_normals[index][1],
                    normals[index][2] - bind_normals[index][2],
                )
                if _significant(nd):
                    norm_deltas.append(SwgBlendVector(index=index, delta=nd))
        if pos_deltas or norm_deltas:
            targets.append(
                SwgBlendTarget(name=name, position_deltas=pos_deltas, normal_deltas=norm_deltas)
            )
    return targets