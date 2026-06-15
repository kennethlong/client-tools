"""Cache-layer tests for ``tre_compare.cache`` (Phase-29 Plan 02 / D-10 / SC#4).

The cache is the HIGHEST-risk plan (REVIEWS): a ~40-line re-implementation of the
Phase-28 merge sourcing rows from sqlite instead of re-parsing. These tests are its
acceptance harness — they PROVE the parse-skip (a spy on ``iter_node_entries`` asserting
call-count, NOT mere row-equality, P5), the mtime_ns-keyed invalidation + ``--no-cache``
bypass (P4), the cached-merge PARITY across the EXPANDED 7-case matrix incl. shadowed-tuple
ORDER + rejected-LIST order (P2/A4), the ``tre_file`` drill-in payload cache (Pitfall 7),
the hash memo round-trip, and the concurrent-MISS race (no PRIMARY KEY 500, Sonnet).

All tests use ``tmp_path`` for the ``.sqlite`` DB (no shared cache state).
"""

from __future__ import annotations

import threading

import _fixtures
import pytest

from tre_compare.scanner import SearchNode, parse_shared_file
from tre_compare.virtual_tree import build_virtual_tree


# =======================================================================================
# helpers
# =======================================================================================
def _node_for(path, kind="tree", priority=9, seq=0):
    """A bare SearchNode pointing at an on-disk archive (the cache stats it itself)."""
    return SearchNode(
        kind=kind,
        priority=priority,
        abspath=str(path),
        raw_key=f"search{kind}_{seq:02d}_{priority}",
        sku=seq,
        cfg_seq=seq,
    )


def _spy_iter_entries(monkeypatch):
    """Wrap ``tre_compare.cache.iter_node_entries`` with a call-counter and return the box.

    Patches the symbol AS IMPORTED BY cache.py so the HIT/MISS/no_cache tests assert the
    REAL parse-skip (P5), not merely row-equality.
    """
    import tre_compare.cache as cache_mod

    real = cache_mod.iter_node_entries
    box = {"calls": 0}

    def counting(node, node_errors):
        box["calls"] += 1
        return real(node, node_errors)

    monkeypatch.setattr(cache_mod, "iter_node_entries", counting)
    return box


# =======================================================================================
# HIT / MISS / invalidation / no_cache — parse-skip PROVEN by a spy (P5)
# =======================================================================================
def test_miss_then_hit_parse_skip(tmp_path, monkeypatch):
    from tre_compare.cache import Cache

    spy = _spy_iter_entries(monkeypatch)
    arc = _fixtures.build_tre(tmp_path / "a.tre", [("d/x.dds", 4, 4)], version="0004")
    node = _node_for(arc)
    cache = Cache(tmp_path / "c.sqlite")

    rows1 = cache.archive_entries(node)        # MISS -> parses
    rows2 = cache.archive_entries(node)        # HIT -> NO parse

    assert spy["calls"] == 1, "the HIT must skip the parse (parse-skip proven, not row-equality)"
    assert rows1 == rows2
    assert any(r["vpath"] == "d/x.dds" for r in rows1)


def test_invalidation_rewrite_misses(tmp_path, monkeypatch):
    from tre_compare.cache import Cache

    spy = _spy_iter_entries(monkeypatch)
    arc = _fixtures.build_tre(tmp_path / "a.tre", [("d/x.dds", 4, 4)], version="0004")
    node = _node_for(arc)
    cache = Cache(tmp_path / "c.sqlite")

    cache.archive_entries(node)                # MISS (1)
    cache.archive_entries(node)                # HIT (still 1)
    assert spy["calls"] == 1

    # rewrite with NEW bytes (new mtime_ns + size) -> MISS again, new rows.
    _fixtures.build_tre(tmp_path / "a.tre", [("d/x.dds", 9, 9), ("d/y.dds", 2, 2)], version="0004")
    rows = cache.archive_entries(node)         # MISS (2)
    assert spy["calls"] == 2
    assert {r["vpath"] for r in rows} == {"d/x.dds", "d/y.dds"}


def test_no_cache_bypass_reparses(tmp_path, monkeypatch):
    from tre_compare.cache import Cache

    spy = _spy_iter_entries(monkeypatch)
    arc = _fixtures.build_tre(tmp_path / "a.tre", [("d/x.dds", 4, 4)], version="0004")
    node = _node_for(arc)
    cache = Cache(tmp_path / "c.sqlite")

    cache.archive_entries(node, no_cache=True)  # parse (1)
    cache.archive_entries(node, no_cache=True)  # parse AGAIN (2) — bypasses the cache
    assert spy["calls"] == 2, "no_cache must re-parse even on an unchanged archive (P4 false-HIT window)"


