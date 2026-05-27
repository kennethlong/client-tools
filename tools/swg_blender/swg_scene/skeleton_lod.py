"""SLOD LOD skeleton template loader (LodSkeletonTemplate 0000)."""

from __future__ import annotations

from swg_iff.reader import IffReader, IffError
from swg_iff.tags import TAG_0000, TAG_INFO, TAG_SKTM, TAG_SLOD, tag_to_str
from swg_scene.skeleton import SwgSkeleton, _load_sktm_version


def load_slod_all_levels(path: str | Path) -> list[SwgSkeleton]:
    """Load every SKTM detail level from a root-level SLOD file."""
    reader = IffReader.from_file(path)
    reader.enter_form(TAG_SLOD)
    try:
        version_tag = reader.current_name()
        if version_tag != TAG_0000:
            raise IffError(f"unsupported SLOD version {tag_to_str(version_tag)}")
        reader.enter_form(TAG_0000)
        try:
            reader.enter_chunk(TAG_INFO)
            lod_count = reader.read_int16()
            reader.exit_chunk(TAG_INFO)
            skeletons: list[SwgSkeleton] = []
            for detail in range(lod_count):
                block = reader._peek_block()
                if not block.is_form or block.form_type != TAG_SKTM:
                    raise IffError(
                        f"expected nested SKTM at SLOD detail {detail}, "
                        f"got {block.form_type_str or block.tag_str}"
                    )
                reader.enter_form(TAG_SKTM)
                try:
                    skeletons.append(_load_sktm_version(reader))
                finally:
                    reader.exit_form(TAG_SKTM)
            return skeletons
        finally:
            reader.exit_form(TAG_0000)
    finally:
        reader.exit_form(TAG_SLOD)


def load_slod_from_reader(reader: IffReader, *, lod_index: int = 0) -> SwgSkeleton:
    """Load one BasicSkeletonTemplate (SKTM) from a root-level SLOD form."""
    reader.enter_form(TAG_SLOD)
    try:
        version_tag = reader.current_name()
        if version_tag != TAG_0000:
            raise IffError(f"unsupported SLOD version {tag_to_str(version_tag)}")
        reader.enter_form(TAG_0000)
        try:
            reader.enter_chunk(TAG_INFO)
            lod_count = reader.read_int16()
            reader.exit_chunk(TAG_INFO)
            if lod_count < 1:
                raise IffError("SLOD INFO reports zero detail levels")
            if lod_index < 0 or lod_index >= lod_count:
                raise IffError(
                    f"SLOD lod_index {lod_index} out of range (0..{lod_count - 1})"
                )
            for detail in range(lod_count):
                block = reader._peek_block()
                if not block.is_form or block.form_type != TAG_SKTM:
                    raise IffError(
                        f"expected nested SKTM at SLOD detail {detail}, "
                        f"got {block.form_type_str or block.tag_str}"
                    )
                if detail == lod_index:
                    reader.enter_form(TAG_SKTM)
                    try:
                        return _load_sktm_version(reader)
                    finally:
                        reader.exit_form(TAG_SKTM)
                reader.skip_block()
            raise IffError(f"SLOD detail index {lod_index} not found")
        finally:
            reader.exit_form(TAG_0000)
    finally:
        reader.exit_form(TAG_SLOD)