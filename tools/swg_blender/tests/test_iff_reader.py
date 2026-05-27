"""Tests for swg_iff reader."""

from __future__ import annotations

import struct

import pytest

from swg_iff.dump import dump_tree
from swg_iff.reader import IffError, IffReader, root_form_type, validate_iff
from swg_iff.tags import TAG_FORM, TAG_MESH, tag_from_str, tag_to_str
from swg_iff.writer import make_chunk, make_form, make_mesh_shell


def test_tag_roundtrip():
    t = tag_from_str("MESH")
    assert tag_to_str(t) == "MESH"


def test_validate_simple_form():
    data = make_form("TEST", make_chunk("DATA", b"\x01\x02\x03\x04"))
    assert validate_iff(data)


def test_validate_rejects_truncated():
    data = make_form("TEST", b"")
    bad = data[:-1]
    assert not validate_iff(bad)


def test_reader_enter_mesh_shell():
    data = make_mesh_shell()
    assert root_form_type(data) == TAG_MESH
    reader = IffReader(data)
    reader.enter_form(TAG_MESH)
    assert reader.current_name() == tag_from_str("0005")
    reader.enter_form(tag_from_str("0005"))
    reader.enter_form(tag_from_str("SPS "))
    reader.enter_form(tag_from_str("0005"))
    reader.enter_chunk(tag_from_str("CNT "))
    assert reader.read_int32() == 0
    reader.exit_chunk()
    reader.exit_form()
    reader.exit_form()
    reader.exit_form()
    reader.exit_form()


def test_read_float_vector_chunk():
    payload = struct.pack("<fff", 1.0, 2.0, 3.0)
    data = make_form("ROOT", make_chunk("VEC ", payload))
    reader = IffReader(data)
    reader.enter_form(tag_from_str("ROOT"))
    reader.enter_chunk(tag_from_str("VEC "))
    assert reader.read_float_vector() == (1.0, 2.0, 3.0)
    reader.exit_chunk()
    reader.exit_form()


def test_dump_tree_includes_mesh():
    data = make_mesh_shell()
    text = dump_tree(data)
    assert "MESH" in text
    assert "SPS" in text


def test_chunk_overflow_raises():
    data = make_form("ROOT", make_chunk("DATA", b"\x00"))
    reader = IffReader(data)
    reader.enter_form(tag_from_str("ROOT"))
    reader.enter_chunk(tag_from_str("DATA"))
    reader.read_uint8()
    with pytest.raises(IffError):
        reader.read_uint8()
    reader.exit_chunk()
    reader.exit_form()
