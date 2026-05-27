"""SLOD LOD skeleton template writer (LodSkeletonTemplate 0000)."""

from __future__ import annotations

from pathlib import Path

from swg_iff.writer import make_chunk, make_chunk_int16, make_form
from swg_scene.skeleton import SwgSkeleton
from swg_scene.skeleton_export import write_skeleton


def write_slod(skeletons: list[SwgSkeleton]) -> bytes:
    if not skeletons:
        raise ValueError("SLOD requires at least one detail level skeleton")

    body = make_chunk("INFO", make_chunk_int16(len(skeletons)))
    for skel in skeletons:
        body += write_skeleton(skel, version=skel.version or "0002")
    return make_form("SLOD", make_form("0000", body))


def write_slod_file(path: str | Path, skeletons: list[SwgSkeleton]) -> None:
    Path(path).write_bytes(write_slod(skeletons))
