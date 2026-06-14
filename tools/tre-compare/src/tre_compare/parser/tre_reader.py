# Vendored 2026-06-14 from swg-blender-plugin
#   source: D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py
#   commit: f803f58782e675e85844960fe868b0849405249a (master)
# Copied per Phase-28 D-03; owned and may diverge. No live link back.
"""Read SOE retail and SwgRestoration TRE/TOC archives (version-aware).

Supports per-.tre version tags 0004, 0005 (24-byte TOC) and 6000 (32-byte TOC).
Master indexes: retail SearchTOC (TAG_TOC/0001) and SwgRestoration COT2000.

Retail zlib payloads decompress via read_tre_payload(); Restoration/extended tags
(6000, 0006, 5000) may remain obfuscated at rest (use TreeFileExtractor).
"""

from __future__ import annotations

import shutil
import struct
import zlib
from dataclasses import dataclass
from enum import Enum
from pathlib import Path

TAG_TREE_LE = struct.pack("<I", 0x54524545)
TAG_TOC_LE = struct.pack("<I", 0x544F4320)
TAG_0001_LE = struct.pack("<I", 0x30303031)

COT2000_MAGIC = b" COT2000"
COT2000_TRE_NAME_OFFSET = 36
COT2000_GLOBAL_ENTRY_FMT = "<BBHIIIII"
COT2000_GLOBAL_ENTRY_SIZE = 32

STANDARD_HEADER_SIZE = 36
STANDARD_HEADER_FMT = "<4s4s7I"

TOC_ENTRY_FMT = "<Iiiiii"
TOC_ENTRY_SIZE_RETAIL = 24
TOC_ENTRY_SIZE_EXTENDED = 32

SEARCH_TOC_HEADER_SIZE = 36
SEARCH_TOC_HEADER_FMT = "<4s4sBBBB6I"
SEARCH_TOC_ENTRY_FMT = "<BBHIIIII"
SEARCH_TOC_ENTRY_SIZE = 24

SUPPORTED_TRE_VERSIONS = frozenset({"0004", "0005", "6000", "0006", "5000"})
RETAIL_TRE_VERSIONS = frozenset({"0004", "0005"})
EXTENDED_TRE_VERSIONS = frozenset({"6000", "0006", "5000"})


class UnsupportedTreVersionError(ValueError):
    """Raised when a .tre version tag is not handled."""


class TreFormatError(ValueError):
    """Raised when archive bytes do not match expected layout."""


class MasterIndexKind(str, Enum):
    COT2000 = "cot2000"
    SEARCH_TOC = "search_toc"


@dataclass(frozen=True)
class TreHeader:
    path: Path
    token: str
    version: str
    number_of_files: int
    toc_offset: int
    toc_compressor: int
    size_of_toc: int
    block_compressor: int
    size_of_name_block: int
    uncomp_size_of_name_block: int


@dataclass(frozen=True)
class TreEntry:
    path: str
    crc: int
    length: int
    offset: int
    compressor: int
    compressed_length: int


@dataclass(frozen=True)
class Cot2000Header:
    path: Path
    number_of_files: int
    size_of_toc_block: int
    size_of_name_block: int
    number_of_tree_files: int
    size_of_tree_name_block: int
    tre_files: tuple[str, ...]
    toc_block_offset: int
    name_block_offset: int


@dataclass(frozen=True)
class Cot2000Entry:
    path: str
    tre_file: str
    tree_index: int
    crc: int
    offset: int
    length: int
    compressor: int
    compressed_length: int


@dataclass(frozen=True)
class SearchTocHeader:
    path: Path
    number_of_files: int
    toc_compressor: int
    name_block_compressor: int
    size_of_toc: int
    size_of_name_block: int
    uncomp_size_of_name_block: int
    number_of_tree_files: int
    size_of_tree_file_name_block: int
    tre_files: tuple[str, ...]
    toc_offset: int


@dataclass(frozen=True)
class SearchTocEntry:
    path: str
    tre_file: str
    tree_index: int
    crc: int
    offset: int
    length: int
    compressor: int
    compressed_length: int


def tag_to_str(tag_bytes: bytes) -> str:
    return tag_bytes.decode("latin1", errors="replace")


def toc_entry_stride(version: str) -> int:
    if version in RETAIL_TRE_VERSIONS or version == "5000":
        return TOC_ENTRY_SIZE_RETAIL
    if version in EXTENDED_TRE_VERSIONS:
        return TOC_ENTRY_SIZE_EXTENDED
    raise UnsupportedTreVersionError(
        f"unsupported TRE version {version!r} (supported: {sorted(SUPPORTED_TRE_VERSIONS)})"
    )


