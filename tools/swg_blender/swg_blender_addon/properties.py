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


class SWGCreatureProjectSettings(PropertyGroup):
    """M16 multi-part creature TRE workspace (SAT -> LMG -> MGN)."""

    workspace_dir: StringProperty(
        name="Workspace",
        description="serverdata layout folder (swg_tre_project.json lives here)",
        subtype="DIR_PATH",
        default="",
    )
    sat_relpath: StringProperty(
        name="SAT path",
        description="TreeFile path to root .sat, e.g. appearance/abyssin_m.sat",
        default="appearance/abyssin_m.sat",
    )
    tre_path: StringProperty(
        name="Source TRE",
        description="Optional .tre to extract assets from (retail zlib)",
        subtype="FILE_PATH",
        default="",
    )
    copy_textures: BoolProperty(
        name="Copy textures on import",
        description="Copy DDS from shaders when materializing workspace",
        default=True,
    )
    ignore_blend_targets: BoolProperty(
        name="Ignore blend targets",
        description="Skip shape keys when exporting .mgn from Blender",
        default=False,
    )
    rebuild_rsp: BoolProperty(
        name="Rebuild rsp on export",
        description="Regenerate rsp/*.rsp after export",
        default=True,
    )
    pack_tre: BoolProperty(
        name="Pack TRE after export",
        description="Run TreeFileBuilder after export (requires exe)",
        default=False,
    )


class SWGBuildingProjectSettings(PropertyGroup):
    """M17 building TRE workspace (POB -> cells -> mesh)."""

    workspace_dir: StringProperty(
        name="Workspace",
        subtype="DIR_PATH",
        default="",
    )
    pob_relpath: StringProperty(
        name="POB path",
        description="TreeFile path e.g. appearance/echo_base_pob.pob",
        default="appearance/echo_base_pob.pob",
    )
    tre_path: StringProperty(
        name="Source TRE",
        subtype="FILE_PATH",
        default="",
    )
    copy_textures: BoolProperty(name="Copy textures on import", default=True)
    rebuild_rsp: BoolProperty(name="Rebuild rsp on export", default=True)
    pack_tre: BoolProperty(name="Pack TRE after export", default=False)


CLASSES = (SWGExportSettings, SWGCreatureProjectSettings, SWGBuildingProjectSettings)