# =======================================================================================
# PARITY — cached merge == build_virtual_tree (basic + EXPANDED 7-case matrix, A4/P2)
# =======================================================================================
def _assert_tree_parity(cached, builder):
    """Structural equality INCLUDING shadowed-tuple ORDER + rejected LIST order (P2)."""
    assert set(cached.entries) == set(builder.entries), "entries keys differ"
    for key, be in builder.entries.items():
        ce = cached.entries[key]
        assert ce.winner_node == be.winner_node, f"winner differs for {key}"
        assert ce.length == be.length and ce.compressed_length == be.compressed_length
        # shadowed TUPLE element-equal AND order-sensitive (P2 — prior plan asserted only membership)
        assert ce.shadowed == be.shadowed, f"shadowed tuple/order differs for {key}"
    assert cached.tombstoned == builder.tombstoned, "tombstoned set differs"
    assert cached.rejected == builder.rejected, "rejected LIST (order) differs"
    assert len(cached.node_errors) == len(builder.node_errors), "node_error count differs"


def test_parity_basic(tmp_path):
    from tre_compare.cache import build_virtual_tree_cached, Cache

    meta = synthetic = _fixtures  # noqa: F841 (use the conftest fixture below instead)
    # build the synthetic install inline (mirror conftest's synthetic_install)
    install = tmp_path / "install"
    install.mkdir()
    (install / "override").mkdir()
    tomb = _fixtures.build_tre(install / "high_tomb.tre", [("foo/x.dds", 0, 0)], version="0004")
    real = _fixtures.build_tre(install / "low_real.tre", [("foo/x.dds", 11, 11)], version="0004")
    absent = _fixtures.build_search_toc(
        install / "high_absent.toc", [("foo/y.dds", 0, 0, 0)], tree_files=("t.tre",)
    )
    realtoc = _fixtures.build_search_toc(
        install / "low_real.toc", [("foo/y.dds", 7, 7, 128)], tree_files=("t.tre",)
    )
    cfg = install / "client.cfg"
    _fixtures.write_synthetic_cfg(
        cfg,
        [
            ("path", 10, install / "override"),
            ("tree", 9, tomb),
            ("tree", 8, real),
            ("toc", 5, absent),
            ("toc", 4, realtoc),
        ],
    )
    scan = parse_shared_file(cfg)
    builder = build_virtual_tree(scan)
    cache = Cache(tmp_path / "c.sqlite")
    cached = build_virtual_tree_cached(scan, cache)
    _assert_tree_parity(cached, builder)


def _build_parity_matrix_install(tmp_path):
    """Forge an install exercising ALL 7 P2 cases. Returns the cfg path.

    1. tree-tombstone-then-real-winner   (mid.tre tombstones a/t.dds, low.tre carries real)
    2. lower-priority tombstone AFTER a visible winner -> must NOT evict (win.tre real win/w.dds
       at p9, low.tre tombstones win/w.dds at p3)
    3. toc-absent (length==0 AND a separate offset==0 entry) -> never tombstones
    4. 3-way multi-shadow: hi/h.dds real at p9, then two REAL later copies at p8 + p7 ->
       shadowed has 2 nodes IN ORDER
    5. a `path` (loose dir) node in the mix carrying loose/l.dds
    6. a rejected key: an interior-`..` raw name -> safe_virtual_key None
    7. a malformed archive (truncated) -> node_error
    """
    inst = tmp_path / "matrix"
    inst.mkdir()
    loose = inst / "loose"
    loose.mkdir()
    (loose / "l.dds").write_bytes(b"loose-bytes")

    # case 4 winner + two real shadows (descending priority 10 > 8 > 7).
    # NOTE: a/t.dds is deliberately ABSENT here so the mid (p9) tombstone is reached FIRST.
    hi = _fixtures.build_tre(
        inst / "hi.tre",
        [("hi/h.dds", 5, 5)],
        version="0004",
    )
    # case 1: mid tombstones a/t.dds (tree length 0); case 4 first shadow of hi/h.dds
    mid = _fixtures.build_tre(
        inst / "mid.tre",
        [("a/t.dds", 0, 0), ("hi/h.dds", 5, 5)],
        version="0004",
    )
    # case 4 second shadow + case 2 winner win/w.dds (real) at p7
    s2 = _fixtures.build_tre(
        inst / "s2.tre",
        [("hi/h.dds", 5, 5), ("win/w.dds", 6, 6)],
        version="0004",
    )
    # case 1 real winner for a/t.dds (after the mid tombstone -> stays tombstoned)
    low = _fixtures.build_tre(
        inst / "low.tre",
        [("a/t.dds", 4, 4), ("win/w.dds", 0, 0)],  # case 2: lower tombstone AFTER winner -> no evict
        version="0004",
    )
    # case 3 toc-absent: length==0 entry AND offset==0 entry; plus a real toc winner
    toc = _fixtures.build_search_toc(
        inst / "idx.toc",
        [("abs/z.dds", 0, 0, 5), ("abs/q.dds", 7, 7, 0), ("real/r.dds", 8, 8, 200)],
        tree_files=("t.tre",),
    )
    # case 6 rejected: interior `..`
    rej = _fixtures.build_tre(inst / "rej.tre", [("bad/../bad/r.dds", 6, 6)], version="0004")
    # case 7 malformed: truncate
    brk = _fixtures.build_tre(inst / "brk.tre", [("brk/a.dds", 1, 1)], version="0004")
    brk.write_bytes(brk.read_bytes()[:18])

    cfg = inst / "client.cfg"
    _fixtures.write_synthetic_cfg(
        cfg,
        [
            ("path", 12, loose),
            ("tree", 10, hi),
            ("tree", 9, mid),   # win at? hi/h.dds first claimed by hi(p10); mid is shadow
            ("tree", 8, s2),
            ("tree", 7, low),
            ("toc", 6, toc),
            ("tree", 5, rej),
            ("tree", 4, brk),
        ],
        toc_tree_path=str(inst).replace("\\", "/") + "/",
    )
    return cfg


