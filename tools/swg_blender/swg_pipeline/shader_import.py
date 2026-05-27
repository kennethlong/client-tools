"""Load SSHT static shaders into ShaderBuildSpec (Phase 10.1)."""

from __future__ import annotations

from pathlib import Path

from swg_iff.reader import IffReader
from swg_iff.tags import TAG_0000, tag_from_str
from swg_pipeline.shader_builder import ShaderBuildSpec, ShaderMaterialSpec
from swg_pipeline.shader_effects import alpha_reference_for_effect, texture_factors_for_effect
from swg_pipeline.shader_stub import ShaderStub, _tag_int_to_str


def load_shader_build_spec(path: str | Path) -> ShaderBuildSpec:
    """Parse an SSHT 0000/0001 file into a ShaderBuildSpec."""
    return spec_from_shader_stub(ShaderStub.from_file(path))


def spec_from_shader_stub(stub: ShaderStub) -> ShaderBuildSpec:
    texture_tags = {slot.tag for slot in stub.textures}
    texture_coord_sets = _parse_tcss(stub.tcss_raw)
    texture_factors = _parse_tfns(stub.tfns_raw) or texture_factors_for_effect(
        stub.effect_path, texture_tags
    )
    alpha_reference = _parse_arvs(stub.arvs_raw) or alpha_reference_for_effect(
        stub.effect_path, texture_tags
    )
    if not texture_coord_sets:
        texture_coord_sets = {"MAIN": 0}
        next_idx = 1
        for slot in stub.textures:
            if slot.tag == "MAIN":
                continue
            if slot.tag not in texture_coord_sets:
                texture_coord_sets[slot.tag] = next_idx
                next_idx += 1

    return ShaderBuildSpec(
        effect=stub.effect_path,
        material=ShaderMaterialSpec(),
        textures=list(stub.textures),
        texture_coord_sets=texture_coord_sets,
        texture_factors=texture_factors,
        alpha_reference=alpha_reference,
    )


def _parse_tag_byte_pairs(data: bytes, *, value_size: int) -> dict[str, int]:
    entries: dict[str, int] = {}
    stride = 4 + value_size
    offset = 0
    while offset + stride <= len(data):
        tag_int = int.from_bytes(data[offset : offset + 4], "big")
        if value_size == 1:
            value = data[offset + 4]
        else:
            value = int.from_bytes(data[offset + 4 : offset + stride], "big")
        entries[_tag_int_to_str(tag_int)] = value
        offset += stride
    return entries


def _parse_chunk_payload(raw: bytes, form_tag: str) -> bytes:
    if not raw:
        return b""
    reader = IffReader(raw)
    reader.enter_form(tag_from_str(form_tag))
    try:
        reader.enter_chunk(TAG_0000)
        data = reader.read_chunk_remaining()
        reader.exit_chunk(TAG_0000)
        return data
    finally:
        reader.exit_form(tag_from_str(form_tag))


def _parse_tcss(raw: bytes) -> dict[str, int]:
    return _parse_tag_byte_pairs(_parse_chunk_payload(raw, "TCSS"), value_size=1)


def _parse_tfns(raw: bytes) -> dict[str, int]:
    return _parse_tag_byte_pairs(_parse_chunk_payload(raw, "TFNS"), value_size=4)


def _parse_arvs(raw: bytes) -> dict[str, int]:
    return _parse_tag_byte_pairs(_parse_chunk_payload(raw, "ARVS"), value_size=1)
