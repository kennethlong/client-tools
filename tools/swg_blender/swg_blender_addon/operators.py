"""Blender operators for SWG IFF import/export."""

from __future__ import annotations

import re
from pathlib import Path

import bpy
from bpy.props import BoolProperty, EnumProperty, StringProperty
from bpy.types import Operator
from bpy_extras.io_utils import ExportHelper, ImportHelper

from swg_blender_addon.path_util import ensure_pipeline_on_path

_SAFE_NAME = re.compile(r"[^A-Za-z0-9._-]+")


def _safe_stem(context: bpy.types.Context, default: str = "swg_export") -> str:
    obj = context.active_object
    if obj is None:
        return default
    stem = _SAFE_NAME.sub("_", obj.name).strip("._")
    return stem or default


def _report_exception(op: Operator, exc: BaseException) -> set[str]:
    op.report({"ERROR"}, str(exc))
    return {"CANCELLED"}


def _swg_settings(context: bpy.types.Context):
    return context.scene.swg_export


def _finalize_manifest(bundle_root: Path, context: bpy.types.Context) -> None:
    settings = _swg_settings(context)
    if not settings.author and not settings.export_notes:
        return
    ensure_pipeline_on_path()
    from swg_pipeline.export_manifest import apply_manifest_author_notes

    for name in ("swg_export_manifest.json", "swg_phase7_manifest.json"):
        manifest = bundle_root / name
        if manifest.is_file():
            apply_manifest_author_notes(
                manifest,
                author=settings.author.strip(),
                notes=settings.export_notes.strip(),
            )
            return


def _apply_swg_main_env() -> None:
    try:
        import os

        addon = bpy.context.preferences.addons.get("swg_blender_addon")
        if addon is None:
            return
        raw = getattr(addon.preferences, "swg_main_root", "").strip()
        if not raw:
            return
        path = Path(raw)
        if path.name.lower() == "serverdata":
            os.environ["SWG_MAIN"] = str(path.parent)
        else:
            os.environ["SWG_MAIN"] = str(path)
    except Exception:
        pass


class SWG_OT_import_static_msh(Operator, ImportHelper):
    bl_idname = "swg.import_static_msh"
    bl_label = "SWG Static Mesh (.msh)"
    bl_options = {"REGISTER", "UNDO"}

    filename_ext = ".msh"
    filter_glob: StringProperty(default="*.msh", options={"HIDDEN"})

    def execute(self, context: bpy.types.Context) -> set[str]:
        try:
            ensure_pipeline_on_path()
            _apply_swg_main_env()
            from swg_blender.import_static import import_static_mesh

            import_static_mesh(self.filepath)
        except Exception as exc:
            return _report_exception(self, exc)
        self.report({"INFO"}, f"Imported {Path(self.filepath).name}")
        return {"FINISHED"}


class SWG_OT_import_skeletal_mgn(Operator, ImportHelper):
    bl_idname = "swg.import_skeletal_mgn"
    bl_label = "SWG Skeletal Mesh (.mgn)"
    bl_options = {"REGISTER", "UNDO"}

    filename_ext = ".mgn"
    filter_glob: StringProperty(default="*.mgn", options={"HIDDEN"})

    def execute(self, context: bpy.types.Context) -> set[str]:
        try:
            ensure_pipeline_on_path()
            _apply_swg_main_env()
            from swg_blender.import_skeletal import import_skeletal_mesh

            import_skeletal_mesh(self.filepath)
        except Exception as exc:
            return _report_exception(self, exc)
        self.report({"INFO"}, f"Imported {Path(self.filepath).name}")
        return {"FINISHED"}


class SWG_OT_export_static_msh(Operator, ExportHelper):
    bl_idname = "swg.export_static_msh"
    bl_label = "SWG Static Mesh (.msh)"
    bl_options = {"REGISTER"}

    filename_ext = ".msh"
    filter_glob: StringProperty(default="*.msh", options={"HIDDEN"})

    def execute(self, context: bpy.types.Context) -> set[str]:
        try:
            ensure_pipeline_on_path()
            from swg_blender.export_static import export_static_mesh_from_selection

            export_static_mesh_from_selection(self.filepath)
        except Exception as exc:
            return _report_exception(self, exc)
        self.report({"INFO"}, f"Exported {Path(self.filepath).name}")
        return {"FINISHED"}


