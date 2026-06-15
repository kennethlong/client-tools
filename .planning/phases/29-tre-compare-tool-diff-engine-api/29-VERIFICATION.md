---
phase: 29-tre-compare-tool-diff-engine-api
verified: 2026-06-14T22:45:00Z
human_verified: 2026-06-15T03:55:00Z
status: passed
score: 4/4
overrides_applied: 0
human_verification:
  - test: "With TRE_COMPARE_INTEGRATION=1 set plus TRE_COMPARE_LEFT_CFG and TRE_COMPARE_RIGHT_CFG pointing at two real client.cfg files, run: cd tools/tre-compare && uv run pytest -m integration -q"
    expected: "Both integration tests run (not skipped). The two-install real-pair test returns a non-empty set diff, a non-empty tri-state file diff with valid statuses, and the second build_virtual_tree_cached call proves a cache HIT via the iter_node_entries spy (second-compare call-count strictly less than first-compare count). This is the SC#4 north-star."
    why_human: "SC#4 requires real SWG installation files (TRE/TOC archives). The machine running verification must have two installations available and supply their cfg paths via env vars."
    result: "PASSED 2026-06-15 — ran SWGSource (4 SKU TOCs, ~200k entries) vs SWG Restoration (SwgRestoration.toc). pytest -m integration => 2 passed. SET rows=5, FILE rows=219,109 (identical 99,749 / changed 102,711 / added 10,650 / removed 5,999), 0 errors/rejected/tombstoned; cache HIT measured ~3x (18.09s parse -> 5.99s). See 29-HUMAN-UAT.md."
---

# Phase 29: TRE Compare Tool — Diff Engine + API Verification Report

