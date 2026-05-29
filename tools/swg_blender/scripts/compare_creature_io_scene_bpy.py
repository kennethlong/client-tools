"""Side-by-side abyssin creature: swg_blender vs io_scene_swg_msh (run inside Blender)."""

from __future__ import annotations

import argparse
import json
import math
import sys
import types
from pathlib import Path
from typing import Any

DEFAULT_SAT = "appearance/abyssin_m.sat"
OURS_COLL_PREFIX = "SWG_Creature_"
IO_COLL = "io_scene_abyssin_m"
# Shared mid-gray for geometry compare (swg has no materials; io defaults to near-white).
COMPARE_NEUTRAL_MAT = "SWG_Compare_Neutral"
COMPARE_NEUTRAL_RGBA = (0.55, 0.55, 0.55, 1.0)
IO_SKEL_COLL = "all_b.skt"
IO_MGN_LOD0 = (
    "appearance/mesh/abyssin_m_l0.mgn",
    "appearance/mesh/abyssin_m_body_l0.mgn",
    "appearance/mesh/abyssin_m_arms_l0.mgn",
)
TORSO_ONLY_MGN = ("appearance/mesh/abyssin_m_body_l0.mgn",)


def _ensure_repo_on_path() -> Path:
    root = Path(__file__).resolve().parents[1]
    root_str = str(root)
    if root_str not in sys.path:
        sys.path.insert(0, root_str)
    return root


def _script_argv() -> list[str]:
    if "--" in sys.argv:
        return sys.argv[sys.argv.index("--") + 1 :]
    argv = sys.argv[1:]
    if argv and argv[0].endswith(".py"):
        return argv[1:]
    return argv


def apply_io_scene_blender4_patches() -> None:
    import bpy

    if "audioop" not in sys.modules:
        mod = types.ModuleType("audioop")
        mod.cross = lambda *args, **kwargs: b""
        sys.modules["audioop"] = mod

    if not hasattr(bpy.types.Mesh, "use_auto_smooth"):

        def _get_auto_smooth(self):
            return False

        def _set_auto_smooth(self, value):
            if value and getattr(self, "polygons", None):
                for poly in self.polygons:
                    poly.use_smooth = True

        bpy.types.Mesh.use_auto_smooth = property(_get_auto_smooth, _set_auto_smooth)

    if not hasattr(bpy.types.Mesh, "normals_split_custom_set"):

        def _normals_split_custom_set(self, normals):
            if hasattr(self, "normals_split_custom_set_from_vertices") and len(normals) == len(
                self.vertices
            ):
                self.normals_split_custom_set_from_vertices(normals)

        bpy.types.Mesh.normals_split_custom_set = _normals_split_custom_set

    if not hasattr(bpy.types.Object, "face_maps"):

        class _FakeFaceMap:
            def add(self, indices):
                pass

        class _FakeFaceMaps:
            def new(self, name=""):
                return _FakeFaceMap()

        bpy.types.Object.face_maps = property(lambda self: _FakeFaceMaps())


def find_layer_collection(layer_coll: Any, name: str) -> Any | None:
    if layer_coll.collection.name == name:
        return layer_coll
    for child in layer_coll.children:
        found = find_layer_collection(child, name)
        if found:
            return found
    return None


def set_active_collection(name: str) -> Any | None:
    import bpy

    coll = bpy.data.collections.get(name)
    if coll is None:
        return None
    lc = find_layer_collection(bpy.context.view_layer.layer_collection, name)
    if lc:
        bpy.context.view_layer.active_layer_collection = lc
    return coll


def remove_collection_tree(name: str) -> None:
    import bpy

    coll = bpy.data.collections.get(name)
    if coll is None:
        return

    def _purge(c):
        for obj in list(c.objects):
            bpy.data.objects.remove(obj, do_unlink=True)
        for child in list(c.children):
            _purge(child)
            bpy.data.collections.remove(child)

    _purge(coll)
    bpy.data.collections.remove(coll)


