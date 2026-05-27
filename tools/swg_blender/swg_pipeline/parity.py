"""Static mesh parity / byte-compare harness (Phase 8.10)."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

from swg_iff.reader import IffReader
from swg_iff.tags import TAG_APPR, TAG_0005, TAG_MESH
from swg_scene.mesh_static import load_static_mesh
from swg_scene.mesh_static_export import write_static_mesh
from swg_scene.model import SwgScene


@dataclass
class ParityDiff:
    ok: bool
    errors: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
    stats: dict[str, object] = field(default_factory=dict)


def _appearance_bytes_from_msh(data: bytes) -> bytes | None:
    reader = IffReader(data)
    reader.enter_form(TAG_MESH)
    try:
        reader.enter_form(TAG_0005)
        try:
            while not reader.at_end_of_form():
                block = reader._peek_block()
                if block.is_form and block.form_type == TAG_APPR:
                    return reader.read_raw_block()
                reader.skip_block()
        finally:
            reader.exit_form(TAG_0005)
    finally:
        reader.exit_form(TAG_MESH)
    return None


def compare_static_mesh_bytes(reference: bytes, candidate: bytes, *, compare_appearance: bool = True) -> ParityDiff:
    diff = ParityDiff(ok=True)
    if compare_appearance:
        ref_appr = _appearance_bytes_from_msh(reference)
        cand_appr = _appearance_bytes_from_msh(candidate)
        if ref_appr != cand_appr:
            diff.ok = False
            diff.errors.append(
                f"APPR bytes differ (ref={len(ref_appr or b'')} cand={len(cand_appr or b'')})"
            )
    if reference == candidate:
        diff.stats["byte_identical"] = True
    else:
        diff.stats["byte_identical"] = False
        diff.warnings.append(
            f"full file size ref={len(reference)} cand={len(candidate)}"
        )
    return diff


def compare_scenes(reference: SwgScene, candidate: SwgScene) -> ParityDiff:
    diff = ParityDiff(ok=True)
    if len(reference.meshes) != len(candidate.meshes):
        diff.ok = False
        diff.errors.append(
            f"shader group count {len(reference.meshes)} != {len(candidate.meshes)}"
        )
        return diff

    ref_verts = 0
    ref_tris = 0
    for index, (before, after) in enumerate(zip(reference.meshes, candidate.meshes, strict=True)):
        prefix = f"mesh[{index}]"
        if before.material.shader_relpath != after.material.shader_relpath:
            diff.warnings.append(
                f"{prefix}: shader path {before.material.shader_relpath!r} != {after.material.shader_relpath!r}"
            )
        if len(before.positions) != len(after.positions):
            diff.ok = False
            diff.errors.append(
                f"{prefix}: vertex count {len(before.positions)} != {len(after.positions)}"
            )
        if len(before.indices) != len(after.indices):
            diff.ok = False
            diff.errors.append(
                f"{prefix}: index count {len(before.indices)} != {len(after.indices)}"
            )
        elif before.indices != after.indices:
            diff.warnings.append(f"{prefix}: triangle index order differs")
        ref_verts += len(before.positions)
        ref_tris += len(before.indices) // 3

    diff.stats.update(
        {
            "mesh_groups": len(reference.meshes),
            "vertices": ref_verts,
            "triangles": ref_tris,
        }
    )
    return diff


def compare_scene_geometry(
    reference: SwgScene,
    candidate: SwgScene,
    *,
    tolerance: float = 1e-4,
) -> ParityDiff:
    """Topology + per-vertex positions (engine space) within tolerance."""
    diff = compare_scenes(reference, candidate)
    if not diff.ok:
        return diff

    for index, (before, after) in enumerate(
        zip(reference.meshes, candidate.meshes, strict=True)
    ):
        prefix = f"mesh[{index}]"
        for vi, (p0, p1) in enumerate(zip(before.positions, after.positions, strict=True)):
            if any(abs(p0[k] - p1[k]) > tolerance for k in range(3)):
                diff.ok = False
                diff.errors.append(f"{prefix}: vertex {vi} position delta > {tolerance}")
                break

    ref_skel = reference.skeleton
    cand_skel = candidate.skeleton
    if (ref_skel is None) != (cand_skel is None):
        diff.ok = False
        diff.errors.append("skeleton presence mismatch")
        return diff
    if ref_skel and cand_skel:
        if len(ref_skel.joints) != len(cand_skel.joints):
            diff.ok = False
            diff.errors.append(
                f"joint count {len(ref_skel.joints)} != {len(cand_skel.joints)}"
            )
        else:
            for ji, (j0, j1) in enumerate(
                zip(ref_skel.joints, cand_skel.joints, strict=True)
            ):
                if j0.name != j1.name:
                    diff.warnings.append(f"joint[{ji}]: name {j0.name!r} != {j1.name!r}")
                t0, t1 = j0.bind_translation, j1.bind_translation
                if any(abs(t0[k] - t1[k]) > tolerance for k in range(3)):
                    diff.ok = False
                    diff.errors.append(f"joint[{ji}]: bind_translation delta > {tolerance}")
                    break

    return diff


def roundtrip_scene(mesh_path: str | Path, *, rebuild_appearance: bool = False, optimize_indices: bool = False) -> SwgScene:
    original = load_static_mesh(mesh_path)
    original.rebuild_appearance = rebuild_appearance
    original.optimize_indices = optimize_indices
    exported = write_static_mesh(original)
    from swg_pipeline.validate import load_static_mesh_from_bytes

    return load_static_mesh_from_bytes(exported)


def parity_report(mesh_path: str | Path, *, preserve_appearance: bool = True) -> ParityDiff:
    path = Path(mesh_path)
    original = load_static_mesh(path)
    original.rebuild_appearance = not preserve_appearance
    original.optimize_indices = False
    reference_bytes = path.read_bytes()
    exported_bytes = write_static_mesh(original)

    byte_diff = compare_static_mesh_bytes(reference_bytes, exported_bytes, compare_appearance=preserve_appearance)
    roundtrip = load_static_mesh(path)
    roundtrip.rebuild_appearance = not preserve_appearance
    roundtrip.optimize_indices = False
    from swg_pipeline.validate import load_static_mesh_from_bytes

    candidate = load_static_mesh_from_bytes(exported_bytes)
    semantic = compare_scenes(roundtrip, candidate)
    return ParityDiff(
        ok=byte_diff.ok and semantic.ok,
        errors=byte_diff.errors + semantic.errors,
        warnings=byte_diff.warnings + semantic.warnings,
        stats={**byte_diff.stats, **semantic.stats},
    )
