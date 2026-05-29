"""Decode PRTO portal property cells (CELL forms)."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

from swg_iff.reader import IffError, IffReader
from swg_iff.tags import TAG_0005, TAG_CELL, TAG_DATA, tag_from_str
from swg_scene.portal_property import load_portal_property


@dataclass
class SwgPortalCellSummary:
    name: str
    can_see_parent: bool
    appearance_relpath: str
    floor_relpath: str | None
    portal_count: int = 0
    cell_raw: bytes = b""
    extra_blocks: list[bytes] = field(default_factory=list)


def _decode_cell_raw(cell_raw: bytes) -> SwgPortalCellSummary:
    reader = IffReader(cell_raw)
    reader.enter_form(TAG_CELL)
    try:
        version = reader.current_name()
        if version != TAG_0005:
            raise IffError(f"unsupported CELL version {version!r}")
        reader.enter_form(TAG_0005)
        try:
            extra_blocks: list[bytes] = []
            portal_count = 0
            can_see_parent = False
            name = ""
            appearance = ""
            floor: str | None = None
            while not reader.at_end_of_form():
                block = reader._peek_block()
                tag = block.tag_str.strip() if not block.is_form else block.form_type_str.strip()
                if tag == "DATA":
                    reader.enter_chunk(TAG_DATA)
                    try:
                        portal_count = reader.read_int32()
                        can_see_parent = reader.read_bool8()
                        name = reader.read_string()
                        appearance = reader.read_string()
                        floor: str | None = None
                        if reader.read_bool8():
                            floor = reader.read_string()
                    finally:
                        reader.exit_chunk(TAG_DATA)
                else:
                    extra_blocks.append(reader.read_raw_block())
            return SwgPortalCellSummary(
                name=name,
                can_see_parent=can_see_parent,
                appearance_relpath=appearance,
                floor_relpath=floor,
                portal_count=portal_count,
                cell_raw=cell_raw,
                extra_blocks=extra_blocks,
            )
        finally:
            reader.exit_form(TAG_0005)
    finally:
        reader.exit_form(TAG_CELL)


def load_portal_cells(path: str | Path) -> list[SwgPortalCellSummary]:
    spec = load_portal_property(path)
    return [_decode_cell_raw(raw) for raw in spec.cells_raw]


def load_portal_cells_bytes(data: bytes) -> list[SwgPortalCellSummary]:
    from swg_scene.portal_property import load_portal_property_bytes

    spec = load_portal_property_bytes(data)
    return [_decode_cell_raw(raw) for raw in spec.cells_raw]


def write_cell_bytes(summary: SwgPortalCellSummary) -> bytes:
    """Encode one CELL/0005 block from editable fields."""
    from swg_iff.writer import (
        make_chunk,
        make_chunk_bool8,
        make_chunk_int32,
        make_chunk_string,
        make_form,
    )

    payload = make_chunk_int32(summary.portal_count)
    payload += make_chunk_bool8(summary.can_see_parent)
    payload += make_chunk_string(summary.name)
    payload += make_chunk_string(summary.appearance_relpath)
    if summary.floor_relpath:
        payload += make_chunk_bool8(True)
        payload += make_chunk_string(summary.floor_relpath)
    else:
        payload += make_chunk_bool8(False)
    body = make_chunk("DATA", payload) + b"".join(summary.extra_blocks)
    return make_form("CELL", make_form("0005", body))


def summary_from_manifest_cell(
    entry: dict[str, object],
    *,
    portal_count: int = 0,
    extra_blocks: list[bytes] | None = None,
) -> SwgPortalCellSummary:
    floor = str(entry.get("floor_relpath") or "").strip()
    return SwgPortalCellSummary(
        name=str(entry.get("name") or ""),
        can_see_parent=bool(entry.get("can_see_parent")),
        appearance_relpath=str(entry.get("appearance_relpath") or ""),
        floor_relpath=floor or None,
        portal_count=portal_count,
        extra_blocks=list(extra_blocks or []),
    )


def cell_fields_unchanged(
    entry: dict[str, object], original: SwgPortalCellSummary
) -> bool:
    floor = str(entry.get("floor_relpath") or "").strip() or None
    return (
        str(entry.get("name") or "") == original.name
        and bool(entry.get("can_see_parent")) == original.can_see_parent
        and str(entry.get("appearance_relpath") or "") == original.appearance_relpath
        and floor == original.floor_relpath
        and int(entry.get("portal_count", original.portal_count)) == original.portal_count
    )
