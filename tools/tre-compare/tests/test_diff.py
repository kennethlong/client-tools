"""Phase-29 diff-engine tests over synthetic install pairs (TRE-02/03/04).

Drives the FULL ``parse_shared_file -> build_virtual_tree`` pipeline per side from
``build_install_pair`` (tests/_fixtures.py), then exercises the pure ``tre_compare.diff``
engine: set-level diff (TRE-02), file-level tri-state + the tombstoned/rejected/node_error
QUALIFIER (TRE-03), and drill-in + content hash with symmetric TREE/TOC payload resolution
and never-raise degrade (TRE-04).

NO test references ``crc`` as a change signal (D-06: crc is a path-CRC, not a content hash).
"""

from __future__ import annotations

import _fixtures
import pytest

from tre_compare.diff import (
    DriveHashResult,
    diff_archive_set,
    diff_virtual_trees,
    drill_in,
)
from tre_compare.scanner import parse_shared_file
from tre_compare.virtual_tree import build_virtual_tree


@pytest.fixture
def pair(tmp_path):
    meta = _fixtures.build_install_pair(tmp_path)
    meta["scanL"] = parse_shared_file(meta["left_cfg"])
    meta["scanR"] = parse_shared_file(meta["right_cfg"])
    meta["vtL"] = build_virtual_tree(meta["scanL"])
    meta["vtR"] = build_virtual_tree(meta["scanR"])
    return meta


# --------------------------------------------------------------------------------------
# SET-level diff (TRE-02)
# --------------------------------------------------------------------------------------
def _set_rows_by_basename(rows):
    out = {}
    for r in rows:
        out.setdefault(r["basename"], []).append(r)
    return out


def test_set(pair):
    rows = diff_archive_set(pair["scanL"], pair["scanR"])
    by = _set_rows_by_basename(rows)

    # leftonly.tre only on left -> removed
    assert any(r["status"] == "removed" for r in by[pair["leftonly_archive"]])
    # rightonly.tre only on right -> added
    assert any(r["status"] == "added" for r in by[pair["rightonly_archive"]])
    # common.tre differs (entry count + size) -> changed with deltas surfaced
    common = [r for r in by[pair["common_archive"]] if r["kind"] == "tree"][0]
    assert common["status"] == "changed"
    assert common["entry_count_delta"] != 0 or common["size_delta"] != 0
    assert "version_left" in common and "version_right" in common


def test_set_collision(pair):
    rows = diff_archive_set(pair["scanL"], pair["scanR"])
    collide = [r for r in rows if r["basename"] == "collide.tre" or r["basename"] == "collide.toc"]
    kinds = {r["kind"] for r in collide}
    # the tree AND the toc sharing basename "collide" must BOTH appear, keyed by (basename, kind)
    assert "tree" in kinds and "toc" in kinds
    # neither overwrote the other: at least two distinct rows
    assert len([r for r in rows if r["basename"].startswith("collide")]) >= 2


def test_set_corrupt(pair):
    rows = diff_archive_set(pair["scanL"], pair["scanR"])
    broken = [r for r in rows if r["basename"] == pair["broken_archive"]]
    assert broken, "broken archive must still produce a row"
    assert broken[0]["status"] == "fault"
    assert broken[0].get("fault_left")
    # the OTHER archives still diffed (call did not abort)
    assert any(r["status"] in ("added", "removed", "changed", "identical") for r in rows)


# --------------------------------------------------------------------------------------
# FILE-level tri-state (TRE-03)
# --------------------------------------------------------------------------------------
def _file_rows_by_path(result):
    return {r["path"]: r for r in result["rows"]}


def test_files(pair):
    result = diff_virtual_trees(pair["vtL"], pair["vtR"])
    rows = _file_rows_by_path(result)
    assert rows[pair["added"]]["status"] == "added"
    assert rows[pair["removed"]]["status"] == "removed"
    assert rows[pair["changed"]]["status"] == "changed"
    assert rows[pair["same"]]["status"] == "identical-by-metadata"
    # false-identical reads identical-by-metadata at the bulk (metadata) level
    assert rows[pair["false_identical"]]["status"] == "identical-by-metadata"


# --------------------------------------------------------------------------------------
# QUALIFIER cases (TRE-03 / P1)
# --------------------------------------------------------------------------------------
def test_qualifier_tombstoned(pair):
    result = diff_virtual_trees(pair["vtL"], pair["vtR"])
    rows = _file_rows_by_path(result)
    # foo/x.dds tombstoned on LEFT, real on RIGHT -> qualifier tombstoned_left, NOT bare added
    left_tomb = rows[pair["tombstone_left"]]
    assert "tombstoned_left" in left_tomb.get("qualifier", [])
    # tombstoned-both-sides path must NOT vanish; carries both qualifiers
    both = rows.get(pair["tombstone_both"])
    assert both is not None, "tombstoned-both-sides path must not silently vanish"
    assert "tombstoned_left" in both.get("qualifier", [])
    assert "tombstoned_right" in both.get("qualifier", [])


