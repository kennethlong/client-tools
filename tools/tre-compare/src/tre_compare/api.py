"""FastAPI HTTP surface for tre-compare (Phase-29 Plan 03 / D-01/D-02/D-07/D-08/D-09).

A ``create_app(cache, installs_path) -> FastAPI`` FACTORY (NOT a module-level ``app`` /
``Cache`` — a module-level Cache leaks state across TestClient tests, REVIEWS also-agreed)
exposing four STATELESS routes that compose the Plan-01 pure diff engine over the Plan-02
sqlite cached merge:

  * ``POST /compare/set``    -> ``{rows}`` set-level archive diff (corrupt archive -> a
                               ``fault`` row, NEVER a 500).
  * ``POST /compare/files``  -> the ENTIRE ``{rows, summary}`` lean-row payload, NO
                               pagination (D-08/D-09).
  * ``POST /file/detail``    -> winner + shadowed + per-side on-demand xxhash + an upgraded
                               verdict (incl. ``content-indeterminate``), hashing ONLY on
                               drill-in. ``drill_in`` domain statuses map
                               ``not_found`` -> 404, ``rejected`` -> 400.
  * ``GET  /installs``       -> the config-driven install-picker list (tomllib; D-02).

DRILL-IN HASH MEMO (P3 — Cursor+Sonnet+Opus): :func:`get_or_compute_hash` is an
API-LAYER helper (sqlite stays OUT of ``diff.py``) that memoizes the drill-in hash keyed on
the RESOLVED PAYLOAD ``.tre``'s identity for a TOC-served winner — NOT the ``.toc``'s. A
changed payload ``.tre`` under an UNCHANGED ``.toc`` therefore yields a FRESH key (the bytes
live in the payload ``.tre``; keying on the ``.toc`` would serve a stale hash). For a
tree/path winner it keys on ``winner_node.abspath`` directly.

Bind (``__main__``): uvicorn on ``127.0.0.1`` ONLY (localhost single-user posture,
T-29-03-03) — never ``0.0.0.0``.

``create_app`` + the diff functions are importable so Phase 30 builds the app with ZERO
backend refactor.
"""

from __future__ import annotations

import logging
import os
from dataclasses import replace
from pathlib import Path

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel

from . import diff as _diff
from .cache import Cache, build_virtual_tree_cached
from .config import load_installs
from .web_static import WEB_DIR, SpaStaticFiles

_log = logging.getLogger(__name__)
from .diff import (
    DriveHashResult,
    diff_archive_set,
    diff_virtual_trees,
    drill_in,
    hash_virtual_file,
)
from .scanner import ScanResult, SearchNode, parse_shared_file


# =======================================================================================
# Pydantic request models
# =======================================================================================
class PairRequest(BaseModel):
    """A two-install compare request: each side's ``client.cfg`` path."""

    left_cfg: str
    right_cfg: str


class FileDetailRequest(PairRequest):
    """A drill-in request: the install pair + the virtual ``path`` to inspect."""

    path: str


# =======================================================================================
# helpers
# =======================================================================================
def _validate_cfg(cfg_path: str) -> None:
    """Raise ``HTTPException(400)`` with an error envelope when *cfg_path* is unusable.

    Validates existence + readability of a request-controlled cfg path BEFORE the scanner
    opens it (T-29-03-01) so a bad path degrades to a 400, never a 500.
    """
    p = Path(cfg_path)
    if not p.is_file() or not os.access(p, os.R_OK):
        raise HTTPException(
            status_code=400,
            detail={"error": "cfg path missing or unreadable", "cfg": cfg_path},
        )


def _scan_pair(req: PairRequest) -> tuple[ScanResult, ScanResult]:
    """Validate + parse both cfgs into scans (400 on a bad cfg path)."""
    _validate_cfg(req.left_cfg)
    _validate_cfg(req.right_cfg)
    return parse_shared_file(req.left_cfg), parse_shared_file(req.right_cfg)


