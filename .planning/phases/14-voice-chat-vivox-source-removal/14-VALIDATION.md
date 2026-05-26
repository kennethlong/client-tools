---
phase: 14
slug: voice-chat-vivox-source-removal
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-05-25
---

# Phase 14 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> This is a **source-removal** phase: validation is grep-zero + Debug/Release
> link-grep + dual-renderer boot, NOT a unit-test suite. There is no pytest/jest
> harness — the "tests" are deterministic CLI greps and a manual boot gate.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | none — C++ build + ripgrep + manual boot gate |
| **Config file** | `src/build/win32/swg.sln` (MSBuild; msbuild NOT on PATH — use full path) |
| **Quick run command** | `rg -i "vivox" src/ --glob "*.rsp" --glob "*.vcxproj" -c` (expect 0 hits) |
| **Full suite command** | MSBuild Debug + Release, then grep link output for `unresolved external symbol` (expect 0); then dual-renderer boot |
| **Estimated runtime** | grep < 5s · full Debug+Release build ~minutes · boot gate manual |

---

## Sampling Rate

- **After every task commit:** Run the scoped grep for the symbols/refs that task removed (expect 0 new hits in the touched area) and confirm the affected library still compiles.
- **After every plan wave:** Build the affected libraries (and SwgClient where linked) Debug; grep link output for `unresolved external symbol` == 0.
- **Before `/gsd:verify-work`:** Full criterion sweep — criterion #1 grep-zero (`vivox`/`Vivox` across `.rsp`/source/include), criterion #2 symbol grep-zero, criterion #4 Debug **and** Release link clean (grep, not just exit 0), criterion #5 dual-renderer boot.
- **Max feedback latency:** grep gates < 5s; build gate = single-config build time.

---

## Per-Task Verification Map

> Populated by the planner per task. Each removal/de-wire task's automated check is
> a scoped ripgrep (symbol/ref count == 0 after removal) plus the affected library
> compiling. Link-grep and boot are wave/phase-level gates, not per-task.

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| {N}-01-01 | 01 | 1 | DECRUFT-05 | — | N/A (dead-code removal) | grep | `rg -i "vivox\|CuiVoiceChat\|SwgCuiVoice" <touched-paths> -c` (== 0) | n/a | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

*None — this is a removal phase with no new test infrastructure. Validation reuses the
existing MSBuild + ripgrep + manual boot toolchain established in Phases 12–13.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Criterion #1 — grep-zero `vivox`/`Vivox` across `.rsp`, source, include paths | DECRUFT-05 | CLI grep, no test harness | `rg -i "vivox" src/` returns 0 matches |
| Criterion #2 — voice-symbol grep-zero (`CuiVoiceChat*`, `SwgCuiVoice*`, `VOICE_INVITE`, `voiceInvite/Kick`, `setVoice*`, `ms_voice*`) | DECRUFT-05 | CLI grep | `rg "CuiVoiceChat\|SwgCuiVoice\|VOICE_INVITE\|voiceInvite\|voiceKick\|ms_voice" src/` returns 0 matches |
| Criterion #4 — Debug **and** Release link clean | DECRUFT-05 | `/FORCE` downgrades unresolved externals to warnings + still emits a binary (Phase 12/13 finding); must grep link log, not trust exit 0 | Build each config; `Select-String "unresolved external symbol"` over the link output returns 0 |
| Criterion #5 — dual-renderer boot to character select | DECRUFT-07 | Requires the SWGSource VM + live client launch; not scriptable | Set `rasterMajor=5` then `=11` in `client_d.cfg` (debug exe reads `client_d.cfg`); boot to char-select under each |

---

## Validation Sign-Off

- [ ] Every removal/de-wire task has a scoped grep-zero automated check + affected library compiles
- [ ] Sampling continuity: no wave leaves the tree un-buildable (D-02a)
- [ ] Wave 0 N/A — existing toolchain covers all checks
- [ ] No watch-mode flags
- [ ] Link gate greps for `unresolved external symbol` (Debug AND Release), not just MSBuild exit 0 (D-09)
- [ ] `nyquist_compliant: true` set in frontmatter once the planner's per-task map is complete

**Approval:** pending
