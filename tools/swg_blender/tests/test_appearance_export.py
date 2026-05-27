"""Phase 8.1 appearance export tests."""

from __future__ import annotations

from pathlib import Path

from swg_iff.reader import validate_iff
from swg_scene.appearance_export import bounds_from_meshes, write_appearance_0003
from swg_scene.appearance_import import parse_hardpoints_from_appearance
from swg_scene.mesh_static import load_static_mesh
from swg_scene.mesh_static_export import write_static_mesh
from swg_scene.model import SwgHardpoint, SwgScene
from tests.test_golden import _resolve_golden_msh


def test_golden_bed_has_foot_hardpoint():
    scene = load_static_mesh(_resolve_golden_msh())
    assert scene.appearance_raw is not None
    hardpoints = parse_hardpoints_from_appearance(scene.appearance_raw)
    names = {hp.name for hp in hardpoints}
    assert "foot" in names


def test_rebuild_appearance_preserves_hardpoints(tmp_path: Path):
    original = load_static_mesh(_resolve_golden_msh())
    assert original.appearance_raw is not None
    expected = parse_hardpoints_from_appearance(original.appearance_raw)

    rebuilt = SwgScene(
        meshes=original.meshes,
        hardpoints=expected,
        rebuild_appearance=True,
    )
    data = write_static_mesh(rebuilt)
    assert validate_iff(data)

    appr_start = data.find(b"APPR")
    assert appr_start >= 0
    # APPR chunk is inside MESH; parse from appearance bytes only.
    from swg_scene.mesh_static import load_static_mesh as load

    out = tmp_path / "rebuilt.msh"
    out.write_bytes(data)
    loaded = load(out)
    assert loaded.appearance_raw is not None
    got = parse_hardpoints_from_appearance(loaded.appearance_raw)
    assert {hp.name for hp in got} == {hp.name for hp in expected}


def test_write_appearance_0003_includes_extent():
    scene = load_static_mesh(_resolve_golden_msh())
    bounds = bounds_from_meshes(scene.meshes)
    assert bounds is not None
    appr = write_appearance_0003(
        meshes=scene.meshes,
        hardpoints=[SwgHardpoint(name="test", matrix=(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0))],
        bounds=bounds,
    )
    assert b"EXBX" in appr
    assert b"HPTS" in appr
    hardpoints = parse_hardpoints_from_appearance(appr)
    assert hardpoints[0].name == "test"
