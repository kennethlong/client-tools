"""Phase 8.7 DDS conversion tests."""

from __future__ import annotations

import struct
from pathlib import Path

import pytest

from swg_pipeline.dds import (
    DDS_MAGIC,
    DdsFormat,
    png_to_dds_bytes,
    read_dds_info,
    write_dds_a8r8g8b8,
)

pytest.importorskip("PIL")


def _make_png(path: Path, size: tuple[int, int] = (4, 4)) -> None:
    from PIL import Image

    img = Image.new("RGBA", size, (10, 20, 30, 255))
    img.save(path)


def test_write_dds_a8r8g8b8_header(tmp_path: Path):
    rgba = bytes([255, 0, 0, 128] * 16)
    data = write_dds_a8r8g8b8(rgba, 4, 4)
    assert struct.unpack_from("<I", data, 0)[0] == DDS_MAGIC
    out = tmp_path / "hdr.dds"
    out.write_bytes(data)
    info = read_dds_info(out)
    assert info.width == 4
    assert info.height == 4
    assert info.is_compressed is False


def test_png_to_dds_bytes_uncompressed_fallback(tmp_path: Path):
    png = tmp_path / "test.png"
    _make_png(png)
    dds = png_to_dds_bytes(png, fmt=DdsFormat.DXT5, allow_texconv=False)
    out = tmp_path / "out.dds"
    out.write_bytes(dds)
    info = read_dds_info(out)
    assert info.width == 4
    assert info.height == 4
    assert info.is_compressed is False


def test_png_to_dds_file_roundtrip(tmp_path: Path):
    from swg_pipeline.dds import png_to_dds_file

    png = tmp_path / "tile.png"
    dds = tmp_path / "tile.dds"
    _make_png(png, (8, 8))
    info = png_to_dds_file(png, dds, fmt=DdsFormat.A8R8G8B8, allow_texconv=False)
    assert dds.is_file()
    assert info.width == 8
    assert info.height == 8