"""Detail appearance templates (DTLA) — load/write (Phase 11)."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

from swg_iff.reader import IffError, IffReader
from swg_iff.tags import (
    TAG_CHLD,
    TAG_DATA,
    TAG_DTLA,
    TAG_INFO,
    TAG_PIVT,
    TAG_RADR,
    TAG_TEST,
    TAG_WRIT,
    tag_from_str,
    tag_to_str,
)
from swg_iff.writer import (
    make_chunk,
    make_chunk_float,
    make_chunk_int32,
    make_chunk_string,
    make_chunk_uint8,
    make_form,
)


@dataclass
class SwgDetailLevel:
    level_id: int
    near_distance: float
    far_distance: float
    appearance_path: str = ""


@dataclass
class SwgDetailAppearance:
    version: str = "0008"
    appearance_raw: bytes = b""
    pivot_flags: int = 0
    levels: list[SwgDetailLevel] = field(default_factory=list)
    radar_raw: bytes | None = None
    test_raw: bytes | None = None
    write_raw: bytes | None = None


def _load_optional_form(reader: IffReader, tag_name: str) -> bytes | None:
    block = reader._peek_block()
    if not block.is_form or block.form_type_str.strip() != tag_name:
        return None
    return reader.read_raw_block()


def _load_entries(reader: IffReader) -> list[SwgDetailLevel]:
    levels: list[SwgDetailLevel] = []
    reader.enter_chunk(TAG_INFO)
    try:
        while reader.chunk_bytes_remaining() >= 12:
            level_id = reader.read_int32()
            near_distance = reader.read_float()
            far_distance = reader.read_float()
            levels.append(
                SwgDetailLevel(
                    level_id=level_id,
                    near_distance=near_distance,
                    far_distance=far_distance,
                )
            )
    finally:
        reader.exit_chunk(TAG_INFO)

    reader.enter_form(TAG_DATA)
    try:
        by_id = {level.level_id: level for level in levels}
        while not reader.at_end_of_form():
            reader.enter_chunk(TAG_CHLD)
            try:
                level_id = reader.read_int32()
                path = reader.read_string()
            finally:
                reader.exit_chunk(TAG_CHLD)
            if level_id in by_id:
                by_id[level_id].appearance_path = path
    finally:
        reader.exit_form(TAG_DATA)
    return levels


def load_detail_appearance(path: str | Path) -> SwgDetailAppearance:
    return load_detail_appearance_bytes(Path(path).read_bytes())


def load_detail_appearance_bytes(data: bytes) -> SwgDetailAppearance:
    reader = IffReader(data)
    reader.enter_form(TAG_DTLA)
    try:
        version = tag_to_str(reader.current_name())
        version_int = int(version) if version.isdigit() else 0
        if version_int < 1 or version_int > 8:
            raise IffError(f"unsupported DTLA version {version!r}")
        reader.enter_form(tag_from_str(version))
        try:
            appearance_raw = b""
            pivot_flags = 0
            levels: list[SwgDetailLevel] = []
            radar_raw = None
            test_raw = None
            write_raw = None
            while not reader.at_end_of_form():
                block = reader._peek_block()
                if block.is_form and block.form_type_str.strip() == "APPR":
                    appearance_raw = reader.read_raw_block()
                elif not block.is_form and block.tag_str.strip() == "PIVT":
                    reader.enter_chunk(TAG_PIVT)
                    try:
                        pivot_flags = reader.read_uint8()
                    finally:
                        reader.exit_chunk(TAG_PIVT)
                elif not block.is_form and block.tag_str.strip() == "INFO":
                    levels = _load_entries(reader)
                elif block.is_form and block.form_type_str.strip() == "RADR":
                    radar_raw = reader.read_raw_block()
                elif block.is_form and block.form_type_str.strip() == "TEST":
                    test_raw = reader.read_raw_block()
                elif block.is_form and block.form_type_str.strip() == "WRIT":
                    write_raw = reader.read_raw_block()
                else:
                    reader.skip_block()
            return SwgDetailAppearance(
                version=version,
                appearance_raw=appearance_raw,
                pivot_flags=pivot_flags,
                levels=levels,
                radar_raw=radar_raw,
                test_raw=test_raw,
                write_raw=write_raw,
            )
        finally:
            reader.exit_form(tag_from_str(version))
    finally:
        reader.exit_form(TAG_DTLA)


def _write_entries(levels: list[SwgDetailLevel]) -> bytes:
    info_payload = b""
    data_chunks = b""
    for level in levels:
        info_payload += (
            make_chunk_int32(level.level_id)
            + make_chunk_float(level.near_distance)
            + make_chunk_float(level.far_distance)
        )
        data_chunks += make_chunk(
            "CHLD",
            make_chunk_int32(level.level_id) + make_chunk_string(level.appearance_path),
        )
    return make_chunk("INFO", info_payload) + make_form("DATA", data_chunks)


def _empty_shape_form(tag: str) -> bytes:
    return make_form(tag, make_chunk("INFO", make_chunk_int32(0)))


def write_detail_appearance_bytes(spec: SwgDetailAppearance) -> bytes:
    version = spec.version if spec.version.isdigit() and 1 <= int(spec.version) <= 8 else "0008"
    version_int = int(version)
    body = b""
    if spec.appearance_raw and version_int >= 4:
        body += spec.appearance_raw
    if version_int >= 6:
        body += make_chunk("PIVT", make_chunk_uint8(spec.pivot_flags & 0xFF))
    body += _write_entries(spec.levels)
    if version_int >= 7:
        body += spec.radar_raw if spec.radar_raw else _empty_shape_form("RADR")
    if version_int >= 2:
        body += spec.test_raw if spec.test_raw else _empty_shape_form("TEST")
        body += spec.write_raw if spec.write_raw else _empty_shape_form("WRIT")
    return make_form("DTLA", make_form(version, body))


def write_detail_appearance(path: str | Path, spec: SwgDetailAppearance) -> None:
    Path(path).write_bytes(write_detail_appearance_bytes(spec))


def build_detail_appearance(
    levels: list[SwgDetailLevel],
    *,
    meshes: list | None = None,
    hardpoints: list | None = None,
    version: str = "0008",
    use_pivot_point: bool = False,
    disable_lod_crossfade: bool = False,
) -> SwgDetailAppearance:
    """Build DTLA/0008 from LOD level table + optional APPR bounds."""
    from swg_scene.appearance_export import write_appearance_0003
    from swg_scene.model import SwgMesh

    appearance_raw = b""
    if meshes:
        mesh_list = [m for m in meshes if isinstance(m, SwgMesh)]
        if mesh_list:
            appearance_raw = write_appearance_0003(
                meshes=mesh_list,
                hardpoints=hardpoints or [],
            )
    flags = 0
    if use_pivot_point:
        flags |= 1
    if disable_lod_crossfade:
        flags |= 2
    return SwgDetailAppearance(
        version=version,
        appearance_raw=appearance_raw,
        pivot_flags=flags,
        levels=levels,
    )
