"""Pure-function diff engine for two SWG installs (Phase-29 / TRE-02/03/04).

The HEADLESS core every later layer (cache, FastAPI, Phase-30 UI, future TREM) imports.
It MUST stay pure: it imports ONLY ``.parser`` / ``.parser.tre_reader`` / ``.scanner`` /
``.virtual_tree`` + stdlib + ``xxhash``. It imports NO ``fastapi`` and NO ``sqlite3`` at any
scope, so Phase 30 needs zero backend refactor.

Three layers:
  * SET-level  (TRE-02): :func:`diff_archive_set` over the two scans' tree/toc nodes,
    keyed by ``(basename, kind)`` so a tree<->toc basename collision surfaces as two
    distinct rows. A corrupt archive degrades to a ``fault`` row via :func:`stat_archive`'s
    fault wrapper, never aborting the set diff.
  * FILE-level (TRE-03): :func:`diff_virtual_trees` emits lean tri-state rows from
    ``(length, compressed_length)`` (NEVER the TOC ``crc`` path-CRC), plus an OPTIONAL
    ``qualifier`` that distinguishes a one-sided tombstone / rejection / parse-failure from
    a misleading bare added/removed. A tombstoned-both-sides path still emits a row.
  * DRILL-in  (TRE-04): :func:`drill_in` returns winner metadata + winning archive +
    shadowed copies + a structured :class:`DriveHashResult` (xxhash of DECOMPRESSED bytes)
    per side, resolving a same-(len,clen)-different-bytes false-identical. TREE and TOC
    payloads resolve SYMMETRICALLY (match ``fix_up_file_name(e.path)==vpath``, read by RAW
    ``e.path``). The hash read NEVER raises — a bare ``KeyError`` (NOT in ``_NODE_FAULTS``)
    degrades to a ``DriveHashResult`` error. ``drill_in`` returns a DOMAIN status
    (``ok``/``not_found``/``rejected``); diff.py stays HTTP-agnostic (Plan 03 maps these).
"""

from __future__ import annotations

import os
import posixpath
from dataclasses import dataclass

import xxhash

from .parser import (
    detect_master_index_kind,
    read_cot2000_entries,
    read_search_toc_entries,
    read_tre_entries,
    read_tre_header,
    read_tre_payload,
)
from .parser.tre_reader import (
    MasterIndexKind,
    read_cot2000_header,
    read_search_toc_header,
)
from .scanner import ScanResult, SearchNode
from .virtual_tree import (
    _NODE_FAULTS,
    MergedEntry,
    VirtualTree,
    fix_up_file_name,
    safe_virtual_key,
)

# read_tre_payload raises a BARE ``KeyError(logical_name)`` on a name miss; KeyError is NOT in
# _NODE_FAULTS (Opus, code-verified). It MUST be caught alongside the format/IO faults or a
# drill-in 500s. Local fault tuple for the never-raise hash read (T-29-01-02).
_HASH_FAULTS = (*_NODE_FAULTS, KeyError)


# =======================================================================================
# Result dataclasses (frozen)
# =======================================================================================
@dataclass(frozen=True)
class ArchiveStat:
    """Per-archive set-level metadata. ``fault`` is non-None when stat failed (size/count -1)."""

    basename: str
    abspath: str
    kind: str
    size: int
    entry_count: int
    version: str
    fault: str | None = None


@dataclass(frozen=True)
class DriveHashResult:
    """Structured content-hash result (Codex divergent finding).

    hexdigest      -- xxh3_64 hex of the DECOMPRESSED bytes, or None on a degraded read.
    error          -- the str(exc) when the read degraded (None on success).
    source_archive -- basename of the archive the winner was served from.
    """

    hexdigest: str | None
    error: str | None
    source_archive: str


@dataclass(frozen=True)
class DrillResult:
    """Domain drill-in result. ``status`` in {"ok","not_found","rejected"} (HTTP-agnostic)."""

    status: str
    path: str = ""
    winner_left: MergedEntry | None = None
    winner_right: MergedEntry | None = None
    winning_archive_left: str | None = None
    winning_archive_right: str | None = None
    shadowed_left: tuple[SearchNode, ...] = ()
    shadowed_right: tuple[SearchNode, ...] = ()
    hash_left: DriveHashResult | None = None
    hash_right: DriveHashResult | None = None
    verdict: str | None = None


