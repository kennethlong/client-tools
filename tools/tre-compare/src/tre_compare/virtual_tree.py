"""Engine-faithful virtual-tree merge for SWG TRE/TOC archives (Phase-28).

Mirrors ``TreeFile.cpp`` / ``TreeFile_SearchNode.cpp`` precedence, canonicalization,
and tombstone semantics in pure Python (D-03; no engine imports). Consumes the
priority-ordered :class:`~tre_compare.scanner.SearchNode` list and produces a single
merged :class:`VirtualTree` (canonical-path -> winning entry, with shadowed copies).

References (RESEARCH §Patterns 2/3/4, §Open-Q1; VERIFIED vs source this session):
  * Pattern 2 first-match-wins (TreeFile.cpp:437-461) — iterate nodes in priority order;
    the FIRST node with a path wins; later copies are shadowed.
  * Pattern 3 ``fixUpFileName`` (TreeFile.cpp:511-601) — strip LEADING slashes/./../, then
    backslash->forward-slash, lowercase, collapse repeated slashes. Ported VERBATIM in
    :func:`fix_up_file_name`.
  * Pattern 4 PER-NODE-TYPE tombstone — SearchTree (.tre) length-0 = global remove
    (TreeFile_SearchNode.cpp:397 + the find() ``!deleted`` guard); SearchTOC (.toc)
    length-0/offset-0 = "absent here, keep searching", NEVER shadows
    (TreeFile_SearchNode.cpp:807, :831).
  * Open-Q1 RESOLVED — searchPath (loose dir) nodes are eagerly ``os.walk``'d and their
    files injected at the node priority so the priority-10 override participates.

================================ APPROXIMATION BOUNDARY ================================
The following are KNOWN, INTENTIONAL out-of-scope items for Phase 28 (review finding #7);
Phase 29 must revisit any that real installs exercise:

  * The eager ``searchPath`` ``os.walk`` is a STATIC snapshot (a manifest), NOT the
    engine's lazy per-lookup ``SearchPath::exists`` [TreeFile_SearchNode.cpp:109-113] —
    files added/removed after the scan are not reflected. This is the ONE place the static
    model adds behavior the lazy engine resolves at runtime.

  * :func:`safe_virtual_key` rejects interior-``..`` / absolute / drive / UNC keys — a
    DELIBERATE non-engine-faithful hardening. The engine ``fixUpFileName`` strips only
    LEADING ``..``; this wrapper additionally drops a key the client WOULD resolve. An
    accepted compare-tool tradeoff (possible false missing/changed in Phase 29) that keeps
    the Phase-29 extraction key space safe (threat T-28-03-01).

  * The scanner/merge OMIT ``searchAbsolute``, ``searchCache``, and preloaded cache files
    [TreeFile.cpp:151-181, VERIFIED] — absent from whitengold's live cfg. Files served ONLY
    via those nodes are not in the virtual tree. Out-of-scope for Phase 28; flagged for
    Phase 29 if real installs use them.

  * Case-insensitive collapse: ``fix_up_file_name`` lowercases, so two loose files differing
    only by case collapse to one key (engine-faithful — the engine also lowercases).
=======================================================================================
"""

from __future__ import annotations

import os
import stat
import struct
import zlib
from dataclasses import dataclass, field, replace
from typing import Iterator

from .parser import (
    TreFormatError,
    UnsupportedTreVersionError,
    detect_master_index_kind,
    read_cot2000_entries,
    read_search_toc_entries,
    read_tre_entries,
    read_tre_header,
    toc_entry_stride,
)
from .parser.tre_reader import (
    COT2000_GLOBAL_ENTRY_SIZE,
    MasterIndexKind,
    read_cot2000_header,
    read_search_toc_header,
)
from .scanner import ScanResult, SearchNode

# Sane upper bound on a header's declared file count — a header claiming billions of files
# is rejected up front (review finding #8 / Cursor; threat T-28-03-04).
MAX_ENTRIES = 5_000_000

# Exceptions a malformed/hostile archive node may raise; caught per-node for fail-isolation.
_NODE_FAULTS = (
    TreFormatError,
    UnsupportedTreVersionError,
    struct.error,
    OSError,
    MemoryError,
    zlib.error,
)


