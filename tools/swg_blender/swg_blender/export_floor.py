"""Export Blender floor object(s) back to SWG `.flr`."""

from __future__ import annotations

from pathlib import Path
from typing import Any

from swg_blender.bpy_geometry import mesh_vertex_positions_engine
from swg_scene.floor_mesh import SwgFloorMesh, SwgFloorTri, load_floor_mesh, write_floor_mesh_bytes


def _mesh_faces_indices(mesh: Any) -> list[tuple[int, int, int]]:
    faces: list[tuple[int, int, int]] = []
    for poly in mesh.polygons:
        if len(poly.vertices) != 3:
            raise ValueError(f"floor mesh {mesh.name!r}: non-triangle face; triangulate first")
        faces.append(tuple(int(i) for i in poly.vertices))
    return faces


def floor_objects_to_spec(
    objects: list[Any],
    *,
    original: SwgFloorMesh | None = None,
) -> SwgFloorMesh:
    if not objects:
        raise ValueError("no floor objects")
    obj = objects[0]
    mesh = obj.data
    if mesh is None:
        raise ValueError(f"object {obj.name!r} has no mesh")

    positions = mesh_vertex_positions_engine(obj)
    faces = _mesh_faces_indices(mesh)
    triangles: list[SwgFloorTri] = []
    orig_tris = list(original.triangles) if original else []

    for index, corners in enumerate(faces):
        if index < len(orig_tris) and orig_tris[index].corners == corners:
            tri = orig_tris[index]
            triangles.append(
                SwgFloorTri(
                    corners=corners,
                    index=tri.index,
                    neighbors=tri.neighbors,
                    normal=tri.normal,
                    edge_types=tri.edge_types,
                    fallthrough=tri.fallthrough,
                    part_tag=tri.part_tag,
                    portal_ids=tri.portal_ids,
                )
            )
        else:
            triangles.append(
                SwgFloorTri(
                    corners=corners,
                    index=index,
                    neighbors=(-1, -1, -1),
                    normal=(0.0, 0.0, 1.0),
                    edge_types=(0, 0, 0),
                    fallthrough=False,
                    part_tag=0,
                    portal_ids=(-1, -1, -1),
                )
            )

    return SwgFloorMesh(
        version=original.version if original else "0006",
        vertices=positions,
        triangles=triangles,
        btree_raw=original.btree_raw if original else None,
        bedg_raw=original.bedg_raw if original else None,
        pgrf_raw=original.pgrf_raw if original else None,
    )


def export_floor_mesh(
    filepath: str | Path,
    *,
    objects: list[Any],
    preserve_metadata_from: str | Path | None = None,
) -> None:
    path = Path(filepath)
    original = None
    src = preserve_metadata_from or path
    if Path(src).is_file():
        original = load_floor_mesh(src)
    spec = floor_objects_to_spec(objects, original=original)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(write_floor_mesh_bytes(spec))