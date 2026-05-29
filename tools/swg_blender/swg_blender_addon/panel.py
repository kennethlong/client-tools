"""3D View sidebar for SWG pipeline (Phase 15 export panel)."""

from __future__ import annotations

import bpy
from bpy.types import Panel


def _settings(context: bpy.types.Context):
    return context.scene.swg_export


class SWG_PT_export(Panel):
    bl_label = "Export"
    bl_idname = "SWG_PT_export"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "SWG"
    bl_parent_id = "SWG_PT_pipeline"
    bl_options = {"DEFAULT_CLOSED"}

    def draw(self, context: bpy.types.Context) -> None:
        settings = _settings(context)
        layout = self.layout
        layout.prop(settings, "export_type")
        layout.prop(settings, "author")
        layout.prop(settings, "export_notes")

        box = layout.box()
        box.label(text="Bundle options")
        box.prop(settings, "mesh_relpath")
        box.prop(settings, "copy_textures")
        box.prop(settings, "ignore_textures")
        box.prop(settings, "write_rsp")
        box.prop(settings, "write_client_cfg")
        box.prop(settings, "pack_tre")

        skel = layout.box()
        skel.label(text="Skeletal / animation")
        skel.prop(settings, "anim_format")
        skel.prop(settings, "ignore_blend_targets")
        skel.prop(settings, "export_sat")

        if settings.export_type == "BUILDING_BUNDLE":
            info = layout.box()
            info.label(text="Building export writes POB via CLI", icon="INFO")
            info.label(text="Use hierarchy + collision/floor nodes")


class SWG_PT_creature_project(Panel):
    bl_label = "Creature project"
    bl_idname = "SWG_PT_creature_project"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "SWG"
    bl_parent_id = "SWG_PT_pipeline"
    bl_options = {"DEFAULT_CLOSED"}

    def draw(self, context: bpy.types.Context) -> None:
        settings = context.scene.swg_creature_project
        layout = self.layout
        layout.label(text="Full SAT graph (TRE round-trip)")
        layout.label(text="Collections: per-.lmg + LODs (l1+ hidden)")
        layout.prop(settings, "workspace_dir")
        layout.prop(settings, "sat_relpath")
        layout.prop(settings, "tre_path")
        layout.prop(settings, "copy_textures")
        layout.separator()
        layout.operator(
            "swg.import_creature_project",
            text="Import creature project",
            icon="IMPORT",
        )
        layout.operator(
            "swg.rebuild_creature_sat_lmg",
            text="Rebuild SAT/LMG only",
            icon="FILE_REFRESH",
        )
        box = layout.box()
        box.label(text="Export (SWG_Creature_* / nested meshes)")
        box.prop(settings, "ignore_blend_targets")
        box.prop(settings, "rebuild_rsp")
        box.prop(settings, "pack_tre")
        box.operator(
            "swg.export_creature_project",
            text="Export creature project",
            icon="EXPORT",
        )


class SWG_PT_building_project(Panel):
    bl_label = "Building project"
    bl_idname = "SWG_PT_building_project"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "SWG"
    bl_parent_id = "SWG_PT_pipeline"
    bl_options = {"DEFAULT_CLOSED"}

    def draw(self, context: bpy.types.Context) -> None:
        settings = context.scene.swg_building_project
        layout = self.layout
        layout.label(text="POB cells, floor wire, portals (TRE round-trip)")
        layout.prop(settings, "workspace_dir")
        layout.prop(settings, "pob_relpath")
        layout.prop(settings, "tre_path")
        layout.prop(settings, "copy_textures")
        layout.separator()
        layout.operator(
            "swg.import_building_project",
            text="Import building project",
            icon="IMPORT",
        )
        box = layout.box()
        box.label(text="Export (POB rewrite when manifest unchanged)")
        box.prop(settings, "rebuild_rsp")
        box.prop(settings, "pack_tre")
        box.operator(
            "swg.export_building_project",
            text="Export building project",
            icon="EXPORT",
        )


class SWG_PT_hierarchy(Panel):
    bl_label = "Hierarchy"
    bl_idname = "SWG_PT_hierarchy"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "SWG"
    bl_parent_id = "SWG_PT_pipeline"
    bl_options = {"DEFAULT_CLOSED"}

    def draw(self, context: bpy.types.Context) -> None:
        layout = self.layout
        layout.label(text="Maya naming: mesh0, c0, l0, pob")
        layout.label(text="portals/, cells/, collision, floor")
        layout.operator("swg.validate_hierarchy", icon="CHECKMARK")


class SWG_PT_pipeline(Panel):
    bl_label = "SWG Pipeline"
    bl_idname = "SWG_PT_pipeline"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "SWG"

    def draw(self, context: bpy.types.Context) -> None:
        layout = self.layout
        layout.operator("swg.run_export", text="Run Export", icon="EXPORT")
        layout.separator()
        col = layout.column(align=True)
        col.label(text="Import")
        col.operator("swg.import_static_msh", text="Static .msh", icon="IMPORT")
        col.operator("swg.import_skeletal_mgn", text="Skeletal .mgn", icon="IMPORT")
        col.separator()
        col.label(text="Quick export")
        col.operator("swg.export_static_msh", text="Static .msh", icon="EXPORT")
        col.operator("swg.export_skeletal_mgn", text="Skeletal .mgn", icon="EXPORT")
        col.operator("swg.export_skeleton_skt", text="Skeleton .skt", icon="EXPORT")
        col.operator("swg.export_animation_ans", text="Animation .ans", icon="EXPORT")
        col.operator("swg.export_client_bundle", text="Static bundle", icon="FILE_FOLDER")


CLASSES = (
    SWG_PT_pipeline,
    SWG_PT_creature_project,
    SWG_PT_building_project,
    SWG_PT_export,
    SWG_PT_hierarchy,
)
