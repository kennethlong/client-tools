---
phase: 29-tre-compare-tool-diff-engine-api
plan: 03
subsystem: tre-compare-tool
tags: [fastapi, http-api, app-factory, payload-keyed-memo, installs-picker, localhost-bind, sc4-integration]
requires:
  - "tre_compare.diff.diff_archive_set/diff_virtual_trees/drill_in/hash_virtual_file + DriveHashResult/DrillResult + _payload_tre_for_toc + _HASH_FAULTS (Phase 29-01)"
  - "tre_compare.cache.Cache (get_file_hash/put_file_hash node-keyed) + build_virtual_tree_cached + module-level iter_node_entries (Phase 29-02)"
  - "tre_compare.scanner.parse_shared_file/ScanResult/SearchNode (Phase 28-03)"
  - "fastapi/uvicorn runtime + httpx dev (Phase 29-01 deps)"
provides:
  - "tre_compare.api.create_app(cache, installs_path) FACTORY — four stateless routes, no module-level Cache"
  - "tre_compare.api.get_or_compute_hash — API-layer payload-keyed drill-in hash memo (sqlite stays out of diff.py)"
  - "tre_compare.config.load_installs + InstallEntry (tomllib installs.toml picker feed)"
  - "tre_compare.__main__ — localhost-only (127.0.0.1) uvicorn entrypoint"
  - "tests conftest tmp_cache/api_client/tmp_installs fixtures + test_api.py + env-gated real-pair integration test"
affects:
  - "Phase 30 (React SPA — consumes create_app's four-route JSON unchanged; install picker from /installs)"
tech-stack:
  added: []
  patterns:
    - "app FACTORY (create_app) over an injected Cache — no module-level state leak across TestClient tests (D-01/D-03)"
    - "API-layer payload-keyed hash memo via a synthetic SearchNode(abspath=payload_tre) so cache.get/put_file_hash key on the .tre, not the .toc (P3)"
    - "diff.drill_in stays HTTP-agnostic; the route maps domain status not_found->404 / rejected->400"
    - "config-driven installs picker (tomllib), gitignored real config + tracked .example"
    - "localhost-only uvicorn bind (127.0.0.1) as the single remote-reachability control"
key-files:
  created:
    - tools/tre-compare/src/tre_compare/config.py
    - tools/tre-compare/src/tre_compare/api.py
    - tools/tre-compare/src/tre_compare/__main__.py
    - tools/tre-compare/installs.toml.example
    - tools/tre-compare/tests/test_api.py
  modified:
    - tools/tre-compare/.gitignore
    - tools/tre-compare/tests/conftest.py
    - tools/tre-compare/tests/test_integration.py
decisions:
  - "the API-layer get_or_compute_hash keys the memo on the RESOLVED payload .tre via a replace(node, abspath=payload_tre) synthetic SearchNode, because cache.get/put_file_hash already stat node.abspath internally — so payload-keying needs only a swapped abspath, no cache API change (P3)"
  - "the route recomputes the verdict from the MEMOIZED hashes (not diff.drill_in's unmemoized ones) so content-indeterminate/confirmed/changed reflect the cached path"
  - "removed the literal 0.0.0.0 from __main__ prose so the acceptance grep (grep -v '^#' | grep -c 0.0.0.0 == 0) is unambiguous — the bind was always 127.0.0.1"
metrics:
  duration: "~18 min"
  completed: 2026-06-15
  tasks: 3
  files: 8
---

# Phase 29 Plan 03: FastAPI HTTP Surface (create_app factory + payload-keyed memo) Summary

