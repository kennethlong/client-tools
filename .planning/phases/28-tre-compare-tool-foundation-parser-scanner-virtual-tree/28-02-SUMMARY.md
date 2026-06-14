---
phase: 28-tre-compare-tool-foundation-parser-scanner-virtual-tree
plan: 02
subsystem: tooling
tags: [python, parser, tre, toc, cot2000, vendoring, pytest, tre-compare]

# Dependency graph
requires:
  - "28-01: isolated uv library scaffold (src layout, empty parser/ subpackage, pytest test root, zero runtime deps)"
provides:
  - "Vendored, owned, stdlib-only TRE/TOC/COT2000 parser at tools/tre-compare/src/tre_compare/parser/ (D-03)"
  - "Public parser API re-exported from parser/__init__.py (SUPPORTED_TRE_VERSIONS, TreEntry/Cot2000Entry/SearchTocEntry, read_*/parse_* fns, exceptions)"
  - "Zero swg_pipeline/engine imports — package is fully copy-paste extractable (D-01)"
  - "Provenance-pinned vendored files (source path + commit f803f58782e675e85844960fe868b0849405249a)"
  - "Dual-length data model on every entry type (length + compressed_length, snake_case) for Phase-29 changed-detection"
  - "Parser import-surface + data-model smoke test (tests/test_parser.py, 7 passing)"
affects: [28-03-scanner-virtual-tree, 28-04-fixtures-integration, phase-29-diff-api, phase-30-web-ui]

# Tech tracking
tech-stack:
  added: []
  patterns: ["vendor-copy-and-own with provenance header (D-03)", "single-import-rewrite for extractability (.tre_decrypt sibling)", "stdlib-only zero-runtime-dep discipline (D-01)", "dataclasses.fields() data-model contract test"]

key-files:
  created:
    - tools/tre-compare/src/tre_compare/parser/tre_reader.py
    - tools/tre-compare/src/tre_compare/parser/tre_decrypt.py
    - tools/tre-compare/tests/test_parser.py
  modified:
    - tools/tre-compare/src/tre_compare/parser/__init__.py

key-decisions:
  - "Vendored from swg-blender-plugin master HEAD (f803f58782e675e85844960fe868b0849405249a) — the commit the plan pinned and verified this session; the last content-touching commit on the two files was dd8a10806 (2026-05-31), so HEAD == content state"
  - "Verbatim copy + exactly two edits per file (provenance header + the one import rewrite); NO refactor on vendor, NO dual-length wrapper — the source dataclasses already expose snake_case length + compressed_length (review finding #5 confirmed against live source)"
  - "Provenance header source: comment intentionally contains the literal path .../swg_pipeline/...; the zero-cross-package guarantee is about IMPORTS — grep 'from swg_pipeline' returns 0; this is the agreed D-03 provenance record, not a live link"

patterns-established:
  - "Vendor-copy-and-own (D-03): each vendored file carries a 4-line provenance header (source path + commit + ownership note); only permitted edits are the header and the cross-package import rewrite"
  - "Extractability (D-01): the single in-function cross-package import (read_tre_payload zlib fallback) rewritten swg_pipeline.tre_decrypt -> .tre_decrypt; src/ has zero swg_pipeline/engine imports"
  - "Data-model contract test via dataclasses.fields() — asserts both length fields present and the camelCase spelling absent"

requirements-completed: [TRE-01]

# Metrics
duration: ~6min
completed: 2026-06-14
---

# Phase 28 Plan 02: Vendor TRE/TOC Parser Summary

**Vendored `tre_reader.py` + `tre_decrypt.py` verbatim from swg-blender-plugin (commit `f803f587…`) into `tools/tre-compare/src/tre_compare/parser/` with provenance headers and the single mandatory import rewrite (`swg_pipeline.tre_decrypt` → `.tre_decrypt`) — a stdlib-only, engine-decoupled, fully-extractable parser subpackage recognizing all five TREE variants (0004/0005/6000/0006/5000) plus COT2000 + SearchTOC, with both `length` and `compressed_length` on every entry type, proven by a green `tests/test_parser.py` (7 passed).**

