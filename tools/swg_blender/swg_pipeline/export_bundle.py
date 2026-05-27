"""Export a client-ready static mesh bundle (serverdata layout)."""

from __future__ import annotations

import argparse
import json
import shutil
import sys
from dataclasses import dataclass, field
from pathlib import Path

from swg_pipeline.export_manifest import build_export_manifest, write_export_manifest
from swg_pipeline.pack_pipeline import PackError
from swg_pipeline.rsp_builder import (
    build_rsp_from_roots,
    client_search_path_snippet,
    write_client_cfg_fragment,
)
from swg_pipeline.shader_stub import ShaderStub, copy_shader_stub_for_mesh
from swg_pipeline.shader_builder import write_shader_for_mesh
from swg_pipeline.texture_bundle import (
    copy_textures_from_shader_file,
    copy_textures_from_shader_spec,
)
from swg_pipeline.swg_main_paths import (
    appearance_animation,
    appearance_mesh,
    appearance_skeleton,
    shader_template,
    serverdata_dir,
    serverdata_file,
)
from swg_iff.reader import IffError, root_form_type, validate_iff
from swg_iff.tags import TAG_SSHT
from swg_pipeline.validate import (
    ValidationResult,
    validate_shader_file,
    validate_shader_template,
    validate_static_mesh,
)
from swg_scene.mesh_skeletal import load_skeletal_mesh
from swg_scene.mesh_static import load_static_mesh
from swg_scene.mesh_static_export import write_static_mesh_file
from swg_scene.model import SwgScene
from swg_scene.skeleton import load_skeleton
from swg_scene.skeletal_appearance import (
    SwgSkeletonBinding,
    load_skeletal_appearance,
    make_skeletal_appearance_for_bundle,
    write_skeletal_appearance_file,
)


@dataclass
class BundleResult:
    output_dir: Path
    mesh_path: Path
    shader_paths: list[Path] = field(default_factory=list)
    texture_paths: list[Path] = field(default_factory=list)
    manifest_path: Path | None = None
    rsp_paths: list[Path] = field(default_factory=list)
    client_cfg_path: Path | None = None

    def as_dict(self) -> dict[str, object]:
        return {
            "output_dir": str(self.output_dir),
            "mesh": str(self.mesh_path.relative_to(self.output_dir)).replace("\\", "/"),
            "shaders": [
                str(p.relative_to(self.output_dir)).replace("\\", "/")
                for p in self.shader_paths
            ],
            "textures": [
                str(p.relative_to(self.output_dir)).replace("\\", "/")
                for p in self.texture_paths
            ],
            "manifest": str(self.manifest_path) if self.manifest_path else None,
            "rsp_files": [
                str(p.relative_to(self.output_dir)).replace("\\", "/")
                for p in self.rsp_paths
            ],
            "client_cfg": str(self.client_cfg_path) if self.client_cfg_path else None,
        }


@dataclass
class SkeletalBundleResult:
    output_dir: Path
    mesh_path: Path
    skeleton_path: Path
    appearance_path: Path | None = None
    animation_paths: list[Path] = field(default_factory=list)
    shader_paths: list[Path] = field(default_factory=list)
    texture_paths: list[Path] = field(default_factory=list)
    manifest_path: Path | None = None
    rsp_paths: list[Path] = field(default_factory=list)
    client_cfg_path: Path | None = None

    def as_dict(self) -> dict[str, object]:
        root = self.output_dir
        return {
            "output_dir": str(root),
            "mesh": str(self.mesh_path.relative_to(root)).replace("\\", "/"),
            "skeleton": str(self.skeleton_path.relative_to(root)).replace("\\", "/"),
            "appearance": (
                str(self.appearance_path.relative_to(root)).replace("\\", "/")
                if self.appearance_path
                else None
            ),
            "animations": [
                str(p.relative_to(root)).replace("\\", "/") for p in self.animation_paths
            ],
            "shaders": [
                str(p.relative_to(root)).replace("\\", "/") for p in self.shader_paths
            ],
            "textures": [
                str(p.relative_to(root)).replace("\\", "/") for p in self.texture_paths
            ],
            "manifest": str(self.manifest_path) if self.manifest_path else None,
            "rsp_files": [
                str(p.relative_to(root)).replace("\\", "/") for p in self.rsp_paths
            ],
            "client_cfg": str(self.client_cfg_path) if self.client_cfg_path else None,
        }


