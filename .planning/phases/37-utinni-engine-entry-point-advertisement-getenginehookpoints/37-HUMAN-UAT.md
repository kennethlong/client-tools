---
status: partial
phase: 37-utinni-engine-entry-point-advertisement-getenginehookpoints
source: [37-VERIFICATION.md]
started: 2026-06-21
updated: 2026-06-21
---

## Current Test

[awaiting human testing — requires Utinni injection into a running client; cannot be verified from this repo]

## Tests

### 1. First-detour crash gone (spec §7 #2)
expected: Inject Utinni into a running `SwgClient_d.exe` (rasterMajor=5), resolve hook points by name via `GetEngineHookPoints()`, and trigger the `config::loadOverrideConfig` detour. No `0xC0000005 target=0x00401000` first-detour crash — the name-resolved address is correct where the hardcoded SWGEmu RVA was wrong.
result: [pending]

### 2. SWGEmu no-regression (spec §7 #4)
expected: Run the same Utinni overlay against an SWGEmu Pre-CU client. No behavior change — the new `GetEngineHookPoints` export is purely additive and inert when not consumed, so SWGEmu pathways are unaffected.
result: [pending]

### 3. DX11 overlay renders off the contract (spec §7 #5)
expected: Inject Utinni + ImGui overlay under rasterMajor=11 (gl11/D3D11). Overlay renders using name-resolved engine hook points (`graphics::install` and the graphics group), confirming EPA-03 kickoff works off the contract rather than hardcoded addresses.
result: [pending]

## Summary

total: 3
passed: 0
issues: 0
pending: 3
skipped: 0
blocked: 0

## Gaps