# ---------------------------------------------------------------------------------------
# (a) fix_up_file_name — VERBATIM engine port (TreeFile.cpp:511-601). Strips LEADING
#     slashes/./../ ONLY, then lowercases + forward-slashes + collapses repeats. It does
#     NOT reject interior `..` or absolute paths (review finding #3 — engine-faithful).
# ---------------------------------------------------------------------------------------
def fix_up_file_name(name: str) -> str:
    """Canonicalize *name* exactly as the engine ``fixUpFileName`` does (verbatim port).

    Strips ALL leading ``\\``/``/``, then leading ``./``/``.\\`` (repeatedly), then leading
    ``../``/``..\\`` (repeatedly); converts every ``\\`` to ``/``; lowercases every non-slash
    char; collapses repeated consecutive slashes to a single ``/``. Interior ``..`` and
    absolute/UNC forms are LEFT AS-IS (the engine does not reject them) — rejection lives in
    the separate :func:`safe_virtual_key` hardening wrapper.
    """
    f = name
    while f[:1] in ("\\", "/"):
        f = f[1:]
    while f[:2] in ("./", ".\\"):
        f = f[2:]
    while f[:3] in ("../", "..\\"):
        f = f[3:]
    out: list[str] = []
    prev_slash = False
    for c in f:
        if c in ("\\", "/"):
            cur_slash = True
            ch = "/"
        else:
            cur_slash = False
            ch = c.lower()
        if not cur_slash or not prev_slash:
            out.append(ch)
            prev_slash = cur_slash
    return "".join(out)


# ---------------------------------------------------------------------------------------
# (b) safe_virtual_key — SEPARATE hardening wrapper (review finding #3; threat T-28-03-01).
#     NOT named after the engine routine; a DELIBERATE non-engine divergence.
# ---------------------------------------------------------------------------------------
def safe_virtual_key(name: str) -> str | None:
    """Canonicalize via :func:`fix_up_file_name`, then REJECT keys that escape the virtual root.

    DELIBERATE, NON-ENGINE-FAITHFUL hardening (review finding #3): the engine's
    ``fixUpFileName`` strips only LEADING ``..``; this wrapper additionally drops keys with a
    residual interior ``..`` segment, a drive-letter/UNC/absolute first segment, or an empty
    result. A compare tool may thus drop a path the client would resolve — an accepted
    tradeoff to keep the Phase-29 extraction key space safe. Returns the canonical key, or
    ``None`` for an unsafe/empty key (skip-and-record, NOT raise, so a hostile archive cannot
    DoS the whole scan).
    """
    # UNC reject on the RAW name FIRST: fix_up_file_name strips leading slashes, so a
    # `//host/share` / `\\host\share` form would otherwise canonicalize to a benign-looking
    # `host/share` key and the post-canonicalization guard below could never fire (review
    # finding #3 — the wrapper's documented "reject UNC first segment" intent).
    raw = name.replace("\\", "/")
    if raw.startswith("//"):
        return None
    key = fix_up_file_name(name)
    if not key:
        return None
    segs = key.split("/")
    if any(s == ".." for s in segs):  # residual interior `..`
        return None
    first = segs[0]
    # drive-letter absolute (`c:` / `c:/x`) — first segment ends with ':' or is like `c:...`
    if first.endswith(":") or (len(first) >= 2 and first[1] == ":"):
        return None
    return key


# ---------------------------------------------------------------------------------------
# (c) MergedEntry / (d) VirtualTree data model
# ---------------------------------------------------------------------------------------
@dataclass(frozen=True)
class MergedEntry:
    """A single winning entry in the merged virtual tree.

    shadowed -- ONLY later REAL visible candidate nodes shadowed by the winner (review
                finding #4). It EXCLUDES toc absent-here (length-0/offset-0) entries AND
                tree tombstones. This single definition is asserted by Plan 04.
    """

    canonical_path: str
    winner_node: SearchNode
    length: int
    compressed_length: int
    shadowed: tuple[SearchNode, ...]


@dataclass(frozen=True)
class VirtualTree:
    """The merged virtual tree.

    entries     -- canonical_path -> MergedEntry (the visible winners)
    tombstoned  -- canonical paths globally removed by a tree length-0 tombstone
    rejected    -- raw paths skipped by safe_virtual_key (traversal/absolute/UNC/empty)
    node_errors -- (node, error-message) per malformed/OOB-bounds node (review finding #8) —
                   makes malformed-archive skips AND bounds-preflight failures OBSERVABLE
                   to tests and Phase 29.
    """

    entries: dict[str, MergedEntry] = field(default_factory=dict)
    tombstoned: set[str] = field(default_factory=set)
    rejected: list[str] = field(default_factory=list)
    node_errors: list[tuple[SearchNode, str]] = field(default_factory=list)


# ---------------------------------------------------------------------------------------
# (e) Per-node entry enumeration + bounds preflights
# ---------------------------------------------------------------------------------------
def _is_reparse_or_link(full: str) -> bool:
    """True if *full* is a symlink OR (Windows) a reparse point (junction/mount)."""
    try:
        if os.path.islink(full):
            return True
        st = os.lstat(full)
        attrs = getattr(st, "st_file_attributes", 0)
        if attrs & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0):
            return True
    except OSError:
        # If we cannot stat it, treat as unsafe and let the caller skip+record it.
        return True
    return False


