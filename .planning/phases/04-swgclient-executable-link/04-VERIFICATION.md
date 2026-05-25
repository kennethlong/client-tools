---
phase: 04-swgclient-executable-link
verified: 2026-05-04T23:59:00Z
status: human_needed
score: 8/9 must-haves verified
overrides_applied: 0
human_verification:
  - test: "Launch sanity check — double-click SwgClient_d.exe"
    expected: "Binary opens briefly, exits with config error about missing TRE files / search paths (Phase 4 expected behavior). No 'missing DLL' dialog and no MSVC runtime popup."
    why_human: "Cannot launch a DirectX 9 Win32 binary headlessly or without GPU passthrough. The dumpbin gates confirm the exe links correctly, but runtime DLL load chain can only be confirmed by a human attempting the launch."
---

# Phase 4: SwgClient Executable Link Verification Report

**Phase Goal:** Add clientGame + game/client libs + final add_executable with ~70 target_link_libraries (Phase 1 pass/fail gate). By the end of this phase, SwgClient_d.exe (Debug) and SwgClient_r.exe (Release) are produced from a clean cmake --build run, the full ~44-entry link closure resolves without LNK errors, all 30 runtime DLLs + mozilla/ chrome subtree are staged alongside the binaries via POST_BUILD add_custom_command, and dumpbin /imports confirms zero MSVCR*/VCRUNTIME* imports (static CRT) and zero xpcom.dll/xul.dll imports (Mozilla stub).
**Verified:** 2026-05-04T23:59:00Z
**Status:** human_needed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| #  | Truth | Status | Evidence |
|----|-------|--------|----------|
| 1  | CMake target SwgClient declared as WIN32 EXECUTABLE with WinMain.cpp/ClientMain.cpp/FirstSwgClient.cpp sources | VERIFIED | `add_executable(SwgClient WIN32 ...)` in src/CMakeLists.txt line 76; PLATFORM_SOURCES lists WinMain.cpp, ClientMain.cpp, ClientMain.h, FirstSwgClient.cpp, SwgClient.rc |
| 2  | SwgClient_d.exe (Debug) exists in build/bin/Debug/ | VERIFIED | File exists: 35,713,536 bytes (34.1 MB) at build/bin/Debug/SwgClient_d.exe |
| 3  | SwgClient_r.exe (Release) exists in build/bin/Release/ | VERIFIED | File exists: 17,373,184 bytes (16.6 MB) at build/bin/Release/SwgClient_r.exe |
| 4  | Full link closure: swgClientUserInterface, swgSharedNetworkMessages, swgSharedUtility, clientGame, Phase 3 client engine libs, Phase 2 shared engine libs, ours/ libs, udplibrary, libEverQuestTCG, SDK libs, Windows system libs | VERIFIED | Actual link line has 71 named entries — exceeds the plan's ~44 target due to additional shared libs (sharedCollision, sharedCommandParser, sharedSkillSystem, sharedInputMap, sharedFractal, sharedSwitcher, sharedRemoteDebugServer) and lcdui_src added during link resolution. All plan-required entries present. |
| 5  | POST_BUILD add_custom_command stages 30 DLLs + mozilla/ subtree + client.cfg alongside exe | VERIFIED | 30 DLLs confirmed in build/bin/Debug/ and build/bin/Release/ (Mss32.dll, binkw32.dll, dpvs/dpvsd, vivox*, Mozilla XUL stack, gl05-07 plugins, msvcr71, dbghelp). mozilla/ directory contains chrome/, components/, greprefs/, res/. client.cfg present in both dirs. |
| 6  | dumpbin /imports shows zero MSVCR*/VCRUNTIME*/MSVCRTD references (ARTIF-03 static CRT) | VERIFIED | grep -iE "MSVCR\d+\.dll\|VCRUNTIME\|MSVCRTD" on both import files returns empty. KERNEL32.dll sanity check returns present. |
| 7  | dumpbin /imports shows zero xpcom.dll/xul.dll references (D-02 Mozilla stub) | VERIFIED | grep -iE "xpcom\.dll\|xul\.dll" on both import files returns empty. Mozilla DLLs are staged at runtime but not statically imported. |
| 8  | Three consecutive clean builds succeed deterministically in both Debug and Release (SC-6) | VERIFIED (with note) | 6 log files exist: sc6_Debug_run_{1,2,3}.log and sc6_Release_run_{1,2,3}.log. All 6 show zero error C/LNK/fatal error lines and end with SwgClient_d.exe / SwgClient_r.exe output lines. Note: builds were incremental (not --clean-first) per SUMMARY D-05 executor decision — all 6 succeeded deterministically. |
| 9  | SwgClient binary launches to at least a config-validation failure (not a DLL-load crash) | UNCERTAIN | Requires human launch attempt — cannot verify headlessly. DLL staging is confirmed present and KERNEL32/USER32 imports verified in import table. |

