---
phase: 3
slug: client-engine-libraries-sdk-heavy-tier
status: draft
nyquist_compliant: true
wave_0_complete: false
created: 2026-05-04
---

# Phase 3 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CMake build verification (no unit-test framework in this codebase) |
| **Config file** | `src/CMakeLists.txt` (root) |
| **Quick run command** | `cmake --build D:/Code/swg-client/build --config Debug --target clientDirectInput -- /m:4` |
| **Full suite command** | `cmake --build D:/Code/swg-client/build --config Debug --parallel 8` (then `--config Release`) |
| **Estimated runtime** | ~5–15 min per full parallel build (depending on target set) |

---

## Sampling Rate

- **After every task commit:** `cmake --build build --config Debug --target <newlib> -- /m:4`
- **After every plan wave (post-Tier 7, post-Tier 9):** Full parallel Debug build of all Phase 3 targets so far
- **Before `/gsd-verify-work`:** 3x consecutive `cmake --build build --config Debug --parallel 8` (SC-4 gate)
- **Max feedback latency:** ~5 min (single-target quick build)

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 3-01-01 | 01 | 0 | LAUNCH-02 | — | libMozilla stub builds without XPCOM headers | build | `cmake --build build --config Debug --target libMozilla -- /m:1` | Wave 0 gap | ⬜ pending |
| 3-01-02 | 01 | 0 | BUILD-03 | — | ui static lib builds (126 .cpp) | build | `cmake --build build --config Debug --target ui -- /m:4` | Wave 0 gap | ⬜ pending |
| 3-02-01 | 02 | 1 | BUILD-03 | — | Tier 6 libs produce .lib | build | `cmake --build build --config Debug --target clientAnimation clientObject clientTextureRenderer clientDirectInput clientBugReporting clientParticle -- /m:4` | Wave 0 gap | ⬜ pending |
| 3-03-01 | 03 | 2 | BUILD-03 | — | clientGraphics produces .lib (DX9 + DPVS) | build | `cmake --build build --config Debug --target clientGraphics -- /m:1` | Wave 0 gap | ⬜ pending |
| 3-03-02 | 03 | 2 | BUILD-03 | — | clientAudio produces .lib (Miles) | build | `cmake --build build --config Debug --target clientAudio -- /m:1` | Wave 0 gap | ⬜ pending |
| 3-04-01 | 04 | 3 | BUILD-03 | — | Tier 8+9 libs produce .lib | build | `cmake --build build --config Debug --target clientSkeletalAnimation clientTerrain clientUserInterface -- /m:4` | Wave 0 gap | ⬜ pending |
| 3-04-02 | 04 | 3 | LAUNCH-02 | — | XPCOM symbol absent from clientUserInterface.lib | symbol check | `powershell -Command "dumpbin /symbols build/lib/Debug/clientUserInterface.lib | Select-String xpcom"` | Wave 0 gap | ⬜ pending |
| 3-05-01 | 05 | 4 | BUILD-03 | — | clientGame produces .lib (343 .cpp) | build | `cmake --build build --config Debug --target clientGame -- /m:8` | Wave 0 gap | ⬜ pending |
| 3-SC-4 | 05 | 4 | BUILD-03 | — | 3x consecutive parallel Debug builds deterministic | determinism | `cmake --build build --config Debug --parallel 8` (×3) | Wave 0 gap | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [x] `src/cmake/stubs/libMozilla_stub.cpp` created — stub for 8 libMozilla public API functions (Plan 03-01 Task 1)
- [x] `engine/client/library/CMakeLists.txt` declares `libMozilla` inline target (Pattern: Phase 2 INTERFACE stub inline declaration) (Plan 03-01 Task 2)
- [x] `external/3rd/library/CMakeLists.txt` adds `ui` subdirectory entry (BEFORE engine/client processing order) (Plan 03-01 Task 3)
- [ ] Phase 3 SC verification script (PowerShell) covering SC-1 through SC-5 — see 03-05-PLAN.md checkpoint task

*No test framework installation required — pure CMake build verification.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| SC-1: 13 .lib files present in Debug build output | BUILD-03 SC-1 | `dir` output requires human inspection of lib names | `dir D:\Code\swg-client\build\lib\Debug\client*.lib` — count and verify 13 entries (clientAnimation, clientAudio, clientBugReporting, clientDirectInput, clientGame, clientGraphics, clientObject, clientParticle, clientSkeletalAnimation, clientTerrain, clientTextureRenderer, clientUserInterface + libMozilla) |
| SC-2: Release config produces same .lib set | BUILD-03 SC-2 | Requires Release build trigger + inspection | `cmake --build build --config Release --parallel 8`, then `dir build\lib\Release\client*.lib` |
| SC-3: First DX9 .cpp compile succeeds in clientGraphics | BUILD-03 SC-3 | DX9 header conflict risk (D-13) requires compile-output inspection | Build clientGraphics single-target, check no `C1083`/`C2065` on `d3d9.h` includes |
| SC-5: XPCOM symbols absent from clientUserInterface.lib | LAUNCH-02 | Symbol table must be visually verified | PowerShell: `dumpbin /symbols build\lib\Debug\clientUserInterface.lib \| Select-String xpcom,xul,nspr4,plc4,profdirserviceprovider` — must return empty |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 15 min (single-target build)
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
