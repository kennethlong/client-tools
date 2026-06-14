---
phase: 28-tre-compare-tool-foundation-parser-scanner-virtual-tree
plan: 03
subsystem: tooling
tags: [python, scanner, virtual-tree, tre, toc, cot2000, merge, tombstone, fixupfilename, tre-compare]

# Dependency graph
requires:
  - "28-01: isolated uv library scaffold (src layout, pytest test root, zero runtime deps)"
  - "28-02: vendored stdlib-only TRE/TOC/COT2000 parser (read_tre_entries/read_*_header/read_search_toc_entries/read_cot2000_entries/toc_entry_stride/detect_master_index_kind + exceptions)"
provides:
  - "scanner.parse_shared_file(cfg_path) — hand-parses [SharedFile] repeated/indexed keys (NOT configparser) → engine-faithful (-priority, KIND_RANK[kind], cfg_seq)-ordered SearchNode list (cfg path is a parameter, D-08)"
  - "Both key grammars: _NN_ (sku) AND bare-priority (legacy empty-skuText) — review finding #6"
  - "virtual_tree.fix_up_file_name — VERBATIM engine port (leading-.. only; keeps interior ..)"
  - "virtual_tree.safe_virtual_key — SEPARATE hardening wrapper rejecting interior-../drive/UNC/empty (threat T-28-03-01)"
  - "virtual_tree.build_virtual_tree — single descending pass first-hit-wins merge on canonical path (never crc)"
  - "PER-NODE-TYPE tombstone: tree length-0 = global remove; toc length-0/offset-0 = skip-only (never shadows)"
  - "shadowed = later REAL copies only (excludes tombstones/absent) — review finding #4"
  - "Eager deterministic searchPath os.walk (reparse-dir prune before descent, followlinks=False) — Open-Q1 RESOLVED"
  - ".tre AND .toc header bounds preflight + count*stride cap (threat T-28-03-04)"
  - "VirtualTree.node_errors diagnostics + rejected list (review finding #8); MergedEntry/VirtualTree data model"
affects: [28-04-fixtures-integration, phase-29-diff-api, phase-30-web-ui]

# Tech tracking
tech-stack:
  added: []
  patterns: ["engine-faithful (-priority, kind_rank, cfg_seq) registration-order sort", "verbatim engine port + SEPARATE hardening wrapper split (fix_up_file_name vs safe_virtual_key)", "single-descending-pass first-hit-wins with guard-before-tombstone-branch", "per-node-type tombstone (tree=global vs toc=skip)", "bounds-preflight-at-merge-boundary for hostile headers", "fail-isolated per-node enumeration (node_errors)", "deterministic escape-safe os.walk (dir-prune before descent)"]

key-files:
  created:
    - tools/tre-compare/src/tre_compare/scanner.py
    - tools/tre-compare/src/tre_compare/virtual_tree.py
  modified:
    - tools/tre-compare/src/tre_compare/__init__.py

key-decisions:
  - "Imported the two .toc header readers (read_search_toc_header, read_cot2000_header) directly from .parser.tre_reader (plus COT2000_GLOBAL_ENTRY_SIZE + MasterIndexKind) — they were not in Plan 02's parser/__init__ re-export list; importing from the submodule avoids retro-editing the Plan 02 public-API contract while still enabling the .toc bounds preflight"
  - "Worded the scanner D-08 docstring to avoid the literal `stage/client.cfg` string so the no-hardcoded-path acceptance grep returns 0 cleanly (the literal appeared only in explanatory prose; reworded to 'no default cfg location')"
  - "Loose-file model: length == compressed_length == on-disk size, offset 0 (loose files are uncompressed); rel path yielded raw and run through safe_virtual_key in the merge for uniform rejection (backslash→/ normalization handles Windows os.sep in walked rel paths)"

