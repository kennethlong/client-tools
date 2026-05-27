"""Phase 10 shader effect inference tests."""

from __future__ import annotations

from swg_pipeline.shader_effects import (
    EFFECT_ALPHA,
    EFFECT_EMISMAP,
    EFFECT_SPECMAP,
    alpha_reference_for_effect,
    infer_effect_path,
    texture_factors_for_effect,
)


def test_infer_specmap_from_spec_slot():
    effect = infer_effect_path(texture_tags={"MAIN", "SPEC"}, has_alpha=False)
    assert effect == EFFECT_SPECMAP
    assert texture_factors_for_effect(effect, {"SPEC"}) == {"KCLB": 0x000000FF}
    assert alpha_reference_for_effect(effect, {"SPEC"}) == {"CEPS": 1}


def test_infer_emismap():
    effect = infer_effect_path(texture_tags={"MAIN", "EMIS"}, has_alpha=False)
    assert effect == EFFECT_EMISMAP


def test_infer_alpha():
    effect = infer_effect_path(texture_tags={"MAIN"}, has_alpha=True)
    assert effect == EFFECT_ALPHA
    assert alpha_reference_for_effect(effect, {"MAIN"}) == {"MAIN": 1}


def test_effect_override_wins():
    custom = "effect\\custom.eft"
    assert infer_effect_path(texture_tags={"MAIN"}, effect_override=custom) == custom
