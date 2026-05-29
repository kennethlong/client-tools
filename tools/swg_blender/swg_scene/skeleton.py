"""SKTM skeleton template loader (BasicSkeletonTemplate 0001/0002)."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

from swg_iff.reader import IffReader, IffError
from swg_iff.tags import (
    TAG_0001,
    TAG_0002,
    TAG_BPMJ,
    TAG_BPRO,
    TAG_BPTR,
    TAG_INFO,
    TAG_JROR,
    TAG_NAME,
    TAG_PRNT,
    TAG_RPRE,
    TAG_RPST,
    TAG_SKTM,
    TAG_SLOD,
    tag_to_str,
)

Quaternion = tuple[float, float, float, float]
Vector3 = tuple[float, float, float]


@dataclass
class SwgJoint:
    name: str
    parent_index: int
    bind_translation: Vector3 = (0.0, 0.0, 0.0)
    bind_rotation: Quaternion = (0.0, 0.0, 0.0, 1.0)
    pre_rotation: Quaternion = (0.0, 0.0, 0.0, 1.0)
    post_rotation: Quaternion = (0.0, 0.0, 0.0, 1.0)


@dataclass
class SwgSkeleton:
    source_path: str = ""
    version: str = ""
    joints: list[SwgJoint] = field(default_factory=list)


def load_skeleton(path: str | Path, *, lod_index: int = 0) -> SwgSkeleton:
    p = Path(path)
    reader = IffReader.from_file(p)
    root = reader.current_name()
    if root == TAG_SKTM:
        skel = _load_sktm_form(reader)
    elif root == TAG_SLOD:
        from swg_scene.skeleton_lod import load_slod_from_reader

        skel = load_slod_from_reader(reader, lod_index=lod_index)
    else:
        raise IffError(f"unsupported skeleton form {tag_to_str(root)}")
    skel.source_path = str(p)
    return skel


def _load_sktm_form(reader: IffReader) -> SwgSkeleton:
    reader.enter_form(TAG_SKTM)
    try:
        return _load_sktm_version(reader)
    finally:
        reader.exit_form(TAG_SKTM)


def _load_sktm_version(reader: IffReader) -> SwgSkeleton:
    version_tag = reader.current_name()
    if version_tag == TAG_0001:
        skel = _load_0001(reader)
    elif version_tag == TAG_0002:
        skel = _load_0002(reader)
    else:
        raise IffError(f"unsupported SKTM version {tag_to_str(version_tag)}")
    skel.version = tag_to_str(version_tag)
    return skel


def _load_0001(reader: IffReader) -> SwgSkeleton:
    reader.enter_form(TAG_0001)
    try:
        return _load_skeleton_body(reader, include_bpmj=True)
    finally:
        reader.exit_form(TAG_0001)


def _load_0002(reader: IffReader) -> SwgSkeleton:
    reader.enter_form(TAG_0002)
    try:
        return _load_skeleton_body(reader, include_bpmj=False)
    finally:
        reader.exit_form(TAG_0002)


def _read_quaternion(reader: IffReader) -> Quaternion:
    return (
        reader.read_float(),
        reader.read_float(),
        reader.read_float(),
        reader.read_float(),
    )


def _load_skeleton_body(reader: IffReader, *, include_bpmj: bool) -> SwgSkeleton:
    reader.enter_chunk(TAG_INFO)
    joint_count = reader.read_int32()
    reader.exit_chunk(TAG_INFO)

    reader.enter_chunk(TAG_NAME)
    names = [reader.read_string() for _ in range(joint_count)]
    reader.exit_chunk(TAG_NAME)

    reader.enter_chunk(TAG_PRNT)
    parents = [reader.read_int32() for _ in range(joint_count)]
    reader.exit_chunk(TAG_PRNT)

    reader.enter_chunk(TAG_RPRE)
    pre_rotations = [_read_quaternion(reader) for _ in range(joint_count)]
    reader.exit_chunk(TAG_RPRE)

    reader.enter_chunk(TAG_RPST)
    post_rotations = [_read_quaternion(reader) for _ in range(joint_count)]
    reader.exit_chunk(TAG_RPST)

    reader.enter_chunk(TAG_BPTR)
    translations = [reader.read_float_vector() for _ in range(joint_count)]
    reader.exit_chunk(TAG_BPTR)

    reader.enter_chunk(TAG_BPRO)
    rotations = [_read_quaternion(reader) for _ in range(joint_count)]
    reader.exit_chunk(TAG_BPRO)

    if include_bpmj:
        reader.enter_chunk(TAG_BPMJ)
        reader.read_bytes(48 * joint_count)
        reader.exit_chunk(TAG_BPMJ)

    reader.enter_chunk(TAG_JROR)
    reader.read_bytes(4 * joint_count)
    reader.exit_chunk(TAG_JROR)

    joints = [
        SwgJoint(
            name=names[i],
            parent_index=parents[i],
            bind_translation=translations[i],
            bind_rotation=rotations[i],
            pre_rotation=pre_rotations[i],
            post_rotation=post_rotations[i],
        )
        for i in range(joint_count)
    ]
    return SwgSkeleton(joints=joints)