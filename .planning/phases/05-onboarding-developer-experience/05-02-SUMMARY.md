---
phase: 05-onboarding-developer-experience
plan: "02"
subsystem: infra
tags: [cmake, msvc, visual-studio-2022, clean-clone, build-verification, stlport, directx9]

# Dependency graph
requires:
  - phase: 01-cmake-skeleton-foundations-spike-and-lock
    provides: CMakeLists.txt skeleton and Find*.cmake modules that this plan validates
provides:
  - Clean-clone build verification (DEV-01: CLI Debug+Release from zero)
  - IDE build verification (DEV-02: VS 2022 Build Solution, /MP confirmed)
  - Environment variable audit (DEV-03: DXSDK_DIR/MILES_DIR/MOZILLA_DIR all vendored)
  - git tag v1-build-verified at d899657fa
  - FindSTLPort.cmake auto-write of typeinfo.h shim at configure time
affects:
  - 05-03-onboarding-docs
  - any future phase that performs a clean clone or hands off to a new developer

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Configure-time file(WRITE) for generated shims — FindSTLPort.cmake now emits typeinfo.h at cmake configure, not via manual placement"
    - "Clean-clone gate: delete build/, cmake configure, cmake --build Debug + Release, dumpbin /EXPORTS spot-check"

key-files:
  created:
    - build/bin/Debug/SwgClient_d.exe (34.1 MB, rebuilt from clean clone)
    - build/bin/Release/SwgClient_r.exe (16.6 MB, rebuilt from clean clone)
  modified:
    - src/cmake/win32/FindSTLPort.cmake (auto-fix: typeinfo.h shim now emitted via file(WRITE) at configure time)

key-decisions:
  - "typeinfo.h shim must be written by CMake configure, not placed manually — manual placement is lost when build/ is deleted"
  - "DEV-03 confirmed: all three SDKs (DirectX, Miles, Mozilla) resolve via vendored paths; no env vars required for a clean clone"
  - "v1-build-verified tag pinned at d899657fa — this commit is the clean-clone baseline"

patterns-established:
  - "Clean-clone verification sequence: rm -rf build/ → cmake configure → cmake --build Debug → cmake --build Release → dumpbin gate"
  - "IDE verification: open .sln in VS 2022, Build Solution, confirm /MP in generated .vcxproj"

requirements-completed: [DEV-01, DEV-02, DEV-03]

# Metrics
duration: multi-session (across checkpoint 1 and checkpoint 2)
completed: 2026-05-05
---

# Phase 05 Plan 02: Clean-Clone Verification Summary

**CMake clean-clone gate verified end-to-end: CLI Debug+Release builds, VS 2022 IDE Build Solution, and vendored-SDK audit all pass; typeinfo.h shim bug fixed at configure time and tagged v1-build-verified.**

## Performance

- **Duration:** Multi-session (two human-verify checkpoints)
- **Started:** Session 1 (DEV-03 env check + cmake configure + builds)
- **Completed:** 2026-05-05 (DEV-02 IDE checkpoint approved)
- **Tasks:** 3 (Task 1 auto + Checkpoint 1 human-verify + Checkpoint 2 human-verify)
- **Files modified:** 1 source file modified (FindSTLPort.cmake), 2 binaries rebuilt

## Accomplishments

- Deleted build/ from scratch and reproduced both SwgClient_d.exe (34.1 MB) and SwgClient_r.exe (16.6 MB) via CLI — proving the CMake build is fully self-contained from a clean clone
- Confirmed all four dumpbin /EXPORTS spot-checks pass (Direct3D9, Miles, DPVS, XPCOM gates) — the dependency graph is wired correctly
- Verified VS 2022 Build Solution succeeds with /MP (MultiProcessorCompilation=true) active in the generated .vcxproj — the IDE path is equally sound
- Audited DXSDK_DIR, MILES_DIR, and MOZILLA_DIR: all three are null/empty; every SDK resolves through vendored paths under src/external/3rd — no environment variables needed for a new developer
- Pinned git tag v1-build-verified at commit d899657fa as the clean-clone baseline

## Task Commits

Each task was committed atomically:

1. **Task 1: DEV-03 env check + clean build (Debug + Release)** - `d899657fa` (feat)

Checkpoint 1 (human-verify): dumpbin gates PASS, v1-build-verified tagged at d899657fa
Checkpoint 2 (human-verify): VS 2022 Build Solution SUCCESS, /MP confirmed

**Plan metadata:** to be committed with this SUMMARY

## Files Created/Modified

- `build/bin/Debug/SwgClient_d.exe` - Debug client binary, 34.1 MB, rebuilt from clean clone; timestamp confirmed updated by user
- `build/bin/Release/SwgClient_r.exe` - Release client binary, 16.6 MB, rebuilt from clean clone
- `src/cmake/win32/FindSTLPort.cmake` - Auto-fix: typeinfo.h shim now written by file(WRITE) at CMake configure time instead of requiring manual placement

## Decisions Made

- The typeinfo.h shim must be generated at CMake configure time (file(WRITE)) rather than placed manually. Manual placement is invisible to version control and is silently lost when build/ is deleted, which breaks every clean clone. Fixing this in FindSTLPort.cmake is the correct permanent fix.
- DEV-03 result: no environment variables are required for any of the three primary SDKs (DirectX 9, Miles, Mozilla). This is the intended developer experience and it holds.
- v1-build-verified tagged at d899657fa as the stable clean-clone baseline. Future clean-clone regression tests should verify against this tag.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] FindSTLPort.cmake: typeinfo.h shim lost on clean-clone (build/ delete)**
- **Found during:** Task 1 (DEV-03 env check + cmake configure + clean build)
- **Issue:** The STLPort typeinfo.h compatibility shim had been placed manually in the build tree. Deleting build/ removed it, causing configure or compile failures on a true clean clone. The shim was not being regenerated at cmake configure time.
- **Fix:** Updated src/cmake/win32/FindSTLPort.cmake to emit the typeinfo.h shim via `file(WRITE ...)` during cmake configure, so every clean-clone configure produces it automatically with no manual step.
- **Files modified:** src/cmake/win32/FindSTLPort.cmake
- **Verification:** Clean clone (build/ deleted), cmake configure succeeded, Debug and Release builds succeeded, dumpbin gates passed.
- **Committed in:** d899657fa (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (Rule 1 — bug, configure-time shim generation)
**Impact on plan:** Essential correctness fix. Without it, the clean-clone gate itself was broken. No scope creep.

## Issues Encountered

- None beyond the typeinfo.h shim bug documented above. Once FindSTLPort.cmake was corrected, both CLI and IDE builds proceeded cleanly.

## User Setup Required

None — no external service configuration required. All SDKs are vendored; no environment variables needed.

## Next Phase Readiness

- Clean-clone baseline is verified and tagged (v1-build-verified @ d899657fa)
- IDE and CLI build paths are both confirmed sound
- Phase 05-03 (onboarding docs / README / build.md) can now reference concrete, verified steps
- No blockers

## Self-Check: PASSED

- FindSTLPort.cmake modification: present in commit d899657fa
- SwgClient_d.exe and SwgClient_r.exe: user confirmed timestamps updated after clean build
- git tag v1-build-verified: confirmed at d899657fa
- Checkpoint 1 (dumpbin gates): user confirmed PASS
- Checkpoint 2 (VS 2022 IDE): user confirmed "approved success timestamp looks good"

---
*Phase: 05-onboarding-developer-experience*
*Completed: 2026-05-05*
