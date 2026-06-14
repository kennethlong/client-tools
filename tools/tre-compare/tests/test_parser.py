"""Import-surface + data-model smoke test for the vendored parser (Phase-28 D-03).

Plan 28-02 vendors metadata-only parsing capability; the fixture-backed
byte-built archive round-trip tests live in Plan 04. This module asserts the two
things that must hold regardless of fixtures:

  1. the public IMPORT SURFACE (every symbol the package + Phase 29 rely on), and
  2. the DATA MODEL — every entry dataclass exposes BOTH `length` and
     `compressed_length` (snake_case), the Phase-29 changed-detection
     forward-compat contract (CONTEXT <specifics>).

References: 28-RESEARCH §Validation Architecture (parser Test Map rows),
28-PATTERNS §"both length AND compressed_length present" flag.
"""

from __future__ import annotations

import dataclasses

import pytest

from tre_compare.parser import (
    SUPPORTED_TRE_VERSIONS,
    Cot2000Entry,
    SearchTocEntry,
    TreEntry,
    TreFormatError,
    UnsupportedTreVersionError,
    detect_master_index_kind,
    list_tre,
    parse_master_index,
    read_cot2000_entries,
    read_search_toc_entries,
    read_tre_entries,
    read_tre_header,
    read_tre_payload,
    toc_entry_stride,
)
from tre_compare.parser import tre_reader
from tre_compare.parser import parse_master_index
from tre_compare.parser.tre_reader import MasterIndexKind

import _fixtures

EXPECTED_VERSION_TAGS = ["0004", "0005", "6000", "0006", "5000"]


def test_parser_public_api() -> None:
    """Every public symbol imports, callables are callable, and all five TREE
    version tags are recognized (SC#1)."""
    # All five TREE format variants recognized — SC#1.
    assert SUPPORTED_TRE_VERSIONS == frozenset({"0004", "0005", "6000", "0006", "5000"})
    # Re-export from parser/__init__ matches the underlying module symbol.
    assert tre_reader.SUPPORTED_TRE_VERSIONS == SUPPORTED_TRE_VERSIONS

    callables = (
        read_tre_header,
        read_tre_entries,
        list_tre,
        read_tre_payload,
        detect_master_index_kind,
        parse_master_index,
        read_cot2000_entries,
        read_search_toc_entries,
        toc_entry_stride,
    )
    for fn in callables:
        assert callable(fn), f"{fn!r} is not callable"

    # The two public exceptions are exception classes.
    assert issubclass(UnsupportedTreVersionError, Exception)
    assert issubclass(TreFormatError, Exception)

    # The three entry types are dataclasses.
    for cls in (TreEntry, Cot2000Entry, SearchTocEntry):
        assert dataclasses.is_dataclass(cls), f"{cls!r} is not a dataclass"


def test_entry_dataclasses_expose_both_lengths() -> None:
    """Phase-29 forward-compat: every entry type exposes BOTH `length` and
    snake_case `compressed_length` (NOT camelCase `compressedLength`)."""
    for cls in (TreEntry, Cot2000Entry, SearchTocEntry):
        field_names = {f.name for f in dataclasses.fields(cls)}
        assert "length" in field_names, f"{cls.__name__} missing `length`"
        assert "compressed_length" in field_names, (
            f"{cls.__name__} missing snake_case `compressed_length`"
        )
        # Negative assertion: the camelCase spelling CONTEXT §specifics used
        # must NOT be a field — Phase-29 code must use the snake_case name.
        assert "compressedLength" not in field_names, (
            f"{cls.__name__} exposes camelCase `compressedLength`; expected snake_case"
        )


@pytest.mark.parametrize("tag", EXPECTED_VERSION_TAGS)
def test_supported_version_tags_individually(tag: str) -> None:
    """Per-tag coverage the Test Map calls for; the byte-parse-per-tag round
    trip is `test_parse_each_version_tag` below."""
    assert tag in SUPPORTED_TRE_VERSIONS


@pytest.mark.parametrize("tag", EXPECTED_VERSION_TAGS)
def test_parse_each_version_tag(tmp_path, tag: str) -> None:
    """Byte-build a TRE per version tag and parse it back (real bytes, not just the
    frozenset). The 6000/0006 cases PROVE the extended-stride 8-byte TOC-row pad: a
    24-byte stride for those tags would mis-align ``base = i * stride`` on the 2nd row."""
    p = _fixtures.build_tre(
        tmp_path / f"v_{tag}.tre",
        [("foo/a.dds", 10, 10), ("foo/b.dds", 20, 20)],
        version=tag,
    )
    entries = read_tre_entries(str(p))
    paths = [e.path.lower() for e in entries]
    assert "foo/a.dds" in paths and "foo/b.dds" in paths, paths
    # second entry alignment proves the stride math (the pad for extended tags)
    assert entries[1].length == 20, entries


def test_parse_compressed_blocks(tmp_path) -> None:
    """A zlib-compressed TOC + name block exercises the parser's ``_decompress_block``
    path — the same code the merge relies on for compressed real archives (finding #10)."""
    p = _fixtures.build_tre(
        tmp_path / "compressed.tre",
        [("foo/zz.dds", 33, 33)],
        version="0004",
        compress=True,
    )
    hdr = read_tre_header(str(p))
    assert hdr.toc_compressor != 0 and hdr.block_compressor != 0
    entries = read_tre_entries(str(p))
    assert any(e.path.lower() == "foo/zz.dds" for e in entries), entries
    # a compressed SearchTOC also round-trips through the two-phase decompress path
    sp = _fixtures.build_search_toc(
        tmp_path / "compressed.toc",
        [("foo/zz.dds", 33, 33)],
        tree_files=("a.tre",),
        compress=True,
    )
    sentries = read_search_toc_entries(str(sp))
    assert any(e.path.lower() == "foo/zz.dds" for e in sentries), sentries


def test_parse_cot2000_and_searchtoc(tmp_path) -> None:
    """Two mandatory master-index round-trips (finding #3 — COT2000 is READ, not just
    detected; this is the literal SC#1 proof)."""
    # SearchTOC two-phase fn_field walk
    sp = _fixtures.build_search_toc(
        tmp_path / "s.toc", [("foo/s.dds", 7, 7)], tree_files=("a.tre",)
    )
    se = read_search_toc_entries(str(sp))
    assert any(e.path.lower() == "foo/s.dds" for e in se), se
    assert detect_master_index_kind(str(sp)) == MasterIndexKind.SEARCH_TOC
    assert parse_master_index(str(sp))["kind"] == "search_toc"

    # COT2000 — READ back, kind-detected, and parse_master_index routes it
    cp = _fixtures.build_cot2000(tmp_path / "m.toc", "a.tre", [("foo/c.dds", 5, 5)])
    ce = read_cot2000_entries(str(cp))
    assert any(e.path.lower() == "foo/c.dds" for e in ce), ce
    assert detect_master_index_kind(str(cp)) == MasterIndexKind.COT2000
    assert parse_master_index(str(cp))["kind"] == "cot2000"
