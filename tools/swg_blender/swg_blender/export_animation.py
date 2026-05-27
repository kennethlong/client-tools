"""Export SWG keyframe animation (.kfat) from Blender actions or scene IR."""

from __future__ import annotations

import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from swg_scene.animation import SATCCF_xTranslation, SATCCF_yTranslation, SATCCF_zTranslation, load_kfat
from swg_scene.animation_export import write_animation_file
from swg_scene.coords import blender_to_engine_position
from swg_scene.model import SwgClip, SwgQuatKey, SwgRealKey, SwgTransformTrack

Quaternion = tuple[float, float, float, float]


def export_kfat_roundtrip_source(path: str | Path, output: str | Path) -> None:
    clip = load_kfat(path)
    write_animation_file(output, clip, format="kfat", version=clip.version or "0003")


def _quat_conjugate(quaternion: Quaternion) -> Quaternion:
    x, y, z, w = quaternion
    return (-x, -y, -z, w)


def _quat_multiply(left: Quaternion, right: Quaternion) -> Quaternion:
    ax, ay, az, aw = left
    bx, by, bz, bw = right
    return (
        aw * bx + ax * bw + ay * bz - az * by,
        aw * by - ax * bz + ay * bw + az * bx,
        aw * bz + ax * by - ay * bx + az * bw,
        aw * bw - ax * bx - ay * by - az * bz,
    )


def _quat_delta(current: Quaternion, bind: Quaternion) -> Quaternion:
    return _quat_multiply(current, _quat_conjugate(bind))


def _add_real_key(
    keys: list[SwgRealKey],
    frame: int,
    value: float,
    *,
    last_frame: bool,
    epsilon: float = 0.0005,
) -> None:
    write_data = True
    if keys:
        last_value = keys[-1].value
        delta = value - last_value
        write_data = (delta > epsilon) or (delta < -epsilon)
        if write_data and keys[-1].frame != frame - 1:
            keys.append(SwgRealKey(frame=frame - 1, value=last_value))
    if last_frame and len(keys) > 1:
        write_data = True
    if write_data:
        keys.append(SwgRealKey(frame=frame, value=value))


def _add_quat_key(
    keys: list[SwgQuatKey],
    frame: int,
    rotation: Quaternion,
    *,
    last_frame: bool,
    epsilon: float = 0.001,
) -> None:
    write_data = True
    if keys:
        last_rotation = keys[-1].rotation
        write_data = any(
            abs(rotation[index] - last_rotation[index]) > epsilon for index in range(4)
        )
        if write_data and keys[-1].frame != frame - 1:
            keys.append(SwgQuatKey(frame=frame - 1, rotation=last_rotation))
    if last_frame and len(keys) > 1:
        write_data = True
    if write_data:
        keys.append(SwgQuatKey(frame=frame, rotation=rotation))


@dataclass
class JointSample:
    name: str
    rotation_keys: list[SwgQuatKey] = field(default_factory=list)
    x_translation_keys: list[SwgRealKey] = field(default_factory=list)
    y_translation_keys: list[SwgRealKey] = field(default_factory=list)
    z_translation_keys: list[SwgRealKey] = field(default_factory=list)