def detect_master_index_kind(path: str | Path) -> MasterIndexKind:
    data = Path(path).read_bytes()
    if data.startswith(COT2000_MAGIC):
        return MasterIndexKind.COT2000
    if len(data) >= 8 and data[0:4] == TAG_TOC_LE and data[4:8] == TAG_0001_LE:
        return MasterIndexKind.SEARCH_TOC
    raise TreFormatError(
        f"not a recognized master index: {path} (expected COT2000 or SearchTOC)"
    )


def read_tre_header(path: str | Path) -> TreHeader:
    data = Path(path).read_bytes()
    if len(data) < STANDARD_HEADER_SIZE:
        raise TreFormatError(f"TRE too small: {path}")
    token_b, version_b = struct.unpack_from("<4s4s", data, 0)
    if token_b != TAG_TREE_LE:
        raise TreFormatError(f"Not a TREE archive: {path} (token={token_b!r})")
    version = tag_to_str(version_b)
    if version not in SUPPORTED_TRE_VERSIONS:
        raise UnsupportedTreVersionError(
            f"unsupported TRE version {version!r} in {path} "
            f"(supported: {sorted(SUPPORTED_TRE_VERSIONS)})"
        )
    nf, toc_off, toc_comp, sz_toc, blk_comp, sz_nb, uncomp_nb = struct.unpack_from(
        "<7I", data, 8
    )
    return TreHeader(
        path=Path(path),
        token=tag_to_str(token_b),
        version=version,
        number_of_files=nf,
        toc_offset=toc_off,
        toc_compressor=toc_comp,
        size_of_toc=sz_toc,
        block_compressor=blk_comp,
        size_of_name_block=sz_nb,
        uncomp_size_of_name_block=uncomp_nb,
    )


def _decompress_block(raw: bytes, compressed: bool) -> bytes:
    if not compressed:
        return raw
    return zlib.decompress(raw)


def read_tre_entries(path: str | Path) -> list[TreEntry]:
    p = Path(path)
    data = p.read_bytes()
    header = read_tre_header(p)
    stride = toc_entry_stride(header.version)
    toc_raw = data[header.toc_offset : header.toc_offset + header.size_of_toc]
    toc_data = _decompress_block(toc_raw, header.toc_compressor != 0)
    expected = header.number_of_files * stride
    if len(toc_data) < expected:
        raise TreFormatError(
            f"TOC too small in {p.name}: got {len(toc_data)} bytes, need {expected} "
            f"({header.number_of_files} x {stride})"
        )
    name_off = header.toc_offset + header.size_of_toc
    name_raw = data[name_off : name_off + header.size_of_name_block]
    names = _decompress_block(name_raw, header.block_compressor != 0)
    entries: list[TreEntry] = []
    for i in range(header.number_of_files):
        base = i * stride
        crc, length, offset, comp, clen, fnoff = struct.unpack_from(TOC_ENTRY_FMT, toc_data, base)
        end = names.find(b"\0", fnoff)
        if end < 0:
            raise TreFormatError(f"Bad fileNameOffset {fnoff} in {p.name} entry {i}")
        rel_path = names[fnoff:end].decode("latin1")
        entries.append(
            TreEntry(
                path=rel_path,
                crc=crc,
                length=length,
                offset=offset,
                compressor=comp,
                compressed_length=clen,
            )
        )
    return entries


def list_tre(path: str | Path) -> list[str]:
    return [e.path for e in read_tre_entries(path)]


def read_tre_payload(path: str | Path, logical_name: str) -> bytes:
    """Read payload bytes for an entry (decompresses zlib when compressor is set)."""
    p = Path(path)
    header = read_tre_header(p)
    data = p.read_bytes()
    needle = logical_name.replace("\\", "/").lower()
    for entry in read_tre_entries(p):
        if entry.path.replace("\\", "/").lower() != needle:
            continue
        size = entry.compressed_length if entry.compressor else entry.length
        raw = data[entry.offset : entry.offset + size]
        if entry.compressor:
            try:
                return zlib.decompress(raw)
            except zlib.error as exc:
                from .tre_decrypt import try_read_tre_payload

                alt = try_read_tre_payload(raw, entry.compressor, version=header.version)
                if alt.ok:
                    return alt.data
                raise TreFormatError(
                    f"zlib decompress failed for {logical_name!r} in {p.name} "
                    f"(compressor={entry.compressor}, version={header.version}). {alt.note}"
                ) from exc
        return raw
    raise KeyError(logical_name)


