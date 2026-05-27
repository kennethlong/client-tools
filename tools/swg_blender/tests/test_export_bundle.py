"""Tests for swg_pipeline.export_bundle."""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from swg_pipeline.export_bundle import export_static_bundle_from_mesh_file
from swg_pipeline.swg_main_paths import shader_template
from swg_pipeline.validate import validate_static_mesh, validate_shader_template
from tests.test_golden import _resolve_golden_msh


def test_export_bundle_from_golden(tmp_path: Path):
    mesh = _resolve_golden_msh()
    ref = shader_template("shader/frn_all_bed_sm_s1_as9.sht")
    if ref is None:
        pytest.skip("swg-main serverdata not available")

    result = export_static_bundle_from_mesh_file(
        mesh,
        tmp_path / "bundle",
        shader_reference=ref,
        copy_textures=False,
    )

    assert result.mesh_path.is_file()
    assert len(result.shader_paths) == 1
    assert validate_static_mesh(result.mesh_path).ok
    assert validate_shader_template(result.shader_paths[0]).ok
    assert result.manifest_path is not None
    assert result.rsp_paths
    assert result.client_cfg_path is not None
    manifest = json.loads(result.manifest_path.read_text(encoding="utf-8"))
    assets = manifest.get("assets", manifest)
    assert assets["mesh"].startswith("appearance/mesh/")
    assert manifest.get("pipeline_version") == "3.009"
    assert manifest["client_test_notes"]
    assert manifest["rsp_files"]
    assert assets["client_cfg"]


def test_export_bundle_no_rsp(tmp_path: Path):
    mesh = _resolve_golden_msh()
    ref = shader_template("shader/frn_all_bed_sm_s1_as9.sht")
    if ref is None:
        pytest.skip("swg-main serverdata not available")

    result = export_static_bundle_from_mesh_file(
        mesh,
        tmp_path / "bundle",
        shader_reference=ref,
        copy_textures=False,
        write_rsp=False,
        write_client_cfg=False,
    )
    assert result.mesh_path.is_file()
    assert not result.rsp_paths
    assert result.client_cfg_path is None


def test_export_bundle_roundtrip_topology(tmp_path: Path):
    mesh = _resolve_golden_msh()
    ref = shader_template("shader/frn_all_bed_sm_s1_as9.sht")
    if ref is None:
        pytest.skip("swg-main serverdata not available")

    result = export_static_bundle_from_mesh_file(
        mesh,
        tmp_path / "bundle",
        mesh_relpath="appearance/mesh/test_bed.msh",
        shader_reference=ref,
        copy_textures=False,
    )

    roundtrip = validate_static_mesh(result.mesh_path)
    assert roundtrip.ok
    assert roundtrip.stats["vertices"] == 139
    assert roundtrip.stats["triangles"] == 201