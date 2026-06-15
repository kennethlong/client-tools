"""TestClient route tests for the tre-compare FastAPI surface (Phase-29 Plan 03).

Drives ``create_app(cache=tmp_cache, installs_path=...)`` over the synthetic
``build_install_pair`` install pair via ``fastapi.testclient.TestClient`` (httpx dev dep
from Plan 01). Covers the four routes' shapes, the lean-row contract, the optional
qualifier, the 400/404 error envelope, corrupt-archive fault-isolation (never 500), the
payload-keyed drill-in hash memo (a mutated payload .tre with an unchanged .toc returns a
FRESH hash), /installs, and cache injection isolation (no module-level Cache leak).
"""

from __future__ import annotations

import struct
from pathlib import Path

import pytest
from fastapi.testclient import TestClient

import _fixtures
from tre_compare.api import create_app
from tre_compare.cache import Cache
from tre_compare.parser.tre_reader import STANDARD_HEADER_FMT

_VALID_SET_STATUSES = {"added", "removed", "changed", "identical", "fault"}
_VALID_FILE_STATUSES = {"added", "removed", "changed", "identical-by-metadata", "tombstoned"}


@pytest.fixture
def pair(tmp_path: Path) -> dict:
    """Build the synthetic install pair for the API tests."""
    return _fixtures.build_install_pair(tmp_path)


def _body(client: TestClient, route: str, info: dict, **extra) -> dict:
    payload = {"left_cfg": str(info["left_cfg"]), "right_cfg": str(info["right_cfg"]), **extra}
    resp = client.post(route, json=payload)
    return resp


# ---------------------------------------------------------------------------------------
# /compare/set
# ---------------------------------------------------------------------------------------
def test_compare_set(api_client, pair):
    resp = _body(api_client, "/compare/set", pair)
    assert resp.status_code == 200, resp.text
    rows = resp.json()["rows"]
    assert isinstance(rows, list) and rows
    for row in rows:
        assert row["status"] in _VALID_SET_STATUSES
    statuses = {r["status"] for r in rows}
    # the pair forges removed/added/changed at the set level + a collide pair (two rows).
    assert {"added", "removed", "changed"} <= statuses


def test_compare_set_corrupt_never_500(api_client, pair):
    # broken.tre is a truncated archive on the LEFT -> a fault row, NOT a 500.
    resp = _body(api_client, "/compare/set", pair)
    assert resp.status_code == 200, resp.text
    rows = resp.json()["rows"]
    fault_rows = [r for r in rows if r["status"] == "fault"]
    assert fault_rows, "corrupt broken.tre must degrade to a fault row, not 500"
    assert any("broken" in r["basename"] for r in fault_rows)
    # other rows still present (the diff was not aborted)
    assert any(r["status"] in ("added", "removed", "changed") for r in rows)


# ---------------------------------------------------------------------------------------
# /compare/files
# ---------------------------------------------------------------------------------------
def test_compare_files_shape(api_client, pair):
    resp = _body(api_client, "/compare/files", pair)
    assert resp.status_code == 200, resp.text
    body = resp.json()
    assert set(body) >= {"rows", "summary"}
    rows = body["rows"]
    assert rows
    for row in rows:
        assert set(row) >= {"path", "status", "left", "right"}
        assert row["status"] in _VALID_FILE_STATUSES
        for side in ("left", "right"):
            v = row[side]
            assert v is None or set(v) == {"len", "clen"}
        # NO hashes in the bulk rows (drill-in only)
        assert "hexdigest" not in row and "hash" not in row


def test_compare_files_qualifier(api_client, pair):
    resp = _body(api_client, "/compare/files", pair)
    rows = resp.json()["rows"]
    by_path = {r["path"]: r for r in rows}
    # foo/x.dds is tombstoned on the LEFT, real on the right -> tombstoned_left qualifier.
    tomb = by_path[pair["tombstone_left"]]
    assert "qualifier" in tomb
    assert "tombstoned_left" in tomb["qualifier"]


def test_compare_files_summary_counts(api_client, pair):
    resp = _body(api_client, "/compare/files", pair)
    summary = resp.json()["summary"]
    assert "status_counts" in summary
    for side in ("left", "right"):
        assert set(summary[side]) >= {"node_errors", "rejected", "tombstoned"}