# =======================================================================================
# SET-level (TRE-02)
# =======================================================================================
def stat_archive(node: SearchNode) -> ArchiveStat:
    """Stat one archive node into an :class:`ArchiveStat`; NEVER raise (P-also-agreed).

    The WHOLE body is wrapped in ``_NODE_FAULTS`` — a corrupt/garbage archive degrades to a
    fault row (size/count -1, ``fault`` populated) so it cannot abort the set diff.
    """
    basename = os.path.basename(node.abspath)
    try:
        size = os.path.getsize(node.abspath)
        if node.kind == "tree":
            hdr = read_tre_header(node.abspath)
            return ArchiveStat(
                basename=basename,
                abspath=node.abspath,
                kind=node.kind,
                size=size,
                entry_count=hdr.number_of_files,
                version=hdr.version,
            )
        # toc: branch on the concrete master-index kind for the count + a version string.
        kind = detect_master_index_kind(node.abspath)
        if kind == MasterIndexKind.SEARCH_TOC:
            h = read_search_toc_header(node.abspath)
            return ArchiveStat(
                basename=basename,
                abspath=node.abspath,
                kind=node.kind,
                size=size,
                entry_count=h.number_of_files,
                version=MasterIndexKind.SEARCH_TOC.value,
            )
        h = read_cot2000_header(node.abspath)
        return ArchiveStat(
            basename=basename,
            abspath=node.abspath,
            kind=node.kind,
            size=size,
            entry_count=h.number_of_files,
            version=MasterIndexKind.COT2000.value,
        )
    except _NODE_FAULTS as exc:
        return ArchiveStat(
            basename=basename,
            abspath=node.abspath,
            kind=node.kind,
            size=-1,
            entry_count=-1,
            version="",
            fault=str(exc),
        )


def _archive_stats(scan: ScanResult) -> dict[tuple[str, str], ArchiveStat]:
    """Map ``(basename, kind)`` -> ArchiveStat over the tree/toc nodes only (skip loose path)."""
    out: dict[tuple[str, str], ArchiveStat] = {}
    for n in scan.nodes:
        if n.kind not in ("tree", "toc"):
            continue  # loose searchPath dirs are not archives (Pitfall 6)
        st = stat_archive(n)
        out[(st.basename, n.kind)] = st
    return out


def diff_archive_set(scanL: ScanResult, scanR: ScanResult) -> list[dict]:
    """Set-level archive diff keyed by ``(basename, kind)`` (TRE-02 / D-04 / D-05 / SC#1).

    only-left -> ``removed``; only-right -> ``added``; both -> ``changed`` if size OR
    entry_count OR version differs else ``identical``. If either side's stat returned a fault
    the row status is ``fault`` (with ``fault_left``/``fault_right``). A ``(basename, kind)``
    key keeps a tree<->toc basename collision as two distinct rows (P-also-agreed).
    """
    left = _archive_stats(scanL)
    right = _archive_stats(scanR)
    rows: list[dict] = []
    for key in sorted(set(left) | set(right)):
        basename, kind = key
        l = left.get(key)
        r = right.get(key)
        row: dict = {"basename": basename, "kind": kind}
        if l is not None and l.fault is not None:
            row["fault_left"] = l.fault
        if r is not None and r.fault is not None:
            row["fault_right"] = r.fault
        if (l is not None and l.fault is not None) or (r is not None and r.fault is not None):
            row["status"] = "fault"
            row["version_left"] = l.version if l else None
            row["version_right"] = r.version if r else None
            rows.append(row)
            continue
        if l is None:
            row["status"] = "added"
            row["version_right"] = r.version
            rows.append(row)
            continue
        if r is None:
            row["status"] = "removed"
            row["version_left"] = l.version
            rows.append(row)
            continue
        changed = (l.size != r.size) or (l.entry_count != r.entry_count) or (l.version != r.version)
        row["status"] = "changed" if changed else "identical"
        row["size_delta"] = r.size - l.size
        row["entry_count_delta"] = r.entry_count - l.entry_count
        row["version_left"] = l.version
        row["version_right"] = r.version
        rows.append(row)
    return rows


# =======================================================================================
# FILE-level tri-state + qualifier (TRE-03)
# =======================================================================================
def _side_meta(entry: MergedEntry | None) -> dict | None:
    if entry is None:
        return None
    return {"len": entry.length, "clen": entry.compressed_length}


