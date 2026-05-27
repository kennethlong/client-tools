"""SKMG skeletal mesh loader (SkeletalMeshGeneratorTemplate 0002-0004)."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

from swg_iff.reader import IffReader, IffError
from swg_iff.tags import (
    TAG_0002,
    TAG_0003,
    TAG_0004,
    TAG_BLTS,
    TAG_HPTS,
    TAG_DOT3,
    TAG_INFO,
    TAG_ITL,
    TAG_NAME,
    TAG_NIDX,
    TAG_NORM,
    TAG_OITL,
    TAG_PIDX,
    TAG_POSN,
    TAG_PRIM,
    TAG_PSDT,
    TAG_SKMG,
    TAG_SKTM,
    TAG_TCSD,
    TAG_TCSF,
    TAG_TXCI,
    TAG_TWDT,
    TAG_TWHD,
    TAG_XFNM,
    tag_to_str,
)
from swg_scene.mesh_static import scene_to_blender_coords
from swg_scene.blend_targets import load_blts
from swg_scene.occlusion import SwgOcclusionData, load_occlusion_from_reader
from swg_scene.skmg_dot3 import (
    load_global_dot3,
    load_psdt_dot3_indices,
    resolve_shader_dot3,
)
from swg_scene.model import SwgMaterialRef, SwgMesh, SwgScene

Vector3 = tuple[float, float, float]


@dataclass
class TransformWeight:
    transform_index: int
    weight: float


@dataclass
class SkeletalMeshInfo:
    source_path: str = ""
    version: str = ""
    max_transforms_per_vertex: int = 0
    max_transforms_per_shader: int = 0
    skeleton_template_names: list[str] = field(default_factory=list)
    transform_names: list[str] = field(default_factory=list)
    positions: list[Vector3] = field(default_factory=list)
    position_count: int = 0
    transform_weight_data_count: int = 0
    transform_weight_counts: list[int] = field(default_factory=list)
    transform_weights: list[TransformWeight] = field(default_factory=list)
    normal_count: int = 0
    normals: list[Vector3] = field(default_factory=list)


def load_skeletal_mesh_info(path: str | Path) -> SkeletalMeshInfo:
    """Load SKMG metadata through TWHD/TWDT (+ NORM when present)."""
    p = Path(path)
    reader = IffReader.from_file(p)
    reader.enter_form(TAG_SKMG)
    try:
        version_tag = reader.current_name()
        if version_tag not in (TAG_0002, TAG_0003, TAG_0004):
            raise IffError(f"unsupported SKMG version {tag_to_str(version_tag)}")
        reader.enter_form(version_tag)
        try:
            info = _load_skinned_header(reader)
        finally:
            reader.exit_form(version_tag)
        info.source_path = str(p)
        info.version = tag_to_str(version_tag)
        return info
    finally:
        reader.exit_form(TAG_SKMG)


def load_skeletal_mesh(path: str | Path, *, blender_coords: bool = False) -> SwgScene:
    """Load `.mgn` / SKMG into scene IR (engine coordinates unless blender_coords)."""
    p = Path(path)
    reader = IffReader.from_file(p)
    reader.enter_form(TAG_SKMG)
    try:
        version_tag = reader.current_name()
        if version_tag not in (TAG_0002, TAG_0003, TAG_0004):
            raise IffError(f"unsupported SKMG version {tag_to_str(version_tag)}")
        reader.enter_form(version_tag)
        try:
            scene = _load_skmg_version(reader)
        finally:
            reader.exit_form(version_tag)
    finally:
        reader.exit_form(TAG_SKMG)

    scene.source_path = str(p)
    scene.root_form = "SKMG"
    if scene.meshes and p.stem and not scene.meshes[0].name:
        scene.meshes[0].name = p.stem
    if blender_coords:
        scene = scene_to_blender_coords(scene)
    return scene


def _load_skinned_header(reader: IffReader) -> SkeletalMeshInfo:
    header = _read_info_chunk(reader)
    skeleton_names = _read_string_chunk(reader, TAG_SKTM, header.skeleton_name_count)
    transform_names = _read_string_chunk(reader, TAG_XFNM, header.transform_name_count)
    positions = _read_vector3_chunk(reader, TAG_POSN, header.position_count)
    transform_weight_counts = _read_int32_chunk(reader, TAG_TWHD, header.position_count)
    transform_weights = _read_transform_weights(reader, header.transform_weight_data_count)
    normals = _read_optional_vector3_chunk(reader, TAG_NORM, header.normal_count)
    return SkeletalMeshInfo(
        max_transforms_per_vertex=header.max_transforms_per_vertex,
        max_transforms_per_shader=header.max_transforms_per_shader,
        skeleton_template_names=skeleton_names,
        transform_names=transform_names,
        positions=positions,
        position_count=header.position_count,
        transform_weight_data_count=header.transform_weight_data_count,
        transform_weight_counts=transform_weight_counts,
        transform_weights=transform_weights,
        normal_count=header.normal_count,
        normals=normals,
    )


def _load_skmg_version(reader: IffReader) -> SwgScene:
    header = _read_info_chunk(reader)
    skeleton_names = _read_string_chunk(reader, TAG_SKTM, header.skeleton_name_count)
    transform_names = _read_string_chunk(reader, TAG_XFNM, header.transform_name_count)
    positions = _read_vector3_chunk(reader, TAG_POSN, header.position_count)
    transform_weight_counts = _read_int32_chunk(reader, TAG_TWHD, header.position_count)
    transform_weights = _read_transform_weights(reader, header.transform_weight_data_count)
    normals = _read_optional_vector3_chunk(reader, TAG_NORM, header.normal_count)
    dot3_vectors = load_global_dot3(reader)

    pre_psdt_blocks: list[bytes] = []
    block = reader._peek_block()
    if block.is_form and block.form_type == TAG_HPTS:
        pre_psdt_blocks.append(reader.read_raw_block())

    blend_targets = []
    block = reader._peek_block()
    if block.is_form and block.form_type == TAG_BLTS:
        blend_targets = load_blts(reader, header.blend_target_count)

    occlusion = SwgOcclusionData()
    if header.occlusion_zone_name_count:
        occlusion = load_occlusion_from_reader(
            reader,
            counts=(
                header.occlusion_zone_name_count,
                header.occlusion_zone_combination_count,
                header.zones_this_occludes_count,
                header.occlusion_layer,
            ),
        )

    while not reader.at_end_of_form():
        block = reader._peek_block()
        if block.is_form and block.form_type == TAG_PSDT:
            break
        pre_psdt_blocks.append(reader.read_raw_block())

    meshes: list[SwgMesh] = []
    for _ in range(header.per_shader_data_count):
        meshes.append(
            _load_psdt(
                reader,
                positions=positions,
                normals=normals,
                transform_names=transform_names,
                skeleton_names=skeleton_names,
                transform_weight_counts=transform_weight_counts,
                transform_weights=transform_weights,
                dot3_vectors=dot3_vectors,
            )
        )

    post_psdt_blocks: list[bytes] = []
    while not reader.at_end_of_form():
        post_psdt_blocks.append(reader.read_raw_block())

    return SwgScene(
        meshes=meshes,
        dot3_vectors=dot3_vectors,
        blend_targets=blend_targets,
        occlusion=occlusion,
        skmg_occlusion_counts=(
            header.occlusion_zone_name_count,
            header.occlusion_zone_combination_count,
            header.zones_this_occludes_count,
            header.occlusion_layer,
        ),
        skmg_pre_psdt_blocks=pre_psdt_blocks,
        skmg_post_psdt_blocks=post_psdt_blocks,
    )


@dataclass
class _InfoHeader:
    max_transforms_per_vertex: int
    max_transforms_per_shader: int
    skeleton_name_count: int
    transform_name_count: int
    position_count: int
    transform_weight_data_count: int
    normal_count: int
    per_shader_data_count: int
    blend_target_count: int
    occlusion_zone_name_count: int
    occlusion_zone_combination_count: int
    zones_this_occludes_count: int
    occlusion_layer: int


def _read_info_chunk(reader: IffReader) -> _InfoHeader:
    reader.enter_chunk(TAG_INFO)
    header = _InfoHeader(
        max_transforms_per_vertex=reader.read_int32(),
        max_transforms_per_shader=reader.read_int32(),
        skeleton_name_count=reader.read_int32(),
        transform_name_count=reader.read_int32(),
        position_count=reader.read_int32(),
        transform_weight_data_count=reader.read_int32(),
        normal_count=reader.read_int32(),
        per_shader_data_count=reader.read_int32(),
        blend_target_count=reader.read_int32(),
        occlusion_zone_name_count=reader.read_int16(),
        occlusion_zone_combination_count=reader.read_int16(),
        zones_this_occludes_count=reader.read_int16(),
        occlusion_layer=reader.read_int16(),
    )
    reader.exit_chunk(TAG_INFO)
    return header


def _read_string_chunk(reader: IffReader, tag: int, count: int) -> list[str]:
    reader.enter_chunk(tag)
    values = [reader.read_string() for _ in range(count)]
    reader.exit_chunk(tag)
    return values


def _read_vector3_chunk(reader: IffReader, tag: int, count: int) -> list[Vector3]:
    reader.enter_chunk(tag)
    values = [reader.read_float_vector() for _ in range(count)]
    reader.exit_chunk(tag)
    return values


def _read_int32_chunk(reader: IffReader, tag: int, count: int) -> list[int]:
    reader.enter_chunk(tag)
    values = [reader.read_int32() for _ in range(count)]
    reader.exit_chunk(tag)
    return values


def _read_transform_weights(reader: IffReader, count: int) -> list[TransformWeight]:
    reader.enter_chunk(TAG_TWDT)
    weights = [
        TransformWeight(reader.read_int32(), reader.read_float()) for _ in range(count)
    ]
    reader.exit_chunk(TAG_TWDT)
    return weights


def _read_optional_vector3_chunk(reader: IffReader, tag: int, count: int) -> list[Vector3]:
    block = reader._peek_block()
    if block.is_form or block.tag != tag:
        return []
    reader.enter_chunk(tag)
    values = [reader.read_float_vector() for _ in range(count)]
    reader.exit_chunk(tag)
    return values


def _skip_optional_dot3_chunk(reader: IffReader) -> None:
    block = reader._peek_block()
    if block.is_form or block.tag != TAG_DOT3:
        return
    reader.enter_chunk(TAG_DOT3)
    count = reader.read_int32()
    reader.read_bytes(max(0, count) * 16)
    reader.exit_chunk(TAG_DOT3)


def _skip_until_form(reader: IffReader, form_type: int) -> None:
    while not reader.at_end_of_form():
        block = reader._peek_block()
        if block.is_form and block.form_type == form_type:
            return
        reader.skip_block()


def _weight_offsets(counts: list[int]) -> list[int]:
    offsets = [0]
    for count in counts:
        offsets.append(offsets[-1] + count)
    return offsets


def _expand_shader_vertices(
    *,
    positions: list[Vector3],
    normals: list[Vector3],
    position_indices: list[int],
    normal_indices: list[int],
    transform_weight_counts: list[int],
    transform_weights: list[TransformWeight],
) -> tuple[list[Vector3], list[Vector3], list[list[tuple[int, float]]]]:
    tw_offsets = _weight_offsets(transform_weight_counts)
    mesh_positions: list[Vector3] = []
    mesh_normals: list[Vector3] = []
    mesh_weights: list[list[tuple[int, float]]] = []

    for shader_vert, pos_idx in enumerate(position_indices):
        mesh_positions.append(positions[pos_idx])
        if normal_indices:
            mesh_normals.append(normals[normal_indices[shader_vert]])
        elif pos_idx < len(normals):
            mesh_normals.append(normals[pos_idx])
        else:
            mesh_normals.append((0.0, 0.0, 1.0))

        start = tw_offsets[pos_idx]
        count = transform_weight_counts[pos_idx]
        mesh_weights.append(
            [
                (transform_weights[start + j].transform_index, transform_weights[start + j].weight)
                for j in range(count)
            ]
        )

    return mesh_positions, mesh_normals, mesh_weights


def _triangle_area(positions: list[Vector3], i0: int, i1: int, i2: int) -> float:
    ax, ay, az = positions[i0]
    bx, by, bz = positions[i1]
    cx, cy, cz = positions[i2]
    ux, uy, uz = ax - cx, ay - cy, az - cz
    vx, vy, vz = bx - ax, by - ay, bz - az
    cxp = uy * vz - uz * vy
    cyp = uz * vx - ux * vz
    czp = ux * vy - uy * vx
    return cxp * cxp + cyp * cyp + czp * czp


def _load_indexed_tris(
    reader: IffReader,
    *,
    tag: int,
    positions: list[Vector3],
    position_indices: list[int],
    occluded: bool,
) -> tuple[list[int], list[tuple[int, int, int, int]]]:
    reader.enter_chunk(tag)
    tri_count = reader.read_int32()
    indices: list[int] = []
    occlusion_triangles: list[tuple[int, int, int, int]] = []
    for _ in range(tri_count):
        zone_index = reader.read_int16() if occluded else 0
        i0 = reader.read_int32()
        i1 = reader.read_int32()
        i2 = reader.read_int32()
        g0 = position_indices[i0]
        g1 = position_indices[i1]
        g2 = position_indices[i2]
        if _triangle_area(positions, g0, g1, g2) == 0.0:
            continue
        indices.extend((i0, i1, i2))
        if occluded:
            occlusion_triangles.append((zone_index, i0, i1, i2))
    reader.exit_chunk(tag)
    return indices, occlusion_triangles


def _load_psdt(
    reader: IffReader,
    *,
    positions: list[Vector3],
    normals: list[Vector3],
    dot3_vectors: list[tuple[float, float, float, float]],
    transform_names: list[str],
    skeleton_names: list[str],
    transform_weight_counts: list[int],
    transform_weights: list[TransformWeight],
) -> SwgMesh:
    reader.enter_form(TAG_PSDT)
    try:
        reader.enter_chunk(TAG_NAME)
        shader_path = reader.read_string()
        reader.exit_chunk(TAG_NAME)

        reader.enter_chunk(TAG_PIDX)
        shader_vert_count = reader.read_int32()
        position_indices = [reader.read_int32() for _ in range(shader_vert_count)]
        reader.exit_chunk(TAG_PIDX)

        normal_indices: list[int] = []
        block = reader._peek_block()
        if not block.is_form and block.tag == TAG_NIDX:
            reader.enter_chunk(TAG_NIDX)
            normal_indices = [reader.read_int32() for _ in range(shader_vert_count)]
            reader.exit_chunk(TAG_NIDX)

        dot3_indices = load_psdt_dot3_indices(reader, shader_vert_count)

        uvs: list[list[tuple[float, float]]] = []
        block = reader._peek_block()
        if not block.is_form and block.tag == TAG_TXCI:
            reader.enter_chunk(TAG_TXCI)
            set_count = reader.read_int32()
            dims = [reader.read_int32() for _ in range(set_count)]
            reader.exit_chunk(TAG_TXCI)
            reader.enter_form(TAG_TCSF)
            uvs = [[] for _ in range(set_count)]
            for set_idx in range(set_count):
                reader.enter_chunk(TAG_TCSD)
                for _ in range(shader_vert_count):
                    coords = [reader.read_float() for _ in range(dims[set_idx])]
                    uvs[set_idx].append((coords[0], coords[1] if len(coords) > 1 else 0.0))
                reader.exit_chunk(TAG_TCSD)
            reader.exit_form(TAG_TCSF)

        if dot3_indices and dot3_vectors:
            dot3_coords = resolve_shader_dot3(dot3_indices, dot3_vectors)
            if uvs:
                uvs.append(dot3_coords)
            else:
                uvs = [[], dot3_coords]

        indices: list[int] = []
        mesh_occlusion_triangles: list[tuple[int, int, int, int]] = []
        reader.enter_form(TAG_PRIM)
        reader.enter_chunk(TAG_INFO)
        prim_count = reader.read_int32()
        reader.exit_chunk(TAG_INFO)
        for _ in range(prim_count):
            block = reader._peek_block()
            if block.is_form:
                reader.skip_block()
                continue
            if block.tag == TAG_ITL:
                itl_indices, _ = _load_indexed_tris(
                    reader,
                    tag=TAG_ITL,
                    positions=positions,
                    position_indices=position_indices,
                    occluded=False,
                )
                indices.extend(itl_indices)
            elif block.tag == TAG_OITL:
                oitl_indices, occlusion_triangles = _load_indexed_tris(
                    reader,
                    tag=TAG_OITL,
                    positions=positions,
                    position_indices=position_indices,
                    occluded=True,
                )
                indices.extend(oitl_indices)
                mesh_occlusion_triangles = occlusion_triangles
            else:
                reader.skip_block()
        reader.exit_form(TAG_PRIM)

        mesh_positions, mesh_normals, mesh_weights = _expand_shader_vertices(
            positions=positions,
            normals=normals,
            position_indices=position_indices,
            normal_indices=normal_indices,
            transform_weight_counts=transform_weight_counts,
            transform_weights=transform_weights,
        )

        return SwgMesh(
            name=Path(shader_path).stem,
            positions=mesh_positions,
            normals=mesh_normals,
            uvs=uvs,
            indices=indices,
            material=SwgMaterialRef(
                shader_relpath=shader_path,
                has_dot3=bool(dot3_indices),
                texture_coord_sets={"MAIN": 0, "DOT3": 1} if dot3_indices else {},
            ),
            transform_names=list(transform_names),
            skeleton_template_names=list(skeleton_names),
            bone_weights=mesh_weights,
            occlusion_triangles=mesh_occlusion_triangles,
        )
    finally:
        reader.exit_form(TAG_PSDT)