def clear_comparison_scene() -> None:
    import bpy

    bpy.ops.wm.read_factory_settings(use_empty=True)


def purge_previous_imports(sat_stem: str) -> None:
    remove_collection_tree(f"{OURS_COLL_PREFIX}{sat_stem}")
    remove_collection_tree(IO_COLL)
    remove_collection_tree(IO_SKEL_COLL)


def _mesh_bounds(obj: Any) -> dict[str, float] | None:
    from mathutils import Vector

    if obj.type != "MESH" or not obj.data.vertices:
        return None
    mw = obj.matrix_world
    xs, ys, zs = [], [], []
    for v in obj.data.vertices:
        w = mw @ v.co
        xs.append(w.x)
        ys.append(w.y)
        zs.append(w.z)
    return {
        "verts": len(obj.data.vertices),
        "x": (min(xs), max(xs)),
        "y": (min(ys), max(ys)),
        "z": (min(zs), max(zs)),
    }


def print_side_summary(label: str, coll_name: str) -> None:
    import bpy

    coll = bpy.data.collections.get(coll_name)
    if not coll:
        print(f"{label}: (missing {coll_name})")
        return
    print(f"{label} ({coll_name}):")
    for obj in sorted(coll.all_objects, key=lambda o: o.name):
        if obj.type == "MESH":
            b = _mesh_bounds(obj)
            if b:
                print(
                    f"  {obj.name}: {b['verts']} verts "
                    f"x{b['x']} y{b['y']} z{b['z']}"
                )
        elif obj.type == "ARMATURE":
            print(f"  {obj.name}: armature ({len(obj.data.bones)} bones)")


def _ensure_neutral_compare_material(
    rgba: tuple[float, float, float, float] = COMPARE_NEUTRAL_RGBA,
) -> Any:
    import bpy

    mat = bpy.data.materials.get(COMPARE_NEUTRAL_MAT)
    if mat is None:
        mat = bpy.data.materials.new(COMPARE_NEUTRAL_MAT)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()
    emit = nodes.new("ShaderNodeEmission")
    emit.inputs["Color"].default_value = rgba
    emit.inputs["Strength"].default_value = 1.0
    out = nodes.new("ShaderNodeOutputMaterial")
    links.new(out.inputs["Surface"], emit.outputs["Emission"])
    return mat


def _retail_io_global_matrix() -> Any:
    from bpy_extras.io_utils import axis_conversion
    from mathutils import Matrix

    return Matrix.Scale(1, 4) @ axis_conversion(
        to_forward="Z", to_up="Y"
    ).to_4x4()


def _set_mesh_auto_smooth(mesh: Any, *, enabled: bool = True) -> None:
    try:
        mesh.use_auto_smooth = enabled
    except AttributeError:
        if enabled and getattr(mesh, "polygons", None):
            for poly in mesh.polygons:
                poly.use_smooth = True


def _bpy_mesh_from_io_parity_mgn(
    context: Any,
    path: Path,
    *,
    global_matrix: Any,
    neutral_mat: Any | None,
) -> Any:
    """Build one Blender mesh object from swg_scene io-parity loader (matches import_mgn)."""
    import bpy

    from swg_scene.mesh_skeletal import load_skeletal_mesh_io_parity

    mgn = load_skeletal_mesh_io_parity(path)
    mesh_name = path.stem
    mesh = bpy.data.meshes.new(mesh_name)
    blender_verts = [[p[0], p[1], p[2]] for p in mgn.positions]
    blender_norms = [[n[0], n[1], -n[2]] for n in mgn.normals]
    scene_object = bpy.data.objects.new(mesh_name, mesh)
    context.collection.objects.link(scene_object)

    tris: list[list[int]] = []
    normals: list[list[float]] = []
    for psdt in mgn.psdts:
        for face in psdt.faces:
            g0, g1, g2 = face.verts
            n0, n1, n2 = face.norms
            if max(g0, g1, g2) >= len(blender_verts) or max(n0, n1, n2) >= len(blender_norms):
                continue
            normals.append(blender_norms[n0])
            normals.append(blender_norms[n1])
            normals.append(blender_norms[n2])
            tris.append([g0, g1, g2])

    mesh.from_pydata(blender_verts, [], tris)
    _set_mesh_auto_smooth(mesh)
    if normals and len(normals) == len(mesh.loops):
        mesh.normals_split_custom_set(normals)
    mesh.transform(global_matrix)
    if neutral_mat is not None:
        mesh.materials.clear()
        mesh.materials.append(neutral_mat)
    mesh.validate()
    mesh.update()
    return scene_object


