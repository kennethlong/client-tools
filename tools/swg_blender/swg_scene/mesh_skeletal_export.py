"""SKMG skeletal mesh writer (SkeletalMeshGeneratorTemplate 0004)."""

from __future__ import annotations

from pathlib import Path

from swg_iff.writer import (
    make_chunk,
    make_chunk_float,
    make_chunk_float_vector,
    make_chunk_int16,
    make_chunk_int32,
    make_chunk_string,
    make_form,
)
from swg_scene.blend_targets import write_blts
from swg_scene.skmg_dot3 import (
    build_dot3_pools,
    dot3_indices_for_pool,
    shader_dot3_from_mesh_uvs,
    write_global_dot3_chunk,
    write_psdt_dot3_indices,
)
from swg_scene.model import SwgMesh, SwgScene
from swg_scene.occlusion import occlusion_counts, write_occlusion_chunks, write_oitl_chunk

Vector3 = tuple[float, float, float]
Weight = tuple[int, float]


def _quantize(value: float, *, places: int = 6) -> float:
    return round(value, places)


def _vec3_key(v: Vector3) -> tuple[float, float, float]:
    return (_quantize(v[0]), _quantize(v[1]), _quantize(v[2]))


def _weights_key(weights: list[Weight]) -> tuple[Weight, ...]:
    return tuple(sorted(weights, key=lambda w: (w[0], w[1])))


def _build_global_skin_pools(mesh: SwgMesh) -> tuple[
    list[Vector3],
    list[Vector3],
    list[int],
    list[int],
    list[int],
    list[Weight],
]:
    """Build bind-pose POSN/NORM pools and PIDX/NIDX maps (Maya SKMG layout)."""
    shader_vert_count = len(mesh.positions)
    if shader_vert_count == 0:
        raise ValueError("mesh has no vertices to export")

    normals = mesh.normals if mesh.normals else [(0.0, 0.0, 1.0)] * shader_vert_count
    if len(normals) != shader_vert_count:
        raise ValueError("normal count must match position count")
    if len(mesh.bone_weights) != shader_vert_count:
        raise ValueError("bone weight count must match position count")

    global_positions: list[Vector3] = []
    position_index: dict[Vector3, int] = {}
    position_weights: dict[int, tuple[Weight, ...]] = {}

    global_normals: list[Vector3] = []
    normal_index: dict[Vector3, int] = {}

    position_indices: list[int] = []
    normal_indices: list[int] = []
    weight_counts: list[int] = []
    flat_weights: list[Weight] = []

    for vert_idx in range(shader_vert_count):
        pos = mesh.positions[vert_idx]
        pos_key = _vec3_key(pos)
        if pos_key not in position_index:
            position_index[pos_key] = len(global_positions)
            global_positions.append(pos)
            weights = mesh.bone_weights[vert_idx]
            position_weights[position_index[pos_key]] = _weights_key(weights)
        else:
            global_idx = position_index[pos_key]
            if _weights_key(mesh.bone_weights[vert_idx]) != position_weights[global_idx]:
                raise ValueError(
                    f"shader vertices {vert_idx} and pool index {global_idx} share a bind "
                    "position but have different skin weights"
                )

        norm = normals[vert_idx]
        norm_key = _vec3_key(norm)
        if norm_key not in normal_index:
            normal_index[norm_key] = len(global_normals)
            global_normals.append(norm)

        position_indices.append(position_index[pos_key])
        normal_indices.append(normal_index[norm_key])

    for global_idx in range(len(global_positions)):
        weights = position_weights[global_idx]
        weight_counts.append(len(weights))
        flat_weights.extend(weights)

    return (
        global_positions,
        global_normals,
        position_indices,
        normal_indices,
        weight_counts,
        flat_weights,
    )


def build_skmg_bind_pools(mesh: SwgMesh):
    """Return global bind pools and index maps used by SKMG export."""
    return _build_global_skin_pools(mesh)


