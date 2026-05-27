"""SKTM skeleton template writer (BasicSkeletonTemplate 0001/0002)."""

from __future__ import annotations

from pathlib import Path

from swg_iff.writer import (
    make_chunk,
    make_chunk_float_vector,
    make_chunk_int32,
    make_chunk_quaternion,
    make_chunk_string,
    make_chunk_uint32,
    make_form,
)
from swg_scene.skeleton import SwgSkeleton


def write_skeleton(skel: SwgSkeleton, *, version: str | None = None) -> bytes:
    version = version or skel.version or "0002"
    if version not in ("0001", "0002"):
        raise ValueError(f"unsupported SKTM export version {version!r}")

    joint_count = len(skel.joints)
    info = make_chunk("INFO", make_chunk_int32(joint_count))

    names = make_chunk("NAME", b"".join(make_chunk_string(j.name) for j in skel.joints))
    parents = make_chunk("PRNT", b"".join(make_chunk_int32(j.parent_index) for j in skel.joints))

    identity = make_chunk_quaternion(0.0, 0.0, 0.0, 1.0)
    rpre = make_chunk("RPRE", identity * joint_count)
    rpst = make_chunk("RPST", identity * joint_count)

    bptr = make_chunk(
        "BPTR",
        b"".join(make_chunk_float_vector(*j.bind_translation) for j in skel.joints),
    )
    bpro = make_chunk(
        "BPRO",
        b"".join(make_chunk_quaternion(*j.bind_rotation) for j in skel.joints),
    )

    body = info + names + parents + rpre + rpst + bptr + bpro

    if version == "0001":
        bpmj = make_chunk("BPMJ", b"\x00" * (48 * joint_count))
        body += bpmj

    jror = make_chunk("JROR", b"".join(make_chunk_uint32(0) for _ in skel.joints))
    body += jror

    return make_form("SKTM", make_form(version, body))


def write_skeleton_file(path: str | Path, skel: SwgSkeleton, *, version: str | None = None) -> None:
    Path(path).write_bytes(write_skeleton(skel, version=version))