@dataclass
class Phase7BundleResult:
    output_dir: Path
    static: BundleResult
    skeletal: SkeletalBundleResult
    manifest_path: Path
    client_cfg_path: Path
    rsp_paths: list[Path]
    spawn_notes_path: Path

    def as_dict(self) -> dict[str, object]:
        return {
            "output_dir": str(self.output_dir),
            "static": self.static.as_dict(),
            "skeletal": self.skeletal.as_dict(),
            "manifest": str(self.manifest_path),
            "client_cfg": str(self.client_cfg_path),
            "rsp_files": [str(p) for p in self.rsp_paths],
            "spawn_notes": str(self.spawn_notes_path),
        }


def _resolve_under_serverdata(relpath: str) -> Path | None:
    return serverdata_file(relpath)


def _bake_normal_texture(mesh, output_dir: Path, *, size: int = 256) -> Path | None:
    if not mesh.material.has_dot3:
        return None
    from swg_pipeline.dds import png_to_dds_file
    from swg_pipeline.normal_map import bake_swgmesh_normal_png

    rel = f"texture/{mesh.name}_norm.png"
    png_path = output_dir / rel
    bake_swgmesh_normal_png(mesh, png_path, width=size, height=size)
    dds_path = png_to_dds_file(png_path, png_path.with_suffix(".dds"))
    return dds_path


def _copy_texture_if_available(relpath: str, output_dir: Path) -> Path | None:
    from swg_pipeline.texture_bundle import copy_texture_relpath

    return copy_texture_relpath(relpath, output_dir)


def _copy_textures_for_shader_paths(
    shader_paths: list[Path],
    output_dir: Path,
) -> list[Path]:
    copied: list[Path] = []
    seen: set[str] = set()
    for shader_path in shader_paths:
        for path in copy_textures_from_shader_file(shader_path, output_dir):
            rel = path.relative_to(output_dir).as_posix()
            if rel in seen:
                continue
            seen.add(rel)
            copied.append(path)
    return copied