class SWG_OT_export_skeletal_mgn(Operator, ExportHelper):
    bl_idname = "swg.export_skeletal_mgn"
    bl_label = "SWG Skeletal Mesh (.mgn)"
    bl_options = {"REGISTER"}

    filename_ext = ".mgn"
    filter_glob: StringProperty(default="*.mgn", options={"HIDDEN"})

    def execute(self, context: bpy.types.Context) -> set[str]:
        try:
            ensure_pipeline_on_path()
            from swg_blender.export_skeletal import export_skeletal_mesh_from_selection

            export_skeletal_mesh_from_selection(self.filepath)
        except Exception as exc:
            return _report_exception(self, exc)
        self.report({"INFO"}, f"Exported {Path(self.filepath).name}")
        return {"FINISHED"}


class SWG_OT_export_skeleton_skt(Operator, ExportHelper):
    bl_idname = "swg.export_skeleton_skt"
    bl_label = "SWG Skeleton (.skt)"
    bl_options = {"REGISTER"}

    filename_ext = ".skt"
    filter_glob: StringProperty(default="*.skt", options={"HIDDEN"})

    def execute(self, context: bpy.types.Context) -> set[str]:
        try:
            ensure_pipeline_on_path()
            from swg_blender.export_skeletal import export_skeleton_from_selection

            export_skeleton_from_selection(self.filepath)
        except Exception as exc:
            return _report_exception(self, exc)
        self.report({"INFO"}, f"Exported {Path(self.filepath).name}")
        return {"FINISHED"}


class SWG_OT_export_animation_ans(Operator, ExportHelper):
    bl_idname = "swg.export_animation_ans"
    bl_label = "SWG Animation (.ans)"
    bl_options = {"REGISTER"}

    filename_ext = ".ans"
    filter_glob: StringProperty(default="*.ans", options={"HIDDEN"})

    anim_format: EnumProperty(
        name="Format",
        items=(
            ("ckat", "CKAT (retail compressed)", ""),
            ("kfat", "KFAT (uncompressed)", ""),
        ),
        default="ckat",
    )

    def execute(self, context: bpy.types.Context) -> set[str]:
        try:
            ensure_pipeline_on_path()
            from swg_blender.export_animation import export_animation_from_selection

            anim_format = self.anim_format or _swg_settings(context).anim_format
            export_animation_from_selection(self.filepath, format=anim_format)
        except Exception as exc:
            return _report_exception(self, exc)
        self.report({"INFO"}, f"Exported {Path(self.filepath).name} ({anim_format})")
        return {"FINISHED"}


