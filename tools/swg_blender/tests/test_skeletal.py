"""Phase 3 skeletal IFF loaders — SKTM + SKMG."""

from __future__ import annotations

from pathlib import Path

import pytest

from swg_iff.reader import IffReader
from swg_iff.tags import TAG_SKMG, TAG_SKTM, tag_to_str
from swg_scene.mesh_skeletal import load_skeletal_mesh, load_skeletal_mesh_info
from swg_scene.skeleton import load_skeleton
from swg_pipeline.swg_main_paths import appearance_mesh, appearance_skeleton, swg_main_root

GOLDEN_SKT = "at_at.skt"
GOLDEN_MGN = "abyssin_m_arms_l0.mgn"


def _resolve_skt() -> Path:
    if (Path(__file__).parent / "golden" / GOLDEN_SKT).is_file():
        return Path(__file__).parent / "golden" / GOLDEN_SKT
    root = swg_main_root()
    if root:
        p = appearance_skeleton(GOLDEN_SKT)
        if p and p.is_file():
            return p
    pytest.skip("No skeleton: copy at_at.skt to tests/golden/ or set SWG_MAIN")


def _resolve_mgn() -> Path:
    if (Path(__file__).parent / "golden" / GOLDEN_MGN).is_file():
        return Path(__file__).parent / "golden" / GOLDEN_MGN
    root = swg_main_root()
    if root:
        p = appearance_mesh(GOLDEN_MGN)
        if p and p.is_file():
            return p
    pytest.skip("No mgn: copy abyssin_m_arms_l0.mgn to tests/golden/ or set SWG_MAIN")


def test_skt_is_valid_iff():
    path = _resolve_skt()
    data = path.read_bytes()
    reader = IffReader(data)
    reader.enter_form(TAG_SKTM)
    version = tag_to_str(reader.current_name())
    reader.exit_form(TAG_SKTM)
    assert version in ("0001", "0002")


def test_load_at_at_skeleton():
    skel = load_skeleton(_resolve_skt())
    assert len(skel.joints) >= 2
    assert skel.joints[0].name
    assert skel.joints[0].parent_index == -1 or skel.joints[0].parent_index >= -1


def test_load_abyssin_arms_mgn_header():
    info = load_skeletal_mesh_info(_resolve_mgn())
    assert info.version in ("0002", "0003", "0004")
    assert info.position_count == 96
    assert len(info.positions) == info.position_count
    assert len(info.normals) == info.normal_count
    assert info.transform_names


def test_load_abyssin_arms_mgn_skin_weights():
    info = load_skeletal_mesh_info(_resolve_mgn())
    assert len(info.transform_weight_counts) == info.position_count
    assert sum(info.transform_weight_counts) == info.transform_weight_data_count
    assert len(info.transform_weights) == info.transform_weight_data_count
    assert all(0 <= w.transform_index < len(info.transform_names) for w in info.transform_weights)
    assert all(w.weight >= 0.0 for w in info.transform_weights)
    observed_max = max(info.transform_weight_counts)
    if info.max_transforms_per_vertex > 0:
        assert observed_max <= info.max_transforms_per_vertex
    assert observed_max >= 1

    offset = 0
    for count in info.transform_weight_counts:
        vertex_weights = info.transform_weights[offset : offset + count]
        assert len(vertex_weights) == count
        assert abs(sum(w.weight for w in vertex_weights) - 1.0) < 0.02
        offset += count
    assert offset == len(info.transform_weights)


def test_load_abyssin_arms_mgn_full_mesh():
    scene = load_skeletal_mesh(_resolve_mgn())
    assert len(scene.meshes) == 1
    mesh = scene.meshes[0]
    assert len(mesh.positions) == 110
    assert len(mesh.bone_weights) == 110
    assert len(mesh.indices) // 3 == 154
    assert mesh.uvs and len(mesh.uvs[0]) == 110
    assert mesh.material.shader_relpath == "shader/abyssin_body.sht"
    assert mesh.transform_names
    assert mesh.skeleton_template_names


def test_import_skeletal_dry_run():
    from swg_blender.import_skeletal import load_mgn

    scene = load_mgn(_resolve_mgn(), blender_coords=False)
    assert scene.meshes[0].positions

def _resolve_all_b_slod() -> Path:
    golden = Path(__file__).parent / "golden" / "all_b.skt"
    if golden.is_file():
        return golden
    from swg_pipeline.swg_main_paths import appearance_skeleton

    live = appearance_skeleton("all_b.skt")
    if live:
        return live
    pytest.skip("No all_b.skt: copy to tests/golden/ or set SWG_MAIN")


def test_load_all_b_slod_skeleton_lod0():
    skel = load_skeleton(_resolve_all_b_slod(), lod_index=0)
    assert len(skel.joints) == 38
    assert skel.joints[0].name


def test_load_all_b_slod_skeleton_lod3():
    skel = load_skeleton(_resolve_all_b_slod(), lod_index=3)
    assert len(skel.joints) == 21


def test_load_mgn_auto_resolves_all_b_skeleton():
    from swg_blender.import_skeletal import load_mgn

    if not _resolve_all_b_slod().is_file():
        pytest.skip("golden all_b.skt missing")
    scene = load_mgn(_resolve_mgn(), skt_path=None, blender_coords=False)
    assert scene.skeleton is not None
    assert len(scene.skeleton.joints) == 38