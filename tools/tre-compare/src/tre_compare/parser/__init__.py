"""Vendored TRE/TOC parser (Phase-28 D-03 / D-04).

Holds the vendored, owned copies of tre_reader.py and tre_decrypt.py from
swg-blender-plugin (commit f803f58782e675e85844960fe868b0849405249a). Owned and
may diverge — no live link back. Re-exports the public API so downstream
(scanner, virtual-tree, Phase-29 diff) imports stay clean and refactor-free.
"""

from .tre_reader import (
    SUPPORTED_TRE_VERSIONS,
    TreEntry,
    Cot2000Entry,
    SearchTocEntry,
    read_tre_header,
    read_tre_entries,
    list_tre,
    read_tre_payload,
    detect_master_index_kind,
    parse_master_index,
    read_cot2000_entries,
    read_search_toc_entries,
    toc_entry_stride,
    UnsupportedTreVersionError,
    TreFormatError,
)

__all__ = [
    "SUPPORTED_TRE_VERSIONS",
    "TreEntry",
    "Cot2000Entry",
    "SearchTocEntry",
    "read_tre_header",
    "read_tre_entries",
    "list_tre",
    "read_tre_payload",
    "detect_master_index_kind",
    "parse_master_index",
    "read_cot2000_entries",
    "read_search_toc_entries",
    "toc_entry_stride",
    "UnsupportedTreVersionError",
    "TreFormatError",
]
