"""Import SWG skeletal mesh (.mgn) + optional skeleton (.skt) into Blender."""

from __future__ import annotations

import math
import sys
from pathlib import Path
from typing import Any

from swg_scene.coords import engine_to_blender_position
from swg_scene.mesh_skeletal import load_skeletal_mesh
from swg_scene.model import SwgJoint, SwgMesh, SwgScene, SwgSkeleton
from swg_scene.skeleton import load_skeleton as load_skt_file

IMPORT_ROTATION_EULER = (math.pi / 2.0, 0.0, 0.0)


def _set_active_object(obj: Any) -> None:
    import bpy

    view = bpy.context.view_layer
    for o in view.objects:
        o.select_set(False)
    obj.select_set(True)
    view.objects.active = obj


def _viewport_override_kwargs(active_obj: Any) -> dict[str, Any]:
    import bpy

    view = bpy.context.view_layer
    window = bpy.context.window
    if window is None and bpy.context.window_manager.windows:
        window = bpy.context.window_manager.windows[0]

    area = None
    region = None
    if window is not None:
        for candidate in window.screen.areas:
            if candidate.type == "VIEW_3D":
                area = candidate
                for reg in candidate.regions:
                    if reg.type == "WINDOW":
                        region = reg
                        break
                break

    kwargs: dict[str, Any] = {
        "scene": bpy.context.scene,
        "view_layer": view,
        "active_object": active_obj,
        "object": active_obj,
        "selected_objects": [active_obj],
        "selected_editable_objects": [active_obj],
    }
    if window is not None:
        kwargs["window"] = window
        kwargs["screen"] = window.screen
    if area is not None:
        kwargs["area"] = area
    if region is not None:
        kwargs["region"] = region
    return kwargs


def _object_mode_set(mode: str, *, active_obj: Any) -> None:
    import bpy

    _set_active_object(active_obj)
    if not hasattr(bpy.context, "temp_override"):
        bpy.ops.object.mode_set(mode=mode)
        return
    kwargs = _viewport_override_kwargs(active_obj)
    if "area" not in kwargs or "region" not in kwargs:
        raise RuntimeError(
            "Cannot switch object mode without a 3D Viewport (Scripting-only layout). "
            "Split the 3D View or run from the Viewport's Scripting tab."
        )
    with bpy.context.temp_override(**kwargs):
        bpy.ops.object.mode_set(mode=mode)


def _ensure_repo_on_path() -> None:
    root = Path(__file__).resolve().parents[1]
    root_str = str(root)
    if root_str not in sys.path:
        sys.path.insert(0, root_str)


def _resolve_skeleton_path(template_relpath: str) -> Path | None:
    from swg_pipeline.swg_main_paths import appearance_skeleton

    name = Path(template_relpath.replace("\\", "/")).name
    path = appearance_skeleton(name)
    return path if path and path.is_file() else None


def _skeleton_from_skt(path: Path) -> SwgSkeleton | None:
    try:
        skel = load_skt_file(path)
    except Exception:
        return None
    return SwgSkeleton(
        joints=[
            SwgJoint(
                name=joint.name,
                parent_index=joint.parent_index,
                bind_translation=joint.bind_translation,
                bind_rotation=joint.bind_rotation,
                pre_rotation=joint.pre_rotation,
                post_rotation=joint.post_rotation,
            )
            for joint in skel.joints
        ]
    )


def _fallback_skeleton_from_mesh(mesh: SwgMesh) -> SwgSkeleton:
    return SwgSkeleton(
        joints=[SwgJoint(name=name, parent_index=-1) for name in mesh.transform_names]
    )


def load_mgn(
    path: str | Path,
    *,
    skt_path: str | Path | None = None,
    blender_coords: bool = True,
) -> SwgScene:
    _ensure_repo_on_path()
    scene = load_skeletal_mesh(path, blender_coords=blender_coords)
    if skt_path:
        skel = _skeleton_from_skt(Path(skt_path))
        if skel:
            scene.skeleton = skel
    elif scene.meshes and scene.meshes[0].skeleton_template_names:
        resolved = _resolve_skeleton_path(scene.meshes[0].skeleton_template_names[0])
        if resolved:
            skel = _skeleton_from_skt(resolved)
            if skel:
                scene.skeleton = skel
    if scene.skeleton is None and scene.meshes:
        scene.skeleton = _fallback_skeleton_from_mesh(scene.meshes[0])
    return scene


