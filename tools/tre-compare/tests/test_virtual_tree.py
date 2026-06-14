"""Virtual-tree merge behavioral suite (Phase-28 Plan 04).

Locks every SC#3 merge mechanic against the real
``tre_compare.virtual_tree.build_virtual_tree``:

  * fix_up_file_name canonicalization (engine-faithful — keeps interior ``..``)
  * safe_virtual_key SPLIT hardening (rejects traversal/drive/UNC/empty) — finding #3
  * first-hit-wins + shadowed = REAL copies only (finding #4)
  * PER-NODE-TYPE tombstone: tree length-0 = GLOBAL remove; toc length-0/offset-0 =
    skip-only (never shadows) — Pitfall 1
  * the INVERSE case: a LOWER tree tombstone does NOT un-claim a higher winner (#4)
  * same-priority CROSS-KIND tree-wins-over-toc (#1)
  * searchPath override shadowing + missing/empty-dir edges (#7)
  * path-traversal reject → ``rejected`` (T-28-03-01)
  * malformed node + .tre & .toc oversized-header preflight → ``node_errors``
    (T-28-03-02/04, findings #6/#2a/#8)
  * the cfg-driven END-TO-END tombstone proof through parse_shared_file → build_virtual_tree
    over PURPOSE-BUILT genuine on-disk archives (user decision 2026-06-14 — SC#3 literal)
"""

from __future__ import annotations

import struct
from pathlib import Path

from tre_compare.scanner import SearchNode, ScanResult, parse_shared_file
from tre_compare.virtual_tree import (
    build_virtual_tree,
    fix_up_file_name,
    safe_virtual_key,
)

import _fixtures


# ----------------------------------------------------------------------------------------
# Direct-merge helpers: build genuine on-disk archives + a hand-ordered SearchNode list.
# (build_virtual_tree consumes nodes already in engine-faithful order, so we order them
#  by descending priority + KIND_RANK here, mirroring the scanner's sort.)
# ----------------------------------------------------------------------------------------
_KIND_RANK = {"path": 0, "tree": 1, "toc": 2}


def _node(kind: str, priority: int, abspath: Path, seq: int) -> SearchNode:
    return SearchNode(
        kind=kind,
        priority=priority,
        abspath=str(abspath),
        raw_key=f"search{kind}_{seq:02d}_{priority}",
        sku=seq,
        cfg_seq=seq,
    )


def _scan(*nodes: SearchNode) -> ScanResult:
    ordered = sorted(nodes, key=lambda n: (-n.priority, _KIND_RANK[n.kind], n.cfg_seq))
    return ScanResult(nodes=list(ordered))


# ======================================================================================
# fix_up_file_name + safe_virtual_key (the split)
# ======================================================================================
def test_fix_up_file_name() -> None:
    assert fix_up_file_name("\\foo\\\\Bar/baz.dds") == "foo/bar/baz.dds"
    assert fix_up_file_name("./a/b") == "a/b"
    assert fix_up_file_name("../x") == "x"
    assert fix_up_file_name("MiXeD/CASE") == "mixed/case"
    assert fix_up_file_name("a//b///c") == "a/b/c"
    # engine-faithful: fix_up_file_name does NOT reject interior `..`
    assert fix_up_file_name("a/../../x") == "a/../../x"


def test_safe_virtual_key_rejects_traversal() -> None:
    # the SPLIT proof: rejection lives on the wrapper, not the engine-named function
    assert safe_virtual_key("a/../../x") is None
    assert safe_virtual_key("c:/x") is None
    assert safe_virtual_key("//host/x") is None
    assert safe_virtual_key("") is None
    assert safe_virtual_key("foo/bar.dds") == "foo/bar.dds"