patterns-established:
  - "Split engine-port + hardening: fix_up_file_name stays VERBATIM/engine-faithful (interior .. kept); safe_virtual_key is the documented, deliberate non-engine divergence (review #3)"
  - "First-hit-wins INVARIANT enforced by ordering: the `key in claimed or key in tombstoned` guard runs BEFORE the tree-length-0 branch, so a lower-priority tombstone can never un-claim a won winner (review #1; claimed.pop deleted)"
  - "Bounds preflight at the wrapper, parser left UNMODIFIED (D-03): both .tre (toc+name<=EOF, count*stride<=size_of_toc, count<=MAX_ENTRIES) and .toc (SEARCH_TOC/COT2000 blocks<=EOF, COT2000 count*32<=toc-block) checked before the typed reader decompresses"

requirements-completed: [TRE-01]

# Metrics
duration: ~6min
completed: 2026-06-14
---

# Phase 28 Plan 03: Scanner + Virtual-Tree Merge Summary

**The correctness keystone of the TRE tool: `scanner.py` hand-parses `[SharedFile]` repeated/indexed keys (both `_NN_` and bare-priority grammars, cfg path a parameter) into an engine-faithful `(-priority, KIND_RANK[kind], cfg_seq)`-ordered `SearchNode` list, and `virtual_tree.py` ports `fixUpFileName` VERBATIM, adds a SEPARATE `safe_virtual_key` hardening wrapper, and merges first-hit-wins on canonical path in a single descending pass with PER-NODE-TYPE tombstone (tree=global-remove, toc=skip-only), eager deterministic `searchPath` walk (reparse-dir pruned before descent), `.tre`+`.toc` header bounds preflights, and observable `node_errors`/`rejected` diagnostics — all behaviors smoke-verified; the full behavioral suite is gated by Plan 04.**

## Performance

- **Duration:** ~6 min
- **Started:** 2026-06-14T19:33Z
- **Completed:** 2026-06-14T19:39Z
- **Tasks:** 2
- **Files created:** 2 (scanner.py, virtual_tree.py)
- **Files modified:** 1 (__init__.py)

## Accomplishments

