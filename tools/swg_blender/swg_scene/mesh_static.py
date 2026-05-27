"""Static mesh (MESH / SPS) loader — port of MeshAppearanceTemplate + ShaderPrimitiveSetTemplate."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from swg_iff.reader import IffReader, IffError
from swg_iff.tags import (
    TAG_APPR,
    TAG_0001,
    TAG_0005,
    TAG_CNT,
    TAG_INDX,
    TAG_SIDX,
    TAG_INFO,
    TAG_MESH,
    TAG_NAME,
    TAG_SPS,
)
from swg_scene.coords import engine_to_blender_position, engine_to_blender_vector
from swg_scene.model import SwgDirectionSortedIndices, SwgMaterialRef, SwgMesh, SwgScene
from swg_scene.vertex_buffer import (
    read_index_buffer_uint16,
    read_sorted_index_buffer,
    read_vertex_buffer_vtxa,
)

# ShaderPrimitiveSetTemplate::SPSPT_indexedTriangleList
SPSPT_INDEXED_TRIANGLE_LIST = 9


@dataclass
class _PrimitiveData:
    positions: list[tuple[float, float, float]]
    normals: list[tuple[float, float, float]]
    uvs: list[list[tuple[float, float]]]
    colors0: list[int]
    indices: list[int]
    sorted_indices: list[SwgDirectionSortedIndices]


def load_static_mesh(path: str | Path) -> SwgScene:
    """Load a `.msh` MeshAppearance into scene IR (engine coordinates)."""
    p = Path(path)
    reader = IffReader.from_file(p)
    scene = load_static_mesh_from_reader(reader)
    scene.source_path = str(p)
    if scene.meshes and p.stem:
        scene.meshes[0].name = p.stem
    return scene


def load_static_mesh_from_reader(reader: IffReader) -> SwgScene:
    reader.enter_form(TAG_MESH)
    try:
        version = reader.current_name()
        if version != TAG_0005:
            raise IffError(
                f"unsupported MESH version {reader._peek_block().form_type_str!r} "
                f"(only 0005 implemented for Phase 1)"
            )
        return _load_mesh_0005(reader)
    finally:
        reader.exit_form(TAG_MESH)


def _load_mesh_0005(reader: IffReader) -> SwgScene:
    reader.enter_form(TAG_0005)
    try:
        appearance_raw = _read_appearance_template(reader)
        shader_groups = _load_sps(reader)
    finally:
        reader.exit_form(TAG_0005)

    meshes: list[SwgMesh] = []
    for shader_path, primitives in shader_groups:
        merged = _merge_primitives(primitives)
        mesh = SwgMesh(
            name=Path(shader_path).stem,
            positions=merged.positions,
            normals=merged.normals,
            uvs=merged.uvs,
            colors0=merged.colors0,
            indices=merged.indices,
            sorted_indices=merged.sorted_indices,
            material=SwgMaterialRef(shader_relpath=shader_path),
        )
        meshes.append(mesh)

    return SwgScene(root_form="MESH", meshes=meshes, appearance_raw=appearance_raw)


def _read_appearance_template(reader: IffReader) -> bytes | None:
    """Read raw APPR FORM bytes; return None if absent."""
    while not reader.at_end_of_form():
        block = reader._peek_block()
        if block.is_form and block.form_type == TAG_SPS:
            return None
        if block.is_form and block.form_type == TAG_APPR:
            return reader.read_raw_block()
        reader.skip_block()
    raise IffError("MESH 0005 missing SPS form")


def _load_sps(reader: IffReader) -> list[tuple[str, list[_PrimitiveData]]]:
    reader.enter_form(TAG_SPS)
    try:
        version = reader.current_name()
        if version != TAG_0001:
            raise IffError(f"unsupported SPS version (only 0001 implemented)")
        return _load_sps_0001(reader)
    finally:
        reader.exit_form(TAG_SPS)


def _load_sps_0001(reader: IffReader) -> list[tuple[str, list[_PrimitiveData]]]:
    reader.enter_form(TAG_0001)
    try:
        reader.enter_chunk(TAG_CNT)
        shader_count = reader.read_int32()
        reader.exit_chunk(TAG_CNT)

        groups: list[tuple[str, list[_PrimitiveData]]] = []
        for _ in range(shader_count):
            block = reader._peek_block()
            if not block.is_form:
                raise IffError(
                    f"expected shader group FORM at {block.offset:#x}, got {block.tag_str!r}"
                )
            group_form = block.form_type
            reader.enter_form(group_form)
            try:
                reader.enter_chunk(TAG_NAME)
                shader_path = reader.read_string()
                reader.exit_chunk(TAG_NAME)

                reader.enter_chunk(TAG_INFO)
                primitive_count = reader.read_int32()
                reader.exit_chunk(TAG_INFO)

                primitives: list[_PrimitiveData] = []
                for _ in range(primitive_count):
                    primitives.append(_load_shader_primitive_0001(reader))
                groups.append((shader_path, primitives))
            finally:
                reader.exit_form(group_form)
        return groups
    finally:
        reader.exit_form(TAG_0001)


def _load_shader_primitive_0001(reader: IffReader) -> _PrimitiveData:
    reader.enter_form(TAG_0001)
    try:
        reader.enter_chunk(TAG_INFO)
        primitive_type = reader.read_int32()
        has_indices = reader.read_bool8()
        has_sorted_indices = reader.read_bool8()
        reader.exit_chunk(TAG_INFO)

        if primitive_type != SPSPT_INDEXED_TRIANGLE_LIST:
            raise IffError(f"unsupported primitive type {primitive_type} (need indexed trilist)")
        vb = read_vertex_buffer_vtxa(reader)

        indices: list[int] = []
        sorted_indices: list[SwgDirectionSortedIndices] = []
        if has_indices:
            reader.enter_chunk(TAG_INDX)
            indices = read_index_buffer_uint16(reader)
            reader.exit_chunk(TAG_INDX)

        if has_sorted_indices:
            reader.enter_chunk(TAG_SIDX)
            sorted_indices = [
                SwgDirectionSortedIndices(direction=direction, indices=idxs)
                for direction, idxs in read_sorted_index_buffer(reader)
            ]
            reader.exit_chunk(TAG_SIDX)

        return _PrimitiveData(
            positions=vb.positions,
            normals=vb.normals,
            uvs=vb.uvs,
            colors0=vb.colors0,
            indices=indices,
            sorted_indices=sorted_indices,
        )
    finally:
        reader.exit_form(TAG_0001)


def _merge_primitives(primitives: list[_PrimitiveData]) -> _PrimitiveData:
    if len(primitives) == 1:
        return primitives[0]

    positions: list[tuple[float, float, float]] = []
    normals: list[tuple[float, float, float]] = []
    uvs: list[list[tuple[float, float]]] = []
    colors0: list[int] = []
    indices: list[int] = []
    sorted_indices: list[SwgDirectionSortedIndices] = []
    offset = 0

    for prim in primitives:
        if not uvs:
            uvs = [[] for _ in range(len(prim.uvs))]
        positions.extend(prim.positions)
        normals.extend(prim.normals)
        if prim.colors0:
            if not colors0 and offset == 0:
                colors0 = []
            colors0.extend(prim.colors0)
        for set_idx, layer in enumerate(prim.uvs):
            uvs[set_idx].extend(layer)
        indices.extend(i + offset for i in prim.indices)
        sorted_indices.extend(prim.sorted_indices)
        offset += len(prim.positions)

    return _PrimitiveData(
        positions=positions,
        normals=normals,
        uvs=uvs,
        colors0=colors0,
        indices=indices,
        sorted_indices=sorted_indices,
    )


def scene_to_blender_coords(scene: SwgScene) -> SwgScene:
    """Return a copy with positions/normals converted to Blender space."""
    out = SwgScene(
        source_path=scene.source_path,
        root_form=scene.root_form,
        skeleton=scene.skeleton,
        animations=list(scene.animations),
    )
    for mesh in scene.meshes:
        out.meshes.append(
            SwgMesh(
                name=mesh.name,
                positions=[
                    engine_to_blender_position(*p) for p in mesh.positions
                ],
                normals=[engine_to_blender_vector(*n) for n in mesh.normals],
                uvs=[list(layer) for layer in mesh.uvs],
                colors0=list(mesh.colors0),
                indices=list(mesh.indices),
                sorted_indices=list(mesh.sorted_indices),
                material=mesh.material,
                transform_names=list(mesh.transform_names),
                skeleton_template_names=list(mesh.skeleton_template_names),
                bone_weights=[list(w) for w in mesh.bone_weights],
            )
        )
    return out
