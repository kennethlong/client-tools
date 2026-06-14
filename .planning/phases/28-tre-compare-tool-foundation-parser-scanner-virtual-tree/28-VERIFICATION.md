---
phase: 28-tre-compare-tool-foundation-parser-scanner-virtual-tree
verified: 2026-06-14T21:30:00Z
status: passed
score: 10/10 must-haves verified
overrides_applied: 0
human_verification_resolved:
  - test: "Gated integration test: cd tools/tre-compare && $env:TRE_COMPARE_INTEGRATION=1; uv run pytest -m integration -q"
    result: "1 passed, 38 deselected in 2.44s — confirmed live by the orchestrator on this machine (SWGSource v3.0 install present). The verifier subagent could not run it; the orchestrator ran the opt-in test directly and it exited 0, proving path@10-first / toc@0-last precedence + priority-10 override shadowing on the real cfg."
    resolved: 2026-06-14T21:35:00Z
---

# Phase 28: TRE Compare Tool Foundation — Verification Report

**Phase Goal:** Stand up the headless, fully unit-tested backend foundation for the TRE compare tool — parser reuse (vendored from D:/Code/swg-blender-plugin/swg_pipeline/), cfg search-path scanning, and engine-faithful virtual-tree merging. Tool lives at tools/tre-compare/ as an isolated, extractable uv package with zero engine imports.
**Verified:** 2026-06-14T21:30:00Z
**Status:** human_needed
**Re-verification:** No — initial verification

---

## Goal Achievement

### Observable Truths