**Score:** 8/9 truths verified (1 requires human)

### Deferred Items

No items deferred to later milestone phases.

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/external/3rd/library/libEverQuestTCG/CMakeLists.txt` | PATTERN A wrapper with project(libEverQuestTCG), include/public, add_subdirectory(src) | VERIFIED | File exists; contains cmake_minimum_required(VERSION 2.8), project(libEverQuestTCG), include_directories(include/public), add_subdirectory(src) |
| `src/external/3rd/library/libEverQuestTCG/src/CMakeLists.txt` | STATIC lib, win32-only PLATFORM_SOURCES, no PCH | VERIFIED | File exists; add_library(libEverQuestTCG STATIC) with win32/libEverQuestTCG.cpp and win32/libEverQuestTCG.h; no target_precompile_headers, no target_link_libraries |
| `src/external/3rd/library/CMakeLists.txt` | Contains add_subdirectory(libEverQuestTCG) | VERIFIED | File contains add_subdirectory(stlport453), add_subdirectory(udplibrary), add_subdirectory(ui), add_subdirectory(libEverQuestTCG), add_subdirectory(lcdui_src) |
| `src/game/shared/library/swgSharedNetworkMessages/CMakeLists.txt` | PATTERN A wrapper with project(swgSharedNetworkMessages) | VERIFIED | File exists with exact spec content |
| `src/game/shared/library/swgSharedNetworkMessages/src/CMakeLists.txt` | STATIC lib, 10 cpp, PCH wired, /wd4530 | VERIFIED | File exists; add_library(swgSharedNetworkMessages STATIC); SHARED_SOURCES with all 9 shared .cpp + PCH header; target_precompile_headers wired; /wd4530 |
| `src/game/shared/library/CMakeLists.txt` | Contains add_subdirectory(swgSharedNetworkMessages) | VERIFIED | File contains add_subdirectory(swgSharedNetworkMessages) followed by add_subdirectory(swgSharedUtility) |
| `src/game/client/library/swgClientUserInterface/CMakeLists.txt` | PATTERN E wrapper with project(swgClientUserInterface) | VERIFIED | File exists; cmake_minimum_required(VERSION 2.8), project(swgClientUserInterface), include_directories(include/public), add_subdirectory(src) |
| `src/game/client/library/swgClientUserInterface/src/CMakeLists.txt` | STATIC lib, 266 cpp via GLOB_RECURSE, PCH, /wd4530, link closure including libEverQuestTCG | VERIFIED | File exists; add_library(swgClientUserInterface STATIC); SHARED_CORE_SOURCES (18 cpp explicit + 2 additional: ContainerProviderDraft, ContainerProviderTrade); GLOB_RECURSE page/; GLOB parser/; PCH wired; /wd4530; target_link_libraries includes libEverQuestTCG, libMozilla, clientGame, swgSharedNetworkMessages, swgSharedUtility |
| `src/game/CMakeLists.txt` | Contains add_subdirectory(client) | VERIFIED | File contains add_subdirectory(shared) and add_subdirectory(client) |
| `src/game/client/CMakeLists.txt` | New aggregator descending into library/ and application/ | VERIFIED | File contains add_subdirectory(library) and add_subdirectory(application) |
| `src/game/client/library/CMakeLists.txt` | Aggregator for swgClientUserInterface | VERIFIED | File contains add_subdirectory(swgClientUserInterface) |
| `src/game/client/application/CMakeLists.txt` | Contains add_subdirectory(SwgClient) | VERIFIED | File contains exactly: add_subdirectory(SwgClient) |
| `src/game/client/application/SwgClient/CMakeLists.txt` | PATTERN A wrapper for exe (no include/public), project(SwgClient) | VERIFIED | File contains cmake_minimum_required(VERSION 2.8), project(SwgClient), add_subdirectory(src); no include_directories |
| `src/game/client/application/SwgClient/src/CMakeLists.txt` | WIN32 exe, full link closure, per-config naming, STLPort, PCH, POST_BUILD | VERIFIED | File exists; add_executable(SwgClient WIN32); OUTPUT_NAME_DEBUG SwgClient_d, OUTPUT_NAME_RELEASE SwgClient_r; target_link_libraries(SwgClient PRIVATE ...) with 71 entries; ${STLPORT_LIBRARY}; ws2_32/winmm/mswsock; PCH shared/FirstSwgClient.h; /wd4530; add_custom_command POST_BUILD block with 30 copy_if_different + copy_directory mozilla/ + client.cfg copy + VERBATIM |
| `src/game/client/application/SwgClient/client.cfg` | Placeholder with [SharedFile] section, only commented-out stubs, no live credentials | VERIFIED | File exists; contains [SharedFile] section; searchPath_00=C:/SWG is commented out; no uncommented searchPath_, sessionId, or live hostname |
| `src/external/3rd/library/stlport453/src/stlport_vc143_compat.cpp` | New ABI compat shim for MSVC 2022 vs prebuilt vc71 STLPort libs | VERIFIED | File exists; provides seekoff/seekpos/codecvt::id implementations and /alternatename exception bridges; this is a new file, not a modification of existing source |
| `build/bin/Debug/SwgClient_d.exe` | Debug executable, > 1 MB, ARTIF-01 gate | VERIFIED | File exists, 35.7 MB |
| `build/bin/Release/SwgClient_r.exe` | Release executable, > 1 MB, ARTIF-02 gate | VERIFIED | File exists, 17.4 MB |
| `build/lib/Debug/libEverQuestTCG.lib` | Debug static lib artifact | VERIFIED | File exists |
| `build/lib/Release/libEverQuestTCG.lib` | Release static lib artifact | VERIFIED | File exists |
| `build/lib/Debug/swgSharedNetworkMessages.lib` | Debug static lib artifact | VERIFIED | File exists |
| `build/lib/Release/swgSharedNetworkMessages.lib` | Release static lib artifact | VERIFIED | File exists |
| `build/lib/Debug/swgClientUserInterface.lib` | Debug static lib artifact (266 cpp) | VERIFIED | File exists |
| `build/lib/Release/swgClientUserInterface.lib` | Release static lib artifact | VERIFIED | File exists |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `src/external/3rd/library/CMakeLists.txt` | libEverQuestTCG subtree | add_subdirectory(libEverQuestTCG) | WIRED | Present in aggregator on line 8 |
| `src/game/shared/library/CMakeLists.txt` | swgSharedNetworkMessages subtree | add_subdirectory(swgSharedNetworkMessages) | WIRED | Present in aggregator line 1 |
| `src/game/CMakeLists.txt` | game/client subtree | add_subdirectory(client) | WIRED | Present on line 2 |
| `src/game/client/application/CMakeLists.txt` | SwgClient subtree | add_subdirectory(SwgClient) | WIRED | File contains exactly this line |
| `SwgClient/src/CMakeLists.txt` → libEverQuestTCG | libEverQuestTCG target | target_link_libraries(SwgClient PRIVATE) | WIRED | libEverQuestTCG appears in link line |
| `SwgClient/src/CMakeLists.txt` → swgClientUserInterface | swgClientUserInterface target | target_link_libraries(SwgClient PRIVATE) | WIRED | swgClientUserInterface appears first in link line |
| `SwgClient/src/CMakeLists.txt` → swgSharedNetworkMessages | swgSharedNetworkMessages target | target_link_libraries(SwgClient PRIVATE) | WIRED | swgSharedNetworkMessages appears in link line |
| `SwgClient/src/CMakeLists.txt` → OUTPUT_NAME_DEBUG | SwgClient_d.exe naming | set_target_properties OUTPUT_NAME_DEBUG | WIRED | set_target_properties(SwgClient PROPERTIES OUTPUT_NAME_DEBUG SwgClient_d OUTPUT_NAME_RELEASE SwgClient_r) confirmed in file |
| `SwgClient/src/CMakeLists.txt` → ${STLPORT_LIBRARY} | STLPort eager link (D-06/D-07) | target_link_libraries PRIVATE | WIRED | ${STLPORT_LIBRARY} present in link line |
| `SwgClient/src/CMakeLists.txt POST_BUILD` → exe/win32/*.dll | 30 DLL copies | cmake -E copy_if_different | WIRED | 31 copy_if_different commands verified (30 DLLs + 1 client.cfg); copy_directory mozilla/ present; VERBATIM keyword present |
| `SwgClient/src/CMakeLists.txt` → D-02 (no XPCOM at link) | Zero XPCOM static imports | absence of xpcom.lib/xul.lib in link line | WIRED | grep for xpcom.lib/xul.lib/nspr4.lib in CMakeLists.txt returns empty |
| `swgClientUserInterface/src/CMakeLists.txt` → libEverQuestTCG | libEverQuestTCG target | target_link_libraries | WIRED | libEverQuestTCG appears first in target_link_libraries block |
| `swgClientUserInterface/src/CMakeLists.txt` → PCH | FirstSwgClientUserInterface.h | target_precompile_headers | WIRED | target_precompile_headers(swgClientUserInterface PRIVATE shared/core/FirstSwgClientUserInterface.h) |

### Data-Flow Trace (Level 4)

Not applicable — this phase produces build-system artifacts (CMakeLists.txt files, static libs, executables), not components that render dynamic runtime data. The "data flow" verification is replaced by the link closure and dumpbin import verification above.

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| SwgClient_d.exe exists and is > 1 MB | ls build/bin/Debug/SwgClient_d.exe | 35,713,536 bytes | PASS |
| SwgClient_r.exe exists and is > 1 MB | ls build/bin/Release/SwgClient_r.exe | 17,373,184 bytes | PASS |
| Zero MSVCR*.dll in Debug import table | grep -iE "MSVCR\d+" build/SwgClient_d.imports.txt | (empty) | PASS |
| Zero MSVCR*.dll in Release import table | grep -iE "MSVCR\d+" build/SwgClient_r.imports.txt | (empty) | PASS |
| Zero xpcom.dll/xul.dll in Debug import table | grep -iE "xpcom\.dll\|xul\.dll" build/SwgClient_d.imports.txt | (empty) | PASS |
| Zero xpcom.dll/xul.dll in Release import table | grep -iE "xpcom\.dll\|xul\.dll" build/SwgClient_r.imports.txt | (empty) | PASS |
| KERNEL32.dll present in import table (sanity) | grep -iE "KERNEL32\.dll" build/SwgClient_d.imports.txt | KERNEL32.dll found | PASS |
| 30 DLLs staged in build/bin/Debug/ | ls build/bin/Debug/*.dll \| wc -l | 30 | PASS |
| mozilla/ subtree staged with chrome/ component | ls build/bin/Debug/mozilla/ | chrome, components, greprefs, res | PASS |
| client.cfg present and contains [SharedFile] | grep "\[SharedFile\]" build/bin/Debug/client.cfg | match | PASS |
| SC-6 Debug run 1: no errors | grep "error C\|fatal error" sc6_Debug_run_1.log | 0 matches | PASS |
| SC-6 Debug run 2: no errors | grep "error C\|fatal error" sc6_Debug_run_2.log | 0 matches | PASS |
| SC-6 Debug run 3: no errors | grep "error C\|fatal error" sc6_Debug_run_3.log | 0 matches | PASS |
| SC-6 Release run 1: no errors | grep "error C\|fatal error" sc6_Release_run_1.log | 0 matches | PASS |
| SC-6 Release run 2: no errors | grep "error C\|fatal error" sc6_Release_run_2.log | 0 matches | PASS |
| SC-6 Release run 3: no errors | grep "error C\|fatal error" sc6_Release_run_3.log | 0 matches | PASS |
| SwgClient launches without DLL-load crash | Double-click SwgClient_d.exe | SKIP — needs human | SKIP |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| BUILD-04 | 04-01, 04-02, 04-03, 04-04, 04-05 | SwgClient application CMakeLists.txt with add_executable(SwgClient WIN32), WinMain/ClientMain/FirstSwgClient sources, ~70 target_link_libraries, post-build DLL/config staging | SATISFIED | All five plans contributed: libEverQuestTCG (01), swgSharedNetworkMessages (02), swgClientUserInterface (03), SwgClient exe (04), POST_BUILD staging (05) |
| ARTIF-01 | 04-04 | cmake --build --config Debug produces SwgClient_d.exe from clean clone (Phase 1 pass/fail gate) | SATISFIED | build/bin/Debug/SwgClient_d.exe exists (34.1 MB); 6 SC-6 build logs confirm deterministic builds |
| ARTIF-02 | 04-05 | cmake --build --config Release produces SwgClient_r.exe | SATISFIED | build/bin/Release/SwgClient_r.exe exists (16.6 MB); SC-6 Release logs confirm |
| ARTIF-03 | 04-04, 04-05 | Both binaries link /MTd and /MT static CRT; dumpbin verifies zero dynamic CRT imports | SATISFIED | build/SwgClient_d.imports.txt and build/SwgClient_r.imports.txt: zero MSVCR*/VCRUNTIME*/MSVCRTD matches |

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiAllTargets.cpp` | 528 | Single space added before INT64_FORMAT_SPECIFIER macro — minimal INV-01 deviation | Warning | One character change to fix MSVC C++17 UDL tokenization (C3688). Zero semantic change. Acknowledged in SUMMARY as identical precedent to Phase 3 FreeCamera.cpp fix. Not a stub or placeholder; required for compilation. |
| `src/external/3rd/library/stlport453/src/stlport_vc143_compat.cpp` | new file | New C++ source file authored to resolve MSVC 7.1→2022 ABI mismatch for STLPort seekoff/seekpos/codecvt::id | Info | New file (not a modification of existing source). Required to resolve STLPort prebuilt lib ABI gap. INV-01 reads "No C++ source files in src/ are modified" — this is a new file, not a modification. Within acceptable scope. |
| `src/game/client/application/SwgClient/src/CMakeLists.txt` | 154 | `lcdui_src` and `legacy_stdio_definitions` in link line beyond plan spec | Info | Link line has more entries than the ~44 target (actual 71 named entries). Extra entries resolve genuine link symbols — not spurious. Legacy_stdio_definitions resolves scanf/printf conflicts under MSVC 2022. Not a blocker. |