def export_static_bundle(
    scene: SwgScene,
    output_dir: str | Path,
    *,
    mesh_relpath: str,
    shader_reference: str | Path | None = None,
    shader_texture_overrides: dict[str, str] | None = None,
    shader_effect: str | None = None,
    copy_textures: bool = True,
    build_shaders_from_scene: bool = False,
    bake_normal_maps: bool = False,
    validate: bool = True,
    write_manifest: bool = True,
    write_rsp: bool = True,
    write_client_cfg: bool = True,
) -> BundleResult:
    """
    Write ``appearance/mesh/*.msh`` + optional ``shader/*.sht`` (+ textures) under
    ``output_dir`` using TreeFile-relative paths.

    When ``build_shaders_from_scene`` is True and no ``shader_reference`` is supplied,
    SSHT files are built from ``SwgScene`` material slots (Phase 10.1).
    """
    root = Path(output_dir)
    root.mkdir(parents=True, exist_ok=True)

    mesh_rel = mesh_relpath.replace("\\", "/").lstrip("/")
    mesh_path = root / mesh_rel
    mesh_path.parent.mkdir(parents=True, exist_ok=True)
    write_static_mesh_file(mesh_path, scene)

    baked_norm_paths: list[Path] = []
    if bake_normal_maps:
        for mesh in scene.meshes:
            baked = _bake_normal_texture(mesh, root)
            if baked is not None:
                baked_norm_paths.append(baked)

    shader_paths: list[Path] = []
    texture_paths: list[Path] = []

    if shader_reference is not None:
        seen_shaders: set[str] = set()
        for mesh in scene.meshes:
            rel = mesh.material.shader_relpath
            if not rel or rel in seen_shaders:
                continue
            seen_shaders.add(rel)
            shader_paths.append(
                _install_shader_for_mesh(
                    rel,
                    shader_reference,
                    root,
                    texture_overrides=shader_texture_overrides,
                    effect=shader_effect,
                )
            )

            if copy_textures:
                texture_paths.extend(_copy_textures_for_shader_paths(shader_paths[-1:], root))

    if build_shaders_from_scene and not shader_reference:
        from swg_blender.materials import build_shader_template_from_mesh

        seen_shaders: set[str] = set()
        for mesh in scene.meshes:
            rel = mesh.material.shader_relpath or f"shader/{mesh.name}.sht"
            if rel in seen_shaders:
                continue
            seen_shaders.add(rel)
            spec = build_shader_template_from_mesh(mesh)
            shader_paths.append(_write_shader_template_for_mesh(rel, spec, root))
            if copy_textures:
                from swg_pipeline.shader_builder import ShaderBuildSpec
                from swg_pipeline.shader_extended import CshdSpec

                if isinstance(spec, ShaderBuildSpec):
                    texture_paths.extend(copy_textures_from_shader_spec(spec, root))
                elif isinstance(spec, CshdSpec):
                    texture_paths.extend(copy_textures_from_shader_spec(spec.base, root))

    texture_paths.extend(baked_norm_paths)

    if validate:
        mesh_result = validate_static_mesh(mesh_path)
        if not mesh_result.ok:
            raise ValueError("; ".join(mesh_result.errors))
        for shader_path in shader_paths:
            shader_result = _validate_shader_file(shader_path)
            if not shader_result.ok:
                raise ValueError("; ".join(shader_result.errors))

    rsp_paths: list[Path] = []
    if write_rsp:
        rsp_dir = root / "rsp"
        rsp_paths = build_rsp_from_roots([root], rsp_dir)

    client_cfg_path: Path | None = None
    if write_client_cfg:
        client_cfg_path = write_client_cfg_fragment(
            [root], root / "client_search_paths.cfg"
        )

    result = BundleResult(
        output_dir=root,
        mesh_path=mesh_path,
        shader_paths=shader_paths,
        texture_paths=texture_paths,
        rsp_paths=rsp_paths,
        client_cfg_path=client_cfg_path,
    )

    if write_manifest:
        manifest_path = root / "swg_export_manifest.json"
        result.manifest_path = manifest_path
        write_export_manifest(
            manifest_path,
            build_export_manifest(
                bundle_type="static",
                output_dir=root,
                assets=result.as_dict(),
                rsp_files=[str(p) for p in rsp_paths],
                extra={"client_test_notes": _client_test_notes(root, mesh_rel)},
            ),
        )

    return result


def export_static_bundle_from_mesh_file(
    mesh_source: str | Path,
    output_dir: str | Path,
    *,
    mesh_relpath: str | None = None,
    shader_reference: str | Path | None = None,
    **kwargs: object,
) -> BundleResult:
    """Load an existing `.msh`, optionally re-export, and write a bundle."""
    source = Path(mesh_source)
    scene = load_static_mesh(source)
    if mesh_relpath is None:
        mesh_relpath = f"appearance/mesh/{source.stem}.msh"
    ref = shader_reference
    if ref is None and scene.meshes:
        ref = shader_template(scene.meshes[0].material.shader_relpath)
    return export_static_bundle(
        scene,
        output_dir,
        mesh_relpath=mesh_relpath,
        shader_reference=ref,
        **kwargs,  # type: ignore[arg-type]
    )


def _copy_into_bundle(source: Path, output_dir: Path, relpath: str) -> Path:
    rel = relpath.replace("\\", "/").lstrip("/")
    dest = Path(output_dir) / rel
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, dest)
    return dest


