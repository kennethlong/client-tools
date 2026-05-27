"""Portal property templates (PRTO / .pob) — Phase 13."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

from swg_iff.reader import IffError, IffReader
from swg_iff.tags import (
    TAG_0000,
    TAG_CELS,
    TAG_CRC,
    TAG_DATA,
    TAG_PGRF,
    TAG_PRTO,
    TAG_PRTS,
    tag_from_str,
    tag_to_str,
)
from swg_iff.writer import make_chunk, make_chunk_int32, make_form


@dataclass
class SwgPortalProperty:
    version: str = "0004"
    portal_count: int = 0
    cell_count: int = 0
    portal_geometry_raw: list[bytes] = field(default_factory=list)
    cells_raw: list[bytes] = field(default_factory=list)
    pgrf_raw: bytes | None = None
    crc: int | None = None


def load_portal_property(path: str | Path) -> SwgPortalProperty:
    return load_portal_property_bytes(Path(path).read_bytes())


def load_portal_property_bytes(data: bytes) -> SwgPortalProperty:
    reader = IffReader(data)
    reader.enter_form(TAG_PRTO)
    try:
        version = tag_to_str(reader.current_name())
        if not version.isdigit():
            raise IffError(f"unsupported PRTO version {version!r}")
        reader.enter_form(tag_from_str(version))
        try:
            portal_count = 0
            cell_count = 0
            portal_geometry_raw: list[bytes] = []
            cells_raw: list[bytes] = []
            pgrf_raw = None
            crc = None

            while not reader.at_end_of_form():
                block = reader._peek_block()
                tag = block.tag_str.strip() if not block.is_form else block.form_type_str.strip()
                if tag == "DATA":
                    reader.enter_chunk(TAG_DATA)
                    try:
                        portal_count = reader.read_int32()
                        cell_count = reader.read_int32()
                    finally:
                        reader.exit_chunk(TAG_DATA)
                elif tag == "PRTS":
                    reader.enter_form(TAG_PRTS)
                    try:
                        while not reader.at_end_of_form():
                            portal_geometry_raw.append(reader.read_raw_block())
                    finally:
                        reader.exit_form(TAG_PRTS)
                elif tag == "CELS":
                    reader.enter_form(TAG_CELS)
                    try:
                        while not reader.at_end_of_form():
                            cells_raw.append(reader.read_raw_block())
                    finally:
                        reader.exit_form(TAG_CELS)
                elif tag == "PGRF":
                    pgrf_raw = reader.read_raw_block()
                elif tag == "CRC":
                    reader.enter_chunk(TAG_CRC)
                    try:
                        crc = reader.read_int32()
                    finally:
                        reader.exit_chunk(TAG_CRC)
                else:
                    reader.skip_block()

            return SwgPortalProperty(
                version=version,
                portal_count=portal_count,
                cell_count=cell_count,
                portal_geometry_raw=portal_geometry_raw,
                cells_raw=cells_raw,
                pgrf_raw=pgrf_raw,
                crc=crc,
            )
        finally:
            reader.exit_form(tag_from_str(version))
    finally:
        reader.exit_form(TAG_PRTO)


def write_portal_property_bytes(spec: SwgPortalProperty) -> bytes:
    version = spec.version if spec.version.isdigit() else "0004"
    body = make_chunk(
        "DATA",
        make_chunk_int32(spec.portal_count) + make_chunk_int32(spec.cell_count),
    )
    body += make_form("PRTS", b"".join(spec.portal_geometry_raw))
    body += make_form("CELS", b"".join(spec.cells_raw))
    if spec.pgrf_raw:
        body += spec.pgrf_raw
    if spec.crc is not None:
        body += make_chunk("CRC ", make_chunk_int32(spec.crc))
    return make_form("PRTO", make_form(version, body))


def write_portal_property(path: str | Path, spec: SwgPortalProperty) -> None:
    Path(path).write_bytes(write_portal_property_bytes(spec))


def extract_portal_layout_crc(data: bytes) -> int | None:
    """Return CRC chunk value from a .pob, if present."""
    spec = load_portal_property_bytes(data)
    return spec.crc
