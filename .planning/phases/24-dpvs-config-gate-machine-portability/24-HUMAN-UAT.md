---
status: passed
phase: 24-dpvs-config-gate-machine-portability
source: [24-VERIFICATION.md]
started: 2026-06-13T02:15:43Z
updated: 2026-06-13T02:44:27Z
---

## Current Test

[complete — user approved all 4 runtime checks 2026-06-13]

## Tests

### 1. In-game DPVS occlusion mode (HARD-01)
expected: With `[ClientGraphics/Dpvs] occlusionMode=auto`, walk Mos Eisley plaza → cantina with the Phase-23 DPVS DebugMonitor overlay open. Occlusion bit is SET outdoors (visible-object signal ~359) and CLEAR inside the cantina (~443). Press F11 → force-disables occlusion regardless of mode (D-04). Set `occlusionMode=on` → bit unconditional; `occlusionMode=off` (or no key, Debug) → bit clear (D-02).
result: passed (user approved 2026-06-13)

### 2. Dual-renderer boot gate (HARD-01 / regression)
expected: `rasterMajor=11` in `stage/client_d.cfg` → `SwgClient_d.exe` boots to character select under D3D11 (skinned chars render via multistream-VB flip, no Bloom FATAL). Then `rasterMajor=5` → boots to character select under D3D9. Both clean. Restore `rasterMajor=11` when done.
result: passed (user approved 2026-06-13)

### 3. Miles audio from vendored redist (PORT-01)
expected: Client launches with audio (music + in-world). Failure-inject: rename `stage/miles` away → relaunch → exactly ONE startup WARNING about absent codec providers and ZERO per-sample warning flood (UI 2D wavs may still play). Restore `stage/miles`.
result: passed (user approved 2026-06-13)

### 4. Generated cfg quality + fresh-clone (PORT-01 / PORT-02 / D-10)
expected: Run `tools/setup/setup-client.ps1` (supply TRE root e.g. `D:/Code/SWGSource Client v3.0/`, accept default login server). It writes both cfgs with the clone's own `stage/override` path + a next-steps banner. Inspect generated `stage/client_d.cfg`: no dead keys (swg_dev_bundle, disableMultiStreamVertexBuffers, [ClientGame/Bloom] enable=0, disableG15Lcd, voiceChatEnabled, reportFrameStats), no machine-specific absolute path beyond the supplied TRE root, login is the `@LOGIN_SERVER@` value. Build + boot on both renderers from a fresh clone.

V-key verdicts to record (regress → template must restore the key with a comment):
- `runtimeDisableAsynchronousLoader` (removed)
- `skipSplash` (now true)
- `swg_dev_bundle` (absent)
result: passed (user approved 2026-06-13)

## Summary

total: 4
passed: 4
issues: 0
pending: 0
skipped: 0
blocked: 0

## Gaps
