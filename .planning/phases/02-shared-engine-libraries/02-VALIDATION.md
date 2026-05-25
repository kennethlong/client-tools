---
phase: 2
slug: shared-engine-libraries
status: approved
nyquist_compliant: true
wave_0_complete: false
created: 2026-05-03
---

# Phase 2 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CMake build verification (no test framework — builds are the test) |
| **Config file** | none — CMake is self-contained |
| **Quick run command** | `cmake --build build --config Debug --target <lib> --parallel 4` |
| **Full suite command** | `cmake --build build --config Debug --parallel 8` |
| **Estimated runtime** | ~2–5 minutes (full graph) |

---

## Sampling Rate

- **After every task commit:** Run `cmake --build build --config Debug --target <lib> --parallel 4` for the lib just authored
- **After every plan wave:** Run `cmake --build build --config Debug --parallel 8` for the full wave set
- **Before `/gsd-verify-work`:** Full suite must be green AND 3 consecutive parallel builds pass
- **Max feedback latency:** ~300 seconds (full graph build)

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| time_t-probe | W0 | 0 | SC-5 | — | N/A — build-system only | build | `cmake --build build --config Debug --target sharedDebug --parallel 4` | ❌ W0 | ⬜ pending |
| stubs | W0 | 0 | SC-3 | — | N/A | build | `cmake -S src -B build -G "Visual Studio 17 2022" -A Win32 -T v143,host=x64` | ❌ W0 | ⬜ pending |
| swgSharedUtility | W1 | 1 | BUILD-03 | — | N/A | build | `cmake --build build --config Debug --target swgSharedUtility --parallel 4` | ❌ W0 | ⬜ pending |
| sharedDebug | W1 | 1 | BUILD-03 | — | N/A | build | `cmake --build build --config Debug --target sharedDebug --parallel 4` | ❌ W0 | ⬜ pending |
| sharedThread | W1 | 1 | BUILD-03 | — | N/A | build | `cmake --build build --config Debug --target sharedThread --parallel 4` | ❌ W0 | ⬜ pending |
| sharedSynchronization | W1 | 1 | BUILD-03 | — | N/A | build | `cmake --build build --config Debug --target sharedSynchronization --parallel 4` | ❌ W0 | ⬜ pending |
| sharedMath | W1 | 1 | BUILD-03 | — | N/A | build | `cmake --build build --config Debug --target sharedMath --parallel 4` | ❌ W0 | ⬜ pending |
| sharedMathArchive | W1 | 1 | BUILD-03 | — | N/A | build | `cmake --build build --config Debug --target sharedMathArchive --parallel 4` | ❌ W0 | ⬜ pending |
| sharedFoundation | W2 | 2 | BUILD-03/SC-5 | — | N/A | build | `cmake --build build --config Debug --target sharedFoundation --parallel 4` | ❌ W0 | ⬜ pending |
| sharedFile | W2 | 2 | BUILD-03 | — | N/A | build | `cmake --build build --config Debug --target sharedFile --parallel 4` | ❌ W0 | ⬜ pending |
| sharedCompression | W3 | 3 | BUILD-03 | — | N/A | build | `cmake --build build --config Debug --target sharedCompression --parallel 4` | ❌ W0 | ⬜ pending |
| sharedRandom | W3 | 3 | BUILD-03 | — | N/A | build | `cmake --build build --config Debug --target sharedRandom --parallel 4` | ❌ W0 | ⬜ pending |
| sharedRegex | W3 | 3 | BUILD-03 | — | N/A | build | `cmake --build build --config Debug --target sharedRegex --parallel 4` | ❌ W0 | ⬜ pending |
| sharedImage | W3 | 3 | BUILD-03 | — | N/A | build | `cmake --build build --config Debug --target sharedImage --parallel 4` | ❌ W0 | ⬜ pending |
| sharedLog | W3 | 3 | BUILD-03 | — | N/A | build | `cmake --build build --config Debug --target sharedLog --parallel 4` | ❌ W0 | ⬜ pending |
| sharedXml | W3 | 3 | BUILD-03 | — | N/A | build | `cmake --build build --config Debug --target sharedXml --parallel 4` | ❌ W0 | ⬜ pending |
| sharedIoWin | W3 | 3 | BUILD-03 | — | N/A | build | `cmake --build build --config Debug --target sharedIoWin --parallel 4` | ❌ W0 | ⬜ pending |
| sharedMessageDispatch | W3 | 3 | BUILD-03 | — | N/A | build | `cmake --build build --config Debug --target sharedMessageDispatch --parallel 4` | ❌ W0 | ⬜ pending |
| sharedMemoryManager | W3 | 3 | BUILD-03 | — | N/A | build | `cmake --build build --config Debug --target sharedMemoryManager --parallel 4` | ❌ W0 | ⬜ pending |
| sharedNetwork | W4 | 4 | BUILD-03/SC-5 | — | N/A | build | `cmake --build build --config Debug --target sharedNetwork --parallel 4` | ❌ W0 | ⬜ pending |
| sharedNetworkMessages | W4 | 4 | BUILD-03 | — | N/A | build | `cmake --build build --config Debug --target sharedNetworkMessages --parallel 4` | ❌ W0 | ⬜ pending |
| sharedUtility | W4 | 4 | BUILD-03 | — | N/A | build | `cmake --build build --config Debug --target sharedUtility --parallel 4` | ❌ W0 | ⬜ pending |
| sharedObject | W4 | 4 | BUILD-03 | — | N/A | build | `cmake --build build --config Debug --target sharedObject --parallel 4` | ❌ W0 | ⬜ pending |
| sharedTerrain | W4 | 4 | BUILD-03 | — | N/A | build | `cmake --build build --config Debug --target sharedTerrain --parallel 4` | ❌ W0 | ⬜ pending |
| sharedPathfinding | W4 | 4 | BUILD-03 | — | N/A | build | `cmake --build build --config Debug --target sharedPathfinding --parallel 4` | ❌ W0 | ⬜ pending |
| sharedGame | W4 | 4 | BUILD-03 | — | N/A | build | `cmake --build build --config Debug --target sharedGame --parallel 4` | ❌ W0 | ⬜ pending |
| full-debug | Final | 4 | SC-1/SC-2/SC-3/SC-4 | — | N/A | build | `cmake --build build --config Debug --parallel 8` (×3) | depends | ⬜ pending |
| full-release | Final | 4 | SC-2 | — | N/A | build | `cmake --build build --config Release --parallel 8` | depends | ⬜ pending |
| lib-count | Final | 4 | SC-1 | — | N/A | verify | `dir build\lib\Debug\*.lib \| find /c ".lib"` returns 24 | depends | ⬜ pending |
| time_t-verify | Final | 4 | SC-5 | — | N/A | build | All 3 probe targets compile without static_assert failure | depends | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `src/cmake/stubs/` directory created
- [ ] `src/cmake/stubs/time_t_probe.cpp.in` — template for SC-5 time_t static_assert probe
- [ ] `engine/shared/library/CMakeLists.txt` updated with INTERFACE stubs for sharedCollision, sharedFractal, sharedSkillSystem — required before sharedObject/sharedTerrain configure
- [ ] `swgSharedUtility` CMakeLists.txt authored — required before sharedObject/sharedNetworkMessages/sharedGame configure

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| 3 consecutive parallel builds pass deterministically | SC-4 | No CI; requires human observation of 3 runs | Run `cmake --build build --config Debug --parallel 8` three times; verify no intermittent C2065/C1010 errors |
| ARTIF-03 CRT verification | SC-2 cross-check | Requires dumpbin | `dumpbin /headers build\lib\Debug\sharedDebug.lib \| findstr "MT"` — confirm `/MTd` |
| time_t probe pass confirmation | SC-5 | Build output must be visually confirmed error-free | Build sharedDebug/sharedFoundation/sharedNetwork; confirm no `static_assert` failure in output |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references (probe template, stubs, swgSharedUtility)
- [x] No watch-mode flags
- [x] Feedback latency < 300s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** approved 2026-05-03
