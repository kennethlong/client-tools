"""SWG IFF Pipeline — Blender add-on (wraps tools/swg_blender)."""

bl_info = {
    "name": "SWG IFF Pipeline",
    "author": "swg-client-v2 contributors",
    "version": (0, 3, 0),
    "blender": (3, 0, 0),
    "location": "File > Import / Export; View3D > Sidebar > SWG",
    "description": "Import and export Star Wars Galaxies IFF assets for client testing",
    "category": "Import-Export",
}


def menu_import(self, context):
    self.layout.operator("swg.import_static_msh", text="SWG Static Mesh (.msh)")
    self.layout.operator("swg.import_skeletal_mgn", text="SWG Skeletal Mesh (.mgn)")


def menu_export(self, context):
    self.layout.operator("swg.export_static_msh", text="SWG Static Mesh (.msh)")
    self.layout.operator("swg.export_skeletal_mgn", text="SWG Skeletal Mesh (.mgn)")
    self.layout.operator("swg.export_skeleton_skt", text="SWG Skeleton (.skt)")
    self.layout.operator("swg.export_animation_ans", text="SWG Animation (.ans)")
    self.layout.separator()
    self.layout.operator("swg.export_client_bundle", text="SWG Static Client Bundle")
    self.layout.operator("swg.export_creature_bundle", text="SWG Creature Bundle")


def register():
    import bpy

    from swg_blender_addon.operators import CLASSES as OP_CLASSES
    from swg_blender_addon.panel import CLASSES as PANEL_CLASSES
    from swg_blender_addon.preferences import SWG_AddonPreferences
    from swg_blender_addon.properties import (
        CLASSES as PROP_CLASSES,
        SWGBuildingProjectSettings,
        SWGCreatureProjectSettings,
        SWGExportSettings,
    )

    bpy.utils.register_class(SWG_AddonPreferences)
    for cls in (*PROP_CLASSES, *OP_CLASSES, *PANEL_CLASSES):
        bpy.utils.register_class(cls)
    bpy.types.Scene.swg_export = bpy.props.PointerProperty(type=SWGExportSettings)
    bpy.types.Scene.swg_creature_project = bpy.props.PointerProperty(
        type=SWGCreatureProjectSettings
    )
    bpy.types.Scene.swg_building_project = bpy.props.PointerProperty(
        type=SWGBuildingProjectSettings
    )
    bpy.types.TOPBAR_MT_file_import.append(menu_import)
    bpy.types.TOPBAR_MT_file_export.append(menu_export)


def unregister():
    import bpy

    from swg_blender_addon.operators import CLASSES as OP_CLASSES
    from swg_blender_addon.panel import CLASSES as PANEL_CLASSES
    from swg_blender_addon.preferences import SWG_AddonPreferences
    from swg_blender_addon.properties import CLASSES as PROP_CLASSES

    bpy.types.TOPBAR_MT_file_import.remove(menu_import)
    bpy.types.TOPBAR_MT_file_export.remove(menu_export)
    del bpy.types.Scene.swg_building_project
    del bpy.types.Scene.swg_creature_project
    del bpy.types.Scene.swg_export
    for cls in reversed((*PANEL_CLASSES, *OP_CLASSES, *PROP_CLASSES)):
        bpy.utils.unregister_class(cls)
    bpy.utils.unregister_class(SWG_AddonPreferences)