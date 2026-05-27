"""Phase 14 TRE pack pipeline tests."""

from __future__ import annotations

import json
import tempfile
from pathlib import Path

import pytest

from swg_pipeline.export_manifest import (
    PIPELINE_VERSION,
    build_export_manifest,
    write_export_manifest,
)
from swg_pipeline.pack_pipeline import PackError, pack_bundle
from swg_pipeline.rsp_builder import build_rsp_from_roots, write_rsp_file
from swg_pipeline.tre_decrypt import try_read_tre_payload
from swg_pipeline.tre_reader import list_tre, read_tre_header, toc_entry_stride
from swg_pipeline.tre_tools import find_tool


def test_extended_tre_versions_stride():
    assert toc_entry_stride("0006") == 32
    assert toc_entry_stride("5000") == 32


def test_export_manifest_version_stamp(tmp_path: Path):
    manifest = build_export_manifest(
        bundle_type="static",
        output_dir=tmp_path,
        assets={"mesh": "appearance/mesh/x.msh"},
        author="tester",
    )
    assert manifest["pipeline_version"] == PIPELINE_VERSION
    assert manifest["author"] == "tester"
    out = write_export_manifest(tmp_path / "swg_export_manifest.json", manifest)
    loaded = json.loads(out.read_text(encoding="utf-8"))
    assert loaded["bundle_type"] == "static"


def test_tre_decrypt_zlib_roundtrip():
    import zlib

    raw = zlib.compress(b"payload")
    result = try_read_tre_payload(raw, 1)
    assert result.ok and result.data == b"payload"


def test_pack_bundle_requires_builder(tmp_path: Path):
    root = tmp_path / "bundle"
    (root / "shader").mkdir(parents=True)
    (root / "shader" / "a.sht").write_bytes(b"\0")
    build_rsp_from_roots([root], root / "rsp")
    if find_tool("TreeFileBuilder") is None:
        with pytest.raises(PackError, match="TreeFileBuilder"):
            pack_bundle(root)
        pytest.skip("TreeFileBuilder not built")
    result = pack_bundle(root)
    assert result.tre_paths
    header = read_tre_header(result.tre_paths[0])
    assert header.version in ("0004", "0005")
    paths = list_tre(result.tre_paths[0])
    assert paths


def test_rsp_pack_roundtrip_golden_layout(tmp_path: Path):
    if find_tool("TreeFileBuilder") is None:
        pytest.skip("TreeFileBuilder not built")
    root = tmp_path / "mini"
    (root / "texture").mkdir(parents=True)
    (root / "shader").mkdir()
    (root / "texture" / "a.dds").write_bytes(b"DDS1")
    (root / "shader" / "b.sht").write_bytes(b"\0")
    rsp_dir = root / "rsp"
    write_rsp_file(
        rsp_dir / "dev.rsp",
        {
            "texture/a.dds": str(root / "texture" / "a.dds"),
            "shader/b.sht": str(root / "shader" / "b.sht"),
        },
    )
    tre_out = root / "dev.tre"
    from swg_pipeline.pack_pipeline import pack_rsp_file

    pack_rsp_file(rsp_dir / "dev.rsp", tre_out)
    assert list_tre(tre_out) == ["shader/b.sht", "texture/a.dds"]