def _mesh_to_blender_bmesh(mesh: SwgMesh, *, name: str | None = None) -> Any:
    import bpy

    obj_name = name or mesh.name or "swg_skeletal_mesh"
    # from_pydata keeps duplicate triangles; bmesh.faces.new silently dedupes them.
    faces = [
        tuple(mesh.indices[i : i + 3])
        for i in range(0, len(mesh.indices), 3)
    ]
    me = bpy.data.meshes.new(obj_name)
    me.from_pydata(mesh.positions, [], faces)
    me.update()

    if mesh.uvs and mesh.uvs[0]:
        uv_layer = me.uv_layers.new(name="UVMap")
        for poly in me.polygons:
            for loop_idx in poly.loop_indices:
                vi = me.loops[loop_idx].vertex_index
                if vi < len(mesh.uvs[0]):
                    uv_layer.data[loop_idx].uv = mesh.uvs[0][vi]

    if mesh.normals and len(mesh.normals) == len(mesh.positions):
        try:
            me.use_auto_smooth = True
        except AttributeError:
            pass
        me.normals_split_custom_set_from_vertices(mesh.normals)
        for poly in me.polygons:
            poly.use_smooth = True

    obj = bpy.data.objects.new(obj_name, me)
    if mesh.material.shader_relpath:
        obj["swg_shader_path"] = mesh.material.shader_relpath
    if mesh.transform_names:
        obj["swg_transform_names"] = "|".join(mesh.transform_names)
    return obj


def _assign_vertex_groups(obj: Any, mesh: SwgMesh) -> None:
    for v_idx, weights in enumerate(mesh.bone_weights):
        for bone_index, weight in weights:
            if bone_index < 0 or bone_index >= len(mesh.transform_names):
                continue
            bone_name = mesh.transform_names[bone_index]
            group = obj.vertex_groups.get(bone_name)
            if group is None:
                group = obj.vertex_groups.new(name=bone_name)
            group.add([v_idx], weight, "ADD")


def _swg_quat_to_blender(quat: tuple[float, float, float, float]) -> Any:
    from mathutils import Quaternion

    return Quaternion((-quat[0], -quat[1], quat[2], quat[3]))


def _joint_local_translation(joint: SwgJoint) -> Any:
    from mathutils import Vector

    return Vector(engine_to_blender_position(*joint.bind_translation))


def _finalize_armature_import(
    arm_obj: Any, *, apply_import_rotation: bool, bake_import_rotation: bool
) -> None:
    """Apply +90 deg X on the armature object (io_scene also supports baking into bones)."""
    import bpy

    if not apply_import_rotation:
        return
    arm_obj.rotation_euler = IMPORT_ROTATION_EULER
    if not bake_import_rotation:
        return
    _object_mode_set("OBJECT", active_obj=arm_obj)
    kwargs = _viewport_override_kwargs(arm_obj)
    if "area" in kwargs and "region" in kwargs:
        with bpy.context.temp_override(**kwargs):
            bpy.ops.object.transform_apply(
                location=False, scale=False, rotation=True
            )


def skeleton_to_armature(
    skeleton: SwgSkeleton,
    *,
    name: str = "swg_armature",
    collection: Any,
    connect_bones: bool = False,
    apply_import_rotation: bool = True,
    bake_import_rotation: bool = False,
) -> Any:
    """Build rest pose from SKTM local BPTR/BPRO/RPRE/RPST chain (io_scene_swg_msh parity)."""
    import bpy
    from mathutils import Matrix, Vector

    arm_data = bpy.data.armatures.new(name)
    arm_obj = bpy.data.objects.new(name, arm_data)
    arm_data.display_type = "OCTAHEDRAL"
    arm_obj.show_in_front = True
    collection.objects.link(arm_obj)
    _set_active_object(arm_obj)
    kwargs = _viewport_override_kwargs(arm_obj)
    if "area" not in kwargs or "region" not in kwargs:
        raise RuntimeError(
            "Cannot build armature without a 3D Viewport. Use a layout with a 3D View."
        )

    bones: list[Any] = []

    def _build_edit_bones() -> None:
        edit_bones = arm_data.edit_bones
        bones.clear()
        for joint in skeleton.joints:
            bone = edit_bones.new(joint.name)
            bone.use_deform = True
            bone.use_inherit_rotation = True
            bone["RPRE"] = joint.pre_rotation
            bone["BPRO"] = joint.bind_rotation
            bone["RPST"] = joint.post_rotation
            bones.append(bone)

        def process_bone_hierarchy(
            bone_index: int, parent_transform: Matrix | None = None
        ) -> None:
            if bone_index < 0 or bone_index >= len(skeleton.joints):
                return
            if parent_transform is None:
                parent_transform = Matrix.Identity(4)

            joint = skeleton.joints[bone_index]
            bone = bones[bone_index]

            rpre = _swg_quat_to_blender(joint.pre_rotation)
            rbind = _swg_quat_to_blender(joint.bind_rotation)
            rpost = _swg_quat_to_blender(joint.post_rotation)
            local_translation = _joint_local_translation(joint)
            rotation_matrix = (rpost @ rbind @ rpre).to_matrix().to_4x4()
            local_transform = Matrix.Translation(local_translation) @ rotation_matrix
            world_transform = parent_transform @ local_transform

            bone.head = world_transform.translation
            bone.tail = bone.head + (
                Matrix.Translation(Vector((0.0, 0.0, 0.025))) @ rotation_matrix
            ).translation

            parent_id = joint.parent_index
            if parent_id >= 0:
                bone.parent = bones[parent_id]

            for child_index, child_joint in enumerate(skeleton.joints):
                if child_joint.parent_index == bone_index:
                    process_bone_hierarchy(child_index, world_transform)

        for index, joint in enumerate(skeleton.joints):
            if joint.parent_index < 0:
                process_bone_hierarchy(index)

        if connect_bones:
            for bone in bones:
                children = bone.children
                if children:
                    tail_pos = Vector((0.0, 0.0, 0.0))
                    for child in children:
                        tail_pos += child.head
                    tail_pos /= len(children)
                    bone.tail = tail_pos
                    if len(children) == 1:
                        children[0].use_connect = True
                elif bone.parent is not None:
                    delta = bone.head - bone.parent.head
                    if delta.length > 1e-6:
                        bone.tail = bone.head + delta.normalized() * delta.length * 0.5
                else:
                    bone.tail = bone.head + Vector((0.0, 0.0, 0.05))

    with bpy.context.temp_override(**kwargs):
        bpy.ops.object.mode_set(mode="EDIT")
        _build_edit_bones()
        bpy.ops.object.mode_set(mode="OBJECT")
    _finalize_armature_import(
        arm_obj,
        apply_import_rotation=apply_import_rotation,
        bake_import_rotation=bake_import_rotation,
    )
    return arm_obj


