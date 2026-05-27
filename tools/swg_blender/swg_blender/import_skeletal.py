"""Import SWG skeletal mesh (.mgn) + optional skeleton (.skt) into Blender."""

from __future__ import annotations

import math
import sys
from pathlib import Path
from typing import Any

from swg_scene.coords import engine_to_blender_position, engine_to_blender_vector
from swg_scene.mesh_skeletal import load_skeletal_mesh
from swg_scene.model import SwgJoint, SwgMesh, SwgScene, SwgSkeleton
from swg_scene.skeleton import load_skeleton as load_skt_file

IMPORT_ROTATION_EULER = (math.pi / 2.0, 0.0, 0.0)


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
    import bmesh
    import bpy

    obj_name = name or mesh.name or "swg_skeletal_mesh"
    bm = bmesh.new()
    vert_map = [bm.verts.new(v) for v in mesh.positions]
    bm.verts.ensure_lookup_table()
    bm.verts.index_update()

    if mesh.indices:
        for i in range(0, len(mesh.indices), 3):
            try:
                bm.faces.new([vert_map[mesh.indices[i + j]] for j in range(3)])
            except ValueError:
                pass
    bm.faces.ensure_lookup_table()

    if mesh.uvs and mesh.uvs[0]:
        uv_layer = bm.loops.layers.uv.new("UVMap")
        for face in bm.faces:
            for loop in face.loops:
                vi = loop.vert.index
                if vi < len(mesh.uvs[0]):
                    loop[uv_layer].uv = mesh.uvs[0][vi]

    me = bpy.data.meshes.new(obj_name)
    bm.to_mesh(me)
    bm.free()

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


def _joint_blender_head(joint: SwgJoint) -> tuple[float, float, float]:
    return engine_to_blender_position(*joint.bind_translation)


def _bone_tail(joint: SwgJoint, joints: list[SwgJoint]) -> tuple[float, float, float]:
    head = _joint_blender_head(joint)
    for child in joints:
        if child.parent_index == joints.index(joint):
            return _joint_blender_head(child)
    return (head[0], head[1] + 0.05, head[2])


def skeleton_to_armature(
    skeleton: SwgSkeleton,
    *,
    name: str = "swg_armature",
    collection: Any,
) -> Any:
    import bpy

    arm_data = bpy.data.armatures.new(name)
    arm_obj = bpy.data.objects.new(name, arm_data)
    collection.objects.link(arm_obj)

    bpy.context.view_layer.objects.active = arm_obj
    bpy.ops.object.mode_set(mode="EDIT")
    edit_bones = arm_data.edit_bones
    name_to_bone: dict[str, Any] = {}

    for idx, joint in enumerate(skeleton.joints):
        bone = edit_bones.new(joint.name)
        bone.head = _joint_blender_head(joint)
        bone.tail = _bone_tail(joint, skeleton.joints)
        name_to_bone[joint.name] = bone

    for idx, joint in enumerate(skeleton.joints):
        if joint.parent_index >= 0 and joint.parent_index < len(skeleton.joints):
            parent = skeleton.joints[joint.parent_index]
            name_to_bone[joint.name].parent = name_to_bone[parent.name]

    bpy.ops.object.mode_set(mode="OBJECT")
    return arm_obj


def import_skeletal_mesh(
    filepath: str | Path,
    *,
    skt_path: str | Path | None = None,
    collection: Any | None = None,
    object_name: str | None = None,
    apply_import_rotation: bool = True,
) -> list[Any]:
    """Import one `.mgn` (and optional `.skt`); returns [armature, mesh, ...]."""
    import bpy

    scene = load_mgn(filepath, skt_path=skt_path, blender_coords=True)
    if collection is None:
        collection = bpy.context.collection

    objects: list[Any] = []
    arm_obj = None
    if scene.skeleton and scene.skeleton.joints:
        arm_obj = skeleton_to_armature(scene.skeleton, collection=collection)
        if apply_import_rotation:
            arm_obj.rotation_euler = IMPORT_ROTATION_EULER
        objects.append(arm_obj)

    stem = Path(filepath).stem
    for idx, mesh in enumerate(scene.meshes):
        name = object_name or (mesh.name if len(scene.meshes) == 1 else f"{stem}_{idx}")
        obj = _mesh_to_blender_bmesh(mesh, name=name)
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