class SWG_OT_export_client_bundle(Operator):
    bl_idname = "swg.export_client_bundle"
    bl_label = "SWG Client Test Bundle"
    bl_options = {"REGISTER"}

    directory: StringProperty(name="Bundle folder", subtype="DIR_PATH")
    mesh_relpath: StringProperty(
        name="TreeFile mesh path",
        default="",
        description="e.g. appearance/mesh/my_prop.msh (default: appearance/mesh/<object>.msh)",
    )
    copy_textures: BoolProperty(name="Copy textures from swg-main", default=True)
    write_rsp: BoolProperty(name="Write rsp/ manifests", default=True)
    write_client_cfg: BoolProperty(name="Write client_search_paths.cfg", default=True)
    pack_tre: BoolProperty(name="Pack TRE after export", default=False)

    def invoke(self, context: bpy.types.Context, event: bpy.types.Event) -> set[str]:
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}

    def execute(self, context: bpy.types.Context) -> set[str]:
        if not self.directory:
            self.report({"ERROR"}, "Choose an output directory")
            return {"CANCELLED"}

        try:
            ensure_pipeline_on_path()
            _apply_swg_main_env()
            from swg_blender.export_static import export_static_mesh_from_selection
            from swg_pipeline.export_bundle import export_static_bundle

            settings = _swg_settings(context)
            root = Path(self.directory)
            stem = _safe_stem(context)
            mesh_rel = (
                self.mesh_relpath.strip()
                or settings.mesh_relpath.strip()
                or f"appearance/mesh/{stem}.msh"
            )
            mesh_rel = mesh_rel.replace("\\", "/").lstrip("/")
            mesh_path = root / mesh_rel
            mesh_path.parent.mkdir(parents=True, exist_ok=True)

            scene = export_static_mesh_from_selection(mesh_path)
            shader_ref = None
            if scene.meshes and scene.meshes[0].material.shader_relpath:
                from swg_pipeline.swg_main_paths import shader_template

                shader_ref = shader_template(scene.meshes[0].material.shader_relpath)

            copy_textures = self.copy_textures and not settings.ignore_textures
            export_static_bundle(
                scene,
                root,
                mesh_relpath=mesh_rel,
                shader_reference=shader_ref,
                copy_textures=copy_textures,
                write_rsp=self.write_rsp or settings.write_rsp,
                write_client_cfg=self.write_client_cfg or settings.write_client_cfg,
            )
            _finalize_manifest(root, context)
            if self.pack_tre or settings.pack_tre:
                from swg_pipeline.pack_pipeline import pack_bundle

                pack_bundle(root, rebuild_rsp=False)
        except Exception as exc:
            return _report_exception(self, exc)

        self.report({"INFO"}, f"Bundle written to {self.directory}")
        return {"FINISHED"}


class SWG_OT_export_creature_bundle(Operator):
    bl_idname = "swg.export_creature_bundle"
    bl_label = "SWG Creature Bundle"
    bl_options = {"REGISTER"}

    directory: StringProperty(name="Bundle folder", subtype="DIR_PATH")

    def invoke(self, context: bpy.types.Context, event: bpy.types.Event) -> set[str]:
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}

    def execute(self, context: bpy.types.Context) -> set[str]:
        if not self.directory:
            self.report({"ERROR"}, "Choose an output directory")
            return {"CANCELLED"}
        try:
            ensure_pipeline_on_path()
            _apply_swg_main_env()
            settings = _swg_settings(context)
            from swg_blender.export_animation import export_animation_from_selection
            from swg_blender.export_skeletal import (
                export_skeleton_from_selection,
                export_skeletal_mesh_from_selection,
            )
            from swg_pipeline.export_bundle import export_skeletal_bundle_from_files

            root = Path(self.directory)
            stem = _safe_stem(context)
            mesh_path = root / f"appearance/mesh/{stem}.mgn"
            skel_path = root / f"appearance/skeleton/{stem}.skt"
            mesh_path.parent.mkdir(parents=True, exist_ok=True)
            skel_path.parent.mkdir(parents=True, exist_ok=True)

            export_skeletal_mesh_from_selection(
                mesh_path,
                ignore_blend_targets=settings.ignore_blend_targets,
            )
            export_skeleton_from_selection(skel_path)

            anim_sources: list[Path] = []
            if context.active_object and context.active_object.animation_data:
                anim_path = root / f"appearance/animation/{stem}.ans"
                anim_path.parent.mkdir(parents=True, exist_ok=True)
                try:
                    export_animation_from_selection(
                        anim_path, format=settings.anim_format
                    )
                    anim_sources.append(anim_path)
                except Exception:
                    pass

            export_skeletal_bundle_from_files(
                mesh_path,
                skel_path,
                root,
                animation_sources=anim_sources or None,
                copy_textures=settings.copy_textures and not settings.ignore_textures,
                write_rsp=settings.write_rsp,
                write_client_cfg=settings.write_client_cfg,
                write_appearance=settings.export_sat,
            )
            _finalize_manifest(root, context)
            if settings.pack_tre:
                from swg_pipeline.pack_pipeline import pack_bundle

                pack_bundle(root, rebuild_rsp=False)
        except Exception as exc:
            return _report_exception(self, exc)
        self.report({"INFO"}, f"Creature bundle written to {self.directory}")
        return {"FINISHED"}


