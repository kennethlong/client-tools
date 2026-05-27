"""Validate SWG static mesh and shader template files."""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass, field
from pathlib import Path

from swg_iff.reader import IffError, root_form_type, validate_iff
from swg_iff.tags import TAG_CSHD, TAG_MESH, TAG_OPST, TAG_SSHT, TAG_SWTS, tag_to_str
from swg_scene.mesh_static import load_static_mesh
from swg_scene.mesh_static_export import write_static_mesh


@dataclass
class ValidationResult:
    ok: bool
    errors: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
    stats: dict[str, object] = field(default_factory=dict)

    def merge(self, other: ValidationResult) -> ValidationResult:
        return ValidationResult(
            ok=self.ok and other.ok,
            errors=self.errors + other.errors,
            warnings=self.warnings + other.warnings,
            stats={**self.stats, **other.stats},
        )


def validate_static_mesh(path: str | Path) -> ValidationResult:
    """Structural + loader validation for a `.msh` file."""
    p = Path(path)
    result = ValidationResult(ok=True)

    if not p.is_file():
        return ValidationResult(ok=False, errors=[f"file not found: {p}"])

    data = p.read_bytes()
    if not data:
        return ValidationResult(ok=False, errors=[f"empty file: {p}"])

    if not validate_iff(data):
        return ValidationResult(ok=False, errors=[f"invalid IFF structure: {p}"])

    root = root_form_type(data)
    if root != TAG_MESH:
        result.ok = False
        result.errors.append(
            f"expected root form MESH, got {tag_to_str(root)!r}"
        )
        return result

    try:
        scene = load_static_mesh(p)
    except (IffError, ValueError, OSError) as exc:
        return ValidationResult(ok=False, errors=[f"load failed: {exc}"])

    if not scene.meshes:
        result.ok = False
        result.errors.append("MESH loaded but contains no mesh groups")
        return result

    total_verts = 0
    total_tris = 0
    for index, mesh in enumerate(scene.meshes):
        prefix = f"mesh[{index}]"
        if not mesh.positions:
            result.ok = False
            result.errors.append(f"{prefix}: no positions")
            continue
        if len(mesh.normals) not in (0, len(mesh.positions)):
            result.ok = False
            result.errors.append(
                f"{prefix}: normal count {len(mesh.normals)} != "
                f"position count {len(mesh.positions)}"
            )
        if mesh.indices and len(mesh.indices) % 3 != 0:
            result.ok = False
            result.errors.append(
                f"{prefix}: index count {len(mesh.indices)} is not a multiple of 3"
            )
        if mesh.indices and len(mesh.indices) > 65535:
            result.ok = False
            result.errors.append(
                f"{prefix}: index count {len(mesh.indices)} exceeds uint16 shader limit 65535"
            )
        if mesh.indices and max(mesh.indices) >= len(mesh.positions):
            result.ok = False
            result.errors.append(f"{prefix}: index references out-of-range vertex")
        if not mesh.material.shader_relpath:
            result.warnings.append(f"{prefix}: missing shader_relpath")
        total_verts += len(mesh.positions)
        total_tris += len(mesh.indices) // 3 if mesh.indices else 0

    result.stats.update(
        {
            "mesh_groups": len(scene.meshes),
            "vertices": total_verts,
            "triangles": total_tris,
            "shader_paths": [m.material.shader_relpath for m in scene.meshes],
            "has_appearance_raw": scene.appearance_raw is not None,
        }
    )
    return result


_SHADER_ROOTS = {
    TAG_SSHT: "SSHT",
    TAG_CSHD: "CSHD",
    TAG_SWTS: "SWTS",
    TAG_OPST: "OPST",
}


def validate_shader_template(path: str | Path) -> ValidationResult:
    """Structural validation for SSHT static shader templates."""
    return validate_shader_file(path, expected_root="SSHT")


def validate_shader_file(
    path: str | Path,
    *,
    expected_root: str | None = None,
) -> ValidationResult:
    """Structural validation for any supported `.sht` shader template root."""
    p = Path(path)
    if not p.is_file():
        return ValidationResult(ok=False, errors=[f"file not found: {p}"])

    data = p.read_bytes()
    if not validate_iff(data):
        return ValidationResult(ok=False, errors=[f"invalid IFF structure: {p}"])

    root = root_form_type(data)
    root_name = _SHADER_ROOTS.get(root or 0)
    if root_name is None:
        return ValidationResult(
            ok=False,
            errors=[f"unsupported shader root form {tag_to_str(root or 0)!r}"],
        )
    if expected_root is not None and root_name != expected_root:
        return ValidationResult(
            ok=False,
            errors=[f"expected root form {expected_root}, got {root_name!r}"],
        )

    stats: dict[str, object] = {"shader_path": str(p), "shader_root": root_name}
    if root_name == "SSHT":
        text = data.decode("latin-1", errors="ignore")
        if "TXMS" not in text:
            return ValidationResult(ok=False, errors=["SSHT missing TXMS texture block"])
    else:
        try:
            from swg_pipeline.shader_extended import load_shader_spec

            load_shader_spec(p)
            stats["loaded"] = True
        except Exception as exc:
            return ValidationResult(ok=False, errors=[f"shader load failed: {exc}"])

    return ValidationResult(ok=True, stats=stats)


