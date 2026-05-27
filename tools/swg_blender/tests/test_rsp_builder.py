"""Tests for swg_pipeline.rsp_builder."""

from __future__ import annotations

from pathlib import Path

from swg_pipeline.rsp_builder import (
    build_rsp_maps,
    classify_path,
    format_rsp_line,
    write_rsp_maps,
)


def test_classify_path():
    assert classify_path("appearance/mesh/foo.msh") == "mesh_static"
    assert classify_path("appearance/animation/bar.ans") == "animation"
    assert classify_path("texture/baz.dds") == "texture"
    assert classify_path("misc/readme.txt") == "other"


def test_build_rsp_maps_and_write(tmp_path: Path):
    root = tmp_path / "serverdata"
    (root / "appearance/mesh").mkdir(parents=True)
    (root / "shader").mkdir()
    mesh = root / "appearance/mesh/test.msh"
    mesh.write_bytes(b"\x00")
    sht = root / "shader/test.sht"
    sht.write_bytes(b"\x00")

    maps = build_rsp_maps([root])
    assert "appearance/mesh/test.msh" in maps["mesh_static"]
    assert maps["mesh_static"]["appearance/mesh/test.msh"] == str(mesh.resolve())
    assert "shader/test.sht" in maps["other"]

    out = tmp_path / "rsp"
    written = write_rsp_maps(maps, out)
    assert (out / "data_compressed_mesh_static.rsp").is_file()
    assert len(written) >= 1
    line = (out / "data_compressed_mesh_static.rsp").read_text(encoding="utf-8").splitlines()[0]
    assert line == format_rsp_line("appearance/mesh/test.msh", mesh).strip()


def test_first_root_wins(tmp_path: Path):
    a = tmp_path / "a" / "appearance/mesh"
    b = tmp_path / "b" / "appearance/mesh"
    a.mkdir(parents=True)
    b.mkdir(parents=True)
    fa = tmp_path / "a" / "appearance/mesh/x.msh"
    fb = tmp_path / "b" / "appearance/mesh/x.msh"
    fa.write_bytes(b"a")
    fb.write_bytes(b"b")
    maps = build_rsp_maps([tmp_path / "a", tmp_path / "b"])
    assert maps["mesh_static"]["appearance/mesh/x.msh"] == str(fa.resolve())