## Performance

- **Duration:** ~6 min
- **Completed:** 2026-06-14T19:29Z
- **Tasks:** 2
- **Files created:** 3 (tre_reader.py, tre_decrypt.py, test_parser.py)
- **Files modified:** 1 (parser/__init__.py)

## Accomplishments

- **Vendored both parser files (D-03):** `tre_reader.py` (471 → 475 lines incl. header) and `tre_decrypt.py` copied byte-for-byte from `D:/Code/swg-blender-plugin/swg_pipeline/`, each prepended with the 4-line provenance header pinning the source path + commit `f803f58782e675e85844960fe868b0849405249a`.
- **The one mandatory import rewrite (D-01 extractability):** inside `read_tre_payload`'s zlib-error fallback, `from swg_pipeline.tre_decrypt import try_read_tre_payload` → `from .tre_decrypt import try_read_tre_payload`. This is the only line that referenced the old package; the package is now fully copy-paste extractable.
- **Public API re-exported** from `parser/__init__.py` (with `__all__`): `SUPPORTED_TRE_VERSIONS`, `TreEntry`/`Cot2000Entry`/`SearchTocEntry`, `read_tre_header`/`read_tre_entries`/`list_tre`/`read_tre_payload`, `detect_master_index_kind`/`parse_master_index`, `read_cot2000_entries`/`read_search_toc_entries`, `toc_entry_stride`, `UnsupportedTreVersionError`/`TreFormatError`. Every name verified to exist in the vendored module before export.
- **Dual-length data model confirmed (no wrapper needed):** the vendored `TreEntry`, `Cot2000Entry`, and `SearchTocEntry` dataclasses already declare snake_case `length` AND `compressed_length` verbatim — review finding #5 holds, so the "verbatim + exactly two edits" instruction stands with no alias/wrapper additions.
- **Smoke test green (7 passed):** `test_parser_public_api` (all symbols import + callable + SUPPORTED == all five TREE variants), `test_entry_dataclasses_expose_both_lengths` (dataclasses.fields() proves both length fields present, camelCase `compressedLength` absent), `test_supported_version_tags_individually` (parametrized per-tag × 5).
- **Stdlib-only preserved:** `tre_reader.py` imports `shutil`/`struct`/`zlib`/`dataclasses`/`enum`/`pathlib` + `from __future__`; `tre_decrypt.py` imports `zlib`/`dataclasses` + `from __future__`. Zero third-party imports — D-01 zero-runtime-dep guarantee intact.

## Task Commits

1. **Task 1: Vendor tre_reader.py + tre_decrypt.py with provenance + import rewrite** — `f222dc876` (feat)
2. **Task 2: Parser import-surface + data-model smoke test** — `de4f3f64d` (test)

**Plan metadata** (this SUMMARY + STATE/ROADMAP/REQUIREMENTS) committed separately.

## Files Created/Modified

- `tools/tre-compare/src/tre_compare/parser/tre_reader.py` — vendored version-aware TRE/TOC/COT2000 reader; provenance header + line-251 import rewrite; all other content verbatim.
- `tools/tre-compare/src/tre_compare/parser/tre_decrypt.py` — vendored decrypt/deobfuscation helper (`try_read_tre_payload`, `PayloadReadResult`); provenance header, no code edits (no cross-package import to rewrite).
- `tools/tre-compare/src/tre_compare/parser/__init__.py` — re-exports the public API with `__all__` (was an empty Plan-01 placeholder docstring).
- `tools/tre-compare/tests/test_parser.py` — 3 test functions (7 parametrized cases) asserting import surface + dual-length data model.

## Decisions Made

