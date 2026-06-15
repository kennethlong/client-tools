"""Stdlib-sqlite3 cache layer for tre-compare (Phase-29 Plan 02 / D-10 / SC#4).

Two responsibilities, both keyed on a file IDENTITY of ``(abspath, mtime_ns, size)``
(INTEGER ``st_mtime_ns`` — NOT float ``st_mtime`` REAL; P4 closes the FAT32/network 1-2s
resolution + float-rounding false-HIT window):

  1. PER-ARCHIVE parsed-entry cache (``archive_meta`` + ``archive_entry``): the expensive
     ``iter_node_entries`` parse of a tree/toc archive is run ONCE; subsequent calls on the
     SAME identity return the cached rows WITHOUT re-parsing (proven by a parse-skip spy, P5).
     Each toc row stores its ``tre_file`` payload name so a drill-in resolves the payload
     ``.tre`` without re-parsing a 193k-entry ``.toc`` (Pitfall 7).
  2. DRILL-IN HASH MEMO (``file_hash``): memoizes the Plan-01 ``hash_virtual_file`` hexdigest
     keyed by ``(abspath, mtime_ns, size, vpath, algo)``.

Plus :func:`build_virtual_tree_cached` — a PARITY-EQUIVALENT re-expression of the Phase-28
``build_virtual_tree`` descending-pass merge (lines 360-396, VERBATIM branch logic) that
sources each tree/toc node's entries from the cache and each ``path`` node from the live
``iter_node_entries``. CRITICAL (P2): the tombstone / toc-absent / shadow branches read
``node.kind`` — threaded from the LIVE node, NEVER re-derived from a cached row (a row carries
no kind; only ``archive_meta`` / the SearchNode do — a wrong source flips tombstone<->toc-absent
semantics).

Concurrency (T-29-02-03 / Sonnet / Codex+Opus): the FastAPI (Plan 03) threadpool shares ONE
connection, so ``check_same_thread=False`` + WAL + ``PRAGMA busy_timeout`` + a process-wide
write ``threading.Lock`` guard ALL writes AND the MISS detect-then-insert sequence; the MISS
INSERT is ``INSERT OR IGNORE`` so two threads racing the SAME fresh-archive MISS cannot raise
a PRIMARY KEY ``IntegrityError`` / 500. A fresh cursor is used per op (never shared across
threads). Default db path is resolved via ``__file__`` (under ``tools/tre-compare/.cache/``),
NOT cwd, so a wrong-dir launch never silently gets an empty cache (P-divergent Sonnet).

Pure stdlib + relative imports from ``.parser`` / ``.scanner`` / ``.virtual_tree``. NO fastapi.
"""

from __future__ import annotations

import os
import sqlite3
import threading
from pathlib import Path

from .parser import (
    detect_master_index_kind,
    read_cot2000_entries,
    read_search_toc_entries,
    read_tre_header,
)
from .parser.tre_reader import (
    MasterIndexKind,
    read_cot2000_header,
    read_search_toc_header,
)
from .scanner import ScanResult, SearchNode

# Imported AT MODULE SCOPE so a test can spy ``tre_compare.cache.iter_node_entries`` and assert
# the parse-skip on a HIT (P5). build_virtual_tree_cached also routes `path` nodes through it.
from .virtual_tree import (
    MergedEntry,
    VirtualTree,
    _NODE_FAULTS,
    fix_up_file_name,
    iter_node_entries,
    safe_virtual_key,
)

# Module-relative default DB dir: tools/tre-compare/.cache/  (cache.py is at
# tools/tre-compare/src/tre_compare/cache.py -> parents: tre_compare, src, tre-compare).
_DEFAULT_DB = Path(__file__).resolve().parent.parent.parent / ".cache" / "tre_compare.sqlite"

_DEFAULT_ALGO = "xxh3_64"