**Phase Goal:** Build the set-level + file-level diff engine and the FastAPI surface over the proven virtual tree, so the UI works against real data from day one.
**Verified:** 2026-06-14T22:45:00Z
**Status:** human_needed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths (Roadmap Success Criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | A user can request a set-level compare between two installations and get which TRE archives are added/removed/changed, with size/version deltas | VERIFIED | `diff_archive_set` in `diff.py:175` keys by `(basename, kind)`, produces `added/removed/changed/identical/fault` rows with `size_delta`/`entry_count_delta`/`version_left`/`version_right`. `POST /compare/set` route in `api.py:164` wires it. `test_compare_set` and `test_set` pass. |
| 2 | A user can request a file-level compare with per-file added/removed/changed status, where "changed" is detected via `(length, compressedLength)` — NOT the TOC `crc` field | VERIFIED | `diff_virtual_trees` in `diff.py:253` operates on `(l.length, l.compressed_length)` vs `(r.length, r.compressed_length)`. `grep -n ".crc"` on `diff.py`, `test_diff.py`, and `api.py` returns nothing. `POST /compare/files` route wires the full `{rows, summary}` payload. `test_files`, `test_qualifier_*`, `test_compare_files_shape` all pass. |
| 3 | A user can drill into any file via the API and get metadata, winning archive, shadowed copies, and an on-demand real content hash (xxhash), computed only on drill-in — never eager | VERIFIED | `drill_in` in `diff.py:388` returns `DrillResult(status="ok"/"not_found"/"rejected")` + winner + shadowed + per-side `DriveHashResult`. `hash_virtual_file` at `diff.py:350` uses `xxhash.xxh3_64_hexdigest` on decompressed bytes. `POST /file/detail` in `api.py:180` maps `not_found→404`, `rejected→400`, and routes hashing through `get_or_compute_hash` (payload-keyed memo). `test_file_detail_false_identical`, `test_toc_payload`, `test_tree_payload_asymmetry`, `test_keyerror_degrade` all pass. |
| 4 | A sqlite index cache keyed by `(abspath, mtime, size)` makes re-runs instant, and the API returns correct diff JSON for the SWGSource-vs-whitengold install pair | VERIFIED (partial — synthetic tests pass; real-pair requires human) | `Cache` in `cache.py:110` uses `(abspath, mtime_ns INTEGER, size)` keying with `INSERT OR IGNORE` + WAL + `busy_timeout=5000`. `build_virtual_tree_cached` at `cache.py:298` is parity-equivalent to `build_virtual_tree` across a 7-case matrix (parse-skip proven by `iter_node_entries` spy, call-count==1 across two calls). 60 synthetic tests pass. The real SWGSource-vs-whitengold comparison (SC#4) requires env-gated integration test — see Human Verification Required. |

**Score:** 4/4 truths verified (SC#4 real-pair portion needs human testing)

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `tools/tre-compare/src/tre_compare/diff.py` | Pure diff engine: diff_archive_set, diff_virtual_trees, drill_in, hash_virtual_file, stat_archive + frozen dataclasses | VERIFIED | 440 lines (min 180). Defines all 8 required symbols. No fastapi/sqlite3 imports. `_HASH_FAULTS = (*_NODE_FAULTS, KeyError)`. Symmetric TREE/TOC payload resolution. |
| `tools/tre-compare/tests/test_diff.py` | set / collision / file tri-state / qualifier / summary / drill / hash / toc_payload / tree_payload_asymmetry / keyerror_degrade unit tests | VERIFIED | 208 lines (min 150). All 14 named test functions present and GREEN. |
| `tools/tre-compare/tests/_fixtures.py` | build_install_pair() helper extending Phase-28 byte-builders | VERIFIED | 504 lines. `build_install_pair` at line 318. `build_tre` gained `payloads=` arg for deterministic false-identical pair. |
| `tools/tre-compare/pyproject.toml` | fastapi/uvicorn(bare)/xxhash runtime floors + httpx dev floor | VERIFIED | `fastapi>=0.137`, `uvicorn>=0.49`, `xxhash>=3.7` in `[project].dependencies`; `httpx` in `[dependency-groups].dev`. No upper pins. |
| `tools/tre-compare/uv.lock` | Exact versions frozen; bare uvicorn proven | VERIFIED | 89,645 bytes. `grep -iE 'uvicorn[standard]|uvloop|httptools' uv.lock` returns nothing. fastapi 0.137.0, uvicorn 0.49.0, xxhash 3.7.0 frozen. |
| `tools/tre-compare/src/tre_compare/cache.py` | Cache class (sqlite3): archive_entries, get_file_hash/put_file_hash, build_virtual_tree_cached; __file__-relative default path; busy_timeout; INSERT OR IGNORE MISS | VERIFIED | 368 lines (min 160). All 5 key symbols present. `mtime_ns INTEGER` (not float), `check_same_thread=False`, `journal_mode=WAL`, `busy_timeout=5000`, `INSERT OR IGNORE`, `threading.Lock`, `__file__`-relative default. No fastapi. |
| `tools/tre-compare/tests/test_cache.py` | hit(parse-skip spy)/miss/invalidation + 7-case parity + hash memo + tre_file + concurrent_miss tests | VERIFIED | 317 lines (min 130). All 8 test functions present. Parse-skip proven by spy (call-count, not row-equality). Shadowed-tuple order + rejected-list order asserted. |
| `tools/tre-compare/.gitignore` | .cache/ ignored | VERIFIED | `.cache/` at line 7; `installs.toml` at line 8. Both present. |
| `tools/tre-compare/src/tre_compare/api.py` | create_app factory + four routes + get_or_compute_hash + 404/400 mapping | VERIFIED | 260 lines (min 130). `create_app`, `get_or_compute_hash`, `HTTPException(404)`, `HTTPException(400)`, all four route decorators present. `Cache()` instantiation only inside `create_app` (line 160, behind `if cache is None`). No module-level state. |
| `tools/tre-compare/src/tre_compare/config.py` | installs.toml loader (tomllib) | VERIFIED | 64 lines (min 25). `load_installs` + `InstallEntry` + `tomllib`. Scanner-style tolerance (missing file → `[]`, malformed entry → skip). |
| `tools/tre-compare/src/tre_compare/__main__.py` | uvicorn.run(create_app(), host='127.0.0.1') localhost entrypoint | VERIFIED | 27 lines. `HOST = "127.0.0.1"`. No non-comment `0.0.0.0` literal. Builds via `create_app()`. |
| `tools/tre-compare/tests/test_api.py` | TestClient route tests over injected tmp_cache | VERIFIED | 196 lines (min 120). 12 test functions covering all required behaviors including `test_file_detail_memo_payload_keyed` (mutated payload .tre → fresh hash). |
| `tools/tre-compare/tests/test_integration.py` | env-gated real-pair compare with second-compare cache-HIT assertion | VERIFIED (structure) | 162 lines. References `TRE_COMPARE_LEFT_CFG` and `TRE_COMPARE_RIGHT_CFG`. Spy on `tre_compare.cache.iter_node_entries` for measured HIT. Skips cleanly when unset. |
| `tools/tre-compare/installs.toml.example` | Documents [[installs]] shape | VERIFIED | 727 bytes. `[[installs]]` array-of-tables with `name`/`cfg_path` shape documented. Tracked in git; real `installs.toml` gitignored. |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `diff.py` | `scanner.parse_shared_file + ScanResult.nodes` | `(basename, kind)` match over tree/toc nodes | VERIFIED | `diff_archive_set` at line 165 keys `(os.path.basename(n.abspath), n.kind)` over `n.kind in ("tree", "toc")`. |
| `diff.py` | `virtual_tree.VirtualTree.entries/.tombstoned/.rejected/.node_errors` | file-level dict-key diff + qualifier + drill-in | VERIFIED | Lines 263-264 (`_rejected_keys`), 292-299 (`.tombstoned` qualifier), 313-319 (`.node_errors`/`.rejected`/`.tombstoned` in summary). |
| `diff.py` | `parser.read_tre_payload + read_tre_entries + fix_up_file_name` | symmetric TREE/TOC payload resolve + xxhash | VERIFIED | `read_tre_entries` at line 365; `fix_up_file_name(e.path)==vpath` at line 366; `xxhash.xxh3_64_hexdigest` at line 380. |
| `cache.py` | `virtual_tree.build_virtual_tree (parity target) + safe_virtual_key + _NODE_FAULTS` | re-expressed cached merge, kind-aware | VERIFIED | `safe_virtual_key` imported at module scope (line ~50). `build_virtual_tree_cached` re-expresses the descending-pass merge VERBATIM threading `node.kind` from live node. |
| `cache.py` | stdlib sqlite3 (archive_meta / archive_entry / file_hash) | `(abspath, mtime_ns, size)` keyed CRUD | VERIFIED | 3× `CREATE TABLE IF NOT EXISTS` confirmed (`grep -c` → 3). `mtime_ns INTEGER` schema, `INSERT OR IGNORE`, `busy_timeout=5000`. |
| `api.py` | `tre_compare.diff + tre_compare.cache` | route handlers compose diff over cached merge; `get_or_compute_hash` memoizes drill-in hash | VERIFIED | `from .cache import Cache, build_virtual_tree_cached` + `from .diff import diff_archive_set, diff_virtual_trees, drill_in` at top of api.py. All four routes call through to real diff functions — no hardcoded returns. |
| `api.py` | `tre_compare.config (installs loader)` | `GET /installs` handler | VERIFIED | `from .config import load_installs, InstallEntry` imported. `/installs` route calls `load_installs(installs_path)`. |
| `__main__.py` | `uvicorn + tre_compare.api.create_app` | `uvicorn.run(create_app(), host='127.0.0.1')` | VERIFIED | `HOST = "127.0.0.1"` at line 17; `uvicorn.run(app, host=HOST, port=PORT)` at line 23. |

### Data-Flow Trace (Level 4)

| Artifact | Data Variable | Source | Produces Real Data | Status |
|----------|---------------|--------|--------------------|--------|
| `api.py /compare/set` | `rows` | `diff_archive_set(scanL, scanR)` → `stat_archive(node)` → real `read_tre_header/read_search_toc_header` + `os.path.getsize` | Yes — real archive headers + filesystem stats | FLOWING |
| `api.py /compare/files` | `{rows, summary}` | `build_virtual_tree_cached(scanL/R, cache)` + `diff_virtual_trees(vtL, vtR)` — sourced from sqlite cache or `iter_node_entries` on MISS | Yes — real archive entries from sqlite or parse | FLOWING |
| `api.py /file/detail` | `hash_left`/`hash_right` | `get_or_compute_hash` → `hash_virtual_file` → `read_tre_entries` + `read_tre_payload` → `xxhash.xxh3_64_hexdigest(data)` | Yes — decompressed archive bytes hashed on demand | FLOWING |
| `api.py /installs` | install list | `load_installs(installs_path)` → `tomllib.load(fh)` | Yes — reads real installs.toml; empty list when absent | FLOWING |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Full synthetic test suite (72 tests) | `cd tools/tre-compare && uv run pytest -m "not integration" -q` | 72 passed, 2 deselected, 1 warning (Starlette deprecation, non-blocking) in 1.83s | PASS |
| Runtime deps importable | `uv run python -c "import fastapi, uvicorn, xxhash; print('ok')"` | `ok` | PASS |
| Bare uvicorn proven | `grep -iE 'uvicorn[standard]|uvloop|httptools' uv.lock` | No output | PASS |
| diff.py purity | `grep -v "^#" diff.py \| grep -E 'import fastapi\|import sqlite3'` | No output | PASS |
| sqlite3 stayed out of diff.py (P3 boundary) | `grep -v "^#" diff.py \| grep 'import sqlite3'` | No output | PASS |
| _HASH_FAULTS includes KeyError | `grep -n "_HASH_FAULTS\|KeyError" diff.py` | Line 59: `_HASH_FAULTS = (*_NODE_FAULTS, KeyError)` | PASS |
| 127.0.0.1 bind, no 0.0.0.0 | `grep -v "^#" __main__.py \| grep -c '0\.0\.0\.0'` | 0 | PASS |
| installs.toml gitignored | `grep -c '^installs.toml$' .gitignore` | 1 | PASS |
| Integration tests skip cleanly without env vars | `uv run pytest -m integration -q` (no env vars) | 2 skipped, 72 deselected | PASS |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| TRE-02 | 29-01, 29-02, 29-03 | User can see a set-level compare — archives added/removed/changed with size/version deltas | SATISFIED | `diff_archive_set` (Plan 01), `POST /compare/set` route (Plan 03), `test_set`/`test_set_collision`/`test_set_corrupt` (Plan 01) all GREEN |
| TRE-03 | 29-01, 29-02, 29-03 | User can see a file-level compare with added/removed/changed per-file status (first-match-wins, fixUpFileName normalization) | SATISFIED | `diff_virtual_trees` with `(length, compressed_length)` signal (never crc), optional `qualifier` for tombstone/rejection/parse-failure, `POST /compare/files` route, 6 related tests GREEN |
| TRE-04 | 29-01, 29-02, 29-03 | User can drill into any file — metadata, winner, shadowed, on-demand content hash (xxhash) | SATISFIED | `drill_in` + `hash_virtual_file` with symmetric TREE/TOC payload resolve, `DriveHashResult` (structured, never-raise), `POST /file/detail` route mapping `not_found→404`/`rejected→400`, 8 related tests GREEN |

All three requirement IDs declared across all three plans are satisfied. REQUIREMENTS.md confirms TRE-02, TRE-03, TRE-04 all marked `[x] Complete` under Phase 29.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `tests/test_api.py` | (Starlette warning) | `StarletteDeprecationWarning: Using httpx with starlette.testclient is deprecated; install httpx2 instead` | Info | Non-blocking deprecation warning. Tests pass. Not a code anti-pattern — this is a library version compatibility notice in the test runner output. No action required for Phase 29. |

No stub patterns, no hardcoded empty returns, no TODO/FIXME in source files, no placeholder implementations found. All routes return real computed data.

### Human Verification Required

#### 1. SC#4: Real SWGSource-vs-whitengold Install Pair Compare + Cache HIT

**Test:** Set env vars and run the integration suite:
```
$env:TRE_COMPARE_INTEGRATION = "1"
$env:TRE_COMPARE_LEFT_CFG = "D:\Code\SWGSource Client v3.0\client.cfg"
$env:TRE_COMPARE_RIGHT_CFG = "<path to whitengold client.cfg>"
cd tools/tre-compare && uv run pytest -m integration -q -v
```

**Expected:** Both integration tests run (not skipped). The `test_real_install_pair_compare_and_cache_hit` test:
- Returns `>0` set-level diff rows with valid statuses
- Returns `>0` file-level tri-state rows with statuses in `{added, removed, changed, identical-by-metadata}`
- The second `build_virtual_tree_cached` call (after the first run populated the cache) has `iter_node_entries` call-count strictly less than the first run, proving a measured parse-skip cache HIT (not just row equality)

**Why human:** Requires two real SWG installation directories with actual TRE/TOC archives. The whitengold cfg path is not known at verification time and the SWGSource installation is machine-local. Cannot be automated without those files.

### Gaps Summary

No gaps blocking goal achievement. All synthetic tests pass (72/72). The human verification item is the SC#4 real-pair integration test, which is env-gated by design and skips cleanly when the installation paths are not available. The automated portion of all four roadmap success criteria is fully verified.

---

_Verified: 2026-06-14T22:45:00Z_
_Verifier: Claude (gsd-verifier)_