def _node_error_archives(vt: VirtualTree) -> set[str]:
    """Set of abspaths of nodes that recorded a parse/bounds failure in vt.node_errors."""
    return {node.abspath for (node, _msg) in vt.node_errors}


def _rejected_keys(vt: VirtualTree) -> set[str]:
    """Canonical keys that a RAW path in vt.rejected would have produced via safe_virtual_key.

    vt.rejected holds RAW paths whose safe_virtual_key returned None — so safe_virtual_key
    cannot recover the key. Recover the canonical form by running fix_up_file_name (engine
    canonicalize: lowercase + forward-slash + strip leading dots) THEN posixpath.normpath to
    collapse the offending interior `..` the engine WOULD have resolved — so the qualifier can
    be attached to the row that the OTHER side contributes under that canonical key (P1).
    Empty / escaping ("..") normalized results are dropped (no row to annotate).
    """
    out: set[str] = set()
    for raw in vt.rejected:
        norm = posixpath.normpath(fix_up_file_name(raw))
        if norm and norm != "." and not norm.startswith(".."):
            out.add(norm)
    return out


def diff_virtual_trees(vtL: VirtualTree, vtR: VirtualTree) -> dict:
    """File-level tri-state lean rows + optional qualifier + summary (D-06/D-09/P1).

    Status from ``(length, compressed_length)`` ONLY — never crc. Rows are lean
    ``{path, status, left, right}`` (left/right = ``{len, clen}`` or None). An OPTIONAL
    ``qualifier`` list annotates one-sided tombstone / rejection / parse-failure. A
    tombstoned-both-sides path still emits a row. Returns ``{"rows": [...], "summary": {...}}``.
    """
    err_left = _node_error_archives(vtL)
    err_right = _node_error_archives(vtR)
    rej_left = _rejected_keys(vtL)
    rej_right = _rejected_keys(vtR)

    keys = sorted(
        set(vtL.entries) | set(vtR.entries) | set(vtL.tombstoned) | set(vtR.tombstoned)
    )
    rows: list[dict] = []
    status_counts: dict[str, int] = {}
    for key in keys:
        l = vtL.entries.get(key)
        r = vtR.entries.get(key)
        lm = _side_meta(l)
        rm = _side_meta(r)
        if l is not None and r is not None:
            status = (
                "identical-by-metadata"
                if (l.length, l.compressed_length) == (r.length, r.compressed_length)
                else "changed"
            )
        elif l is not None:
            status = "removed"
        elif r is not None:
            status = "added"
        else:
            # neither side has a visible entry — only reachable when the key is tombstoned
            # on at least one side. Surface as a row (never vanish) with no live metadata.
            status = "tombstoned"

        qualifier: list[str] = []
        if key in vtL.tombstoned:
            qualifier.append("tombstoned_left")
        if key in vtR.tombstoned:
            qualifier.append("tombstoned_right")
        if key in rej_left:
            qualifier.append("rejected_left")
        if key in rej_right:
            qualifier.append("rejected_right")
        if l is not None and l.winner_node.abspath in err_left:
            qualifier.append("error_left")
        if r is not None and r.winner_node.abspath in err_right:
            qualifier.append("error_right")

        row: dict = {"path": key, "status": status, "left": lm, "right": rm}
        if qualifier:
            row["qualifier"] = qualifier
        rows.append(row)
        status_counts[status] = status_counts.get(status, 0) + 1

    summary = {
        "left": {
            "node_errors": len(vtL.node_errors),
            "rejected": len(vtL.rejected),
            "tombstoned": len(vtL.tombstoned),
        },
        "right": {
            "node_errors": len(vtR.node_errors),
            "rejected": len(vtR.rejected),
            "tombstoned": len(vtR.tombstoned),
        },
        "status_counts": status_counts,
    }
    return {"rows": rows, "summary": summary}


# =======================================================================================
# DRILL-in + on-demand content hash (TRE-04)
# =======================================================================================
def _payload_tre_for_toc(node: SearchNode, vpath: str, toc_tree_path: str | None) -> tuple[str, str]:
    """Resolve a TOC-served winner's payload .tre + RAW entry path (RESEARCH Pattern 3).

    Match ``fix_up_file_name(e.path) == vpath`` over the master-index entries, then join the
    entry's embedded ``tre_file`` onto the cfg's ``toc_tree_path`` (no caller path component
    enters the join — T-29-01-03). Returns ``(payload_tre_abspath, raw_entry_path)``. Raises
    ``KeyError(vpath)`` on a miss (caught by ``_HASH_FAULTS`` -> degrade).
    """
    kind = detect_master_index_kind(node.abspath)
    if kind == MasterIndexKind.SEARCH_TOC:
        entries = read_search_toc_entries(node.abspath)
    else:
        entries = read_cot2000_entries(node.abspath)
    for e in entries:
        if fix_up_file_name(e.path) == vpath:
            payload_tre = os.path.join(toc_tree_path or "", e.tre_file)
            return payload_tre, e.path
    raise KeyError(vpath)


