---
phase: 12-orphaned-directory-project-deletes
plan: 02
subsystem: build
tags: [msbuild, swg.sln, SwgClientSetup, mfc, dead-code-removal, decruft]

requires:
  - phase: 12-orphaned-directory-project-deletes
    provides: dual-renderer boot baseline re-established by plan 12-01
provides:
  - SwgClientSetup standalone MFC launcher removed from swg.sln + disk
  - swg.sln project-block removal dress-rehearsed cleanly (precedes lcdui surgery in 12-03)
affects: [12-03, decruft]

tech-stack:
  added: []
  patterns:
    - "swg.sln project removal: delete Project..EndProject block + its 6 ProjectConfigurationPlatforms lines; no regex GUID sweep"

key-files:
  created: []
  modified:
    - src/build/win32/swg.sln

key-decisions:
  - "Targeted block+config-line deletion (no regex GUID sweep) preserved GlobalSection map integrity"

patterns-established:
  - "Validate swg.sln structural integrity post-edit: balanced Project/EndProject counts + build-parse"

requirements-completed: [DECRUFT-02]

duration: ~10min
completed: 2026-05-25
---

# Phase 12 / Plan 02: SwgClientSetup removal Summary

**Removed the SwgClientSetup standalone MFC launcher from swg.sln (Project block + 6 config lines) and deleted its 53-file directory — solution still parses (MSBuild /t:SwgClient exit 0, 0 unresolved) and the client boots to character select under both renderers.**

## Performance

- **Duration:** ~10 min
- **Completed:** 2026-05-25
- **Tasks:** 2 (1 auto + 1 human boot gate)
- **Files modified:** 1 (swg.sln); 1 directory (53 files) deleted

## Accomplishments
- Deleted the `SwgClientSetup` Project block (lines 779-783, incl. its `ProjectSection` dependency on `{ECBDFCFF-...}`) and its 6 `ProjectConfigurationPlatforms` lines (1797-1802) from `swg.sln`.
- Deleted `src/game/client/application/SwgClientSetup/` (53 files: `.vcxproj`, ~40 MFC dialog `.cpp/.h`, `.rc`, `res/`, `resource.h`).
- Verified zero `SwgClientSetup`/`9080903C` references remain; Project/EndProject counts balanced (129/129); GlobalSection config map intact.
- Build gate: `/t:SwgClient` exit 0, 0 unresolved externals — solution parses cleanly with the project gone.
- Dual-renderer boot gate PASSED — character select under `rasterMajor=5` and `=11`; no regression (as expected — SwgClientSetup was never in the runtime client).

## Task Commits

1. **Task 1: Remove SwgClientSetup from swg.sln + delete dir** — `c10d19f10` (chore)
2. **Task 2: Dual-renderer boot gate** — human-verified PASS (operator-run).

## Files Created/Modified
- `src/build/win32/swg.sln` — removed SwgClientSetup Project block + 6 ProjectConfigurationPlatforms lines

## Decisions Made
- None beyond plan: targeted block + config-line deletion (no regex GUID sweep, per the plan's anti-corruption guidance). The `{9080903C}` GUID appeared only in its own block + config lines (verified zero dependency references elsewhere), so removal was a clean self-contained delete.

## Deviations from Plan
None — plan executed exactly as written. (Notably, unlike 12-01, the planned line numbers were accurate and the removal was self-contained.)

## Issues Encountered
None.

## User Setup Required
None.

## Next Phase Readiness
- swg.sln project-block removal validated cleanly — Wave 3 (Plan 12-03, lcdui) swg.sln surgery is unblocked.
- 12-03 edits swg.sln again (lcdui own block + config + 7 dependency lines); this plan's clean removal confirms the targeted-deletion approach works on this file.
- Koogie's uncommitted Direct3d9 working-tree changes left untouched.

---
*Phase: 12-orphaned-directory-project-deletes*
*Completed: 2026-05-25*