def read_cot2000_header(path: str | Path) -> Cot2000Header:
    p = Path(path)
    data = p.read_bytes()
    if not data.startswith(COT2000_MAGIC):
        raise TreFormatError(f"Not a COT2000 file: {p} (magic={data[:8]!r})")
    _reserved, n_files, sz_toc, sz_names, _dup_names, n_trees, sz_tree_names = struct.unpack_from(
        "<7I", data, 8
    )
    pos = COT2000_TRE_NAME_OFFSET
    tre_names: list[str] = []
    for _ in range(n_trees):
        end = data.find(b"\0", pos)
        if end < 0:
            raise TreFormatError(f"Truncated tree name block in {p.name}")
        tre_names.append(data[pos:end].decode("ascii", errors="replace"))
        pos = end + 1
    toc_off = pos
    name_off = toc_off + sz_toc
    return Cot2000Header(
        path=p,
        number_of_files=n_files,
        size_of_toc_block=sz_toc,
        size_of_name_block=sz_names,
        number_of_tree_files=n_trees,
        size_of_tree_name_block=sz_tree_names,
        tre_files=tuple(tre_names),
        toc_block_offset=toc_off,
        name_block_offset=name_off,
    )


def read_cot2000_entries(path: str | Path) -> list[Cot2000Entry]:
    header = read_cot2000_header(path)
    data = header.path.read_bytes()
    names = data[header.name_block_offset : header.name_block_offset + header.size_of_name_block]
    toc_base = header.toc_block_offset
    raw_entries: list[tuple[int, int, int, int, int, int, int]] = []
    for i in range(header.number_of_files):
        off = toc_base + i * COT2000_GLOBAL_ENTRY_SIZE
        comp, _, tree_idx, crc, fn_len, file_off, length, clen = struct.unpack_from(
            COT2000_GLOBAL_ENTRY_FMT, data, off
        )
        raw_entries.append((comp, tree_idx, crc, fn_len, file_off, length, clen))
    fn_offset = 0
    entries: list[Cot2000Entry] = []
    for comp, tree_idx, crc, fn_len, file_off, length, clen in raw_entries:
        end = names.find(b"\0", fn_offset)
        if end < 0:
            raise TreFormatError(f"Bad name block at offset {fn_offset}")
        rel_path = names[fn_offset:end].decode("latin1")
        tre_file = header.tre_files[tree_idx] if tree_idx < len(header.tre_files) else "?"
        entries.append(
            Cot2000Entry(
                path=rel_path,
                tre_file=tre_file,
                tree_index=tree_idx,
                crc=crc,
                offset=file_off,
                length=length,
                compressor=comp,
                compressed_length=clen,
            )
        )
        fn_offset += fn_len + 1
    if fn_offset != header.size_of_name_block:
        raise TreFormatError(
            f"Name block size mismatch: walked {fn_offset}, header says {header.size_of_name_block}"
        )
    return entries


def read_search_toc_header(path: str | Path) -> SearchTocHeader:
    p = Path(path)
    data = p.read_bytes()
    if len(data) < SEARCH_TOC_HEADER_SIZE:
        raise TreFormatError(f"SearchTOC too small: {p}")
    token_b, version_b, toc_comp, name_comp, _u1, _u2, n_files, sz_toc, sz_nb, uncomp_nb, n_trees, sz_tree_names = (
        struct.unpack_from(SEARCH_TOC_HEADER_FMT, data, 0)
    )
    if token_b != TAG_TOC_LE or version_b != TAG_0001_LE:
        raise TreFormatError(f"Not a SearchTOC file: {p} (token={token_b!r} version={version_b!r})")
    pos = SEARCH_TOC_HEADER_SIZE
    tre_names: list[str] = []
    for _ in range(n_trees):
        end = data.find(b"\0", pos)
        if end < 0:
            raise TreFormatError(f"Truncated tree name block in {p.name}")
        tre_names.append(data[pos:end].decode("latin1", errors="replace"))
        pos = end + 1
    return SearchTocHeader(
        path=p,
        number_of_files=n_files,
        toc_compressor=toc_comp,
        name_block_compressor=name_comp,
        size_of_toc=sz_toc,
        size_of_name_block=sz_nb,
        uncomp_size_of_name_block=uncomp_nb,
        number_of_tree_files=n_trees,
        size_of_tree_file_name_block=sz_tree_names,
        tre_files=tuple(tre_names),
        toc_offset=pos,
    )


