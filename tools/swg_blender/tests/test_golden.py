"""Regression tests against real serverdata meshes (swg-main or golden copy)."""

from __future__ import annotations

from pathlib import Path

import pytest

from swg_iff.dump import dump_tree
from swg_iff.reader import IffReader, root_form_type, validate_iff
from swg_iff.tags import TAG_CKAT, TAG_KFAT, TAG_MESH, TAG_SLOD, tag_from_str

GOLDEN_DIR = Path(__file__).parent / "golden"
GOLDEN_MSH = GOLDEN_DIR / "frn_all_bed_sm_s1_l0.msh"
GOLDEN_ANS = GOLDEN_DIR / "all_b_cbt_pistol_run_ready.ans"
GOLDEN_SLOD = GOLDEN_DIR / "all_b.skt"
GOLDEN_CKAT = GOLDEN_DIR / "all_b_emt_jump_add.ans"


def _resolve_golden_msh() -> Path:
    if GOLDEN_MSH.is_file():
        return GOLDEN_MSH
    from swg_pipeline.swg_main_paths import appearance_mesh

    live = appearance_mesh("frn_all_bed_sm_s1_l0.msh")
    if live:
        return live
    pytest.skip(
        "No golden mesh: copy from swg-main/serverdata or set SWG_MAIN env var"
    )


def _resolve_golden_ans() -> Path:
    if GOLDEN_ANS.is_file():
        return GOLDEN_ANS
    from swg_pipeline.swg_main_paths import appearance_animation

    live = appearance_animation("all_b_cbt_pistol_run_ready.ans")
    if live:
        return live
    pytest.skip(
        "No golden KFAT animation: copy all_b_cbt_pistol_run_ready.ans to tests/golden/"
    )


def _resolve_golden_ckat() -> Path:
    if GOLDEN_CKAT.is_file():
        return GOLDEN_CKAT
    from swg_pipeline.swg_main_paths import appearance_animation

    live = appearance_animation("all_b_emt_jump_add.ans")
    if live:
        return live
    pytest.skip(
        "No golden CKAT animation: copy all_b_emt_jump_add.ans to tests/golden/"
    )


def _resolve_golden_slod() -> Path:
    if GOLDEN_SLOD.is_file():
        return GOLDEN_SLOD
    from swg_pipeline.swg_main_paths import appearance_skeleton

    live = appearance_skeleton("all_b.skt")
    if live:
        return live
    pytest.skip("No golden SLOD skeleton: copy all_b.skt to tests/golden/")


def test_golden_mesh_valid_iff():
    path = _resolve_golden_msh()
    data = path.read_bytes()
    assert validate_iff(data)
    assert root_form_type(data) == TAG_MESH


def test_golden_mesh_has_sps_and_vtxa():
    path = _resolve_golden_msh()
    data = path.read_bytes()
    text = dump_tree(data)
    assert "MESH" in text
    assert "SPS" in text
    assert "VTXA" in text
    assert "INDX" in text


def test_golden_mesh_reader_finds_sps():
    path = _resolve_golden_msh()
    reader = IffReader(path.read_bytes())
    reader.enter_form(TAG_MESH)
    reader.enter_form(tag_from_str("0005"))
    found = False
    for block in reader.iter_blocks():
        if block.is_form and (block.form_type_str or "").startswith("SPS"):
            reader.enter_form(tag_from_str("SPS "))
            found = True
            break
        reader.skip_block()
    assert found
    reader.exit_form()
    reader.exit_form()
    reader.exit_form()


def test_golden_slod_is_valid_iff():
    path = _resolve_golden_slod()
    data = path.read_bytes()
    assert validate_iff(data)
    assert root_form_type(data) == TAG_SLOD


def test_golden_animation_is_kfat():
    path = _resolve_golden_ans()
    data = path.read_bytes()
    assert validate_iff(data)
    assert root_form_type(data) == TAG_KFAT


def test_golden_ckat_is_valid_iff():
    path = _resolve_golden_ckat()
    data = path.read_bytes()
    assert validate_iff(data)
    assert root_form_type(data) == TAG_CKAT