def test_parity_matrix(tmp_path):
    from tre_compare.cache import build_virtual_tree_cached, Cache

    cfg = _build_parity_matrix_install(tmp_path)
    scan = parse_shared_file(cfg)
    builder = build_virtual_tree(scan)
    cache = Cache(tmp_path / "c.sqlite")
    cached = build_virtual_tree_cached(scan, cache)
    _assert_tree_parity(cached, builder)

    # sanity: the matrix actually exercised the cases it claims
    assert "a/t.dds" in builder.tombstoned, "case 1: a/t.dds must be tombstoned"
    assert builder.rejected, "case 6: a rejected key must exist"
    assert builder.node_errors, "case 7: a node_error must exist"
    # case 4: 3-way multi-shadow -> winner has exactly 2 shadows IN ORDER
    h = builder.entries["hi/h.dds"]
    assert len(h.shadowed) == 2, "case 4: 3-way shadow -> 2 shadowed nodes"
    # case 2: win/w.dds stays a real winner despite a lower tombstone
    assert "win/w.dds" in builder.entries
    assert "win/w.dds" not in builder.tombstoned


# =======================================================================================
# hash memo (file_hash) round-trip (TRE-04 fast path)
# =======================================================================================
def test_hash_memo_roundtrip(tmp_path):
    from tre_compare.cache import Cache

    arc = _fixtures.build_tre(tmp_path / "a.tre", [("d/x.dds", 4, 4)], version="0004")
    node = _node_for(arc)
    cache = Cache(tmp_path / "c.sqlite")

    assert cache.get_file_hash(node, "d/x.dds") is None  # cold MISS
    cache.put_file_hash(node, "d/x.dds", "deadbeefcafef00d")
    assert cache.get_file_hash(node, "d/x.dds") == "deadbeefcafef00d"  # HIT


# =======================================================================================
# tre_file stored on toc rows (Pitfall 7 — drill-in without re-parsing a 193k-entry .toc)
# =======================================================================================
def test_tre_file_stored(tmp_path):
    from tre_compare.cache import Cache

    toc = _fixtures.build_search_toc(
        tmp_path / "idx.toc", [("p/p.dds", 10, 10, 64)], tree_files=("payload.tre",)
    )
    node = _node_for(toc, kind="toc")
    cache = Cache(tmp_path / "c.sqlite")
    rows = cache.archive_entries(node)
    row = next(r for r in rows if r["vpath"] == "p/p.dds")
    assert row["tre_file"] == "payload.tre"


# =======================================================================================
# concurrent MISS — two threads racing the same fresh archive, no PRIMARY KEY 500 (Sonnet)
# =======================================================================================
def test_concurrent_miss_no_integrity_error(tmp_path):
    from tre_compare.cache import Cache

    arc = _fixtures.build_tre(tmp_path / "a.tre", [("d/x.dds", 4, 4)], version="0004")
    node = _node_for(arc)
    cache = Cache(tmp_path / "c.sqlite")

    barrier = threading.Barrier(2)
    results: list = []
    errors: list = []

    def worker():
        try:
            barrier.wait()
            results.append(cache.archive_entries(node))
        except Exception as exc:  # noqa: BLE001 — we explicitly assert none escape
            errors.append(exc)

    threads = [threading.Thread(target=worker) for _ in range(2)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()

    assert not errors, f"concurrent MISS raised: {errors!r}"
    assert len(results) == 2
    assert all(any(r["vpath"] == "d/x.dds" for r in rows) for rows in results)
