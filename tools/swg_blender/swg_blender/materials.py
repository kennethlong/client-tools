"""Extract SWG shader build specs from Blender materials (Phase 8.8 / 10.1)."""

from __future__ import annotations

from typing import Any

from swg_pipeline.shader_builder import ShaderBuildSpec, ShaderMaterialSpec
from swg_pipeline.shader_effects import (
    alpha_reference_for_effect,
    coord_set_index_for_tag,
    infer_effect_path,
    texture_factors_for_effect,
)
from swg_pipeline.shader_stub import ShaderTextureSlot
from swg_scene.model import SwgMaterialRef

_TEXTURE_PROP_TAGS = (
    "MAIN",
    "SPEC",
    "NRML",
    "CNRM",
    "EMIS",
    "DETA",
    "ENVM",
    "MASK",
    "HUEB",
)


def _treefile_texture_path(image: Any, *, slot: str, fallback_stem: str) -> str:
    custom = image.get("swg_treefile_path") if hasattr(image, "get") else None
    if custom:
        return str(custom).replace("\\", "/")
    name = getattr(image, "name", fallback_stem) or fallback_stem
    stem = name.rsplit(".", 1)[0]
    suffix = "" if slot in ("MAIN",) else f"_{slot.lower()}"
    return f"texture/{stem}{suffix}.dds"


def _image_from_input(principled: Any, socket_name: str) -> Any | None:
    socket = principled.inputs.get(socket_name)
    if socket is None or not socket.is_linked:
        return None
    link = socket.links[0]
    from_node = link.from_node
    if from_node.type == "TEX_IMAGE" and from_node.image is not None:
        return from_node.image
    return None


def _principled_bsdf(material: Any) -> Any | None:
    if material is None:
        return None
    for node in material.node_tree.nodes:
        if node.type == "BSDF_PRINCIPLED":
            return node
    return None


def _material_has_alpha(material: Any, principled: Any | None) -> bool:
    if material is not None:
        if material.get("swg_alpha"):
            return True
        blend = getattr(material, "blend_method", None)
        if blend in ("CLIP", "HASHED", "BLEND"):
            return True
    if principled is not None:
        alpha_input = principled.inputs.get("Alpha")
        if alpha_input is not None:
            if alpha_input.is_linked:
                return True
            if float(alpha_input.default_value) < 0.999:
                return True
    return False


def _append_texture_slot(
    textures: list[ShaderTextureSlot],
    *,
    tag: str,
    path: str,
) -> None:
    tag = tag.upper()
    if any(slot.tag == tag for slot in textures):
        return
    textures.append(ShaderTextureSlot(tag=tag, path=path.replace("\\", "/")))


def build_shader_spec_from_object(obj: Any) -> ShaderBuildSpec:
    """Build SSHT spec from the mesh object's active material."""
    material = getattr(obj, "active_material", None)
    shader_path = obj.get("swg_shader_path") or f"shader/{obj.name}.sht"
    return _build_shader_spec(material, obj_name=obj.name, shader_path=shader_path)


def build_material_ref_from_object(obj: Any) -> SwgMaterialRef:
    spec = build_shader_spec_from_object(obj)
    shader_path = obj.get("swg_shader_path") or f"shader/{obj.name}.sht"
    return SwgMaterialRef(
        shader_relpath=str(shader_path),
        texture_slots={slot.tag: slot.path for slot in spec.textures},
        effect=spec.effect,
        texture_coord_sets=dict(spec.texture_coord_sets),
        has_dot3="DOT3" in spec.texture_coord_sets or "CNRM" in {s.tag for s in spec.textures},
    )


def _build_shader_spec(material: Any, *, obj_name: str, shader_path: str) -> ShaderBuildSpec:
    textures: list[ShaderTextureSlot] = []
    texture_coord_sets: dict[str, int] = {"MAIN": 0}
    principled = _principled_bsdf(material)
    diffuse = (1.0, 1.0, 1.0, 1.0)
    has_dot3 = False
    punchout = bool(material and material.get("swg_punchout"))

    if principled is not None:
        base = principled.inputs["Base Color"].default_value
        diffuse = (float(base[0]), float(base[1]), float(base[2]), float(base[3]))
        main_image = _image_from_input(principled, "Base Color")
        if main_image is not None:
            _append_texture_slot(
                textures,
                tag="MAIN",
                path=_treefile_texture_path(main_image, slot="MAIN", fallback_stem=obj_name),
            )
        for socket, tag in (
            ("Specular IOR Level", "SPEC"),
            ("Specular", "SPEC"),
            ("Emission", "EMIS"),
            ("Normal", "CNRM"),
        ):
            image = _image_from_input(principled, socket)
            if image is not None:
                _append_texture_slot(
                    textures,
                    tag=tag,
                    path=_treefile_texture_path(image, slot=tag, fallback_stem=obj_name),
                )
                if tag == "CNRM":
                    has_dot3 = True

    if not textures:
        override_main = material.get("swg_texture_MAIN") if material is not None else None
        _append_texture_slot(
            textures,
            tag="MAIN",
            path=str(override_main or f"texture/{obj_name}.dds"),
        )

    if material is not None:
        for tag in _TEXTURE_PROP_TAGS:
            custom_path = material.get(f"swg_texture_{tag}")
            if custom_path:
                _append_texture_slot(textures, tag=tag, path=str(custom_path))
            uv_index = material.get(f"swg_uv_{tag}")
            if uv_index is not None:
                texture_coord_sets[tag] = int(uv_index)
        if material.get("DOT3"):
            has_dot3 = True

    effect_override = None
    if material is not None:
        effect_override = material.get("swg_shader_effect") or material.get("Effect")

    texture_tags = {slot.tag for slot in textures}
    has_alpha = _material_has_alpha(material, principled)
    effect = infer_effect_path(
        texture_tags=texture_tags,
        effect_override=str(effect_override) if effect_override else None,
        has_alpha=has_alpha,
        punchout=punchout,
    )

    texture_factors = texture_factors_for_effect(effect, texture_tags)
    alpha_reference = alpha_reference_for_effect(effect, texture_tags)

    if has_dot3 or "CNRM" in texture_tags or "NRML" in texture_tags:
        texture_coord_sets["DOT3"] = coord_set_index_for_tag(texture_coord_sets, "DOT3", default=1)
    for tag in texture_tags:
        if tag not in texture_coord_sets:
            texture_coord_sets[tag] = coord_set_index_for_tag(texture_coord_sets, tag)

    return ShaderBuildSpec(
        effect=effect,
        material=ShaderMaterialSpec(diffuse=diffuse, ambient=diffuse),
        textures=textures,
        texture_coord_sets=texture_coord_sets,
        texture_factors=texture_factors,
        alpha_reference=alpha_reference,
    )


