---
phase: 29-tre-compare-tool-diff-engine-api
plan: 01
subsystem: tre-compare-tool
tags: [diff-engine, tre, toc, virtual-tree, xxhash, fastapi-deps, pure-function]
requires:
  - "tre_compare.scanner.parse_shared_file + ScanResult/SearchNode (Phase 28-03)"
  - "tre_compare.virtual_tree.build_virtual_tree + VirtualTree/MergedEntry + _NODE_FAULTS + fix_up_file_name/safe_virtual_key (Phase 28-03)"
  - "tre_compare.parser.read_tre_payload/read_tre_entries/read_search_toc_entries/read_cot2000_entries + headers (Phase 28-02)"
provides:
  - "tre_compare.diff.diff_archive_set (set-level, (basename,kind)-keyed, fault-tolerant)"
  - "tre_compare.diff.diff_virtual_trees (file tri-state + tombstoned/rejected/error qualifier + summary)"
  - "tre_compare.diff.drill_in (domain ok/not_found/rejected) + hash_virtual_file (never-raise xxhash)"
  - "frozen result dataclasses ArchiveStat / DriveHashResult / DrillResult"
  - "tools/tre-compare/tests/_fixtures.build_install_pair (+ build_tre payloads= arg)"
  - "fastapi/uvicorn(bare)/xxhash runtime + httpx dev floors in pyproject + uv.lock"
affects:
  - "Phase 29-02 (cache.py — imports diff result shapes)"
  - "Phase 29-03 (FastAPI routes — maps drill_in rejected->400/not_found->404)"
  - "Phase 30 (React SPA — consumes diff/summary/drill JSON shapes)"
tech-stack:
  added: [fastapi, uvicorn, xxhash, httpx]
  patterns: ["pure-function diff engine (no web/db imports)", "structured never-raise hash result", "(basename,kind) set keying", "optional qualifier annotation (not a hash)"]
key-files:
  created:
    - tools/tre-compare/src/tre_compare/diff.py
    - tools/tre-compare/tests/test_diff.py
  modified:
    - tools/tre-compare/tests/_fixtures.py
    - tools/tre-compare/pyproject.toml
    - tools/tre-compare/uv.lock
decisions:
  - "posixpath.normpath(fix_up_file_name(raw)) recovers a rejected-key canonical form for the rejected qualifier (interior `..` collapse the engine would resolve)"
  - "build_tre gains a payloads= arg to forge deterministic same-(len,clen)-different-bytes false-identical pairs"
  - "uvicorn kept BARE (no [standard]); uv.lock grep-proven (no uvloop/httptools)"
metrics:
  duration: "~25 min"
  completed: 2026-06-15
  tasks: 3
  files: 5
---

# Phase 29 Plan 01: TRE Diff Engine + FastAPI/xxhash Deps Summary

Pure-function `tre_compare.diff` engine (set-level + file tri-state + qualifier + symmetric
drill-in + never-raise content hash) plus the three Phase-29 runtime deps (FastAPI, bare
uvicorn, xxhash; httpx dev), landed RED→GREEN with 14 new diff tests over synthetic install
pairs. The engine imports NO fastapi / NO sqlite3 so Phase 30 needs zero backend refactor.

## What Was Built

- **Task 1 — deps + re-lock (`4741094f0`):** `fastapi>=0.137`, `uvicorn>=0.49` (BARE), `xxhash>=3.7`
  in `[project].dependencies`; `httpx` in `[dependency-groups].dev`. `uv lock` + `uv sync`
  resolved 24 pkgs. All `>=` floors resolved as-written on today's index — **no bumps needed**.
  `grep -iE 'uvicorn\[standard\]|uvloop|httptools' uv.lock` returns nothing → bare uvicorn proven
  (the `import uvicorn` check alone does NOT prove this, per REVIEWS Codex/Opus).

- **Task 2 — fixtures + RED tests (`af7cd45e5`):** `build_install_pair(tmp_path)` lays out two
  synthetic installs (`left/`, `right/`) each with its own `client.cfg`, engineered to drive every
  set dimension (changed/added/removed/(tree,toc)-collision/corrupt) and file dimension
  (added/removed/changed/identical/false-identical/tombstoned-left/tombstoned-both/rejected-right/
  node-error) plus the drill cases (TOC-served payload, leading-dot/doubled-slash TREE asymmetry,
  KeyError degrade). `build_tre` gained a `payloads=` arg so the false-identical pair has the SAME
  `(length, compressed_length)` but DIFFERENT bytes deterministically. 14 tests RED on
  `ModuleNotFoundError: tre_compare.diff` (bind the real API, not stubs).

- **Task 3 — pure `diff.py` (`a9eaced54`):** `diff_archive_set` ((basename,kind) keyed, fault rows),
  `diff_virtual_trees` (lean `(len,clen)` tri-state rows + optional qualifier list + summary),
  `drill_in` (domain `ok`/`not_found`/`rejected`), `hash_virtual_file` (xxh3_64 of decompressed
  bytes, symmetric TREE/TOC resolve, `_HASH_FAULTS=(*_NODE_FAULTS,KeyError)` never-raise), plus
  frozen `ArchiveStat`/`DriveHashResult`/`DrillResult`. All 14 GREEN; full suite **52 passed, 1
  integration deselected**.

## Requirements Satisfied

- **TRE-02** — `diff_archive_set` produces added/removed/changed/fault/identical rows with
  size + entry-count + version deltas, keyed by `(basename, kind)` so a tree↔toc basename
  collision surfaces as two distinct rows; a corrupt archive degrades to a `fault` row via
  `stat_archive`'s `_NODE_FAULTS` wrapper, never aborting the set diff.
