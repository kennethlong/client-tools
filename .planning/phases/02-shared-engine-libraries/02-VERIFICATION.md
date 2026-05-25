---
phase: 02-shared-engine-libraries
verified: 2026-05-04T00:00:00Z
status: human_needed
score: 5/5 must-haves verified
overrides_applied: 0
human_verification:
  - test: "Run three consecutive parallel Debug builds and confirm all succeed without C2065/C1010 errors"
    expected: "All three invocations of `cmake --build build --config Debug --parallel 8` exit 0 with no PCH race errors"
    why_human: "SC-4 requires three consecutive runs — cannot re-run builds in verifier scope. User manually signed off via Task 4 checkpoint in 02-06-PLAN.md, which constitutes the required evidence."
---

# Phase 2: Shared Engine Libraries Verification Report

**Phase Goal:** By the end of this phase, all 22 libraries under `engine/shared/library/*` build as static `.lib` files via per-library `CMakeLists.txt` files authored by 1:1 directory-level adoption from swg-main's CMake tree, and three consecutive `cmake --build --config Debug --parallel 8` runs succeed deterministically (no header-include-order flakiness).
**Verified:** 2026-05-04
**Status:** human_needed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Per-library CMakeLists.txt exists for each of the 23 shared engine libraries (ROADMAP SC-1 list) using three-level nesting | ✓ VERIFIED | All 23 `<lib>/CMakeLists.txt` and all 23 `<lib>/src/CMakeLists.txt` confirmed present. 24 `add_subdirectory` calls in `engine/shared/library/CMakeLists.txt` (23 engine shared + sharedFoundationTypes from Phase 1). |
| 2 | All produce `.lib` artefacts in both Debug and Release configs (SC-2) | ✓ VERIFIED | 34 `.lib` files in `build/lib/Debug/` (24 Phase 2 + 10 Phase 1). 34 `.lib` files in `build/lib/Release/`. All 23 ROADMAP SC-1 libs plus `swgSharedUtility` confirmed present in both configs. |
| 3 | Tier ordering enforced by `target_link_libraries` (SC-3) | ✓ VERIFIED | `sharedFoundation` → `sharedFile`; `sharedNetwork` → `sharedLog` + `sharedFoundation` + `udplibrary`; `sharedObject` → `sharedCollision` (INTERFACE) + `sharedGame` + `sharedTerrain` + `swgSharedUtility`. Configure produces no "Target not found" errors (CMakeCache exists, solution generated cleanly). |
| 4 | Three consecutive parallel Debug builds succeed deterministically (SC-4) | ? UNCERTAIN | Cannot programmatically re-run 3 sequential builds in verifier scope. User manually signed off via Task 4 checkpoint (human-verify gate) in 02-06-PLAN.md. PCH infrastructure verified in place: 21 of 23 compiled libs have `target_precompile_headers` declared; 2 exempt (sharedSynchronization per swg-main pattern, sharedMathArchive header-only). |
| 5 | `static_assert(sizeof(time_t) == 4, ...)` probe placed in at least one `.cpp` per top-level representative tier and passes (SC-5) | ✓ VERIFIED | 6 `time_t_probe.obj` files confirmed: `sharedDebug/Debug`, `sharedDebug/Release`, `sharedFoundation/Debug`, `sharedFoundation/Release`, `sharedNetwork/Debug`, `sharedNetwork/Release`. Template at `src/cmake/stubs/time_t_probe.cpp.in` contains `static_assert(sizeof(time_t) == 4, ...)`. Generated `.cpp` confirmed in each lib's binary dir. |

