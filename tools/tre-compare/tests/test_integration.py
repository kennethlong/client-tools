"""Env/marker-gated real-install integration test (Phase-28 D-07).

EXACTLY ONE integration test against the real ``stage/client.cfg`` (D-07: "exactly one
optional integration test"). Gated behind ``TRE_COMPARE_INTEGRATION`` + the
``integration`` marker so the default ``uv run pytest -m "not integration"`` run stays
machine-independent (CI/extraction-safe). Skips cleanly (never fails) when the env var
is unset or the real cfg / archives / override dir are absent (D-08 — the cfg uses
machine-specific absolute paths).

Run on this PowerShell machine:
    $env:TRE_COMPARE_INTEGRATION=1; uv run pytest -m integration -q
POSIX equivalent:
    TRE_COMPARE_INTEGRATION=1 uv run pytest -m integration -q
"""

from __future__ import annotations

import os
from pathlib import Path

import pytest

import tre_compare.cache as cache_mod
from tre_compare.cache import Cache, build_virtual_tree_cached
from tre_compare.diff import diff_archive_set, diff_virtual_trees
from tre_compare.scanner import parse_shared_file
from tre_compare.virtual_tree import build_virtual_tree

REAL_CFG = Path(r"D:/Code/swg-client-v2/stage/client.cfg")

_VALID_FILE_STATUSES = {"added", "removed", "changed", "identical-by-metadata", "tombstoned"}


@pytest.mark.integration
@pytest.mark.skipif(
    not os.environ.get("TRE_COMPARE_INTEGRATION"),
    reason="set TRE_COMPARE_INTEGRATION=1 to run the real-install scan",
)
def test_real_client_cfg_scan_and_merge() -> None:
    if not REAL_CFG.exists():
        pytest.skip(f"real cfg absent: {REAL_CFG}")
    scan = parse_shared_file(str(REAL_CFG))
    # SC#3 precedence proof: searchPath@10 sorts before searchTOC@0
    assert scan.nodes[0].priority == 10 and scan.nodes[0].kind == "path"
    assert scan.nodes[-1].kind == "toc" and scan.nodes[-1].priority == 0
    # Build the merged tree; tolerate missing real archives (skip if the SWGSource
    # install dir is absent — the cfg points at machine-specific abs paths, D-08).
    if not all(Path(n.abspath).exists() for n in scan.nodes):
        pytest.skip("real archives/override dir not present on this machine")
    vt = build_virtual_tree(scan)
    assert len(vt.entries) > 0
    # SC#3 OVERRIDE SHADOWING PROOF (review finding #4): the priority-10 searchPath override
    # must actually WIN at least one path. Only assert when the override dir exists AND is
    # non-empty (an empty override is a valid machine state, not a test failure):
    override_node = next(n for n in scan.nodes if n.kind == "path" and n.priority == 10)
    override_dir = Path(override_node.abspath)
    override_files = [f for f in override_dir.rglob("*") if f.is_file()] if override_dir.exists() else []
    if override_files:
        path_winners = [e for e in vt.entries.values() if e.winner_node.kind == "path"]
        assert path_winners, "priority-10 override dir has files but none won the merge"
        assert any(e.winner_node.priority == 10 for e in path_winners)
    # SC#3 SCOPE NOTE (user decision 2026-06-14): this integration test proves precedence +
    # override shadowing on the REAL cfg. Tombstone behavior is proven LITERALLY (not narrowed)
    # by test_tombstone_end_to_end_via_cfg in test_virtual_tree.py — it drives the SAME
    # parse_shared_file->build_virtual_tree pipeline against purpose-built genuine on-disk TRE
    # archives wired through a live-modeled client.cfg. A production install cannot be
    # guaranteed to contain a length-0 tombstone entry, and forging one INTO the user's
    # installed .tre would violate D-03 read-only — so the end-to-end tombstone proof uses a
    # test-authored (but genuine) .tre, which is NOT a D-03 violation. No ROADMAP SC#3
    # amendment is required.