# ======================================================================================
# first-hit-wins + shadowed
# ======================================================================================
def test_first_hit_wins(tmp_path) -> None:
    hi = _fixtures.build_tre(tmp_path / "hi.tre", [("foo/a.dds", 10, 10)])
    lo = _fixtures.build_tre(tmp_path / "lo.tre", [("foo/a.dds", 99, 99)])
    n_hi = _node("tree", 8, hi, 0)
    n_lo = _node("tree", 5, lo, 1)
    vt = build_virtual_tree(_scan(n_hi, n_lo))
    entry = vt.entries["foo/a.dds"]
    assert entry.winner_node.priority == 8
    assert entry.length == 10
    assert n_lo in entry.shadowed


# ======================================================================================
# PER-NODE-TYPE tombstone — the #1 correctness nuance
# ======================================================================================
def test_searchtree_length0_tombstone_removes_globally(tmp_path) -> None:
    tomb = _fixtures.build_tre(tmp_path / "tomb.tre", [("foo/x.dds", 0, 0)])
    real = _fixtures.build_tre(tmp_path / "real.tre", [("foo/x.dds", 11, 11)])
    n_tomb = _node("tree", 9, tomb, 0)
    n_real = _node("tree", 5, real, 1)
    vt = build_virtual_tree(_scan(n_tomb, n_real))
    assert "foo/x.dds" not in vt.entries
    assert "foo/x.dds" in vt.tombstoned


def test_searchtree_lower_priority_length0_does_not_tombstone_winner(tmp_path) -> None:
    """INVERSE/NEGATIVE case (#4): a HIGH real .tre + a LOWER length-0 .tre — the winner
    REMAINS, the path is NOT tombstoned, and the lower length-0 node is NOT in shadowed
    (shadowed = REAL copies only)."""
    real = _fixtures.build_tre(tmp_path / "real.tre", [("foo/x.dds", 11, 11)])
    tomb = _fixtures.build_tre(tmp_path / "tomb.tre", [("foo/x.dds", 0, 0)])
    n_real = _node("tree", 9, real, 0)
    n_tomb = _node("tree", 5, tomb, 1)
    vt = build_virtual_tree(_scan(n_real, n_tomb))
    assert "foo/x.dds" in vt.entries
    entry = vt.entries["foo/x.dds"]
    assert entry.winner_node.priority == 9
    assert "foo/x.dds" not in vt.tombstoned
    assert n_tomb not in entry.shadowed


def test_searchtoc_length0_does_NOT_shadow(tmp_path) -> None:
    """A high .toc with a length-0 (and one with offset-0) entry must NOT shadow a lower
    real copy — it is absent-here-keep-searching (Pitfall 1)."""
    absent_len = _fixtures.build_search_toc(
        tmp_path / "absent_len.toc", [("foo/y.dds", 0, 0, 64)], tree_files=("t.tre",)
    )
    absent_off = _fixtures.build_search_toc(
        tmp_path / "absent_off.toc", [("foo/y.dds", 5, 5, 0)], tree_files=("t.tre",)
    )
    real = _fixtures.build_search_toc(
        tmp_path / "real.toc", [("foo/y.dds", 7, 7, 128)], tree_files=("t.tre",)
    )
    n_alen = _node("toc", 9, absent_len, 0)
    n_aoff = _node("toc", 8, absent_off, 1)
    n_real = _node("toc", 4, real, 2)
    vt = build_virtual_tree(_scan(n_alen, n_aoff, n_real))
    assert "foo/y.dds" in vt.entries
    assert vt.entries["foo/y.dds"].winner_node.priority == 4
    assert "foo/y.dds" not in vt.tombstoned


# ======================================================================================
# same-priority cross-kind: tree wins over toc (the kind_rank behavioral proof)
# ======================================================================================
def test_same_priority_cross_kind_tree_wins_over_toc(tmp_path) -> None:
    toc = _fixtures.build_search_toc(
        tmp_path / "k.toc", [("foo/k.dds", 5, 5, 16)], tree_files=("t.tre",)
    )
    tre = _fixtures.build_tre(tmp_path / "k.tre", [("foo/k.dds", 9, 9)])
    # cfg lists the TOC first; tree must still win at the same priority.
    n_toc = _node("toc", 5, toc, 0)
    n_tre = _node("tree", 5, tre, 1)
    vt = build_virtual_tree(_scan(n_toc, n_tre))
    assert vt.entries["foo/k.dds"].winner_node.kind == "tree"