def write_skeletal_mesh(scene: SwgScene, *, version: str = "0004") -> bytes:
    if not scene.meshes:
        raise ValueError("scene has no meshes to export")
    if version != "0004":
        raise ValueError("only SKMG 0004 export is implemented")

    if len(scene.meshes) != 1:
        raise ValueError("only single-shader SKMG export is implemented")
    mesh = scene.meshes[0]
    (
        global_positions,
        global_normals,
        position_indices,
        normal_indices,
        weight_counts,
        flat_weights,
    ) = _build_global_skin_pools(mesh)

    transform_names = mesh.transform_names
    skeleton_names = mesh.skeleton_template_names

    max_per_vertex = max(weight_counts) if weight_counts else 0

    info_payload = b"".join(
        [
            make_chunk_int32(max_per_vertex),
            make_chunk_int32(0),
            make_chunk_int32(len(skeleton_names)),
            make_chunk_int32(len(transform_names)),
            make_chunk_int32(len(global_positions)),
            make_chunk_int32(len(flat_weights)),
            make_chunk_int32(len(global_normals)),
            make_chunk_int32(1),
            make_chunk_int32(len(scene.blend_targets)),
            *(
                make_chunk_int16(v)
                for v in (
                    occlusion_counts(scene.occlusion)
                    if scene.occlusion.zone_names
                    else scene.skmg_occlusion_counts
                )
            ),
        ]
    )
    body = make_chunk("INFO", info_payload)
    body += make_chunk("SKTM", b"".join(make_chunk_string(n) for n in skeleton_names))
    body += make_chunk("XFNM", b"".join(make_chunk_string(n) for n in transform_names))
    body += make_chunk(
        "POSN",
        b"".join(make_chunk_float_vector(*p) for p in global_positions),
    )
    body += make_chunk("TWHD", b"".join(make_chunk_int32(c) for c in weight_counts))
    twdt_payload = b"".join(
        make_chunk_int32(idx) + make_chunk_float(weight) for idx, weight in flat_weights
    )
    body += make_chunk("TWDT", twdt_payload)
    body += make_chunk(
        "NORM",
        b"".join(make_chunk_float_vector(*n) for n in global_normals),
    )

    dot3_vectors = list(scene.dot3_vectors)
    dot3_indices: list[int] = []
    shader_dot3 = shader_dot3_from_mesh_uvs(mesh)
    if shader_dot3:
        if dot3_vectors:
            dot3_indices = dot3_indices_for_pool(shader_dot3, dot3_vectors)
        else:
            dot3_vectors, dot3_indices = build_dot3_pools(shader_dot3)
    if dot3_vectors:
        body += write_global_dot3_chunk(dot3_vectors)

    body += write_blts(scene.blend_targets)
    if scene.occlusion.zone_names:
        body += write_occlusion_chunks(scene.occlusion)
    body += b"".join(scene.skmg_pre_psdt_blocks)
    body += _write_psdt(
        mesh,
        position_indices=position_indices,
        normal_indices=normal_indices,
        dot3_indices=dot3_indices,
    )
    body += b"".join(scene.skmg_post_psdt_blocks)

    return make_form("SKMG", make_form(version, body))


def write_skeletal_mesh_file(path: str | Path, scene: SwgScene) -> None:
    Path(path).write_bytes(write_skeletal_mesh(scene))


def _write_psdt(
    mesh: SwgMesh,
    *,
    position_indices: list[int],
    normal_indices: list[int],
    dot3_indices: list[int] | None = None,
) -> bytes:
    shader_path = mesh.material.shader_relpath or f"shader/{mesh.name}.sht"
    vert_count = len(mesh.positions)

    psdt = make_chunk("NAME", make_chunk_string(shader_path))
    psdt += make_chunk(
        "PIDX",
        make_chunk_int32(vert_count) + b"".join(make_chunk_int32(i) for i in position_indices),
    )
    if normal_indices:
        psdt += make_chunk("NIDX", b"".join(make_chunk_int32(i) for i in normal_indices))

    if dot3_indices:
        psdt += write_psdt_dot3_indices(dot3_indices)

    uvs = mesh.uvs[0] if mesh.uvs else []
    if uvs:
        psdt += make_chunk("TXCI", make_chunk_int32(1) + make_chunk_int32(2))
        tcsd = b"".join(make_chunk_float(u) + make_chunk_float(v) for u, v in uvs)
        psdt += make_form("TCSF", make_chunk("TCSD", tcsd))

    prim_body = make_chunk("INFO", make_chunk_int32(1))
    prim_body += _write_oitl(mesh)
    psdt += make_form("PRIM", prim_body)

    return make_form("PSDT", psdt)


def _write_oitl(mesh: SwgMesh) -> bytes:
    if mesh.occlusion_triangles:
        return write_oitl_chunk(mesh.occlusion_triangles)
    indices = mesh.indices
    if not indices:
        return make_chunk("OITL", make_chunk_int32(0))
    if len(indices) % 3:
        raise ValueError("mesh indices must be triangles")
    triangles = [
        (0, indices[i], indices[i + 1], indices[i + 2])
        for i in range(0, len(indices), 3)
    ]
    return write_oitl_chunk(triangles)