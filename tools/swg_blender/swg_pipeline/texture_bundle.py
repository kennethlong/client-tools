"""Copy retail/generated textures into export bundles (Phase 10.7)."""

from __future__ import annotations

from pathlib import Path

from swg_pipeline.dds import install_texture_for_bundle
from swg_pipeline.shader_builder import ShaderBuildSpec
from swg_pipeline.shader_stub import ShaderStub


def copy_texture_relpath(relpath: str, output_dir: str | Path) -> Path | None:
    return install_texture_for_bundle(relpath, output_dir)


def copy_textures_from_paths(
    relpaths: list[str],
    output_dir: str | Path,
) -> list[Path]:
    copied: list[Path] = []
    seen: set[str] = set()
    for relpath in relpaths:
        norm = relpath.replace("\\", "/")
        if not norm or norm in seen:
            continue
        seen.add(norm)
        dest = copy_texture_relpath(norm, output_dir)
        if dest is not None:
            copied.append(dest)
    return copied


def copy_textures_from_shader_stub(
    stub: ShaderStub,
    output_dir: str | Path,
) -> list[Path]:
    return copy_textures_from_paths([slot.path for slot in stub.textures], output_dir)


def copy_textures_from_shader_file(
    shader_path: str | Path,
    output_dir: str | Path,
) -> list[Path]:
    stub = ShaderStub.from_file(shader_path)
    return copy_textures_from_shader_stub(stub, output_dir)


def copy_textures_from_shader_spec(
    spec: ShaderBuildSpec,
    output_dir: str | Path,
) -> list[Path]:
    return copy_textures_from_paths([slot.path for slot in spec.textures], output_dir)