# ======================================================================================
# searchPath override + missing/empty-dir edges
# ======================================================================================
def test_searchpath_override_shadows_lower(tmp_path) -> None:
    ov = tmp_path / "override"
    (ov / "foo").mkdir(parents=True)
    (ov / "foo" / "z.dds").write_bytes(b"loose-bytes")
    tre = _fixtures.build_tre(tmp_path / "low.tre", [("foo/z.dds", 999, 999)])
    n_path = _node("path", 10, ov, 0)
    n_tree = _node("tree", 5, tre, 1)
    vt = build_virtual_tree(_scan(n_path, n_tree))
    entry = vt.entries["foo/z.dds"]
    assert entry.winner_node.kind == "path"
    assert entry.length == len(b"loose-bytes")  # loose size, not the tre's 999


def test_searchpath_missing_dir_yields_nothing(tmp_path) -> None:
    missing = tmp_path / "does-not-exist"
    tre = _fixtures.build_tre(tmp_path / "ok.tre", [("foo/q.dds", 3, 3)])
    n_path = _node("path", 10, missing, 0)
    n_tree = _node("tree", 5, tre, 1)
    vt = build_virtual_tree(_scan(n_path, n_tree))
    # missing dir contributes nothing and is NOT a node_error; valid node still merges
    assert "foo/q.dds" in vt.entries
    assert all("does-not-exist" not in msg for _, msg in vt.node_errors)


def test_searchpath_empty_dir(tmp_path) -> None:
    empty = tmp_path / "empty"
    empty.mkdir()
    tre = _fixtures.build_tre(tmp_path / "ok.tre", [("foo/q.dds", 3, 3)])
    vt = build_virtual_tree(_scan(_node("path", 10, empty, 0), _node("tree", 5, tre, 1)))
    assert "foo/q.dds" in vt.entries
    # the empty path node contributes zero entries; no path winner exists
    assert all(e.winner_node.kind != "path" for e in vt.entries.values())


# ======================================================================================
# path-traversal reject (T-28-03-01)
# ======================================================================================
def test_path_traversal_rejected(tmp_path) -> None:
    bad = _fixtures.build_tre(
        tmp_path / "bad.tre",
        [("a/../../x.dds", 5, 5), ("c:/abs.dds", 5, 5), ("foo/ok.dds", 5, 5)],
    )
    vt = build_virtual_tree(_scan(_node("tree", 5, bad, 0)))
    assert "foo/ok.dds" in vt.entries
    # hostile keys are not in the merged tree and ARE recorded in rejected
    keys = set(vt.entries)
    assert not any(".." in k for k in keys)
    assert not any(k.startswith("c:") for k in keys)
    assert any(".." in r for r in vt.rejected)
    assert any(r.lower().startswith("c:") for r in vt.rejected)


# ======================================================================================
# malformed node + .tre/.toc oversized-header preflight → node_errors (T-28-03-02/04)
# ======================================================================================
def test_malformed_archive_node_skipped(tmp_path) -> None:
    garbage = tmp_path / "garbage.tre"
    garbage.write_bytes(b"this is not a tre archive at all, just noise bytes")
    good = _fixtures.build_tre(tmp_path / "good.tre", [("foo/g.dds", 4, 4)])
    vt = build_virtual_tree(_scan(_node("tree", 9, garbage, 0), _node("tree", 5, good, 1)))
    assert "foo/g.dds" in vt.entries  # the valid node still merges
    bad_node = _node("tree", 9, garbage, 0)
    assert any(n.abspath == bad_node.abspath and msg for n, msg in vt.node_errors)


