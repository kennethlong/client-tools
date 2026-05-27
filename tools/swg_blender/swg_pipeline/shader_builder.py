"""Build SSHT static shader templates from material specs (Phase 8.8)."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

from swg_iff.writer import (
    make_chunk,
    make_chunk_float,
    make_chunk_int32,
    make_chunk_string,
    make_chunk_uint32,
    make_form,
    make_chunk_bool8,
)
from swg_pipeline.shader_stub import ShaderStub, ShaderTextureSlot, _tag_str_to_int
from swg_pipeline.validate import validate_shader_template

VectorArgb = tuple[float, float, float, float]


@dataclass
class ShaderMaterialSpec:
    tag: str = "MAIN"
    ambient: VectorArgb = (1.0, 1.0, 1.0, 1.0)
    diffuse: VectorArgb = (1.0, 1.0, 1.0, 1.0)
    emissive: VectorArgb = (1.0, 0.0, 0.0, 0.0)
    specular: VectorArgb = (1.0, 0.0, 0.0, 0.0)
    specular_power: float = 0.0


@dataclass
class ShaderBuildSpec:
    effect: str = "effect\\a_simple.eft"
    material: ShaderMaterialSpec = field(default_factory=ShaderMaterialSpec)
    textures: list[ShaderTextureSlot] = field(default_factory=list)
    texture_coord_sets: dict[str, int] = field(default_factory=dict)
    texture_factors: dict[str, int] = field(default_factory=dict)
    alpha_reference: dict[str, int] = field(default_factory=dict)


def _write_matl(material: ShaderMaterialSpec) -> bytes:
    payload = b"".join(
        make_chunk_float(v)
        for channel in (
            material.ambient,
            material.diffuse,
            material.emissive,
            material.specular,
        )
        for v in channel
    )
    payload += make_chunk_float(material.specular_power)
    return make_chunk("MATL", payload)


def _write_mats(material: ShaderMaterialSpec) -> bytes:
    body = make_chunk("TAG ", make_chunk_int32(_tag_str_to_int(material.tag))) + _write_matl(
        material
    )
    return make_form("MATS", make_form("0000", body))


def _write_tcss(texture_coord_sets: dict[str, int]) -> bytes:
    sets = dict(texture_coord_sets)
    sets.setdefault("MAIN", 0)
    payload = b""
    for tag, index in sorted(sets.items()):
        payload += make_chunk_int32(_tag_str_to_int(tag))
        payload += bytes([index & 0xFF])
    return make_form("TCSS", make_chunk("0000", payload))


def _write_tfns(texture_factors: dict[str, int]) -> bytes:
    if not texture_factors:
        return b""
    payload = b""
    for tag, value in sorted(texture_factors.items()):
        payload += make_chunk_int32(_tag_str_to_int(tag))
        payload += make_chunk_uint32(value)
    return make_form("TFNS", make_chunk("0000", payload))


def _write_arvs(alpha_reference: dict[str, int]) -> bytes:
    if not alpha_reference:
        return b""
    payload = b""
    for tag, value in sorted(alpha_reference.items()):
        payload += make_chunk_int32(_tag_str_to_int(tag))
        payload += bytes([value & 0xFF])
    return make_form("ARVS", make_chunk("0000", payload))


def build_shader_stub(spec: ShaderBuildSpec) -> ShaderStub:
    from swg_pipeline.shader_stub import _write_textures

    if not spec.textures:
        raise ValueError("shader spec requires at least one texture slot")

    return ShaderStub(
        mats_raw=_write_mats(spec.material),
        textures=list(spec.textures),
        tcss_raw=_write_tcss(spec.texture_coord_sets),
        tfns_raw=_write_tfns(spec.texture_factors),
        arvs_raw=_write_arvs(spec.alpha_reference),
        effect_path=spec.effect,
    )


def build_shader_bytes(spec: ShaderBuildSpec) -> bytes:
    return build_shader_stub(spec).to_bytes()


def write_shader_from_spec(
    spec: ShaderBuildSpec,
    output: str | Path,
    *,
    validate: bool = True,
) -> Path:
    out = Path(output)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(build_shader_bytes(spec))
    if validate:
        result = validate_shader_template(out)
        if not result.ok:
            raise ValueError("; ".join(result.errors))
    return out


def write_shader_for_mesh(
    mesh_shader_relpath: str,
    spec: ShaderBuildSpec,
    output_dir: str | Path,
    *,
    validate: bool = True,
) -> Path:
    rel = mesh_shader_relpath.replace("\\", "/").lstrip("/")
    out = Path(output_dir) / rel
    return write_shader_from_spec(spec, out, validate=validate)


def specmap_defaults() -> tuple[dict[str, int], dict[str, int]]:
    """Retail spec-map shaders commonly carry KCLB/CEPS blocks (golden bed pattern)."""
    return {"KCLB": 0x000000FF}, {"CEPS": 1}