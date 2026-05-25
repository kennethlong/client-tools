---
phase: 8
slug: dead-code-removal-track-b
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-05-07
---

# Phase 8 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CMake build system (cmake --build) + manual boot verify |
| **Config file** | `src/CMakeLists.txt` (root) |
| **Quick run command** | `cmake --build build --config Debug --target <tool> 2>&1 | tail -5` |
| **Full suite command** | `cmake --build build --config Debug 2>&1 | tail -20` |
| **Estimated runtime** | ~2–5 minutes (full graph) |

---

## Sampling Rate

- **After every task commit:** Run `cmake --build build --config Debug --target <current_tool>` — verify tool target compiles without error
- **After every plan wave:** Run `cmake --build build --config Debug` — verify full graph including SwgClient
- **Before `/gsd-verify-work`:** Full suite must produce `SwgClient_d.exe` + all wave tools without linker errors
- **Max feedback latency:** ~300 seconds (full Debug build)

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 8-01-01 | 01 | 1 | CLEAN-06 | — | N/A | build | `cmake --build build --config Debug --target LabelHashTool` | ✅ | ⬜ pending |
| 8-01-02 | 01 | 1 | CLEAN-06 | — | N/A | build | `cmake --build build --config Debug --target StringFileTool` | ✅ | ⬜ pending |
| 8-01-03 | 01 | 1 | CLEAN-06 | — | N/A | build | `cmake --build build --config Debug --target WordCountTool` | ✅ | ⬜ pending |
| 8-02-01 | 02 | 2 | CLEAN-06 | — | N/A | build | `cmake --build build --config Debug --target ParticleEditor` | ✅ | ⬜ pending |
| 8-02-02 | 02 | 2 | CLEAN-06 | — | N/A | build | `cmake --build build --config Debug --target TerrainEditor` | ✅ | ⬜ pending |
| 8-03-01 | 03 | 3 | CLEAN-06 | — | N/A | build | `cmake --build build --config Debug --target SwgGodClient` | ✅ | ⬜ pending |
| 8-04-01 | 04 | 4 | CLEAN-06 | — | N/A | build | `cmake --build build --config Debug` | ✅ | ⬜ pending |
| 8-04-02 | 04 | 4 | CLEAN-06 | — | N/A | manual | Boot SwgClient_d.exe to character select | N/A | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

Existing infrastructure covers all phase requirements. No test framework installation needed — validation is CMake build success + manual boot verify.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| SwgClient_d.exe boots to character select | CLEAN-06 SC-3 | Requires running SWGSource VM at 192.168.1.200; no automated headless path | Launch `build/bin/Debug/SwgClient_d.exe`, verify login UI appears and character select loads |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 300s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