def import_skeletal_mesh(
    filepath: str | Path,
    *,
    skt_path: str | Path | None = None,
    collection: Any | None = None,
    object_name: str | None = None,
    apply_import_rotation: bool = True,
    existing_armature: Any | None = None,
) -> list[Any]:
    """Import one `.mgn` (and optional `.skt`); returns [armature, mesh, ...]."""
    import bpy

    scene = load_mgn(filepath, skt_path=skt_path, blender_coords=True)
    if collection is None:
        collection = bpy.context.collection

    objects: list[Any] = []
    arm_obj = existing_armature
    if arm_obj is None and scene.skeleton and scene.skeleton.joints:
        arm_obj = skeleton_to_armature(
            scene.skeleton,
            collection=collection,
            apply_import_rotation=apply_import_rotation,
            bake_import_rotation=False,
        )
        objects.append(arm_obj)
    elif arm_obj is not None and arm_obj not in objects:
        objects.append(arm_obj)

    stem = Path(filepath).stem
    for idx, mesh in enumerate(scene.meshes):
        if len(scene.meshes) == 1:
            name = object_name or mesh.name or stem
        else:
            name = mesh.name or f"{stem}_{idx}"
        obj = _mesh_to_blender_bmesh(mesh, name=name)
        if len(scene.meshes) > 1:
            obj["swg_submesh_index"] = idx
        _assign_vertex_groups(obj, mesh)
        if apply_import_rotation:
            obj.rotation_euler = IMPORT_ROTATION_EULER
        collection.objects.link(obj)
        if arm_obj is not None:
            mod = obj.modifiers.new("Armature", "ARMATURE")
            mod.object = arm_obj
        objects.append(obj)

    return objects


def main(argv: list[str] | None = None) -> int:
    import argparse

    parser = argparse.ArgumentParser(description="Import SWG skeletal mesh (.mgn)")
    parser.add_argument("mgn_path", type=Path)
    parser.add_argument("skt_path", type=Path, nargs="?", default=None)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)

    scene = load_mgn(args.mgn_path, skt_path=args.skt_path, blender_coords=not args.dry_run)
    for mesh in scene.meshes:
        tris = len(mesh.indices) // 3 if mesh.indices else 0
        print(
            f"{mesh.name}: {len(mesh.positions)} verts, {tris} tris, "
            f"bones={len(mesh.transform_names)}, shader={mesh.material.shader_relpath!r}"
        )
    if scene.skeleton:
        print(f"skeleton: {len(scene.skeleton.joints)} joints")
    if args.dry_run:
        return 0

    try:
        import bpy  # noqa: F401
    except ImportError:
        print("bpy not available - use --dry-run or run inside Blender", file=sys.stderr)
        return 1

    import_skeletal_mesh(args.mgn_path, skt_path=args.skt_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())