_SCHEMA = """
CREATE TABLE IF NOT EXISTS archive_meta (
    abspath     TEXT    NOT NULL,
    mtime_ns    INTEGER NOT NULL,
    size        INTEGER NOT NULL,
    kind        TEXT,
    version     TEXT,
    entry_count INTEGER,
    PRIMARY KEY (abspath, mtime_ns, size)
);
CREATE TABLE IF NOT EXISTS archive_entry (
    abspath           TEXT    NOT NULL,
    mtime_ns          INTEGER NOT NULL,
    size              INTEGER NOT NULL,
    vpath             TEXT    NOT NULL,
    length            INTEGER,
    compressed_length INTEGER,
    offset            INTEGER,
    tre_file          TEXT,
    PRIMARY KEY (abspath, mtime_ns, size, vpath)
);
CREATE TABLE IF NOT EXISTS file_hash (
    abspath   TEXT    NOT NULL,
    mtime_ns  INTEGER NOT NULL,
    size      INTEGER NOT NULL,
    vpath     TEXT    NOT NULL,
    algo      TEXT    NOT NULL DEFAULT 'xxh3_64',
    hexdigest TEXT,
    PRIMARY KEY (abspath, mtime_ns, size, vpath, algo)
);
"""


def _identity(abspath: str) -> tuple[int, int]:
    """Stat *abspath* ONCE -> (st_mtime_ns INTEGER, st_size). Integer ns, never float (P4)."""
    st = os.stat(abspath)
    return st.st_mtime_ns, st.st_size


