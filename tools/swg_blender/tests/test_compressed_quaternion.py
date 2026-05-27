"""Tests for compressed quaternion pack/unpack."""

from __future__ import annotations

import pytest

from swg_scene.compressed_quaternion import (
    compress_quaternion,
    expand_quaternion,
    get_optimal_compression_format,
)


def test_compress_expand_roundtrip():
    x, y, z, w = 0.01, -0.02, 0.03, 0.999
    x_format = get_optimal_compression_format(x - 0.01, x + 0.01)
    y_format = get_optimal_compression_format(y - 0.01, y + 0.01)
    z_format = get_optimal_compression_format(z - 0.01, z + 0.01)
    packed = compress_quaternion(x, y, z, w, x_format, y_format, z_format)
    expanded = expand_quaternion(packed, x_format, y_format, z_format)
    assert expanded[0] == pytest.approx(x, abs=0.01)
    assert expanded[1] == pytest.approx(y, abs=0.01)
    assert expanded[2] == pytest.approx(z, abs=0.01)
    assert expanded[3] == pytest.approx(w, abs=0.15)