The importable HTTP surface Phase 30's SPA consumes unchanged: a `create_app(cache,
installs_path)` FACTORY exposing four STATELESS routes that compose the Plan-01 pure diff
over the Plan-02 cached merge, an API-layer PAYLOAD-KEYED drill-in hash memo
(`get_or_compute_hash` — sqlite stays out of `diff.py`), a tomllib installs picker, and a
localhost-only uvicorn entrypoint. 12 new TestClient tests over an injected `tmp_cache` +
an env-gated real-pair integration test with a measured cache-HIT. Whole suite **72
passed, 2 deselected**.

## What Was Built

- **Task 1 — `config.py` + `installs.toml.example` + gitignore (`7eb055cbf`):**
  `load_installs(config_path) -> list[InstallEntry]` parses a `[[installs]]` array-of-tables
  via stdlib `tomllib`; NO `fastapi` import. Scanner-style tolerance: a missing config file
  returns `[]` (empty picker, not an error), a malformed/field-missing row is SKIPPED, a
  malformed TOML document degrades to `[]`. Default path resolved via `__file__`
  (`tools/tre-compare/installs.toml`) but the arg is overridable (testability).
  `installs.toml.example` documents the `name`/`cfg_path` shape (tracked); `installs.toml`
  added to `.gitignore` UNCONDITIONALLY (REVIEWS also-agreed — the real config carries
  machine-specific absolute install paths).

- **Task 2 — `api.py` factory + `__main__.py` + conftest + `test_api.py` (RED `baf2e18b9` →
  GREEN `b0d5442b4`):** `create_app(cache=None, installs_path=None) -> FastAPI` instantiates a
  default `Cache()` ONLY when `cache is None` (no module-level `Cache` — REVIEWS: a
  module-level Cache leaks state across TestClient tests). Pydantic `PairRequest{left_cfg,
  right_cfg}` + `FileDetailRequest(PairRequest){path}`. `_validate_cfg` raises
  `HTTPException(400, {"error", "cfg"})` on a missing/unreadable cfg BEFORE the scanner opens
  it. The four routes: `/compare/set` (`diff_archive_set`, corrupt archive → fault row, never
  500); `/compare/files` (`build_virtual_tree_cached` both + `diff_virtual_trees` → the FULL
  `{rows, summary}`, no pagination D-08); `/file/detail` (`drill_in` → `not_found`→404 /
  `rejected`→400 / else 200) with the per-side hash routed through the memo;
  `/installs` (`load_installs`). `__main__` binds `127.0.0.1` ONLY.

- **`get_or_compute_hash` (P3 payload-keyed memo):** keeps sqlite OUT of `diff.py`. For a
  `toc` winner it resolves the payload `.tre` EXACTLY as `diff._payload_tre_for_toc` does
  (match `fix_up_file_name(e.path)==vpath`, join `toc_tree_path`), then keys the memo on
  that payload `.tre`'s identity via `replace(node, abspath=payload_tre)` (because
  `cache.get/put_file_hash` already stat `node.abspath` — so payload-keying is just a
  swapped abspath, no cache API change). GET → on HIT return a synthetic `DriveHashResult`;
  on MISS `hash_virtual_file` → PUT (skipped when the read errored). A changed payload `.tre`
  under an unchanged `.toc` MISSes → a FRESH hash (proven by
  `test_file_detail_memo_payload_keyed`).

- **Task 3 — env-gated real-pair integration test (`46eac9150`):**
  `test_real_install_pair_compare_and_cache_hit` reads `TRE_COMPARE_LEFT_CFG` /
  `TRE_COMPARE_RIGHT_CFG`, skips cleanly when unset/absent (the default
  `pytest -m "not integration"` run stays machine-independent). When both present: set diff →
  rows; `diff_virtual_trees` → >0 tri-state rows; a SECOND `build_virtual_tree_cached` over
  the SAME cache PROVES the parse-skip via a spy on `tre_compare.cache.iter_node_entries`
  (second-compare call-count ≤ the loose `path`-node count AND strictly < the first compare —
  a MEASURED HIT, not row-equality, P5/SC#4).

## Requirements Satisfied

- **TRE-02** — `/compare/set` serves the set-level archive diff; a corrupt archive degrades
  to a `fault` row (proven by `test_compare_set_corrupt_never_500`), never a 500.
- **TRE-03** — `/compare/files` returns the ENTIRE lean-row array `{path, status, left,
  right}` (+ optional `qualifier`) + a summary count block in one response, no pagination
  (D-08/D-09); a one-sided tombstone surfaces a `qualifier` through the API
  (`test_compare_files_qualifier`).
- **TRE-04** — `/file/detail` returns winner + shadowed + per-side on-demand xxhash + an
  upgraded verdict (`content-confirmed-identical`/`content-changed`/`content-indeterminate`),
  hashing only on drill-in, MEMOIZED payload-keyed (P3); `not_found`→404, `rejected`→400.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] cache `get_file_hash`/`put_file_hash` take a `SearchNode`, not a raw `(abspath, mtime_ns, size)` tuple**

- **Found during:** Task 2 (wiring `get_or_compute_hash`).
- **Issue:** The plan's interface block described keying via
  `cache.get_file_hash(payload_abspath, mtime_ns, size, vpath, algo)`, but the Plan-02
  `Cache.get_file_hash(node, vpath, *, algo)` actually takes a `SearchNode` and stats
  `node.abspath` internally — a literal port of the plan's signature would not type-check.
- **Fix:** key the memo on the payload by passing a synthetic `replace(node,
  abspath=payload_tre)` SearchNode to the existing `get_file_hash`/`put_file_hash`. This
  achieves the P3 payload-keying with ZERO cache API change (the stat happens inside the
  cache on the swapped abspath). Verified by `test_file_detail_memo_payload_keyed` (mutated
  `payload.tre` → fresh hash).
- **Files modified:** `tools/tre-compare/src/tre_compare/api.py`
- **Commit:** `b0d5442b4`

**2. [Rule 3 - Blocking] literal `0.0.0.0` in `__main__` docstring/comment tripped the acceptance grep**

- **Found during:** Task 2 verification.
- **Issue:** The plan's acceptance grep `grep -v '^#' __main__.py | grep -c '0.0.0.0' == 0`
  matched the `0.0.0.0` literal inside the module DOCSTRING (line 6 does not start with `#`,
  so `grep -v '^#'` does not filter it) and a `# never 0.0.0.0` comment, even though the
  actual bind was always `127.0.0.1`.
