# Phase 29: TRE Compare Tool — Diff Engine + API - Pattern Map

**Mapped:** 2026-06-14
**Files analyzed:** 7 new / 4 extended (3 core modules + `config.py` + `__main__.py` + tests)
**Analogs found:** 4 strong in-repo analogs / 3 genuinely-new layers (sqlite, FastAPI, xxhash — NO analog, conventions documented instead)

**Scope note:** All analogs live inside the standalone package `tools/tre-compare/src/tre_compare/` + its `tests/`. Phase 28 is frozen — Phase 29 adds new modules only; the planner must NOT edit `scanner.py` / `virtual_tree.py` / `parser/`.

---

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `src/tre_compare/diff.py` (NEW) | service (pure-fn engine) | transform | `virtual_tree.py` (pure fn over dataclasses) + `scanner.py` (frozen dataclass results) | role-match (strong) |
| `src/tre_compare/cache.py` (NEW) | store (sqlite index) | CRUD + transform | `virtual_tree.py` `build_virtual_tree` / `iter_node_entries` (parity target) | partial — NO sqlite analog in repo |
| `src/tre_compare/config.py` (NEW) | config (installs loader) | file-I/O (read) | `scanner.py` `parse_shared_file` (pure cfg→dataclass loader) | role-match |
| `src/tre_compare/api.py` (NEW) | controller (FastAPI routes) | request-response | none in repo (first dependency) | NO analog — conventions documented |
| `src/tre_compare/__main__.py` (NEW) | entrypoint | event-driven (server) | none in repo | NO analog |
| `tests/test_diff.py` (NEW) | test | — | `tests/test_virtual_tree.py` + `tests/_fixtures.py` byte-builders | exact |
| `tests/test_cache.py` (NEW) | test | — | `tests/conftest.py` `synthetic_install` + tmp_path | role-match |
| `tests/test_api.py` (NEW) | test | — | none (TestClient) — RESEARCH §Code Examples | partial |
| `tests/_fixtures.py` (EXTEND) | test fixture | — | itself (add `build_install_pair`) | exact |
| `tests/conftest.py` (EXTEND) | test fixture | — | itself (add `api_client`, `tmp_cache`) | exact |
| `tests/test_integration.py` (EXTEND) | test | — | itself (env-gated marker) | exact |
| `pyproject.toml` (EXTEND) | config | — | itself (Phase-28 dep/marker shape) | exact |

---

## Real Dataclass Field Reference (what diff/cache/api bind to)

These are the exact frozen-dataclass fields the new code consumes. **Bind to these names verbatim.**

**`SearchNode`** (`scanner.py:56`): `kind` (`"tree"|"toc"|"path"`), `priority: int`, `abspath: str`, `raw_key: str`, `sku: int`, `cfg_seq: int`.

**`ScanResult`** (`scanner.py:78`): `nodes: list[SearchNode]`, `max_search_priority: int|None`, `toc_tree_path: str|None`. ← `toc_tree_path` is the prefix Phase 29 must thread for TOC-payload hashing.

**`MergedEntry`** (`virtual_tree.py:162`): `canonical_path: str`, `winner_node: SearchNode`, `length: int`, `compressed_length: int`, `shadowed: tuple[SearchNode, ...]`.

**`VirtualTree`** (`virtual_tree.py:178`): `entries: dict[str, MergedEntry]`, `tombstoned: set[str]`, `rejected: list[str]`, `node_errors: list[tuple[SearchNode, str]]`.

**`TreHeader`** (`tre_reader.py:62`): `version: str`, `number_of_files: int`, `toc_offset`, `size_of_toc`, `size_of_name_block`, … ← set-level version + entry-count come from here.

**`TreEntry`** (`tre_reader.py:76`): `path`, `crc`, `length`, `offset`, `compressor`, `compressed_length`. **NEVER use `crc` for change detection** (path-CRC).

**`SearchTocEntry`** (`tre_reader.py:126`) / **`Cot2000Entry`** (`tre_reader.py:99`): both carry `path`, **`tre_file: str`** (payload `.tre` bare name — join with `toc_tree_path`), `crc`, `offset`, `length`, `compressor`, `compressed_length`.

