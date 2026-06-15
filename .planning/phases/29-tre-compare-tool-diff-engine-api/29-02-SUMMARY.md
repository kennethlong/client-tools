---
phase: 29-tre-compare-tool-diff-engine-api
plan: 02
subsystem: tre-compare-tool
tags: [cache, sqlite, virtual-tree, parity-merge, concurrency, mtime_ns, drill-in]
requires:
  - "tre_compare.virtual_tree.build_virtual_tree + iter_node_entries + safe_virtual_key + fix_up_file_name + _NODE_FAULTS + MergedEntry/VirtualTree (Phase 28-03)"
  - "tre_compare.scanner.ScanResult/SearchNode (Phase 28-03)"
  - "tre_compare.parser.read_tre_header/read_search_toc_header/read_cot2000_header/read_search_toc_entries/read_cot2000_entries/detect_master_index_kind (Phase 28-02)"
  - "tools/tre-compare/tests/_fixtures (build_tre payloads=, build_search_toc, write_synthetic_cfg) + conftest synthetic_install (Phase 28-04 / 29-01)"
provides:
  - "tre_compare.cache.Cache (sqlite3): archive_entries(node, node_errors, no_cache) parse-skip cache keyed (abspath, mtime_ns, size)"
  - "tre_compare.cache.Cache.get_file_hash/put_file_hash drill-in hash memo keyed (abspath, mtime_ns, size, vpath, algo)"
  - "tre_compare.cache.build_virtual_tree_cached — parity-equivalent merge sourcing rows from sqlite, kind threaded from the live node"
  - "archive_meta / archive_entry / file_hash sqlite schema (mtime_ns INTEGER); tre_file stored per toc row (Pitfall 7)"
affects:
  - "Phase 29-03 (FastAPI routes — share one Cache across the threadpool; summary/rows served from cache; drill-in memoizes hashes)"
tech-stack:
  added: []
  patterns: ["stdlib sqlite3 identity cache keyed (abspath, mtime_ns INTEGER, size)", "parse-skip proven by spy (call-count), not row-equality", "re-expressed parity merge sourcing rows from sqlite, kind from live node", "INSERT OR IGNORE + under-lock MISS re-check (concurrent-MISS-safe)", "__file__-relative default db path"]
key-files:
  created:
    - tools/tre-compare/src/tre_compare/cache.py
    - tools/tre-compare/tests/test_cache.py
  modified:
    - tools/tre-compare/.gitignore
decisions:
  - "mtime_ns INTEGER st_mtime_ns, NOT float st_mtime REAL — closes the FAT32/network 1-2s resolution + float-rounding false-HIT window (P4)"
  - "the HIT is PROVEN by a parse-skip spy on iter_node_entries (call-count==1 across two calls), not mere row-equality (P5)"
  - "archive_entries takes a SearchNode (not a bare abspath) so node.kind is threaded into the cached merge — a row carries no kind (P2)"
  - "MISS detect-then-INSERT is re-checked under a process-wide write Lock + INSERT OR IGNORE so two concurrent MISSes cannot race a PRIMARY KEY 500 (Sonnet)"
  - "default db path resolved via __file__ (tools/tre-compare/.cache/), never cwd, so a wrong-dir launch never silently empties the cache (P-divergent Sonnet)"
metrics:
  duration: "~20 min"
  completed: 2026-06-15
  tasks: 2
  files: 3
---

# Phase 29 Plan 02: TRE Cache Layer (sqlite + parity merge) Summary

Stdlib-sqlite3 cache (`tre_compare.cache`) landed RED→GREEN: a per-archive parse-skip
cache keyed `(abspath, mtime_ns, size)` (INTEGER `st_mtime_ns`, P4), a drill-in hash
memo, and a `build_virtual_tree_cached` that re-expresses the Phase-28 merge from cached
rows and is PARITY-EQUIVALENT to `build_virtual_tree` across an EXPANDED 7-case matrix —
proven by 8 new tests including a parse-skip spy (call-count, not row-equality, P5) and a
concurrent-MISS race. Whole suite 60 passed (+8). REVIEWS named this the highest-risk
plan; the ~40-line merge re-implementation is now retired by the expanded matrix, not a
single happy-path fixture.