def _mgn_files_for_compare(*, torso_only: bool) -> tuple[str, ...]:
    return TORSO_ONLY_MGN if torso_only else IO_MGN_LOD0


def import_swg_mgn_io_parity(
    workspace: Path,
    sat_stem: str,
    *,
    neutral_materials: bool = True,
    torso_only: bool = False,
) -> None:
    """Import same LOD0 MGN files as io_scene (global verts + retail axis_conversion)."""
    import bpy

    apply_io_scene_blender4_patches()
    coll_name = f"{OURS_COLL_PREFIX}{sat_stem}"
    if coll_name not in bpy.data.collections:
        coll = bpy.data.collections.new(coll_name)
        bpy.context.scene.collection.children.link(coll)
    set_active_collection(coll_name)
    global_matrix = _retail_io_global_matrix()
    neutral_mat = _ensure_neutral_compare_material() if neutral_materials else None
    mgn_files = _mgn_files_for_compare(torso_only=torso_only)
    label = "torso-only" if torso_only else "3x LOD0 MGN"
    print(f"swg_blender: io-parity {label} + retail global_matrix")
    for rel in mgn_files:
        path = workspace / rel.replace("\\", "/")
        if not path.is_file():
            raise FileNotFoundError(path)
        print(f"swg io-parity: importing {rel}")
        _bpy_mesh_from_io_parity_mgn(
            bpy.context,
            path,
            global_matrix=global_matrix,
            neutral_mat=neutral_mat,
        )


def _zero_blend_shape_keys(coll_name: str) -> None:
    """io_scene import_mgn adds BLTS morphs; Blender may leave them at 1.0 (inflates belly)."""
    import bpy

    coll = bpy.data.collections.get(coll_name)
    if coll is None:
        return
    for obj in coll.all_objects:
        if obj.type != "MESH" or obj.data.shape_keys is None:
            continue
        for key_block in obj.data.shape_keys.key_blocks:
            if key_block.name != "Basis":
                key_block.value = 0.0
        obj.update_tag(refresh={"DATA"})
    bpy.context.view_layer.update()


def _apply_neutral_materials(coll_name: str) -> None:
    import bpy

    coll = bpy.data.collections.get(coll_name)
    if coll is None:
        return
    mat = _ensure_neutral_compare_material()
    for obj in coll.all_objects:
        if obj.type != "MESH" or obj.data is None:
            continue
        obj.data.materials.clear()
        obj.data.materials.append(mat)
        if getattr(obj.data, "polygons", None):
            for poly in obj.data.polygons:
                poly.use_smooth = False


def offset_collections(sat_stem: str, offset: float) -> None:
    """Separate sides on Y so the default camera sees the same profile (not mirror hemispheres)."""
    import bpy

    ours = f"{OURS_COLL_PREFIX}{sat_stem}"
    for coll_name, dy in ((ours, -offset), (IO_COLL, offset), (IO_SKEL_COLL, offset)):
        coll = bpy.data.collections.get(coll_name)
        if not coll:
            continue
        for obj in coll.all_objects:
            obj.location.x = 0.0
            obj.location.y = dy


