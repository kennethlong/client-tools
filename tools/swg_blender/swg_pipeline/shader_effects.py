"""SWG static shader effect inference (aligned with io_scene_swg / Maya exporter heuristics)."""

from __future__ import annotations

EFFECT_SIMPLE = "effect\\a_simple.eft"
EFFECT_SPECMAP = "effect\\a_specmap.eft"
EFFECT_ALPHA = "effect\\a_alpha.eft"
EFFECT_PUNCHOUT = "effect\\a_punchout.eft"
EFFECT_EMISMAP = "effect\\a_emismap.eft"
EFFECT_SPECMAP_EMIS = "effect\\a_specmap_cbmp_emismap_bloom.eft"
EFFECT_SIMPLE_CBMP = "effect\\a_simple_cbmp.eft"
EFFECT_SPECMAP_CBMP = "effect\\a_specmap_cbmp.eft"
EFFECT_ALPHA_ENVMASK = "effect\\a_alpha_envmask.eft"
EFFECT_ALPHA_EMIS_FULLBLOOM = "effect\\a_alpha_emis_fullbloom.eft"


def infer_effect_path(
    *,
    texture_tags: set[str],
    effect_override: str | None = None,
    has_alpha: bool = False,
    punchout: bool = False,
) -> str:
    """Choose a retail effect path from texture slots and material hints."""
    if effect_override:
        return effect_override.replace("/", "\\")

    tags = {tag.upper() for tag in texture_tags}
    has_spec = "SPEC" in tags
    has_emis = "EMIS" in tags
    has_norm = "CNRM" in tags or "NRML" in tags
    has_env = "ENVM" in tags
    has_mask = "MASK" in tags

    if punchout:
        return EFFECT_PUNCHOUT
    if has_emis and has_spec and has_norm:
        return EFFECT_SPECMAP_EMIS
    if has_emis and not has_spec:
        if has_alpha:
            return EFFECT_ALPHA_EMIS_FULLBLOOM
        return EFFECT_EMISMAP
    if has_alpha:
        if has_env or has_mask:
            return EFFECT_ALPHA_ENVMASK if not has_spec else "effect\\a_alpha_envmask_spec.eft"
        return EFFECT_ALPHA
    if has_spec and has_norm:
        return EFFECT_SPECMAP_CBMP
    if has_spec:
        return EFFECT_SPECMAP
    if has_norm:
        return EFFECT_SIMPLE_CBMP
    return EFFECT_SIMPLE


def alpha_reference_for_effect(effect_path: str, texture_tags: set[str]) -> dict[str, int]:
    """ARVS alpha-reference bytes keyed by texture slot tag."""
    effect = effect_path.lower().replace("/", "\\")
    tags = {tag.upper() for tag in texture_tags}
    if "specmap" in effect or "SPEC" in tags:
        return {"CEPS": 1}
    basename = effect.rsplit("\\", 1)[-1]
    if basename.startswith("a_alpha") or "punchout" in basename or "invvis" in basename:
        return {"MAIN": 1}
    return {}


def texture_factors_for_effect(effect_path: str, texture_tags: set[str]) -> dict[str, int]:
    """TFNS texture factor map for known retail shader families."""
    effect = effect_path.lower().replace("/", "\\")
    tags = {tag.upper() for tag in texture_tags}
    if "specmap" in effect or "SPEC" in tags:
        return {"KCLB": 0x000000FF}
    return {}


def coord_set_index_for_tag(
    texture_coord_sets: dict[str, int],
    tag: str,
    *,
    default: int | None = None,
) -> int:
    if tag in texture_coord_sets:
        return texture_coord_sets[tag]
    if default is not None:
        return default
    if not texture_coord_sets:
        return 0
    return max(texture_coord_sets.values()) + 1
