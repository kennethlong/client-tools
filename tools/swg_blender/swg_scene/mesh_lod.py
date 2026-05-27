"""MLOD mesh LOD template loader/writer (.lmg)."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

from swg_iff.reader import IffReader, IffError
from swg_iff.tags import TAG_0000, TAG_INFO, TAG_MLOD, TAG_NAME, tag_to_str
from swg_iff.writer import make_chunk, make_chunk_int16, make_chunk_string, make_form


@dataclass
class SwgMeshLod:
    level_paths: list[str] = field(default_factory=list)


def load_mesh_lod(path: str | Path) -> SwgMeshLod:
    reader = IffReader.from_file(path)
    reader.enter_form(TAG_MLOD)
    try:
        version_tag = reader.current_name()
        if version_tag != TAG_0000:
            raise IffError(f"unsupported MLOD version {tag_to_str(version_tag)}")
        reader.enter_form(TAG_0000)
        try:
            reader.enter_chunk(TAG_INFO)
            level_count = reader.read_int16()
            reader.exit_chunk(TAG_INFO)
            level_paths: list[str] = []
            for _ in range(level_count):
                reader.enter_chunk(TAG_NAME)
                level_paths.append(reader.read_string())
                reader.exit_chunk(TAG_NAME)
            return SwgMeshLod(level_paths=level_paths)
        finally:
            reader.exit_form(TAG_0000)
    finally:
        reader.exit_form(TAG_MLOD)


def write_mesh_lod(lod: SwgMeshLod) -> bytes:
    if not lod.level_paths:
        raise ValueError("mesh LOD requires at least one detail level path")

    body = make_chunk("INFO", make_chunk_int16(len(lod.level_paths)))
    body += b"".join(make_chunk("NAME", make_chunk_string(path)) for path in lod.level_paths)
    return make_form("MLOD", make_form("0000", body))


def write_mesh_lod_file(path: str | Path, lod: SwgMeshLod) -> None:
    Path(path).write_bytes(write_mesh_lod(lod))
