"""Scene-level export settings for the SWG add-on (Phase 15)."""

from __future__ import annotations

import bpy
from bpy.props import BoolProperty, EnumProperty, StringProperty
from bpy.types import PropertyGroup


class SWGExportSettings(PropertyGroup):
    export_type: EnumProperty(
        name="Export type",
        description="What to export from the current selection",
        items=(
            ("STATIC_MSH", "Static mesh (.msh)", "Single static mesh file"),
            ("STATIC_BUNDLE", "Static client bundle", "serverdata tree + rsp for client testing"),
            ("SKELETAL_MGN", "Skeletal mesh (.mgn)", "Skinned mesh generator"),
            ("SKELETON_SKT", "Skeleton (.skt)", "Skeleton template"),
            ("ANIMATION_ANS", "Animation (.ans)", "Keyframe / CKAT animation"),
            ("CREATURE_BUNDLE", "Creature bundle", ".mgn + .skt + .sat + optional .ans"),
            ("BUILDING_BUNDLE", "Building bundle", "POB building (CLI pack for full roundtrip)"),
        ),
        default="STATIC_BUNDLE",
    )

    anim_format: EnumProperty(
        name="Animation format",
        items=(
            ("ckat", "CKAT (compressed)", "Retail compressed animation"),
            ("kfat", "KFAT (uncompressed)", "Uncompressed keyframe animation"),
        ),
        default="ckat",
    )

    author: StringProperty(
        name="Author",
        description="Stored in swg_export_manifest.json",
        default="",
    )
    export_notes: StringProperty(
        name="Export notes",
        description="Freeform notes stored in the export manifest",
        default="",
    )

    mesh_relpath: StringProperty(
        name="TreeFile mesh path",
        description="Override for bundle exports (e.g. appearance/mesh/prop.msh)",
        default="",
    )

    copy_textures: BoolProperty(
        name="Copy textures",
        description="Copy DDS textures from swg-main into the bundle",
        default=True,
    )
    ignore_textures: BoolProperty(
        name="Ignore textures",
        description="Skip texture copy even when Copy textures is enabled",
        default=False,
    )
    ignore_blend_targets: BoolProperty(
        name="Ignore blend targets",
        description="Do not export Blender shape keys as BLT/BLTS",
        default=False,
    )

    write_rsp: BoolProperty(name="Write rsp manifests", default=True)
    write_client_cfg: BoolProperty(name="Write client_search_paths.cfg", default=True)
    pack_tre: BoolProperty(
        name="Pack TRE after bundle",
        description="Requires TreeFileBuilder.exe",
        default=False,
    )
    export_sat: BoolProperty(
        name="Write .sat appearance",
        description="Include skeletal appearance template in creature bundle",
        default=True,
    )


CLASSES = (SWGExportSettings,)