- **Fix:** rephrased the prose to "all-interfaces address" so the literal `0.0.0.0` appears
  nowhere; the bind constant remains `HOST = "127.0.0.1"`. The gate now reads 0 / 2 / 1.
- **Files modified:** `tools/tre-compare/src/tre_compare/__main__.py`
- **Commit:** `b0d5442b4`

## Threat Mitigations Applied

- **T-29-03-01** (path traversal / arbitrary file read): `_validate_cfg` checks
  `is_file()` + `os.access(R_OK)` and raises `HTTPException(400, {error, cfg})` before the
  scanner opens a request-controlled cfg (`test_bad_cfg_400`); the drill-in `path` resolves
  only against `safe_virtual_key`-canonical keys already in the merged tree —
  `rejected`→400 (`test_file_detail_rejected`, an interior-`..` path) / `not_found`→404
  (`test_file_detail_not_found`), never an open. Residual arbitrary-cfg-open is bounded by
  the localhost-only bind (T-29-03-03). The scanner's `read_text` size cap is documented as
  accepted (Opus LOW) in a conftest NOTE.
- **T-29-03-02** (malformed-archive DoS): per-side `build_virtual_tree_cached` reuses the
  Phase-28 bounds preflight + `MAX_ENTRIES` + per-node fault isolation; a bad archive
  records into `node_errors` (surfaced in `summary`) and degrades to a fault row — never
  500s (`test_compare_set_corrupt_never_500`).
- **T-29-03-03** (remote exposure): `__main__` binds `127.0.0.1` ONLY; the acceptance grep
  asserts no non-comment `0.0.0.0` literal. Single-user localhost posture.
- **T-29-03-04** (committed user-local config): only `installs.toml.example` is tracked;
  `installs.toml` is gitignored unconditionally; the loader treats an absent config as `[]`.
- **T-29-03-05** (stale drill-in hash): `get_or_compute_hash` keys the `file_hash` memo on
  the RESOLVED payload `.tre`'s `(abspath, mtime_ns, size)`, not the `.toc`'s — a changed
  payload yields a fresh key (`test_file_detail_memo_payload_keyed`).

## Verification

- `uv run pytest -m "not integration" -q` → **72 passed, 2 deselected** (12 new api tests +
  Plan-01/02 + Phase-28 tests still green).
- `uv run pytest -m integration -q` (env unset) → **2 skipped, 72 deselected** (both
  integration tests skip cleanly).
- `grep -E '@app\.(post|get)' api.py` → the four routes (`/compare/set`, `/compare/files`,
  `/file/detail`, `/installs`).
- `grep -cE 'def create_app|get_or_compute_hash|status_code=404|status_code=400' api.py` → 8
  (factory + memo + status mapping present).
- `Cache()` appears in `api.py` only inside `create_app` (line 160, behind `if cache is
  None`) — no module-level instantiation.
- `grep -v '^#' diff.py | grep 'import sqlite3'` → clean (P3 boundary held — sqlite stayed
  out of `diff.py`).
- `__main__`: non-comment `0.0.0.0` count 0; `127.0.0.1` count 2; builds via `create_app()`.
- `grep -c '^installs.toml$' .gitignore` → 1.
- `wc -l api.py` → 260 (≥130).

## TDD Gate Compliance

Task 2 ran a per-task RED→GREEN cycle: RED gate `test(29-03)` `baf2e18b9` (test_api.py +
conftest fixtures, collection error on missing `tre_compare.api`) → GREEN gate `feat(29-03)`
`b0d5442b4` (api.py + __main__.py, 12/12 green). Tasks 1 and 3 are non-TDD (config loader /
env-gated integration test). All gate commits present in `git log`.

## Known Stubs

None. The API is wired end-to-end against the real Plan-01 diff engine + Plan-02 sqlite
cache + Phase-28 scanner, exercised over genuine on-disk synthetic archives via TestClient
(`create_app → parse_shared_file → build_virtual_tree_cached → diff/drill → memo`). No
placeholder data, no hardcoded empty returns. `installs.toml.example` carries documentation
sample paths (intentional, for the picker shape), not stubbed app behavior.

## Self-Check: PASSED

- FOUND: tools/tre-compare/src/tre_compare/config.py
- FOUND: tools/tre-compare/src/tre_compare/api.py
- FOUND: tools/tre-compare/src/tre_compare/__main__.py
- FOUND: tools/tre-compare/installs.toml.example
- FOUND: tools/tre-compare/tests/test_api.py
- FOUND commit: 7eb055cbf (feat config)
- FOUND commit: baf2e18b9 (test RED)
- FOUND commit: b0d5442b4 (feat GREEN)
- FOUND commit: 46eac9150 (test integration)
