"""Tests for building_graph POB dependency closure."""

from __future__ import annotations

from pathlib import Path

import pytest

from swg_pipeline.building_graph import building_cells, resolve_building_graph
from swg_pipeline.swg_main_paths import appearance_pob


def _require_pob() -> Path:
    p = appearance_pob("echo_base_pob.pob")
    if p is None:
        pytest.skip("no echo_base_pob.pob on swg-main")
    return p


def test_echo_base_building_graph():
    pob = _require_pob()
    graph = resolve_building_graph(pob)
    assert graph.root_pob.endswith("echo_base_pob.pob")
    assert graph.cell_count == 41
    assert graph.portal_count > 0
    assert len(graph.mesh_paths) >= 30


def test_building_cells_primary_mesh():
    pob = _require_pob()
    graph = resolve_building_graph(pob)
    cells = building_cells(graph, pob.parent.parent)
    assert len(cells) == graph.cell_count
    with_mesh = [c for c in cells if c.get("primary_mesh_relpath")]
    assert len(with_mesh) >= 35


def test_echo_base_cells_have_floor_paths():
    pob = _require_pob()
    graph = resolve_building_graph(pob)
    cells = building_cells(graph, pob.parent.parent)
    floors = [c for c in cells if c.get("floor_relpath")]
    assert len(floors) == graph.cell_count
    assert all(str(c["floor_relpath"]).endswith(".flr") for c in floors)


def test_building_import_materializes_floors(tmp_path: Path):
    from swg_pipeline.tre_project_building import import_building_project

    pob = _require_pob()
    result = import_building_project(tmp_path, "appearance/echo_base_pob.pob")
    assert not result.missing
    for rel in result.graph.floor_paths[:3]:
        path = tmp_path / rel.replace("\\", "/")
        assert path.is_file(), rel


def test_tauntaun_grounds_has_duplicate_triangles():
    """Retail mesh[1] repeats triangles; import must not dedupe them (bmesh faces.new did)."""
    from swg_pipeline.swg_main_paths import serverdata_file
    from swg_scene.mesh_static import load_static_mesh

    path = serverdata_file("appearance/mesh/echo_base_pob_r26_tauntaun_grounds_r26.msh")
    if path is None:
        pytest.skip("no tauntaun_grounds msh on swg-main")
    mesh1 = load_static_mesh(path).meshes[1]
    tris = len(mesh1.indices) // 3
    unique = len({tuple(sorted(mesh1.indices[i : i + 3])) for i in range(0, len(mesh1.indices), 3)})
    assert tris == 41 and unique == 25


def test_cell_encode_matches_retail_fields():
    from swg_scene.pob_cells import load_portal_cells, write_cell_bytes

    pob = _require_pob()
    original = load_portal_cells(pob)[0]
    rebuilt = write_cell_bytes(original)
    from swg_scene.pob_cells import _decode_cell_raw

    again = _decode_cell_raw(rebuilt)
    assert again.name == original.name
    assert again.appearance_relpath == original.appearance_relpath
    assert again.floor_relpath == original.floor_relpath
    assert again.portal_count == original.portal_count


def test_cell_encode_byte_parity():
    from swg_scene.pob_cells import load_portal_cells, write_cell_bytes

    pob = _require_pob()
    matched = sum(
        1 for c in load_portal_cells(pob) if write_cell_bytes(c) == c.cell_raw
    )
    assert matched == 41


def test_rewrite_pob_byte_parity_when_unchanged(tmp_path: Path):
    from swg_pipeline.tre_project_building import import_building_project, rewrite_building_pob

    pob = _require_pob()
    data = pob.read_bytes()
    import_building_project(tmp_path, "appearance/echo_base_pob.pob")
    rewrite_building_pob(tmp_path)
    out = tmp_path / "appearance/echo_base_pob.pob"
    assert out.read_bytes() == data


def test_portal_idtl_byte_roundtrip():
    from swg_scene.indexed_triangle_list import load_idtl_bytes, write_idtl_bytes
    from swg_scene.portal_property import load_portal_property
    from swg_pipeline.swg_main_paths import appearance_pob

    pob = appearance_pob("echo_base_pob.pob")
    if pob is None:
        pytest.skip("no echo_base_pob.pob")
    spec = load_portal_property(pob)
    matched = 0
    for raw in spec.portal_geometry_raw:
        verts, indices = load_idtl_bytes(raw)
        if write_idtl_bytes(verts, indices) == raw:
            matched += 1
    assert matched == spec.portal_count


def test_rewrite_pob_preserves_portal_geometry(tmp_path: Path):
    from swg_pipeline.tre_project_building import import_building_project, rewrite_building_pob
    from swg_scene.portal_property import load_portal_property

    pob = _require_pob()
    import_building_project(tmp_path, "appearance/echo_base_pob.pob")
    before = load_portal_property(tmp_path / "appearance/echo_base_pob.pob")
    prts = list(before.portal_geometry_raw)
    rewrite_building_pob(tmp_path)
    after = load_portal_property(tmp_path / "appearance/echo_base_pob.pob")
    assert after.portal_geometry_raw == prts
    assert after.cell_count == before.cell_count


def test_primary_msh_python_roundtrip(tmp_path: Path):
    from swg_pipeline.parity import compare_scenes, roundtrip_scene
    from swg_scene.mesh_static import load_static_mesh

    pob = _require_pob()
    graph = resolve_building_graph(pob)
    cells = building_cells(graph, pob.parent.parent)
    rel = next(c["primary_mesh_relpath"] for c in cells if c.get("primary_mesh_relpath"))
    path = pob.parent.parent / str(rel).replace("\\", "/")
    if not path.is_file():
        pytest.skip(f"missing {rel}")
    original = load_static_mesh(path)
    rt = roundtrip_scene(path, rebuild_appearance=False, optimize_indices=False)
    diff = compare_scenes(original, rt)
    assert diff.ok, diff.errors
