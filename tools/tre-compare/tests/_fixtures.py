"""Programmatic byte-builders for minimal valid TRE / SearchTOC / COT2000 archives
plus a synthetic ``client.cfg`` writer (Phase-28 D-06).

These builders forge the *exact* on-disk layouts the vendored parser
(``tre_compare.parser.tre_reader``) reads, so every synthetic archive round-trips
through the real parser. They are PURE STDLIB (``struct``, ``zlib``, ``pathlib``) with no stdlib-ini-parser
and no cross-package engine import, so the corpus runs anywhere after extraction
(D-01/D-06).

Layouts are matched VERBATIM against the vendored reader's struct formats + offset
arithmetic (see ``28-04-PLAN.md`` <struct_layouts_verbatim>, cross-checked vs
``tre_reader.py`` this session):

  * TRE       : header (STANDARD_HEADER_FMT, 36B) + N×stride TOC + NUL-term name block.
                stride = toc_entry_stride(version) (24 retail / 32 extended; the
                extended rows pad each 24B TOC_ENTRY_FMT row with 8 zero bytes).
  * SearchTOC : TWO-PHASE — header (36B) + tree-name block + TOC block (N×24) + name
                block walked via ``fn_offset += fn_field + 1``.
  * COT2000   : magic(8) + header("<7I" at 8) + tree-name block(@36) + TOC block(N×32)
                + name block walked via ``fn_offset += fn_len + 1`` (size MUST match).

A ``compress=True`` option zlib-compresses the TOC + name blocks (sizes become the
COMPRESSED lengths; ``uncompSizeOfNameBlock`` stays uncompressed) so the parser's
``_decompress_block`` path is exercised (review finding #10).
"""

from __future__ import annotations

import struct
import zlib
from pathlib import Path

from tre_compare.parser.tre_reader import (
    COT2000_GLOBAL_ENTRY_FMT,
    COT2000_MAGIC,
    COT2000_TRE_NAME_OFFSET,
    SEARCH_TOC_ENTRY_FMT,
    SEARCH_TOC_HEADER_FMT,
    SEARCH_TOC_HEADER_SIZE,
    STANDARD_HEADER_FMT,
    TAG_0001_LE,
    TAG_TOC_LE,
    TAG_TREE_LE,
    TOC_ENTRY_FMT,
    toc_entry_stride,
)

# Entry = (name, length, compressed_length) with an optional 4th element (offset).
Entry = tuple


def _name_block(names: list[str]) -> bytes:
    """NUL-terminated, row-order name block (each name + a single b"\\x00")."""
    out = bytearray()
    for n in names:
        out += n.encode("latin-1") + b"\x00"
    return bytes(out)


def _entry_fields(entry: tuple) -> tuple[str, int, int, int]:
    """Normalize an entry tuple to (name, length, compressed_length, offset)."""
    name = entry[0]
    length = entry[1]
    clen = entry[2]
    offset = entry[3] if len(entry) > 3 else 0
    return name, length, clen, offset