def _preflight_tre(node: SearchNode, node_errors: list[tuple[SearchNode, str]]) -> bool:
    """Bounds preflight for a `.tre` node (review findings #6/#8/Cursor; threat T-28-03-04).

    Runs BEFORE read_tre_entries slices/decompresses. Returns True if safe to read.
    """
    hdr = read_tre_header(node.abspath)
    file_size = os.path.getsize(node.abspath)
    stride = toc_entry_stride(hdr.version)
    if hdr.toc_offset + hdr.size_of_toc + hdr.size_of_name_block > file_size:
        node_errors.append((node, "bounds preflight failed: toc+name block past EOF"))
        return False
    # `size_of_toc` is the COMPRESSED on-disk block size when `toc_compressor != 0`; the
    # entry array (number_of_files * stride) is UNcompressed, so it legitimately exceeds the
    # compressed size. This sanity bound is only meaningful for a stored (uncompressed) toc —
    # applying it to a zlib-compressed toc falsely rejected every SWGEmu-style loose `.tre`
    # (no master `.toc`). The EOF guard above already bounds the on-disk read either way.
    if hdr.toc_compressor == 0 and hdr.number_of_files * stride > hdr.size_of_toc:
        node_errors.append((node, "bounds preflight failed: number_of_files * stride > size_of_toc"))
        return False
    if hdr.number_of_files > MAX_ENTRIES:
        node_errors.append((node, f"bounds preflight failed: number_of_files > MAX_ENTRIES ({MAX_ENTRIES})"))
        return False
    return True


def _preflight_toc(node: SearchNode, node_errors: list[tuple[SearchNode, str]]) -> bool:
    """Bounds preflight for a `.toc` master index (review finding #2a/Codex; threat T-28-03-04).

    Validates the declared TOC + name (+ COT2000 count*stride) ranges fit within file size
    BEFORE the typed reader slices/decompresses. The header readers are cheap (read once +
    walk the small tree-name block). Returns True if safe to read.
    """
    file_size = os.path.getsize(node.abspath)
    kind = detect_master_index_kind(node.abspath)
    if kind == MasterIndexKind.SEARCH_TOC:
        h = read_search_toc_header(node.abspath)
        if h.toc_offset + h.size_of_toc + h.size_of_name_block > file_size:
            node_errors.append((node, "toc bounds preflight failed: SEARCH_TOC blocks past EOF"))
            return False
        if h.number_of_files > MAX_ENTRIES:
            node_errors.append((node, f"toc bounds preflight failed: SEARCH_TOC number_of_files > MAX_ENTRIES"))
            return False
    else:  # COT2000
        h = read_cot2000_header(node.abspath)
        if h.toc_block_offset + h.size_of_toc_block + h.size_of_name_block > file_size:
            node_errors.append((node, "toc bounds preflight failed: COT2000 blocks past EOF"))
            return False
        if h.number_of_files * COT2000_GLOBAL_ENTRY_SIZE > h.size_of_toc_block:
            node_errors.append((node, "toc bounds preflight failed: COT2000 number_of_files * 32 > size_of_toc_block"))
            return False
        if h.number_of_files > MAX_ENTRIES:
            node_errors.append((node, "toc bounds preflight failed: COT2000 number_of_files > MAX_ENTRIES"))
            return False
    return True


def _walk_search_path(
    node: SearchNode, node_errors: list[tuple[SearchNode, str]]
) -> Iterator[tuple[str, int, int, int]]:
    """Eager, deterministic, escape-safe os.walk of a searchPath (loose dir) node.

    Open-Q1 RESOLVED: inject each loose file at the node priority. PRUNES symlink/reparse
    DIRS from dirnames[:] BEFORE descent (review finding #5 — prevents junction loops/escapes),
    skips reparse FILES, sorts for determinism (review finding #7), uses followlinks=False.
    A missing override dir yields nothing and is NOT a node_error.
    """
    base = node.abspath
    if not os.path.isdir(base):
        return
    for dirpath, dirnames, filenames in os.walk(base, followlinks=False):
        # PRUNE unsafe dirs BEFORE descent (mutate dirnames in place so os.walk skips them).
        safe_dirs: list[str] = []
        for d in dirnames:
            full = os.path.join(dirpath, d)
            if _is_reparse_or_link(full):
                node_errors.append((node, f"skipped reparse/symlink dir: {full}"))
                continue
            safe_dirs.append(d)
        dirnames[:] = sorted(safe_dirs)
        filenames.sort()
        for fn in filenames:
            full = os.path.join(dirpath, fn)
            if _is_reparse_or_link(full):
                node_errors.append((node, f"skipped reparse/symlink file: {full}"))
                continue
            rel = os.path.relpath(full, base)
            if rel.startswith("..") or os.path.isabs(rel):
                node_errors.append((node, f"skipped escaping path: {full}"))
                continue
            try:
                file_size = os.path.getsize(full)
            except OSError as exc:
                node_errors.append((node, f"stat failed: {full} ({exc})"))
                continue
            # Loose files are uncompressed: length == compressed_length == on-disk size; offset 0.
            # Yield the raw rel; the merge runs it through safe_virtual_key for uniform rejection.
            yield (rel, file_size, file_size, 0)


