---
phase: 05-onboarding-developer-experience
verified: 2026-05-05T00:00:00Z
status: human_needed
score: 5/5 must-haves verified
overrides_applied: 0
human_verification:
  - test: "Verify the documented output path matches reality"
    expected: "README.md states 'bin/Debug/SwgClient_d.exe' but the CMake build places the exe at 'build/bin/Debug/SwgClient_d.exe' (CMAKE_RUNTIME_OUTPUT_DIRECTORY = build/bin). Decide: accept the inaccuracy as-is, or update README.md to state the correct path."
    why_human: "The path discrepancy is a documentation error, not a build failure. The exe exists and the build works. A human must decide whether to accept this documentation state or fix it before Phase 6."
---

# Phase 5: Onboarding + Developer Experience Verification Report

**Phase Goal:** By the end of this phase, a fresh checkout of the repository can be built end-to-end via two documented CLI commands or via the Visual Studio 2022 IDE without environment-variable hunting, and a top-level README documents prerequisites, exact build commands, and runtime asset-path setup.

**Verified:** 2026-05-05

**Status:** human_needed

**Re-verification:** No — initial verification.

---

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Fresh-clone CLI build produces SwgClient_d.exe and SwgClient_r.exe | VERIFIED | build/bin/Debug/SwgClient_d.exe (34.1 MB), build/bin/Release/SwgClient_r.exe (16.6 MB) confirmed by human checkpoint and found on filesystem; git tag v1-build-verified at d899657fa |
| 2 | VS 2022 Build Solution works with /MP parallel compilation observed | VERIFIED | Human checkpoint 2 in 05-02-PLAN.md approved; user confirmed Build Solution success and /MP visible in Output pane; build/whitengold.sln exists |
| 3 | No env vars (DXSDK_DIR, MILES_DIR, MOZILLA_DIR) required | VERIFIED | All three vars return null/empty on this machine; FindSTLPort.cmake, FindDirectX9.cmake, FindMiles.cmake, FindMozilla.cmake all default to vendored paths under src/external/3rd/library/ |
| 4 | README.md and docs/build.md document prerequisites, exact CLI command, IDE workflow, and runtime asset setup | VERIFIED (with documentation warning — see Human Verification) | README.md contains exact cmake command, VS 2022 17.8+, Win11 SDK 10.0.26100+, CMake 3.27+, Git prereqs, IDE steps, searchPath config; docs/build.md covers all required content |
| 5 | Phase 6 pre-flight tasks P2.01 + P2.03 completed and recorded | VERIFIED | docs/recon/08-phase6-preflight.md exists with both P2.01 (sharedNetworkMessages identical between whitengold and swg-main) and P2.03 (swg-main empty externalAuthURL = unconditional authOK) sections fully populated |

**Score:** 5/5 truths verified

---

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `README.md` | GitHub landing page — prereqs + exact cmake commands + IDE steps + runtime config + docs links | VERIFIED | Exists at repo root; contains exact cmake configure command with -T host=x64,v143; all prereqs listed; links to docs/build.md; see WARNING below |
| `docs/build.md` | Deep build reference — architecture + SDK landscape + wave order + quirks + staging layout | VERIFIED | Contains stlport_vc143_compat, FreeCamera.cpp, SwgCuiAllTargets.cpp, INV-01, POST_BUILD, searchPath, DXSDK_DIR=NOT required, Wave 0 + Wave 1, SDK landscape table |
| `docs/recon/08-phase6-preflight.md` | Phase 6 pre-flight — P2.01 + P2.03 findings | VERIFIED | Contains both ## P2.01 and ## P2.03 sections; validateStationKey default=false; externalAuthUrl empty bypass; 44453 port; sessionId guidance; historical credentials disclaimed |
| `src/cmake/win32/FindSTLPort.cmake` | typeinfo.h shim emitted at configure time (auto-fix from 05-02) | VERIFIED | file(WRITE ...) on line 24 emits build/stlport453/include/typeinfo.h at every cmake configure; no manual placement required |
| `build/bin/Debug/SwgClient_d.exe` | Debug client binary rebuilt from clean clone | VERIFIED | 34.1 MB (35,713,536 bytes); human-confirmed rebuilt from clean clone (build/ deleted before configure) |
| `build/bin/Release/SwgClient_r.exe` | Release client binary rebuilt from clean clone | VERIFIED | 16.6 MB (17,373,184 bytes); human-confirmed same clean-clone session |
| `build/whitengold.sln` | VS 2022 solution produced by cmake configure | VERIFIED | Exists at D:/Code/swg-client/build/whitengold.sln |
| `build/bin/Debug/` (30 DLLs + mozilla/ + client.cfg) | POST_BUILD staging | VERIFIED | 30 DLLs, client.cfg, mozilla/ directory all present in build/bin/Debug/; confirmed via filesystem listing |

