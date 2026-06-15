---
status: complete
phase: 29-tre-compare-tool-diff-engine-api
source: [29-VERIFICATION.md]
started: 2026-06-15T03:28:44Z
updated: 2026-06-15T03:55:00Z
---

## Current Test

[all items passed]

## Tests

### 1. SC#4 — Real two-install pair compare (cache HIT proven)
expected: With `TRE_COMPARE_INTEGRATION=1` and both `TRE_COMPARE_LEFT_CFG` / `TRE_COMPARE_RIGHT_CFG` pointing at real install directories, the integration test produces a non-empty tri-state diff (added/removed/changed rows) over the real archive pair, AND the second compare registers a measured cache HIT (the `iter_node_entries` spy proves parse-skip, not merely row equality).
result: PASSED (2026-06-15). Ran against two real, distinct installs present on this machine:
  - LEFT  = SWGSource Client v3.0 — 4 SKU TOCs (`D:/Code/SWGSource Client v3.0/sku{0..3}_client.toc`)
  - RIGHT = SWG Restoration — `D:/SWG Restoration/SwgRestoration.toc`
  Authored two throwaway absolute-path `[SharedFile]` cfgs (same pattern as the canonical
  `stage/client.cfg` REAL_CFG; cfg values are taken verbatim as node abspaths per `scanner.py:162`),
  then removed them. Results:
  - SET-level rows: 5
  - FILE-level rows: 219,109 (identical-by-metadata 99,749 / changed 102,711 / added 10,650 / removed 5,999)
  - 0 node_errors, 0 rejected, 0 tombstoned on both sides (clean parse of ~200k+ real entries)
  - cache: first build 18.09s (MISS/parse) → second build 5.99s (measured HIT, ~3× via parse-skip)
  - `pytest -m integration` → **2 passed** (single-install scan + this pair compare).

Note: the literal "SWGSource-vs-whitengold" pairing is also runnable — whitengold's packed form is
`stage/client.cfg` (SWGSource archives + the priority-10 `stage/override` loose dir + 2 extra `.tre`),
so that diff isolates whitengold's override delta. SWGSource-vs-Restoration was used here as the
richer cross-server demonstration.

## Summary

total: 1
passed: 1
issues: 0
pending: 0
skipped: 0
blocked: 0

## Gaps
