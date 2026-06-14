"""Scanner behavioral suite (Phase-28 Plan 04).

Drives ``tre_compare.scanner.parse_shared_file`` against synthetic ``[SharedFile]``
cfgs written by ``_fixtures.write_synthetic_cfg`` (or hand-written cfg text) to lock:

  * priority order (HIGHER searched FIRST — Pitfall 2)
  * engine-faithful same-priority CROSS-KIND order path→tree→toc (NOT cfg line order)
    (review finding #1 — REPLACES the removed engine-wrong ``test_stable_tie_order``)
  * same-kind same-priority cfg-order tie-break
  * repeated key names → distinct nodes (hand-parser vs configparser — Pitfall 4)
  * bare-priority legacy grammar (review finding #6)
  * quote strip + comment / scalar handling
  * cfg path is a parameter (D-08)
"""

from __future__ import annotations

from pathlib import Path

from tre_compare.scanner import parse_shared_file


def _write(path: Path, body: str) -> Path:
    path.write_text(body, encoding="utf-8")
    return path


def test_priority_order_higher_first(tmp_path) -> None:
    """path@10 sorts first; the toc@0 is LAST (higher priority number searched first)."""
    cfg = _write(
        tmp_path / "c.cfg",
        "[SharedFile]\n"
        '\tsearchPath_00_10="ov/"\n'
        '\tsearchTree_01_8="hi.tre"\n'
        '\tsearchTree_02_7="lo.tre"\n'
        '\tsearchTOC_03_3="a.toc"\n'
        '\tsearchTOC_04_0="b.toc"\n',
    )
    scan = parse_shared_file(str(cfg))
    assert scan.nodes[0].kind == "path" and scan.nodes[0].priority == 10
    assert scan.nodes[-1].kind == "toc" and scan.nodes[-1].priority == 0


def test_same_priority_kind_order_path_before_tree_before_toc(tmp_path) -> None:
    """At ONE priority, cfg lists toc → tree → path, but the scanner orders them
    path → tree → toc (engine registration order, NOT cfg line order). Finding #1."""
    cfg = _write(
        tmp_path / "c.cfg",
        "[SharedFile]\n"
        '\tsearchTOC_00_5="a.toc"\n'
        '\tsearchTree_01_5="b.tre"\n'
        '\tsearchPath_02_5="ov/"\n',
    )
    scan = parse_shared_file(str(cfg))
    kinds = [n.kind for n in scan.nodes]
    assert kinds == ["path", "tree", "toc"], kinds


def test_same_kind_same_priority_keeps_cfg_order(tmp_path) -> None:
    """Two searchTree keys at the same priority keep cfg discovery order (cfg_seq tie)."""
    cfg = _write(
        tmp_path / "c.cfg",
        "[SharedFile]\n"
        '\tsearchTree_00_5="first.tre"\n'
        '\tsearchTree_01_5="second.tre"\n',
    )
    scan = parse_shared_file(str(cfg))
    vals = [n.abspath for n in scan.nodes]
    assert vals == ["first.tre", "second.tre"], vals


def test_repeated_key_yields_two_nodes(tmp_path) -> None:
    """The SAME key NAME twice yields TWO nodes (proves hand-parser, not configparser)."""
    cfg = _write(
        tmp_path / "c.cfg",
        "[SharedFile]\n"
        '\tsearchTree_00_5="one.tre"\n'
        '\tsearchTree_00_5="two.tre"\n',
    )
    scan = parse_shared_file(str(cfg))
    assert len(scan.nodes) == 2, scan.nodes
    assert {n.abspath for n in scan.nodes} == {"one.tre", "two.tre"}


def test_bare_priority_key_parses(tmp_path) -> None:
    """A bare-priority key ``searchPath10`` parses to priority 10 + sentinel sku -1
    (legacy empty-skuText grammar, review finding #6)."""
    cfg = _write(
        tmp_path / "c.cfg",
        "[SharedFile]\n" '\tsearchPath10="ov/"\n',
    )
    scan = parse_shared_file(str(cfg))
    assert len(scan.nodes) == 1
    n = scan.nodes[0]
    assert n.kind == "path" and n.priority == 10 and n.sku == -1


def test_quote_strip_and_comments(tmp_path) -> None:
    """Quoted values stripped; ``#`` + non-section + non-``=`` lines ignored;
    maxSearchPriority / TOCTreePath captured as scalars."""
    cfg = _write(
        tmp_path / "c.cfg",
        "[Other]\n"
        '\tsearchTree_00_9="ignored.tre"\n'
        "[SharedFile]\n"
        "\t# a comment line\n"
        "\tmaxSearchPriority=12\n"
        '\tTOCTreePath="D:/x/"\n'
        '\tsearchTree_00_5="quoted.tre"\n'
        "\tnot-a-kv-line\n",
    )
    scan = parse_shared_file(str(cfg))
    assert scan.max_search_priority == 12
    assert scan.toc_tree_path == "D:/x/"
    assert [n.abspath for n in scan.nodes] == ["quoted.tre"]  # [Other] key ignored


def test_cfg_path_is_parameter(tmp_path) -> None:
    """Two distinct cfg files yield distinct results (D-08 — no hardcoded path)."""
    a = _write(tmp_path / "a.cfg", '[SharedFile]\n\tsearchTree_00_5="a.tre"\n')
    b = _write(tmp_path / "b.cfg", '[SharedFile]\n\tsearchTOC_00_3="b.toc"\n')
    sa = parse_shared_file(str(a))
    sb = parse_shared_file(str(b))
    assert [n.abspath for n in sa.nodes] == ["a.tre"]
    assert [n.abspath for n in sb.nodes] == ["b.toc"]
