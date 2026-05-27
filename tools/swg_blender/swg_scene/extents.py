"""SWG extent IFF load/write (EXBX, EXSP, XCYL, CMSH, NULL) — Phase 12."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Union

from swg_iff.reader import IffError, IffReader
from swg_iff.tags import tag_from_str, tag_to_str
from swg_iff.writer import (
    make_chunk,
    make_chunk_float,
    make_chunk_float_vector,
    make_empty_form,
    make_form,
)
from swg_scene.appearance_export import AxisAlignedBounds, write_box_extent


@dataclass
class SwgSphereExtent:
    center: tuple[float, float, float]
    radius: float


@dataclass
class SwgBoxExtent:
    min_xyz: tuple[float, float, float]
    max_xyz: tuple[float, float, float]


@dataclass
class SwgCylinderExtent:
    base: tuple[float, float, float]
    radius: float
    height: float


@dataclass
class SwgMeshExtent:
    vertices: list[tuple[float, float, float]]
    indices: list[int]


SwgExtent = Union[SwgBoxExtent, SwgSphereExtent, SwgCylinderExtent, SwgMeshExtent, None]


def extent_root_type(data: bytes) -> str | None:
    from swg_iff.reader import root_form_type

    root = root_form_type(data)
    return tag_to_str(root) if root is not None else None


def load_extent_bytes(data: bytes) -> SwgExtent:
    reader = IffReader(data)
    block = reader._peek_block()
    if not block.is_form:
        raise IffError("extent root must be a FORM")
    tag = block.form_type_str.strip()
    if tag == "NULL":
        reader.enter_form(tag_from_str("NULL"))
        reader.exit_form(tag_from_str("NULL"))
        return None
    if tag == "EXBX":
        return _load_box(reader)
    if tag == "EXSP":
        return _load_sphere(reader)
    if tag == "XCYL":
        return _load_cylinder(reader)
    if tag == "CMSH":
        return _load_mesh_extent(reader)
    raise IffError(f"unsupported extent form {tag!r}")


def _load_sphere(reader: IffReader) -> SwgSphereExtent:
    reader.enter_form(tag_from_str("EXSP"))
    try:
        reader.enter_form(tag_from_str("0001"))
        try:
            reader.enter_chunk(tag_from_str("SPHR"))
            center = reader.read_float_vector()
            radius = reader.read_float()
            reader.exit_chunk(tag_from_str("SPHR"))
            return SwgSphereExtent(center=center, radius=radius)
        finally:
            reader.exit_form(tag_from_str("0001"))
    finally:
        reader.exit_form(tag_from_str("EXSP"))


def _load_box(reader: IffReader) -> SwgBoxExtent:
    reader.enter_form(tag_from_str("EXBX"))
    try:
        reader.enter_form(tag_from_str("0001"))
        try:
            min_xyz = max_xyz = None
            while not reader.at_end_of_form():
                block = reader._peek_block()
                if block.is_form and block.form_type_str.strip() == "EXSP":
                    reader.skip_block()
                elif not block.is_form and block.tag_str.strip() == "BOX":
                    reader.enter_chunk(tag_from_str("BOX "))
                    try:
                        max_xyz = reader.read_float_vector()
                        min_xyz = reader.read_float_vector()
                    finally:
                        reader.exit_chunk(tag_from_str("BOX "))
                else:
                    reader.skip_block()
            if min_xyz is None or max_xyz is None:
                raise IffError("EXBX missing BOX chunk")
            return SwgBoxExtent(min_xyz=min_xyz, max_xyz=max_xyz)
        finally:
            reader.exit_form(tag_from_str("0001"))
    finally:
        reader.exit_form(tag_from_str("EXBX"))


def _load_cylinder(reader: IffReader) -> SwgCylinderExtent:
    reader.enter_form(tag_from_str("XCYL"))
    try:
        reader.enter_form(tag_from_str("0000"))
        try:
            reader.enter_chunk(tag_from_str("CYLN"))
            base = reader.read_float_vector()
            radius = reader.read_float()
            height = reader.read_float()
            reader.exit_chunk(tag_from_str("CYLN"))
            return SwgCylinderExtent(base=base, radius=radius, height=height)
        finally:
            reader.exit_form(tag_from_str("0000"))
    finally:
        reader.exit_form(tag_from_str("XCYL"))


def _load_mesh_extent(reader: IffReader) -> SwgMeshExtent:
    from swg_scene.indexed_triangle_list import load_idtl_from_reader

    reader.enter_form(tag_from_str("CMSH"))
    try:
        reader.enter_form(tag_from_str("0000"))
        try:
            verts, indices = load_idtl_from_reader(reader)
            return SwgMeshExtent(vertices=verts, indices=indices)
        finally:
            reader.exit_form(tag_from_str("0000"))
    finally:
        reader.exit_form(tag_from_str("CMSH"))


def write_extent_bytes(extent: SwgExtent) -> bytes:
    if extent is None:
        return make_empty_form("NULL")
    if isinstance(extent, SwgBoxExtent):
        bounds = AxisAlignedBounds(
            extent.min_xyz[0],
            extent.min_xyz[1],
            extent.min_xyz[2],
            extent.max_xyz[0],
            extent.max_xyz[1],
            extent.max_xyz[2],
        )
        return write_box_extent(bounds)
    if isinstance(extent, SwgSphereExtent):
        body = make_chunk(
            "SPHR",
            make_chunk_float_vector(*extent.center) + make_chunk_float(extent.radius),
        )
        return make_form("EXSP", make_form("0001", body))
    if isinstance(extent, SwgCylinderExtent):
        body = make_chunk(
            "CYLN",
            make_chunk_float_vector(*extent.base)
            + make_chunk_float(extent.radius)
            + make_chunk_float(extent.height),
        )
        return make_form("XCYL", make_form("0000", body))
    if isinstance(extent, SwgMeshExtent):
        from swg_scene.indexed_triangle_list import write_idtl_bytes

        return make_form("CMSH", make_form("0000", write_idtl_bytes(extent.vertices, extent.indices)))
    raise TypeError(f"unsupported extent type {type(extent)!r}")
