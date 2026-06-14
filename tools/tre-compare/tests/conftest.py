"""Shared pytest fixtures for tre-compare (Phase-28 D-05/D-06).

Exposes the programmatic byte-built TRE/SearchTOC/COT2000 builders (``_fixtures.py``)
as pytest fixtures bound to ``tmp_path``. The ``synthetic_install`` fixture lays out a
temp dir with archives + an override dir + a synthetic ``client.cfg`` and returns the
cfg path, so a test can drive the FULL ``parse_shared_file → build_virtual_tree``
pipeline against genuine on-disk archives without touching any real install (D-06).
"""

from __future__ import annotations

from pathlib import Path

import pytest

import _fixtures


@pytest.fixture
def fixtures():
    """Expose the byte-builder module directly for tests that forge ad-hoc archives."""
    return _fixtures


@pytest.fixture
def synthetic_install(tmp_path: Path):
    """Lay out a portable synthetic install and return a dict of paths (incl. ``cfg``).

    Builds, under ``tmp_path``:
      * a HIGH-priority searchTree ``.tre`` carrying a length-0 TOMBSTONE for ``foo/x.dds``
      * a LOWER-priority searchTree ``.tre`` carrying a REAL ``foo/x.dds``
      * a HIGH-priority searchTOC ``.toc`` carrying a length-0/offset-0 ``foo/y.dds``
        (which must NOT shadow the lower real copy)
      * a LOWER-priority searchTOC ``.toc`` carrying a REAL ``foo/y.dds``
      * an empty override dir at the top priority
    then writes a live-modeled ``client.cfg`` wiring them in descending priority. The
    cfg LISTS the high tombstone nodes first; the merge must still tombstone ``foo/x.dds``
    globally and serve ``foo/y.dds`` from the lower real ``.toc``.
    """
    install = tmp_path / "install"
    install.mkdir()
    override = install / "override"
    override.mkdir()

    tomb_tre = _fixtures.build_tre(
        install / "high_tomb.tre", [("foo/x.dds", 0, 0)], version="0004"
    )
    real_tre = _fixtures.build_tre(
        install / "low_real.tre", [("foo/x.dds", 11, 11)], version="0004"
    )
    absent_toc = _fixtures.build_search_toc(
        install / "high_absent.toc",
        [("foo/y.dds", 0, 0, 0)],
        tree_files=("t.tre",),
    )
    real_toc = _fixtures.build_search_toc(
        install / "low_real.toc",
        [("foo/y.dds", 7, 7, 128)],
        tree_files=("t.tre",),
    )

    cfg = install / "client.cfg"
    # cfg-discovery order lists the HIGH tombstone/absent nodes first, with descending
    # priorities, exactly as a live cfg might. searchPath@10 > tree@9/8 > toc@5/4.
    _fixtures.write_synthetic_cfg(
        cfg,
        [
            ("path", 10, override),
            ("tree", 9, tomb_tre),
            ("tree", 8, real_tre),
            ("toc", 5, absent_toc),
            ("toc", 4, real_toc),
        ],
    )
    return {
        "cfg": cfg,
        "install": install,
        "override": override,
        "tomb_tre": tomb_tre,
        "real_tre": real_tre,
        "absent_toc": absent_toc,
        "real_toc": real_toc,
    }
