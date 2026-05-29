"""Round-trip tests for skeletal IFF export."""

from __future__ import annotations

from pathlib import Path

from swg_scene.mesh_skeletal import load_skeletal_mesh, load_skeletal_mesh_info
from swg_scene.mesh_skeletal_export import write_skeletal_mesh, write_skeletal_mesh_file
from swg_scene.skeleton import load_skeleton
from swg_scene.skeleton_export import write_skeleton_file
from tests.test_skeletal import _resolve_mgn, _resolve_skt


def test_mgn_roundtrip_topology(tmp_path: Path):
    original = load_skeletal_mesh(_resolve_mgn())
    mesh_a = original.meshes[0]
    out = tmp_path / "roundtrip.mgn"
    write_skeletal_mesh_file(out, original)
    roundtrip = load_skeletal_mesh(out)
    mesh_b = roundtrip.meshes[0]

    assert len(mesh_b.positions) == len(mesh_a.positions)
    assert len(mesh_b.indices) == len(mesh_a.indices)
    assert len(mesh_b.bone_weights) == len(mesh_a.bone_weights)
    assert mesh_b.material.shader_relpath == mesh_a.material.shader_relpath
    assert mesh_b.transform_names == mesh_a.transform_names


def test_mgn_roundtrip_preserves_bind_pose_pool(tmp_path: Path):
    """Golden SKMG uses a shared POSN pool smaller than expanded shader verts."""
    path = _resolve_mgn()
    before_info = load_skeletal_mesh_info(path)
    scene = load_skeletal_mesh(path)
    mesh_a = scene.meshes[0]

    assert before_info.position_count < len(mesh_a.positions)

    out = tmp_path / "roundtrip.mgn"
    write_skeletal_mesh_file(out, scene)
    after_info = load_skeletal_mesh_info(out)
    roundtrip = load_skeletal_mesh(out)
    mesh_b = roundtrip.meshes[0]

    assert after_info.position_count == before_info.position_count
    assert after_info.normal_count == before_info.normal_count
    assert after_info.transform_weight_data_count == before_info.transform_weight_data_count
    assert len(mesh_b.positions) == len(mesh_a.positions)
    assert len(mesh_b.indices) == len(mesh_a.indices)


GOLDEN_MULTI_MGN = "abyssin_m_l0.mgn"


def _resolve_multi_shader_mgn() -> Path:
    if (Path(__file__).parent / "golden" / GOLDEN_MULTI_MGN).is_file():
        return Path(__file__).parent / "golden" / GOLDEN_MULTI_MGN
    from swg_pipeline.swg_main_paths import appearance_mesh, swg_main_root

    root = swg_main_root()
    if root:
        found = appearance_mesh(GOLDEN_MULTI_MGN)
        if found and found.is_file():
            return found
    pytest.skip("No mgn: copy abyssin_m_l0.mgn to tests/golden/ or set SWG_MAIN")


def test_multi_shader_mgn_roundtrip_topology(tmp_path: Path):
    original = load_skeletal_mesh(_resolve_multi_shader_mgn())
    assert len(original.meshes) >= 2
    out = tmp_path / "roundtrip.mgn"
    write_skeletal_mesh_file(out, original)
    roundtrip = load_skeletal_mesh(out)
    assert len(roundtrip.meshes) == len(original.meshes)
    for before, after in zip(original.meshes, roundtrip.meshes, strict=True):
        assert len(before.positions) == len(after.positions)
        assert len(before.indices) == len(after.indices)
        assert before.material.shader_relpath == after.material.shader_relpath


def test_skt_roundtrip_joints(tmp_path: Path):
    original = load_skeleton(_resolve_skt())
    out = tmp_path / "roundtrip.skt"
    write_skeleton_file(out, original, version=original.version)
    roundtrip = load_skeleton(out)
    assert len(roundtrip.joints) == len(original.joints)
    assert [j.name for j in roundtrip.joints] == [j.name for j in original.joints]
    assert [j.parent_index for j in roundtrip.joints] == [j.parent_index for j in original.joints]