---
status: partial
phase: 29-tre-compare-tool-diff-engine-api
source: [29-VERIFICATION.md]
started: 2026-06-15T03:28:44Z
updated: 2026-06-15T03:28:44Z
---

## Current Test

[awaiting human testing]

## Tests

### 1. SC#4 — Real SWGSource-vs-whitengold install-pair compare (cache HIT proven)
expected: With `TRE_COMPARE_INTEGRATION=1` and both `TRE_COMPARE_LEFT_CFG` / `TRE_COMPARE_RIGHT_CFG` pointing at real install directories, the integration test produces a non-empty tri-state diff (added/removed/changed rows) over the real archive pair, AND the second compare registers a measured cache HIT (the `iter_node_entries` spy proves parse-skip, not merely row equality). Requires the actual SWG archive files, which are not present on the build machine, so automated CI skips it.
result: [pending]

Run:
```
cd tools/tre-compare
TRE_COMPARE_INTEGRATION=1 \
  TRE_COMPARE_LEFT_CFG=<path-to-left-install-cfg> \
  TRE_COMPARE_RIGHT_CFG=<path-to-right-install-cfg> \
  uv run pytest -m integration -q
```

## Summary

total: 1
passed: 0
issues: 0
pending: 1
skipped: 0
blocked: 0

## Gaps