def iter_node_entries(
    node: SearchNode, node_errors: list[tuple[SearchNode, str]]
) -> Iterator[tuple[str, int, int, int]]:
    """Yield ``(path, length, compressed_length, offset)`` for every entry in *node*.

    Fail-isolation (threat T-28-03-02): the WHOLE per-node body is wrapped so one hostile/
    garbage archive records ``(node, str(exc))`` in *node_errors* and is skipped, never
    aborting the multi-node scan. Both `.tre` and `.toc` nodes run a bounds preflight BEFORE
    the typed reader slices/decompresses (threat T-28-03-04).
    """
    try:
        if node.kind == "tree":
            if not _preflight_tre(node, node_errors):
                return
            for e in read_tre_entries(node.abspath):
                yield (e.path, e.length, e.compressed_length, e.offset)
        elif node.kind == "toc":
            if not _preflight_toc(node, node_errors):
                return
            kind = detect_master_index_kind(node.abspath)
            if kind == MasterIndexKind.SEARCH_TOC:
                for e in read_search_toc_entries(node.abspath):
                    yield (e.path, e.length, e.compressed_length, e.offset)
            else:
                for e in read_cot2000_entries(node.abspath):
                    yield (e.path, e.length, e.compressed_length, e.offset)
        elif node.kind == "path":
            yield from _walk_search_path(node, node_errors)
    except _NODE_FAULTS as exc:
        node_errors.append((node, str(exc)))
        return


# ---------------------------------------------------------------------------------------
# (d) build_virtual_tree — single descending pass, first-hit-wins, per-node-type tombstone
# ---------------------------------------------------------------------------------------
def build_virtual_tree(scan: ScanResult, *, follow_search_path: bool = True) -> VirtualTree:
    """Merge *scan.nodes* into a single :class:`VirtualTree` (engine-faithful first-hit-wins).

    The merge is a SINGLE DESCENDING PASS over ``scan.nodes`` (already engine-faithful
    ordered: higher priority first; path<tree<toc within priority). INVARIANT: only the
    FIRST encountered record for a canonical key decides visibility — exactly like the engine
    ``find()`` walk. A tombstone matters ONLY when reached BEFORE any real winner.

    The merge key is :func:`safe_virtual_key` (which wraps :func:`fix_up_file_name`) ONLY —
    NEVER the TOC ``crc`` field (it is a path-CRC, not a content hash; RESEARCH anti-pattern).
    """
    claimed: dict[str, MergedEntry] = {}
    tombstoned: set[str] = set()
    rejected: list[str] = []
    node_errors: list[tuple[SearchNode, str]] = []

    for node in scan.nodes:
        if node.kind == "path" and not follow_search_path:
            continue
        for (path, length, clen, offset) in iter_node_entries(node, node_errors):
            key = safe_virtual_key(path)  # review finding #3 — hardening wrapper
            if key is None:  # residual `..` / drive / UNC / empty
                rejected.append(path)
                continue
            if key in claimed or key in tombstoned:
                # A LATER (lower-or-equal priority) copy for an ALREADY-decided key.
                # The first encountered record already decided visibility. Record the shadow
                # ONLY when a real winner exists AND this later entry is itself a REAL visible
                # candidate — NOT a tree tombstone, NOT a toc absent entry (review finding #4).
                # If key is tombstoned (no winner), just skip.
                if key in claimed:
                    is_tree_tombstone = node.kind == "tree" and length == 0
                    is_toc_absent = node.kind == "toc" and (length == 0 or offset == 0)
                    if not is_tree_tombstone and not is_toc_absent:
                        claimed[key] = replace(
                            claimed[key],
                            shadowed=claimed[key].shadowed + (node,),
                        )
                continue
            # FIRST encounter for this key — decide visibility now:
            if node.kind == "tree" and length == 0:
                tombstoned.add(key)  # tree tombstone reached first -> global remove
                continue
            if node.kind == "toc" and (length == 0 or offset == 0):
                continue  # absent HERE; NEVER tombstone — keep searching lower priority
            claimed[key] = MergedEntry(
                canonical_path=key,
                winner_node=node,
                length=length,
                compressed_length=clen,
                shadowed=(),
            )

    return VirtualTree(
        entries=claimed,
        tombstoned=tombstoned,
        rejected=rejected,
        node_errors=node_errors,
    )
