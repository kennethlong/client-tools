---
phase: 4
slug: swgclient-executable-link
status: draft
nyquist_compliant: true
wave_0_complete: false
created: 2026-05-04
---

# Phase 4 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | None — no test framework present (CLAUDE.md confirms; swgclient-build.md: "Not detected") |
| **Config file** | N/A — build-system phase only |
| **Quick run command** | `cmake --build build --config Debug --target SwgClient` |
| **Full suite command** | `cmake --build build --config Debug && cmake --build build --config Release` |
| **Estimated runtime** | ~5–15 minutes (incremental), ~30–45 minutes (clean) |

---

## Sampling Rate

- **After every task commit:** Run `cmake --build build --config Debug --target SwgClient`
- **After every plan wave:** Debug + Release both succeed; `dumpbin /imports` check passes
- **Before `/gsd-verify-work`:** Three consecutive clean parallel builds must be green (SC-6)
- **Max feedback latency:** ~15 minutes (incremental build)

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 4-01-01 | 01 (libEverQuestTCG) | 1 | BUILD-04 | — | N/A | Build | `cmake --build build --config Debug --target libEverQuestTCG` | N/A — build artifact | ⬜ pending |
| 4-02-01 | 02 (swgSharedNetworkMessages) | 1 | BUILD-04 | — | N/A | Build | `cmake --build build --config Debug --target swgSharedNetworkMessages` | N/A — build artifact | ⬜ pending |
| 4-03-01 | 03 (swgClientUserInterface) | 2 | BUILD-04 | — | N/A | Build | `cmake --build build --config Debug --target swgClientUserInterface` | N/A — build artifact | ⬜ pending |
| 4-04-01 | 04 (SwgClient exe) | 3 | BUILD-04, ARTIF-01 | — | N/A | Build gate | `cmake --build build --config Debug --target SwgClient` | N/A | ⬜ pending |
| 4-05-01 | 05 (POST_BUILD + gates) | 4 | ARTIF-01, ARTIF-02 | — | N/A | Build gate | `cmake --build build --config Debug && cmake --build build --config Release` | N/A | ⬜ pending |
| 4-05-02 | 05 (POST_BUILD + gates) | 4 | ARTIF-03 | — | No MSVCR*/xpcom in imports | Manual | `dumpbin /imports build/bin/Debug/SwgClient_d.exe \| findstr -i "msvcr vcruntime xpcom xul"` (must be empty) | N/A | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

No traditional test files needed — this is a build-system phase. All validation is via build artifacts and dumpbin inspection.

*Existing infrastructure (CMake + dumpbin) covers all phase requirements.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Zero MSVCR*/VCRUNTIME*/MSVCRTD.dll in imports | ARTIF-03 | dumpbin CLI, no automated harness | `dumpbin /imports bin/Debug/SwgClient_d.exe \| findstr -i "msvcr vcruntime msvcrtd"` must return empty |
| Zero xpcom.dll / xul.dll in imports | ARTIF-03 | dumpbin CLI | `dumpbin /imports bin/Debug/SwgClient_d.exe \| findstr -i "xpcom xul"` must return empty |
| Three consecutive clean parallel builds pass | SC-6 | Manual timing required | `cmake --build build --config Debug --clean-first --parallel 8` × 3 |
| DLLs staged alongside exe | SC-5 | Filesystem check | `dir bin\Debug\Mss32.dll bin\Debug\binkw32.dll bin\Debug\dpvsd.dll` all exist |
| placeholder client.cfg staged | D-05 | Filesystem check | `type bin\Debug\client.cfg` shows commented searchPath stubs, no live credentials |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 15 minutes
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
