"""Phase 15 add-on support tests (no bpy required)."""

from __future__ import annotations

import json
from pathlib import Path

from swg_pipeline.export_manifest import apply_manifest_author_notes


def test_apply_manifest_author_notes(tmp_path: Path):
    manifest = tmp_path / "swg_export_manifest.json"
    manifest.write_text(
        json.dumps({"pipeline_version": "3.009", "assets": {"mesh": "appearance/mesh/x.msh"}}),
        encoding="utf-8",
    )
    apply_manifest_author_notes(manifest, author="artist", notes="test export")
    data = json.loads(manifest.read_text(encoding="utf-8"))
    assert data["author"] == "artist"
    assert data["notes"] == "test export"


def test_addon_files_present():
    root = Path(__file__).resolve().parents[1] / "swg_blender_addon"
    assert (root / "properties.py").is_file()
    assert (root / "panel.py").is_file()
    text = (root / "properties.py").read_text(encoding="utf-8")
    assert "CREATURE_BUNDLE" in text
    assert "ignore_blend_targets" in text
