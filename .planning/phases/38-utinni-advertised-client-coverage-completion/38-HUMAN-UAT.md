---
status: passed
phase: 38-utinni-advertised-client-coverage-completion
source: [38-VERIFICATION.md]
started: 2026-06-22
updated: 2026-06-22
---

## Current Test

[complete — maintainer smoke pass approved 2026-06-22; contract v3 / 94 endpoints, detours fire]

## Tests

### 1. Runtime self-check (v3 contract, 94 endpoints)
expected: Boot `SwgClient_d.exe` under `rasterMajor=5` (gl05) to char-select. Debug log shows `utinni_verifyNoNullNoDup(): PASS (94 rows, 94 required)`.
result: pass (maintainer-approved 2026-06-22)

### 2. D3D11 non-regression
expected: Boot under `rasterMajor=11` (gl11). Window opens to char-select, no crash.
result: pass (maintainer-approved 2026-06-22)

### 3. Live inject-smoke (advertised client, v3)
expected: Inject Utinni → `endpoints: resolved 94/94 by name`, no `0xC0000005`, and the 4 detoured endpoints fire (38-05 real-entry fix). DX11-overlay leg also needs the consumer-side `tryInstall()` fix (Utinni repo, out of scope here).
result: pass (maintainer-approved 2026-06-22)

## Summary

total: 3
passed: 3
issues: 0
pending: 0
skipped: 0
blocked: 0

## Gaps