class Cache:
    """sqlite3 cache for per-archive parsed entries + drill-in hash memos.

    ``Cache(db_path=None, *, enabled=True)`` — ``db_path`` defaults to the ``__file__``-relative
    ``.cache/tre_compare.sqlite``. ``enabled=False`` makes every ``archive_entries`` a live parse
    (a global cache-bust knob alongside the per-call ``no_cache=``).
    """

    def __init__(self, db_path: str | Path | None = None, *, enabled: bool = True) -> None:
        self.enabled = enabled
        db = Path(db_path) if db_path is not None else _DEFAULT_DB
        db.parent.mkdir(parents=True, exist_ok=True)
        self._conn = sqlite3.connect(str(db), check_same_thread=False)
        self._conn.row_factory = sqlite3.Row
        # WAL + NORMAL sync + busy_timeout (Codex+Opus) for threadpool sharing.
        self._conn.execute("PRAGMA journal_mode=WAL")
        self._conn.execute("PRAGMA synchronous=NORMAL")
        self._conn.execute("PRAGMA busy_timeout=5000")
        self._conn.executescript(_SCHEMA)
        self._conn.commit()
        # Guards ALL writes AND the MISS detect-then-insert sequence (Sonnet concurrent-MISS).
        self._lock = threading.Lock()

    def close(self) -> None:
        self._conn.close()

    # ----------------------------------------------------------------------------------
    # per-archive parsed-entry cache
    # ----------------------------------------------------------------------------------
    def _select_entries(self, abspath: str, mtime_ns: int, size: int) -> list[dict]:
        cur = self._conn.execute(
            "SELECT vpath, length, compressed_length, offset, tre_file "
            "FROM archive_entry WHERE abspath=? AND mtime_ns=? AND size=? "
            "ORDER BY rowid",
            (abspath, mtime_ns, size),
        )
        return [dict(r) for r in cur.fetchall()]

    def _has_meta(self, abspath: str, mtime_ns: int, size: int) -> bool:
        cur = self._conn.execute(
            "SELECT 1 FROM archive_meta WHERE abspath=? AND mtime_ns=? AND size=? LIMIT 1",
            (abspath, mtime_ns, size),
        )
        return cur.fetchone() is not None

    def _toc_tre_files(self, node: SearchNode) -> dict[str, str]:
        """Map ``fix_up_file_name(e.path) -> tre_file`` for a toc node (Pitfall 7).

        Lets the MISS parse stamp each archive_entry row with the payload .tre name so a later
        drill-in needn't re-parse the whole master index. Best-effort: a fault yields {}.
        """
        try:
            kind = detect_master_index_kind(node.abspath)
            entries = (
                read_search_toc_entries(node.abspath)
                if kind == MasterIndexKind.SEARCH_TOC
                else read_cot2000_entries(node.abspath)
            )
            return {fix_up_file_name(e.path): e.tre_file for e in entries}
        except _NODE_FAULTS:
            return {}

    def _archive_meta_fields(self, node: SearchNode) -> tuple[str | None, int | None]:
        """Best-effort (version, entry_count) for archive_meta; (None, None) on a fault."""
        try:
            if node.kind == "tree":
                h = read_tre_header(node.abspath)
                return h.version, h.number_of_files
            kind = detect_master_index_kind(node.abspath)
            if kind == MasterIndexKind.SEARCH_TOC:
                h = read_search_toc_header(node.abspath)
                return MasterIndexKind.SEARCH_TOC.value, h.number_of_files
            h = read_cot2000_header(node.abspath)
            return MasterIndexKind.COT2000.value, h.number_of_files
        except _NODE_FAULTS:
            return None, None

    def archive_entries(
        self,
        node: SearchNode,
        node_errors: list[tuple[SearchNode, str]] | None = None,
        *,
        no_cache: bool = False,
    ) -> list[dict]:
        """Return cached/parsed entry rows for a tree/toc *node* (P2/P4).

        Takes a :class:`SearchNode` (NOT a bare abspath) so ``node.kind`` is threaded. Each row
        is ``{vpath, length, compressed_length, offset, tre_file}``. On a HIT the cached rows are
        returned WITHOUT re-parsing (parse-skip, P5). On a MISS the archive is parsed via the
        module-level ``iter_node_entries`` (Phase-28 fault isolation: a malformed archive records
        into *node_errors* and yields no rows), the ``tre_file`` payload name is resolved for toc
        rows (Pitfall 7), and the rows + ``archive_meta`` are persisted under the write lock with
        ``INSERT OR IGNORE`` (concurrent-MISS safe). When ``no_cache`` / ``not self.enabled`` the
        cache is bypassed entirely (re-parse every call — P4 mtime-preserving false-HIT window).
        """
        mtime_ns, size = _identity(node.abspath)
        abspath = node.abspath

        if not no_cache and self.enabled and self._has_meta(abspath, mtime_ns, size):
            return self._select_entries(abspath, mtime_ns, size)

        # MISS (or bypass) — PARSE. Fault isolation: iter_node_entries records into errs + skips.
        errs: list[tuple[SearchNode, str]] = []
        toc_map = self._toc_tre_files(node) if node.kind == "toc" else {}
        rows: list[dict] = []
        for (path, length, clen, offset) in iter_node_entries(node, errs):
            tre_file = toc_map.get(fix_up_file_name(path)) if node.kind == "toc" else None
            rows.append(
                {
                    "vpath": path,
                    "length": length,
                    "compressed_length": clen,
                    "offset": offset,
                    "tre_file": tre_file,
                }
            )

        # surface MISS-parse errors so a malformed archive on a cache MISS stays observable (Codex)
        if node_errors is not None:
            node_errors.extend(errs)

        if no_cache or not self.enabled:
            return rows

        version, entry_count = self._archive_meta_fields(node)
        with self._lock:
            # RE-CHECK under the lock: another thread may have filled it during our parse.
            if self._has_meta(abspath, mtime_ns, size):
                return self._select_entries(abspath, mtime_ns, size)
            self._conn.execute(
                "INSERT OR IGNORE INTO archive_meta "
                "(abspath, mtime_ns, size, kind, version, entry_count) VALUES (?,?,?,?,?,?)",
                (abspath, mtime_ns, size, node.kind, version, entry_count),
            )
            self._conn.executemany(
                "INSERT OR IGNORE INTO archive_entry "
                "(abspath, mtime_ns, size, vpath, length, compressed_length, offset, tre_file) "
                "VALUES (?,?,?,?,?,?,?,?)",
                [
                    (
                        abspath,
                        mtime_ns,
                        size,
                        r["vpath"],
                        r["length"],
                        r["compressed_length"],
                        r["offset"],
                        r["tre_file"],
                    )
                    for r in rows
                ],
            )
            self._conn.commit()
            return self._select_entries(abspath, mtime_ns, size)

    # ----------------------------------------------------------------------------------
    # drill-in hash memo
    # ----------------------------------------------------------------------------------
    def get_file_hash(
        self, node: SearchNode, vpath: str, *, algo: str = _DEFAULT_ALGO
    ) -> str | None:
        """Return the memoized hexdigest for (node identity, vpath, algo), or None on a MISS."""
        mtime_ns, size = _identity(node.abspath)
        cur = self._conn.execute(
            "SELECT hexdigest FROM file_hash "
            "WHERE abspath=? AND mtime_ns=? AND size=? AND vpath=? AND algo=?",
            (node.abspath, mtime_ns, size, vpath, algo),
        )
        row = cur.fetchone()
        return row["hexdigest"] if row is not None else None

    def put_file_hash(
        self, node: SearchNode, vpath: str, hexdigest: str, *, algo: str = _DEFAULT_ALGO
    ) -> None:
        """Memoize *hexdigest* under (node identity, vpath, algo) (INSERT OR REPLACE)."""
        mtime_ns, size = _identity(node.abspath)
        with self._lock:
            self._conn.execute(
                "INSERT OR REPLACE INTO file_hash "
                "(abspath, mtime_ns, size, vpath, algo, hexdigest) VALUES (?,?,?,?,?,?)",
                (node.abspath, mtime_ns, size, vpath, algo, hexdigest),
            )
            self._conn.commit()


