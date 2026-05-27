"""Tests for swg_pipeline.shader_stub."""

from __future__ import annotations

from pathlib import Path

import pytest

from swg_pipeline.shader_stub import ShaderStub, copy_shader_stub
from swg_pipeline.swg_main_paths import shader_template
from swg_pipeline.validate import validate_shader_template


def _bed_shader() -> Path:
    path = shader_template("shader/frn_all_bed_sm_s1_as9.sht")
    if path is None:
        pytest.skip("swg-main serverdata not available")
    return path


def test_copy_shader_stub_byte_identical(tmp_path: Path):
    ref = _bed_shader()
    out = tmp_path / "copy.sht"
    copy_shader_stub(ref, out)
    assert out.read_bytes() == ref.read_bytes()
    result = validate_shader_template(out)
    assert result.ok, result.errors


def test_copy_shader_stub_override_main_texture(tmp_path: Path):
    ref = _bed_shader()
    out = tmp_path / "custom.sht"
    new_main = "texture/my_custom_bed.dds"
    copy_shader_stub(ref, out, texture_overrides={"MAIN": new_main})
    stub = ShaderStub.from_file(out)
    main = next(slot for slot in stub.textures if slot.tag == "MAIN")
    assert main.path == new_main
    assert validate_shader_template(out).ok


def test_copy_shader_stub_override_effect(tmp_path: Path):
    ref = _bed_shader()
    out = tmp_path / "custom.sht"
    copy_shader_stub(ref, out, effect="effect\\a_basic.eft")
    stub = ShaderStub.from_file(out)
    assert stub.effect_path == "effect\\a_basic.eft"