---
phase: 28-tre-compare-tool-foundation-parser-scanner-virtual-tree
plan: 04
subsystem: tooling
tags: [python, pytest, fixtures, tre, toc, cot2000, tombstone, merge, integration, tre-compare]

# Dependency graph
requires:
  - "28-02: vendored stdlib-only TRE/TOC/COT2000 parser (read_tre_entries/read_search_toc_entries/read_cot2000_entries/detect_master_index_kind/toc_entry_stride + struct constants)"
  - "28-03: scanner.parse_shared_file + virtual_tree.build_virtual_tree/fix_up_file_name/safe_virtual_key (the contracts under test)"
provides:
  - "Portable synthetic byte-builder corpus (tests/_fixtures.py): build_tre / build_search_toc / build_cot2000 / write_synthetic_cfg — pure stdlib, round-trips the vendored parser (D-06)"
  - "conftest synthetic_install fixture: lays out genuine on-disk tombstone-vs-real .tre/.toc node pairs + override dir + live-modeled client.cfg under tmp_path"
  - "Full behavioral suite (38 synthetic tests, machine-independent): per-version-tag parse (incl. extended-stride 6000/0006), compressed-block decompress, COT2000 + SearchTOC two-phase round-trip, scanner precedence + cross-kind + bare-priority + repeated-key + cfg-param, fixUpFileName + safe_virtual_key split, first-hit-wins, BOTH tombstone cases + inverse, cross-kind tree-wins, searchPath override/missing/empty, traversal reject, malformed + .tre/.toc oversized-header preflight"
  - "cfg-driven END-TO-END tombstone proof via parse_shared_file→build_virtual_tree over purpose-built genuine archives (SC#3 literal — user decision 2026-06-14)"
  - "Exactly one D-07 env/marker-gated integration test against the real stage/client.cfg (default off, clean-skip when absent); PASSES on this machine"
affects: [phase-29-diff-api, phase-30-web-ui]

# Tech tracking
tech-stack:
  added: []
  patterns: ["programmatic byte-built archive fixtures (struct.pack to the vendored layout, no committed blobs)", "two-phase SearchTOC fn_field-walked name block fixture", "extended-stride 8-byte TOC-row pad for 6000/0006", "zlib-compress fixture option exercising _decompress_block", "env+marker gated integration test (default-off, clean-skip)", "synthetic_install conftest fixture for cfg-driven end-to-end merge tests"]

key-files:
  created:
    - tools/tre-compare/tests/_fixtures.py
    - tools/tre-compare/tests/test_scanner.py
    - tools/tre-compare/tests/test_virtual_tree.py
    - tools/tre-compare/tests/test_integration.py
  modified:
    - tools/tre-compare/tests/conftest.py
    - tools/tre-compare/tests/test_parser.py
    - tools/tre-compare/src/tre_compare/virtual_tree.py

key-decisions:
  - "Imported MasterIndexKind from tre_compare.parser.tre_reader (submodule) in test_parser.py — it is not in Plan 02's parser/__init__ re-export (matches the same decision Plan 03 made for the .toc header readers); avoids retro-editing the Plan 02 public API surface"
  - "Reworded the _fixtures.py module docstring to avoid the literal 'configparser'/'swg_pipeline' strings so the no-stdlib-ini / no-engine-import acceptance grep returns 0 (the prose, not an import, was the only match — same precedent as Plan 02/03's grep-prose rewordings)"
  - "build_search_toc / build_cot2000 default offset is 0; a 'real' (non-absent) .toc test entry must pass an explicit non-zero offset, because the merge treats toc length-0 OR offset-0 as absent-here. Documented in the fixtures; the toc-preflight + cross-kind + end-to-end tests pass offset=64/128/16 for their real copies"
  - "Rule 1 auto-fix: safe_virtual_key's documented UNC-reject was unreachable (fix_up_file_name strips leading slashes BEFORE the key.startswith('//') guard could fire). Added a raw-name //host /\\\\host check at the top of the wrapper so it rejects UNC as the docstring/threat-model intends"

patterns-established:
  - "Byte-built fixtures forge the EXACT vendored layout and are validated by round-tripping the real parser inside acceptance one-liners + dedicated round-trip tests (no decompiled/committed binary blobs)"
  - "Direct-merge tests hand-order a SearchNode list with the same (-priority, KIND_RANK, cfg_seq) sort the scanner uses; the END-TO-END test instead drives the REAL parse_shared_file→build_virtual_tree pipeline over genuine on-disk archives"
  - "Integration test is the ONLY place a machine-specific absolute path lives; gated off by default + clean-skips when the install is absent (D-07/D-08, T-28-04-02)"