def _install_shader_for_mesh(
    shader_relpath: str,
    reference: str | Path,
    output_dir: Path,
    *,
    texture_overrides: dict[str, str] | None = None,
    effect: str | None = None,
) -> Path:
    """Copy SSHT via stub patcher; copy other shader forms (CSHD, etc.) verbatim."""
    ref = Path(reference)
    data = ref.read_bytes()
    if not validate_iff(data):
        raise ValueError(f"reference shader is not valid IFF: {ref}")
    if root_form_type(data) == TAG_SSHT:
        return copy_shader_stub_for_mesh(
            shader_relpath,
            reference=ref,
            output_dir=output_dir,
            texture_overrides=texture_overrides,
            effect=effect,
        )
    return _copy_into_bundle(ref, output_dir, shader_relpath)


def _write_shader_template_for_mesh(
    shader_relpath: str,
    spec,
    output_dir: str | Path,
    *,
    validate: bool = True,
) -> Path:
    from swg_pipeline.shader_builder import write_shader_for_mesh
    from swg_pipeline.shader_extended import ShaderBuildSpec, write_shader_template

    rel = shader_relpath.replace("\\", "/").lstrip("/")
    out = Path(output_dir) / rel
    if isinstance(spec, ShaderBuildSpec):
        return write_shader_for_mesh(rel, spec, output_dir, validate=validate)
    return write_shader_template(spec, out, validate=validate)


def _validate_shader_file(path: Path) -> ValidationResult:
    data = path.read_bytes()
    if not validate_iff(data):
        return ValidationResult(ok=False, errors=[f"invalid IFF structure: {path}"])
    return validate_shader_file(path)


def _resolve_golden_or_serverdata(
    golden: Path,
    serverdata_fn,
) -> Path:
    if golden.is_file():
        return golden
    resolved = serverdata_fn(golden.name)
    if resolved is not None:
        return resolved
    raise FileNotFoundError(
        f"fixture not found: {golden.name} (copy to {golden.parent} or set SWG_MAIN)"
    )


