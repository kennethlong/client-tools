"""Export SWG skeletal mesh (.mgn) and skeleton (.skt) from Blender or scene IR."""

from __future__ import annotations

import sys
from pathlib import Path
from typing import Any

from swg_blender.bpy_geometry import armature_bind_positions_engine
from swg_blender.export_static import mesh_object_to_swg_mesh as _mesh_geometry_from_object
from swg_scene.mesh_skeletal import load_skeletal_mesh
from swg_scene.mesh_skeletal_export import write_skeletal_mesh_file
from swg_scene.model import SwgJoint, SwgMaterialRef, SwgMesh, SwgScene, SwgSkeleton
from swg_scene.skeleton import load_skeleton
from swg_scene.skeleton_export import write_skeleton_file
from swg_blender.shape_keys import collect_blend_targets_from_object


def export_mgn_roundtrip_source(path: str | Path, output: str | Path) -> None:
    scene = load_skeletal_mesh(path)
    write_skeletal_mesh_file(output, scene)


def export_skt_roundtrip_source(path: str | Path, output: str | Path) -> None:
    skel = load_skeleton(path)
    write_skeleton_file(output, skel, version=skel.version or "0002")


def _normalize_bone_weights(weights: list[tuple[int, float]]) -> list[tuple[int, float]]:
    filtered = [(index, weight) for index, weight in weights if weight > 1e-6]
    return sorted(filtered, key=lambda item: item[0])


def _find_armature_object(mesh_obj: Any, armature: Any | None = None) -> Any | None:
    if armature is not None:
        return armature
    for mod in mesh_obj.modifiers:
        if mod.type == "ARMATURE" and mod.object is not None:
            return mod.object
    return None


def _armature_transform_names(arm_obj: Any) -> list[str]:
    return [bone.name for bone in arm_obj.data.bones]


def _mesh_bone_weights(obj: Any, transform_names: list[str]) -> list[list[tuple[int, float]]]:
    name_to_index = {name: index for index, name in enumerate(transform_names)}
    mesh = obj.data
    weights_per_vertex: list[list[tuple[int, float]]] = []
    for vertex_index in range(len(mesh.vertices)):
        weights: list[tuple[int, float]] = []
        for group in obj.vertex_groups:
            bone_index = name_to_index.get(group.name)
            if bone_index is None:
                continue
            try:
                weight = group.weight(vertex_index)
            except RuntimeError:
                weight = 0.0
            if weight > 1e-6:
                weights.append((bone_index, weight))
        weights_per_vertex.append(_normalize_bone_weights(weights))
    return weights_per_vertex


def _resolve_skeleton_template_names(
    obj: Any,
    arm_obj: Any | None,
    skeleton_template_names: list[str] | None,
) -> list[str]:
    if skeleton_template_names is not None:
        return list(skeleton_template_names)
    for source in (obj, arm_obj):
        if source is None:
            continue
        template = source.get("swg_skeleton_template")
        if template:
            return [str(template)]
    return []


def mesh_object_to_swg_mesh(
    obj: Any,
    *,
    armature: Any | None = None,
    transform_names: list[str] | None = None,
    skeleton_template_names: list[str] | None = None,
) -> SwgMesh:
    """Convert one skinned Blender mesh object to SwgMesh (requires bpy mesh data)."""
    base = _mesh_geometry_from_object(obj)
    arm_obj = _find_armature_object(obj, armature)

    if transform_names is None:
        stored = obj.get("swg_transform_names")
        if stored:
            if isinstance(stored, str):
                transform_names = [name for name in stored.split("|") if name]
            else:
                transform_names = list(stored)
        elif arm_obj is not None:
            transform_names = _armature_transform_names(arm_obj)
        elif obj.vertex_groups:
            transform_names = [group.name for group in obj.vertex_groups]
        else:
            transform_names = []

    if not transform_names:
        raise ValueError(f"mesh {obj.name!r} has no armature or vertex groups for skinning")

    bone_weights = _mesh_bone_weights(obj, transform_names)
    skel_templates = _resolve_skeleton_template_names(obj, arm_obj, skeleton_template_names)

    return SwgMesh(
        name=base.name,
        positions=base.positions,
        normals=base.normals,
        uvs=base.uvs,
        indices=base.indices,
        material=SwgMaterialRef(shader_relpath=base.material.shader_relpath),
        transform_names=list(transform_names),
        skeleton_template_names=skel_templates,
        bone_weights=bone_weights,
    )


def armature_object_to_swg_skeleton(arm_obj: Any) -> SwgSkeleton:
    """Convert a Blender armature rest pose to SwgSkeleton."""
    bones = list(arm_obj.data.bones)
    name_to_index = {bone.name: index for index, bone in enumerate(bones)}
    joints: list[SwgJoint] = []
    bind_positions = armature_bind_positions_engine(arm_obj)
    for bone, bind_translation in zip(bones, bind_positions, strict=True):
        parent_index = -1
        if bone.parent is not None:
            parent_index = name_to_index[bone.parent.name]
        joints.append(
            SwgJoint(
                name=bone.name,
                parent_index=parent_index,
                bind_translation=bind_translation,
                bind_rotation=(0.0, 0.0, 0.0, 1.0),
            )
        )
    return SwgSkeleton(joints=joints)


