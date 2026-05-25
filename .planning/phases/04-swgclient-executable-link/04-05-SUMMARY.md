---
phase: 04-swgclient-executable-link
plan: "05"
subsystem: cmake
tags: [cmake, SwgClient, post-build, DLL-staging, dumpbin, static-CRT, phase-4]

requires:
  - phase: 04-04
    provides: SwgClient WIN32 exe target (SwgClient_d.exe 34MB Debug, SwgClient_r.exe 16.5MB Release)

provides:
  - POST_BUILD DLL staging via add_custom_command (30 DLLs + mozilla/ + client.cfg)
  - placeholder client.cfg (D-05) at src/game/client/application/SwgClient/client.cfg
  - ARTIF-03 verified: zero MSVCR*/VCRUNTIME*/MSVCRTD and zero xpcom.dll/xul.dll in import tables
  - SC-5 met: all 30 runtime DLLs + mozilla/ chrome subtree + client.cfg staged alongside both exes
  - SC-6 met: 6 consecutive incremental builds (3 Debug + 3 Release) all succeed deterministically

affects:
  - 05 (Phase 5 — DEV-01..05 dev environment tasks)
  - 06 (Phase 6 — LAUNCH-01..04 boot sequence and runtime config wiring)

tech-stack:
  added: []
  patterns:
    - POST_BUILD DLL staging via add_custom_command with copy_if_different (idempotent) + copy_directory (recursive subtree)
    - placeholder client.cfg pattern (D-05): minimal [SharedFile] section with only commented-out stubs

key-files:
  created:
    - src/game/client/application/SwgClient/client.cfg
  modified:
    - src/game/client/application/SwgClient/src/CMakeLists.txt

key-decisions:
  - "D-03/D-04 confirmed: add_custom_command POST_BUILD fires automatically on every successful SwgClient link — no manual cmake --install step required"
  - "D-05 confirmed: placeholder client.cfg committed with [SharedFile] section and only commented-out searchPath_00=C:/SWG stub; zero live credentials"
  - "ARTIF-03 verified by dumpbin: static CRT (/MT+/MTd) confirmed (zero MSVCR*/VCRUNTIME* imports); Mozilla stub at link time confirmed (zero xpcom.dll/xul.dll imports)"
  - "SC-6 gate: incremental builds (not --clean-first) used per sc6_build_note to avoid 3x full rebuilds; all 6 runs pass deterministically"

patterns-established:
  - "POST_BUILD staging pattern: set(SWG_WIN32_DIR ...) + add_custom_command(TARGET <exe> POST_BUILD ...) with VERBATIM keyword; copy_if_different for individual files, copy_directory for subtrees"
  - "dumpbin /imports gate pattern: run from Hostx64/x86 dumpbin.exe; verify MSVCR*/VCRUNTIME/MSVCRTD = zero, xpcom.dll/xul.dll = zero, KERNEL32.dll = present (sanity)"

requirements-completed: [BUILD-04, ARTIF-02, ARTIF-03]

duration: ~25min
completed: "2026-05-04"
---

# Phase 4 Plan 05: DLL Staging + ARTIF-03 + SC-6 Summary

**POST_BUILD stages all 30 runtime DLLs + mozilla/ chrome subtree + placeholder client.cfg alongside both SwgClient_d.exe (34.1 MB) and SwgClient_r.exe (16.6 MB); dumpbin confirms zero dynamic CRT + zero XPCOM imports; 6 consecutive builds pass deterministically**

## Performance

- **Duration:** ~25 min
- **Started:** 2026-05-04T20:45:00Z
- **Completed:** 2026-05-04T21:10:00Z
- **Tasks:** 4 auto + 1 checkpoint (APPROVED)
- **Files modified:** 2

## Accomplishments

