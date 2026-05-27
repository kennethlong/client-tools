"""Scene interchange representation (IFF <-> Blender)."""

from __future__ import annotations

from dataclasses import dataclass, field

from swg_scene.occlusion import SwgOcclusionData


@dataclass
class SwgMaterialRef:
    shader_relpath: str = ""
    texture_slots: dict[str, str] = field(default_factory=dict)
    effect: str = ""
    texture_coord_sets: dict[str, int] = field(default_factory=dict)
    has_dot3: bool = False




@dataclass
class SwgBlendVector:
    index: int
    delta: tuple[float, float, float]


@dataclass
class SwgBlendTargetHardpoint:
    dynamic_hardpoint_index: int
    delta_position: tuple[float, float, float]
    delta_rotation: tuple[float, float, float, float]


@dataclass
class SwgBlendTarget:
    name: str
    position_deltas: list[SwgBlendVector] = field(default_factory=list)
    normal_deltas: list[SwgBlendVector] = field(default_factory=list)
    dot3_deltas: list[SwgBlendVector] = field(default_factory=list)
    hardpoint_targets: list[SwgBlendTargetHardpoint] = field(default_factory=list)

@dataclass
class SwgHardpoint:
    name: str = ""
    matrix: tuple[
        float,
        float,
        float,
        float,
        float,
        float,
        float,
        float,
        float,
        float,
        float,
        float,
    ] = (
        1.0,
        0.0,
        0.0,
        0.0,
        0.0,
        1.0,
        0.0,
        0.0,
        0.0,
        0.0,
        1.0,
        0.0,
    )


@dataclass
class SwgDirectionSortedIndices:
    direction: tuple[float, float, float]
    indices: list[int] = field(default_factory=list)


@dataclass
class SwgMesh:
    name: str = ""
    positions: list[tuple[float, float, float]] = field(default_factory=list)
    normals: list[tuple[float, float, float]] = field(default_factory=list)
    uvs: list[list[tuple[float, float]]] = field(default_factory=list)
    colors0: list[int] = field(default_factory=list)
    indices: list[int] = field(default_factory=list)
    sorted_indices: list[SwgDirectionSortedIndices] = field(default_factory=list)
    material: SwgMaterialRef = field(default_factory=SwgMaterialRef)
    transform_names: list[str] = field(default_factory=list)
    skeleton_template_names: list[str] = field(default_factory=list)
    bone_weights: list[list[tuple[int, float]]] = field(default_factory=list)
    occlusion_triangles: list[tuple[int, int, int, int]] = field(default_factory=list)


@dataclass
class SwgJoint:
    name: str = ""
    parent_index: int = -1
    bind_translation: tuple[float, float, float] = (0.0, 0.0, 0.0)
    bind_rotation: tuple[float, float, float, float] = (0.0, 0.0, 0.0, 1.0)


@dataclass
class SwgSkeleton:
    joints: list[SwgJoint] = field(default_factory=list)


Quaternion = tuple[float, float, float, float]


@dataclass
class SwgRealKey:
    frame: int
    value: float


@dataclass
class SwgQuatKey:
    frame: int
    rotation: Quaternion


@dataclass
class SwgTransformTrack:
    name: str
    has_animated_rotation: bool = False
    rotation_channel_index: int = -1
    translation_mask: int = 0
    x_translation_channel_index: int = -1
    y_translation_channel_index: int = -1
    z_translation_channel_index: int = -1


@dataclass
class SwgClip:
    name: str = ""
    source_path: str = ""
    version: str = "0003"
    fps: float = 30.0
    frame_count: int = 0
    skeleton_template_path: str = ""
    transforms: list[SwgTransformTrack] = field(default_factory=list)
    rotation_channels: list[list[SwgQuatKey]] = field(default_factory=list)
    static_rotations: list[Quaternion] = field(default_factory=list)
    translation_channels: list[list[SwgRealKey]] = field(default_factory=list)
    static_translations: list[float] = field(default_factory=list)


@dataclass
class SwgScene:
    source_path: str = ""
    root_form: str = ""
    meshes: list[SwgMesh] = field(default_factory=list)
    skeleton: SwgSkeleton | None = None
    animations: list[SwgClip] = field(default_factory=list)
    appearance_raw: bytes | None = None
    hardpoints: list[SwgHardpoint] = field(default_factory=list)
    has_floor: bool = False
    floor_name: str = ""
    rebuild_appearance: bool = False
    optimize_indices: bool = False
    dot3_vectors: list[tuple[float, float, float, float]] = field(default_factory=list)
    blend_targets: list[SwgBlendTarget] = field(default_factory=list)
    occlusion: SwgOcclusionData = field(default_factory=SwgOcclusionData)
    skmg_occlusion_counts: tuple[int, int, int, int] = (0, 0, 0, -1)
    skmg_pre_psdt_blocks: list[bytes] = field(default_factory=list)
    skmg_post_psdt_blocks: list[bytes] = field(default_factory=list)