def export_skeletal_mesh(
    filepath: str | Path,
    *,
    objects: list[Any] | None = None,
    scene: SwgScene | None = None,
    armature: Any | None = None,
    skeleton_template_names: list[str] | None = None,
    ignore_blend_targets: bool = False,
) -> SwgScene:
    """Write `.mgn` from Blender mesh object(s) or a pre-built SwgScene."""
    mesh_path = Path(filepath)
    if scene is None:
        if not objects:
            raise ValueError("provide objects or scene")
        arm_obj = _find_armature_object(objects[0], armature)
        skel = armature_object_to_swg_skeleton(arm_obj) if arm_obj is not None else None

        preserve: SwgScene | None = None
        if mesh_path.is_file():
            try:
                preserve = load_skeletal_mesh(mesh_path)
            except Exception:
                preserve = None

        meshes = [
            mesh_object_to_swg_mesh(
                obj,
                armature=armature,
                skeleton_template_names=skeleton_template_names,
            )
            for obj in objects
        ]
        blend_targets = []
        if not ignore_blend_targets:
            for obj, mesh in zip(objects, meshes, strict=True):
                blend_targets.extend(collect_blend_targets_from_object(obj, mesh))

        scene = SwgScene(
            meshes=meshes,
            skeleton=skel,
            blend_targets=blend_targets,
            dot3_vectors=list(preserve.dot3_vectors) if preserve else [],
            occlusion=preserve.occlusion if preserve else SwgScene().occlusion,
            skmg_occlusion_counts=preserve.skmg_occlusion_counts if preserve else (),
            skmg_pre_psdt_blocks=list(preserve.skmg_pre_psdt_blocks) if preserve else [],
            skmg_post_psdt_blocks=list(preserve.skmg_post_psdt_blocks) if preserve else [],
        )
    write_skeletal_mesh_file(mesh_path, scene)
    scene.source_path = str(mesh_path)
    return scene


def export_skeleton(
    filepath: str | Path,
    *,
    armature: Any | None = None,
    skeleton: SwgSkeleton | None = None,
    version: str | None = None,
) -> SwgSkeleton:
    """Write `.skt` from a Blender armature or pre-built SwgSkeleton."""
    if skeleton is None:
        if armature is None:
            raise ValueError("provide armature or skeleton")
        skeleton = armature_object_to_swg_skeleton(armature)

    write_skeleton_file(filepath, skeleton, version=version or "0002")
    return skeleton


def export_skeletal_mesh_from_selection(
    filepath: str | Path,
    *,
    armature: Any | None = None,
    skeleton_template_names: list[str] | None = None,
    ignore_blend_targets: bool = False,
) -> SwgScene:
    import bpy

    selected = [obj for obj in bpy.context.selected_objects if obj.type == "MESH"]
    if len(selected) != 1:
        raise ValueError("select exactly one mesh object for skeletal export")
    return export_skeletal_mesh(
        filepath,
        objects=selected,
        armature=armature,
        skeleton_template_names=skeleton_template_names,
        ignore_blend_targets=ignore_blend_targets,
    )


def export_skeleton_from_selection(
    filepath: str | Path,
    *,
    version: str | None = None,
) -> SwgSkeleton:
    import bpy

    armatures = [obj for obj in bpy.context.selected_objects if obj.type == "ARMATURE"]
    if len(armatures) != 1:
        raise ValueError("select exactly one armature object for skeleton export")
    return export_skeleton(filepath, armature=armatures[0], version=version)


def main(argv: list[str] | None = None) -> int:
    import argparse

    parser = argparse.ArgumentParser(description="Export SWG skeletal IFF from sources or Blender")
    sub = parser.add_subparsers(dest="command", required=True)

    mgn = sub.add_parser("mgn", help="load .mgn IR and re-serialize")
    mgn.add_argument("input", type=Path)
    mgn.add_argument("output", type=Path)

    skt = sub.add_parser("skt", help="load .skt and re-serialize")
    skt.add_argument("input", type=Path)
    skt.add_argument("output", type=Path)

    export_mgn = sub.add_parser("export-mgn", help="export selected Blender mesh to .mgn")
    export_mgn.add_argument("output", type=Path)

    export_skt = sub.add_parser("export-skt", help="export selected Blender armature to .skt")
    export_skt.add_argument("output", type=Path)
    export_skt.add_argument("--version", default="0002")

    args = parser.parse_args(argv)
    if args.command == "mgn":
        export_mgn_roundtrip_source(args.input, args.output)
    elif args.command == "skt":
        export_skt_roundtrip_source(args.input, args.output)
    elif args.command == "export-mgn":
        try:
            import bpy  # noqa: F401
        except ImportError:
            print("bpy not available - run inside Blender", file=sys.stderr)
            return 1
        export_skeletal_mesh_from_selection(args.output)
    else:
        try:
            import bpy  # noqa: F401
        except ImportError:
            print("bpy not available - run inside Blender", file=sys.stderr)
            return 1
        export_skeleton_from_selection(args.output, version=args.version)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())