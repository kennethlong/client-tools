"""Blender shape keys -> SKMG blend targets (BLT/BLTS)."""

from __future__ import annotations

from typing import Any

from swg_blender.export_static import mesh_object_to_swg_mesh
from swg_scene.blend_targets import build_blend_targets_from_pool_deltas
from swg_scene.mesh_skeletal_export import build_skmg_bind_pools
from swg_scene.model import SwgBlendTarget, SwgMesh


def _blend_target_name(key_name: str) -> str:
    name = key_name.strip()
    if name.endswith("Shape"):
        name = name[: -len("Shape")]
    return name


def _mesh_geometry_with_shape_key(obj: Any, key_block: Any) -> tuple[list, list]:
    mesh = obj.data
    basis = mesh.shape_keys.key_blocks[0]
    positions: list[tuple[float, float, float]] = []
    normals: list[tuple[float, float, float]] = []
    for index, vert in enumerate(mesh.vertices):
        co = basis.data[index].co + key_block.data[index].co
        no = basis.data[index].co  # placeholder replaced below
        _ = no
        positions.append((co.x, co.y, co.z))
    # Reuse export path for engine coords + normals on bind pose, then patch positions.
    bind = mesh_object_to_swg_mesh(obj)
    from swg_scene.coords import blender_to_engine_position, blender_to_engine_vector

    positions = [
        blender_to_engine_position(
            basis.data[i].co.x + key_block.data[i].co.x,
            basis.data[i].co.y + key_block.data[i].co.y,
            basis.data[i].co.z + key_block.data[i].co.z,
        )
        for i in range(len(mesh.vertices))
    ]
    if bind.normals:
        normals = list(bind.normals)
    else:
        mesh.calc_normals()
        normals = [
            blender_to_engine_vector(v.normal.x, v.normal.y, v.normal.z) for v in mesh.vertices
        ]
    return positions, normals


def _pool_from_shader_data(
    shader_positions: list[tuple[float, float, float]],
    shader_normals: list[tuple[float, float, float]],
    position_indices: list[int],
    normal_indices: list[int],
    pool_size: int,
) -> tuple[list[tuple[float, float, float]], list[tuple[float, float, float]]]:
    pool_positions = [(0.0, 0.0, 0.0)] * pool_size
    pool_normals = [(0.0, 0.0, 1.0)] * pool_size
    for shader_index, pool_index in enumerate(position_indices):
        pool_positions[pool_index] = shader_positions[shader_index]
    norm_indices = normal_indices or position_indices
    for shader_index, pool_index in enumerate(norm_indices):
        if shader_index < len(shader_normals):
            pool_normals[pool_index] = shader_normals[shader_index]
    return pool_positions, pool_normals


def collect_blend_targets_from_object(obj: Any, bind_mesh: SwgMesh) -> list[SwgBlendTarget]:
    """Export Blender shape keys as SKMG blend targets for a skinned mesh."""
    mesh = obj.data
    if mesh is None or mesh.shape_keys is None:
        return []

    (
        bind_positions,
        bind_normals,
        position_indices,
        normal_indices,
        _weight_counts,
        _flat_weights,
    ) = build_skmg_bind_pools(bind_mesh)
    pool_size = len(bind_positions)

    names: list[str] = []
    shape_positions: list[list[tuple[float, float, float]]] = []
    shape_normals: list[list[tuple[float, float, float]]] = []

    for key in mesh.shape_keys.key_blocks[1:]:
        if key.name == "Basis":
            continue
        shader_positions, shader_normals = _mesh_geometry_with_shape_key(obj, key)
        pool_pos, pool_norm = _pool_from_shader_data(
            shader_positions,
            shader_normals,
            position_indices,
            normal_indices,
            pool_size,
        )
        names.append(_blend_target_name(key.name))
        shape_positions.append(pool_pos)
        shape_normals.append(pool_norm)

    return build_blend_targets_from_pool_deltas(
        names=names,
        bind_positions=bind_positions,
        bind_normals=bind_normals,
        shape_positions=shape_positions,
        shape_normals=shape_normals,
    )