"""Floor mesh IFF (FLOR/0006) — Phase 12."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

from swg_iff.reader import IffError, IffReader
from swg_iff.tags import (
    TAG_0006,
    TAG_BEDG,
    TAG_BTRE,
    TAG_FLOR,
    TAG_PGRF,
    TAG_TRIS,
    TAG_VERT,
    tag_from_str,
    tag_to_str,
)
from swg_iff.writer import (
    make_chunk,
    make_chunk_float_vector,
    make_chunk_int32,
    make_chunk_uint8,
    make_form,
)


@dataclass
class SwgFloorTri:
    corners: tuple[int, int, int]
    index: int
    neighbors: tuple[int, int, int]
    normal: tuple[float, float, float]
    edge_types: tuple[int, int, int]
    fallthrough: bool
    part_tag: int
    portal_ids: tuple[int, int, int]


@dataclass
class SwgFloorMesh:
    version: str = "0006"
    vertices: list[tuple[float, float, float]] = field(default_factory=list)
    triangles: list[SwgFloorTri] = field(default_factory=list)
    btree_raw: bytes | None = None
    bedg_raw: bytes | None = None
    pgrf_raw: bytes | None = None


def read_floor_tri(reader: IffReader) -> SwgFloorTri:
    c0, c1, c2 = reader.read_int32(), reader.read_int32(), reader.read_int32()
    index = reader.read_int32()
    n0, n1, n2 = reader.read_int32(), reader.read_int32(), reader.read_int32()
    normal = reader.read_float_vector()
    e0, e1, e2 = reader.read_uint8(), reader.read_uint8(), reader.read_uint8()
    fallthrough = reader.read_bool8()
    part_tag = reader.read_int32()
    p0, p1, p2 = reader.read_int32(), reader.read_int32(), reader.read_int32()
    return SwgFloorTri(
        corners=(c0, c1, c2),
        index=index,
        neighbors=(n0, n1, n2),
        normal=normal,
        edge_types=(e0, e1, e2),
        fallthrough=fallthrough,
        part_tag=part_tag,
        portal_ids=(p0, p1, p2),
    )


def write_floor_tri_bytes(tri: SwgFloorTri) -> bytes:
    payload = b"".join(make_chunk_int32(v) for v in tri.corners)
    payload += make_chunk_int32(tri.index)
    payload += b"".join(make_chunk_int32(v) for v in tri.neighbors)
    payload += make_chunk_float_vector(*tri.normal)
    payload += b"".join(make_chunk_uint8(v) for v in tri.edge_types)
    payload += make_chunk_uint8(1 if tri.fallthrough else 0)
    payload += make_chunk_int32(tri.part_tag)
    payload += b"".join(make_chunk_int32(v) for v in tri.portal_ids)
    return payload


def load_floor_mesh(path: str | Path) -> SwgFloorMesh:
    return load_floor_mesh_bytes(Path(path).read_bytes())


def load_floor_mesh_bytes(data: bytes) -> SwgFloorMesh:
    reader = IffReader(data)
    reader.enter_form(TAG_FLOR)
    try:
        version = tag_to_str(reader.current_name())
        if version != "0006":
            raise IffError(f"unsupported FLOR version {version!r}")
        reader.enter_form(TAG_0006)
        try:
            vertices: list[tuple[float, float, float]] = []
            triangles: list[SwgFloorTri] = []
            btree_raw = None
            bedg_raw = None
            pgrf_raw = None
            while not reader.at_end_of_form():
                block = reader._peek_block()
                tag = block.tag_str.strip() if not block.is_form else block.form_type_str.strip()
                if tag == "VERT":
                    reader.enter_chunk(TAG_VERT)
                    try:
                        count = reader.read_int32()
                        vertices = [reader.read_float_vector() for _ in range(count)]
                    finally:
                        reader.exit_chunk(TAG_VERT)
                elif tag == "TRIS":
                    reader.enter_chunk(TAG_TRIS)
                    try:
                        count = reader.read_int32()
                        triangles = [read_floor_tri(reader) for _ in range(count)]
                    finally:
                        reader.exit_chunk(TAG_TRIS)
                elif tag == "BTRE":
                    btree_raw = reader.read_raw_block()
                elif tag == "BEDG":
                    bedg_raw = reader.read_raw_block()
                elif tag == "PGRF":
                    pgrf_raw = reader.read_raw_block()
                else:
                    reader.skip_block()
            return SwgFloorMesh(
                version=version,
                vertices=vertices,
                triangles=triangles,
                btree_raw=btree_raw,
                bedg_raw=bedg_raw,
                pgrf_raw=pgrf_raw,
            )
        finally:
            reader.exit_form(TAG_0006)
    finally:
        reader.exit_form(TAG_FLOR)


def write_floor_mesh_bytes(spec: SwgFloorMesh) -> bytes:
    version = spec.version if spec.version.isdigit() else "0006"
    vert_payload = make_chunk_int32(len(spec.vertices)) + b"".join(
        make_chunk_float_vector(*v) for v in spec.vertices
    )
    tri_payload = make_chunk_int32(len(spec.triangles)) + b"".join(
        write_floor_tri_bytes(t) for t in spec.triangles
    )
    body = make_chunk("VERT", vert_payload) + make_chunk("TRIS", tri_payload)
    if spec.btree_raw:
        body += spec.btree_raw
    if spec.bedg_raw:
        body += spec.bedg_raw
    if spec.pgrf_raw:
        body += spec.pgrf_raw
    return make_form("FLOR", make_form(version, body))


def write_floor_mesh(path: str | Path, spec: SwgFloorMesh) -> None:
    Path(path).write_bytes(write_floor_mesh_bytes(spec))
