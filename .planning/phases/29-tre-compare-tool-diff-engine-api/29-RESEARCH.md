# Phase 29: TRE Compare Tool — Diff Engine + API — Research

**Researched:** 2026-06-14
**Domain:** Python diff-engine + FastAPI service + sqlite cache over the Phase-28 `tools/tre-compare/` library
**Confidence:** HIGH (Phase-28 surface read verbatim this session; dep versions verified against PyPI; real cfg inspected; TOC-payload drill-in VERIFIED end-to-end against the real install)

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions (D-01..D-12)
- **D-01:** Stateless `POST`-a-pair core contract. Each compare sends the two install `client.cfg` paths in the request body; **no stateful server-side install registry/lifecycle**. The sqlite cache (keyed by `(abspath, mtime, size)`) is the only persistence. Single-user localhost tool.
- **D-02:** Lightweight **`GET /installs`** returning a **config-driven** list of known install cfg paths (from a small tool-local config, NOT a stateful registry) to populate Phase 30's picker.
- **D-03:** Endpoint surface (planner may refine exact paths/verbs, keep the shape):
  - `POST /compare/set`   `{ left_cfg, right_cfg }` → set-delta (added/removed/changed archives)
  - `POST /compare/files` `{ left_cfg, right_cfg }` → full file-level diff (D-08/D-09)
  - `POST /file/detail`   `{ left_cfg, right_cfg, path }` → drill-in (D-07) incl. on-demand hash
  - `GET  /installs` → config-driven picker list
- **D-04:** Set-level: **match archives by basename** (filename only, ignoring directory). only-in-A = **removed**, only-in-B = **added**, present-in-both = **changed candidate**.
- **D-05:** An archive is **`changed`** if **any of**: on-disk file **size**, **entry-count**, or **TREE version tag** differs. All three deltas surfaced in the row. Cheap, no-content-hashing signal (header/stat reads only).
- **D-06:** **Tri-state file status** per merged-tree entry: `added` / `removed` / `changed` / `identical-by-metadata`, where `identical-by-metadata` = `(length, compressedLength)` match but content **NOT** hash-verified; `changed` = `(length, clen)` differ. Drill-in xxhash (D-07) upgrades `identical-by-metadata` → `content-confirmed-identical` or `content-changed`. **NEVER** use TOC `crc` (path-CRC, not content hash).
- **D-07:** **Drill-in** returns: file metadata (`length`, `compressedLength`, winning `SearchNode`), **winning archive**, **shadowed copies** (from `MergedEntry.shadowed`), and an **on-demand xxhash** computed **only on drill-in** for BOTH sides — never eager full-archive hashing.
- **D-08:** **Full payload, client-virtualized delivery.** `/compare/files` returns the entire flat diff array in one JSON response + a summary count block; Phase 30 virtualizes/filters/searches in-browser. No server-side pagination.
- **D-09:** Keep per-row JSON **lean** (`{ path, status, left:{len,clen}, right:{len,clen} }`). Hashes NOT in bulk payload — only in drill-in (D-07).
- **D-10:** **sqlite cache stores per-archive parsed TOC entries**, keyed by `(abspath, mtime, size)`. Re-runs skip re-parsing. Also memoize drill-in xxhash in a `file_hash` table keyed by `(abspath, mtime, size, vpath)`. Granular/reusable across ANY install pairing — NOT a per-pair diff blob.
- **D-11:** **xxhash via the `xxhash` PyPI package (C extension)**, added to `pyproject.toml`, pinned in `uv.lock`.
- **D-12:** New runtime deps: **FastAPI + uvicorn (ASGI) + xxhash**. **sqlite is stdlib** (no dep). All pinned in `uv.lock`; directory stays copy-paste-extractable; no engine imports.

**Carried forward from Phase 28 (locked — do NOT re-litigate):**
- Standalone/extractable: `tools/tre-compare/` fully isolated package — own `pyproject.toml` + `uv.lock` + venv, **zero engine imports**, copy-paste movable. (Hard constraint.)
- Toolchain: uv for venv/lock/run, pytest for tests.
- "Changed" signal source: `(length, compressed_length)` + on-demand xxhash, **never** TOC `crc`.