### Task 1 — scanner.py (commit d0a784c16)
- `parse_shared_file(cfg_path)` reads the cfg line-by-line (NO configparser), toggling on the `[SharedFile]` section header, skipping blank/`#`/non-`=` lines, stripping a single surrounding double-quote pair, capturing `maxSearchPriority`/`TOCTreePath` scalars.
- Matches BOTH key grammars (review #6): `^search(TOC|Tree|Path)_(\d+)_(\d+)$` (sku) and `^search(TOC|Tree|Path)(\d+)$` (bare-priority legacy, sku=-1 sentinel). Priority is the FINAL trailing integer (Pitfall 5). Repeated key NAMES yield distinct nodes (cfg_seq tie-break).
- ENGINE-FAITHFUL sort (review #1): `sorted(..., key=lambda n: (-n.priority, KIND_RANK[n.kind], n.cfg_seq))` with `KIND_RANK = {"path":0,"tree":1,"toc":2}` — NOT a plain `-priority` stable sort.
- Smoke-verified: live-cfg block sorts `path(10)→tree(8)→tree(7)→toc(3..0)`; same-priority cross-kind orders path→tree→toc REGARDLESS of cfg line order (toc listed first still loses to tree); repeated toc keys keep cfg discovery order; bare-priority `searchPath10` parses to priority 10 / sku -1; other-section keys ignored.

### Task 2 — virtual_tree.py (commit 3a1d3df83)
- `fix_up_file_name` VERBATIM engine port (TreeFile.cpp:511-601): strips leading slashes/`./`/`..` only, backslash→/, lowercase, collapse repeats. Keeps interior `..` (engine-faithful, review #3) — verified `fix_up_file_name('a/../../x')=='a/../../x'`.
- `safe_virtual_key` SEPARATE hardening wrapper: rejects residual interior `..`, drive-letter/UNC/absolute first segment, and empty → `None` (skip-and-record, not raise). Verified `s('a/../../x') is None`, `s('c:/x') is None`, `s('foo/bar.dds')=='foo/bar.dds'`.
- `build_virtual_tree` single descending pass, first-hit-wins; the `key in claimed or key in tombstoned` guard runs BEFORE the tree-length-0 branch (review #1, `claimed.pop` deleted). PER-NODE-TYPE tombstone: tree length-0 reached first → global `tombstoned`; toc length-0/offset-0 → skip-only, never tombstones. `shadowed` records later REAL copies only (excludes `is_tree_tombstone`/`is_toc_absent`, review #4).
- Eager deterministic `searchPath` `os.walk(followlinks=False)`: prunes symlink/reparse DIRS from `dirnames[:]` BEFORE descent, skips reparse FILES, sorts dirs+files, confirms `relpath` does not escape; loose files injected at node priority (Open-Q1). Missing dir → no entries, no error.
- `.tre` bounds preflight (toc+name≤EOF, count*stride≤size_of_toc, count≤MAX_ENTRIES=5M) and `.toc` bounds preflight (SEARCH_TOC + COT2000 blocks≤EOF, COT2000 count*32≤toc-block) run BEFORE the typed reader (threat T-28-03-04). Per-node body wrapped in try/except for `TreFormatError`/`UnsupportedTreVersionError`/`struct.error`/`OSError`/`MemoryError`/`zlib.error` → `node_errors` (threat T-28-03-02, review #8).
- Smoke-verified all 9 behaviors: first-hit-wins + shadowed-real; tree-length-0 high→global remove; INVERSE lower tree-length-0 does NOT un-claim winner (not tombstoned, not shadowed); same-priority cross-kind tree-beats-toc; toc length-0 AND offset-0 absent-here (lower real served, not shadowed); traversal+absolute rejected to `rejected`; searchPath override beats lower tree (loose size, not tree size); missing-dir no-error.

## Task Commits

1. **Task 1: scanner.py — hand-parse [SharedFile] → engine-faithful SearchNode list** — `d0a784c16` (feat)
2. **Task 2: virtual_tree.py — fixUpFileName + safe_virtual_key + first-hit-wins merge** — `3a1d3df83` (feat)

**Plan metadata** (this SUMMARY + STATE/ROADMAP/REQUIREMENTS) committed separately.

## Files Created/Modified

- `tools/tre-compare/src/tre_compare/scanner.py` — `SearchNode`/`ScanResult` dataclasses + `parse_shared_file` hand parser with the engine-faithful `(-priority, KIND_RANK, cfg_seq)` sort and both key grammars.
- `tools/tre-compare/src/tre_compare/virtual_tree.py` — `fix_up_file_name` (verbatim) + `safe_virtual_key` (hardening) + `MergedEntry`/`VirtualTree` + `iter_node_entries` (with `.tre`/`.toc` preflights + fail-isolation) + `_walk_search_path` (escape-safe deterministic walk) + `build_virtual_tree` (single-pass first-hit-wins + per-node-type tombstone).
- `tools/tre-compare/src/tre_compare/__init__.py` — re-exports `parse_shared_file`/`SearchNode`/`ScanResult` (Task 1) + `fix_up_file_name`/`safe_virtual_key`/`build_virtual_tree`/`MergedEntry`/`VirtualTree` (Task 2).

## Decisions Made

- **`.toc` header readers imported from the submodule:** `read_search_toc_header`/`read_cot2000_header` + `COT2000_GLOBAL_ENTRY_SIZE`/`MasterIndexKind` were not in Plan 02's `parser/__init__` re-export list. Imported them directly from `.parser.tre_reader` for the `.toc` bounds preflight rather than retro-editing Plan 02's public-API surface. The merge's primary readers (`read_tre_entries`/`read_search_toc_entries`/`read_cot2000_entries`/`read_tre_header`/`toc_entry_stride`/`detect_master_index_kind`) all come from the public `.parser` re-export.
- **D-08 docstring wording:** the literal `stage/client.cfg` initially appeared in two explanatory docstring lines (explaining what is NOT hardcoded). Reworded to "no default cfg location" so the acceptance grep `grep -c 'stage/client.cfg'` returns 0 — the intent (no hardcoded path) was always satisfied; this removed the prose false-positive.
- **Loose-file metadata convention:** loose `searchPath` files yield `(rel, size, size, 0)` (uncompressed: length == compressed_length == on-disk size, offset 0); the raw OS-separator rel path is yielded and normalized by `safe_virtual_key` in the merge (uniform rejection path, backslash→/ handled by `fix_up_file_name`).

## Deviations from Plan

None — plan executed exactly as written. The two noted decisions (submodule import of the `.toc` header readers; reworded D-08 docstring to satisfy the literal grep) are within the plan's specified behavior, not deviations: the plan's `<interfaces>` block explicitly lists `read_search_toc_header`/`read_cot2000_header` as available parser API, and D-08 mandates no hardcoded path.

## Threat Model Notes

All three Plan-03-owned mitigations are implemented at the merge boundary (parser left UNMODIFIED, D-03):
- **T-28-03-01 (path traversal):** `fix_up_file_name` strips leading `..` (engine-faithful); `safe_virtual_key` additionally rejects interior-`..`/drive/UNC/empty (documented non-engine hardening); `os.walk` prunes reparse/symlink dirs before descent + confirms `relpath` does not escape; unsafe keys → `rejected`, unsafe dirs/files → `node_errors`.
- **T-28-03-02 (malformed-header DoS):** per-node enumeration wrapped in try/except over the 6 fault types → `(node, str(exc))` in observable `node_errors`, skipped not fatal.
- **T-28-03-04 (oversized-header OOM):** `.tre` AND `.toc` bounds preflights (offsets/blocks ≤ file size, count*stride ≤ declared TOC size, count ≤ MAX_ENTRIES) run BEFORE the typed reader slices/decompresses. (zlib EXPANSION of TOC/name blocks remains unbounded — flagged for Phase 29, RESEARCH A3, per the plan.)

## Known Stubs

None. Both modules are fully functional implementations with no hardcoded empty values, placeholder text, or unwired data sources. The behavioral test suite + synthetic byte-built fixtures are scoped to Plan 04 by design (the plan's `<verify>` blocks state "MISSING — Plan 04 provides the behavioral suite"); this is the agreed wave split, not a stub.

## Next Phase Readiness

- **Plan 04 (fixtures + integration)** is unblocked: it can byte-build synthetic TRE/TOC/COT2000 archives + synthetic cfgs and assert the full behavioral contract — scanner ordering (incl. same-priority cross-kind + bare-priority + repeated keys), `fix_up_file_name`/`safe_virtual_key`, both tombstone cases (incl. the inverse lower-priority-tombstone test), the oversized-header `.toc` preflight test, and the searchPath override/missing/empty-dir tests — against these importable objects. The env-gated D-07 integration test can run `parse_shared_file(stage/client.cfg)` + `build_virtual_tree`.
- **Phase 29 (diff/API)** has the merge data model locked: `VirtualTree.entries` (canonical_path → `MergedEntry` with `length`+`compressed_length` for the cheap changed-signal, `winner_node`, `shadowed`), plus `tombstoned`/`rejected`/`node_errors` for full provenance/diagnostics. The merge key is the canonical path string only (never crc).

## Self-Check: PASSED

- Created files verified present: `scanner.py`, `virtual_tree.py` (both on disk); modified `__init__.py`.
- Commits verified in git log: `d0a784c16` (Task 1), `3a1d3df83` (Task 2).
- `uv run pytest -m "not integration"` → 8 passed (existing suite still green); all Task 1 + Task 2 acceptance greps + import/split/fixup spot-checks pass; 9-case merge smoke + 3-case walk smoke all PASSED (temp scripts removed pre-commit).

---
*Phase: 28-tre-compare-tool-foundation-parser-scanner-virtual-tree*
*Completed: 2026-06-14*