def enable_io_scene(
    io_scene_dir: Path, workspace: Path, *, allow_install: bool = False
) -> str:
    import bpy
    import addon_utils

    apply_io_scene_blender4_patches()
    parent = str(io_scene_dir.parent)
    if parent not in sys.path:
        sys.path.insert(0, parent)
    addon_mod = "io_scene_swg"
    if addon_mod not in bpy.context.preferences.addons:
        for mod in addon_utils.modules():
            if mod.__name__ == addon_mod:
                addon_utils.enable(addon_mod, default_set=True, persistent=True)
                break
        if addon_mod not in bpy.context.preferences.addons:
            if allow_install:
                try:
                    bpy.ops.preferences.addon_install(
                        filepath=str(io_scene_dir), overwrite=True
                    )
                except TypeError:
                    bpy.ops.preferences.addon_install(filepath=str(io_scene_dir))
                bpy.ops.preferences.addon_enable(module=addon_mod)
            else:
                raise RuntimeError(
                    f"{addon_mod} is not enabled. In Blender: Edit > Preferences > Add-ons, "
                    f"install/enable 'SWG Mesh' from "
                    f"{io_scene_dir.parent}, then re-run. "
                    "Or pass --install-io-addon (uses bpy.ops; can be unstable)."
                )
    prefs = bpy.context.preferences.addons[addon_mod].preferences
    prefs.swg_root = str(workspace.resolve())
    return addon_mod


def _safe_io_import_skt(skt_path: Path) -> None:
    """io_scene import_skt bakes rotation via bpy.ops; needs a 3D View context."""
    import bpy
    from io_scene_swg import import_skt as io_import_skt

    io_import_skt.import_skt(
        bpy.context, str(skt_path), lod_0_only=True, connect_bones=False
    )
    coll = bpy.data.collections.get(IO_SKEL_COLL)
    if not coll:
        return
    arm = next((o for o in coll.objects if o.type == "ARMATURE"), None)
    if arm is None:
        return
    import math
    from swg_blender.import_skeletal import _set_active_object, _viewport_override_kwargs

    arm.rotation_euler = (math.pi / 2.0, 0.0, 0.0)
    _set_active_object(arm)
    kwargs = _viewport_override_kwargs(arm)
    if "area" in kwargs and "region" in kwargs:
        with bpy.context.temp_override(**kwargs):
            bpy.ops.object.transform_apply(location=False, scale=False, rotation=True)


def import_io_scene(
    workspace: Path,
    io_scene_dir: Path,
    skt_relpath: str,
    *,
    import_skt_file: bool,
    geometry_only: bool = True,
    allow_install: bool = False,
    torso_only: bool = False,
) -> None:
    import bpy
    from mathutils import Matrix

    apply_io_scene_blender4_patches()
    enable_io_scene(io_scene_dir, workspace, allow_install=allow_install)
    from io_scene_swg import import_mgn
    from io_scene_swg import support

    skt_path = workspace / skt_relpath.replace("\\", "/")
    if import_skt_file:
        if not skt_path.is_file():
            raise FileNotFoundError(skt_path)
        _safe_io_import_skt(skt_path)
    else:
        print("io_scene: skipping .skt import (mesh-only compare; use --io-skt to enable)")

    set_active_collection(IO_COLL)
    global_matrix = _retail_io_global_matrix()

    _orig_configure = support.configure_material_from_swg_shader

    def _configure_geometry_only(material, shader, swg_root, tex_to_png):
        # No DDS/shader nodes (crash-safe); match swg side neutral gray for compare.
        _ensure_neutral_compare_material()
        material.use_nodes = True
        bsdf = material.node_tree.nodes.get("Principled BSDF")
        if bsdf is None:
            bsdf = material.node_tree.nodes.new("ShaderNodeBsdfPrincipled")
        bsdf.inputs["Base Color"].default_value = COMPARE_NEUTRAL_RGBA
        bsdf.inputs["Roughness"].default_value = 0.6
        return None

    def _configure_material_safe(material, shader, swg_root, tex_to_png):
        return _orig_configure(material, shader, swg_root, tex_to_png)

    support.configure_material_from_swg_shader = (
        _configure_geometry_only if geometry_only else _configure_material_safe
    )
    if geometry_only:
        print("io_scene: geometry-only (no shader/DDS); use --io-materials for full materials")
    mgn_files = _mgn_files_for_compare(torso_only=torso_only)
    if torso_only:
        print("io_scene: torso-only (abyssin_m_body_l0.mgn)")
    try:
        for rel in mgn_files:
            path = workspace / rel.replace("\\", "/")
            if not path.is_file():
                raise FileNotFoundError(path)
            print(f"io_scene: importing {rel}")
            import_mgn.import_mgn(bpy.context, str(path), global_matrix=global_matrix)
    finally:
        support.configure_material_from_swg_shader = _orig_configure


