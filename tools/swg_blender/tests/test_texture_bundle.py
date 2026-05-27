"""Phase 10.7 texture bundle copy tests."""

from __future__ import annotations

from pathlib import Path

from swg_pipeline.texture_bundle import copy_textures_from_paths


def test_copy_textures_from_serverdata_paths(tmp_path: Path, monkeypatch):
    source = tmp_path / "source.dds"
    source.write_bytes(b"DDS placeholder")

    def fake_serverdata_file(relpath: str) -> Path | None:
        if relpath.replace("\\", "/") == "texture/test_main.dds":
            return source
        return None

    monkeypatch.setattr(
        "swg_pipeline.swg_main_paths.serverdata_file",
        fake_serverdata_file,
    )

    copied = copy_textures_from_paths(
        ["texture/test_main.dds", "texture/test_main.dds", "texture/missing.dds"],
        tmp_path / "bundle",
    )
    assert len(copied) == 1
    dest = tmp_path / "bundle" / "texture" / "test_main.dds"
    assert dest.is_file()
    assert dest.read_bytes() == source.read_bytes()