### Claude's Discretion
- Exact endpoint paths/verbs, Pydantic field names, error-envelope shape, localhost-only binding/security posture.
- sqlite schema details, cache file location (within tool dir, gitignored), whether merged-tree build is memoized in-process vs re-run-from-cache each request.
- Whether on-demand hash reads decompressed vs stored-compressed bytes — pick the one that makes false-identical detection correct (likely decompressed/full extracted bytes).
- Test strategy mirrors Phase 28: synthetic fixtures (default) + one env-gated integration test against real SWGSource-vs-whitengold pair (SC#4).

### Deferred Ideas (OUT OF SCOPE)
- React/Vite/shadcn SPA — Phase 30 (TRE-05).
- Server-side pagination/filtering — explicitly NOT built (D-08).
- Stateful install registry/lifecycle — explicitly NOT built (D-01).
- TRE management (extract/repack/IFF-aware diff) — TREM-01..03, later. **Architecture must not preclude these.**
- Phase-28 approximation-boundary items (lazy searchPath re-scan, `searchAbsolute`/`searchCache`/preloaded-cache nodes) — revisit ONLY if real cfgs exercise them. **Verified below: the real cfg does NOT exercise them.**
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| TRE-02 | Set-level compare → added/removed/changed archives with size/version deltas | Diff §Set-Level binds to `parse_shared_file().nodes` (SearchNode.abspath basename) + `read_tre_header().version`/`number_of_files` + `os.path.getsize`. See Architecture Pattern 1. |
| TRE-03 | File-level compare of merged virtual trees; `changed` via `(length, compressedLength)` NOT TOC `crc` | Diff §File-Level binds to two `build_virtual_tree().entries` dicts; `MergedEntry.length`/`.compressed_length`. Tri-state per D-06. Pattern 2. |
| TRE-04 | Drill-in: metadata, winning archive, shadowed copies, on-demand xxhash | Drill-in binds to `MergedEntry.winner_node`/`.shadowed` + `read_tre_payload` (decompressed bytes) → xxhash. The TOC-payload resolution path is VERIFIED working on the real install — see Pattern 3 + Verified Findings. |
</phase_requirements>

## Summary

Phase 29 adds three thin layers on top of the proven, unit-tested Phase-28 core (`tre_compare.parser` + `.scanner` + `.virtual_tree`): (1) a pure-function **diff engine** that consumes the existing `ScanResult.nodes` (set-level) and two `VirtualTree.entries` dicts (file-level); (2) a **FastAPI** app exposing four stateless endpoints; (3) a **stdlib sqlite3** cache of per-archive parsed entries and memoized drill-in hashes. No Phase-28 refactor is needed — every public dataclass the API and diff need (`SearchNode`, `ScanResult`, `MergedEntry`, `VirtualTree`) already exposes exactly the fields the locked decisions reference.

The single non-trivial technical wrinkle is the **on-demand content hash for a TOC-served winner** (D-07/TRE-04). The Phase-28 reader's `read_tre_payload(tre_path, logical_name)` returns *decompressed* bytes, but only for a **`.tre`** path. When a virtual file's winner is a **`toc`** node, the `SearchNode.abspath` points at the *master index* (`.toc`), not the payload `.tre` that actually holds the bytes. The payload `.tre` is resolved as `TOCTreePath + entry.tre_file` (both `SearchTocEntry` and `Cot2000Entry` carry a `tre_file` field; `ScanResult.toc_tree_path` carries the prefix). Phase 29 must thread `TOCTreePath` to resolve TOC-served payloads — this is explicitly flagged in the Phase-28 `ScanResult.toc_tree_path` docstring as "Phase 29 must thread it if it ever resolves `.toc`-referenced payload `.tre`s." The real SWGSource cfg is **all `searchTOC`**, so this path WILL be exercised by the north-star SC#4 scenario — it is not optional. **This mechanism is now VERIFIED end-to-end against the real install** (see §Verified Findings) — it is a confirmed binding, not an assumption.

**Primary recommendation:** Build the diff engine as **pure stdlib functions** in a new `tre_compare.diff` module (no FastAPI/sqlite imports — keep it importable and testable in isolation, so Phase 30 and TREM-01..03 can reuse it). Layer FastAPI (`tre_compare.api`) and the sqlite cache (`tre_compare.cache`) around it. Add `fastapi`, bare `uvicorn`, and `xxhash` to `pyproject.toml` `dependencies`, pin via `uv lock`. Read **decompressed** bytes for the drill-in hash (correct false-identical detection — verified: decompressed length matches `entry.length`). For TOC winners, resolve the payload `.tre` via `TOCTreePath + entry.tre_file` before calling `read_tre_payload`.

## Verified Findings (post-draft live probe, 2026-06-14)

Run against the real install on this machine `[VERIFIED: probe of "D:/Code/SWGSource Client v3.0/sku0_client.toc"]`:

- The real `sku*_client.toc` master indexes are **SearchTOC (TAG_TOC/0001)**, NOT COT2000. `detect_master_index_kind` returns `MasterIndexKind.SEARCH_TOC`. (Both parser paths exist; the diff/hash code must branch on `detect_master_index_kind` — Pattern 3 does.)
- `sku0_client.toc` indexes **193,475 entries** — confirms the 100k-scale assumption behind D-08 (full-payload, client-virtualized) is realistic; a single install side is ~200k entries, so the merged diff array is genuinely large and the lean-row contract (D-09) matters.
- `entry.tre_file` is a **bare filename** (e.g. `data_animation_00.tre`). `os.path.join(TOCTreePath, entry.tre_file)` → `D:/Code/SWGSource Client v3.0/data_animation_00.tre` → **exists**. This resolves Open-Q1/A1 (the join is bare-name + `TOCTreePath`, not pre-qualified).
- `read_tre_payload(payload_tre, entry.path)` returns **decompressed** bytes whose length **exactly equals `entry.length`** (probed entry: 27616 decompressed == 27616 `length`). Confirms: (a) the TOC→payload resolution works end-to-end, (b) decompressed-byte hashing is the correct false-identical signal, (c) `read_tre_payload` is the right call for the drill-in hash.
- `searchAbsolute`/`searchCache`/preload nodes are **absent** from the real cfg → the Phase-28 approximation boundary is NOT exercised. Carry the static-snapshot behavior forward; no Phase-29 revisit needed.
- `xxhash` is not yet installed (expected — it is a Phase-29 dep to add). All other bindings ran on the existing zero-dep environment.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Set-level archive diff (TRE-02) | Diff engine (pure fn) | Cache (header/stat memo) | Pure transform over `ScanResult.nodes` + header reads; cacheable by `(abspath,mtime,size)` |
| File-level virtual-tree diff (TRE-03) | Diff engine (pure fn) | Cache (parsed entries) | Pure transform over two `VirtualTree.entries` dicts; entries come from cached per-archive parses |
| Drill-in metadata + shadowed (TRE-04) | Diff engine / virtual_tree | — | `MergedEntry` already carries `winner_node` + `shadowed`; pure read |
| On-demand content hash (D-07) | Parser (`read_tre_payload`) + new hash util | Cache (`file_hash` table) | Decompress via existing reader; xxhash the bytes; memoize by `(abspath,mtime,size,vpath)` |
| HTTP surface / request validation | API (FastAPI + Pydantic) | — | Stateless POST-a-pair; localhost-bound |
| Persistence / invalidation | Cache (stdlib sqlite3) | — | `(abspath,mtime,size)` keying; mtime/size invalidation |
| Install picker feed | API (`GET /installs`) | tool-local config file | Config-driven, no registry state (D-02) |

## Standard Stack

### Core (new for Phase 29)
| Library | Version (verified) | Purpose | Why Standard |
|---------|--------------------|---------|--------------|
| `fastapi` | 0.137.0 | ASGI web framework + Pydantic request/response models + auto OpenAPI | The de-facto modern Python API framework; first-class Pydantic v2 typing matches the lean-row contract (D-09) `[VERIFIED: pypi.org/pypi/fastapi/json, requires-python>=3.10]` |
| `uvicorn` | 0.49.0 | ASGI server to run the FastAPI app | The canonical FastAPI server; recommend **bare** (not `[standard]`) to minimize binary-wheel surface for extractability `[VERIFIED: pypi.org/pypi/uvicorn/json]` |
| `xxhash` | 3.7.0 | Fast non-cryptographic content hash for drill-in (D-07/D-11) | SC#3 names xxhash explicitly; C-extension `xxh3_64`/`xxh64` is ~GB/s, ideal for on-demand single-file hashing `[VERIFIED: pypi.org/pypi/xxhash/json, requires-python>=3.8]` |

### Supporting (transitive / already present)
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `pydantic` | 2.13.4 (FastAPI dep) | Request/response model validation | Pulled transitively by FastAPI; do NOT pin separately unless a feature needs it `[VERIFIED: pypi]` |
| `sqlite3` | stdlib | Per-archive entry cache + hash memo (D-10) | Always — no dep (D-12) |
| `tomllib` | stdlib (3.11+) | Parse tool-local `installs.toml` (D-02) | `GET /installs` config loader |
| `hashlib`/`zlib`/`struct` | stdlib | Already used by the vendored parser | Already present |
| `pytest` | 9.1.0 (dev) | Test runner (Phase-28 dev group) | Already present |
| `httpx` | (FastAPI `TestClient` dep) | In-process API testing via `fastapi.testclient.TestClient` | Test-only; `starlette.testclient.TestClient` requires `httpx`. Add to `dev` group. `[CITED: fastapi.tiangolo.com/tutorial/testing]` |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `xxhash` (C ext) | stdlib `hashlib.blake2b`/`sha1` | Pure-stdlib (zero new dep, preserves extractability more cleanly) but slower; **D-11 LOCKED xxhash** — not a real choice, noted only for completeness |
| `fastapi`+`uvicorn` | stdlib `http.server` + manual JSON | Zero deps but hand-rolled routing/validation/OpenAPI; **D-12 LOCKED FastAPI** |
| bare `uvicorn` | `uvicorn[standard]` | `[standard]` adds `uvloop`(no Win wheel)/`httptools`/`websockets` — extra binary surface. For a localhost single-user tool bare is sufficient and **lighter for extractability** (see Pitfall 5). |

**Installation:**
```bash
# from tools/tre-compare/
uv add fastapi uvicorn xxhash
uv add --dev httpx          # for fastapi TestClient
uv lock                     # pin in uv.lock (D-11/D-12)
uv sync
```

**Version verification (run before locking; training data may be stale):**
```bash
uv run python -c "import fastapi, uvicorn, xxhash; print(fastapi.__version__, uvicorn.__version__, xxhash.VERSION)"
```
Verified 2026-06-14 against PyPI: fastapi 0.137.0, uvicorn 0.49.0, xxhash 3.7.0, pydantic 2.13.4 (transitive).

## Architecture Patterns

### System Architecture Diagram

```
                        ┌─────────────────────────── tre_compare.api (FastAPI) ──────────────────────────┐
  HTTP (localhost)      │  POST /compare/set    POST /compare/files    POST /file/detail   GET /installs  │
  ───────────────────►  │      │                     │                      │                   │         │
  { left_cfg,           │      ▼                     ▼                      ▼                   ▼         │
    right_cfg[, path] } │  Pydantic request models (validate cfg paths exist / are readable)             │
                        └──────┼─────────────────────┼──────────────────────┼───────────────────┼────────┘
                               │                     │                      │                   │
                               ▼                     ▼                      ▼                   ▼
                        ┌──────────────── tre_compare.diff (pure functions, NO web/db imports) ──────────┐
                        │  diff_archive_set(scanL, scanR)        diff_virtual_trees(vtL, vtR)            │
                        │  drill_in(vtL, vtR, path, ...)  ──────────────► hash_virtual_file(side, path)  │
                        └──────┬─────────────────────┬──────────────────────┬───────────────────┬────────┘
                               │                     │                      │                   │
                               ▼ scan/headers        ▼ merged entries       ▼ winner+shadowed   ▼ config list
                        ┌──────────────── tre_compare.cache (stdlib sqlite3) ──────────────────┐  ┌──────────┐
                        │  archive_entries(abspath,mtime,size) ─┐  file_hash(abspath,mtime,    │  │ installs │
                        │  (memoized per-archive parsed TOC)    │   size,vpath) (xxhash memo)   │  │  .json/  │
                        └──────┬────────────────────────────────┼──────────────────────────────┘  │  .toml   │
                               │ MISS                           │ MISS                              └──────────┘
                               ▼                                ▼
                        ┌──────────── tre_compare.parser / .scanner / .virtual_tree (PHASE 28, unchanged) ──┐
                        │  parse_shared_file(cfg) → ScanResult{ nodes, max_search_priority, toc_tree_path } │
                        │  build_virtual_tree(scan) → VirtualTree{ entries, tombstoned, rejected,           │
                        │                                          node_errors }                            │
                        │  read_tre_header / read_tre_entries / read_*_toc_entries / read_tre_payload       │
                        └──────────────────────────────────────────────────────────────────────────────────┘
```

Trace the north-star (SC#4): `POST /compare/files {SWGSource cfg, whitengold cfg}` → validate both cfgs → for each side: `parse_shared_file` (cache-independent, cheap) then `build_virtual_tree` (per-archive entry parses served from cache where `(abspath,mtime,size)` hits) → `diff_virtual_trees(vtL, vtR)` produces the flat tri-state array + summary → one JSON response (D-08). Drill-in later: `POST /file/detail {…, path}` → look up `MergedEntry` both sides → `hash_virtual_file` decompresses + xxhashes (memoized) → upgrade verdict.

### Recommended Project Structure
```
tools/tre-compare/
├── pyproject.toml            # + fastapi, uvicorn, xxhash deps; + httpx dev
├── uv.lock                   # re-locked
├── src/tre_compare/
│   ├── parser/               # Phase 28 — UNCHANGED
│   ├── scanner.py            # Phase 28 — UNCHANGED
│   ├── virtual_tree.py       # Phase 28 — UNCHANGED
│   ├── diff.py               # NEW: pure-fn diff engine (set + file + drill-in + hash)
│   ├── cache.py              # NEW: stdlib sqlite3 cache (archive entries + file_hash)
│   ├── config.py             # NEW: tool-local installs config loader (D-02)
│   ├── api.py                # NEW: FastAPI app + Pydantic models + routes
│   └── __main__.py           # NEW: `python -m tre_compare` → uvicorn.run(app, host=127.0.0.1)
└── tests/
    ├── _fixtures.py          # Phase 28 byte-builders — REUSE; extend with build_install_pair()
    ├── conftest.py           # Phase 28 — extend with api_client + tmp cache fixtures
    ├── test_diff.py          # NEW: set + file tri-state diff on synthetic pairs
    ├── test_cache.py         # NEW: hit/miss/invalidation determinism
    ├── test_api.py           # NEW: TestClient route tests
    └── test_integration.py   # Phase 28 — extend/add gated SWGSource-vs-whitengold compare
```

### Pattern 1: Set-Level Diff (TRE-02 / D-04 / D-05)
**What:** Basename-match the two installs' archive node lists; classify added/removed/changed; surface size/entry-count/version deltas.
**When:** `POST /compare/set`.
**Binds to (REAL signatures):** `parse_shared_file(cfg) -> ScanResult`; iterate `scan.nodes` (`SearchNode.kind in {"tree","toc","path"}`, `.abspath`). For each `tree` node: `read_tre_header(abspath).version` + `.number_of_files`, `os.path.getsize(abspath)`. For each `toc` node: branch on `detect_master_index_kind` → `read_search_toc_header`/`read_cot2000_header` for `.number_of_files`; version tag = the master-index kind string. (Skip `path` nodes — loose dirs have no single archive identity; they participate in file-level.)
```python
# tre_compare/diff.py  — pure stdlib, no web/db imports
import os
from dataclasses import dataclass
from .scanner import parse_shared_file
from .parser import read_tre_header, detect_master_index_kind  # version/entry-count

@dataclass(frozen=True)
class ArchiveStat:
    basename: str
    abspath: str
    kind: str            # "tree" | "toc"
    size: int            # os.path.getsize
    entry_count: int     # header.number_of_files
    version: str         # TREE tag ("0004".. ) or master-index kind for toc

def stat_archive(node) -> ArchiveStat:
    size = os.path.getsize(node.abspath)
    if node.kind == "tree":
        h = read_tre_header(node.abspath)
        return ArchiveStat(os.path.basename(node.abspath), node.abspath, "tree",
                           size, h.number_of_files, h.version)
    # toc master index: branch on detect_master_index_kind →
    # read_search_toc_header / read_cot2000_header for number_of_files; version = kind.value
    ...

def diff_archive_set(scanL, scanR) -> list[dict]:
    L = {os.path.basename(n.abspath): n for n in scanL.nodes if n.kind in ("tree","toc")}
    R = {os.path.basename(n.abspath): n for n in scanR.nodes if n.kind in ("tree","toc")}
    rows = []
    for name in sorted(set(L) | set(R)):
        if name in L and name not in R: rows.append({"archive": name, "status": "removed", ...})
        elif name in R and name not in L: rows.append({"archive": name, "status": "added", ...})
        else:
            sl, sr = stat_archive(L[name]), stat_archive(R[name])
            changed = (sl.size != sr.size) or (sl.entry_count != sr.entry_count) or (sl.version != sr.version)
            rows.append({"archive": name, "status": "changed" if changed else "identical",
                         "size_delta": sr.size - sl.size,
                         "entry_count_delta": sr.entry_count - sl.entry_count,
                         "version_left": sl.version, "version_right": sr.version})
    return rows
```

### Pattern 2: File-Level Tri-State Diff (TRE-03 / D-06 / D-09)
**What:** Compare two `VirtualTree.entries` dicts by canonical key; emit lean tri-state rows.
**Binds to (REAL signatures):** `build_virtual_tree(scan) -> VirtualTree`; `vt.entries: dict[str, MergedEntry]`; `MergedEntry.length`, `.compressed_length`. **Key is already canonical** (`safe_virtual_key` applied in Phase 28) — diff by dict key directly.
```python
def diff_virtual_trees(vtL, vtR) -> dict:
    L, R = vtL.entries, vtR.entries
    rows = []
    for key in sorted(set(L) | set(R)):
        l, r = L.get(key), R.get(key)
        if l and not r:   status = "removed"
        elif r and not l: status = "added"
        elif (l.length, l.compressed_length) != (r.length, r.compressed_length):
            status = "changed"
        else:
            status = "identical-by-metadata"   # D-06: NOT hash-verified
        rows.append({"path": key, "status": status,
                     "left":  {"len": l.length, "clen": l.compressed_length} if l else None,
                     "right": {"len": r.length, "clen": r.compressed_length} if r else None})
    summary = {  # count block (D-08)
        "added": ..., "removed": ..., "changed": ..., "identical_by_metadata": ...,
        "left_node_errors": len(vtL.node_errors), "right_node_errors": len(vtR.node_errors),
        "left_rejected": len(vtL.rejected), "right_rejected": len(vtR.rejected),
        "left_tombstoned": len(vtL.tombstoned), "right_tombstoned": len(vtR.tombstoned),
    }
    return {"rows": rows, "summary": summary}
```
The `summary` MUST surface `node_errors`/`rejected`/`tombstoned` counts per the Phase-28 §Code-Context directive ("the API should surface these so the UI can flag malformed/skipped archives").

### Pattern 3: Drill-In + On-Demand Hash (TRE-04 / D-07)
**What:** For one path, return winner metadata + winning archive + shadowed list + a real xxhash of BOTH sides' content; resolve `identical-by-metadata` → `content-confirmed-identical` | `content-changed`.
**Binds to (REAL signatures):** `MergedEntry.winner_node` (a `SearchNode`), `.shadowed` (`tuple[SearchNode, ...]`), `read_tre_payload(tre_path, logical_name) -> bytes` (decompressed). **`scan.toc_tree_path`** is required to resolve TOC-served payloads. **This whole path is VERIFIED working on the real SearchTOC install (§Verified Findings).**
```python
import xxhash
from .parser import read_tre_payload, read_search_toc_entries, read_cot2000_entries
from .parser import detect_master_index_kind, MasterIndexKind
from .virtual_tree import fix_up_file_name

def _payload_tre_for_toc(node, vpath, toc_tree_path) -> tuple[str, str]:
    """Resolve (payload_tre_abspath, logical_name) for a file served via a .toc master index."""
    # match the entry by canonical path; both SearchTocEntry and Cot2000Entry carry .tre_file
    kind = detect_master_index_kind(node.abspath)
    entries = (read_search_toc_entries if kind == MasterIndexKind.SEARCH_TOC
               else read_cot2000_entries)(node.abspath)
    for e in entries:
        if fix_up_file_name(e.path) == vpath:   # canonicalize to compare
            payload = os.path.join(toc_tree_path or "", e.tre_file)  # VERIFIED: bare name + TOCTreePath
            return payload, e.path
    raise KeyError(vpath)

def hash_virtual_file(node, vpath, toc_tree_path) -> str | None:
    if node.kind == "tree":
        data = read_tre_payload(node.abspath, vpath)         # decompressed bytes
    elif node.kind == "toc":
        tre_path, logical = _payload_tre_for_toc(node, vpath, toc_tree_path)
        data = read_tre_payload(tre_path, logical)
    elif node.kind == "path":
        with open(os.path.join(node.abspath, vpath), "rb") as fh: data = fh.read()
    else:
        return None
    return xxhash.xxh3_64_hexdigest(data)   # decompressed content hash (D-07)
```
**Decompressed vs stored-compressed (Claude's discretion → decided):** hash **decompressed** bytes. Two entries can share `(length, compressed_length)` yet differ in content, AND compressed streams can differ for identical content (different compressor settings). Hashing decompressed bytes is the *only* hash that correctly resolves false-identicals (D-06's stated purpose) and is robust across the retail-zlib vs Restoration-obfuscated divergence (`read_tre_payload` already normalizes both via its `tre_decrypt` fallback). VERIFIED: decompressed length == `entry.length` on the real install.

**Performance note (drill-in cost):** `_payload_tre_for_toc` calls `read_search_toc_entries`/`read_cot2000_entries` which parse the ENTIRE master index (193k entries) to find one path. That is acceptable for a single on-demand drill-in, but the cache layer (D-10 `archive_entry` table already stores `tre_file` per vpath) should serve the `(payload_tre, logical)` lookup from sqlite on the hot path instead of re-parsing the whole `.toc`. Recommend: drill-in consults the cache's `archive_entry` row for `(winner.abspath, mtime, size, vpath)` → reads `tre_file` directly; falls back to a full parse only on a cache miss.

### Anti-Patterns to Avoid
- **Using TOC `crc` for change detection:** it is a path-CRC, not content. LOCKED out by D-06 and asserted by Phase-28 `build_virtual_tree` docstring. `TreEntry.crc`/`SearchTocEntry.crc`/`Cot2000Entry.crc` exist but MUST NOT feed the diff.
- **Eager full-archive hashing:** SC#3 forbids it. Hash only on drill-in, one vpath at a time.
- **Importing FastAPI/sqlite into `diff.py`:** keep the engine pure so Phase 30 + TREM-01..03 can import it headless. API and cache wrap it.
- **Mutating Phase-28 modules:** D-01 carried-forward + CONTEXT "no refactor of Phase-28 code should be needed." Add new modules only.
- **Server-side pagination/filter:** D-08 forbids; return the full array.
- **Binding uvicorn to 0.0.0.0:** localhost single-user tool — bind `127.0.0.1` (security posture, Claude's discretion → decided).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| TRE/TOC parsing, decompression, version detection | Custom byte reader | Phase-28 `tre_compare.parser` (`read_tre_*`, `read_tre_payload`) | Already version-aware (0004/0005/6000/0006/5000 + COT2000 + SearchTOC) + zlib + decrypt fallback + bounds-checked |
| Search-path precedence / merge / tombstones | Re-implement engine semantics | Phase-28 `build_virtual_tree` | Engine-faithful, unit-tested; `MergedEntry` already has everything drill-in needs |
| cfg `[SharedFile]` parsing | `configparser` | Phase-28 `parse_shared_file` | Repeated-key grammar breaks configparser; hand parser already correct |
| Content hash | hashlib loop / custom | `xxhash.xxh3_64_hexdigest` | C-extension, GB/s, D-11 locked |
| HTTP routing + JSON validation + OpenAPI | `http.server` | FastAPI + Pydantic | Validation, typed models, auto docs, TestClient |
| Caching parsed entries | pickle blobs / ad-hoc files | stdlib `sqlite3` | Atomic, queryable, `(abspath,mtime,size)` keying, concurrent-read safe |

**Key insight:** Phase 29 is almost entirely *composition* of Phase-28 primitives + two well-known libraries. The only genuinely new logic is (a) the tri-state classification rules and (b) the TOC-payload resolution for the drill-in hash (verified working). Everything else is wiring.

## sqlite Cache Design (D-10)

**Schema (granular, reusable across ANY pairing — NOT per-pair):**
```sql
-- per-archive parsed TOC entries, keyed by archive identity (abspath,mtime,size)
CREATE TABLE IF NOT EXISTS archive_meta (
    abspath TEXT NOT NULL,
    mtime   REAL NOT NULL,        -- os.stat().st_mtime
    size    INTEGER NOT NULL,     -- os.stat().st_size
    kind    TEXT NOT NULL,        -- 'tree' | 'toc'
    version TEXT,                 -- TREE tag or master-index kind (for set-level version delta)
    entry_count INTEGER,
    PRIMARY KEY (abspath, mtime, size)
);
CREATE TABLE IF NOT EXISTS archive_entry (
    abspath TEXT NOT NULL, mtime REAL NOT NULL, size INTEGER NOT NULL,
    vpath   TEXT NOT NULL,        -- canonical path (safe_virtual_key)
    length  INTEGER NOT NULL,
    compressed_length INTEGER NOT NULL,
    offset  INTEGER NOT NULL,
    tre_file TEXT,                -- for toc entries: payload .tre name (powers drill-in hash w/o re-parse)
    PRIMARY KEY (abspath, mtime, size, vpath)
);
-- memoized drill-in content hashes (D-10), keyed (abspath,mtime,size,vpath)
CREATE TABLE IF NOT EXISTS file_hash (
    abspath TEXT NOT NULL, mtime REAL NOT NULL, size INTEGER NOT NULL,
    vpath   TEXT NOT NULL,
    algo    TEXT NOT NULL DEFAULT 'xxh3_64',
    hexdigest TEXT NOT NULL,
    PRIMARY KEY (abspath, mtime, size, vpath, algo)
);
```
**Invalidation:** key includes `(mtime, size)` — a rewritten archive yields a different key, so a stale row is simply never matched (no explicit delete needed; optionally prune rows whose `(abspath)` has a newer `(mtime,size)` to bound growth). Cheap correctness.

**Scale note (from §Verified Findings):** one real install side is ~200k indexed entries; `archive_entry` for a SWGSource+whitengold pair could hold several hundred thousand rows. sqlite handles this trivially; keep the PK index (it is the lookup key) and consider an index on `(abspath,mtime,size)` alone for the per-archive bulk read used by the cached merge.

**Connection lifecycle under FastAPI:** sqlite + the `check_same_thread` default is the trap. Recommended posture for a localhost single-user tool:
- Open ONE connection at app startup with `sqlite3.connect(db_path, check_same_thread=False)` and `PRAGMA journal_mode=WAL` (better concurrent read/write) + `PRAGMA synchronous=NORMAL`. `[CITED: docs.python.org/3/library/sqlite3.html]`
- Guard writes with a `threading.Lock` (FastAPI runs sync `def` routes in a threadpool; WAL allows concurrent readers + one writer). For this single-user tool, a process-wide write lock is simplest and correct.
- Cache file location: `tools/tre-compare/.cache/tre_compare.sqlite` (gitignored — add `.cache/` to `.gitignore`).
- Whether to rebuild the virtual tree in-process per request or re-run from cache: **recommend re-run-from-cache** — `build_virtual_tree` consumes a `ScanResult`; feed it cached `archive_entry` rows by wrapping them so `iter_node_entries` is bypassed for cache hits. (Alternative: an in-process LRU keyed by cfg-mtime; simpler but loses cross-process reuse. Cache-backed is the D-10 intent.)

**Cache-aware merge wrinkle:** `build_virtual_tree` calls `iter_node_entries(node, ...)` directly (reads the file). To serve from cache without modifying Phase-28 code, the cache layer should provide a parallel `build_virtual_tree_cached(scan, cache)` that reuses Phase-28's *merge logic* but sources entries from the DB. Two options: (a) duplicate the ~40-line merge loop in `cache.py` (small, keeps Phase 28 frozen), or (b) Phase 28 could expose an injectable entry-source — but that is a refactor CONTEXT says is unnecessary. **Recommend (a):** a thin cached merge that imports `safe_virtual_key` + the tombstone rules from `virtual_tree` and iterates cached rows. Flag for planner: this is the one place Phase-29 *re-expresses* Phase-28 logic; keep it tested against the Phase-28 builder for parity.

## FastAPI Surface (D-01/D-02/D-03/D-08/D-09)

**Endpoints (stateless POST-a-pair):**
```python
# tre_compare/api.py
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel

app = FastAPI(title="tre-compare", version="0.2.0")

class PairRequest(BaseModel):
    left_cfg: str
    right_cfg: str

class FileDetailRequest(PairRequest):
    path: str

@app.post("/compare/set")
def compare_set(req: PairRequest): ...     # → {"rows": [...], "summary": {...}}

@app.post("/compare/files")
def compare_files(req: PairRequest): ...   # → {"rows": [...lean...], "summary": {...}}  (D-08/D-09)

@app.post("/file/detail")
def file_detail(req: FileDetailRequest): ...  # → winner, shadowed[], left/right hash, verdict

@app.get("/installs")
def installs(): ...   # config-driven (D-02)
```
- **Lean rows (D-09):** `{path, status, left:{len,clen}, right:{len,clen}}`. No hashes in bulk.
- **Full payload (D-08):** return the entire `rows` array; no pagination params. ~200k entries/side → the merged array is large but a few MB JSON — fine on localhost. Consider `ORJSONResponse` (orjson) ONLY if serialization proves slow (extra dep — defer; stdlib JSON is fine for SC#4).
- **Error envelope (Claude's discretion → recommend):** validate cfg paths exist + are readable; on failure raise `HTTPException(400, {"error": "...", "cfg": "..."})`. Surface per-archive `node_errors` in the `summary`, NOT as a hard error (a malformed archive shouldn't 500 the whole diff — mirrors Phase-28 fail-isolation).
- **Binding:** `uvicorn.run(app, host="127.0.0.1", port=8765)` in `__main__.py` — localhost-only.
- **`GET /installs` config:** read a tool-local `installs.toml`/`installs.json` (e.g. `tools/tre-compare/installs.toml`) listing `{name, cfg_path}` entries; stdlib `tomllib` (3.11+, already the floor) parses it. NOT a registry — just a static picker feed.

**Keep importable for Phase 30:** export `app` from `tre_compare.api` and the diff functions from `tre_compare.diff`; Phase 30's single localhost server can `from tre_compare.api import app` and add a static-file mount with zero backend refactor.

## Dependency Story / Extractability (D-11/D-12)

- Add to `pyproject.toml` `[project].dependencies`: `fastapi`, `uvicorn` (recommend bare, not `[standard]`, to minimize binary-wheel surface on fresh clone — see Pitfall 5), `xxhash`. Add `httpx` to `[dependency-groups].dev`.
- **Version-pin strategy (consistent with Phase-28 conventions):** Phase 28 used **unpinned `dependencies = []`** and relied on `uv.lock` for reproducibility; the only `>=` floors are dev (`pytest>=9.1.0`) and build (`uv_build>=0.11.7,<0.12.0`). **Mirror this:** list deps WITHOUT upper pins in `pyproject.toml` (e.g. `"fastapi>=0.137"`, `"xxhash>=3.7"`), let `uv.lock` freeze exact versions. This matches the Phase-28 "`uv.lock` is the reproducibility contract, pyproject lists floors" pattern and the no-forward-pin `[build-system]` choice. `requires-python>=3.11` floor is preserved (all three deps support 3.11: fastapi needs 3.10+, uvicorn 3.10+, xxhash 3.8+). `[VERIFIED: pypi requires-python fields]`
- **Extractability preserved:** all three are pure-PyPI wheels (xxhash ships manylinux/win wheels — no compiler needed on a fresh clone). No engine imports introduced. `uv sync` on a copied directory resolves from `uv.lock`. `.cache/` sqlite is gitignored local state, not committed.

## Common Pitfalls

### Pitfall 1: TOC-served winner has no decompressible bytes at its `abspath`
**What goes wrong:** Calling `read_tre_payload(node.abspath, vpath)` when `node.kind == "toc"` fails — the `.toc` master index holds metadata, not payload bytes.
**Why:** Content lives in payload `.tre`s referenced by `entry.tre_file`, resolved via `TOCTreePath`.
**How to avoid:** Resolve `os.path.join(scan.toc_tree_path, entry.tre_file)` → `read_tre_payload(that_tre, entry.path)`. Thread `scan.toc_tree_path` into the drill-in path (Pattern 3). VERIFIED working on the real install.
**Warning signs:** `TreFormatError("Not a TREE archive ... token=...")` or `KeyError` on drill-in of a TOC-served file. The real SWGSource cfg is ALL `searchTOC` → this fires immediately if unhandled.

### Pitfall 2: Payload `.tre` referenced by TOC may itself be missing/obfuscated
**What goes wrong:** Restoration extended tags (6000/0006/5000) "may remain obfuscated at rest" (tre_reader docstring). `read_tre_payload` has a `tre_decrypt` fallback but can still `TreFormatError`.
**How to avoid:** Drill-in hash must degrade gracefully — return `hash: null, hash_error: "..."` rather than 500. The verdict for an un-hashable side stays `identical-by-metadata` (honest).
**Warning signs:** `TreFormatError("zlib decompress failed ...")`.

### Pitfall 3: sqlite `check_same_thread` under FastAPI threadpool
**What goes wrong:** Default `sqlite3.connect` connection is single-thread-affine; FastAPI runs sync routes in a threadpool → `ProgrammingError: SQLite objects created in a thread can only be used in that same thread.`
**How to avoid:** `connect(..., check_same_thread=False)` + WAL + a write `Lock` (see Cache Design).
**Warning signs:** Intermittent `ProgrammingError` under concurrent requests.

### Pitfall 4: Canonical-key mismatch between diff and drill-in
**What goes wrong:** `vt.entries` keys are `safe_virtual_key`-canonicalized, but a raw `entry.path` from a TOC re-read in drill-in is NOT. Matching `entry.path == vpath` fails.
**How to avoid:** Canonicalize with `fix_up_file_name`/`safe_virtual_key` on BOTH sides of any comparison (Pattern 3 uses `fix_up_file_name(e.path) == vpath`). Note `safe_virtual_key` can return `None` for rejected keys — those never appear in `entries`, so drill-in of a rejected path is a 404.
**Warning signs:** drill-in 404 for a path that IS in `/compare/files`.

### Pitfall 5: `uvicorn[standard]` pulls platform wheels that can complicate fresh-clone extractability
**What goes wrong:** `[standard]` adds `uvloop` (no Windows wheel — silently skipped) + `httptools`/`websockets`. Not fatal, but extra binary surface contrary to the lean-extractability constraint.
**How to avoid:** Use bare `uvicorn`. The tool is single-user localhost; the perf extras are unnecessary.
**Warning signs:** `uv sync` on a fresh clone pulling more than expected; uvloop build chatter on Linux.

### Pitfall 6: `path` (searchPath/loose) nodes in set-level diff
**What goes wrong:** Loose-dir override nodes have no single-archive identity; basename-matching them as "archives" is meaningless.
**How to avoid:** Set-level (D-04) iterates only `tree`/`toc` nodes. Loose files still participate in the **file-level** diff via `build_virtual_tree`. The real cfg has exactly one `searchPath` (the priority-10 override) — set-level should skip it; file-level includes its files.

### Pitfall 7: Re-parsing a 193k-entry `.toc` on every drill-in
**What goes wrong:** `_payload_tre_for_toc` parses the whole master index to resolve one path; doing this per drill-in is wasteful at real scale.
**How to avoid:** Serve `tre_file` for `(winner.abspath, vpath)` from the cache's `archive_entry` table (it stores `tre_file`); full-parse only on cache miss.
**Warning signs:** Slow drill-in latency on the real SWGSource side.

## Code Examples

### Testing FastAPI routes (TestClient)
```python
# Source: fastapi.tiangolo.com/tutorial/testing
from fastapi.testclient import TestClient
from tre_compare.api import app

def test_compare_files_route(synthetic_install_pair):
    client = TestClient(app)
    r = client.post("/compare/files", json={
        "left_cfg": str(synthetic_install_pair["left_cfg"]),
        "right_cfg": str(synthetic_install_pair["right_cfg"]),
    })
    assert r.status_code == 200
    body = r.json()
    assert "rows" in body and "summary" in body
    assert {row["status"] for row in body["rows"]} <= {
        "added","removed","changed","identical-by-metadata"}
```

### Deterministic cache test
```python
def test_cache_hit_and_invalidation(tmp_path, fixtures):
    db = tmp_path / "c.sqlite"
    tre = fixtures.build_tre(tmp_path / "a.tre", [("foo/x.dds", 5, 5)])
    cache = Cache(db)
    e1 = cache.archive_entries(str(tre))          # MISS → parses
    e2 = cache.archive_entries(str(tre))          # HIT  → same rows, no re-parse
    assert e1 == e2
    fixtures.build_tre(tre, [("foo/x.dds", 9, 9)])  # rewrite → new (mtime,size)
    e3 = cache.archive_entries(str(tre))          # MISS again (invalidated by key)
    assert e3 != e1
```

### Drill-in hash upgrade
```python
def test_drill_in_resolves_false_identical(fixtures, tmp_path):
    # two installs, same (length,clen) but different bytes → identical-by-metadata in bulk,
    # content-changed after drill-in hash
    ...
    detail = client.post("/file/detail", json={..., "path": "foo/x.dds"}).json()
    assert detail["left_hash"] != detail["right_hash"]
    assert detail["verdict"] == "content-changed"
```

## State of the Art

| Old Approach | Current Approach | When | Impact |
|--------------|------------------|------|--------|
| `requests`/`flask` sync API | FastAPI + Pydantic v2 + ASGI | mainstream since ~2023 | Typed models, auto-validation, OpenAPI for free |
| `hashlib.md5` for content fingerprint | `xxhash.xxh3_64` | xxh3 stable since xxhash 1.x C lib | ~10x faster non-crypto hashing; fine for non-adversarial diff |
| `startlette.testclient` requiring `requests` | `TestClient` on `httpx` | starlette 0.21+ | `httpx` is the test dep, not `requests` |

**Deprecated/outdated:** Pydantic v1 `.dict()`/`Config` patterns — use v2 `model_dump()`/`model_config`. FastAPI 0.137 ships Pydantic v2.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | pytest 9.1.0 (Phase-28 dev group) + `fastapi.testclient.TestClient` (needs `httpx` dev dep) |
| Config file | `tools/tre-compare/pyproject.toml` `[tool.pytest.ini_options]` (markers, testpaths=["tests"]) |
| Quick run command | `uv run pytest -m "not integration" -q` |
| Full suite command | `uv run pytest -q` (then env-gated: `$env:TRE_COMPARE_INTEGRATION=1; uv run pytest -m integration -q`) |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| TRE-02 | Set diff: added/removed/changed + size/entry/version deltas | unit | `uv run pytest tests/test_diff.py -k set -x` | ❌ Wave 0 |
| TRE-03 | File diff tri-state via (length,clen), never crc | unit | `uv run pytest tests/test_diff.py -k files -x` | ❌ Wave 0 |
| TRE-03 | node_errors/rejected/tombstoned surfaced in summary | unit | `uv run pytest tests/test_diff.py -k summary -x` | ❌ Wave 0 |
| TRE-04 | Drill-in metadata + winner + shadowed | unit | `uv run pytest tests/test_diff.py -k drill -x` | ❌ Wave 0 |
| TRE-04 | On-demand xxhash resolves false-identical (decompressed bytes) | unit | `uv run pytest tests/test_diff.py -k hash -x` | ❌ Wave 0 |
| TRE-04 | TOC-served winner hash resolves payload .tre via TOCTreePath | unit | `uv run pytest tests/test_diff.py -k toc_payload -x` | ❌ Wave 0 |
| D-10 | sqlite cache hit/miss/invalidation deterministic | unit | `uv run pytest tests/test_cache.py -x` | ❌ Wave 0 |
| D-01..D-03 | API routes return correct shapes (TestClient) | unit | `uv run pytest tests/test_api.py -x` | ❌ Wave 0 |
| SC#4 | Real SWGSource-vs-whitengold compare returns correct JSON | integration | `$env:TRE_COMPARE_INTEGRATION=1; uv run pytest -m integration -k pair -q` | ⚠️ extend existing test_integration.py |

### Sampling Rate
- **Per task commit:** `uv run pytest -m "not integration" -q` (machine-independent, < a few seconds)
- **Per wave merge:** full `uv run pytest -q`
- **Phase gate:** full suite green + ONE manual env-gated integration run against the real pair (SC#4) before `/gsd-verify-work`.

### Wave 0 Gaps
- [ ] `tests/test_diff.py` — covers TRE-02/03/04 (set, file tri-state, drill-in, hash, toc_payload)
- [ ] `tests/test_cache.py` — covers D-10 (hit/miss/invalidation)
- [ ] `tests/test_api.py` — covers D-01..D-03/D-08/D-09 via TestClient
- [ ] `tests/_fixtures.py` extension — `build_install_pair()` helper: two synthetic cfgs+archives differing in set + file dimensions (incl. a same-(len,clen)-different-bytes pair for the false-identical hash test, and a TOC-served file for the TOCTreePath drill-in test)
- [ ] `tests/conftest.py` extension — `api_client` (TestClient) + `tmp_cache` fixtures
- [ ] `tests/test_integration.py` — add a gated TWO-install compare (SWGSource cfg + a second real install cfg) for SC#4; skip cleanly when either install absent
- [ ] Dev dep: `uv add --dev httpx` (TestClient requires it)

### How to test the sqlite cache deterministically
Use `tmp_path` for the DB file (no shared state); drive MISS→HIT→invalidate by rewriting a synthetic archive (changes `(mtime,size)` → key miss). Assert identical rows on HIT, different rows after rewrite. No real install needed.

## Security Domain

> `security_enforcement` not found disabled in config; included. This is a localhost single-user diagnostic tool reading local files — threat surface is narrow but non-zero (it opens arbitrary cfg-referenced paths + decompresses untrusted archives).

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | localhost single-user; no auth surface |
| V3 Session Management | no | stateless, no sessions (D-01) |
| V4 Access Control | partial | bind `127.0.0.1` only; no remote exposure |
| V5 Input Validation | yes | Pydantic models validate request bodies; cfg paths validated exist+readable before open; `safe_virtual_key` already rejects `..`/absolute/UNC traversal keys (Phase-28 hardening carried forward) |
| V6 Cryptography | no | xxhash is non-crypto by design (content fingerprint, not security) — correct choice, NOT a hand-rolled crypto failure |
| V12 File/Resource | yes | decompression bounds: Phase-28 `_preflight_tre`/`_preflight_toc` + `MAX_ENTRIES` already guard zip-bomb/OOB; drill-in hash reads one entry's payload (bounded by `compressed_length`/`length`) |

### Known Threat Patterns
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Path traversal via crafted archive entry / cfg path | Tampering | `safe_virtual_key` (Phase-28) rejects `..`/drive/UNC; validate cfg paths server-side; never join unsanitized vpath to filesystem for the loose `path` case without `safe_virtual_key` |
| Zip-bomb / OOB read on hostile archive | DoS | Phase-28 preflights + `MAX_ENTRIES` + per-node fail-isolation (`node_errors`); drill-in degrades to `hash_error` not 500 |
| Arbitrary file open via attacker-supplied cfg | Info disclosure | localhost-only bind; this is a single-user local tool — acceptable residual; do NOT expose remotely |
| Decompress untrusted bytes (`zlib.decompress`) | DoS | bounded by header sizes (preflight); catch `zlib.error` per-node (already done) |

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | ~~TOC-served winners resolve payload via `TOCTreePath + entry.tre_file`~~ — **NOW VERIFIED** against the real SearchTOC install (§Verified Findings); no longer an assumption | Pattern 3 | RESOLVED — bare-name join confirmed; decompressed length == entry.length |
| A2 | Set-level uses only `tree`/`toc` nodes (skip `path`); `toc` "version" delta = master-index kind string | Pattern 1 | If user expects a per-loose-dir set row, table differs cosmetically. LOW — D-04 says "archives". |
| A3 | Bare `uvicorn` (not `[standard]`) is sufficient | Stack / Pitfall 5 | If WebSocket/HTTP-2 ever needed (Phase 30), add `[standard]`. LOW — Phase 30 is static SPA over REST. |
| A4 | Re-expressing the merge loop in `cache.py` (option a) stays parity with Phase-28 `build_virtual_tree` | Cache Design | Drift between cached and uncached merge → wrong diffs. MEDIUM — mitigate with a parity test (cached vs builder on the same synthetic install). |
| A5 | ~200k-row/side lean payload serializes fast enough with stdlib JSON | FastAPI surface / D-08 | If slow, add orjson. LOW — few MB on localhost; verified entry scale is ~193k/side. |

## Open Questions

1. **Second real install cfg for the SC#4 pair.**
   - What we know: the existing integration test points only at `stage/client.cfg` (the SWGSource/whitengold composite). SC#4 wants a *pair* (SWGSource vs whitengold as two distinct installs).
   - What's unclear: the path to the second install's cfg.
   - Recommendation: parameterize the integration test with two env-var cfg paths (`TRE_COMPARE_LEFT_CFG`, `TRE_COMPARE_RIGHT_CFG`), skip cleanly when unset — keeps the default run machine-independent (matches Phase-28 gating discipline). Kenny supplies the two real cfg paths at verify time. (This is the only remaining open item; the TOC-payload binding that was Open-Q1 in the draft is now RESOLVED — see §Verified Findings.)

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| uv | venv/lock/run (Phase-28 D-05) | ✓ (Phase 28 used it; probe ran via `uv run`) | — | — |
| Python 3.11+ | requires-python floor | ✓ (.python-version=3.11) | 3.11 | — |
| fastapi/uvicorn/xxhash wheels | new deps | ✓ (PyPI manylinux/win wheels) | 0.137.0 / 0.49.0 / 3.7.0 | hashlib (xxhash) — but D-11 locks xxhash |
| Real SWGSource install | SC#4 integration | ✓ on this machine (probe confirmed `sku0_client.toc` + payload `.tre`s present) | `D:/Code/SWGSource Client v3.0/` | env-gated skip |
| Second real install cfg (whitengold) | SC#4 pair | ⚠️ path TBD | — | env-gated skip; Open-Q1 |

**Missing dependencies with no fallback:** none (all default tests are synthetic/machine-independent).
**Missing with fallback:** the SC#4 real *pair* needs two cfg paths Kenny supplies at verify time; absent → integration test skips cleanly.

## Sources

### Primary (HIGH confidence)
- Phase-28 source read verbatim this session: `tools/tre-compare/src/tre_compare/{virtual_tree.py, scanner.py, parser/__init__.py, parser/tre_reader.py}`, `pyproject.toml`, `README.md`, `.python-version`, `.gitignore`, `tests/{_fixtures.py, conftest.py, test_integration.py}` — exact public signatures.
- **Live probe of the real install** (`uv run python` against `D:/Code/SWGSource Client v3.0/sku0_client.toc`): SearchTOC kind, 193,475 entries, bare `tre_file` join to `TOCTreePath` exists, `read_tre_payload` decompressed length == `entry.length`. See §Verified Findings.
- `stage/client.cfg` `[SharedFile]` section — real install shape (TOCTreePath="D:/Code/SWGSource Client v3.0/", all searchTOC + searchTree + one searchPath override; NO searchAbsolute/searchCache → approximation boundary NOT exercised).
- PyPI JSON (`pypi.org/pypi/{fastapi,uvicorn,xxhash,pydantic}/json`) — verified latest versions + requires-python, 2026-06-14.

### Secondary (MEDIUM confidence)
- FastAPI docs (testing, Pydantic models) `[CITED: fastapi.tiangolo.com]` — TestClient/httpx pattern (training knowledge, stable).
- Python sqlite3 docs `[CITED: docs.python.org/3/library/sqlite3.html]` — check_same_thread / WAL (training knowledge, stable).

### Tertiary (LOW confidence)
- None load-bearing; all critical bindings verified against Phase-28 source or a live probe.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — versions PyPI-verified; deps map directly to locked decisions.
- Architecture/diff bindings: HIGH — every field referenced exists in Phase-28 dataclasses read this session.
- TOC-payload drill-in: HIGH — VERIFIED end-to-end against the real SearchTOC install (was MEDIUM in draft; the live probe resolved it: bare `tre_file` + `TOCTreePath` join exists, `read_tre_payload` decompressed length == `entry.length`).
- Cache/API/pitfalls: HIGH — standard stdlib sqlite + FastAPI patterns; pitfalls drawn from real Phase-28 constraints + verified scale (~193k entries/side).

**Research date:** 2026-06-14
**Valid until:** ~2026-07-14 (stable libraries; re-verify dep versions if planning slips past 30 days)

## RESEARCH COMPLETE
