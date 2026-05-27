"""Split meshes to respect per-shader 65535 index limit (Phase 8.9)."""

from __future__ import annotations

from copy import deepcopy

from swg_scene.model import SwgMaterialRef, SwgMesh

MAX_SHADER_INDEX_COUNT = 65535


def split_mesh_by_index_limit(mesh: SwgMesh, max_indices: int = MAX_SHADER_INDEX_COUNT) -> list[SwgMesh]:
    if max_indices <= 0:
        raise ValueError("max_indices must be positive")
    if len(mesh.indices) <= max_indices:
        return [mesh]
    if len(mesh.indices) % 3 != 0:
        raise ValueError(f"mesh {mesh.name!r} indices are not triangle-aligned")

    parts: list[SwgMesh] = []
    cursor = 0
    part_index = 0
    while cursor < len(mesh.indices):
        chunk_indices: list[int] = []
        remap: dict[int, int] = {}
        positions: list[tuple[float, float, float]] = []
        normals: list[tuple[float, float, float]] = []
        uvs = [[] for _ in mesh.uvs] if mesh.uvs else []
        colors0: list[int] = []

        while cursor < len(mesh.indices) and len(chunk_indices) + 3 <= max_indices:
            tri = mesh.indices[cursor : cursor + 3]
            for old in tri:
                if old not in remap:
                    new_index = len(positions)
                    remap[old] = new_index
                    positions.append(mesh.positions[old])
                    if mesh.normals:
                        normals.append(mesh.normals[old])
                    for set_idx, layer in enumerate(mesh.uvs):
                        if layer:
                            uvs[set_idx].append(layer[old])
                    if mesh.colors0:
                        colors0.append(mesh.colors0[old])
            chunk_indices.extend(remap[old] for old in tri)
            cursor += 3

        if not chunk_indices:
            raise ValueError(
                f"unable to split mesh {mesh.name!r}: triangle exceeds index budget"
            )

        part_name = mesh.name if part_index == 0 else f"{mesh.name}_p{part_index:02d}"
        parts.append(
            SwgMesh(
                name=part_name,
                positions=positions,
                normals=normals or [(0.0, 0.0, 1.0)] * len(positions),
                uvs=uvs or [[]],
                colors0=colors0,
                indices=chunk_indices,
                sorted_indices=[],
                material=deepcopy(mesh.material),
                transform_names=list(mesh.transform_names),
                skeleton_template_names=list(mesh.skeleton_template_names),
                bone_weights=[list(row) for row in mesh.bone_weights],
            )
        )
        part_index += 1

    return parts



def expand_meshes_for_export(meshes: list[SwgMesh], max_indices: int = MAX_SHADER_INDEX_COUNT) -> list[SwgMesh]:
    expanded: list[SwgMesh] = []
    for mesh in meshes:
        expanded.extend(split_mesh_by_index_limit(mesh, max_indices=max_indices))
    return expanded
