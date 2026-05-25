---
phase: 05-onboarding-developer-experience
plan: 01
subsystem: docs
tags: [cmake, readme, build-guide, documentation, onboarding, stlport, directx9, vivox, miles, dpvs, bink, xpcom]

# Dependency graph
requires:
  - phase: 04-swgclient-executable-link
    provides: SwgClient_d.exe + SwgClient_r.exe produced; POST_BUILD staging of 30 DLLs + mozilla/ + client.cfg; dumpbin gates verified (static CRT, zero XPCOM imports)
provides:
  - README.md at repo root: developer quickstart with exact cmake configure command, IDE steps, prerequisites, runtime asset setup, and docs links
  - docs/build.md: deep build reference covering architecture, Wave 0-5 tier table, SDK landscape, POST_BUILD staging layout, known quirks (stlport_vc143_compat, FreeCamera.cpp, SwgCuiAllTargets.cpp), compile flags, runtime asset config
affects:
  - 05-02 (clean-clone verify — reads README.md to validate commands are correct)
  - 05-03 (Phase 6 pre-flight recon — reads docs/build.md for Phase 6 pre-flight context)
  - Phase 6 (planning agent reads docs/build.md before creating PLAN.md)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Developer documentation split: terse root README (GitHub landing) + deep docs/build.md (living build guide)"
    - "Wave-order documentation pattern: Wave 0-5 tier table with all lib names per wave"
    - "SDK landscape table: vendored path, link strategy, runtime status per SDK"

key-files:
  created:
    - README.md
    - docs/build.md
  modified: []

key-decisions:
  - "D-01 honored: two-document strategy — README.md (terse quickstart) + docs/build.md (deep reference)"
  - "D-02 honored: README scoped to intro + prereqs + exact CLI command + IDE steps + links only — no deep architecture"
  - "D-03 honored: docs/build.md covers architecture overview, SDK landscape, POST_BUILD staging, known quirks, compile flags, runtime asset setup"
  - "D-04 honored: docs/build.md is markdown only — no HTML twin authored"
  - "T-05-01 mitigated: loginServer.cfg DSN values (sdswgp5b, OCI credentials) NOT copied into docs; referenced only as historical, non-live"
  - "T-05-03 mitigated: exact configure command string verified verbatim in README.md acceptance tests"
  - "INV-01 documented: two minimal C++ source edits (FreeCamera.cpp type cast, SwgCuiAllTargets.cpp UDL space) called out explicitly in docs/build.md"

patterns-established:
  - "Root README as GitHub landing page: 2-3 sentence intro, prereqs, exact CLI commands, IDE steps, runtime assets, docs links, constraints"
  - "docs/build.md as deep reference: architecture → SDK landscape → output layout → CRT policy → quirks → compile flags → runtime config → links"

requirements-completed:
  - DEV-04

# Metrics
duration: 5min
completed: 2026-05-05
---

# Phase 5 Plan 01: Developer Documentation Summary

**Root README.md (GitHub quickstart) and docs/build.md (deep build reference) authored — developer can build SwgClient_d.exe from the README alone in under 5 minutes of reading**

## Performance

- **Duration:** ~5 min
- **Started:** 2026-05-05T13:33:43Z
- **Completed:** 2026-05-05T13:38:39Z
- **Tasks:** 2 of 2
- **Files modified:** 2

## Accomplishments

- README.md created at repo root: terse GitHub landing page with exact cmake configure command (`-T host=x64,v143` flag verbatim), prerequisites (VS 2022 17.8+, Win11 SDK 10.0.26100+, CMake 3.27+, Git; all SDKs vendored), IDE steps (whitengold.sln), runtime assets note, docs links, and constraints summary
- docs/build.md created under docs/: deep build reference with Wave 0-5 dependency tier table (22+ libs per wave), SDK landscape table (13 SDKs with vendored paths, link strategy, runtime status), POST_BUILD staging layout (30 DLLs + mozilla/ + client.cfg), known quirks section (stlport_vc143_compat ABI shim, two minimal source edits, XPCOM stub, UICanvasInitialization.cpp + SwgVideoCapture.cpp exclusions), compile flags table, runtime asset setup with client.cfg searchPath_NN pattern
- Threat model T-05-01 mitigated: loginServer.cfg DSN values explicitly NOT included; doc notes they are historical non-live references

## Task Commits

Each task was committed atomically:

1. **Task 1: Author root README.md** - `064a85c4a` (docs)
2. **Task 2: Author docs/build.md** - `35900820e` (docs)

**Plan metadata:** committed with SUMMARY.md below

## Files Created/Modified

- `D:/Code/swg-client/README.md` - GitHub landing page: intro, prerequisites, exact cmake commands, IDE steps, runtime assets, docs links, constraints
- `D:/Code/swg-client/docs/build.md` - Deep build reference: architecture, SDK landscape, Wave 0-5 tiers, 30-DLL staging layout, known quirks, compile flags, Phase 6 asset config

## Decisions Made

- D-01 through D-04 all honored as specified in CONTEXT.md
- Threat model T-05-01 mitigation applied: no SOE production DSN/credentials in either document
- Threat model T-05-03 mitigation applied: exact configure command verified via grep acceptance test

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Wave table row format used numeric prefix instead of "Wave N" string**
- **Found during:** Task 2 verification (acceptance criteria check)
- **Issue:** Plan's acceptance criterion requires the string "Wave 0" or "Wave 1" verbatim; initial table used `| 0 (foundations) |` rows without "Wave" prefix
- **Fix:** Updated all six wave rows to use `| Wave 0 (foundations) |` format, matching the plan template and acceptance criteria
- **Files modified:** docs/build.md
- **Verification:** `grep -c "Wave 0" docs/build.md` returns 1; `grep -c "Wave 1" docs/build.md` returns 1
- **Committed in:** 35900820e (Task 2 commit, fix applied before commit)

---

**Total deviations:** 1 auto-fixed (Rule 1 - formatting mismatch against acceptance criteria)
**Impact on plan:** Minor formatting fix with zero content change. No scope creep.

## Issues Encountered

None — plan content was fully specified in the PLAN.md `<interfaces>` block; no exploration needed.

## User Setup Required

None - no external service configuration required.

## Known Stubs

None — both documents are narrative markdown with no data-binding or UI rendering.

## Threat Flags

None — docs/build.md explicitly avoids SOE production credentials per T-05-01; no new network endpoints introduced.

## Next Phase Readiness

- README.md and docs/build.md ready for 05-02 clean-clone verification: README contains the exact cmake commands the verification script will exercise
- Phase 6 planning agent can read docs/build.md for architecture context before authoring Phase 6 PLAN.md files
- DEV-04 requirement satisfied

## Self-Check

Checking created files exist and commits are recorded:

- `README.md`: exists at D:/Code/swg-client/README.md
- `docs/build.md`: exists at D:/Code/swg-client/docs/build.md
- Task 1 commit `064a85c4a`: present in git log
- Task 2 commit `35900820e`: present in git log

## Self-Check: PASSED

---
*Phase: 05-onboarding-developer-experience*
*Completed: 2026-05-05*