def build_tre(
    path: Path,
    entries: list[tuple],
    *,
    version: str = "0004",
    compress: bool = False,
) -> Path:
    """Write a minimal VALID ``.tre`` that ``read_tre_entries`` round-trips.

    *entries* is a list of ``(name, length, compressed_length)`` (optionally a 4th
    ``offset``). Set ``length=0`` for an entry to forge a SearchTree TOMBSTONE.
    *version* selects the TREE tag (0004/0005/6000/0006/5000); extended tags use the
    32-byte stride (each 24B TOC row padded with 8 zero bytes). ``compress=True``
    zlib-compresses the TOC + name blocks (review finding #10).
    """
    path = Path(path)
    stride = toc_entry_stride(version)
    pad = b"\x00" * (stride - 24)

    names = [_entry_fields(e)[0] for e in entries]
    name_block = _name_block(names)
    uncomp_name_size = len(name_block)

    # Per-entry fileNameOffset = running offset into the (uncompressed) name block.
    toc = bytearray()
    fn_off = 0
    for e in entries:
        name, length, clen, offset = _entry_fields(e)
        crc = zlib.crc32(name.encode("latin-1")) & 0xFFFFFFFF
        toc += struct.pack(TOC_ENTRY_FMT, crc, length, offset, 0, clen, fn_off)
        toc += pad
        fn_off += len(name.encode("latin-1")) + 1

    toc_compressor = 0
    block_compressor = 0
    toc_block = bytes(toc)
    name_out = name_block
    if compress:
        toc_block = zlib.compress(bytes(toc))
        name_out = zlib.compress(name_block)
        toc_compressor = 1
        block_compressor = 1

    size_of_toc = len(toc_block)
    size_of_name_block = len(name_out)
    toc_offset = struct.calcsize(STANDARD_HEADER_FMT)  # header is 36 bytes

    header = struct.pack(
        STANDARD_HEADER_FMT,
        TAG_TREE_LE,
        version.encode("latin-1"),
        len(entries),       # numberOfFiles
        toc_offset,         # tocOffset
        toc_compressor,     # tocCompressor
        size_of_toc,        # sizeOfTOC
        block_compressor,   # blockCompressor
        size_of_name_block, # sizeOfNameBlock
        uncomp_name_size,   # uncompSizeOfNameBlock
    )
    path.write_bytes(header + toc_block + name_out)
    return path


def build_search_toc(
    path: Path,
    entries: list[tuple],
    *,
    tree_files: tuple[str, ...] = ("a.tre",),
    compress: bool = False,
) -> Path:
    """Write a minimal VALID retail SearchTOC master index (TAG_TOC/0001).

    Mirrors the verified TWO-PHASE layout: header (36B) + tree-name block + TOC block
    (N×24, each ``SEARCH_TOC_ENTRY_FMT``) + name block walked via
    ``fn_offset += fn_field + 1``. *entries* accepts ``(name, length, compressed_length)``
    (+ optional ``offset``); ``length=0``/``offset=0`` build the non-shadowing TOC case.
    ``compress=True`` zlib-compresses the TOC + name blocks (review finding #10).
    """
    path = Path(path)
    tree_name_block = _name_block(list(tree_files))
    size_of_tree_file_name_block = len(tree_name_block)
    toc_offset = SEARCH_TOC_HEADER_SIZE + size_of_tree_file_name_block

    names = [_entry_fields(e)[0] for e in entries]
    name_block = _name_block(names)
    uncomp_name_size = len(name_block)

    toc = bytearray()
    for e in entries:
        name, length, clen, offset = _entry_fields(e)
        crc = zlib.crc32(name.encode("latin-1")) & 0xFFFFFFFF
        fn_field = len(name.encode("latin-1"))  # walked: fn_offset += fn_field + 1
        toc += struct.pack(
            SEARCH_TOC_ENTRY_FMT,
            0,        # compressor
            0,        # unused
            0,        # tree_index
            crc,
            fn_field,
            offset,   # file_offset
            length,
            clen,     # compressed_length
        )

    toc_compressor = 0
    name_block_compressor = 0
    toc_block = bytes(toc)
    name_out = name_block
    if compress:
        toc_block = zlib.compress(bytes(toc))
        name_out = zlib.compress(name_block)
        toc_compressor = 1
        name_block_compressor = 1

    size_of_toc = len(toc_block)
    size_of_name_block = len(name_out)

    header = struct.pack(
        SEARCH_TOC_HEADER_FMT,
        TAG_TOC_LE,
        TAG_0001_LE,
        toc_compressor,          # tocCompressor:u8
        name_block_compressor,   # nameBlockCompressor:u8
        0,                       # unused:u8
        0,                       # unused:u8
        len(entries),            # numberOfFiles
        size_of_toc,             # sizeOfTOC
        size_of_name_block,      # sizeOfNameBlock
        uncomp_name_size,        # uncompSizeOfNameBlock
        len(tree_files),         # numberOfTreeFiles
        size_of_tree_file_name_block,
    )
    path.write_bytes(header + tree_name_block + toc_block + name_out)
    return path