requirements-completed: [TRE-01]

# Metrics
duration: ~7min
completed: 2026-06-14
---

# Phase 28 Plan 04: Synthetic Corpus + Behavioral Suite + Gated Integration Summary

**The phase's "fully unit-tested" deliverable: a portable, pure-stdlib synthetic byte-builder corpus (`build_tre`/`build_search_toc`/`build_cot2000`/`write_synthetic_cfg`) that round-trips the vendored parser, driving a 38-test machine-independent behavioral suite that locks every SC#3 mechanic — per-version-tag parse (incl. the extended-stride 6000/0006 8-byte pad), zlib-compressed-block decompress, COT2000 + SearchTOC two-phase round-trips, engine-faithful scanner precedence + same-priority cross-kind order + bare-priority grammar + repeated keys, `fixUpFileName` + the `safe_virtual_key` split, first-hit-wins, BOTH per-node-type tombstone cases PLUS the inverse lower-priority case, the cross-kind tree-wins case, searchPath override/missing/empty edges, path-traversal reject, and `.tre`+`.toc` oversized-header preflight skips (asserting `node_errors`) — PLUS a cfg-driven END-TO-END tombstone proof through the real `parse_shared_file → build_virtual_tree` pipeline over purpose-built genuine on-disk archives (SC#3 met LITERALLY), AND exactly one D-07 env/marker-gated integration test against the real `stage/client.cfg` that PASSES on this machine (path@10-first / toc@0-last precedence + priority-10 override-shadowing) and clean-skips elsewhere.**

## Performance

- **Duration:** ~7 min
- **Started:** 2026-06-14T19:43Z
- **Completed:** 2026-06-14T19:51Z
- **Tasks:** 3
- **Files created:** 4 (_fixtures.py, test_scanner.py, test_virtual_tree.py, test_integration.py)
- **Files modified:** 3 (conftest.py, test_parser.py, virtual_tree.py)

## Accomplishments

### Task 1 — _fixtures.py + conftest synthetic_install (commit ada8bd670)
- `build_tre(path, entries, *, version, compress)` forges header (STANDARD_HEADER_FMT) + N×stride TOC + NUL-term name block; per-entry `fileNameOffset` is the running name-block offset; extended tags (6000/0006) pad each 24-byte TOC_ENTRY_FMT row with 8 zero bytes so `base = i * stride` aligns; `compress=True` zlib-compresses the TOC + name blocks (sizes → compressed, `uncompSizeOfNameBlock` stays uncompressed).
- `build_search_toc(...)` forges the verified TWO-PHASE layout: 36-byte header + tree-name block + N×24 TOC (`SEARCH_TOC_ENTRY_FMT`, `fn_field = len(name)`) + row-order name block walked via `fn_offset += fn_field + 1`; `compress` option included.
- `build_cot2000(path, tre_name, entries)` forges magic(8) + `"<7I"` header@8 + tree-name block@36 + N×32 TOC + name block whose size EXACTLY equals the walked total (or the reader raises the mismatch `TreFormatError`).
- `write_synthetic_cfg(path, nodes, ...)` writes a TAB-indented, double-quoted `[SharedFile]` block with a `#` comment + `maxSearchPriority`/`TOCTreePath` scalars; nodes accept `(kind, priority, value)` in cfg-discovery order, plus a `kind_bare` variant for bare-priority keys.
- `conftest.synthetic_install` lays out genuine on-disk tombstone-vs-real `.tre`/`.toc` node pairs + an empty override dir + a live-modeled `client.cfg` under `tmp_path`.
- All three round-trip acceptance one-liners print `*-roundtrip-ok`; every Task-1 grep passes (build_cot2000, struct.pack, two-phase fn_field, compress/zlib, toc_entry_stride; configparser/swg_pipeline = 0).

