"""Tests for creature_graph SAT dependency closure."""

from __future__ import annotations

from pathlib import Path

import pytest

from swg_pipeline.creature_graph import (
    creature_parts,
    normalize_tre_relpath,
    resolve_creature_graph,
)
from swg_pipeline.swg_main_paths import serverdata_file


def _resolve_abyssin_sat() -> Path:
    p = serverdata_file("appearance/abyssin_m.sat")
    if p is not None:
        return p
    pytest.skip("No appearance/abyssin_m.sat (set SWG_MAIN or add golden)")


def test_normalize_tre_relpath():
    assert normalize_tre_relpath(r"appearance\mesh\a.mgn") == "appearance/mesh/a.mgn"


def test_abyssin_creature_graph_closure():
    sat_path = _resolve_abyssin_sat()
    graph = resolve_creature_graph(sat_path)
    assert graph.root_sat.endswith("abyssin_m.sat")
    assert len(graph.mesh_generator_paths) >= 1
    assert any(p.endswith(".lmg") for p in graph.mesh_generator_paths)
    assert len(graph.mesh_paths) >= 1
    assert all(p.endswith(".mgn") for p in graph.mesh_paths)
    assert any(p.endswith("all_b.skt") for p in graph.skeleton_paths)
    assets = graph.all_asset_relpaths()
    assert graph.root_sat in assets
    assert len(assets) >= len(graph.mesh_paths) + 1


def test_abyssin_graph_collects_shaders_when_present():
    sat_path = _resolve_abyssin_sat()
    graph = resolve_creature_graph(sat_path, collect_shaders=True)
    if not graph.shader_paths:
        pytest.skip("No shader paths resolved from abyssin mgn files")
    assert all(p.startswith("shader/") or ".sht" in p for p in graph.shader_paths)


def test_creature_parts_lod_chain():
    sat_path = _resolve_abyssin_sat()
    graph = resolve_creature_graph(sat_path)
    parts = creature_parts(graph, sat_path.parent.parent)
    assert parts
    first = parts[0]
    lods = first.get("lod_mgn_relpaths") or []
    assert len(lods) >= 2
    assert first["lod0_mgn_relpath"] == lods[0]
    assert all(rel.endswith(".mgn") for rel in lods)