**`MasterIndexKind`** (`tre_reader.py:57`, a `str, Enum`): `SEARCH_TOC` / (COT2000) — `detect_master_index_kind(path)` returns it.

**Reader entry points** (re-exported from `tre_compare.parser`, `parser/__init__.py:9`): `read_tre_header`, `read_tre_entries`, `read_tre_payload(path, logical_name) -> bytes` (decompresses), `read_search_toc_entries`, `read_cot2000_entries`, `detect_master_index_kind`, `toc_entry_stride`, `TreFormatError`, `UnsupportedTreVersionError`. `read_search_toc_header` / `read_cot2000_header` import from `parser.tre_reader` directly (as `virtual_tree.py:65` does).

---

## Pattern Assignments

### `src/tre_compare/diff.py` (service, pure-fn transform)

**Analog:** `virtual_tree.py` (pure stdlib transform, frozen dataclasses, NO web/db imports) + `scanner.py` (frozen `@dataclass` result types). This is the cleanest in-repo template — copy its *shape*, not its logic.

**Module-header / import pattern** — copy from `virtual_tree.py:46-71`. Note the `from __future__ import annotations`, the `from .parser import (...)` clean re-export usage, and `from .parser.tre_reader import (...)` for the header readers not re-exported at package level:
```python
from __future__ import annotations
import os
from dataclasses import dataclass, field, replace
from .parser import (
    detect_master_index_kind, read_tre_header, read_tre_entries,
    read_tre_payload, read_search_toc_entries, read_cot2000_entries,
    TreFormatError, UnsupportedTreVersionError,
)
from .parser.tre_reader import MasterIndexKind, read_search_toc_header, read_cot2000_header
from .scanner import ScanResult, SearchNode, parse_shared_file
from .virtual_tree import VirtualTree, MergedEntry, build_virtual_tree, fix_up_file_name, safe_virtual_key
```
**CRITICAL:** `diff.py` must NOT import `fastapi`, `sqlite3`, or `xxhash` at module scope beyond `xxhash` for the drill-in hash util (RESEARCH anti-pattern: keep the engine pure so Phase 30 + TREM can import it headless). RESEARCH places `xxhash` inside `diff.py`'s `hash_virtual_file`; acceptable since it is the hash primitive, but keep FastAPI/sqlite OUT.

**Frozen-dataclass result pattern** — copy from `MergedEntry` (`virtual_tree.py:162-176`) and `SearchNode` (`scanner.py:56-76`). New result types (`ArchiveStat`, diff rows) should be `@dataclass(frozen=True)` with a docstring naming the field semantics, matching the house style:
```python
@dataclass(frozen=True)
class MergedEntry:
    canonical_path: str
    winner_node: SearchNode
    length: int
    compressed_length: int
    shadowed: tuple[SearchNode, ...]
```

**Set-level diff core** (TRE-02 / D-04 / D-05) — basename-match `tree`/`toc` nodes only (skip `path`, RESEARCH Pitfall 6). Bind to `os.path.basename(node.abspath)`, `os.path.getsize`, `read_tre_header(...).version` + `.number_of_files`, and branch `toc` on `detect_master_index_kind`. See RESEARCH Pattern 1 (lines 191-231) for the verbatim `diff_archive_set` skeleton.

**File-level tri-state diff** (TRE-03 / D-06 / D-09) — iterate `sorted(set(L) | set(R))` over two `VirtualTree.entries` dicts; status from `(l.length, l.compressed_length) != (r.length, r.compressed_length)`. **Keys are already `safe_virtual_key`-canonical** (set in `build_virtual_tree`, `virtual_tree.py:364`) — diff by dict key directly. See RESEARCH Pattern 2 (lines 237-259). The `summary` MUST surface `len(vt.node_errors)`, `len(vt.rejected)`, `len(vt.tombstoned)` per side (Phase-28 code-context directive, CONTEXT lines 161-162).

**Fault-isolation idiom to PRESERVE** — copy the per-node try/except shape from `iter_node_entries` (`virtual_tree.py:308-338`). Drill-in hashing must degrade to `hash: null, hash_error: "..."` rather than raise (RESEARCH Pitfall 2), mirroring the `node_errors.append((node, str(exc)))` pattern:
```python
_NODE_FAULTS = (TreFormatError, UnsupportedTreVersionError, struct.error, OSError, MemoryError, zlib.error)
try:
    ...
except _NODE_FAULTS as exc:
    node_errors.append((node, str(exc)))
```

