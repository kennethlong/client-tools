"""SMAT (skeletal appearance) load + export tests."""

from __future__ import annotations

from pathlib import Path

import pytest

from swg_iff.reader import validate_iff
from swg_iff.tags import tag_from_str
from swg_pipeline.swg_main_paths import serverdata_file
from swg_scene.skeletal_appearance import (
    SwgSkeletonBinding,
    load_skeletal_appearance,
    make_skeletal_appearance_for_bundle,
    write_skeletal_appearance,
    write_skeletal_appearance_file,
)


def _resolve_abyssin_sat() -> Path:
    golden = Path(__file__).parent / "golden" / "abyssin_m.sat"
    if golden.is_file():
        return golden
    p = serverdata_file("appearance/abyssin_m.sat")
    if p is not None:
        return p
    pytest.skip("No abyssin_m.sat in tests/golden/ or swg-main serverdata")


def test_abyssin_sat_valid_iff():
    path = _resolve_abyssin_sat()
    assert validate_iff(path.read_bytes())


def test_load_abyssin_sat():
    path = _resolve_abyssin_sat()
    sat = load_skeletal_appearance(path)
    assert sat.version == "0003"
    assert len(sat.mesh_generator_paths) == 3
    assert sat.mesh_generator_paths[0].endswith("abyssin_m.lmg")
    assert len(sat.skeleton_bindings) == 2
    assert sat.skeleton_bindings[0].skeleton_relpath.endswith("all_b.skt")
    assert sat.skeleton_bindings[1].attachment_transform == "head"
    assert sat.create_animation_controller is True
    assert len(sat.skeleton_lat_pairs) == 2


def test_abyssin_sat_byte_roundtrip(tmp_path: Path):
    path = _resolve_abyssin_sat()
    original = path.read_bytes()
    sat = load_skeletal_appearance(path)
    rebuilt = write_skeletal_appearance(sat)
    assert rebuilt == original


def test_write_bundle_sat(tmp_path: Path):
    sat = make_skeletal_appearance_for_bundle(
        mesh_relpaths=["appearance/mesh/test_l0.mgn"],
        skeleton_bindings=[
            SwgSkeletonBinding("appearance/skeleton/all_b.skt", "")
        ],
        create_animation_controller=True,
        skeleton_lat_pairs=[
            ("appearance/skeleton/all_b.skt", "appearance/lat/all_m.lat")
        ],
    )
    data = write_skeletal_appearance(sat)
    assert validate_iff(data)
    out = write_skeletal_appearance_file(tmp_path / "test.sat", sat)
    loaded = load_skeletal_appearance(out)
    assert loaded.mesh_generator_paths == sat.mesh_generator_paths
    assert loaded.skeleton_lat_pairs == sat.skeleton_lat_pairs


def test_smat_root_tag():
    sat = make_skeletal_appearance_for_bundle(
        mesh_relpaths=["appearance/mesh/x.mgn"],
        skeleton_bindings=[SwgSkeletonBinding("appearance/skeleton/x.skt")],
    )
    data = write_skeletal_appearance(sat)
    assert data[8:12] == tag_from_str("SMAT").to_bytes(4, "big")
