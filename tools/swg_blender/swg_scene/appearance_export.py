"""APPR/0003 appearance template writers — visual extent + hardpoints (Phase 8.1)."""

from __future__ import annotations

import math
from dataclasses import dataclass

from swg_iff.writer import (
    make_chunk,
    make_chunk_bool8,
    make_chunk_float,
    make_chunk_float_vector,
    make_empty_form,
    make_form,
)
from swg_scene.model import SwgHardpoint, SwgMesh


@dataclass(frozen=True)
class AxisAlignedBounds:
    min_x: float
    min_y: float
    min_z: float
    max_x: float
    max_y: float
    max_z: float

    @property
    def center(self) -> tuple[float, float, float]:
        return (
            (self.min_x + self.max_x) * 0.5,
            (self.min_y + self.max_y) * 0.5,
            (self.min_z + self.max_z) * 0.5,
        )

    @property
    def radius(self) -> float:
        cx, cy, cz = self.center
        r2 = max(
            (self.max_x - cx) ** 2 + (self.max_y - cy) ** 2 + (self.max_z - cz) ** 2,
            (self.min_x - cx) ** 2 + (self.min_y - cy) ** 2 + (self.min_z - cz) ** 2,
            (self.max_x - cx) ** 2 + (self.min_y - cy) ** 2 + (self.min_z - cz) ** 2,
            (self.min_x - cx) ** 2 + (self.min_y - cy) ** 2 + (self.min_z - cz) ** 2,
        )
        return math.sqrt(r2)


def bounds_from_positions(
    positions: list[tuple[float, float, float]],
) -> AxisAlignedBounds | None:
    if not positions:
        return None
    xs = [p[0] for p in positions]
    ys = [p[1] for p in positions]
    zs = [p[2] for p in positions]
    return AxisAlignedBounds(min(xs), min(ys), min(zs), max(xs), max(ys), max(zs))


def bounds_from_meshes(meshes: list[SwgMesh]) -> AxisAlignedBounds | None:
    positions: list[tuple[float, float, float]] = []
    for mesh in meshes:
        positions.extend(mesh.positions)
    return bounds_from_positions(positions)


def make_chunk_float_transform(matrix: tuple[float, ...]) -> bytes:
    if len(matrix) != 12:
        raise ValueError("transform matrix must have 12 floats (3x4 row-major)")
    payload = b"".join(make_chunk_float(v) for v in matrix)
    return payload


def write_sphere_extent(center: tuple[float, float, float], radius: float) -> bytes:
    sphr = make_chunk(
        "SPHR",
        make_chunk_float_vector(*center) + make_chunk_float(radius),
    )
    return make_form("EXSP", make_form("0001", sphr))


def write_box_extent(bounds: AxisAlignedBounds) -> bytes:
    cx, cy, cz = bounds.center
    radius = bounds.radius
    sphere = write_sphere_extent((cx, cy, cz), radius)
    box = make_chunk(
        "BOX ",
        make_chunk_float_vector(bounds.max_x, bounds.max_y, bounds.max_z)
        + make_chunk_float_vector(bounds.min_x, bounds.min_y, bounds.min_z),
    )
    return make_form("EXBX", make_form("0001", sphere + box))


def write_hardpoints(hardpoints: list[SwgHardpoint]) -> bytes:
    if not hardpoints:
        return make_empty_form("HPTS")
    chunks = b""
    for hp in hardpoints:
        name = hp.name.encode("ascii", errors="replace") + b"\x00"
        payload = make_chunk_float_transform(hp.matrix) + name
        chunks += make_chunk("HPNT", payload)
    return make_form("HPTS", chunks)


def write_floor(has_floor: bool = False, floor_name: str = "") -> bytes:
    data = make_chunk_bool8(has_floor)
    if has_floor and floor_name:
        data += floor_name.encode("ascii", errors="replace") + b"\x00"
    return make_form("FLOR", make_chunk("DATA", data))


def write_appearance_0003(
    *,
    meshes: list[SwgMesh],
    hardpoints: list[SwgHardpoint] | None = None,
    bounds: AxisAlignedBounds | None = None,
    collision_extent: bytes | None = None,
    has_floor: bool = False,
    floor_name: str = "",
) -> bytes:
    """Build APPR/0003 bytes (visual extent + optional collision + hardpoints)."""
    hp = hardpoints or []
    b = bounds if bounds is not None else bounds_from_meshes(meshes)
    if b is None:
        visual = make_empty_form("NULL")
    else:
        visual = write_box_extent(b)

    collision = collision_extent if collision_extent is not None else make_empty_form("NULL")

    body = visual + collision + write_hardpoints(hp) + write_floor(has_floor, floor_name)
    return make_form("APPR", make_form("0003", body))