def validate_static_mesh_with_shader(
    mesh_path: str | Path,
    *,
    serverdata: Path | None = None,
) -> ValidationResult:
    """Validate mesh and resolve each referenced `.sht` under serverdata/shader/."""
    from swg_pipeline.swg_main_paths import serverdata_dir, shader_template

    mesh_result = validate_static_mesh(mesh_path)
    if not mesh_result.ok:
        return mesh_result

    sd = serverdata or serverdata_dir()
    if sd is None:
        mesh_result.warnings.append(
            "SWG_MAIN/serverdata not configured — skipping shader file checks"
        )
        return mesh_result

    combined = mesh_result
    for shader_relpath in mesh_result.stats.get("shader_paths", []):
        if not shader_relpath:
            continue
        shader_path = shader_template(str(shader_relpath))
        if shader_path is None:
            combined.ok = False
            combined.errors.append(
                f"shader not found under serverdata: {shader_relpath}"
            )
            continue
        shader_result = validate_shader_template(shader_path)
        combined = combined.merge(shader_result)

    return combined


def validate_roundtrip(mesh_path: str | Path) -> ValidationResult:
    """Load → export → reload and compare topology."""
    p = Path(mesh_path)
    try:
        original = load_static_mesh(p)
        exported = write_static_mesh(original)
        roundtrip = load_static_mesh_from_bytes(exported)
    except (IffError, ValueError, OSError) as exc:
        return ValidationResult(ok=False, errors=[f"roundtrip failed: {exc}"])

    if len(original.meshes) != len(roundtrip.meshes):
        return ValidationResult(
            ok=False,
            errors=[
                f"mesh group count changed: {len(original.meshes)} -> "
                f"{len(roundtrip.meshes)}"
            ],
        )

    for index, (before, after) in enumerate(
        zip(original.meshes, roundtrip.meshes, strict=True)
    ):
        if len(before.positions) != len(after.positions):
            return ValidationResult(
                ok=False,
                errors=[f"mesh[{index}] vertex count changed"],
            )
        if before.indices != after.indices:
            return ValidationResult(
                ok=False,
                errors=[f"mesh[{index}] indices changed"],
            )

    return ValidationResult(
        ok=True,
        stats={
            "vertices": sum(len(m.positions) for m in original.meshes),
            "triangles": sum(len(m.indices) // 3 for m in original.meshes),
        },
    )


def load_static_mesh_from_bytes(data: bytes):
    from swg_iff.reader import IffReader
    from swg_scene.mesh_static import load_static_mesh_from_reader

    return load_static_mesh_from_reader(IffReader(data))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Validate SWG static mesh / shader files")
    parser.add_argument("paths", nargs="+", type=Path, help="`.msh` or `.sht` files")
    parser.add_argument(
        "--with-shader",
        action="store_true",
        help="for `.msh`, also resolve and validate referenced `.sht` files",
    )
    parser.add_argument(
        "--roundtrip",
        action="store_true",
        help="for `.msh`, run load→export→reload topology check",
    )
    args = parser.parse_args(argv)

    exit_code = 0
    for path in args.paths:
        suffix = path.suffix.lower()
        if suffix == ".msh":
            if args.roundtrip:
                result = validate_roundtrip(path)
            elif args.with_shader:
                result = validate_static_mesh_with_shader(path)
            else:
                result = validate_static_mesh(path)
        elif suffix == ".sht":
            result = validate_shader_file(path)
        else:
            print(f"{path}: unknown extension (expected .msh or .sht)", file=sys.stderr)
            exit_code = 1
            continue

        status = "OK" if result.ok else "FAIL"
        print(f"{path}: {status}")
        for key, value in sorted(result.stats.items()):
            print(f"  {key}: {value}")
        for warning in result.warnings:
            print(f"  WARN: {warning}")
        for error in result.errors:
            print(f"  ERROR: {error}", file=sys.stderr)
        if not result.ok:
            exit_code = 1

    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())