"""Parse APPR hardpoints from static mesh appearance bytes."""

from __future__ import annotations

from swg_iff.reader import IffReader
from swg_iff.tags import TAG_APPR, tag_from_str
from swg_scene.model import SwgHardpoint


def parse_hardpoints_from_appearance(appr_raw: bytes) -> list[SwgHardpoint]:
    reader = IffReader(appr_raw)
    reader.enter_form(TAG_APPR)
    try:
        version = reader.current_name()
        if version != tag_from_str("0003"):
            return []
        reader.enter_form(tag_from_str("0003"))
        try:
            # visual + collision extent forms
            reader.read_raw_block()
            reader.read_raw_block()
            if not reader.at_end_of_form():
                block = reader._peek_block()
                if block.is_form and block.form_type_str.strip() != "HPTS":
                    return []
            reader.enter_form(tag_from_str("HPTS"))
            try:
                hardpoints: list[SwgHardpoint] = []
                while not reader.at_end_of_form():
                    reader.enter_chunk(tag_from_str("HPNT"))
                    matrix = tuple(reader.read_float() for _ in range(12))
                    rem = reader.read_chunk_remaining()
                    name = rem.split(b"\x00", 1)[0].decode("ascii", errors="replace")
                    reader.exit_chunk(tag_from_str("HPNT"))
                    hardpoints.append(SwgHardpoint(name=name, matrix=matrix))
                return hardpoints
            finally:
                reader.exit_form(tag_from_str("HPTS"))
        finally:
            reader.exit_form(tag_from_str("0003"))
    finally:
        reader.exit_form(TAG_APPR)