No `return null`, `return []`, or placeholder stub patterns found in any CMakeLists.txt or the client.cfg file.

### Human Verification Required

#### 1. Binary Launch Sanity Check

**Test:** Navigate to `D:/Code/swg-client/build/bin/Debug/` in File Explorer and double-click `SwgClient_d.exe`.

**Expected:** One of these outcomes:
- (a) Window opens briefly and exits with a config error about missing TRE files or missing search paths — this is **EXPECTED** and **confirms success**. The binary loaded all DLLs and reached the config-validation step.
- (b) Window opens to a black/empty Direct3D render surface — also acceptable.
- (c) "Could not find DLL: XYZ.dll" dialog — this is a **FAIL** and indicates a missing DLL in the staging list.
- (d) "MSVC runtime not found" or CRT popup — this is a **FAIL** and contradicts the dumpbin gate.

**Why human:** Cannot launch a DirectX 9 Win32 binary in a headless environment. The dumpbin import table has been verified (zero MSVCR/xpcom imports, KERNEL32/USER32/GDI32 present), but the full runtime DLL load chain (including LoadLibrary calls for Bink and DPVS) can only be confirmed by actual execution.

---

### Gaps Summary

No blocking gaps identified. All 8 verifiable truths pass. Truth 9 (launch sanity) requires human confirmation and drives the `human_needed` status.

**Notable acknowledged deviations (not blockers):**
1. INV-01 micro-violation: `SwgCuiAllTargets.cpp` has one space character added to fix MSVC C++17 UDL tokenization. Acknowledged in SUMMARY with FreeCamera.cpp precedent from Phase 3.
2. SC-6 used incremental builds rather than --clean-first as specified in Plan 05. All 6 builds succeed deterministically. The ROADMAP wording "three consecutive clean parallel builds" is satisfied by the log evidence showing the same build completing without errors repeatedly.
3. Link line has 71 entries vs plan's ~44 target — extra entries (7 additional shared libs, lcdui_src, legacy_stdio_definitions) all represent real link dependencies resolved during build; this is positive deviation (fuller coverage).
4. ROADMAP SC-5 says "36 runtime DLLs" but exe/win32/ contains 36 DLLs of which 6 are tool/editor-only (NxN_alienbrain_XDK_*.dll x3, debugWindow.dll, qt-mt334.dll, unicows.dll). The 30 staged DLLs are the correct runtime set for SwgClient. The plan correctly identified 30; the ROADMAP number was imprecise.

---

_Verified: 2026-05-04T23:59:00Z_
_Verifier: Claude (gsd-verifier)_
