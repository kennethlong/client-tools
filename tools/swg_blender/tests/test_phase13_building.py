"""Phase 13 POB/APT building tests."""

from __future__ import annotations

import tempfile
from pathlib import Path

import pytest

from swg_iff.reader import validate_iff
from swg_pipeline.swg_main_paths import appearance_apt, appearance_pob
from swg_scene.appearance_redirect import (
    load_appearance_redirect,
    write_appearance_redirect_bytes,
)
from swg_scene.building_export import export_apt, export_pob
from swg_scene.hierarchy import decode_export_name
from swg_scene.portal_property import (
    extract_portal_layout_crc,
    load_portal_property,
    load_portal_property_bytes,
    write_portal_property_bytes,
)


def _require_pob() -> Path:
    p = appearance_pob("echo_base_pob.pob")
    if p is None:
        p = appearance_pob("blacksun_transport_player.pob")
    if p is None:
        pytest.skip("no POB fixture on swg-main")
    return p


def _require_apt() -> Path:
    p = appearance_apt("abyssin_m_arms.apt")
    if p is None:
        pytest.skip("no APT fixture on swg-main")
    return p


def test_pob_roundtrip_counts():
    original = load_portal_property(_require_pob())
    assert original.portal_count > 0
    assert original.cell_count > 0
    assert len(original.portal_geometry_raw) == original.portal_count
    assert len(original.cells_raw) == original.cell_count
    out = Path(tempfile.gettempdir()) / "pob_roundtrip.pob"
    out.write_bytes(write_portal_property_bytes(original))
    assert validate_iff(out.read_bytes())
    rebuilt = load_portal_property(out)
    assert rebuilt.version == original.version
    assert rebuilt.portal_count == original.portal_count
    assert rebuilt.cell_count == original.cell_count


def test_pob_byte_roundtrip():
    path = _require_pob()
    data = path.read_bytes()
    spec = load_portal_property_bytes(data)
    out = write_portal_property_bytes(spec)
    assert out == data


def test_pob_crc_extract():
    path = _require_pob()
    crc = extract_portal_layout_crc(path.read_bytes())
    assert crc is not None


def test_apt_roundtrip():
    original = load_appearance_redirect(_require_apt())
    assert original.target_path
    out = Path(tempfile.gettempdir()) / "apt_roundtrip.apt"
    out.write_bytes(write_appearance_redirect_bytes(original))
    assert validate_iff(out.read_bytes())
    rebuilt = load_appearance_redirect(out)
    assert rebuilt.target_path == original.target_path


def test_hierarchy_building_nodes():
    assert decode_export_name("pob") is not None
    assert decode_export_name("portal0") is not None
    assert decode_export_name("cells") is not None


def test_building_export_helpers(tmp_path):
    path = _require_pob()
    spec = load_portal_property(path)
    out_pob = export_pob(tmp_path / "test.pob", spec)
    assert out_pob.is_file()
    apt_out = export_apt(tmp_path / "cell.apt", "appearance/mesh/foo.msh")
    assert apt_out.is_file()
    assert load_appearance_redirect(apt_out).target_path.endswith("foo.msh")