- **Provenance commit pinning:** the plan instructed pinning to commit `f803f58782e675e85844960fe868b0849405249a` (verified as swg-blender-plugin master HEAD this session). The last commit to *touch the content* of these two files was `dd8a10806` (2026-05-31); HEAD `f803f587` is the current tree state of master, so the pinned hash correctly identifies "the master commit the bytes were taken from." Recorded the plan-specified hash in both headers, matching the acceptance grep.
- **No dual-length wrapper:** confirmed by reading the live source that all three entry dataclasses already expose snake_case `length` + `compressed_length` — added no alias/wrapper (would have violated "verbatim + exactly two edits"). The test asserts the contract via `dataclasses.fields()`.
- **Provenance `source:` path comment vs. extractability grep:** the header `source:` line contains the literal string `swg_pipeline/...`. The D-01/zero-cross-package guarantee is about *imports*, not comments — `grep 'from swg_pipeline'` returns 0 and `grep 'import swg_pipeline'` returns 0. The path comment is the required D-03 provenance record. See Acceptance Criteria Note below.

## Deviations from Plan

None — plan executed exactly as written. The two permitted edits per file (provenance header + the single import rewrite) were applied; no behavioral changes, no refactor, no wrapper fields.

## Acceptance Criteria Note

The plan's blanket criterion `grep -rn 'swg_pipeline\|src/engine\|import engine' tools/tre-compare/src/` "returns no matches" technically matches the two provenance-header `source:` comment lines (`.../swg_pipeline/tre_reader.py` and `.../tre_decrypt.py`). These are **comments, not imports** — they are the D-03-mandated provenance record (the plan's own header template writes that exact path). The criterion's *intent* (the must_haves wording) is "ZERO imports from swg_pipeline or the engine repo," which is fully satisfied:

- `grep -c 'from swg_pipeline' tools/tre-compare/src/tre_compare/parser/tre_reader.py` → `0`
- `grep -rn 'from swg_pipeline\|import swg_pipeline' tools/tre-compare/src/` → no matches (PASS)

The import-rewrite (`from .tre_decrypt import`) and both provenance grep checks all pass exactly as specified.

## Threat Model Notes

- **T-28-02-01 (DoS, malformed header fields):** disposition `mitigate` — the hostile-input bounds preflight lands in **Plan 03** at the merge boundary (T-28-03-04). Plan 02 added **no new unbounded reads** beyond what the vendored source already does; the parser is left unmodified (D-03). Per review finding #6, this is the correct seam.
- **T-28-02-02 (decompression bomb in `read_tre_payload`):** disposition `accept (this phase)` — `read_tre_payload` is a Phase-29-only drill-in path NOT invoked by the Phase-28 merge (which reads TOC metadata only, RESEARCH A3). Flagged for the Phase-29 plan to cap decompressed size against declared `length`.
- **T-28-02-03 (vendor drift):** `mitigate` — provenance header pins source + commit; the only edits made are the header and the one import rewrite, grep-asserted, with zero `swg_pipeline` import residue.

## Known Stubs

None. The vendored parser is fully functional code (the source is proven against real archives in swg-blender-plugin). Synthetic-fixture-backed byte-parse round-trip tests are scoped to Plan 04 by design, not stubbed here.

## Next Phase Readiness

- **Plan 03 (scanner + virtual-tree)** is unblocked: `from tre_compare.parser import read_tre_entries, read_search_toc_entries, read_cot2000_entries, ...` resolves cleanly; the entry data model (path + length + compressed_length + offset + compressor) is the merge input. Plan 03 also owns the T-28-02-01 hostile-header bounds preflight at the merge boundary.
- **Plan 04 (fixtures + integration)** can build byte-level synthetic TRE/TOC/COT2000 archives and assert full parse round-trips per version tag against this parser.
- **Phase 29 (diff/API)** has the forward-compat contract locked: `(length, compressed_length)` is the cheap changed-signal on every entry type, no re-parse needed.

## Self-Check: PASSED

- Created files verified present: `tre_reader.py`, `tre_decrypt.py`, `tests/test_parser.py`, and modified `parser/__init__.py` (all on disk).
- Commits verified in git log: `f222dc876` (Task 1), `de4f3f64d` (Task 2).
- `uv run pytest tests/test_parser.py -x -q` → 7 passed.

---
*Phase: 28-tre-compare-tool-foundation-parser-scanner-virtual-tree*
*Completed: 2026-06-14*