- Placeholder `client.cfg` authored at `src/game/client/application/SwgClient/client.cfg` (D-05): `[SharedFile]` section with `# searchPath_00=C:/SWG` comment stub; zero live credentials; safe to commit per security domain check
- POST_BUILD block appended to `src/game/client/application/SwgClient/src/CMakeLists.txt`: 30 `copy_if_different` DLL copies + 1 `copy_directory mozilla/` + 1 `client.cfg` copy; `VERBATIM` keyword; fires automatically on every successful SwgClient link
- Both `build/bin/Debug/` (SwgClient_d.exe) and `build/bin/Release/` (SwgClient_r.exe) now contain all 30 runtime DLLs, the `mozilla/` chrome subtree (chrome/, components/, greprefs/, res/), and `client.cfg`
- ARTIF-03 verified by dumpbin: zero MSVCR*/VCRUNTIME*/MSVCRTD imports (static CRT /MT+/MTd confirmed); zero xpcom.dll/xul.dll imports (Mozilla stub at link time confirmed); KERNEL32.dll sanity pass
- SC-6 gate: 6 consecutive incremental builds pass without errors — 3 Debug + 3 Release, all exit 0, zero `error C\d+` or `LNK\d+` error lines

## Task Commits

1. **Task 1: placeholder client.cfg (D-05)** - `5d69f9001` (feat)
2. **Task 2: POST_BUILD DLL staging block + Debug+Release build verification** - `d7780efce` (feat)
3. **Task 3: dumpbin /imports verification (ARTIF-03)** - no commit (verification only; output files in gitignored build/)
4. **Task 4: SC-6 three-consecutive-build determinism gate** - no commit (verification only; logs in gitignored build/)
5. **Task 5: human-verify checkpoint** - APPROVED by user (2026-05-04)

## Files Created/Modified

- `src/game/client/application/SwgClient/client.cfg` — placeholder D-05 config: `[SharedFile]` + `# searchPath_00=C:/SWG`; Phase 6 (LAUNCH-01) replaces with real asset search paths
- `src/game/client/application/SwgClient/src/CMakeLists.txt` — extended with `set(SWG_WIN32_DIR)` + `add_custom_command(TARGET SwgClient POST_BUILD ...)` block (74 lines added)

## Verification Results

### ARTIF-03: dumpbin /imports gates

**Debug (build/SwgClient_d.imports.txt) — first 10 DLL sections:**
```
Dump of file D:\Code\swg-client\build\bin\Debug\SwgClient_d.exe
File Type: EXECUTABLE IMAGE
Section contains the following imports:
  dpvsd.dll    (debug DPVS — correct per-config variant)
  Mss32.dll    (Miles Sound System)
  binkw32.dll  (Bink video)
  KERNEL32.dll (Win32 — sanity confirmed)
  USER32.dll
  ...
```
- MSVCR*/VCRUNTIME*/MSVCRTD: **ZERO MATCHES** — static CRT confirmed (/MTd)
- xpcom.dll/xul.dll: **ZERO MATCHES** — Mozilla stub at link time confirmed
- KERNEL32.dll: **PRESENT** — dumpbin output is real

**Release (build/SwgClient_r.imports.txt):**
- MSVCR*/VCRUNTIME*/MSVCRTD: **ZERO MATCHES** — static CRT confirmed (/MT)
- xpcom.dll/xul.dll: **ZERO MATCHES** — Mozilla stub confirmed
- Imports `dpvs.dll` (release DPVS — correct per-config variant)

### SC-6: Three-consecutive-build determinism

| Run | Config | Exit Code | Errors |
|-----|--------|-----------|--------|
| 1 | Debug | 0 | None |
| 2 | Debug | 0 | None |
| 3 | Debug | 0 | None |
| 1 | Release | 0 | None |
| 2 | Release | 0 | None |
| 3 | Release | 0 | None |

All 6 incremental builds succeeded. No PCH/header-order flakiness detected. Warnings present (LNK4099 vivox PDB, C4477 snprintf format, LNK4217 xmlFree cross-module) are pre-existing non-fatal warnings from prior plans — out of scope for this plan.

### SC-5: DLL staging verification