def export_skeletal_bundle_from_files(
    mgn_source: str | Path,
    skt_source: str | Path,
    output_dir: str | Path,
    *,
    animation_sources: list[str | Path] | None = None,
    shader_reference: str | Path | None = None,
    copy_textures: bool = True,
    bake_normal_maps: bool = False,
    validate: bool = True,
    write_manifest: bool = True,
    write_rsp: bool = True,
    write_client_cfg: bool = True,
    write_appearance: bool = True,
    appearance_name: str | None = None,
    mesh_relpaths: list[str] | None = None,
    skeleton_bindings: list[SwgSkeletonBinding] | None = None,
    skeleton_lat_pairs: list[tuple[str, str]] | None = None,
) -> SkeletalBundleResult:
    """Copy skeletal mesh, skeleton, optional animations, and shader into a dev bundle."""
    root = Path(output_dir)
    root.mkdir(parents=True, exist_ok=True)

    mgn_src = Path(mgn_source)
    skt_src = Path(skt_source)
    mesh_rel = f"appearance/mesh/{mgn_src.stem}.mgn"
    skel_rel = f"appearance/skeleton/{skt_src.stem}.skt"

    mesh_path = _copy_into_bundle(mgn_src, root, mesh_rel)
    skeleton_path = _copy_into_bundle(skt_src, root, skel_rel)

    animation_paths: list[Path] = []
    if animation_sources:
        for anim_src in animation_sources:
            p = Path(anim_src)
            anim_rel = f"appearance/animation/{p.stem}.ans"
            animation_paths.append(_copy_into_bundle(p, root, anim_rel))

    shader_paths: list[Path] = []
    texture_paths: list[Path] = []
    if shader_reference is not None:
        scene = load_skeletal_mesh(mesh_path)
        if not scene.meshes:
            raise ValueError(f"{mesh_path} contains no meshes")
        mesh = scene.meshes[0]
        rel = mesh.material.shader_relpath or f"shader/{mesh.name}.sht"
        shader_paths.append(
            _install_shader_for_mesh(rel, shader_reference, root)
        )
        if copy_textures:
            texture_paths.extend(_copy_textures_for_shader_paths(shader_paths, root))

    appearance_path: Path | None = None
    if write_appearance:
        sat_name = appearance_name or f"{mgn_src.stem}.sat"
        appearance_rel = f"appearance/{sat_name}"
        mesh_paths = mesh_relpaths or [mesh_rel.replace("\\", "/")]
        bindings = skeleton_bindings or [
            SwgSkeletonBinding(
                skeleton_relpath=skel_rel.replace("\\", "/"),
                attachment_transform="",
            )
        ]
        lat_pairs = skeleton_lat_pairs
        if lat_pairs is None and animation_paths:
            lat_pairs = [
                (
                    skel_rel.replace("\\", "/"),
                    f"appearance/lat/{skt_src.stem}.lat",
                )
            ]
        sat = make_skeletal_appearance_for_bundle(
            mesh_relpaths=mesh_paths,
            skeleton_bindings=bindings,
            create_animation_controller=bool(animation_paths),
            skeleton_lat_pairs=lat_pairs,
        )
        appearance_path = write_skeletal_appearance_file(root / appearance_rel, sat)
        if validate:
            load_skeletal_appearance(appearance_path)

    if validate:
        load_skeletal_mesh(mesh_path)
        load_skeleton(skeleton_path)
        for shader_path in shader_paths:
            shader_result = _validate_shader_file(shader_path)
            if not shader_result.ok:
                raise ValueError("; ".join(shader_result.errors))

    rsp_paths: list[Path] = []
    if write_rsp:
        rsp_paths = build_rsp_from_roots([root], root / "rsp")

    client_cfg_path: Path | None = None
    if write_client_cfg:
        client_cfg_path = write_client_cfg_fragment(
            [root], root / "client_search_paths.cfg"
        )

    result = SkeletalBundleResult(
        output_dir=root,
        mesh_path=mesh_path,
        skeleton_path=skeleton_path,
        appearance_path=appearance_path,
        animation_paths=animation_paths,
        shader_paths=shader_paths,
        texture_paths=texture_paths,
        rsp_paths=rsp_paths,
        client_cfg_path=client_cfg_path,
    )

    if write_manifest:
        manifest_path = root / "swg_export_manifest.json"
        result.manifest_path = manifest_path
        write_export_manifest(
            manifest_path,
            build_export_manifest(
                bundle_type="skeletal",
                output_dir=root,
                assets=result.as_dict(),
                rsp_files=[str(p) for p in rsp_paths],
                extra={
                    "client_test_notes": _skeletal_client_test_notes(
                        root, mesh_rel, skel_rel
                    )
                },
            ),
        )

    return result


