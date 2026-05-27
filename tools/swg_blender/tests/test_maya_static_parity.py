"""Phase 8.10 byte-compare / parity harness tests."""

from __future__ import annotations

from pathlib import Path

import pytest

from swg_pipeline.parity import compare_scenes, compare_static_mesh_bytes, parity_report, roundtrip_scene
from swg_scene.mesh_static import load_static_mesh
from swg_scene.mesh_static_export import write_static_mesh
from tests.test_golden import _resolve_golden_msh


def test_golden_bed_semantic_roundtrip():
    path = _resolve_golden_msh()
    original = load_static_mesh(path)
    original.rebuild_appearance = False
    original.optimize_indices = False
    roundtrip = roundtrip_scene(path, rebuild_appearance=False, optimize_indices=False)
    diff = compare_scenes(original, roundtrip)
    assert diff.ok, diff.errors
    assert diff.stats["vertices"] == 139
    assert diff.stats["triangles"] == 201


def test_golden_bed_preserves_appearance_bytes():
    path = _resolve_golden_msh()
    original = load_static_mesh(path)
    original.rebuild_appearance = False
    original.optimize_indices = False
    exported = write_static_mesh(original)
    ref = path.read_bytes()
    byte_diff = compare_static_mesh_bytes(ref, exported, compare_appearance=True)
    assert byte_diff.ok, byte_diff.errors


def test_parity_report_ok_on_golden_bed():
    report = parity_report(_resolve_golden_msh(), preserve_appearance=True)
    assert report.ok, report.errors + report.warnings
