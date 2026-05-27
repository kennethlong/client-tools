"""Phase 11 CMPA/DTLA tests."""

from __future__ import annotations

import tempfile
from pathlib import Path

import pytest

from swg_iff.reader import validate_iff
from swg_pipeline.swg_main_paths import appearance_component, appearance_lod
from swg_scene.component_appearance import (
    load_component_appearance,
    write_component_appearance_bytes,
)
from swg_scene.detail_appearance import (
    load_detail_appearance,
    write_detail_appearance_bytes,
)
from swg_scene.hierarchy import decode_export_name, validate_hierarchy_names


def _require_cmp() -> Path:
    p = appearance_component("barc_speeder_destroyed_l0.cmp")
    if p is None:
        p = appearance_component("aaa_bluebolt.cmp")
    if p is None:
        pytest.skip("no CMPA fixture on swg-main")
    return p


def _require_lod() -> Path:
    p = appearance_lod("abyssin_m_arms.lod")
    if p is None:
        pytest.skip("no DTLA fixture on swg-main")
    return p


def test_cmpa_roundtrip():
    original = load_component_appearance(_require_cmp())
    assert original.parts
    out = Path(tempfile.gettempdir()) / "cmp_roundtrip.cmp"
    out.write_bytes(write_component_appearance_bytes(original))
    assert validate_iff(out.read_bytes())
    rebuilt = load_component_appearance(out)
    assert rebuilt.version == original.version
    assert len(rebuilt.parts) == len(original.parts)
    assert rebuilt.parts[0].appearance_path == original.parts[0].appearance_path


def test_dtla_roundtrip():
    original = load_detail_appearance(_require_lod())
    assert original.levels
    out = Path(tempfile.gettempdir()) / "lod_roundtrip.lod"
    out.write_bytes(write_detail_appearance_bytes(original))
    assert validate_iff(out.read_bytes())
    rebuilt = load_detail_appearance(out)
    assert rebuilt.version == original.version
    assert len(rebuilt.levels) == len(original.levels)
    for before, after in zip(original.levels, rebuilt.levels, strict=True):
        assert before.level_id == after.level_id
        assert before.appearance_path == after.appearance_path


def test_hierarchy_decode():
    info = decode_export_name("c0")
    assert info is not None
    assert info.node_type == "cmp"
    assert info.index == 0


def test_hierarchy_lint_duplicate():
    report = validate_hierarchy_names(["mesh0", "mesh0", "c0"])
    assert not report.ok