def hash_virtual_file(node: SearchNode, vpath: str, toc_tree_path: str | None) -> DriveHashResult:
    """xxhash the DECOMPRESSED bytes of *vpath* served by *node*; NEVER raise.

    The whole resolve+read is wrapped in ``_HASH_FAULTS`` (= ``_NODE_FAULTS`` + ``KeyError``,
    the Opus never-raise fix). On any fault -> ``DriveHashResult(hexdigest=None, error=...)``.

      * tree -> SYMMETRIC resolve: iterate ``read_tre_entries``, find ``fix_up_file_name(e.path)
        == vpath``, read by the RAW ``e.path`` (NOT the canonical vpath — Opus asymmetry fix).
      * toc  -> ``_payload_tre_for_toc`` then ``read_tre_payload`` of the payload .tre.
      * path -> loose file at ``os.path.join(node.abspath, vpath)``.
    """
    source = os.path.basename(node.abspath)
    try:
        if node.kind == "tree":
            raw_path = None
            for e in read_tre_entries(node.abspath):
                if fix_up_file_name(e.path) == vpath:
                    raw_path = e.path
                    break
            if raw_path is None:
                raise KeyError(vpath)
            data = read_tre_payload(node.abspath, raw_path)
        elif node.kind == "toc":
            payload_tre, raw_path = _payload_tre_for_toc(node, vpath, toc_tree_path)
            data = read_tre_payload(payload_tre, raw_path)
            source = os.path.basename(payload_tre)
        else:  # loose searchPath file
            with open(os.path.join(node.abspath, vpath), "rb") as fh:
                data = fh.read()
        return DriveHashResult(
            hexdigest=xxhash.xxh3_64_hexdigest(data),
            error=None,
            source_archive=source,
        )
    except _HASH_FAULTS as exc:
        return DriveHashResult(hexdigest=None, error=str(exc), source_archive=source)


def drill_in(
    vtL: VirtualTree,
    vtR: VirtualTree,
    path: str,
    scanL: ScanResult,
    scanR: ScanResult,
) -> DrillResult:
    """Drill into one virtual path: winner + shadowed + per-side content hash (D-07).

    Returns a DOMAIN status: ``rejected`` (safe_virtual_key->None), ``not_found`` (absent both
    sides), or ``ok``. diff.py stays HTTP-agnostic — Plan 03 maps rejected->400, not_found->404.
    The verdict upgrades an ``identical-by-metadata`` pair to ``content-confirmed-identical`` /
    ``content-changed`` (or ``content-indeterminate`` when a side's hash degraded).
    """
    key = safe_virtual_key(path)
    if key is None:
        return DrillResult(status="rejected", path=path)
    l = vtL.entries.get(key)
    r = vtR.entries.get(key)
    if l is None and r is None:
        return DrillResult(status="not_found", path=key)

    hash_left = (
        hash_virtual_file(l.winner_node, key, scanL.toc_tree_path) if l is not None else None
    )
    hash_right = (
        hash_virtual_file(r.winner_node, key, scanR.toc_tree_path) if r is not None else None
    )

    verdict = None
    if l is not None and r is not None:
        hl = hash_left.hexdigest if hash_left else None
        hr = hash_right.hexdigest if hash_right else None
        if hl is None or hr is None:
            verdict = "content-indeterminate"
        elif hl == hr:
            verdict = "content-confirmed-identical"
        else:
            verdict = "content-changed"

    return DrillResult(
        status="ok",
        path=key,
        winner_left=l,
        winner_right=r,
        winning_archive_left=os.path.basename(l.winner_node.abspath) if l else None,
        winning_archive_right=os.path.basename(r.winner_node.abspath) if r else None,
        shadowed_left=l.shadowed if l else (),
        shadowed_right=r.shadowed if r else (),
        hash_left=hash_left,
        hash_right=hash_right,
        verdict=verdict,
    )