def read_search_toc_entries(path: str | Path) -> list[SearchTocEntry]:
    header = read_search_toc_header(path)
    data = header.path.read_bytes()
    pos = header.toc_offset
    toc_raw = data[pos : pos + header.size_of_toc]
    pos += header.size_of_toc
    toc_data = _decompress_block(toc_raw, header.toc_compressor != 0)
    expected = header.number_of_files * SEARCH_TOC_ENTRY_SIZE
    if len(toc_data) < expected:
        raise TreFormatError(f"SearchTOC table too small in {header.path.name}")
    raw_rows: list[tuple[int, int, int, int, int, int, int]] = []
    for i in range(header.number_of_files):
        off = i * SEARCH_TOC_ENTRY_SIZE
        comp, _unused, tree_idx, crc, fn_field, file_off, length, clen = struct.unpack_from(
            SEARCH_TOC_ENTRY_FMT, toc_data, off
        )
        raw_rows.append((comp, tree_idx, crc, fn_field, file_off, length, clen))
    fn_offset = 0
    path_fields: list[tuple[int, int, int, int, int, int, int]] = []
    for comp, tree_idx, crc, fn_field, file_off, length, clen in raw_rows:
        path_fields.append((comp, tree_idx, crc, fn_offset, file_off, length, clen))
        fn_offset += fn_field + 1
    name_raw = data[pos : pos + header.size_of_name_block]
    names = _decompress_block(name_raw, header.name_block_compressor != 0)
    if len(names) < header.uncomp_size_of_name_block:
        raise TreFormatError(f"Short name block in {header.path.name}")
    entries: list[SearchTocEntry] = []
    for comp, tree_idx, crc, fnoff, file_off, length, clen in path_fields:
        end = names.find(b"\0", fnoff)
        if end < 0:
            raise TreFormatError(f"Bad filename offset {fnoff}")
        rel = names[fnoff:end].decode("latin1")
        tre = header.tre_files[tree_idx] if tree_idx < len(header.tre_files) else "?"
        entries.append(
            SearchTocEntry(
                path=rel,
                tre_file=tre,
                tree_index=tree_idx,
                crc=crc,
                offset=file_off,
                length=length,
                compressor=comp,
                compressed_length=clen,
            )
        )
    return entries


def parse_cot2000(path: str | Path) -> dict:
    header = read_cot2000_header(path)
    entries = read_cot2000_entries(path)
    ext_counts: dict[str, int] = {}
    for entry in entries:
        if "." in entry.path:
            ext = entry.path.rsplit(".", 1)[-1].lower()
            ext_counts[ext] = ext_counts.get(ext, 0) + 1
    return {
        "kind": MasterIndexKind.COT2000.value,
        "path": header.path,
        "tre_files": list(header.tre_files),
        "indexed_path_count": len(entries),
        "indexed_paths_sample": [e.path for e in entries[:20]],
        "extension_counts": ext_counts,
        "header": header,
    }


def parse_search_toc(path: str | Path) -> dict:
    header = read_search_toc_header(path)
    entries = read_search_toc_entries(path)
    ext_counts: dict[str, int] = {}
    for entry in entries:
        if "." in entry.path:
            ext = entry.path.rsplit(".", 1)[-1].lower()
            ext_counts[ext] = ext_counts.get(ext, 0) + 1
    return {
        "kind": MasterIndexKind.SEARCH_TOC.value,
        "path": header.path,
        "tre_files": list(header.tre_files),
        "indexed_path_count": len(entries),
        "indexed_paths_sample": [e.path for e in entries[:20]],
        "extension_counts": ext_counts,
        "header": header,
    }


def parse_master_index(path: str | Path) -> dict:
    kind = detect_master_index_kind(path)
    if kind == MasterIndexKind.COT2000:
        return parse_cot2000(path)
    return parse_search_toc(path)


def parse_toc2000(path: str | Path) -> dict:
    """Alias for parse_cot2000 (legacy name)."""
    return parse_cot2000(path)


def backup_tre(path: str | Path, suffix: str = ".bak") -> Path:
    src = Path(path)
    dest = src.with_suffix(src.suffix + suffix)
    shutil.copy2(src, dest)
    return dest
