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

from tre_compare.scanner import parse_shared_file
from tre_compare.virtual_tree import build_virtual_tree

REAL_CFG = Path(r"D:/Code/swg-client-v2/stage/client.cfg")


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