**Drill-in + TOC-payload hash** (TRE-04 / D-07) — bind to `MergedEntry.winner_node` + `.shadowed`; for `toc` winners resolve `os.path.join(scan.toc_tree_path or "", entry.tre_file)` then `read_tre_payload(payload_tre, entry.path)`; xxhash the **decompressed** bytes. Canonicalize with `fix_up_file_name(e.path) == vpath` on BOTH sides (RESEARCH Pitfall 4). See RESEARCH Pattern 3 (lines 265-294) — VERIFIED working end-to-end on the real SearchTOC install.

---

### `src/tre_compare/cache.py` (store, sqlite CRUD + parity transform)

**Analog: PARTIAL — NO sqlite3 code exists anywhere in the repo.** The closest behavioral analog is `build_virtual_tree` (`virtual_tree.py:344-402`), which `cache.py` must stay **parity-equivalent** to via a `build_virtual_tree_cached(scan, cache)`.

**What to copy from the analog (the merge logic to re-express):** the single-descending-pass first-hit-wins loop, tombstone rules, and `safe_virtual_key` keying from `build_virtual_tree` (`virtual_tree.py:355-396`). Per RESEARCH Cache Design "option (a)", re-express this ~40-line loop in `cache.py` sourcing entries from sqlite instead of `iter_node_entries`, importing `safe_virtual_key` + the tombstone predicates from `virtual_tree`. **Flag for planner (RESEARCH A4, MEDIUM risk):** add a parity test asserting `build_virtual_tree_cached` == `build_virtual_tree` on the same synthetic install.

**Fault-isolation idiom to PRESERVE:** wrap the cache's per-archive parse (the MISS path that calls `iter_node_entries` / the typed readers) in the same `_NODE_FAULTS` try/except as `iter_node_entries` (`virtual_tree.py:336-338`) so a hostile archive records an error and is skipped, never aborting the build. Store `tre_file` in `archive_entry` so drill-in resolves the payload `.tre` without re-parsing a 193k-entry `.toc` (RESEARCH Pitfall 7).

**NO in-repo pattern for these — follow RESEARCH instead:**
- Schema: three tables `archive_meta` / `archive_entry` / `file_hash`, keyed `(abspath, mtime, size[, vpath])` — RESEARCH §sqlite Cache Design (lines 323-351). Invalidation is implicit (key includes `(mtime, size)`).
- Connection lifecycle: `sqlite3.connect(db, check_same_thread=False)` + `PRAGMA journal_mode=WAL` + `synchronous=NORMAL` + a process-wide write `threading.Lock` (RESEARCH Pitfall 3).
- Cache file location: `tools/tre-compare/.cache/tre_compare.sqlite`, gitignored (add `.cache/` to `.gitignore`).

---

### `src/tre_compare/config.py` (config, file-I/O loader)

**Analog:** `scanner.py` `parse_shared_file` (`scanner.py:110-186`) — the in-repo template for "pure file → frozen-dataclass loader, REQUIRED path arg, no default location, tolerant of malformed lines." Mirror: `from __future__ import annotations`, a frozen `@dataclass` result, read via `Path(...).read_text(...)`, and silently skip malformed entries rather than crash (`scanner.py:147-150` `try/except ValueError: pass`).

**Difference from analog:** use stdlib `tomllib` (3.11+ floor, already met) to parse a tool-local `installs.toml` listing `{name, cfg_path}` (D-02). NOT a stateful registry — a static picker feed.

---

### `src/tre_compare/api.py` (controller, request-response)

**Analog: NONE — this is the first FastAPI/dependency surface in a previously zero-dep package.** Follow RESEARCH §FastAPI Surface (lines 367-399) + these carried-forward package conventions:

