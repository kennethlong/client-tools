"""Playground mirror tests (temp dir + optional live fixture)."""

from __future__ import annotations

import pytest
from pathlib import Path

from swg_pipeline.sample_tre import (
    ensure_playground_copy,
    playground_is_current,
    sample_tre_dir,
)


def test_ensure_playground_copy_temp(tmp_path: Path):
    src = tmp_path / "ref"
    src.mkdir()
    (src / "a.tre").write_bytes(b"TREE")
    (src / "b.toc").write_bytes(b" COT2000")
    import os
    os.environ["SWG_SAMPLE_TRE_DIR"] = str(src)
    try:
        pg = ensure_playground_copy()
        assert pg == src / "playground"
        assert (pg / "a.tre").read_bytes() == b"TREE"
        assert playground_is_current()
        # idempotent
        pg2 = ensure_playground_copy()
        assert pg2 == pg
    finally:
        os.environ.pop("SWG_SAMPLE_TRE_DIR", None)


def test_live_playground_status():
    root = sample_tre_dir()
    if not root:
        pytest.skip("no sample TRE dir")
    pg = root / "playground"
    if not pg.is_dir():
        pytest.skip("playground not initialized — run: python -m swg_pipeline.tre_playground init")
    assert playground_is_current()