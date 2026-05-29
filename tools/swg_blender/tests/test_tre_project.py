"""Tests for TRE creature project materialize (M16.2)."""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from swg_pipeline.creature_graph import creature_parts
from swg_pipeline.swg_main_paths import serverdata_file
from swg_pipeline.tre_project import (
    PROJECT_MANIFEST,
    import_creature_project,
    load_project_manifest,
)


def _resolve_abyssin_sat() -> str:
    p = serverdata_file("appearance/abyssin_m.sat")
    if p is not None:
        return "appearance/abyssin_m.sat"
    pytest.skip("No appearance/abyssin_m.sat (set SWG_MAIN)")


def test_import_creature_workspace_from_serverdata(tmp_path: Path):
    sat_rel = _resolve_abyssin_sat()
    result = import_creature_project(tmp_path / "abyssin_ws", sat_rel)
    assert result.manifest_path.is_file()
    assert result.ok, f"missing: {result.missing}"
    manifest = load_project_manifest(result.workspace)
    assert manifest["project_type"] == "creature"
    assert manifest["root_sat"].endswith("abyssin_m.sat")
    assert len(manifest["parts"]) >= 1
    for part in manifest["parts"]:
        mgn = part.get("lod0_mgn_relpath")
        assert mgn
        assert (result.workspace / str(mgn)).is_file()


def test_creature_parts_have_lod0(tmp_path: Path):
    sat_rel = _resolve_abyssin_sat()
    result = import_creature_project(tmp_path / "parts_ws", sat_rel)
    parts = creature_parts(result.graph, result.workspace)
    assert parts
    assert all(p.get("lod0_mgn_relpath") for p in parts)


def test_manifest_json_roundtrip(tmp_path: Path):
    sat_rel = _resolve_abyssin_sat()
    ws = tmp_path / "json_ws"
    import_creature_project(ws, sat_rel)
    data = json.loads((ws / PROJECT_MANIFEST).read_text(encoding="utf-8"))
    assert "graph" in data
    assert "mesh_paths" in data["graph"]

def test_export_creature_rebuild_templates(tmp_path: Path):
    from swg_iff.reader import validate_iff
    from swg_pipeline.tre_project import export_creature_project
    sat_rel = _resolve_abyssin_sat()
    ws = tmp_path / "export_ws"
    import_creature_project(ws, sat_rel)
    result = export_creature_project(ws, use_blender=False)
    assert result.ok
    manifest = json.loads((ws / PROJECT_MANIFEST).read_text(encoding="utf-8"))
    assert "exported_at" in manifest
    assert validate_iff((ws / sat_rel).read_bytes())