### Task 2 — behavioral suite (commit 0d3376516)
- `test_parser.py` extended: `test_parse_each_version_tag` (5 tags, real bytes, 2nd-entry alignment proves the extended pad), `test_parse_compressed_blocks` (`_decompress_block` path), `test_parse_cot2000_and_searchtoc` (both master indexes READ + kind-detected + `parse_master_index` routed — literal SC#1).
- `test_scanner.py`: priority-order-higher-first, same-priority cross-kind path→tree→toc (replaces the removed engine-wrong `test_stable_tie_order`), same-kind cfg-order tie, repeated-key→2-nodes, bare-priority, quote/comment/scalar, cfg-path-param.
- `test_virtual_tree.py`: fixUpFileName vectors (incl. interior-`..` kept) + safe_virtual_key split rejection, first-hit-wins + shadowed-real, tree-length-0 global remove, INVERSE lower-tree-tombstone-does-not-un-claim (shadowed excludes it), toc length-0 AND offset-0 no-shadow, cross-kind tree-wins-over-toc, searchPath override / missing-dir / empty-dir, path-traversal reject → `rejected`, malformed-node skip → `node_errors`, `.tre` and `.toc` oversized-header preflight skip → `node_errors`, and the cfg-driven END-TO-END tombstone test (+ a `test_toc_no_shadow_end_to_end_via_cfg` sibling) driving the real pipeline.
- `uv run pytest -m "not integration"` → 38 passed, no marker warnings; every Task-2 grep gate passes (`test_stable_tie_order` count = 0).

### Task 3 — gated integration test (commit ab776468a)
- `test_integration.py`: exactly one `@pytest.mark.integration` test behind the `TRE_COMPARE_INTEGRATION` env gate; deselected by default; clean-skips when the cfg/archives/override are absent (D-08). Asserts SC#3 precedence (path@10 first, toc@0 last) and override shadowing (priority-10 `winner_node.kind=='path'` wins when the override dir is non-empty).
- Default run: 38 passed, 1 deselected. Opt-in run (`TRE_COMPARE_INTEGRATION=1`): **1 passed** — the real SWGSource v3.0 install IS present on this machine, so the override-shadowing/precedence proof executed (not skipped).

## Task Commits

1. **Task 1: byte-built fixtures + synthetic cfg + conftest** — `ada8bd670` (test)
2. **Task 2: behavioral suite (parser per-tag, scanner, vtree merge) + safe_virtual_key Rule-1 fix** — `0d3376516` (test)
3. **Task 3: gated real-install integration test (D-07)** — `ab776468a` (test)

**Plan metadata** (this SUMMARY + STATE/ROADMAP/REQUIREMENTS/VALIDATION) committed separately.

## Files Created/Modified

- `tools/tre-compare/tests/_fixtures.py` — pure-stdlib byte-builders matching the vendored parser layout exactly.
- `tools/tre-compare/tests/conftest.py` — `fixtures` + `synthetic_install` pytest fixtures.
- `tools/tre-compare/tests/test_parser.py` — added per-tag / compressed-block / COT2000+SearchTOC round-trip tests.
- `tools/tre-compare/tests/test_scanner.py` — scanner precedence + cross-kind + bare-priority + repeated-key + cfg-param suite.
- `tools/tre-compare/tests/test_virtual_tree.py` — full merge suite + cfg-driven end-to-end tombstone proof.
- `tools/tre-compare/tests/test_integration.py` — the one D-07 gated real-install test.
- `tools/tre-compare/src/tre_compare/virtual_tree.py` — Rule-1 fix: `safe_virtual_key` now rejects UNC on the raw name (the post-canonicalization guard was unreachable).

## Decisions Made

- **`MasterIndexKind` import from the submodule** — not in Plan 02's `parser/__init__` re-export; imported from `tre_compare.parser.tre_reader` (same pattern Plan 03 used for the `.toc` header readers). No public-API retro-edit.
- **Docstring reword to satisfy the `configparser`/`swg_pipeline`=0 grep** — the only match was prose in the module docstring; reworded to "no stdlib-ini-parser and no cross-package engine import" (precedent: Plan 02/03 reworded prose to satisfy literal greps). Actual imports were always clean.
- **Explicit non-zero offset for "real" `.toc` test entries** — the merge treats a toc entry with `length==0 OR offset==0` as absent-here, so a real (winning) `.toc` fixture entry must carry a non-zero offset. The default builder offset is 0 (the absent case); real copies pass offset=64/128/16.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] safe_virtual_key UNC reject was unreachable**
- **Found during:** Task 2 (`test_safe_virtual_key_rejects_traversal` — `safe_virtual_key("//host/x")` returned `"host/x"`, not `None`).
- **Issue:** `safe_virtual_key` (Plan 03) documents rejecting "a drive-letter/UNC/absolute first segment", but its `if key.startswith("//")` guard ran AFTER `fix_up_file_name`, which strips ALL leading slashes — so a `//host/x` / `\\host\x` form canonicalized to a benign `host/x` and the UNC guard could never fire. The plan + threat model (T-28-04-01) require UNC rejection.
- **Fix:** Added a `raw = name.replace("\\","/"); if raw.startswith("//"): return None` check at the TOP of the wrapper (before canonicalization). Engine-faithful `fix_up_file_name` is untouched (the split is preserved); only the documented hardening wrapper is corrected.
- **Files modified:** `tools/tre-compare/src/tre_compare/virtual_tree.py`
- **Commit:** `0d3376516`
- This is a source change to Plan-03 code, but it fixes the wrapper to MATCH its own documented intent (and the threat model), not a behavior change to the engine-faithful `fix_up_file_name`. Per project memory it modifies a deliberate hardening seam (not Koogie's diagnostic patches) and has a strong reason (the guard was dead code failing its stated contract).

## Threat Model Notes

- **T-28-04-01 (path-traversal + malformed + bounds-preflight regression guards):** `mitigate` — `test_safe_virtual_key_rejects_traversal` + `test_path_traversal_rejected` (rejected list), `test_malformed_archive_node_skipped` (node_errors), `test_bounds_preflight_rejects_oversized_header` (.tre) + `test_toc_bounds_preflight_rejects_oversized_header` (.toc) all green — the Plan-03 T-28-03-01/02/04 mitigations now have regression coverage. The Rule-1 fix closed a latent UNC hole.
- **T-28-04-02 (machine-path leak in the integration test):** `accept` — the real abspath lives only in the gated `test_integration.py` constant, deselected by default + skipped on extraction (env var absent / path absent). The synthetic corpus has zero machine paths.
- **T-28-04-03 (fixture-builder DoS):** `accept` — fixtures are tiny (few entries, empty/tiny-zlib payloads); the oversized-header tests forge BOUNDED headers without writing large bytes; suite runtime ~0.3s synthetic / ~2.5s integration.

## Known Stubs

None. Every test asserts real behavior against genuine byte-built archives or the real cfg; no placeholder/empty-data tests.

## Threat Flags

None — this plan adds tests + one wrapper-hardening fix; it introduces no new network endpoints, auth paths, or schema. The integration test is the only new trust-boundary contact (real cfg + install), already in the threat register (T-28-04-02).

## TDD Gate Compliance

This is a `type: tdd` plan, but it is a TEST-AUTHORING wave: the implementation (parser/scanner/merge) shipped GREEN in Plans 02/03, and Plan 04's charter is to write the behavioral suite that exercises it. Consequently the RED→GREEN sequence is inverted from a feature-TDD plan — the tests were written against existing GREEN code. The one genuine RED→GREEN moment occurred mid-Task-2: `test_safe_virtual_key_rejects_traversal` and `test_toc_bounds_preflight_rejects_oversized_header` FAILED on first run (a real wrapper bug + a fixture-offset bug), were fixed, and went GREEN — the RED phase surfaced and fixed a latent defect exactly as TDD intends. All commits are `test(...)` (the source fix rode with its surfacing test, Rule 1). No separate `feat(...)` gate commit applies because no new feature was implemented in this plan.

## Next Phase Readiness

- **Phase 29 (diff/API)** inherits a fully-proven, importable core + a portable fixture toolkit: `build_tre`/`build_search_toc`/`build_cot2000`/`write_synthetic_cfg` can construct diff-pair corpora (e.g. SWGSource-vs-whitengold-style A/B sets) without a real install, and the merge data model (`VirtualTree.entries` with `length`+`compressed_length`, `winner_node`, `shadowed`, `tombstoned`, `rejected`, `node_errors`) is locked by tests.
- **Extraction (D-01):** the whole `tools/tre-compare/` tree is copy-paste-able; the synthetic suite (`-m "not integration"`) runs anywhere; only the env-gated integration test references a machine path and clean-skips when absent.

## Self-Check: PASSED

- Created files verified present: `_fixtures.py`, `test_scanner.py`, `test_virtual_tree.py`, `test_integration.py`; modified `conftest.py`, `test_parser.py`, `virtual_tree.py` (all on disk).
- Commits verified in git log: `ada8bd670` (Task 1), `0d3376516` (Task 2), `ab776468a` (Task 3).
- `uv run pytest -m "not integration" -q` → 38 passed; `TRE_COMPARE_INTEGRATION=1 uv run pytest -m integration -q` → 1 passed (38 deselected); all Task 1/2/3 grep acceptance gates pass.

---
*Phase: 28-tre-compare-tool-foundation-parser-scanner-virtual-tree*
*Completed: 2026-06-14*