def test_bounds_preflight_rejects_oversized_header(tmp_path) -> None:
    """A .tre header declaring a size_of_name_block past EOF is skipped by the preflight,
    recorded in node_errors, and read_tre_entries is never reached (no OOM)."""
    p = _fixtures.build_tre(tmp_path / "ovh.tre", [("foo/a.dds", 5, 5)])
    data = bytearray(p.read_bytes())
    # STANDARD_HEADER_FMT = "<4s4s7I": uncompSizeOfNameBlock... sizeOfNameBlock is the
    # 6th uint (index 5) after the two 4s tokens → byte offset 8 + 5*4 = 28.
    struct.pack_into("<I", data, 28, 50_000_000)  # forge oversized sizeOfNameBlock
    p.write_bytes(bytes(data))
    good = _fixtures.build_tre(tmp_path / "good.tre", [("foo/g.dds", 4, 4)])
    vt = build_virtual_tree(_scan(_node("tree", 9, p, 0), _node("tree", 5, good, 1)))
    assert "foo/g.dds" in vt.entries
    assert any(p.name in n.abspath and "bounds preflight" in msg for n, msg in vt.node_errors)


def test_toc_bounds_preflight_rejects_oversized_header(tmp_path) -> None:
    """A SearchTOC header declaring a size_of_name_block past EOF is skipped by the .toc
    preflight, recorded in node_errors, and the typed reader is never reached."""
    p = _fixtures.build_search_toc(
        tmp_path / "ovh.toc", [("foo/a.dds", 5, 5)], tree_files=("a.tre",)
    )
    data = bytearray(p.read_bytes())
    # SEARCH_TOC_HEADER_FMT = "<4s4sBBBB6I": after 8 bytes (tokens) + 4 bytes (4×u8) the
    # six u32s start at offset 12: numberOfFiles@12, sizeOfTOC@16, sizeOfNameBlock@20.
    struct.pack_into("<I", data, 20, 50_000_000)  # forge oversized sizeOfNameBlock
    p.write_bytes(bytes(data))
    # a toc entry needs a non-zero offset to be a REAL (non-absent) entry
    good = _fixtures.build_search_toc(
        tmp_path / "good.toc", [("foo/g.dds", 4, 4, 64)], tree_files=("a.tre",)
    )
    vt = build_virtual_tree(_scan(_node("toc", 9, p, 0), _node("toc", 5, good, 1)))
    assert "foo/g.dds" in vt.entries
    assert any(p.name in n.abspath and "bounds preflight" in msg for n, msg in vt.node_errors)


# ======================================================================================
# cfg-driven END-TO-END tombstone proof (USER DECISION 2026-06-14 — the literal SC#3)
# ======================================================================================
def test_tombstone_end_to_end_via_cfg(synthetic_install) -> None:
    """Drive the FULL parse_shared_file → build_virtual_tree pipeline against PURPOSE-BUILT
    genuine on-disk TRE/TOC archives wired through a live-modeled client.cfg.

    Proves SC#3 LITERALLY: a high-priority .tre length-0 entry tombstones foo/x.dds
    GLOBALLY (even though a lower .tre carries a real copy), AND a high-priority .toc
    length-0/offset-0 entry does NOT shadow the lower real foo/y.dds."""
    cfg = synthetic_install["cfg"]
    scan = parse_shared_file(str(cfg))
    vt = build_virtual_tree(scan)

    # TREE tombstone end-to-end: globally removed
    assert "foo/x.dds" not in vt.entries
    assert "foo/x.dds" in vt.tombstoned

    # TOC no-shadow end-to-end: served from the LOWER real .toc node
    assert "foo/y.dds" in vt.entries
    winner = vt.entries["foo/y.dds"].winner_node
    assert winner.kind == "toc"
    assert winner.priority == 4  # the lower real .toc, not the high length-0/offset-0 one


def test_toc_no_shadow_end_to_end_via_cfg(synthetic_install) -> None:
    """Sibling of the combined end-to-end test, isolating the TOC-no-shadow assertion so
    the grep gate and a focused failure both resolve cleanly."""
    scan = parse_shared_file(str(synthetic_install["cfg"]))
    vt = build_virtual_tree(scan)
    assert "foo/y.dds" in vt.entries
    assert vt.entries["foo/y.dds"].winner_node.priority == 4
