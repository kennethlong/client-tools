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
    trip is Plan 04's fixture-backed test."""
    assert tag in SUPPORTED_TRE_VERSIONS