- **TRE-03** — `diff_virtual_trees` produces tri-state lean rows from `(length, compressed_length)`
  (never crc); an optional `qualifier` distinguishes one-sided tombstone/rejection/parse-failure
  from a bare added/removed; tombstoned-both-sides never vanishes; summary surfaces per-side
  node_errors/rejected/tombstoned + status_counts.
- **TRE-04** — `drill_in` + `hash_virtual_file` return winner + shadowed + a structured
  `DriveHashResult` (decompressed-byte xxhash) that resolves a false-identical, including a
  TOC-served winner (resolved via `toc_tree_path`) AND a leading-dot/doubled-slash TREE winner
  (symmetric raw-path read), degrading (never raising) on a KeyError/decompress fault, with a
  domain not_found/rejected status.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] rejected-qualifier key recovery needed posixpath.normpath**

- **Found during:** Task 3 (test_qualifier_rejected RED→GREEN).
- **Issue:** The plan's `_rejected_keys` recovery via `fix_up_file_name(raw)` alone could not
  match the canonical key. `fix_up_file_name` (verbatim engine port) strips only LEADING `..`;
  the right-side raw `bad/../bad/rej.dds` retained its interior `..`, so it never matched the
  left's canonical `bad/rej.dds` and the `rejected_right` qualifier never attached.
- **Fix:** `_rejected_keys` now runs `posixpath.normpath(fix_up_file_name(raw))` to collapse the
  interior `..` the engine would resolve, dropping empty/escaping results. This is the
  consult-flagged "the engine WOULD resolve this" recovery; it only affects the rejected
  qualifier's row-attachment, not the merge or the bulk status.
- **Files modified:** `tools/tre-compare/src/tre_compare/diff.py`
- **Commit:** `a9eaced54`

**2. [Rule 2 - Missing critical functionality] build_tre payloads= for deterministic hash bytes**

- **Found during:** Task 2.
- **Issue:** `build_tre` wrote only header+TOC+name block — no payload region — so a drill-in
  hash of two same-`(len,clen)` files would read undefined header bytes, making the false-identical
  hash assertion fragile/non-deterministic.
- **Fix:** added an optional `payloads={name: bytes}` arg that appends real payload blobs after the
  name block and patches the per-entry offset, so `read_tre_payload` returns exact controllable
  bytes. Backward-compatible (default `None`).
- **Files modified:** `tools/tre-compare/tests/_fixtures.py`
- **Commit:** `af7cd45e5`

## Threat Mitigations Applied

- **T-29-01-01** (vpath→filesystem): `drill_in` canonicalizes via `safe_virtual_key`; a `None`
  result returns `DrillResult(status="rejected")` before any join. The loose-`path` branch joins
  only the already-vetted key.
- **T-29-01-02** (decompress fault + bare KeyError): `hash_virtual_file` wraps the whole
  resolve+read in `_HASH_FAULTS = (*_NODE_FAULTS, KeyError)` and degrades to a `DriveHashResult`
  error — verified by `test_keyerror_degrade` (monkeypatched name miss → `hexdigest=None` + error,
  no raise).
- **T-29-01-03** (TOC payload .tre resolution): `_payload_tre_for_toc` joins only `toc_tree_path`
  (cfg-owned) + the master-index's own `entry.tre_file`; no caller path component enters the join.
  The TREE path reads by the archive's own raw `e.path`.

## Floor-Resolution Note (REVIEWS LOW)

All three runtime floors (`fastapi>=0.137`, `uvicorn>=0.49`, `xxhash>=3.7`) and the `httpx` dev
dep resolved cleanly via `uv lock` on 2026-06-15 with no version bumps. Frozen versions in
`uv.lock`: fastapi 0.137.0, uvicorn 0.49.0, xxhash 3.7.0, httpx 0.28.1 (+ pydantic 2.13.4,
starlette 1.3.1, anyio 4.13.0 transitive).

## Verification

- `uv run pytest -m "not integration" -q` → **52 passed, 1 deselected** (whole synthetic suite
  incl. new diff tests + all Phase-28 tests still green).
- `grep -v '^#' diff.py | grep -E 'import fastapi|import sqlite3|sqlite3\.'` → nothing (pure-engine).
- `grep -E '_HASH_FAULTS|KeyError' diff.py` → present (Opus never-raise fix).
- `grep -E '\.tombstoned|\.rejected|\.node_errors' diff.py` → all three (P1 qualifier).
- `grep -iE 'uvicorn\[standard\]|uvloop|httptools' uv.lock` → nothing (bare uvicorn).
- `uv run python -c "import fastapi, uvicorn, xxhash; print('ok')"` → `ok`.
- No `.crc` used for any change signal anywhere in `diff.py` or `test_diff.py`.

## TDD Gate Compliance

RED gate `test(29-01)` `af7cd45e5` → GREEN gate `feat(29-01)` `a9eaced54`. No REFACTOR commit
needed. Tasks 2/3 followed the per-task RED/GREEN cycle; Task 1 is a non-TDD dep/lock task.

## Known Stubs

None. The engine is fully wired against the real Phase-28 scanner/virtual-tree/parser APIs and
exercised end-to-end (`parse_shared_file → build_virtual_tree → diff/drill`) over genuine on-disk
synthetic archives. No placeholder data, no hardcoded empty returns.

## Self-Check: PASSED

- FOUND: tools/tre-compare/src/tre_compare/diff.py
- FOUND: tools/tre-compare/tests/test_diff.py
- FOUND: tools/tre-compare/tests/_fixtures.py (build_install_pair @ line 318)
- FOUND: tools/tre-compare/pyproject.toml
- FOUND: tools/tre-compare/uv.lock
- FOUND commit: 4741094f0 (chore deps)
- FOUND commit: af7cd45e5 (test RED)
- FOUND commit: a9eaced54 (feat GREEN)