## What Was Built

- **Task 1 — RED tests (`c661c6aa3`):** `test_cache.py` (317 lines) — nine behaviors:
  parse-skip-proven `miss_then_hit` / `invalidation` / `no_cache` (spy on
  `tre_compare.cache.iter_node_entries` asserting call-count, P5), `parity_basic` (inline
  `synthetic_install`), `parity_matrix` (a richer install exercising all 7 P2 cases),
  `hash_memo` round-trip, `tre_file_stored`, and `concurrent_miss` (two threads + a barrier,
  assert NO `IntegrityError`). The parity assertion is order-sensitive on the `shadowed`
  TUPLE and the `rejected` LIST (the prior plan asserted only membership; REVIEWS P2 demands
  order). RED on `ModuleNotFoundError: tre_compare.cache` (binds the real API).

- **Task 2 — `cache.py` + gitignore (`963a0ad0d`):** `Cache(db_path=None, *, enabled=True)`
  (368 lines): `sqlite3.connect(check_same_thread=False)` + WAL + `synchronous=NORMAL` +
  `busy_timeout=5000`, the three `CREATE TABLE IF NOT EXISTS` (`archive_meta` /
  `archive_entry` / `file_hash`, all `mtime_ns INTEGER`), a `threading.Lock`, and a
  `__file__`-relative default path under `tools/tre-compare/.cache/`. `archive_entries(node,
  node_errors, *, no_cache)` HITs on `(abspath, mtime_ns, size)` or parses via the
  module-level `iter_node_entries` (Phase-28 fault isolation → `node_errors`), stamping each
  toc row with its `tre_file` (Pitfall 7), then persists under the lock with a re-check +
  `INSERT OR IGNORE`. `get_file_hash`/`put_file_hash` memo the Plan-01 hexdigest.
  `build_virtual_tree_cached` re-expresses the descending-pass merge VERBATIM (lines 360-396),
  threading `node.kind` from the LIVE node. `.cache/` added to `.gitignore`. All 8 GREEN;
  full suite **60 passed, 1 integration deselected**.

## Requirements Satisfied

- **TRE-02 / TRE-03 / TRE-04** (cache support tier) — the sqlite cache is the persistence
  layer the Plan-03 FastAPI routes serve from: set/file diffs read cached `archive_entries`
  (no re-parse on a re-run, SC#4) and drill-in reads the memoized `file_hash` + the cached
  `tre_file` (no 193k-entry `.toc` re-parse). The cached merge is parity-equivalent to the
  Phase-28 builder so the cache is transparent to the diff engine.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] parity_matrix fixture left `a/t.dds` real in the highest-priority tree**

- **Found during:** Task 2 (first `parity_matrix` run).
- **Issue:** The RED fixture put a REAL `a/t.dds` (length 3) in `hi.tre` at priority 10 AND a
  tombstone in `mid.tre` at priority 9. First-hit-wins means `hi.tre` claims `a/t.dds` BEFORE
  the tombstone is reached, so case-1 ("tree-tombstone-then-real-winner") was never exercised —
  the test's own sanity assertion (`"a/t.dds" in builder.tombstoned`) failed. The cached-vs-builder
  PARITY assertion itself passed; only the fixture's case coverage was wrong.
- **Fix:** Removed `a/t.dds` from `hi.tre` so the `mid.tre` (p9) tombstone is the FIRST encounter
  for that key; `low.tre`'s real `a/t.dds` then arrives after the tombstone and stays removed —
  the intended case-1 path. `hi/h.dds` still has its 3-way multi-shadow (winner `hi`@10, shadows
  `mid`@9 + `s2`@8) and `win/w.dds` still proves case-2 (lower tombstone after a winner → no evict).