def export_phase7_validation_bundle(
    output_dir: str | Path = r"D:\swg_dev_bundle",
    *,
    copy_textures: bool = False,
) -> Phase7BundleResult:
    """
    Phase 7 dev bundle: golden static prop + abyssin arms skeletal set in one tree.

    Writes ``client_search_paths.cfg``, rsp manifests, and ``PHASE7_SPAWN_NOTES.md``.
    """
    golden_dir = Path(__file__).resolve().parents[1] / "tests" / "golden"
    root = Path(output_dir)
    root.mkdir(parents=True, exist_ok=True)

    static_msh = _resolve_golden_or_serverdata(
        golden_dir / "frn_all_bed_sm_s1_l0.msh",
        appearance_mesh,
    )
    static_shader = shader_template("shader/frn_all_bed_sm_s1_as9.sht")
    if static_shader is None:
        raise FileNotFoundError("shader/frn_all_bed_sm_s1_as9.sht not found on swg-main")

    static_result = export_static_bundle_from_mesh_file(
        static_msh,
        root,
        shader_reference=static_shader,
        copy_textures=copy_textures,
        write_rsp=False,
        write_client_cfg=False,
    )

    mgn = _resolve_golden_or_serverdata(
        golden_dir / "abyssin_m_arms_l0.mgn",
        appearance_mesh,
    )
    skt = _resolve_golden_or_serverdata(
        golden_dir / "all_b.skt",
        appearance_skeleton,
    )
    anim = _resolve_golden_or_serverdata(
        golden_dir / "all_b_cbt_pistol_run_ready.ans",
        appearance_animation,
    )
    skel_shader = shader_template("shader/abyssin_body.sht")
    if skel_shader is None:
        raise FileNotFoundError("shader/abyssin_body.sht not found on swg-main")

    skeletal_result = export_skeletal_bundle_from_files(
        mgn,
        skt,
        root,
        animation_sources=[anim],
        shader_reference=skel_shader,
        copy_textures=copy_textures,
        write_rsp=False,
        write_client_cfg=False,
    )

    rsp_paths = build_rsp_from_roots([root], root / "rsp")
    client_cfg_path = write_client_cfg_fragment(
        [root], root / "client_search_paths.cfg", priority=12, sku=0
    )
    swgsource_snippet = root / "swgsource_client_d_snippet.cfg"
    swgsource_snippet.write_text(
        "\n".join(
            [
                "# Paste into [SharedFile] in client_d.cfg (SWGSource multi-SKU layout)",
                "# Priority 12 beats searchTree_00_7/8 (retail TREs at 7-8).",
                *client_search_path_snippet([root], priority=12, sku=0),
                "",
            ]
        ),
        encoding="utf-8",
    )

    spawn_notes_path = root / "PHASE7_SPAWN_NOTES.md"
    spawn_notes_path.write_text(
        _phase7_spawn_notes(root, static_result, skeletal_result),
        encoding="utf-8",
    )

    manifest_path = root / "swg_phase7_manifest.json"
    result = Phase7BundleResult(
        output_dir=root,
        static=static_result,
        skeletal=skeletal_result,
        manifest_path=manifest_path,
        client_cfg_path=client_cfg_path,
        rsp_paths=rsp_paths,
        spawn_notes_path=spawn_notes_path,
    )
    manifest_path.write_text(json.dumps(result.as_dict(), indent=2), encoding="utf-8")
    return result


def _skeletal_client_test_notes(
    output_dir: Path, mesh_relpath: str, skel_relpath: str
) -> list[str]:
    abs_root = output_dir.resolve()
    return [
        f"Mount bundle: searchPath0={abs_root}",
        f"Skeletal mesh TreeFile path: {mesh_relpath}",
        f"Skeleton TreeFile path: {skel_relpath}",
        "Use a creature/player appearance template that references these paths, or admin spawn with a custom template.",
        "See PHASE7_SPAWN_NOTES.md in the bundle for step-by-step client validation.",
    ]


def _phase7_spawn_notes(
    root: Path,
    static: BundleResult,
    skeletal: SkeletalBundleResult,
) -> str:
    static_mesh = static.mesh_path.relative_to(root).as_posix()
    skel_mesh = skeletal.mesh_path.relative_to(root).as_posix()
    skel_path = skeletal.skeleton_path.relative_to(root).as_posix()
    anim_paths = [p.relative_to(root).as_posix() for p in skeletal.animation_paths]
    static_shader = (
        static.shader_paths[0].relative_to(root).as_posix()
        if static.shader_paths
        else "shader/..."
    )
    anims = "\n".join(f"- `{a}`" for a in anim_paths) or "- (none)"
    return f"""# Phase 7 client validation — spawn notes

Generated by `python -m swg_pipeline.export_bundle phase7`.

## 1. Mount the bundle

Add to your active client cfg under `[SharedFile]` (highest index = highest priority):

```ini
searchPath0={root.resolve()}
```

Or copy from `{root / "client_search_paths.cfg"}`.

Restart the client completely after saving the cfg.

## 2. Static prop test (M2 / M8)

| Asset | TreeFile path |
| --- | --- |
| Mesh | `{static_mesh}` |
| Shader | `{static_shader}` |

**Pass criteria**

- Mesh visible in world (not pink/invisible)
- Correct scale and floor contact
- MAIN texture visible (may load from retail TRE if not copied)

**How to spawn**

- Use `/object createItem` or an admin tool that spawns a static object whose mesh template matches `{static_mesh}`.
- If no retail object references this exact mesh, create or clone an object template pointing at `{static_mesh}`.

## 3. Skeletal test (M4 / M8)

| Asset | TreeFile path |
| --- | --- |
| SKMG | `{skel_mesh}` |
| SKT | `{skel_path}` |
| Animations | see below |

{anims}

**Pass criteria**

- Skinned mesh renders on a humanoid skeleton (no explode/spaghetti weights)
- Idle or test animation plays without log spam

**How to spawn**

- Spawn a humanoid NPC/creature using skeleton `{skel_path}` and mesh `{skel_mesh}`.
- Retail humanoids using `all_b.skt` are the easiest baseline — override only the mesh path if needed.

## 4. Log checks

Watch `log.txt` for:

- `Could not find` / `Unable to load` for `.msh`, `.mgn`, `.skt`, `.sht`
- Shader template failures (pink geometry)

## 5. Record results

Update `tools/swg_blender/PHASE7_VALIDATION.md` with pass/fail and client build info.

See also: `CLIENT_SPAWN_CHECKLIST.md`
"""


