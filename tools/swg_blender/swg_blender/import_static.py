"""Import SWG static mesh (.msh) into Blender via bpy."""

from __future__ import annotations

import math
import sys
from pathlib import Path
from typing import Any

from swg_scene.mesh_static import load_static_mesh, scene_to_blender_coords
from swg_scene.model import SwgMesh, SwgScene

# Validated against retail furniture props: X-flip verts + this object rotation.
IMPORT_ROTATION_EULER = (math.pi / 2.0, 0.0, 0.0)


def _ensure_repo_on_path() -> None:
    root = Path(__file__).resolve().parents[1]
    root_str = str(root)
    if root_str not in sys.path:
        sys.path.insert(0, root_str)


def load_msh(path: str | Path, *, blender_coords: bool = True) -> SwgScene:
    _ensure_repo_on_path()
    scene = load_static_mesh(path)
    if blender_coords:
        return scene_to_blender_coords(scene)
    return scene


def mesh_to_blender_bmesh(mesh: SwgMesh, *, name: str | None = None) -> Any:
    """Create a Blender mesh datablock from SwgMesh. Requires bpy."""
    import bpy

    obj_name = name or mesh.name or "swg_mesh"
    # from_pydata keeps duplicate triangles; bmesh.faces.new silently dedupes them.
    faces = [
        tuple(mesh.indices[i : i + 3])
        for i in range(0, len(mesh.indices), 3)
    ]
    me = bpy.data.meshes.new(obj_name)
    me.from_pydata(mesh.positions, [], faces)
    me.update()

    if mesh.uvs and mesh.uvs[0]:
        uv_layer = me.uv_layers.new(name="UVMap")
        for poly in me.polygons:
            for loop_idx in poly.loop_indices:
                vi = me.loops[loop_idx].vertex_index
                if vi < len(mesh.uvs[0]):
                    uv_layer.data[loop_idx].uv = mesh.uvs[0][vi]

    if mesh.normals and len(mesh.normals) == len(mesh.positions):
        try:
            me.use_auto_smooth = True  # Blender 3.x
        except AttributeError:
            pass
        me.normals_split_custom_set_from_vertices(mesh.normals)
        for poly in me.polygons:
            poly.use_smooth = True

    obj = bpy.data.objects.new(obj_name, me)
    if mesh.material.shader_relpath:
        obj["swg_shader_path"] = mesh.material.shader_relpath

    return obj


def import_static_mesh(
    filepath: str | Path,
    *,
    collection: Any | None = None,
    object_name: str | None = None,
    apply_import_rotation: bool = True,
) -> list[Any]:
    """Import one `.msh` file; returns created Blender objects."""
    import bpy

    scene = load_msh(filepath)
    if collection is None:
        collection = bpy.context.collection

    objects: list[Any] = []
    stem = Path(filepath).stem
    for idx, mesh in enumerate(scene.meshes):
        name = object_name or (mesh.name if len(scene.meshes) == 1 else f"{stem}_{idx}")
        obj = mesh_to_blender_bmesh(mesh, name=name)
        if apply_import_rotation:
            obj.rotation_euler = IMPORT_ROTATION_EULER
        collection.objects.link(obj)
        objects.append(obj)
    return objects


def main(argv: list[str] | None = None) -> int:
    import argparse

    parser = argparse.ArgumentParser(description="Import SWG static mesh (.msh)")
    parser.add_argument("msh_path", type=Path)
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Parse only; print stats (no Blender required)",
    )
    args = parser.parse_args(argv)

    scene = load_msh(args.msh_path, blender_coords=not args.dry_run)
    for mesh in scene.meshes:
        tris = len(mesh.indices) // 3 if mesh.indices else 0
        print(
            f"{mesh.name}: {len(mesh.positions)} verts, {tris} tris, "
            f"shader={mesh.material.shader_relpath!r}"
        )
    if args.dry_run:
        return 0

    try:
        import bpy  # noqa: F401
    except ImportError:
        print("bpy not available - use --dry-run or run inside Blender", file=sys.stderr)
        return 1

    import_static_mesh(args.msh_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())