def import_swg_blender(
    workspace: Path, *, build_skeleton: bool = True, reload_modules: bool = False
) -> dict[str, Any]:
    if reload_modules:
        for name in list(sys.modules):
            if name == "swg_iff" or name.startswith(
                ("swg_iff.", "swg_scene.", "swg_pipeline.", "swg_blender.")
            ):
                del sys.modules[name]
    from swg_pipeline.tre_project_bpy import import_creature_from_workspace

    return import_creature_from_workspace(
        workspace, build_skeleton=build_skeleton
    )


def run_comparison(
    workspace: Path,
    *,
    sat_relpath: str = DEFAULT_SAT,
    io_scene_dir: Path,
    offset: float = 2.5,
    clear_scene: bool = False,
    import_io_skt: bool = False,
    geometry_only: bool = True,
    allow_io_install: bool = False,
    swg_only: bool = False,
    io_only: bool = False,
    swg_build_skeleton: bool = True,
    neutral_materials: bool = True,
    use_sat_import: bool = False,
    torso_only: bool = False,
) -> dict[str, Any]:
    import bpy

    _ensure_repo_on_path()
    workspace = workspace.resolve()
    manifest_path = workspace / "swg_tre_project.json"
    if not manifest_path.is_file():
        raise FileNotFoundError(
            f"workspace missing manifest: {manifest_path}\n"
            "Materialize first: python scripts/compare_creature_io_scene.py --materialize"
        )

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    sat_stem = Path(str(manifest["root_sat"])).stem
    skel_paths = (manifest.get("graph") or {}).get("skeleton_paths") or []
    skt_relpath = str(skel_paths[0]) if skel_paths else "appearance/skeleton/all_b.skt"

    if clear_scene:
        clear_comparison_scene()
    else:
        purge_previous_imports(sat_stem)

    if not swg_only:
        try:
            enable_io_scene(io_scene_dir, workspace, allow_install=allow_io_install)
        except RuntimeError as exc:
            print(f"warning: {exc}")

    if IO_COLL not in bpy.data.collections:
        io_coll = bpy.data.collections.new(IO_COLL)
        bpy.context.scene.collection.children.link(io_coll)

    swg_result: dict[str, Any] = {}
    if not io_only:
        if use_sat_import:
            print("=== importing swg_blender (full SAT + skeleton) ===")
            swg_result = import_swg_blender(
                workspace, build_skeleton=swg_build_skeleton
            )
        else:
            print("=== importing swg_blender (io-parity MGN, default) ===")
            import_swg_mgn_io_parity(
                workspace,
                sat_stem,
                neutral_materials=neutral_materials,
                torso_only=torso_only,
            )
    if not swg_only:
        print("=== importing io_scene_swg_msh ===")
        import_io_scene(
            workspace,
            io_scene_dir,
            skt_relpath,
            import_skt_file=import_io_skt,
            geometry_only=geometry_only,
            allow_install=allow_io_install,
            torso_only=torso_only,
        )
    offset_collections(sat_stem, offset)
    print(
        f"Offset {offset} on Y axis (swg at y=-{offset}, io at y=+{offset}) "
        "so both show the same profile to the default camera"
    )
    if not swg_only:
        _zero_blend_shape_keys(IO_COLL)
        print("io_scene: zeroed blend shape keys (fat/muscle/skinny were at 1.0)")

    ours_coll = f"{OURS_COLL_PREFIX}{sat_stem}"
    if neutral_materials:
        print("Applying shared neutral gray materials (geometry compare; not retail textures)")
        if not io_only:
            _apply_neutral_materials(ours_coll)
        if not swg_only:
            _apply_neutral_materials(IO_COLL)
    if not io_only:
        print_side_summary("swg_blender", ours_coll)
    if not swg_only:
        print_side_summary("io_scene meshes", IO_COLL)
        if import_io_skt:
            print_side_summary("io_scene skeleton", IO_SKEL_COLL)

    return {
        "ok": True,
        "workspace": str(workspace),
        "sat_stem": sat_stem,
        "swg_collection": ours_coll,
        "io_scene_collection": IO_COLL,
        "io_scene_skeleton_collection": IO_SKEL_COLL,
        "offset": offset,
        "swg_import": swg_result,
    }