def joint_samples_to_swg_clip(
    joints: list[JointSample],
    *,
    name: str = "",
    fps: float = 30.0,
    frame_count: int = 0,
    skeleton_template_path: str = "",
) -> SwgClip:
    if not joints:
        raise ValueError("animation export requires at least one joint sample")

    rotation_channels: list[list[SwgQuatKey]] = []
    static_rotations: list[Quaternion] = []
    translation_channels: list[list[SwgRealKey]] = []
    static_translations: list[float] = []
    transforms: list[SwgTransformTrack] = []

    for joint in joints:
        track = SwgTransformTrack(name=joint.name)

        if len(joint.rotation_keys) <= 1:
            track.has_animated_rotation = False
            track.rotation_channel_index = len(static_rotations)
            rotation = joint.rotation_keys[0].rotation if joint.rotation_keys else (0.0, 0.0, 0.0, 1.0)
            static_rotations.append(rotation)
        else:
            track.has_animated_rotation = True
            track.rotation_channel_index = len(rotation_channels)
            rotation_channels.append(list(joint.rotation_keys))

        for axis_keys, flag, index_attr in (
            (joint.x_translation_keys, SATCCF_xTranslation, "x_translation_channel_index"),
            (joint.y_translation_keys, SATCCF_yTranslation, "y_translation_channel_index"),
            (joint.z_translation_keys, SATCCF_zTranslation, "z_translation_channel_index"),
        ):
            if len(axis_keys) <= 1:
                value = axis_keys[0].value if axis_keys else 0.0
                setattr(track, index_attr, len(static_translations))
                static_translations.append(value)
            else:
                track.translation_mask |= flag
                setattr(track, index_attr, len(translation_channels))
                translation_channels.append(list(axis_keys))

        transforms.append(track)

    return SwgClip(
        name=name,
        fps=fps,
        frame_count=frame_count,
        skeleton_template_path=skeleton_template_path,
        transforms=transforms,
        rotation_channels=rotation_channels,
        static_rotations=static_rotations,
        translation_channels=translation_channels,
        static_translations=static_translations,
    )


def _blender_local_to_engine(loc: Any, quat: Any) -> tuple[tuple[float, float, float], Quaternion]:
    import mathutils

    engine_loc = blender_to_engine_position(loc.x, loc.y, loc.z)
    flip = mathutils.Matrix(((-1.0, 0.0, 0.0, 0.0), (0.0, 1.0, 0.0, 0.0), (0.0, 0.0, 1.0, 0.0), (0.0, 0.0, 0.0, 1.0)))
    rotation_matrix = quat.to_matrix().to_4x4()
    engine_matrix = flip @ rotation_matrix @ flip
    engine_quat = engine_matrix.to_quaternion()
    return engine_loc, (engine_quat.x, engine_quat.y, engine_quat.z, engine_quat.w)


def _pose_local_transform(pose_bone: Any) -> tuple[Any, Any]:
    import mathutils

    if pose_bone.parent:
        local_matrix = pose_bone.parent.matrix.inverted() @ pose_bone.matrix
    else:
        local_matrix = pose_bone.matrix.copy()
    location, rotation, _scale = local_matrix.decompose()
    return location, rotation



def action_to_swg_clip(
    armature_obj: Any,
    action: Any,
    *,
    fps: float | None = None,
    bind_frame: int | None = None,
    skeleton_template_path: str | None = None,
) -> SwgClip:
    """Sample a Blender action on an armature into SwgClip IR (KFAT/0003)."""
    import bpy

    if armature_obj.type != "ARMATURE":
        raise ValueError(f"object {armature_obj.name!r} is not an armature")
    if action is None:
        raise ValueError("action is required")

    scene = bpy.context.scene
    frame_start = int(action.frame_start)
    frame_end = int(action.frame_end)
    frame_count = frame_end - frame_start
    if frame_count < 0:
        raise ValueError("action frame range is invalid")

    if fps is None:
        fps = scene.render.fps / scene.render.fps_base

    bind_sample_frame = frame_start if bind_frame is None else bind_frame
    original_frame = scene.frame_current
    original_action = armature_obj.animation_data.action if armature_obj.animation_data else None

    if armature_obj.animation_data is None:
        armature_obj.animation_data_create()
    armature_obj.animation_data.action = action

    bind_loc_rot: dict[str, tuple[tuple[float, float, float], Quaternion]] = {}
    joints: list[JointSample] = []

    try:
        scene.frame_set(bind_sample_frame)
        bpy.context.view_layer.update()
        for bone in armature_obj.data.bones:
            pose_bone = armature_obj.pose.bones.get(bone.name)
            if pose_bone is None:
                continue
            loc, rot = _pose_local_transform(pose_bone)
            bind_loc_rot[bone.name] = _blender_local_to_engine(loc, rot)

        for bone in armature_obj.data.bones:
            joints.append(JointSample(name=bone.name))

        joint_by_name = {joint.name: joint for joint in joints}

        for absolute_frame in range(frame_start, frame_end + 1):
            relative_frame = absolute_frame - frame_start
            last_frame = relative_frame == frame_count
            scene.frame_set(absolute_frame)
            bpy.context.view_layer.update()

            for bone in armature_obj.data.bones:
                pose_bone = armature_obj.pose.bones.get(bone.name)
                if pose_bone is None:
                    continue
                bind_loc, bind_rot = bind_loc_rot[bone.name]
                loc, rot = _pose_local_transform(pose_bone)
                engine_loc, engine_rot = _blender_local_to_engine(loc, rot)
                delta_rot = _quat_delta(engine_rot, bind_rot)
                joint = joint_by_name[bone.name]
                _add_quat_key(joint.rotation_keys, relative_frame, delta_rot, last_frame=last_frame)
                _add_real_key(
                    joint.x_translation_keys,
                    relative_frame,
                    engine_loc[0] - bind_loc[0],
                    last_frame=last_frame,
                )
                _add_real_key(
                    joint.y_translation_keys,
                    relative_frame,
                    engine_loc[1] - bind_loc[1],
                    last_frame=last_frame,
                )
                _add_real_key(
                    joint.z_translation_keys,
                    relative_frame,
                    engine_loc[2] - bind_loc[2],
                    last_frame=last_frame,
                )
    finally:
        scene.frame_set(original_frame)
        if armature_obj.animation_data is not None:
            armature_obj.animation_data.action = original_action
        bpy.context.view_layer.update()

    template_path = skeleton_template_path
    if template_path is None:
        template_path = armature_obj.get("swg_skeleton_template") or ""

    return joint_samples_to_swg_clip(
        joints,
        name=action.name,
        fps=float(fps),
        frame_count=frame_count,
        skeleton_template_path=str(template_path),
    )