def build_cot2000(path: Path, tre_name: str, entries: list[tuple]) -> Path:
    """Write a minimal VALID COT2000 master index that ``read_cot2000_entries`` round-trips.

    Layout: magic (8) + header (``"<7I"`` at offset 8) + tree-name block (@36) + TOC
    block (N×32, each ``COT2000_GLOBAL_ENTRY_FMT``) + name block. The name block size
    MUST equal the walked total (``sum(len(name)+1)``) or the reader raises a
    name-block-size mismatch ``TreFormatError``. *entries* accepts
    ``(name, length, compressed_length)`` (+ optional ``offset``).
    """
    path = Path(path)
    tree_name_block = _name_block([tre_name])
    size_of_tree_name_block = len(tree_name_block)

    names = [_entry_fields(e)[0] for e in entries]
    name_block = _name_block(names)
    size_of_name_block = len(name_block)

    toc = bytearray()
    for e in entries:
        name, length, clen, offset = _entry_fields(e)
        crc = zlib.crc32(name.encode("latin-1")) & 0xFFFFFFFF
        fn_len = len(name.encode("latin-1"))  # walked: fn_offset += fn_len + 1
        toc += struct.pack(
            COT2000_GLOBAL_ENTRY_FMT,
            0,        # compressor
            0,        # unused
            0,        # tree_index
            crc,
            fn_len,
            offset,   # file_offset
            length,
            clen,     # compressed_length
        )
    toc_block = bytes(toc)
    size_of_toc_block = len(toc_block)

    # magic(8) + header("<7I" = 28) = 36 bytes, so the tree-name block starts at 36.
    header = struct.pack(
        "<7I",
        0,                       # reserved
        len(entries),            # numberOfFiles
        size_of_toc_block,       # sizeOfTocBlock
        size_of_name_block,      # sizeOfNameBlock
        0,                       # dupNames
        1,                       # numberOfTreeFiles
        size_of_tree_name_block, # sizeOfTreeNameBlock
    )
    assert len(COT2000_MAGIC) + len(header) == COT2000_TRE_NAME_OFFSET
    path.write_bytes(COT2000_MAGIC + header + tree_name_block + toc_block + name_block)
    return path


def write_synthetic_cfg(
    path: Path,
    nodes: list[tuple],
    *,
    max_priority: int = 12,
    toc_tree_path: str = "synthetic/",
) -> Path:
    """Write a synthetic ``[SharedFile]`` block modeled on the live cfg (D-06).

    *nodes* is a list of ``(kind, priority, fixture_path)`` tuples in cfg-discovery
    order (``kind`` in {"path","tree","toc"}). The writer emits a TAB-indented,
    double-quoted ``[SharedFile]`` section with a ``#`` comment line, the
    ``maxSearchPriority``/``TOCTreePath`` scalars, AND at least one deliberately
    REPEATED key name + one bare-priority key + a same-priority cross-kind pair when
    the supplied nodes happen to contain them. Each node is emitted as a
    ``searchKIND_NN_P="path"`` line so the scanner's ``_NN_`` grammar matches; the
    caller may also pass ``("path_bare", prio, value)`` to emit a bare-priority key.
    """
    path = Path(path)
    lines: list[str] = ["[SharedFile]"]
    lines.append("\t# synthetic [SharedFile] block (Phase-28 D-06 portable corpus)")
    lines.append(f"\tmaxSearchPriority={max_priority}")
    lines.append(f'\tTOCTreePath="{toc_tree_path}"')

    key_kind = {"path": "searchPath", "tree": "searchTree", "toc": "searchTOC"}
    for seq, (kind, priority, value) in enumerate(nodes):
        if kind.endswith("_bare"):
            base = key_kind[kind[: -len("_bare")]]
            lines.append(f'\t{base}{priority}="{_as_posix(value)}"')
        else:
            base = key_kind[kind]
            lines.append(f'\t{base}_{seq:02d}_{priority}="{_as_posix(value)}"')
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return path


def _as_posix(value) -> str:
    return str(value).replace("\\", "/")