---

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `README.md` | `docs/build.md` | markdown link `[docs/build.md](docs/build.md)` | WIRED | Link present at line 44 and line 49 of README.md |
| `cmake configure` | `src/CMakeLists.txt` | `cmake -S src -B build` command | WIRED | src/CMakeLists.txt exists; cmake_minimum_required(VERSION 3.27) confirmed; CMAKE_RUNTIME_OUTPUT_DIRECTORY = build/bin |
| `FindSTLPort.cmake` | `build/stlport453/include/typeinfo.h` | `file(WRITE ...)` at configure time | WIRED | file(WRITE at line 24 of FindSTLPort.cmake; no manual step required on clean clone |
| `docs/recon/08-phase6-preflight.md` | swg-main LoginServer auth path | P2.03 auth bypass analysis | WIRED | Document reads from swg-main ConfigLoginServer.cpp (commit 91f0357); validateStationKey and externalAuthURL documented |
| `docs/recon/08-phase6-preflight.md` | sharedNetworkMessages/clientLoginServer/ | P2.01 file list comparison | WIRED | LoginEnumCluster.h and ClientLoginMessages.h both documented as identical; wire format verdict stated |

---

### Data-Flow Trace (Level 4)

Not applicable — this phase produces documentation artifacts and build verification records, not components with dynamic data rendering. No Level 4 trace required.

---

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| cmake configure succeeds on clean clone | Verified by executor: build/ deleted, cmake -S src -B build -G "Visual Studio 17 2022" -A Win32 -T host=x64,v143 exited 0 | Clean configure succeeded | PASS |
| SwgClient_d.exe present (clean rebuild) | powershell: Test-Path D:/Code/swg-client/build/bin/Debug/SwgClient_d.exe | True; 34.1 MB | PASS |
| SwgClient_r.exe present (clean rebuild) | powershell: Test-Path D:/Code/swg-client/build/bin/Release/SwgClient_r.exe | True; 16.6 MB | PASS |
| git tag v1-build-verified exists | git tag -l "v1*" | v1-build-verified | PASS |
| git tag points to d899657fa | git log --oneline v1-build-verified -1 | d899657fa fix(05-02): add typeinfo.h redirect to FindSTLPort.cmake | PASS |
| DEV-03: DXSDK_DIR null | [System.Environment]::GetEnvironmentVariable("DXSDK_DIR") | (empty) | PASS |
| DEV-03: MILES_DIR null | [System.Environment]::GetEnvironmentVariable("MILES_DIR") | (empty) | PASS |
| DEV-03: MOZILLA_DIR null | [System.Environment]::GetEnvironmentVariable("MOZILLA_DIR") | (empty) | PASS |
| 30 DLLs staged in build/bin/Debug/ | Get-ChildItem ... -Filter '*.dll' \| Measure-Object | Count: 30 | PASS |
| mozilla/ staged in build/bin/Debug/ | Test-Path D:/Code/swg-client/build/bin/Debug/mozilla/ | True | PASS |
| client.cfg staged in build/bin/Debug/ | Get-ChildItem ... -Filter '*.cfg' \| Measure-Object | Count: 1 | PASS |
| README.md exact cmake command present | Grep: cmake -S src -B build -G "Visual Studio 17 2022" | Line 23 of README.md | PASS |
| docs/build.md stlport_vc143_compat present | Grep: stlport_vc143_compat in docs/build.md | Line 25, 99 | PASS |
| docs/recon/08-phase6-preflight.md validateStationKey | Grep: validateStationKey | Lines 157, 186, 194, 204 | PASS |
| typeinfo.h auto-generated at configure time | Grep: file(WRITE) in FindSTLPort.cmake | Line 24 of FindSTLPort.cmake | PASS |

---

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|-------------|--------|----------|
| DEV-01 | 05-02-PLAN.md | CLI build end-to-end from clean checkout | SATISFIED | SwgClient_d.exe and SwgClient_r.exe rebuilt from clean clone (build/ deleted); human checkpoint approved; git tag created |
| DEV-02 | 05-02-PLAN.md | VS 2022 IDE Build Solution with /MP | SATISFIED | Human checkpoint 2 approved; build/whitengold.sln exists; /MP confirmed in generated .vcxproj |
| DEV-03 | 05-02-PLAN.md | No DXSDK_DIR/MILES_DIR/MOZILLA_DIR required | SATISFIED | All three env vars confirmed null/empty; vendored paths resolve via Find modules |
| DEV-04 | 05-01-PLAN.md | README.md lists prereqs + exact CLI commands + IDE workflow + runtime asset setup | SATISFIED (with documentation warning) | README.md and docs/build.md exist with all required content; see WARNING on output path |

---

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `README.md` | 27 | States output path as `bin/Debug/SwgClient_d.exe` | WARNING | Incorrect — actual output path is `build/bin/Debug/SwgClient_d.exe` because CMAKE_RUNTIME_OUTPUT_DIRECTORY = ${CMAKE_CURRENT_BINARY_DIR}/bin and -B build places the binary dir at build/. A new developer following the README would look in the wrong location. No blocker — the build itself is correct. |
| `docs/build.md` | 57, 172 | States output path as `bin/Debug/` and instructs editing `bin/Debug/client.cfg` | WARNING | Same incorrect path as README.md. Both documents need updating to reflect `build/bin/Debug/` as the actual output directory. |

---

### Human Verification Required

#### 1. Output Path Documentation Accuracy

**Test:** Check whether the README.md and docs/build.md path references need correction.

**Expected:** Both documents state `bin/Debug/SwgClient_d.exe` as the output location. The actual output is `build/bin/Debug/SwgClient_d.exe` — one extra directory level because `cmake -S src -B build` places the binary dir at `build/`, and `CMAKE_RUNTIME_OUTPUT_DIRECTORY = ${CMAKE_CURRENT_BINARY_DIR}/bin` resolves to `build/bin/`.

**What to decide:** 
- **Option A — Fix the docs (recommended):** Update README.md line 27 from `bin/Debug/SwgClient_d.exe` to `build/bin/Debug/SwgClient_d.exe`, and similarly fix docs/build.md lines 57 and 172. This is a one-line fix in README.md and two references in docs/build.md.
- **Option B — Accept as-is:** If the convention of the project is to always clean build/ after configure (i.e., developers are expected to know the path relative to build/), accept the current wording with a note in docs/build.md. This leaves a potential point of confusion for new contributors.

**Why human:** The fix is trivial but touches a deliverable (README.md) that was already committed. A human must decide whether to issue a correction commit or accept the documentation state for Phase 5 and fix it as part of Phase 6 runtime setup documentation.

**To accept as-is:** Add an override entry to this VERIFICATION.md frontmatter (template below). To fix: edit README.md line 27 and docs/build.md lines 57 and 172 to replace `bin/Debug/` with `build/bin/Debug/`.

---

### Gaps Summary

No gaps blocking the phase goal. The phase deliverables are all present, wired, and verified:

- CLI build produces correct binaries from a clean clone (SC-1/DEV-01)
- IDE build works with /MP (SC-2/DEV-02)
- No env vars required (SC-3/DEV-03)
- Documentation artifacts exist with all required content (SC-4/DEV-04)
- Phase 6 pre-flight recorded (SC-5)
- git tag v1-build-verified exists at d899657fa
- FindSTLPort.cmake auto-generates typeinfo.h at configure time (clean-clone resilience)

The one issue found — documentation states `bin/Debug/SwgClient_d.exe` when the actual path is `build/bin/Debug/SwgClient_d.exe` — is a WARNING-level documentation inaccuracy. It does not block any of the five success criteria (the build works correctly; only the path description in the docs is wrong). A new developer running the commands would succeed; they would just find the exe at a slightly different path than the docs suggest.

The status is `human_needed` because this documentation inaccuracy should be resolved by human decision: fix or accept.

---

_Verified: 2026-05-05_
_Verifier: Claude (gsd-verifier)_