def export_animation(
    filepath: str | Path,
    *,
    armature: Any | None = None,
    action: Any | None = None,
    clip: SwgClip | None = None,
    fps: float | None = None,
    bind_frame: int | None = None,
    skeleton_template_path: str | None = None,
    format: str = "ckat",
) -> SwgClip:
    if clip is None:
        if armature is None or action is None:
            raise ValueError("provide clip or armature+action")
        clip = action_to_swg_clip(
            armature,
            action,
            fps=fps,
            bind_frame=bind_frame,
            skeleton_template_path=skeleton_template_path,
        )

    output = Path(filepath)
    write_animation_file(output, clip, format=format)
    clip.source_path = str(output)
    return clip


def export_animation_from_selection(
    filepath: str | Path,
    *,
    fps: float | None = None,
    bind_frame: int | None = None,
    format: str = "ckat",
) -> SwgClip:
    import bpy

    armatures = [obj for obj in bpy.context.selected_objects if obj.type == "ARMATURE"]
    if len(armatures) != 1:
        raise ValueError("select exactly one armature object for animation export")

    armature = armatures[0]
    if armature.animation_data is None or armature.animation_data.action is None:
        raise ValueError(f"armature {armature.name!r} has no active action")

    return export_animation(
        filepath,
        armature=armature,
        action=armature.animation_data.action,
        fps=fps,
        bind_frame=bind_frame,
        format=format,
    )


def main(argv: list[str] | None = None) -> int:
    import argparse

    parser = argparse.ArgumentParser(description="Export SWG KFAT animation IFF from sources or Blender")
    sub = parser.add_subparsers(dest="command", required=True)

    kfat = sub.add_parser("kfat", help="load .kfat IR and re-serialize")
    kfat.add_argument("input", type=Path)
    kfat.add_argument("output", type=Path)

    export_kfat = sub.add_parser("export-kfat", help="export selected Blender armature action to .ans")
    export_kfat.add_argument("output", type=Path)
    export_kfat.add_argument("--fps", type=float, default=None)
    export_kfat.add_argument("--bind-frame", type=int, default=None)
    export_kfat.add_argument("--format", choices=("ckat", "kfat"), default="ckat")

    args = parser.parse_args(argv)
    if args.command == "kfat":
        export_kfat_roundtrip_source(args.input, args.output)
        return 0

    try:
        import bpy  # noqa: F401
    except ImportError:
        print("bpy not available - run inside Blender", file=sys.stderr)
        return 1

    export_animation_from_selection(
        args.output,
        fps=args.fps,
        bind_frame=args.bind_frame,
        format=args.format,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())