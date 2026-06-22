---
status: partial
phase: 38-utinni-advertised-client-coverage-completion
source: [38-VERIFICATION.md]
started: 2026-06-22
updated: 2026-06-22
---

## Current Test

[awaiting the maintainer's consolidated smoke pass — deferred by the build-gate-autonomous execution choice; all 9 code-level must-haves already verified]

## Tests

### 1. Runtime self-check (v2 contract, 94 endpoints)
expected: Boot `SwgClient_d.exe` under `rasterMajor=5` (gl05) to char-select. Debug log shows `utinni_verifyNoNullNoDup(): PASS (94 rows, 94 required)` — proves all 16 new addresses (8 groundScene + 4 client/config + 4 chatWindow) resolve non-null + no-dup at runtime.
result: [pending]

### 2. D3D11 non-regression
expected: Boot under `rasterMajor=11` (gl11). Window opens to char-select, no crash. Confirms the standalone D3D11 client is not regressed by the new exe-side rows.
result: [pending]

### 3. Live inject-smoke (advertised client, v2)
expected: Inject Utinni into the running client. Log shows `endpoints: resolved 94/94 by name`, no `0xC0000005`. The DX11-overlay leg additionally requires the consumer-side `tryInstall()` poll-until-device+context fix documented in handback §4a (Utinni-side, out of this repo's scope).
result: [pending]

## Summary

total: 3
passed: 0
issues: 0
pending: 3
skipped: 0
blocked: 0

## Gaps
