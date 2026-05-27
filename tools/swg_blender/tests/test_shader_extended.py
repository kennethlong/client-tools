"""Phase 10 extended shader template tests."""

from __future__ import annotations

import tempfile
from pathlib import Path

import pytest

from swg_pipeline.shader_extended import (
    build_cshd_bytes,
    build_opst_bytes,
    build_swts_bytes,
    load_cshd_spec,
    load_opst_spec,
    load_shader_spec,
    load_swts_spec,
)
from swg_pipeline.swg_main_paths import shader_template
from swg_pipeline.validate import validate_shader_file


def _require(rel: str) -> Path:
    p = shader_template(rel.replace("shader/", ""))
    if p is None:
        pytest.skip(f"{rel} not on swg-main")
    return p


@pytest.mark.parametrize(
    "shader_relpath,loader,builder",
    [
        ("shader/abyssin_body.sht", load_cshd_spec, build_cshd_bytes),
        ("shader/anim_bank_side_screen.sht", load_swts_spec, build_swts_bytes),
        ("shader/arms.sht", load_opst_spec, build_opst_bytes),
    ],
)
def test_extended_shader_roundtrip(shader_relpath, loader, builder):
    original = loader(_require(shader_relpath))
    out = Path(tempfile.gettempdir()) / f"rt_{Path(shader_relpath).name}"
    out.write_bytes(builder(original))
    assert validate_shader_file(out).ok
    rebuilt = loader(out)
    if hasattr(original, "base_shader_path"):
        assert rebuilt.base_shader_path == original.base_shader_path
    elif hasattr(original, "base"):
        assert rebuilt.base.effect == original.base.effect
        assert len(rebuilt.base.textures) == len(original.base.textures)


def test_load_shader_spec_dispatch():
    spec = load_shader_spec(_require("shader/abyssin_body.sht"))
    assert hasattr(spec, "base")