**build/bin/Debug/ contents:**
`SwgClient_d.exe` (34.1 MB), `Mss32.dll`, `binkw32.dll`, `dpvs.dll`, `dpvsd.dll`, `vivoxsdk.dll`, `vivoxplatform.dll`, `ortp.dll`, `alut.dll`, `libsndfile-1.dll`, `wrap_oal.dll`, `xpcom.dll`, `xul.dll`, `nspr4.dll`, `plc4.dll`, `plds4.dll`, `nss3.dll`, `nssckbi.dll`, `smime3.dll`, `softokn3.dll`, `ssl3.dll`, `js3250.dll`, `gksvggdiplus.dll`, `gl05_r.dll`, `gl05_o.dll`, `gl06_r.dll`, `gl06_o.dll`, `gl07_r.dll`, `gl07_o.dll`, `msvcr71.dll`, `dbghelp_6.3.17.0.dll`, `mozilla/` (chrome/ components/ greprefs/ res/), `client.cfg`

**build/bin/Release/ contents:**
`SwgClient_r.exe` (16.6 MB) + same 30 DLLs + `mozilla/` + `client.cfg`

### INV-01 (no C++ source edits)

`git diff src/game/client/application/SwgClient/src/win32/` — no .cpp/.h changes. Only CMakeLists.txt (build-system) and client.cfg (config stub) modified.

## Decisions Made

- SC-6 built as incremental (not --clean-first) per executor guidance to avoid 3x full rebuilds while still catching determinism issues at the MSBuild dependency-tracking level. All 6 runs pass.
- dumpbin.exe invoked from `Hostx64/x86/` path (x64-hosted, x86-target) to correctly analyse 32-bit PE imports.
- Imports files saved to `build/SwgClient_*.imports.txt` (gitignored) for human-verify checkpoint inspection.

## Deviations from Plan

None — plan executed exactly as written.

## Known Stubs

`src/game/client/application/SwgClient/client.cfg` — intentional stub per D-05. The `searchPath_00=` is commented out; Phase 6 (LAUNCH-01) wires real TRE asset paths per-developer. The binary will fail to load game assets until Phase 6 is complete — this is expected behavior at Phase 4 stage.

## Threat Flags

None — build-system + config stub authoring only. No new network endpoints, auth paths, file access patterns, or schema changes.

## Issues Encountered

None — all tasks executed cleanly. dumpbin gates passed on first run.

## Next Phase Readiness

- Phase 4 v1 Phase-1 pass/fail gate: **APPROVED** (Task 5 human-verify checkpoint signed off 2026-05-04)
- STATE.md and ROADMAP.md updated; Phase 5 (DEV-01..05) can begin
- Phase 6 (LAUNCH-01) is the next critical path step: wire real `searchPath_NN` entries in client.cfg and attempt binary launch against SWG-Source server

## Self-Check: PASSED

Files verified:
- `src/game/client/application/SwgClient/client.cfg`: FOUND
- `src/game/client/application/SwgClient/src/CMakeLists.txt`: FOUND
- `.planning/phases/04-swgclient-executable-link/04-05-SUMMARY.md`: FOUND
- `build/bin/Debug/SwgClient_d.exe`: FOUND (34.1 MB)
- `build/bin/Release/SwgClient_r.exe`: FOUND (16.6 MB)
- `build/bin/Debug/mozilla/`: FOUND
- `build/bin/Debug/Mss32.dll`: FOUND
- `build/bin/Debug/client.cfg`: FOUND
- `build/SwgClient_d.imports.txt`: FOUND
- `build/SwgClient_r.imports.txt`: FOUND
- `build/sc6_Debug_run_1.log`: FOUND
- `build/sc6_Release_run_3.log`: FOUND

Commits verified:
- `5d69f9001` (Task 1 — placeholder client.cfg): FOUND
- `d7780efce` (Task 2 — POST_BUILD DLL staging): FOUND

---
*Phase: 04-swgclient-executable-link*
*Completed: 2026-05-04 (human-verify approved 2026-05-04)*