def test_qualifier_rejected(pair):
    result = diff_virtual_trees(pair["vtL"], pair["vtR"])
    rows = _file_rows_by_path(result)
    # bad/rej.dds present on LEFT, rejected on RIGHT (safe_virtual_key->None) -> rejected_right
    rej = rows[pair["rejected_right"]]
    assert "rejected_right" in rej.get("qualifier", [])


def test_qualifier_node_error(pair):
    result = diff_virtual_trees(pair["vtL"], pair["vtR"])
    # the LEFT broken.tre parse-failed (in vtL.node_errors); the summary must surface it
    assert result["summary"]["left"]["node_errors"] >= 1


def test_summary(pair):
    result = diff_virtual_trees(pair["vtL"], pair["vtR"])
    summary = result["summary"]
    for side in ("left", "right"):
        assert "node_errors" in summary[side]
        assert "rejected" in summary[side]
        assert "tombstoned" in summary[side]
    # per-status counts present
    assert "added" in summary["status_counts"]
    assert summary["left"]["tombstoned"] >= 1
    assert summary["right"]["rejected"] >= 1


# --------------------------------------------------------------------------------------
# DRILL-IN + HASH (TRE-04)
# --------------------------------------------------------------------------------------
def test_drill(pair):
    # data/false.dds is served by common.tre on both sides; shadowed copies may be empty,
    # so assert on the winner metadata + ok status.
    res = drill_in(pair["vtL"], pair["vtR"], pair["false_identical"], pair["scanL"], pair["scanR"])
    assert res.status == "ok"
    assert res.winner_left is not None and res.winner_right is not None
    assert res.winning_archive_left == "common.tre"
    assert hasattr(res, "shadowed_left")


def test_drill_not_found(pair):
    missing = drill_in(pair["vtL"], pair["vtR"], "no/such/file.dds", pair["scanL"], pair["scanR"])
    assert missing.status == "not_found"
    rejected = drill_in(pair["vtL"], pair["vtR"], "bad/../escape.dds", pair["scanL"], pair["scanR"])
    assert rejected.status == "rejected"


def test_hash(pair):
    # bulk says identical-by-metadata; drill-in proves content-changed (different bytes).
    bulk = diff_virtual_trees(pair["vtL"], pair["vtR"])
    rows = _file_rows_by_path(bulk)
    assert rows[pair["false_identical"]]["status"] == "identical-by-metadata"

    res = drill_in(pair["vtL"], pair["vtR"], pair["false_identical"], pair["scanL"], pair["scanR"])
    assert res.status == "ok"
    assert isinstance(res.hash_left, DriveHashResult)
    assert isinstance(res.hash_right, DriveHashResult)
    assert res.hash_left.hexdigest is not None
    assert res.hash_right.hexdigest is not None
    assert res.hash_left.hexdigest != res.hash_right.hexdigest
    assert res.verdict == "content-changed"


def test_toc_payload(pair):
    # payload/p.dds is served on the RIGHT via a SearchTOC -> resolves payload.tre via toc_tree_path.
    res = drill_in(pair["vtL"], pair["vtR"], pair["toc_payload"], pair["scanL"], pair["scanR"])
    assert res.status == "ok"
    assert res.hash_right.hexdigest is not None
    assert res.hash_right.error is None


def test_tree_payload_asymmetry(pair):
    # ./asym//z.dds stored on disk; reachable under canonical asym/z.dds; must hash (no KeyError).
    res = drill_in(pair["vtL"], pair["vtR"], pair["asym"], pair["scanL"], pair["scanR"])
    assert res.status == "ok"
    assert res.hash_left.hexdigest is not None
    assert res.hash_left.error is None
    assert res.hash_right.hexdigest is not None


def test_keyerror_degrade(pair, monkeypatch):
    # Force read_tre_payload to raise a BARE KeyError (a name miss not in _NODE_FAULTS);
    # hash_virtual_file must degrade to a DriveHashResult error, never raise.
    from tre_compare import diff as diff_mod

    def _boom(*a, **k):
        raise KeyError("forced-miss")

    monkeypatch.setattr(diff_mod, "read_tre_payload", _boom)
    res = drill_in(pair["vtL"], pair["vtR"], pair["same"], pair["scanL"], pair["scanR"])
    assert res.status == "ok"
    assert res.hash_left.hexdigest is None
    assert res.hash_left.error  # populated error string