# =======================================================================================
# build_virtual_tree_cached — parity-equivalent merge sourcing rows from the cache (A4/P2)
# =======================================================================================
def build_virtual_tree_cached(
    scan: ScanResult,
    cache: Cache,
    *,
    follow_search_path: bool = True,
    no_cache: bool = False,
) -> VirtualTree:
    """Re-express ``build_virtual_tree`` (lines 360-396) sourcing rows from *cache* (A4/P2).

    STRUCTURALLY IDENTICAL output to the builder: entries/winners, tombstoned set, rejected LIST
    (encounter order), shadowed TUPLES (element + order), node_errors. The merge is the SAME
    single descending pass, first-hit-wins; tombstone / toc-absent / shadow branches read
    ``node.kind`` from the LIVE node (NEVER a cached row — P2). tree/toc node entries come from
    ``cache.archive_entries(node, node_errors, no_cache=no_cache)``; ``path`` nodes come from the
    live ``iter_node_entries`` (loose dirs are not cached).
    """
    from dataclasses import replace

    claimed: dict[str, MergedEntry] = {}
    tombstoned: set[str] = set()
    rejected: list[str] = []
    node_errors: list[tuple[SearchNode, str]] = []

    for node in scan.nodes:
        if node.kind == "path" and not follow_search_path:
            continue

        if node.kind in ("tree", "toc"):
            cached_rows = cache.archive_entries(node, node_errors, no_cache=no_cache)
            entry_iter = (
                (r["vpath"], r["length"], r["compressed_length"], r["offset"])
                for r in cached_rows
            )
        else:  # path (loose dir) — live walk, not cached
            entry_iter = iter_node_entries(node, node_errors)

        for (path, length, clen, offset) in entry_iter:
            key = safe_virtual_key(path)
            if key is None:
                rejected.append(path)
                continue
            if key in claimed or key in tombstoned:
                if key in claimed:
                    is_tree_tombstone = node.kind == "tree" and length == 0
                    is_toc_absent = node.kind == "toc" and (length == 0 or offset == 0)
                    if not is_tree_tombstone and not is_toc_absent:
                        claimed[key] = replace(
                            claimed[key],
                            shadowed=claimed[key].shadowed + (node,),
                        )
                continue
            # FIRST encounter — decide visibility now (kind threaded from the LIVE node):
            if node.kind == "tree" and length == 0:
                tombstoned.add(key)
                continue
            if node.kind == "toc" and (length == 0 or offset == 0):
                continue
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