- **Files modified:** `tools/tre-compare/tests/test_cache.py`
- **Commit:** `963a0ad0d` (folded into the GREEN commit with the implementation, per TDD;
  the fixture is test-corpus, not production code).

## Threat Mitigations Applied

- **T-29-02-01** (hostile-archive DoS on a MISS): the MISS parse reuses `iter_node_entries`,
  which runs the Phase-28 `_preflight_tre`/`_preflight_toc` bounds + `MAX_ENTRIES` cap and wraps
  the body in `_NODE_FAULTS`; a bad archive records into the `errs` accumulator (extended into the
  caller's `node_errors`) and yields no rows — never aborting the build. Proven by case 7 of
  `parity_matrix` (truncated `brk.tre` → a `node_error`, parity preserved).
- **T-29-02-02** (stale/poisoned row served fresh): the PK includes `(mtime_ns, size)` with
  INTEGER `st_mtime_ns` (closes the float-rounding + 1-2s FS-resolution window, P4); a rewritten
  archive yields a different key → MISS (proven by `test_invalidation_rewrite_misses`). The
  `no_cache=` per-call flag (+ `enabled=False` global) covers the residual mtime-PRESERVING-copy
  window (proven by `test_no_cache_bypass_reparses`, spy call-count==2).
- **T-29-02-03** (cross-thread misuse + concurrent-MISS PK race): `check_same_thread=False` + WAL
  + `busy_timeout=5000` + a process-wide write `Lock` guarding all writes AND the MISS
  detect+INSERT (with an under-lock re-check) + `INSERT OR IGNORE`. Proven by
  `test_concurrent_miss_no_integrity_error` (two threads + a barrier, no `IntegrityError`).
- **T-29-02-04** (`.sqlite` committed to git): `.cache/` added to `.gitignore`; the default path
  resolves via `__file__` so it always lands under the tool dir, not a launch-cwd-dependent
  location.

## Verification

- `uv run pytest -m "not integration" -q` → **60 passed, 1 deselected** (8 new cache tests +
  Plan-01 diff tests + all Phase-28 tests still green).
- `grep -c 'CREATE TABLE IF NOT EXISTS' cache.py` → **3**.
- `grep -c 'mtime_ns' cache.py` → 33; `grep -E 'mtime REAL' cache.py` → **nothing** (P4).
- `grep -oE 'busy_timeout|INSERT OR IGNORE|__file__|check_same_thread=False|journal_mode=WAL'`
  → all five present (Sonnet/Codex/Opus hardening).
- `grep -v '^#' cache.py | grep -E 'import fastapi'` → **nothing** (pure persistence tier).
- `.gitignore` contains `.cache/` (line 7).
- Line counts: `cache.py` 368 (≥160), `test_cache.py` 317 (≥130).

## TDD Gate Compliance

RED gate `test(29-02)` `c661c6aa3` (8 tests, `ModuleNotFoundError`) → GREEN gate `feat(29-02)`
`963a0ad0d`. No REFACTOR commit needed. The plan-level `type: execute` ran a per-task RED/GREEN
cycle; both gate commits present in `git log`.

## Known Stubs

None. The cache is wired against the real Phase-28 `iter_node_entries`/`safe_virtual_key`/
`_NODE_FAULTS` and the real parser headers/entry readers, exercised end-to-end over genuine
on-disk synthetic archives (parse → sqlite persist → re-SELECT → cached merge). No placeholder
data, no hardcoded empty returns.

## Self-Check: PASSED

- FOUND: tools/tre-compare/src/tre_compare/cache.py
- FOUND: tools/tre-compare/tests/test_cache.py
- FOUND: tools/tre-compare/.gitignore (`.cache/` @ line 7)
- FOUND commit: c661c6aa3 (test RED)
- FOUND commit: 963a0ad0d (feat GREEN)
