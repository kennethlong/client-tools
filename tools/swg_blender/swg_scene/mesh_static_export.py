"""Static mesh (MESH / SPS) writer — port of ShaderPrimitiveSetWriter + MeshBuilder."""

from __future__ import annotations

from pathlib import Path

from swg_iff.writer import (
    make_chunk,
    make_chunk_bool8,
    make_chunk_int32,
    make_chunk_string,
    make_form,
)
from swg_scene.appearance_export import write_appearance_0003
from swg_scene.index_optimize import optimize_triangle_list
from swg_scene.index_split import expand_meshes_for_export
from swg_scene.index_sort import build_sorted_index_arrays, mesh_has_vertex_alpha
from swg_scene.mesh_static import SPSPT_INDEXED_TRIANGLE_LIST
from swg_scene.model import SwgDirectionSortedIndices, SwgMesh, SwgScene
from swg_scene.vertex_buffer import (
    write_index_buffer_uint16,
    write_sorted_index_buffer,
    write_vertex_buffer_vtxa,
)


def write_static_mesh(scene: SwgScene) -> bytes:
    """Serialize scene IR (engine coordinates) to `.msh` bytes."""
    if not scene.meshes:
        raise ValueError("scene has no meshes to export")

    mesh_body = b""
    if scene.appearance_raw and not scene.rebuild_appearance:
        mesh_body += scene.appearance_raw
    else:
        mesh_body += write_appearance_0003(
            meshes=scene.meshes,
            hardpoints=scene.hardpoints,
            has_floor=scene.has_floor,
            floor_name=scene.floor_name,
        )
    export_meshes = expand_meshes_for_export(scene.meshes)
    mesh_body += _write_sps(export_meshes, optimize_indices=scene.optimize_indices)

    return make_form("MESH", make_form("0005", mesh_body))


def write_static_mesh_file(path: str | Path, scene: SwgScene) -> None:
    p = Path(path)
    p.write_bytes(write_static_mesh(scene))


def _write_sps(meshes: list[SwgMesh], *, optimize_indices: bool = False) -> bytes:
    shader_groups = b"".join(
_write_shader_group(index, mesh, optimize_indices=optimize_indices)
        for index, mesh in enumerate(meshes)
    )
    sps_body = make_chunk("CNT ", make_chunk_int32(len(meshes))) + shader_groups
    return make_form("SPS ", make_form("0001", sps_body))


def _write_shader_group(index: int, mesh: SwgMesh, *, optimize_indices: bool = False) -> bytes:
    form_tag = f"{index + 1:04d}"
    shader_path = mesh.material.shader_relpath or f"shader/{mesh.name}.sht"
    primitive = _write_shader_primitive(mesh, optimize_indices=optimize_indices)
    body = (
        make_chunk("NAME", make_chunk_string(shader_path))
        + make_chunk("INFO", make_chunk_int32(1))
        + primitive
    )
    return make_form(form_tag, body)


def _write_shader_primitive(mesh: SwgMesh, *, optimize_indices: bool = False) -> bytes:
    indices = _prepare_indices(mesh, optimize_indices=optimize_indices)
    sorted_arrays = _resolve_sorted_indices(mesh, indices)
    has_sorted_indices = bool(sorted_arrays)
    colors0 = mesh.colors0 if mesh.colors0 else None
    info = (
        make_chunk_int32(SPSPT_INDEXED_TRIANGLE_LIST)
        + make_chunk_bool8(bool(indices))
        + make_chunk_bool8(has_sorted_indices)
    )
    body = (
        make_chunk("INFO", info)
        + write_vertex_buffer_vtxa(
            mesh.positions,
            mesh.normals,
            mesh.uvs,
            colors0=colors0,
        )
    )
    if indices:
        body += make_chunk("INDX", write_index_buffer_uint16(indices))
    if has_sorted_indices:
        payload = write_sorted_index_buffer(
            [(entry.direction, entry.indices) for entry in sorted_arrays]
        )
        body += make_chunk("SIDX", payload)
    return make_form("0001", body)


def _prepare_indices(mesh: SwgMesh, *, optimize_indices: bool) -> list[int]:
    from swg_scene.index_optimize import estimate_cache_misses

    indices = list(mesh.indices)
    if optimize_indices and len(indices) >= 6:
        optimized = optimize_triangle_list(indices)
        if estimate_cache_misses(optimized) <= estimate_cache_misses(indices):
            indices = optimized
    return indices


def _resolve_sorted_indices(
    mesh: SwgMesh,
    indices: list[int],
) -> list[SwgDirectionSortedIndices]:
    if mesh.sorted_indices and indices == mesh.indices:
        return mesh.sorted_indices
    if not indices or not mesh_has_vertex_alpha(mesh.colors0):
        return []
    return [
        SwgDirectionSortedIndices(direction=direction, indices=sorted_indices)
        for direction, sorted_indices in build_sorted_index_arrays(mesh.positions, indices)
    ]