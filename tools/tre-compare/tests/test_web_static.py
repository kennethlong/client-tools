"""SPA static-mount + mount-order regression tests (Phase-30 Plan 01 / D-02).

The ONLY backend test for the whole frontend phase. Covers ``web_static.SpaStaticFiles``
mounted LAST in ``create_app`` so the four API routes win over the greedy ``/`` mount
(T-30-01-03), plus the 404->index.html SPA fallback and the build-absent degrade path.

``create_app`` takes an optional ``web_dir`` so a test injects a synthetic
``tmp_path/web/build`` with a forged ``index.html`` — no real ``npm run build`` needed.
"""

from __future__ import annotations

from pathlib import Path

import pytest
from fastapi.testclient import TestClient

import _fixtures
from tre_compare.api import create_app

_SPA_HTML = "<!doctype html><html><head><title>TRE Compare</title></head><body><div id=root></div></body></html>"


@pytest.fixture
def web_build(tmp_path: Path) -> Path:
    """A synthetic built-SPA dir (tmp_path/web/build/index.html)."""
    build = tmp_path / "web" / "build"
    build.mkdir(parents=True)
    (build / "index.html").write_text(_SPA_HTML, encoding="utf-8")
    # a hashed asset, to prove real files still serve (not the fallback)
    (build / "assets").mkdir()
    (build / "assets" / "index-abc123.js").write_text("console.log('spa');", encoding="utf-8")
    return build


@pytest.fixture
def pair(tmp_path: Path) -> dict:
    """The synthetic install pair (for the mount-order JSON-route assertion)."""
    return _fixtures.build_install_pair(tmp_path)


@pytest.fixture
def spa_client(tmp_cache, tmp_installs, web_build):
    """A TestClient over create_app WITH the synthetic SPA build mounted."""
    app = create_app(cache=tmp_cache, installs_path=tmp_installs, web_dir=web_build)
    with TestClient(app) as client:
        yield client


# ---------------------------------------------------------------------------------------
# Test 1: GET / serves the SPA index.html
# ---------------------------------------------------------------------------------------
def test_root_serves_spa_index(spa_client):
    resp = spa_client.get("/")
    assert resp.status_code == 200, resp.text
    assert "text/html" in resp.headers["content-type"]
    assert "<title>TRE Compare</title>" in resp.text


# ---------------------------------------------------------------------------------------
# Test 2: mount-order regression — the API route still returns JSON, NOT the SPA HTML
# ---------------------------------------------------------------------------------------
def test_api_route_not_shadowed_by_spa_mount(spa_client, pair):
    resp = spa_client.post(
        "/compare/files",
        json={"left_cfg": str(pair["left_cfg"]), "right_cfg": str(pair["right_cfg"])},
    )
    assert resp.status_code == 200, resp.text
    assert "application/json" in resp.headers["content-type"]
    body = resp.json()
    assert set(body) >= {"rows", "summary"}  # the real API shape, not SPA HTML


def test_installs_route_not_shadowed(spa_client):
    resp = spa_client.get("/installs")
    assert resp.status_code == 200, resp.text
    assert "application/json" in resp.headers["content-type"]
    assert isinstance(resp.json(), list)


# ---------------------------------------------------------------------------------------
# Test 3: unknown non-asset path falls back to index.html (SpaStaticFiles 404->index)
# ---------------------------------------------------------------------------------------
def test_unknown_path_falls_back_to_index(spa_client):
    resp = spa_client.get("/some/deep/client/route")
    assert resp.status_code == 200, resp.text
    assert "text/html" in resp.headers["content-type"]
    assert "<title>TRE Compare</title>" in resp.text


def test_real_asset_is_served_not_fallback(spa_client):
    resp = spa_client.get("/assets/index-abc123.js")
    assert resp.status_code == 200, resp.text
    assert "console.log('spa')" in resp.text
    assert "<title>" not in resp.text  # the real JS, not the index fallback


# ---------------------------------------------------------------------------------------
# Test 4: no web/build/ -> create_app still builds; the 4 API routes still work
# ---------------------------------------------------------------------------------------
def test_no_build_dir_api_still_works(tmp_cache, tmp_installs, tmp_path, pair):
    missing = tmp_path / "nonexistent" / "build"
    assert not missing.is_dir()
    app = create_app(cache=tmp_cache, installs_path=tmp_installs, web_dir=missing)
    with TestClient(app) as client:
        # API routes work
        resp = client.post(
            "/compare/files",
            json={"left_cfg": str(pair["left_cfg"]), "right_cfg": str(pair["right_cfg"])},
        )
        assert resp.status_code == 200, resp.text
        assert "application/json" in resp.headers["content-type"]
        # and with no SPA mounted, GET / is a plain 404 (not the SPA, no crash)
        root = client.get("/")
        assert root.status_code == 404