# ---------------------------------------------------------------------------------------
# /file/detail
# ---------------------------------------------------------------------------------------
def test_file_detail_false_identical(api_client, pair):
    resp = _body(api_client, "/file/detail", pair, path=pair["false_identical"])
    assert resp.status_code == 200, resp.text
    body = resp.json()
    assert body["hash_left"]["hexdigest"] != body["hash_right"]["hexdigest"]
    assert body["verdict"] == "content-changed"
    assert "shadowed_left" in body and "shadowed_right" in body


def test_file_detail_not_found(api_client, pair):
    resp = _body(api_client, "/file/detail", pair, path="no/such/file.dds")
    assert resp.status_code == 404, resp.text


def test_file_detail_rejected(api_client, pair):
    # an interior-`..` path is rejected by safe_virtual_key -> drill_in status rejected -> 400.
    resp = _body(api_client, "/file/detail", pair, path="bad/../../etc/passwd")
    assert resp.status_code == 400, resp.text


def test_file_detail_memo_payload_keyed(api_client, pair):
    """A mutated payload .tre (unchanged .toc) -> a FRESH hash (P3 payload-keyed memo)."""
    # first drill: the TOC-served payload/p.dds hashes the bytes of right/payload.tre.
    r1 = _body(api_client, "/file/detail", pair, path=pair["toc_payload"])
    assert r1.status_code == 200, r1.text
    hash1 = r1.json()["hash_right"]["hexdigest"]
    assert hash1 is not None

    # MUTATE the payload .tre bytes (different length/clen -> different size/mtime), WITHOUT
    # touching served.toc. Rewrite payload.tre with a DIFFERENT payload blob.
    payload_tre = pair["right"] / "payload.tre"
    _fixtures.build_tre(
        payload_tre,
        [("payload/p.dds", 12, 12)],
        version="0004",
        payloads={"payload/p.dds": b"MUTATED-byte"},  # 12 bytes, different bytes
    )

    r2 = _body(api_client, "/file/detail", pair, path=pair["toc_payload"])
    assert r2.status_code == 200, r2.text
    hash2 = r2.json()["hash_right"]["hexdigest"]
    assert hash2 is not None
    assert hash2 != hash1, "memo must be keyed on the payload .tre, not the .toc (P3)"


# ---------------------------------------------------------------------------------------
# error envelope
# ---------------------------------------------------------------------------------------
def test_bad_cfg_400(api_client, pair):
    resp = api_client.post(
        "/compare/files",
        json={"left_cfg": "C:/does/not/exist.cfg", "right_cfg": str(pair["right_cfg"])},
    )
    assert resp.status_code == 400, resp.text
    detail = resp.json()["detail"]
    assert "error" in detail and "cfg" in detail


# ---------------------------------------------------------------------------------------
# /installs
# ---------------------------------------------------------------------------------------
def test_installs(api_client):
    resp = api_client.get("/installs")
    assert resp.status_code == 200, resp.text
    body = resp.json()
    assert isinstance(body, list)
    assert body and body[0]["name"] == "synthetic"
    assert "cfg_path" in body[0]


# ---------------------------------------------------------------------------------------
# cache injection isolation (no module-level Cache leak)
# ---------------------------------------------------------------------------------------
def test_cache_injection_isolated(tmp_path: Path, pair):
    cache_a = Cache(tmp_path / "a" / "c.sqlite")
    cache_b = Cache(tmp_path / "b" / "c.sqlite")
    try:
        client_a = TestClient(create_app(cache=cache_a))
        client_b = TestClient(create_app(cache=cache_b))
        # drive a compare through A only
        _body(client_a, "/compare/files", pair)
        # B's sqlite must still be empty (no shared module-level Cache)
        n_a = cache_a._conn.execute("SELECT COUNT(*) FROM archive_meta").fetchone()[0]
        n_b = cache_b._conn.execute("SELECT COUNT(*) FROM archive_meta").fetchone()[0]
        assert n_a > 0
        assert n_b == 0, "two create_app caches must not share state"
    finally:
        cache_a.close()
        cache_b.close()