**Score:** 5/5 truths verified (SC-4 qualifies as human-verified via user sign-off checkpoint)

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/cmake/stubs/time_t_probe.cpp.in` | time_t probe template | ✓ VERIFIED | Exists with `static_assert(sizeof(time_t) == 4, "_USE_32BIT_TIME_T=1 not reaching this TU")` |
| `src/engine/shared/library/CMakeLists.txt` | INTERFACE stubs + 23 add_subdirectory | ✓ VERIFIED | 3 INTERFACE stubs (sharedCollision, sharedFractal, sharedSkillSystem) + 24 add_subdirectory lines (including sharedFoundationTypes) |
| `src/game/CMakeLists.txt` | game subtree entry | ✓ VERIFIED | Contains `add_subdirectory(shared)` |
| `src/game/shared/library/swgSharedUtility/src/CMakeLists.txt` | swgSharedUtility STATIC target | ✓ VERIFIED | `add_library(swgSharedUtility STATIC ...)` confirmed |
| All 23 per-lib `CMakeLists.txt` pairs | Three-level nesting pattern | ✓ VERIFIED | Every lib has both top-level and `src/CMakeLists.txt` — all 46 files confirmed present |
| `build/lib/Debug/*.lib` (24 Phase 2 libs) | Static lib artefacts | ✓ VERIFIED | All 24 named Phase 2 libs present |
| `build/lib/Release/*.lib` (24 Phase 2 libs) | Static lib artefacts | ✓ VERIFIED | All 24 named Phase 2 libs present |
| 6x `time_t_probe.obj` files | Probe compiled in Debug + Release × 3 libs | ✓ VERIFIED | All 6 files present at correct paths in build tree |
| `build/whitengold.sln` | CMake-generated VS 2022 solution | ✓ VERIFIED | File exists; generator confirmed as `Visual Studio 17 2022`, platform `Win32`, toolset `v143,host=x64` |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `src/CMakeLists.txt` | `src/game/CMakeLists.txt` | `add_subdirectory(game)` | ✓ WIRED | Confirmed in root CMakeLists.txt |
| `engine/shared/library/CMakeLists.txt` | INTERFACE stub targets | `add_library(sharedCollision INTERFACE)` | ✓ WIRED | All three INTERFACE stubs confirmed |
| `sharedDebug/src/CMakeLists.txt` | `time_t_probe.cpp.in` | `configure_file(${CMAKE_SOURCE_DIR}/cmake/stubs/time_t_probe.cpp.in ...)` | ✓ WIRED | Line 70 in file; target_sources appending generated file |
| `sharedFoundation/src/CMakeLists.txt` | `time_t_probe.cpp.in` | `configure_file(...)` | ✓ WIRED | Line 172; target_sources confirmed |
| `sharedNetwork/src/CMakeLists.txt` | `time_t_probe.cpp.in` | `configure_file(...)` | ✓ WIRED | Line 111; `udplibrary` and `mswsock ws2_32` also linked |
| `sharedCompression/src/CMakeLists.txt` | `${ZLIB_LIBRARY}` | `target_link_libraries(sharedCompression ${ZLIB_LIBRARY})` | ✓ WIRED | Confirmed |
| `sharedRegex/src/CMakeLists.txt` | `${PCRE_LIBRARY}` | `target_link_libraries(sharedRegex ${PCRE_LIBRARY})` | ✓ WIRED | Confirmed |
| `sharedXml/src/CMakeLists.txt` | `${LIBXML2_LIBRARY}` | `target_link_libraries(sharedXml ${LIBXML2_LIBRARY})` | ✓ WIRED | ICONV guard `if(NOT WIN32)` also confirmed |
| `sharedNetworkMessages/src/CMakeLists.txt` | `swgSharedUtility/include/public` | `include_directories(${SWG_GAME_SOURCE_DIR}/shared/library/swgSharedUtility/include/public)` | ✓ WIRED | Confirmed; 338 .cpp entries confirmed |
| `sharedObject` ↔ `sharedGame` | Mutual link | `target_link_libraries` | ⚠ CIRCULAR | sharedGame links sharedObject; sharedObject links sharedGame. Both `.lib` files produced (compile-only phase succeeds). Will cause LNK2019 at executable link time — deferred to Phase 4. |

### Data-Flow Trace (Level 4)

Not applicable. Phase 2 delivers build-system artifacts (CMakeLists.txt files and static `.lib` files) — there is no dynamic data rendering. All artifacts are configuration files consumed at build time, not at runtime.

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| All 24 Phase 2 libs present in Debug | `ls build/lib/Debug/*.lib \| grep -v .pdb \| wc -l` | 34 (24 Phase 2 + 10 Phase 1) | ✓ PASS |
| All 24 Phase 2 libs present in Release | `ls build/lib/Release/*.lib \| grep -v .pdb \| wc -l` | 34 | ✓ PASS |
| 6 time_t probe .obj files (3 libs × 2 configs) | `find build -name time_t_probe.obj \| wc -l` | 6 | ✓ PASS |
| CMake generator is VS 2022 Win32 | `grep CMAKE_GENERATOR build/CMakeCache.txt` | `Visual Studio 17 2022` | ✓ PASS |
| MSVC runtime /MTd for Debug | `grep RuntimeLibrary build/.../sharedDebug.vcxproj` | `MultiThreadedDebug` | ✓ PASS |
| sharedNetworkMessages has 338 .cpp entries | `grep -c '.cpp$' src/.../sharedNetworkMessages/src/CMakeLists.txt` | 338 | ✓ PASS |
| sharedGame has 140 .cpp entries | `grep -c '.cpp$' src/.../sharedGame/src/CMakeLists.txt` | 140 (expected ~139) | ✓ PASS |
| Three consecutive parallel Debug builds (SC-4) | User manual checkpoint in 02-06 Task 4 | User signed off "approved" | ? HUMAN (see below) |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| BUILD-03 | 02-01 through 02-06 | ~50 per-library CMakeLists.txt files covering engine/shared/library/* | ✓ SATISFIED | 23 engine shared lib pairs (46 files) + swgSharedUtility pair (2 files) = 48 CMakeLists files authored this phase. REQUIREMENTS.md §BUILD-03 scope is 22 shared libs (Phase 2 anchor); actual count is 23+1=24 which exceeds the requirement. |

**Requirement ID cross-reference:** BUILD-03 is the only requirement ID declared across all 6 Phase 2 plans. REQUIREMENTS.md §Traceability confirms BUILD-03 is "anchored at Phase 2 (22 shared libs)". All 6 plans declare `requirements: [BUILD-03]`. Coverage: 100%.

### Anti-Patterns Found

| File | Issue | Severity | Impact |
|------|-------|----------|--------|
| All 23 per-lib `CMakeLists.txt` (top-level) | `cmake_minimum_required(VERSION 2.8)` + `project()` in subdirectories — per REVIEW WR-05, this can reset CMP0091 to OLD behavior, potentially reverting static CRT policy | ⚠ Warning | **Mitigated empirically:** `build/engine/shared/library/sharedDebug/src/sharedDebug.vcxproj` shows `MultiThreadedDebug` (Debug) and `MultiThreaded` (Release) — CMP0091 is being honoured in practice. The root `cmake_policy(SET CMP0091 NEW)` at line 3 of `src/CMakeLists.txt` precedes all `add_subdirectory` calls and persists. WR-05 is a theoretical fragility, not an active defect. |
| `sharedGame/src/CMakeLists.txt` + `sharedObject/src/CMakeLists.txt` | Circular `target_link_libraries`: sharedGame → sharedObject AND sharedObject → sharedGame (per REVIEW CR-02) | ⚠ Warning | **Phase 2 impact: none.** Both `.lib` files compile and exist. The cycle will cause LNK2019 at Phase 4 executable link time. This is a known deferred issue — see Deferred Items below. |
| `src/cmake/win32/FindSTLPort.cmake` | Hardcoded UCRT path `C:/Program Files (x86)/Windows Kits/10/...` (per REVIEW CR-03) | ⚠ Warning | **Phase 2 impact: none on this machine.** Build succeeded. Will silently fail on non-default SDK installs. Advisory for future phases. |
| `sharedThread/src/CMakeLists.txt` | `${CMAKE_THREAD_LIBS_INIT}` used without `find_package(Threads)` (per REVIEW CR-04) | ℹ Info | On Windows/MSVC this evaluates to empty — no functional impact. Advisory only for cross-platform future work. |
| `src/cmake/compat/sharedFoundation/PlatformGlue.h` | `extern "C"` wrapping per REVIEW CR-01 — may cause C2733 for `strsep`/`gmtime_r`/`finite` | ⚠ Warning | **Build succeeded** — all Phase 2 libs compiled. On Windows, `strsep`, `gmtime_r`, and `finite` are not available in MSVC UCRT, so those declarations are likely guarded or absent. The build passing constitutes empirical evidence the CR-01 concern does not manifest in this Win32/MSVC 2022 context. |

**Stub classification verdict:** No stubs found in any CMakeLists.txt. All `add_library(X STATIC ...)` targets enumerate real source files. sharedMathArchive uses a generated stub `.cpp` (header-only pattern) as intended by design — this is not a functional stub.

### Human Verification Required

#### 1. Three Consecutive Parallel Debug Builds (SC-4)

**Test:** From `D:/Code/swg-client`, run:
```powershell
cmake --build build --config Debug --parallel 8
cmake --build build --config Debug --parallel 8
cmake --build build --config Debug --parallel 8
```
**Expected:** All three invocations exit 0. No `error C2065` or `error C1010` errors in any run.
**Why human:** Cannot re-execute builds in verifier scope. User signed off via the required Task 4 checkpoint (human-verify gate) in 02-06-PLAN.md — this is accepted as prior human verification. If the build is re-run from a clean state, human confirmation is required again.

**Note:** The 02-06 plan's Task 4 was a `type="checkpoint:human-verify" gate="blocking"` requiring the user to type "approved" before the phase could be declared complete. The SUMMARY records SC-4 PASS. The verifier accepts this as prior human sign-off but surfaces it here for traceability.

---

### Gaps Summary

No blocking gaps found. All five ROADMAP success criteria are satisfied:

- **SC-1** (per-lib CMakeLists for all 23 engine shared libs): ✓ Verified — 23 lib pairs exist, all wired into the build tree
- **SC-2** (Debug + Release .lib artefacts): ✓ Verified — 24 Phase 2 libs in both configs
- **SC-3** (tier ordering via target_link_libraries): ✓ Verified — key link declarations confirmed; CMake solution generated without configure errors
- **SC-4** (3x deterministic Debug build): ? Human-verified via user sign-off checkpoint — surfaced for traceability
- **SC-5** (time_t probe at Tier 2, 3, 5): ✓ Verified — 6 .obj files confirmed in build tree

The circular `sharedGame` ↔ `sharedObject` link dependency (REVIEW CR-02) is a deferred concern addressed in Phase 4 — it does not block Phase 2's compile-only goal.

### Deferred Items

Items not yet met but explicitly addressed in later milestone phases — not actionable gaps for Phase 2.

| # | Item | Addressed In | Evidence |
|---|------|-------------|----------|
| 1 | Circular `sharedGame` ↔ `sharedObject` link cycle will cause LNK2019 at executable link | Phase 4 | Phase 4 success criteria: "cmake --build build --config Debug produces bin/Debug/SwgClient_d.exe from a clean clone" — executable link verification requires resolving this cycle |
| 2 | `FindSTLPort.cmake` hardcoded UCRT path fragility | Phase 5 | Phase 5 SC: "No vendored-SDK find module requires a manual DXSDK_DIR/MILES_DIR/MOZILLA_DIR environment variable" — find module robustness is validated in Phase 5 |
| 3 | `find_package(Threads)` missing for `CMAKE_THREAD_LIBS_INIT` (sharedThread) | Phase 3+ | Phase 3 begins Linux-compat extensions. On Windows/MSVC this is a no-op; will need resolution before any cross-platform build. |

---

_Verified: 2026-05-04_
_Verifier: Claude (gsd-verifier)_
