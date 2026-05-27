"""Export Blender mesh object(s) to SWG static `.msh` (engine coordinates)."""

from __future__ import annotations

from pathlib import Path
from typing import Any

from swg_blender.hardpoints import collect_hardpoints_for_mesh
from swg_blender.materials import (
    build_material_ref_from_object,
    build_shader_spec_from_object,
    build_shader_template_from_object,
    shader_spec_from_mesh,
)
from swg_pipeline.shader_extended import ShaderBuildSpec, write_shader_template
from swg_pipeline.shader_stub import copy_shader_stub_for_mesh
from swg_scene.per_pixel_lighting import compute_mesh_dot3
from swg_pipeline.validate import validate_static_mesh
from swg_scene.color_utils import colors_are_default, rgba_float_to_packed
from swg_blender.bpy_geometry import mesh_vertex_normals_engine, mesh_vertex_positions_engine
from swg_scene.mesh_static_export import write_static_mesh_file
from swg_scene.model import SwgMaterialRef, SwgMesh, SwgScene






MAX_UV_SETS = 8


def _iter_uv_layers(mesh) -> list[Any]:
    uv_layers = getattr(mesh, "uv_layers", None)
    if uv_layers is None:
        return []
    try:
        return list(uv_layers)
    except TypeError:
        active = getattr(uv_layers, "active", None)
        return [active] if active is not None else []


def _mesh_uv_layers(mesh, dot3_coords=None):
    uvs = []
    for layer in _iter_uv_layers(mesh):
        if len(uvs) >= MAX_UV_SETS:
            break
        per_vertex = [(0.0, 0.0)] * len(mesh.vertices)
        for loop in mesh.loops:
            uv = layer.data[loop.index].uv
            per_vertex[loop.vertex_index] = (uv.x, uv.y)
        uvs.append(per_vertex)
    if dot3_coords is not None:
        uvs.append(dot3_coords)
    return uvs if uvs else [[]]


def _mesh_vertex_colors(mesh):
    color_attr = None
    if hasattr(mesh, 'color_attributes') and mesh.color_attributes:
        color_attr = mesh.color_attributes.get('Col')
        if color_attr is None and mesh.color_attributes.active_color:
            color_attr = mesh.color_attributes.active_color
    if color_attr is None:
        return []
    colors = [0xFFFFFFFF] * len(mesh.vertices)
    if color_attr.domain == 'POINT':
        for index, item in enumerate(color_attr.data):
            colors[index] = rgba_float_to_packed(*item.color)
    else:
        for loop in mesh.loops:
            item = color_attr.data[loop.index]
            colors[loop.vertex_index] = rgba_float_to_packed(*item.color)
    return [] if colors_are_default(colors) else colors

def mesh_object_to_swg_mesh(obj: Any) -> SwgMesh:
    """Convert one Blender mesh object to SwgMesh (requires bpy mesh data)."""
    mesh = obj.data
    if mesh is None or not hasattr(mesh, "vertices"):
        raise ValueError(f"object {obj.name!r} has no mesh data")

    positions = mesh_vertex_positions_engine(obj)
    normals = mesh_vertex_normals_engine(obj)
    if not normals and mesh.vertices:
        normals = [(0.0, 0.0, 1.0)] * len(mesh.vertices)

    material_ref = build_material_ref_from_object(obj)
    dot3_coords = None
    dot3_coords = compute_mesh_dot3(mesh, material=getattr(obj, "active_material", None))
    if dot3_coords is not None:
        material_ref.has_dot3 = True
    uvs = _mesh_uv_layers(mesh, dot3_coords=dot3_coords)

    indices: list[int] = []
    for poly in mesh.polygons:
        if len(poly.vertices) != 3:
            raise ValueError(
                f"mesh {obj.name!r} has non-triangle face; triangulate before export"
            )
        indices.extend(int(i) for i in poly.vertices)

    return SwgMesh(
        name=obj.name,
        positions=positions,
        normals=normals or [(0.0, 0.0, 1.0)] * len(positions),
        uvs=uvs,
        indices=indices,
        colors0=_mesh_vertex_colors(mesh),
        material=material_ref,
    )


def export_static_mesh(
    filepath: str | Path,
    *,
    objects: list[Any] | None = None,
    scene: SwgScene | None = None,
    validate_after_write: bool = False,
    shader_stub_reference: str | Path | None = None,
    shader_output_dir: str | Path | None = None,
    shader_texture_overrides: dict[str, str] | None = None,
    shader_effect: str | None = None,
) -> SwgScene:
    """
    Write `.msh` from Blender objects or a pre-built SwgScene.

    When ``shader_stub_reference`` is set, also writes a `.sht` beside the export
    tree using ``copy_shader_stub_for_mesh`` (requires ``shader_output_dir`` or
    the mesh file's parent directory).
    """
    if scene is None:
        if not objects:
            raise ValueError("provide objects or scene")
        meshes: list[SwgMesh] = []
        hardpoints = []
        for obj in objects:
            meshes.append(mesh_object_to_swg_mesh(obj))
            hardpoints.extend(collect_hardpoints_for_mesh(obj))
        scene = SwgScene(
            meshes=meshes,
            hardpoints=hardpoints,
            rebuild_appearance=True,
            optimize_indices=True,
        )

    mesh_path = Path(filepath)
    write_static_mesh_file(mesh_path, scene)
    scene.source_path = str(mesh_path)

    shader_dir = Path(shader_output_dir) if shader_output_dir else mesh_path.parent
    seen: set[str] = set()
    obj_list = objects or []
    for index, mesh in enumerate(scene.meshes):
        mesh_obj = obj_list[index] if index < len(obj_list) else None
        rel = mesh.material.shader_relpath
        if not rel or rel in seen:
            continue
        seen.add(rel)
        overrides = dict(shader_texture_overrides or {})
        overrides.update(mesh.material.texture_slots)
        effect = shader_effect or mesh.material.effect or None
        if shader_stub_reference is not None:
            copy_shader_stub_for_mesh(
                rel,
                reference=shader_stub_reference,
                output_dir=shader_dir,
                texture_overrides=overrides or None,
                effect=effect,
            )
        else:
            if mesh_obj is not None:
                spec = build_shader_template_from_object(mesh_obj)
            else:
                spec = shader_spec_from_mesh(mesh)
            if isinstance(spec, ShaderBuildSpec):
                if effect:
                    spec.effect = effect
                if overrides:
                    for slot in spec.textures:
                        if slot.tag in overrides:
                            slot.path = overrides[slot.tag]
            out = shader_dir / rel.replace("\\", "/").lstrip("/")
            write_shader_template(spec, out)

    if validate_after_write:
        result = validate_static_mesh(mesh_path)
        if not result.ok:
            raise ValueError("; ".join(result.errors))
    return scene


def export_static_mesh_from_selection(
    filepath: str | Path,
    *,
    validate: bool = False,
    shader_stub_reference: str | Path | None = None,
    shader_output_dir: str | Path | None = None,
    shader_texture_overrides: dict[str, str] | None = None,
) -> SwgScene:
    import bpy

    selected = [obj for obj in bpy.context.selected_objects if obj.type == "MESH"]
    if not selected:
        raise ValueError("no mesh objects selected")
    return export_static_mesh(
        filepath,
        objects=selected,
        validate_after_write=validate,
        shader_stub_reference=shader_stub_reference,
        shader_output_dir=shader_output_dir,
        shader_texture_overrides=shader_texture_overrides,
    )