# =======================================================================================
# SC#4 north-star: env-gated TWO-install real-pair compare + a MEASURED cache-HIT second
# compare (P5 — the HIT is proven by a parse-skip spy on iter_node_entries, not row-equality).
# =======================================================================================
def _resolve_pair_cfgs():
    """Return (left_cfg, right_cfg) Paths from the two env vars, or None to skip."""
    left = os.environ.get("TRE_COMPARE_LEFT_CFG")
    right = os.environ.get("TRE_COMPARE_RIGHT_CFG")
    if not left or not right:
        return None
    lp, rp = Path(left), Path(right)
    if not lp.is_file() or not rp.is_file():
        return None
    return lp, rp


@pytest.mark.integration
@pytest.mark.skipif(
    not os.environ.get("TRE_COMPARE_INTEGRATION"),
    reason="set TRE_COMPARE_INTEGRATION=1 to run the real-install compare",
)
def test_real_install_pair_compare_and_cache_hit(tmp_path: Path, monkeypatch) -> None:
    """SC#4: diff the real SWGSource-vs-whitengold pair; the SECOND compare is a measured HIT.

    Drive both cfgs through a tmp_path-backed Cache:
      1. set-level diff returns rows;
      2. file-level diff_virtual_trees returns >0 lean rows with valid tri-state statuses;
      3. a SECOND build_virtual_tree_cached over the SAME cache PROVES the parse-skip — the
         iter_node_entries spy is hit on the first compare's tree/toc MISSes but ONLY on the
         loose ``path`` nodes (never cached) on the second compare (P5, not row-equality).
    Skips cleanly when either env cfg is unset/absent (the default
    ``uv run pytest -m "not integration"`` run stays machine-independent).
    """
    pair = _resolve_pair_cfgs()
    if pair is None:
        pytest.skip(
            "set TRE_COMPARE_LEFT_CFG + TRE_COMPARE_RIGHT_CFG to two real client.cfg files"
        )
    left_cfg, right_cfg = pair

    scanL = parse_shared_file(str(left_cfg))
    scanR = parse_shared_file(str(right_cfg))
    # tolerate machine-specific abs paths that don't resolve on this box (D-08)
    if not all(Path(n.abspath).exists() for n in scanL.nodes):
        pytest.skip("left install archives not present on this machine")
    if not all(Path(n.abspath).exists() for n in scanR.nodes):
        pytest.skip("right install archives not present on this machine")

    # SET-level diff returns rows.
    set_rows = diff_archive_set(scanL, scanR)
    assert isinstance(set_rows, list) and set_rows

    cache = Cache(tmp_path / "cache" / "pair.sqlite")
    try:
        # ---- spy on the module-level iter_node_entries to MEASURE the parse-skip ----
        real_iter = cache_mod.iter_node_entries
        calls = {"n": 0}

        def _spy(node, errs):
            calls["n"] += 1
            return real_iter(node, errs)

        monkeypatch.setattr(cache_mod, "iter_node_entries", _spy)

        # FIRST compare — every tree/toc archive MISSes -> parses (spy fires).
        vtL1 = build_virtual_tree_cached(scanL, cache)
        vtR1 = build_virtual_tree_cached(scanR, cache)
        first_calls = calls["n"]
        n_archives = sum(1 for n in (*scanL.nodes, *scanR.nodes) if n.kind in ("tree", "toc"))
        assert first_calls >= n_archives, "first compare must parse every tree/toc archive"

        file_diff = diff_virtual_trees(vtL1, vtR1)
        assert file_diff["rows"], "real pair must produce >0 file rows"
        for row in file_diff["rows"]:
            assert row["status"] in _VALID_FILE_STATUSES

        # SECOND compare over the SAME cache — tree/toc HIT (no re-parse); only loose ``path``
        # nodes (never cached) may re-fire the spy.
        calls["n"] = 0
        build_virtual_tree_cached(scanL, cache)
        build_virtual_tree_cached(scanR, cache)
        second_calls = calls["n"]
        n_path_nodes = sum(1 for n in (*scanL.nodes, *scanR.nodes) if n.kind == "path")
        assert second_calls <= n_path_nodes, (
            f"second compare must skip every cached tree/toc parse "
            f"(spy fired {second_calls}x, only {n_path_nodes} loose path nodes allowed)"
        )
        assert second_calls < first_calls, "the second compare must be a measured cache HIT"
    finally:
        cache.close()
