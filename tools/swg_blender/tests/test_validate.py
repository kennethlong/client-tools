"""Tests for swg_pipeline.validate."""

from __future__ import annotations

from pathlib import Path

import pytest

from swg_pipeline.swg_main_paths import shader_template
from swg_pipeline.validate import (
    validate_roundtrip,
    validate_shader_template,
    validate_static_mesh,
    validate_static_mesh_with_shader,
)
from tests.test_golden import _resolve_golden_msh


def test_validate_golden_static_mesh():
    path = _resolve_golden_msh()
    result = validate_static_mesh(path)
    assert result.ok, result.errors
    assert result.stats["vertices"] == 139
    assert result.stats["triangles"] == 201


def test_validate_golden_roundtrip():
    path = _resolve_golden_msh()
    result = validate_roundtrip(path)
    assert result.ok, result.errors


def test_validate_golden_with_shader():
    path = _resolve_golden_msh()
    shader = shader_template("shader/frn_all_bed_sm_s1_as9.sht")
    if shader is None:
        pytest.skip("swg-main serverdata not available")
    result = validate_static_mesh_with_shader(path)
    assert result.ok, result.errors


def test_validate_shader_template_live():
    shader = shader_template("shader/frn_all_bed_sm_s1_as9.sht")
    if shader is None:
        pytest.skip("swg-main serverdata not available")
    result = validate_shader_template(shader)
    assert result.ok, result.errors


def test_validate_missing_file(tmp_path: Path):
    result = validate_static_mesh(tmp_path / "missing.msh")
    assert not result.ok
    assert result.errors