- Pydantic request models bind to real fields: `PairRequest{left_cfg, right_cfg}`, `FileDetailRequest(PairRequest){path}` (RESEARCH lines 374-379).
- Four stateless routes: `POST /compare/set`, `POST /compare/files`, `POST /file/detail`, `GET /installs` (D-03).
- Lean rows (D-09): `{path, status, left:{len,clen}, right:{len,clen}}` — bind `len`→`MergedEntry.length`, `clen`→`MergedEntry.compressed_length`. No hashes in bulk.
- Full payload (D-08): return the entire `rows` array + `summary` count block; no pagination.
- Error envelope: validate cfg paths exist+readable → `HTTPException(400, ...)`; surface per-archive `node_errors` in `summary`, never 500 the whole diff (mirrors Phase-28 fail-isolation).
- Export `app` and the diff functions so Phase 30 can `from tre_compare.api import app` with zero backend refactor.

---

### `src/tre_compare/__main__.py` (entrypoint)

**Analog: NONE.** Follow RESEARCH (line 396): `uvicorn.run(app, host="127.0.0.1", port=8765)` — localhost-only bind (anti-pattern: never `0.0.0.0`).

---

### `tests/test_diff.py`, `tests/test_cache.py`, `tests/test_api.py` (tests)

**Analog (exact): `tests/_fixtures.py` byte-builders + `tests/conftest.py` `synthetic_install` + `test_integration.py` gating.**

**Byte-built-fixture pattern** — the established test idiom is forging *genuine on-disk* archives via the `_fixtures.build_tre` / `build_search_toc` / `build_cot2000` / `write_synthetic_cfg` builders, then driving the REAL pipeline. Copy the `synthetic_install` fixture shape (`conftest.py:25-83`): build a tomb/real `.tre` pair + absent/real `.toc` pair + override dir, wire via `write_synthetic_cfg`, return a dict of paths.
```python
tomb_tre = _fixtures.build_tre(install / "high_tomb.tre", [("foo/x.dds", 0, 0)], version="0004")
real_tre = _fixtures.build_tre(install / "low_real.tre", [("foo/x.dds", 11, 11)], version="0004")
_fixtures.write_synthetic_cfg(cfg, [("path",10,override),("tree",9,tomb_tre),("tree",8,real_tre),("toc",5,absent_toc),("toc",4,real_toc)])
```
**Entry tuple contract:** `(name, length, compressed_length[, offset])`; `length=0` forges a tree tombstone; `length=0`/`offset=0` a non-shadowing TOC absent entry. To build the **false-identical** hash test (D-06), forge two installs with the SAME `(length, compressed_length)` but DIFFERENT payload bytes. To build the **TOC-payload drill-in** test, give `build_search_toc` a `tree_files=(...)` whose name a sibling `build_tre` payload matches, and set `toc_tree_path` in `write_synthetic_cfg` accordingly.

**Extend `_fixtures.py`** with a `build_install_pair()` helper (RESEARCH Wave-0 gap) producing two synthetic cfgs differing in set + file dimensions.

**Extend `conftest.py`** with `api_client` (`TestClient(app)`) and `tmp_cache` (`tmp_path`-backed sqlite) fixtures — mirror the existing `@pytest.fixture` + `tmp_path` style (`conftest.py:19-26`).

**Cache determinism (D-10)** — drive MISS→HIT→invalidate by rewriting a synthetic archive (changes `(mtime,size)` → key miss). See RESEARCH Code Example (lines 466-477).

**API tests** — `from fastapi.testclient import TestClient` over `from tre_compare.api import app` (RESEARCH lines 447-463). Requires `httpx` dev dep.

**Integration test (extend `test_integration.py`)** — copy the env-gating idiom verbatim (`test_integration.py:29-36`): `@pytest.mark.integration` + `@pytest.mark.skipif(not os.environ.get("TRE_COMPARE_INTEGRATION"))` + `pytest.skip` when cfg/archives absent. Add a TWO-install pair via env vars `TRE_COMPARE_LEFT_CFG` / `TRE_COMPARE_RIGHT_CFG` (RESEARCH Open-Q1), skip cleanly when unset.

---

### `pyproject.toml` (config, extend)

