---
phase: 05-onboarding-developer-experience
plan: 03
subsystem: recon
tags: [swg-main, login-server, network-messages, auth, phase-6-preflight]

# Dependency graph
requires:
  - phase: 05-01
    provides: README.md and docs/build.md authored; Phase 5 documentation foundation in place

provides:
  - docs/recon/08-phase6-preflight.md — complete Phase 6 pre-flight recon document
  - P2.01 verdict: sharedNetworkMessages wire format is byte-for-byte identical between whitengold and swg-main
  - P2.03 verdict: swg-main auth bypass confirmed — empty externalAuthURL accepts any credentials

affects:
  - 06-login-client-connect  # Phase 6 planning agent must read 08-phase6-preflight.md before planning

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Point-in-time source survey pattern: include swg-main commit hash in recon docs so readers know the comparison is not permanent"
    - "Threat-aware documentation: historical credentials explicitly labeled as non-live artifacts"

key-files:
  created:
    - docs/recon/08-phase6-preflight.md
  modified: []

key-decisions:
  - "P2.01: sharedNetworkMessages clientLoginServer/ is identical between whitengold and swg-main — no wire format adaptation required for Phase 6 login handshake"
  - "P2.03: swg-main empty externalAuthURL unconditionally sets authOK=1 — any username/password accepted on StellaBellum VM; no server changes needed for Phase 6"
  - "Atomic write deviation: both P2.01 and P2.03 sections written in a single pass after completing all research for both tasks simultaneously; functionality and acceptance criteria fully satisfied"

patterns-established:
  - "Phase 6 pre-flight pattern: survey wire format compatibility and auth mechanism before planning the login connect phase"

requirements-completed:
  - DEV-01

# Metrics
duration: 25min
completed: 2026-05-05
---

# Phase 5 Plan 03: Phase 6 Pre-flight Recon Summary

**Phase 6 pre-flight complete: sharedNetworkMessages wire format identical between whitengold and swg-main; swg-main auth bypass confirmed via empty externalAuthURL accepting any credentials unconditionally**

## Performance

- **Duration:** ~25 min
- **Started:** 2026-05-05T00:00:00Z
- **Completed:** 2026-05-05T00:25:00Z
- **Tasks:** 2
- **Files modified:** 1 (created)

## Accomplishments

- P2.01 confirmed: `ClientLoginMessages.h`, `LoginEnumCluster.h`, and all 8 files in `clientLoginServer/` are byte-for-byte identical between whitengold and swg-main — the login handshake wire format requires no adaptation for Phase 6
- P2.03 confirmed: swg-main's `validateClient()` sets `authOK = 1` unconditionally when `externalAuthURL` is empty (the StellaBellum VM default); the old `validateStationKey` code path is commented out and inert
- Created `docs/recon/08-phase6-preflight.md` as a self-contained reference the Phase 6 planning agent can read without re-reading swg-main source

## Task Commits

Each task was committed atomically:

1. **Task 1: P2.01 sharedNetworkMessages divergence survey** - `87cfd00fe` (docs)
   - Note: P2.03 content was written in the same atomic file creation (see Deviations)

**Plan metadata:** (SUMMARY + metadata commit — to be recorded after SUMMARY committed)

## Files Created/Modified

- `docs/recon/08-phase6-preflight.md` — Phase 6 pre-flight findings covering P2.01 wire format survey and P2.03 auth bypass mechanism; includes historical credential disclaimer per T-05-08 threat mitigation

## Decisions Made

- **P2.01 verdict (D-09 scope):** All 8 source files in `sharedNetworkMessages/clientLoginServer/` are identical between whitengold and swg-main. The sole difference is `Makefile.am` (autotools artifact, whitengold-only). `swgSharedNetworkMessages` subdirectories are identical (5 subdirs in both: combat, consent, core, permissionList, survey). `swgServerNetworkMessages` exists in swg-main only (server-side library; absent from whitengold client-only leak).

- **P2.03 verdict (D-10 scope):** swg-main's `ClientConnection::validateClient()` reads `ConfigLoginServer::getExternalAuthUrl()`. When the URL is empty (default), `authOK = 1` is set unconditionally and `LoginServer::onValidateClient()` is called with full subscription bits (`0xFFFFFFFF`). The client must send `LoginClientId` (not set `[Station] sessionId`) to trigger this path. Default `clientServicePort` is 44453.

## Deviations from Plan

### Atomic Write (Process Deviation — No Rule Required)

**1. Tasks 1 and 2 written in a single pass rather than sequentially**
- **Found during:** Task 1 execution
- **Nature:** Both tasks share a single output file (`docs/recon/08-phase6-preflight.md`). All research for both tasks was completed before writing. Writing both sections atomically is more reliable than writing P2.01 alone, committing, then appending P2.03 separately — the latter would result in a committed file with an incomplete P2.03 section.
- **Impact:** Task 1 commit contains the full file (both P2.01 and P2.03 sections). Task 2 had no additional files to stage. All Task 2 acceptance criteria are satisfied by the Task 1 commit content.
- **Committed in:** `87cfd00fe` (the single commit for this plan's output file)

---

**Total deviations:** 1 process deviation (atomic write)
**Impact on plan:** No scope change. All acceptance criteria for both tasks satisfied. Both P2.01 and P2.03 sections present and verified in the committed file.

## Issues Encountered

None.

## Known Stubs

None — this is a recon document, not a code file. No placeholder data or wired-but-empty sections.

## Threat Flags

No new threat surface introduced. The `docs/recon/08-phase6-preflight.md` document handles
T-05-08 (historical credential disclaimer), T-05-09 (no `externalAuthSecretKey` example value),
and T-05-10 (survey date + swg-main commit hash included) per the plan's threat model.

## Next Phase Readiness

- Phase 6 planning agent can read `docs/recon/08-phase6-preflight.md` and proceed without re-reading swg-main LoginServer source
- Wire format compatibility confirmed — no protocol adaptation work needed in Phase 6
- Auth bypass mechanism documented — Phase 6 needs only to set `loginServerAddress0` and `loginServerPort0` in `client.cfg` and leave `[Station] sessionId` unset
- Phase 5 SC-5 satisfied: both P2.01 and P2.03 pre-flight tasks completed in Phase 5

---
*Phase: 05-onboarding-developer-experience*
*Completed: 2026-05-05*
