"""Tests for Blender-to-IR skeletal export helpers (no bpy required)."""

from __future__ import annotations

from types import SimpleNamespace

import pytest

from swg_blender.export_skeletal import (
    _mesh_bone_weights,
    armature_object_to_swg_skeleton,
    mesh_object_to_swg_mesh,
)
from swg_scene.coords import blender_to_engine_position


def _vec(x: float, y: float, z: float) -> SimpleNamespace:
    return SimpleNamespace(x=x, y=y, z=z)


def test_mesh_object_to_swg_mesh_extracts_skin_weights():
    mesh = SimpleNamespace(
        vertices=[
            SimpleNamespace(co=_vec(1.0, 2.0, 3.0), normal=_vec(0.0, 0.0, 1.0)),
            SimpleNamespace(co=_vec(4.0, 5.0, 6.0), normal=_vec(0.0, 1.0, 0.0)),
        ],
        polygons=[
            SimpleNamespace(vertices=(0, 1, 0), use_smooth=True),
        ],
        uv_layers=SimpleNamespace(active=None),
        loops=[],
    )

    def weight_a(idx: int) -> float:
        return 0.75 if idx == 0 else 0.0

    group_a = SimpleNamespace(name="bone_a", weight=weight_a)
    def weight_b(idx: int) -> float:
        return 0.25 if idx == 0 else 0.0

    group_b = SimpleNamespace(name="bone_b", weight=weight_b)

    obj = SimpleNamespace(
        name="test_mesh",
        data=mesh,
        vertex_groups=[group_a, group_b],
        modifiers=[],
        get=lambda key, default=None: default,
    )

    swg_mesh = mesh_object_to_swg_mesh(
        obj,
        transform_names=["bone_a", "bone_b"],
        skeleton_template_names=["appearance/skeleton/test.skt"],
    )

    assert swg_mesh.positions[0] == blender_to_engine_position(1.0, 2.0, 3.0)
    assert swg_mesh.transform_names == ["bone_a", "bone_b"]
    assert swg_mesh.skeleton_template_names == ["appearance/skeleton/test.skt"]
    assert swg_mesh.bone_weights[0] == [(0, 0.75), (1, 0.25)]
    assert swg_mesh.bone_weights[1] == []


def test_mesh_object_to_swg_mesh_uses_armature_modifier():
    arm_bones = [SimpleNamespace(name="root"), SimpleNamespace(name="child")]
    arm_data = SimpleNamespace(bones=arm_bones)
    arm_obj = SimpleNamespace(data=arm_data, get=lambda key, default=None: default)

    mesh = SimpleNamespace(
        vertices=[SimpleNamespace(co=_vec(0.0, 0.0, 0.0), normal=_vec(0.0, 0.0, 1.0))],
        polygons=[SimpleNamespace(vertices=(0, 0, 0), use_smooth=True)],
        uv_layers=SimpleNamespace(active=None),
        loops=[],
    )
    group = SimpleNamespace(name="root", weight=lambda _idx, _self=None: 1.0)
    mod = SimpleNamespace(type="ARMATURE", object=arm_obj)
    obj = SimpleNamespace(
        name="skinned",
        data=mesh,
        vertex_groups=[group],
        modifiers=[mod],
        get=lambda key, default=None: "shader/test.sht" if key == "swg_shader_path" else default,
    )

    swg_mesh = mesh_object_to_swg_mesh(obj)
    assert swg_mesh.transform_names == ["root", "child"]
    assert swg_mesh.material.shader_relpath == "shader/test.sht"
    assert swg_mesh.bone_weights == [[(0, 1.0)]]


def test_armature_object_to_swg_skeleton_parent_indices():
    parent = SimpleNamespace(name="parent", parent=None, head_local=_vec(0.0, 0.0, 0.0))
    child = SimpleNamespace(name="child", parent=parent, head_local=_vec(0.0, 0.05, 0.0))
    arm_data = SimpleNamespace(bones=[parent, child])
    arm_obj = SimpleNamespace(data=arm_data)

    skel = armature_object_to_swg_skeleton(arm_obj)
    assert [joint.name for joint in skel.joints] == ["parent", "child"]
    assert skel.joints[0].parent_index == -1
    assert skel.joints[1].parent_index == 0
    assert skel.joints[1].bind_translation == blender_to_engine_position(0.0, 0.05, 0.0)


def test_mesh_bone_weights_ignores_unknown_groups():
    mesh = SimpleNamespace(vertices=[SimpleNamespace(), SimpleNamespace()])
    known = SimpleNamespace(name="known", weight=lambda idx, _self=None: 1.0 if idx == 0 else 0.0)
    unknown = SimpleNamespace(name="unknown", weight=lambda _idx, _self=None: 1.0)
    obj = SimpleNamespace(data=mesh, vertex_groups=[known, unknown])

    weights = _mesh_bone_weights(obj, ["known"])
    assert weights == [[(0, 1.0)], []]


def test_mesh_object_to_swg_mesh_requires_skinning():
    mesh = SimpleNamespace(
        vertices=[SimpleNamespace(co=_vec(0.0, 0.0, 0.0), normal=_vec(0.0, 0.0, 1.0))],
        polygons=[SimpleNamespace(vertices=(0, 0, 0), use_smooth=True)],
        uv_layers=SimpleNamespace(active=None),
        loops=[],
    )
    obj = SimpleNamespace(
        name="bare",
        data=mesh,
        vertex_groups=[],
        modifiers=[],
        get=lambda key, default=None: default,
    )
    with pytest.raises(ValueError, match="no armature or vertex groups"):
        mesh_object_to_swg_mesh(obj)