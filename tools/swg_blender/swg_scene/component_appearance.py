"""Component appearance templates (CMPA) — load/write (Phase 11)."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

from swg_iff.reader import IffError, IffReader
from swg_iff.tags import TAG_CMPA, TAG_PART, TAG_RADR, tag_from_str, tag_to_str
from swg_iff.writer import make_chunk, make_chunk_float, make_chunk_int32, make_chunk_string, make_form
from swg_scene.appearance_export import make_chunk_float_transform


@dataclass
class SwgComponentPart:
    appearance_path: str
    transform: tuple[float, ...] = (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0)


@dataclass
class SwgComponentAppearance:
    version: str = "0005"
    appearance_raw: bytes = b""
    parts: list[SwgComponentPart] = field(default_factory=list)
    radar_raw: bytes | None = None


def _read_parts(reader: IffReader, *, old_format: bool) -> list[SwgComponentPart]:
    parts: list[SwgComponentPart] = []
    while not reader.at_end_of_form():
        block = reader._peek_block()
        if block.is_form or block.tag_str.strip() != "PART":
            reader.skip_block()
            continue
        reader.enter_chunk(TAG_PART)
        try:
            name = reader.read_string()
            if old_format:
                px, py, pz = reader.read_float_vector()
                yaw = reader.read_float()
                pitch = reader.read_float()
                roll = reader.read_float()
                transform = (
                    1.0, 0.0, 0.0, px,
                    0.0, 1.0, 0.0, py,
                    0.0, 0.0, 1.0, pz,
                )
                _ = (yaw, pitch, roll)
            else:
                transform = tuple(reader.read_float() for _ in range(12))
            parts.append(SwgComponentPart(name, transform))
        finally:
            reader.exit_chunk(TAG_PART)
    return parts


def load_component_appearance(path: str | Path) -> SwgComponentAppearance:
    return load_component_appearance_bytes(Path(path).read_bytes())


def load_component_appearance_bytes(data: bytes) -> SwgComponentAppearance:
    reader = IffReader(data)
    reader.enter_form(TAG_CMPA)
    try:
        version = tag_to_str(reader.current_name())
        if version not in ("0001", "0002", "0003", "0004", "0005"):
            raise IffError(f"unsupported CMPA version {version!r}")
        reader.enter_form(tag_from_str(version))
        try:
            appearance_raw = b""
            radar_raw = None
            parts: list[SwgComponentPart] = []
            old_parts = version == "0001"
            while not reader.at_end_of_form():
                block = reader._peek_block()
                if block.is_form and block.form_type_str.strip() == "APPR":
                    appearance_raw = reader.read_raw_block()
                elif block.is_form and block.form_type_str.strip() == "RADR":
                    radar_raw = reader.read_raw_block()
                elif not block.is_form and block.tag_str.strip() == "PART":
                    parts.extend(_read_parts(reader, old_format=old_parts))
                else:
                    reader.skip_block()
            return SwgComponentAppearance(
                version=version,
                appearance_raw=appearance_raw,
                parts=parts,
                radar_raw=radar_raw,
            )
        finally:
            reader.exit_form(tag_from_str(version))
    finally:
        reader.exit_form(TAG_CMPA)


def write_component_appearance_bytes(spec: SwgComponentAppearance) -> bytes:
    version = spec.version if spec.version in ("0001", "0002", "0003", "0004", "0005") else "0005"
    body = b""
    if spec.appearance_raw and version in ("0003", "0004", "0005"):
        body += spec.appearance_raw
    if spec.radar_raw and version == "0005":
        body += spec.radar_raw
    elif version == "0005":
        body += make_form(
            "RADR",
            make_chunk("INFO", make_chunk_int32(0)),
        )
    for part in spec.parts:
        if version == "0001":
            raise ValueError("CMPA 0001 export not supported")
        payload = make_chunk_string(part.appearance_path) + make_chunk_float_transform(part.transform)
        body += make_chunk("PART", payload)
    return make_form("CMPA", make_form(version, body))


def write_component_appearance(path: str | Path, spec: SwgComponentAppearance) -> None:
    Path(path).write_bytes(write_component_appearance_bytes(spec))


def build_component_appearance(
    parts: list[SwgComponentPart],
    *,
    meshes: list | None = None,
    hardpoints: list | None = None,
    version: str = "0005",
    include_radar_placeholder: bool = True,
) -> SwgComponentAppearance:
    """Build CMPA/0005 from part list + optional APPR extent from mesh IR."""
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
    radar_raw = None
    if include_radar_placeholder and version == "0005":
        radar_raw = make_form("RADR", make_chunk("INFO", make_chunk_int32(0)))
    return SwgComponentAppearance(
        version=version,
        appearance_raw=appearance_raw,
        parts=parts,
        radar_raw=radar_raw,
    )