def infer_shader_kind(material: Any | None) -> str:
    """Return SSHT, CSHD, SWTS, or OPST based on Blender custom properties."""
    if material is None:
        return "SSHT"
    if material.get("swg_shader_opst") or material.get("swg_skin_swap"):
        return "OPST"
    if material.get("swg_shader_swts") or material.get("swg_animating_texture"):
        return "SWTS"
    if material.get("swg_shader_cshd") or material.get("swg_customizable"):
        return "CSHD"
    return "SSHT"


def build_shader_template_from_object(obj: Any):
    """Build the correct shader template spec for export (Phase 10)."""
    material = getattr(obj, "active_material", None)
    kind = infer_shader_kind(material)
    if kind == "OPST":
        from swg_pipeline.shader_extended import OpstSpec

        base = ""
        if material is not None:
            base = str(
                material.get("swg_skin_swap")
                or material.get("swg_owner_proxy_shader")
                or ""
            )
        return OpstSpec(base_shader_path=base or "shader/owner_proxy_base.sht")
    if kind == "SWTS":
        from swg_pipeline.shader_extended import SwtsSpec

        fps_min = float(material.get("swg_animating_fps_min", 1.0) or 1.0) if material else 1.0
        fps_max = float(material.get("swg_animating_fps_max", fps_min) or fps_min) if material else fps_min
        order = str(material.get("swg_animating_order", "DRTS") if material else "DRTS").upper()
        base_path = str(
            material.get("swg_swts_base_shader")
            or obj.get("swg_shader_path")
            or f"shader/{obj.name}_base.sht"
        )
        frames: list[tuple[str, str]] = []
        if material is not None:
            raw_frames = material.get("swg_animating_frames")
            if isinstance(raw_frames, str):
                for line in raw_frames.splitlines():
                    line = line.strip()
                    if not line:
                        continue
                    tag, _, path = line.partition(":")
                    frames.append((tag.strip().upper() or "MAIN", path.strip()))
        if not frames:
            stem = obj.name
            frames = [("MAIN", f"texture/{stem}_0000.dds")]
        time_max = 1.0 / max(fps_min, 0.001)
        time_min = 1.0 / max(fps_max, fps_min, 0.001)
        return SwtsSpec(
            base_shader_path=base_path,
            order=order,
            time_min=time_min,
            time_max=time_max,
            frames=frames,
        )
    if kind == "CSHD":
        from swg_pipeline.shader_extended import CshdSpec, PaletteCustomization

        base = build_shader_spec_from_object(obj)
        palettes: list[PaletteCustomization] = []
        if material is not None and material.get("swg_palette_path"):
            palettes.append(
                PaletteCustomization(
                    variable_name=str(material.get("swg_palette_var", "color")),
                    is_private=bool(material.get("swg_palette_private")),
                    texture_tag=str(material.get("swg_palette_tag", "MAIN")).upper(),
                    palette_path=str(material.get("swg_palette_path")),
                    default_index=int(material.get("swg_palette_index", 0) or 0),
                )
            )
        return CshdSpec(base=base, palettes=palettes)
    return build_shader_spec_from_object(obj)


def build_shader_template_from_mesh(mesh) -> ShaderBuildSpec:
    return shader_spec_from_mesh(mesh)


def shader_spec_from_mesh(mesh) -> ShaderBuildSpec:
    textures = [
        ShaderTextureSlot(tag=tag, path=path)
        for tag, path in mesh.material.texture_slots.items()
    ]
    if not textures:
        textures = [ShaderTextureSlot(tag="MAIN", path=f"texture/{mesh.name}.dds")]
    texture_tags = {slot.tag for slot in textures}
    effect = infer_effect_path(
        texture_tags=texture_tags,
        effect_override=mesh.material.effect or None,
    )
    return ShaderBuildSpec(
        effect=effect,
        material=ShaderMaterialSpec(),
        textures=textures,
        texture_coord_sets=dict(mesh.material.texture_coord_sets or {"MAIN": 0}),
        texture_factors=texture_factors_for_effect(effect, texture_tags),
        alpha_reference=alpha_reference_for_effect(effect, texture_tags),
    )