def get_or_compute_hash(
    cache: Cache,
    node: SearchNode,
    vpath: str,
    toc_tree_path: str | None,
) -> DriveHashResult:
    """Memoized, PAYLOAD-KEYED drill-in hash (P3) — keeps sqlite OUT of ``diff.py``.

    Resolves the file whose BYTES are hashed and keys the ``file_hash`` memo on THAT file's
    identity:

      * ``node.kind == "toc"``: the bytes live in the payload ``.tre`` at
        ``join(toc_tree_path, entry.tre_file)`` (resolved EXACTLY as
        ``diff._payload_tre_for_toc`` does — match ``fix_up_file_name(e.path) == vpath``).
        The memo is keyed on the PAYLOAD ``.tre``'s ``(abspath, mtime_ns, size)`` so a
        changed payload under an unchanged ``.toc`` MISSes (a FRESH hash, never stale).
      * ``tree`` / ``path``: key on ``node.abspath``.

    Flow: GET (on a HIT return the cached hexdigest as a synthetic :class:`DriveHashResult`)
    -> on a MISS compute via ``diff.hash_virtual_file`` -> PUT the hexdigest (skip the put
    when the read errored). If the payload ``.tre`` can't be resolved (a parse fault on the
    key-resolution side), fall back to a plain ``hash_virtual_file`` — never crash.
    """
    # Pick the node whose identity keys the memo (the file the bytes actually come from).
    memo_node = node
    source_archive = os.path.basename(node.abspath)
    if node.kind == "toc":
        try:
            payload_tre, _raw = _diff._payload_tre_for_toc(node, vpath, toc_tree_path)
            memo_node = replace(node, abspath=payload_tre)
            source_archive = os.path.basename(payload_tre)
        except _diff._HASH_FAULTS:
            # Can't resolve the payload .tre -> skip the memo, degrade through hash_virtual_file.
            return hash_virtual_file(node, vpath, toc_tree_path)

    try:
        cached = cache.get_file_hash(memo_node, vpath)
    except OSError:
        # the memo_node's payload file vanished/unstatable -> just compute (never 500)
        return hash_virtual_file(node, vpath, toc_tree_path)
    if cached is not None:
        return DriveHashResult(hexdigest=cached, error=None, source_archive=source_archive)

    result = hash_virtual_file(node, vpath, toc_tree_path)
    if result.hexdigest is not None:
        try:
            cache.put_file_hash(memo_node, vpath, result.hexdigest)
        except OSError:
            pass  # a put failure must never break the response
    return result


