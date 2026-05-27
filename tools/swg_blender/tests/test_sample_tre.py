"""Tests against D:/Sample-TRE-Files when present."""

from __future__ import annotations

import pytest

from swg_pipeline.sample_tre import master_toc, sample_tre_dir, smallest_tre, tre_files
from swg_pipeline.tre_reader import (
    parse_cot2000,
    read_cot2000_entries,
    read_cot2000_header,
    read_tre_entries,
    read_tre_header,
)


@pytest.fixture(scope="module")
def sample_dir():
    d = sample_tre_dir()
    if not d:
        pytest.skip("SWG_SAMPLE_TRE_DIR / D:\\Sample-TRE-Files not found")
    return d


def test_sample_dir_has_tre_and_toc(sample_dir):
    assert len(tre_files()) >= 40
    assert master_toc() is not None


def test_cot2000_header(sample_dir):
    h = read_cot2000_header(master_toc())
    assert h.number_of_files == 213_086
    assert h.number_of_tree_files == 45
    assert len(h.tre_files) == 45
    assert h.name_block_offset + h.size_of_name_block == h.path.stat().st_size


def test_cot2000_first_entry(sample_dir):
    e = read_cot2000_entries(master_toc())[0]
    assert e.path == "appearance/animation/all_b_emt_laugh_pointing.ans"
    assert e.tre_file.startswith("SwgRestoration_")
    assert e.tree_index == 26


def test_cot2000_summary(sample_dir):
    info = parse_cot2000(master_toc())
    assert len(info["tre_files"]) == 45
    assert info["indexed_path_count"] == 213_086
    assert info["extension_counts"].get("msh", 0) > 10_000


def test_smallest_tre_header(sample_dir):
    tre = smallest_tre()
    assert tre is not None
    header = read_tre_header(tre)
    assert header.version == "6000"
    assert header.number_of_files > 0


def test_smallest_tre_entries_have_paths(sample_dir):
    tre = smallest_tre()
    entries = read_tre_entries(tre)
    assert len(entries) == read_tre_header(tre).number_of_files
    assert all("/" in e.path or e.path.endswith((".pst", ".iff", ".mkr")) for e in entries[:10])