class SWG_OT_run_export(Operator):
    bl_idname = "swg.run_export"
    bl_label = "Run SWG Export"
    bl_options = {"REGISTER"}

    directory: StringProperty(subtype="DIR_PATH")
    filepath: StringProperty(subtype="FILE_PATH")

    def invoke(self, context: bpy.types.Context, event: bpy.types.Event) -> set[str]:
        settings = _swg_settings(context)
        if settings.export_type.endswith("_BUNDLE"):
            context.window_manager.fileselect_add(self)
        else:
            context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}

    def execute(self, context: bpy.types.Context) -> set[str]:
        settings = _swg_settings(context)
        export_type = settings.export_type

        if export_type == "STATIC_BUNDLE":
            if not self.directory:
                return {"CANCELLED"}
            bpy.ops.swg.export_client_bundle("EXEC_DEFAULT", directory=self.directory)
            return {"FINISHED"}

        if export_type == "CREATURE_BUNDLE":
            if not self.directory:
                return {"CANCELLED"}
            bpy.ops.swg.export_creature_bundle("EXEC_DEFAULT", directory=self.directory)
            return {"FINISHED"}

        if export_type == "BUILDING_BUNDLE":
            self.report(
                {"INFO"},
                "Building POB: validate hierarchy in scene, export collision/floor via CLI; "
                "pack with pack_pipeline",
            )
            bpy.ops.swg.validate_hierarchy()
            return {"FINISHED"}

        if not self.filepath:
            self.report({"ERROR"}, "Choose an output file")
            return {"CANCELLED"}

        path = Path(self.filepath)
        try:
            if export_type == "STATIC_MSH":
                bpy.ops.swg.export_static_msh("EXEC_DEFAULT", filepath=str(path))
            elif export_type == "SKELETAL_MGN":
                ensure_pipeline_on_path()
                from swg_blender.export_skeletal import export_skeletal_mesh_from_selection

                export_skeletal_mesh_from_selection(
                    path,
                    ignore_blend_targets=settings.ignore_blend_targets,
                )
            elif export_type == "SKELETON_SKT":
                bpy.ops.swg.export_skeleton_skt("EXEC_DEFAULT", filepath=str(path))
            elif export_type == "ANIMATION_ANS":
                ensure_pipeline_on_path()
                from swg_blender.export_animation import export_animation_from_selection

                export_animation_from_selection(path, format=settings.anim_format)
            else:
                self.report({"ERROR"}, f"Unknown export type {export_type}")
                return {"CANCELLED"}
        except Exception as exc:
            return _report_exception(self, exc)

        self.report({"INFO"}, f"Exported {path.name}")
        return {"FINISHED"}


class SWG_OT_validate_hierarchy(Operator):
    bl_idname = "swg.validate_hierarchy"
    bl_label = "Validate Export Hierarchy"
    bl_options = {"REGISTER"}

    def execute(self, context: bpy.types.Context) -> set[str]:
        try:
            ensure_pipeline_on_path()
            from swg_scene.hierarchy import validate_hierarchy_names
        except Exception as exc:
            return _report_exception(self, exc)

        names: list[str] = []
        for obj in context.selected_objects:
            if obj.parent is None:
                names.append(obj.name)
            for child in obj.children:
                names.append(child.name)
        if not names and context.collection:
            names = [obj.name for obj in context.collection.objects if obj.parent is None]

        report = validate_hierarchy_names(names)
        if report.ok:
            self.report({"INFO"}, f"Hierarchy OK ({len(names)} root child name(s) checked)")
            return {"FINISHED"}
        for issue in report.issues[:5]:
            self.report({"ERROR"}, f"{issue.path}: {issue.message}")
        if len(report.issues) > 5:
            self.report({"WARNING"}, f"{len(report.issues) - 5} more issue(s)")
        return {"CANCELLED"}


CLASSES = (
    SWG_OT_import_static_msh,
    SWG_OT_import_skeletal_mgn,
    SWG_OT_export_static_msh,
    SWG_OT_export_skeletal_mgn,
    SWG_OT_export_skeleton_skt,
    SWG_OT_export_animation_ans,
    SWG_OT_export_client_bundle,
    SWG_OT_export_creature_bundle,
    SWG_OT_run_export,
    SWG_OT_validate_hierarchy,
)