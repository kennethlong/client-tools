"""Phase 7 combined validation bundle tests."""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from swg_pipeline.export_bundle import _validate_shader_file, export_phase7_validation_bundle
from swg_pipeline.swg_main_paths import shader_template
from swg_pipeline.validate import validate_static_mesh


@pytest.fixture
def swg_main_shaders():
    if shader_template("shader/frn_all_bed_sm_s1_as9.sht") is None:
        pytest.skip("swg-main serverdata not available")
    if shader_template("shader/abyssin_body.sht") is None:
        pytest.skip("swg-main shader for abyssin not available")


def test_export_phase7_validation_bundle(tmp_path: Path, swg_main_shaders):
    result = export_phase7_validation_bundle(tmp_path / "bundle")

    assert result.output_dir.is_dir()
    assert result.static.mesh_path.is_file()
    assert result.skeletal.mesh_path.is_file()
    assert result.skeletal.skeleton_path.is_file()
    assert result.skeletal.animation_paths
    assert result.client_cfg_path.is_file()
    assert result.spawn_notes_path.is_file()
    assert result.manifest_path.is_file()
    assert result.rsp_paths

    assert validate_static_mesh(result.static.mesh_path).ok
    for shader_path in result.static.shader_paths + result.skeletal.shader_paths:
        assert _validate_shader_file(shader_path).ok

    manifest = json.loads(result.manifest_path.read_text(encoding="utf-8"))
    assert manifest["static"]["mesh"].endswith("frn_all_bed_sm_s1_l0.msh")
    assert manifest["skeletal"]["mesh"].endswith("abyssin_m_arms_l0.mgn")

    cfg = result.client_cfg_path.read_text(encoding="utf-8")
    assert "searchPath_00_12=" in cfg
    resolved = str(result.output_dir.resolve())
    assert resolved in cfg or resolved.replace("\\", "/") in cfg