# =======================================================================================
# app factory
# =======================================================================================
def create_app(
    cache: Cache | None = None,
    installs_path: str | Path | None = None,
    web_dir: str | Path | None = None,
) -> FastAPI:
    """Build a stateless tre-compare FastAPI app over an INJECTED *cache* + *installs_path*.

    *cache* / *installs_path* default to a real ``Cache()`` + the tool-local
    ``installs.toml`` when None, so production runs need no args while tests inject an
    isolated ``tmp_cache`` (no module-level Cache leak). Importable for Phase 30 reuse.

    *web_dir* (Phase-30 / D-02) defaults to :data:`web_static.WEB_DIR` (``web/build``); when
    that directory exists the built Vite SPA is static-mounted at ``/`` (registered LAST so
    the four API routes win — T-30-01-03). Tests inject a synthetic ``tmp_path/web/build``.
    When the dir is absent the app runs API-only (dev mode / no ``npm run build`` yet).
    """
    if cache is None:
        cache = Cache()
    web_dir = WEB_DIR if web_dir is None else Path(web_dir)

    app = FastAPI(title="tre-compare", version="0.2.0")

    @app.post("/compare/set")
    def compare_set(req: PairRequest) -> dict:
        scanL, scanR = _scan_pair(req)
        # A corrupt archive degrades to a "fault" row inside diff_archive_set (stat_archive
        # is fault-wrapped) — never a 500 (Phase-28 fault isolation).
        return {"rows": diff_archive_set(scanL, scanR)}

    @app.post("/compare/files")
    def compare_files(req: PairRequest) -> dict:
        scanL, scanR = _scan_pair(req)
        # node_errors from a malformed archive accumulate into the cached merge -> surfaced
        # in summary, never 500'd.
        vtL = build_virtual_tree_cached(scanL, cache)
        vtR = build_virtual_tree_cached(scanR, cache)
        return diff_virtual_trees(vtL, vtR)  # FULL {rows, summary}, no pagination (D-08)

    @app.post("/file/detail")
    def file_detail(req: FileDetailRequest) -> dict:
        scanL, scanR = _scan_pair(req)
        vtL = build_virtual_tree_cached(scanL, cache)
        vtR = build_virtual_tree_cached(scanR, cache)
        result = drill_in(vtL, vtR, req.path, scanL, scanR)
        if result.status == "not_found":
            raise HTTPException(status_code=404, detail={"error": "not found", "path": req.path})
        if result.status == "rejected":
            raise HTTPException(status_code=400, detail={"error": "rejected path", "path": req.path})

        # Re-run the per-side hash through the PAYLOAD-KEYED memo (P3) — diff.drill_in already
        # computed an unmemoized hash; we override with the memoized result so a re-drill HITs
        # and a mutated payload .tre MISSes.
        key = result.path
        hash_left = (
            get_or_compute_hash(cache, result.winner_left.winner_node, key, scanL.toc_tree_path)
            if result.winner_left is not None
            else None
        )
        hash_right = (
            get_or_compute_hash(cache, result.winner_right.winner_node, key, scanR.toc_tree_path)
            if result.winner_right is not None
            else None
        )

        # Recompute the verdict from the MEMOIZED hashes (content-indeterminate when a side
        # degraded — Opus).
        verdict = result.verdict
        if result.winner_left is not None and result.winner_right is not None:
            hl = hash_left.hexdigest if hash_left else None
            hr = hash_right.hexdigest if hash_right else None
            if hl is None or hr is None:
                verdict = "content-indeterminate"
            elif hl == hr:
                verdict = "content-confirmed-identical"
            else:
                verdict = "content-changed"

        return {
            "path": key,
            "status": "ok",
            "winning_archive_left": result.winning_archive_left,
            "winning_archive_right": result.winning_archive_right,
            "left": _merged_meta(result.winner_left),
            "right": _merged_meta(result.winner_right),
            "shadowed_left": [_node_brief(n) for n in result.shadowed_left],
            "shadowed_right": [_node_brief(n) for n in result.shadowed_right],
            "hash_left": _hash_brief(hash_left),
            "hash_right": _hash_brief(hash_right),
            "verdict": verdict,
        }

    @app.get("/installs")
    def installs() -> list[dict]:
        return [{"name": e.name, "cfg_path": e.cfg_path} for e in load_installs(installs_path)]

    # Serve the built SPA LAST — the "/" mount is greedy and would shadow the four routes
    # above if registered first (T-30-01-03 / Pitfall 2). Guarded on the build existing so a
    # dev run (API-only, no `npm run build` yet) still works.
    if web_dir.is_dir():
        app.mount("/", SpaStaticFiles(directory=web_dir, html=True), name="spa")
    else:
        _log.info("no built SPA found at %s, run `npm run build` in web/", web_dir)

    return app


# =======================================================================================
# small serializers (dataclass -> json-able dict)
# =======================================================================================
def _merged_meta(entry) -> dict | None:
    if entry is None:
        return None
    return {"len": entry.length, "clen": entry.compressed_length}


def _node_brief(node: SearchNode) -> dict:
    return {
        "archive": os.path.basename(node.abspath),
        "kind": node.kind,
        "priority": node.priority,
    }


def _hash_brief(h: DriveHashResult | None) -> dict | None:
    if h is None:
        return None
    return {"hexdigest": h.hexdigest, "error": h.error, "source_archive": h.source_archive}