**Analog (exact): itself (`pyproject.toml:1-23`).** Mirror Phase-28 dep conventions: list runtime deps in `[project].dependencies` WITHOUT upper pins (`"fastapi>=0.137"`, `"uvicorn>=0.49"`, `"xxhash>=3.7"`); add `"httpx"` to `[dependency-groups].dev` alongside the existing `"pytest>=9.1.0"`. `uv.lock` is the reproducibility contract (D-11/D-12). Keep `requires-python = ">=3.11"`. Add an `integration`-style marker convention if a new marker is needed; the existing `[tool.pytest.ini_options]` markers/testpaths block (`pyproject.toml:21-23`) is the template.

---

## Shared Patterns

### Pure-function, frozen-dataclass, zero-coupling module shape
**Source:** `scanner.py`, `virtual_tree.py` (every public result is `@dataclass(frozen=True)`; modules open `from __future__ import annotations`; only relative `.parser` / `.scanner` imports — NEVER an engine import).
**Apply to:** `diff.py`, `config.py` (and keep `api.py`/`cache.py` from leaking web/db imports into `diff.py`).

### Fault-isolation (per-node skip-and-record, never abort)
**Source:** `virtual_tree.py:77-85` (`_NODE_FAULTS` tuple) + `iter_node_entries` `try/except` (`virtual_tree.py:308-338`) + the `node_errors`/`rejected`/`tombstoned` observability fields (`virtual_tree.py:185-193`).
**Apply to:** `cache.py` per-archive parse, `diff.py` drill-in hash, `api.py` summary (surface counts, don't 500). A malformed archive degrades the row, never the request.

### Canonical key discipline
**Source:** `safe_virtual_key` / `fix_up_file_name` (`virtual_tree.py:93-156`). `vt.entries` keys are already canonical; any raw `entry.path` re-read in drill-in must be re-canonicalized with `fix_up_file_name` before comparison (RESEARCH Pitfall 4). `safe_virtual_key` returning `None` ⇒ path is a 404 in drill-in.
**Apply to:** `diff.py` drill-in, `cache.py` `vpath` keying.

### Byte-built genuine-archive test fixtures + env-gated integration marker
**Source:** `tests/_fixtures.py` builders, `tests/conftest.py` `synthetic_install`, `tests/test_integration.py` gating.
**Apply to:** all three new test modules + the `_fixtures`/`conftest` extensions.

### NEVER use TOC `crc` for change detection
**Source:** asserted in `build_virtual_tree` docstring (`virtual_tree.py:352-354`) and locked by D-06. `crc` exists on `TreEntry`/`SearchTocEntry`/`Cot2000Entry` but is a path-CRC.
**Apply to:** all of `diff.py`. Change signal = `(length, compressed_length)` + on-demand xxhash only.

---

## No Analog Found

Files/concerns with no close match in the repo (use RESEARCH patterns + documented conventions, NOT an invented analog):

| Concern | Role | Data Flow | Reason |
|---------|------|-----------|--------|
| `cache.py` sqlite layer | store | CRUD | No `sqlite3` usage anywhere in repo. Parity target is `build_virtual_tree`; schema/lifecycle from RESEARCH §sqlite Cache Design + Pitfall 3. |
| `api.py` FastAPI app | controller | request-response | First dependency in a zero-dep package; no HTTP/framework code exists. Follow RESEARCH §FastAPI Surface. |
| `__main__.py` uvicorn entry | entrypoint | event-driven | No server bootstrap precedent. RESEARCH line 396. |
| xxhash content hashing | utility | transform | No hashing-for-content precedent (`crc` is path-CRC, explicitly NOT a content hash). D-11 locks `xxhash.xxh3_64_hexdigest` on decompressed bytes. |
| `installs.toml` loader | config | file-I/O | No toml usage; closest *shape* analog is `parse_shared_file`. Use stdlib `tomllib`. |

---

## Metadata

**Analog search scope:** `tools/tre-compare/src/tre_compare/` (`scanner.py`, `virtual_tree.py`, `parser/__init__.py`, `parser/tre_reader.py`), `tools/tre-compare/tests/` (`_fixtures.py`, `conftest.py`, `test_integration.py`), `pyproject.toml`. `.venv/` excluded.
**Files scanned (source read):** 9 (scanner, virtual_tree, parser/__init__, tre_reader [targeted ranges], _fixtures, conftest, test_integration, pyproject, CONTEXT/RESEARCH).
**Pattern extraction date:** 2026-06-14

---

## PATTERN MAPPING COMPLETE