def main(argv: list[str] | None = None) -> int:
    import bpy

    if argv is None:
        argv = _script_argv()
    parser = argparse.ArgumentParser(
        description="Side-by-side creature compare: swg_blender vs io_scene_swg"
    )
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--sat", default=DEFAULT_SAT)
    parser.add_argument(
        "--io-scene-dir",
        type=Path,
        default=Path(r"D:\Code\io_scene_swg_msh\io_scene_swg"),
        help="Path to io_scene_swg add-on folder (contains __init__.py)",
    )
    parser.add_argument("--offset", type=float, default=2.5)
    parser.add_argument(
        "--factory-reset",
        action="store_true",
        help="bpy.ops.wm.read_factory_settings (can crash from Scripting; default is purge-only)",
    )
    parser.add_argument(
        "--io-skt",
        action="store_true",
        help="Import io_scene skeleton (.skt); off by default (mesh-only compare)",
    )
    parser.add_argument(
        "--io-materials",
        action="store_true",
        help="Build io_scene shader nodes / load DDS (can crash Blender; off by default)",
    )
    parser.add_argument(
        "--install-io-addon",
        action="store_true",
        help="bpy.ops addon_install for io_scene (off by default; enable add-on in Prefs instead)",
    )
    parser.add_argument(
        "--no-neutral-materials",
        action="store_true",
        help="Leave default viewport materials (swg=charcoal, io=near-white)",
    )
    parser.add_argument(
        "--no-swg-skeleton",
        action="store_true",
        help="With --use-sat-import: import swg meshes without building armature",
    )
    parser.add_argument(
        "--use-sat-import",
        action="store_true",
        help="Full .sat creature import (armature, per-shader meshes); default is io-parity MGN",
    )
    parser.add_argument(
        "--torso-only",
        action="store_true",
        help="Import only abyssin_m_body_l0.mgn (no l0/arms stack; clearest belly compare)",
    )
    parser.add_argument("--swg-only", action="store_true")
    parser.add_argument("--io-only", action="store_true")
    args = parser.parse_args(argv)

    result = run_comparison(
        args.workspace,
        sat_relpath=args.sat,
        io_scene_dir=args.io_scene_dir,
        offset=args.offset,
        clear_scene=args.factory_reset,
        import_io_skt=args.io_skt,
        geometry_only=not args.io_materials,
        allow_io_install=args.install_io_addon,
        swg_only=args.swg_only,
        io_only=args.io_only,
        swg_build_skeleton=not args.no_swg_skeleton,
        neutral_materials=not args.no_neutral_materials,
        use_sat_import=args.use_sat_import,
        torso_only=args.torso_only,
    )
    print(json.dumps({k: v for k, v in result.items() if k != "swg_import"}, indent=2))
    return 0 if result.get("ok") else 1


if __name__ == "__main__":
    raise SystemExit(main())