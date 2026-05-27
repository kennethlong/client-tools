"""Phase 10 SSHT import + roundtrip tests."""

from __future__ import annotations

import tempfile
from pathlib import Path

import pytest

from swg_pipeline.shader_builder import build_shader_bytes
from swg_pipeline.shader_import import load_shader_build_spec, spec_from_shader_stub
from swg_pipeline.shader_stub import ShaderStub
from swg_pipeline.swg_main_paths import serverdata_file, shader_template
from swg_pipeline.validate import validate_shader_template


def _require(path: str) -> Path:
    p = serverdata_file(path) or shader_template(path.replace("shader/", ""))
    if p is None or not p.is_file():
        pytest.skip(f"{path} not available on swg-main")
    return p


@pytest.mark.parametrize(
    "shader_relpath",
    [
        "shader/frn_all_bed_sm_s1_as9.sht",
        "shader/a1_deluxe_floater_sm_ae9.sht",
        "shader/abyssin_hair_aa7.sht",
    ],
)
def test_load_retail_shader_specs(shader_relpath: str):
    spec = load_shader_build_spec(_require(shader_relpath))
    assert spec.textures
    assert spec.effect.startswith("effect\\")


def test_specmap_roundtrip_matches_retail_tags():
    ref_path = _require("shader/frn_all_bed_sm_s1_as9.sht")
    original = spec_from_shader_stub(ShaderStub.from_file(ref_path))
    out = Path(tempfile.gettempdir()) / "specmap_roundtrip.sht"
    out.write_bytes(build_shader_bytes(original))
    assert validate_shader_template(out).ok
    rebuilt = spec_from_shader_stub(ShaderStub.from_file(out))
    assert rebuilt.effect == original.effect
    assert [(s.tag, s.path) for s in rebuilt.textures] == [
        (s.tag, s.path) for s in original.textures
    ]


def test_emismap_roundtrip():
    ref_path = _require("shader/a1_deluxe_floater_sm_ae9.sht")
    original = load_shader_build_spec(ref_path)
    out = Path(tempfile.gettempdir()) / "emismap_roundtrip.sht"
    out.write_bytes(build_shader_bytes(original))
    rebuilt = load_shader_build_spec(out)
    assert rebuilt.effect == original.effect
    assert {s.tag for s in rebuilt.textures} == {s.tag for s in original.textures}
