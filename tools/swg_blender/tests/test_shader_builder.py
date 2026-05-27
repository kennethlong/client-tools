"""Phase 8.8 shader builder tests."""

from __future__ import annotations

from pathlib import Path

import pytest

from swg_pipeline.shader_builder import ShaderBuildSpec, ShaderMaterialSpec, build_shader_bytes, specmap_defaults
from swg_pipeline.shader_stub import ShaderStub, ShaderTextureSlot
from swg_pipeline.swg_main_paths import shader_template
from swg_pipeline.validate import validate_shader_template


def _bed_shader() -> Path:
    path = shader_template("shader/frn_all_bed_sm_s1_as9.sht")
    if path is None:
        pytest.skip("swg-main serverdata not available")
    return path


def test_build_specmap_shader_validates(tmp_path: Path):
    tfns, arvs = specmap_defaults()
    spec = ShaderBuildSpec(
        effect="effect\\a_specmap.eft",
        material=ShaderMaterialSpec(),
        textures=[
            ShaderTextureSlot(tag="MAIN", path="texture/frn_all_bed_sm_s1.dds"),
            ShaderTextureSlot(tag="SPEC", path="texture/frn_all_bed_sm_s1.dds"),
        ],
        texture_coord_sets={"MAIN": 0},
        texture_factors=tfns,
        alpha_reference=arvs,
    )
    out = tmp_path / "built.sht"
    out.write_bytes(build_shader_bytes(spec))
    result = validate_shader_template(out)
    assert result.ok, result.errors
    stub = ShaderStub.from_file(out)
    assert stub.effect_path == "effect\\a_specmap.eft"
    assert {slot.tag for slot in stub.textures} == {"MAIN", "SPEC"}


def test_build_simple_shader_validates(tmp_path: Path):
    spec = ShaderBuildSpec(
        textures=[ShaderTextureSlot(tag="MAIN", path="texture/test.dds")],
    )
    out = tmp_path / "simple.sht"
    out.write_bytes(build_shader_bytes(spec))
    assert validate_shader_template(out).ok


def test_built_specmap_matches_golden_tags(tmp_path: Path):
    ref = ShaderStub.from_file(_bed_shader())
    tfns, arvs = specmap_defaults()
    spec = ShaderBuildSpec(
        effect=ref.effect_path,
        textures=list(ref.textures),
        texture_coord_sets={"MAIN": 0},
        texture_factors=tfns,
        alpha_reference=arvs,
    )
    out = tmp_path / "compare.sht"
    out.write_bytes(build_shader_bytes(spec))
    built = ShaderStub.from_file(out)
    assert built.effect_path == ref.effect_path
    assert [(s.tag, s.path) for s in built.textures] == [(s.tag, s.path) for s in ref.textures]