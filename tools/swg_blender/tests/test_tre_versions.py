"""Phase 6: version-aware TRE / master index tests."""

from __future__ import annotations

import struct
from pathlib import Path

import pytest

from swg_pipeline.tre_reader import (
    MasterIndexKind,
    TAG_0001_LE,
    TAG_TOC_LE,
    TAG_TREE_LE,
    UnsupportedTreVersionError,
    detect_master_index_kind,
    list_tre,
    parse_master_index,
    read_search_toc_entries,
    read_search_toc_header,
    read_tre_entries,
    read_tre_header,
    read_tre_payload,
    toc_entry_stride,
)

GOLDEN_RETAIL = Path(__file__).resolve().parent / "golden" / "retail_mini_0005.tre"


def test_toc_stride_retail_vs_extended():
    assert toc_entry_stride("0004") == 24
    assert toc_entry_stride("0005") == 24
    assert toc_entry_stride("6000") == 32


def test_unsupported_version_raises(tmp_path: Path):
    bad = tmp_path / "bad.tre"
    hdr = struct.pack("<4s4s7I", TAG_TREE_LE, b"9999", 0, 36, 0, 0, 0, 0, 0)
    bad.write_bytes(hdr + b"\0" * 20)
    with pytest.raises(UnsupportedTreVersionError, match="9999"):
        read_tre_header(bad)


@pytest.mark.skipif(not GOLDEN_RETAIL.is_file(), reason="golden retail_mini_0005.tre missing")
def test_retail_golden_listing():
    header = read_tre_header(GOLDEN_RETAIL)
    assert header.version == "0005"
    assert header.number_of_files == 2
    paths = list_tre(GOLDEN_RETAIL)
    assert paths == ["texture/a.dds", "shader/b.sht"]
    assert read_tre_payload(GOLDEN_RETAIL, "texture/a.dds") == b"DDS1"


def test_build_search_toc_roundtrip(tmp_path: Path):
    """Minimal retail SearchTOC master index (synthetic)."""
    tre_name = b"patch_00.tre\x00"
    names = b"appearance/x.msh\x00"
    toc_entry = struct.pack("<BBHIIIII", 0, 0, 0, 0, len(b"appearance/x.msh"), 100, 4, 4)
    hdr = struct.pack(
        "<4s4sBBBBIIIIII",
        TAG_TOC_LE,
        TAG_0001_LE,
        0,
        0,
        0,
        0,
        1,
        len(toc_entry),
        len(names),
        len(names),
        1,
        len(tre_name),
    )
    toc_path = tmp_path / "master.toc"
    toc_path.write_bytes(hdr + tre_name + toc_entry + names)

    assert detect_master_index_kind(toc_path) == MasterIndexKind.SEARCH_TOC
    info = parse_master_index(toc_path)
    assert info["kind"] == "search_toc"
    assert info["indexed_path_count"] == 1
    assert info["indexed_paths_sample"][0] == "appearance/x.msh"

    header = read_search_toc_header(toc_path)
    assert header.number_of_files == 1
    assert header.tre_files == ("patch_00.tre",)
    entries = read_search_toc_entries(toc_path)
    assert entries[0].path == "appearance/x.msh"
    assert entries[0].tre_file == "patch_00.tre"
    assert entries[0].offset == 100
