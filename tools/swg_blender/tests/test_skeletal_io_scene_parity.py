"""SKT hierarchy math parity with io_scene_swg_msh/import_skt.py (no bpy)."""

from __future__ import annotations

import pytest

from swg_pipeline.swg_main_paths import appearance_skeleton
from swg_scene.coords import engine_to_blender_position
from swg_scene.skeleton import load_skeleton


def _swg_quat_to_tuple(quat):
    return (-quat[0], -quat[1], quat[2], quat[3])


def _bone_heads_io_scene(skel):
    try:
        from mathutils import Matrix, Vector, Quaternion
    except ImportError:
        pytest.skip("mathutils only available inside Blender")

    joints = skel.joints
    heads = [None] * len(joints)

    def process(index, parent):
        joint = joints[index]
        rpre = Quaternion(_swg_quat_to_tuple(joint.pre_rotation))
        rbind = Quaternion(_swg_quat_to_tuple(joint.bind_rotation))
        rpost = Quaternion(_swg_quat_to_tuple(joint.post_rotation))
        t = joint.bind_translation
        local_t = Vector((-t[0], t[1], t[2]))
        rot_m = (rpost @ rbind @ rpre).to_matrix().to_4x4()
        local = Matrix.Translation(local_t) @ rot_m
        world = parent @ local
        heads[index] = tuple(world.translation)
        for child_i, child in enumerate(joints):
            if child.parent_index == index:
                process(child_i, world)

    ident = Matrix.Identity(4)
    for i, joint in enumerate(joints):
        if joint.parent_index < 0:
            process(i, ident)
    return heads


def _bone_heads_swg_blender(skel):
    try:
        from mathutils import Matrix, Vector, Quaternion
    except ImportError:
        pytest.skip("mathutils only available inside Blender")

    joints = skel.joints
    heads = [None] * len(joints)

    def process(index, parent=None):
        if parent is None:
            parent = Matrix.Identity(4)
        joint = joints[index]
        rpre = Quaternion(_swg_quat_to_tuple(joint.pre_rotation))
        rbind = Quaternion(_swg_quat_to_tuple(joint.bind_rotation))
        rpost = Quaternion(_swg_quat_to_tuple(joint.post_rotation))
        local_t = Vector(engine_to_blender_position(*joint.bind_translation))
        rot_m = (rpost @ rbind @ rpre).to_matrix().to_4x4()
        local = Matrix.Translation(local_t) @ rot_m
        world = parent @ local
        heads[index] = tuple(world.translation)
        for child_i, child in enumerate(joints):
            if child.parent_index == index:
                process(child_i, world)

    for i, joint in enumerate(joints):
        if joint.parent_index < 0:
            process(i)
    return heads


@pytest.mark.parametrize("bone_name", ["root", "pelvis", "chest", "lUpperArm", "lAnkle"])
def test_all_b_hierarchy_matches_io_scene(bone_name):
    path = appearance_skeleton("all_b.skt")
    if path is None:
        pytest.skip("no all_b.skt on swg-main")
    skel = load_skeleton(path)
    io_heads = _bone_heads_io_scene(skel)
    swg_heads = _bone_heads_swg_blender(skel)
    for i, joint in enumerate(skel.joints):
        if joint.name == bone_name:
            for a, b in zip(io_heads[i], swg_heads[i]):
                assert a == pytest.approx(b, abs=1e-5)
            return
    pytest.skip(f"bone {bone_name} not in skeleton")