| #  | Truth | Status | Evidence |
|----|-------|--------|----------|
| 1  | Isolated uv package at tools/tre-compare/ with ZERO runtime deps and ZERO engine imports (D-01/D-02) | ✓ VERIFIED | `pyproject.toml` has `dependencies = []`; grep of `src/` for `from swg_pipeline`/`import engine` returns 0 matches (only provenance comments in headers, not imports) |
| 2  | Parser vendored from swg-blender-plugin (all 5 TREE variants + COT2000 + SearchTOC) without rewrite (SC#1) | ✓ VERIFIED | `tre_reader.py` + `tre_decrypt.py` present with provenance header pinning commit `f803f587...`; single import rewrite at line 255 from `swg_pipeline.tre_decrypt` → `.tre_decrypt`; `SUPPORTED_TRE_VERSIONS = frozenset({"0004","0005","6000","0006","5000"})` confirmed |
| 3  | Scanner hand-parses `[SharedFile]` repeated/indexed keys (NOT configparser) → engine-faithful (-priority, kind_rank, cfg_seq) ordered node list; cfg path is a parameter (SC#2, D-08) | ✓ VERIFIED | `scanner.py` uses `_RE_SKU_KEY` + `_RE_BARE_KEY` regex (no configparser import); sorted via `(-priority, KIND_RANK[kind], cfg_seq)`; `def parse_shared_file(cfg_path: str | Path)` — no default; no `stage/client.cfg` literal in module |
| 4  | Virtual-tree builder: fix_up_file_name VERBATIM, safe_virtual_key as SEPARATE hardening wrapper, first-hit-wins on canonical path, per-node-type tombstone (tree=global, toc=skip-only), shadowed=REAL-copies-only, node_errors diagnostics (SC#3) | ✓ VERIFIED | `virtual_tree.py` full read confirms: `fix_up_file_name` keeps interior `..`; `safe_virtual_key` rejects it (plus UNC fix); `claimed/tombstoned` guard precedes tree-length-0 branch (line 368); `claimed.pop` absent; is_tree_tombstone/is_toc_absent guard on shadowed; `VirtualTree.node_errors` field present |
| 5  | .tre AND .toc header bounds preflight + count×stride cap before typed reader (threat T-28-03-04) | ✓ VERIFIED | `_preflight_tre` + `_preflight_toc` functions present; check `toc_offset+size_of_toc+size_of_name_block <= file_size`, `number_of_files * stride <= size_of_toc`, `number_of_files <= MAX_ENTRIES`; `.toc` preflights SEARCH_TOC and COT2000 separately |
| 6  | searchPath os.walk: followlinks=False, reparse-dir PRUNE before descent, filenames sorted, relpath escape check (threat T-28-03-01, review #5/#7) | ✓ VERIFIED | `_walk_search_path` uses `os.walk(base, followlinks=False)`; `dirnames[:] = sorted(safe_dirs)` mutates before descent; `filenames.sort()`; `_is_reparse_or_link` check on both dirs and files; `rel.startswith("..")` guard |
| 7  | Synthetic byte-built fixture corpus: build_tre/build_search_toc/build_cot2000/write_synthetic_cfg — pure stdlib, round-trips vendored parser (D-06) | ✓ VERIFIED | `tests/_fixtures.py` has all four builders; uses `struct.pack`; two-phase SearchTOC `fn_field` layout present; `toc_entry_stride` used for extended-stride padding; `compress`/`zlib` option present |
| 8  | Full behavioral suite: 38 synthetic tests covering all required behaviors — per-version-tag parse, compressed-block, COT2000+SearchTOC round-trips, scanner precedence+kind-order, BOTH tombstone cases + inverse, cross-kind tree-wins, searchPath override/missing/empty, traversal reject, .tre+.toc bounds-preflight, cfg-driven end-to-end tombstone (SC#3) | ✓ VERIFIED | `uv run pytest -m "not integration"` → 38 passed, 1 deselected, 0.36s; all named tests confirmed present: `test_searchtree_lower_priority_length0_does_not_tombstone_winner`, `test_searchtoc_length0_does_NOT_shadow`, `test_same_priority_cross_kind_tree_wins_over_toc`, `test_tombstone_end_to_end_via_cfg`, `test_toc_no_shadow_end_to_end_via_cfg`, `test_safe_virtual_key_rejects_traversal`, `test_same_priority_kind_order_path_before_tree_before_toc`, `test_toc_bounds_preflight_rejects_oversized_header`, etc. `test_stable_tie_order` is absent (correctly removed). |
| 9  | Integration test: exactly 1 test, env/marker-gated, default-off, clean-skips when absent (D-07) | ✓ VERIFIED | `test_integration.py` has exactly 1 `def test_real_client_cfg_scan_and_merge`; `@pytest.mark.integration` + `skipif(not os.environ.get("TRE_COMPARE_INTEGRATION"))`; default run shows 1 deselected; `winner_node.kind == "path"` assertion present |
| 10 | Package-local .gitignore (.venv/, __pycache__, dist/), .python-version pinned to 3.11 floor, uv.lock committed with pytest entry, `integration` marker registered (D-05, extractability) | ✓ VERIFIED | `.gitignore` contains `.venv/`, `__pycache__/`, `dist/`; `.python-version` = `3.11`; `uv.lock` contains `name = "pytest"`; `pyproject.toml` markers = `["integration: ..."]`; `requires-python = ">=3.11"` |

**Score:** 10/10 truths verified

---

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `tools/tre-compare/pyproject.toml` | uv project config: zero deps, pytest dev dep, integration marker, uv_build backend | ✓ VERIFIED | All required fields confirmed |
| `tools/tre-compare/.gitignore` | Python ignores incl. .venv/, dist/ | ✓ VERIFIED | All 6 patterns present |
| `tools/tre-compare/uv.lock` | Committed lockfile with pytest | ✓ VERIFIED | `name = "pytest"` entry present |
| `tools/tre-compare/.python-version` | Pinned to 3.11 | ✓ VERIFIED | Contains `3.11` |
| `tools/tre-compare/src/tre_compare/parser/__init__.py` | Public API re-export | ✓ VERIFIED | Re-exports all symbols with `__all__` |
| `tools/tre-compare/src/tre_compare/parser/tre_reader.py` | Vendored with provenance + import rewrite | ✓ VERIFIED | Commit hash in header; `from .tre_decrypt import` at line 255 |
| `tools/tre-compare/src/tre_compare/parser/tre_decrypt.py` | Vendored with provenance | ✓ VERIFIED | Commit hash in header |
| `tools/tre-compare/src/tre_compare/scanner.py` | `parse_shared_file`, `SearchNode`, `ScanResult` | ✓ VERIFIED | All symbols present; engine-faithful sort; no configparser import |
| `tools/tre-compare/src/tre_compare/virtual_tree.py` | `fix_up_file_name`, `safe_virtual_key`, `build_virtual_tree`, `MergedEntry`, `VirtualTree` | ✓ VERIFIED | All symbols present; merge loop correct; bounds preflights present |
| `tools/tre-compare/tests/_fixtures.py` | `build_tre`, `build_search_toc`, `build_cot2000`, `write_synthetic_cfg` | ✓ VERIFIED | All four builders present; struct.pack based |
| `tools/tre-compare/tests/test_parser.py` | Per-tag, compressed-block, COT2000+SearchTOC tests | ✓ VERIFIED | 6 test functions/parametrized groups |
| `tools/tre-compare/tests/test_scanner.py` | Scanner precedence + kind-order + bare-priority + repeated-key | ✓ VERIFIED | 7 test functions |
| `tools/tre-compare/tests/test_virtual_tree.py` | Full merge suite + end-to-end tombstone tests | ✓ VERIFIED | 16 test functions including all required named tests |
| `tools/tre-compare/tests/test_integration.py` | Exactly 1 env-gated integration test | ✓ VERIFIED | 1 test function; gated on `TRE_COMPARE_INTEGRATION` |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `parser/tre_reader.py` | `parser/tre_decrypt.py` | `from .tre_decrypt import try_read_tre_payload` at line 255 | ✓ WIRED | Rewritten from `swg_pipeline.tre_decrypt`; zero `from swg_pipeline` in src/ |
| `virtual_tree.py` | `tre_compare.parser` | `from .parser import read_tre_entries, ...` + `from .parser.tre_reader import ...` | ✓ WIRED | Both public re-export and submodule import confirmed |
| `virtual_tree.py` | `tre_compare.scanner` | `from .scanner import ScanResult, SearchNode` | ✓ WIRED | Present at module level |
| `tests/test_virtual_tree.py` | `build_virtual_tree` | Direct import + call in all merge tests | ✓ WIRED | `parse_shared_file` + `build_virtual_tree` both called in end-to-end tests |
| `tests/test_integration.py` | `stage/client.cfg` (real install) | env-gated `parse_shared_file(str(REAL_CFG))` | ✓ WIRED (gated) | Guard logic correct; `REAL_CFG = Path(r"D:/Code/swg-client-v2/stage/client.cfg")` |

### Data-Flow Trace (Level 4)

| Artifact | Data Variable | Source | Produces Real Data | Status |
|----------|---------------|--------|-------------------|--------|
| `test_virtual_tree.py` (end-to-end tests) | `vt.entries`, `vt.tombstoned` | `parse_shared_file(cfg)` → `build_virtual_tree(scan)` over genuine on-disk `.tre`/`.toc` byte-built by `_fixtures.py` | Yes — real parser reads genuine binary archives | ✓ FLOWING |
| `virtual_tree.py` `build_virtual_tree` | `claimed`, `tombstoned` | `iter_node_entries` → `read_tre_entries` / `read_search_toc_entries` / `read_cot2000_entries` / `_walk_search_path` | Yes — real archive reads + real fs walk | ✓ FLOWING |
| `scanner.py` `parse_shared_file` | `parsed_nodes` | line-by-line cfg file text read | Yes — real file read + regex parse | ✓ FLOWING |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Full synthetic suite | `cd tools/tre-compare && uv run pytest -m "not integration" -q` | 38 passed, 1 deselected in 0.36s | ✓ PASS |
| Package import | `uv run python -c "import tre_compare; print('import ok')"` | `import ok` | ✓ PASS |
| Zero engine imports in src/ | `grep -rn "from swg_pipeline\|import engine" src/` | No matches (only provenance comments) | ✓ PASS |
| No hardcoded cfg path | `grep -c "stage/client.cfg" src/tre_compare/scanner.py` | 0 | ✓ PASS |
| `claimed.pop` deleted | `grep -n "claimed.pop" src/tre_compare/virtual_tree.py` | No matches | ✓ PASS |
| Integration test gated | Default `uv run pytest -m "not integration"` | 1 deselected (not run) | ✓ PASS |
| Real integration run | `$env:TRE_COMPARE_INTEGRATION=1; uv run pytest -m integration -q` | Not run by verifier — requires human | ? SKIP |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| TRE-01 | 28-01, 28-02, 28-03, 28-04 | User can point the tool at two SWG installations and scan their TRE sets — archives discovered plus each install's cfg search-path order parsed (engine-faithful) | ✓ SATISFIED | `parse_shared_file` + `build_virtual_tree` together deliver engine-faithful scan of any cfg; 38 tests prove behavior; the real-cfg integration test (gated) proves it on the actual install |

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| None found | — | No TODOs, FIXMEs, placeholder returns, hardcoded empty data, or stub patterns observed in the six source modules | — | — |

Notable: `configparser` appears in scanner.py docstring as a explanatory anti-pattern reference (not an import). This is correct and expected.

### Human Verification Required

#### 1. Opt-in Integration Test Against Real Install

**Test:** `cd D:/Code/swg-client-v2/tools/tre-compare` then `$env:TRE_COMPARE_INTEGRATION=1; uv run pytest -m integration -q`

**Expected:** 1 passed (not skipped). The test should assert:
- `scan.nodes[0].priority == 10` and `scan.nodes[0].kind == "path"` (path@10 first)
- `scan.nodes[-1].kind == "toc"` and `scan.nodes[-1].priority == 0` (toc@0 last)
- `len(vt.entries) > 0`
- When the override dir is non-empty: at least one `winner_node.kind == "path"` entry with `priority == 10` wins (SC#3 override-shadowing proof on real cfg)

**Why human:** Cannot run without the SWGSource v3.0 install present and the env var set. The SUMMARY claims "1 passed" on this machine. A human must re-confirm the opt-in run succeeds — this is the SC#3 real-cfg precedence proof for TRE-01.

---

### Gaps Summary

No gaps found. All 10 must-haves are VERIFIED by direct code inspection and the live test run. The single human verification item is a re-confirmation of a previously-passing integration test (the machine environment may have changed since the SUMMARY was written). It does not represent missing implementation — the code, wiring, and gating are all correct.

---

## Detailed Findings

### Plan 01 (Scaffold)

All acceptance criteria satisfied: `pyproject.toml` has `name = "tre-compare"`, `requires-python = ">=3.11"`, `dependencies = []`, `build-backend = "uv_build"`, and the `integration` marker. `.python-version` = `3.11`. `.gitignore` contains `.venv/`, `__pycache__/`, `dist/`. `uv.lock` committed with pytest 9.1.0. `parser/__init__.py` (empty subpackage) and `tests/conftest.py` present.

The plan's acceptance criteria note about `uv_build>=0.11.7` matching `[3-9]` in the anti-forward-pin regex is acknowledged — this is the installed version, not a forward-pin, and `uv build` exit 0 confirmed resolution.

### Plan 02 (Parser Vendoring)

`f803f58782e675e85844960fe868b0849405249a` present in both vendored files. `from .tre_decrypt import try_read_tre_payload` at line 255 of `tre_reader.py`. Zero `from swg_pipeline` anywhere in `src/`. The provenance header's `source:` comment contains the path string but this is explicitly a comment, not an import. `parser/__init__.py` re-exports the full public API with `__all__`.

### Plan 03 (Scanner + Virtual Tree)

`scanner.py`: Both regexes present (`_RE_SKU_KEY` + `_RE_BARE_KEY`); `KIND_RANK` dict used in sort; no `stage/client.cfg` literal; no `configparser` import. `virtual_tree.py`: `fix_up_file_name` verbatim port (interior `..` kept); `safe_virtual_key` with UNC raw-name check before canonicalization (Rule-1 fix from Plan 04); guard `key in claimed or key in tombstoned` at line 368 precedes tree-length-0 branch at line 384; `is_tree_tombstone`/`is_toc_absent` in shadowed logic; `_preflight_tre` + `_preflight_toc` both present; `os.walk(base, followlinks=False)` with `dirnames[:] = sorted(safe_dirs)`. Module docstring states approximation boundary including `searchAbsolute`/`searchCache`.

### Plan 04 (Fixtures + Suite)

`_fixtures.py`: all four builders present; `struct.pack` used; `SEARCH_TOC_ENTRY_FMT`/`fn_field` layout; `toc_entry_stride` for extended-stride padding; `zlib` for compress option. Test count: 38 synthetic + 1 deselected integration = confirmed by live run. All named tests verified by grep. `test_stable_tie_order` absent (correctly removed). End-to-end cfg-driven tests call real `parse_shared_file` + `build_virtual_tree` over purpose-built archives. The UNC safe_virtual_key fix (Rule 1 auto-fix) correctly updated `virtual_tree.py`.

---

_Verified: 2026-06-14T21:30:00Z_
_Verifier: Claude (gsd-verifier)_