def _client_test_notes(output_dir: Path, mesh_relpath: str) -> list[str]:
    abs_root = output_dir.resolve()
    checklist = Path(__file__).resolve().parents[1] / "CLIENT_SPAWN_CHECKLIST.md"
    return [
        "Mount this folder as a TreeFile filesystem search path for loose dev assets.",
        f"Example (add to client_d.cfg [SharedFile] section): searchPath0={abs_root}",
        "Restart the client after editing client_d.cfg.",
        f"Spawn or place an object that references appearance/{Path(mesh_relpath).name} or the full template path your object uses.",
        "If using retail texture paths in the .sht, textures may resolve from existing TREs without copying DDS files.",
        "For custom texture paths, ensure the .dds exists under texture/ in this bundle or in a mounted TRE.",
        f"Full checklist: {checklist}",
    ]


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Export client-ready asset bundles for SWG dev testing"
    )
    sub = parser.add_subparsers(dest="command")

    static_p = sub.add_parser("static", help="export a static .msh bundle")
    static_p.add_argument("--mesh", type=Path, required=True, help="source .msh file")
    static_p.add_argument(
        "--output-dir", type=Path, required=True, help="bundle root directory"
    )
    static_p.add_argument(
        "--mesh-relpath",
        help="TreeFile path for output mesh (default: appearance/mesh/<stem>.msh)",
    )
    static_p.add_argument(
        "--shader-reference",
        type=Path,
        help="reference .sht to copy (default: resolve from mesh shader path on swg-main)",
    )
    static_p.add_argument("--main-texture", help="override MAIN texture in stub")
    static_p.add_argument(
        "--no-copy-textures",
        action="store_true",
        help="do not copy texture/*.dds from swg-main serverdata",
    )
    static_p.add_argument(
        "--build-shaders-from-scene",
        action="store_true",
        help="build SSHT from mesh material slots instead of copying a reference .sht",
    )
    static_p.add_argument("--no-rsp", action="store_true")
    static_p.add_argument("--no-client-cfg", action="store_true")
    static_p.add_argument(
        "--pack-tre",
        action="store_true",
        help="after export, pack rsp/*.rsp -> tre/*.tre (requires TreeFileBuilder)",
    )
    static_p.add_argument("--author", default="", help="author string in export manifest")

    skel_p = sub.add_parser("skeletal", help="export skeletal .mgn + .skt bundle")
    skel_p.add_argument("--mgn", type=Path, required=True)
    skel_p.add_argument("--skt", type=Path, required=True)
    skel_p.add_argument("--output-dir", type=Path, required=True)
    skel_p.add_argument("--animation", type=Path, action="append", default=[])
    skel_p.add_argument("--shader-reference", type=Path)
    skel_p.add_argument("--no-copy-textures", action="store_true")
    skel_p.add_argument("--no-rsp", action="store_true")
    skel_p.add_argument("--no-client-cfg", action="store_true")
    skel_p.add_argument("--pack-tre", action="store_true")
    skel_p.add_argument("--author", default="")

    phase7_p = sub.add_parser(
        "phase7",
        help="export combined Phase 7 validation bundle (static + skeletal)",
    )
    phase7_p.add_argument(
        "--output-dir",
        type=Path,
        default=Path(r"D:\swg_dev_bundle"),
        help="bundle root (default: D:\\swg_dev_bundle)",
    )
    phase7_p.add_argument(
        "--copy-textures",
        action="store_true",
        help="copy MAIN texture DDS files from swg-main serverdata",
    )
    phase7_p.add_argument("--pack-tre", action="store_true")

    # Legacy single-command interface (--mesh without subcommand)
    parser.add_argument("--mesh", type=Path, help=argparse.SUPPRESS)
    parser.add_argument("--output-dir", type=Path, help=argparse.SUPPRESS)
    parser.add_argument("--mesh-relpath", help=argparse.SUPPRESS)
    parser.add_argument("--shader-reference", type=Path, help=argparse.SUPPRESS)
    parser.add_argument("--main-texture", help=argparse.SUPPRESS)
    parser.add_argument("--no-copy-textures", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--no-rsp", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--no-client-cfg", action="store_true", help=argparse.SUPPRESS)

    args = parser.parse_args(argv)

    def _maybe_pack_tre(output_dir: Path) -> dict[str, object] | None:
        if not getattr(args, "pack_tre", False):
            return None
        from swg_pipeline.pack_pipeline import pack_bundle

        packed = pack_bundle(output_dir, rebuild_rsp=False)
        return {
            "tre_files": [str(p) for p in packed.tre_paths],
        }

    def _apply_author(manifest_path: Path | None) -> None:
        author = getattr(args, "author", "") or ""
        if not author or manifest_path is None or not manifest_path.is_file():
            return
        data = json.loads(manifest_path.read_text(encoding="utf-8"))
        data["author"] = author
        manifest_path.write_text(json.dumps(data, indent=2), encoding="utf-8")

    try:
        if args.command == "phase7":
            result = export_phase7_validation_bundle(
                args.output_dir,
                copy_textures=args.copy_textures,
            )
            payload = result.as_dict()
            tre_info = _maybe_pack_tre(args.output_dir)
            if tre_info:
                payload.update(tre_info)
            print(json.dumps(payload, indent=2))
            return 0

        if args.command == "skeletal":
            result = export_skeletal_bundle_from_files(
                args.mgn,
                args.skt,
                args.output_dir,
                animation_sources=args.animation or None,
                shader_reference=args.shader_reference,
                copy_textures=not args.no_copy_textures,
                write_rsp=not args.no_rsp,
                write_client_cfg=not args.no_client_cfg,
            )
            _apply_author(result.manifest_path)
            payload = result.as_dict()
            tre_info = _maybe_pack_tre(args.output_dir)
            if tre_info:
                payload.update(tre_info)
            print(json.dumps(payload, indent=2))
            return 0

        if args.command == "static" or args.mesh is not None:
            if args.mesh is None or args.output_dir is None:
                parser.error("static export requires --mesh and --output-dir")
            overrides: dict[str, str] | None = None
            if args.main_texture:
                overrides = {"MAIN": args.main_texture}
            result = export_static_bundle_from_mesh_file(
                args.mesh,
                args.output_dir,
                mesh_relpath=args.mesh_relpath,
                shader_reference=args.shader_reference,
                shader_texture_overrides=overrides,
                copy_textures=not args.no_copy_textures,
                build_shaders_from_scene=args.build_shaders_from_scene,
                write_rsp=not args.no_rsp,
                write_client_cfg=not args.no_client_cfg,
            )
            _apply_author(result.manifest_path)
            payload = result.as_dict()
            tre_info = _maybe_pack_tre(args.output_dir)
            if tre_info:
                payload.update(tre_info)
            print(json.dumps(payload, indent=2))
            return 0

        parser.print_help()
        return 2
    except (OSError, ValueError, FileNotFoundError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    except PackError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())