"""Add-on preferences."""

from __future__ import annotations

import bpy
from bpy.props import StringProperty
from bpy.types import AddonPreferences


class SWG_AddonPreferences(AddonPreferences):
    bl_idname = "swg_blender_addon"

    pipeline_root: StringProperty(
        name="Pipeline root",
        subtype="DIR_PATH",
        description="Path to tools/swg_blender (leave empty to auto-detect next to this add-on)",
        default="",
    )
    swg_main_root: StringProperty(
        name="swg-main root",
        subtype="DIR_PATH",
        description="Optional swg-main tree root or serverdata folder (sets SWG_MAIN for shader lookup)",
        default="",
    )