---
phase: 16-v2-1-tech-debt-cleanup
plan: 02
subsystem: ui
tags: [dead-code-removal, cleanup, swgClientUserInterface, clientUserInterface, CuiPreferences, decruft]

# Dependency graph
requires:
  - phase: 14-voice-chat-removal
    provides: "Vivox voice-UI deletion that orphaned the CuiPreferences voice-volume statics/accessors"
  - phase: 15-xpcom-mozilla-removal
    provides: "browser CS-action de-wire that left the finalUrl URL-construction block dead (ShellExecute sink already commented)"
provides:
  - "SwgCuiHudAction.cpp with the dead finalUrl URL-construction block + now-dead shellapi.h/ConfigClientGame.h includes removed"
  - "CuiPreferences.cpp/.h with the full orphaned voice-volume API removed (2 statics + 2 REGISTER_OPTION persistence lines + 4 accessors + 4 decls)"
affects: [16-03-link-boot-gate]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Narrow-scope dead-block deletion: honor the locked line bounds, KEEP the enclosing structural brace, leave upstream still-present dead-stores out of range untouched (Pitfall 1)"
    - "Dead-include hygiene: re-confirm via per-file rg that an include's only remaining hit is its own line before removing it"
    - "Atomic full-API removal: statics + their REGISTER_OPTION persistence references + accessors + decls deleted in one edit so the lib still compiles"

key-files:
  created: []
  modified:
    - "src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHudAction.cpp"
    - "src/engine/client/library/clientUserInterface/src/shared/core/CuiPreferences.cpp"
    - "src/engine/client/library/clientUserInterface/src/shared/core/CuiPreferences.h"

key-decisions:
  - "Honored the NARROW D-06 finalUrl scope (block only, ~:1170-1189) — upstream httpParams accumulation (~:1081-1169) intentionally retained, not extended into"
  - "Removed the 2 REGISTER_OPTION(speakerVolume/micVolume) LocalMachine persistence lines together with the statics (they reference the statics; leaving them would break the clientUserInterface compile) — a benign persistence-surface reduction"
  - "D-07: voice statics are plain float + flat accessors, not a positional enum/datatable row → deleted outright with no ordinal placeholders (CR-01 rule does not apply)"

patterns-established:
  - "Per-target grep-zero gate (rg exit 1 = 0 matches = PASS) as the removal-correctness signal; link + boot verification deferred to Plan 16-03"

requirements-completed: []

# Metrics
duration: 8min
completed: 2026-05-27
---

# Phase 16 Plan 02: Dead UI-source residue removal (finalUrl block + voice-volume API) Summary

**Removed two cosmetic dead-source residues from the SwgClient-linked UI libs: the dead `finalUrl` browser-CS URL-construction block (with its now-dead `shellapi.h`/`ConfigClientGame.h` includes) in `SwgCuiHudAction.cpp`, and the full orphaned voice-volume API (statics + REGISTER_OPTION persistence + accessors + decls) in `CuiPreferences`.**

## Performance

- **Duration:** ~8 min
- **Started:** 2026-05-27T14:30:00Z (approx)
- **Completed:** 2026-05-27T14:37:57Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- Deleted the dead `finalUrl` URL-construction block (~:1170-1189) in `SwgCuiHudAction.cpp` — dead because its sole consumer, a `//ShellExecute(...)` call, was already commented out. Net security improvement: removes a latent untrusted-URL-launch path (threat T-16-03).
- Removed the two now-dead includes (`#include "shellapi.h"`, `#include "clientGame/ConfigClientGame.h"`) after re-confirming each was the file's only remaining hit.
- Removed the full orphaned voice-volume API from `CuiPreferences` (orphaned by the Phase 14 Vivox voice-UI delete): 2 statics, 2 `REGISTER_OPTION` LocalMachine persistence lines, 4 accessor bodies + dividers, 4 header decls — all in one atomic edit so `clientUserInterface` still compiles.
- All per-target grep-zero gates PASS (`finalUrl` / `ConfigClientGame` / `shellapi|ShellExecute|SW_SHOW` == 0 in the file; `speakerVolume|micVolume|SpeakerVolume|MicVolume` == 0 repo-wide; `REGISTER_OPTION(speakerVolume|micVolume)` == 0).

## Task Commits

Each task was committed atomically (explicit-path staging only):

1. **Task 1: Delete dead finalUrl block + now-dead includes in SwgCuiHudAction.cpp (D-06)** - `9ffd140b7` (refactor)
2. **Task 2: Remove orphaned voice-volume API from CuiPreferences.cpp/.h (D-06/D-07)** - `842b44989` (refactor)

## Files Created/Modified
- `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHudAction.cpp` - Dead `finalUrl` URL-construction block + 2 now-dead includes removed; enclosing `CuiActions::service` brace and upstream `httpParams` accumulation intact.
- `src/engine/client/library/clientUserInterface/src/shared/core/CuiPreferences.cpp` - Voice-volume statics + 2 `REGISTER_OPTION` persistence lines + 4 accessor bodies removed.
- `src/engine/client/library/clientUserInterface/src/shared/core/CuiPreferences.h` - 4 voice-volume accessor declarations removed.

## Decisions Made
- **Narrow finalUrl scope (D-06 / Pitfall 1):** deleted only the URL-construction block (~:1170-1189), not the upstream `httpParams` accumulation (~:1081-1169). The `Game::getPlayerPath`, `CuiLoginManager`, and `s_confirmCsBrowserSpawn` confirm-box logic were left untouched.
- **REGISTER_OPTION lines removed with the statics:** the 2 `REGISTER_OPTION(speakerVolume)`/`REGISTER_OPTION(micVolume)` lines expand (macro :415) to `LocalMachineOptionManager::registerOption(ms_speakerVolume/ms_micVolume, ...)`, so they reference the deleted statics and MUST go in the same edit or the lib won't compile. Dropping them stops persisting/loading those two LocalMachine keys — a benign persistence-surface change (the Vivox UI that consumed them is gone), not literally zero runtime delta.
- **D-07 no placeholders:** the voice statics are plain `float` + flat get/set, not a positional enum/datatable row, so the Phase 14 CR-01 ordinal-placeholder rule does not apply. Deleted outright.

## Deviations from Plan

None - plan executed exactly as written. All ~approximate line numbers in the plan matched the live tree (shellapi.h :11, ConfigClientGame.h :24, finalUrl block :1170-1189, statics :397-398, REGISTER_OPTION :841-842, accessors :3460-3484, header decls :553-557).

## Issues Encountered
None.

## Known Stubs
None — pure dead-code removal; no placeholder values, mock data, or unwired components introduced.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Both edited libs (`swgClientUserInterface.lib`, `clientUserInterface.lib`) are in the SwgClient link closure. The link + boot regression gate is Plan 16-03's job (per D-08: SwgClient Debug+Release link-grep `unresolved external symbol` == 0, Optimized EXEMPT per DEF-14-01, then one `rasterMajor=11` boot to char-select). No build was performed in this plan.
- No blockers. The protected Koogie Direct3d9 WIP + untracked research docs in the working tree were left untouched/uncommitted.

## Self-Check: PASSED

- FOUND: `.planning/phases/16-v2-1-tech-debt-cleanup/16-02-SUMMARY.md`
- FOUND: `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHudAction.cpp`
- FOUND: `src/engine/client/library/clientUserInterface/src/shared/core/CuiPreferences.cpp`
- FOUND: `src/engine/client/library/clientUserInterface/src/shared/core/CuiPreferences.h`
- FOUND commit: `9ffd140b7` (Task 1)
- FOUND commit: `842b44989` (Task 2)
- Protected Koogie Direct3d9 WIP + untracked research docs: untouched/uncommitted (verified via `git status`)

---
*Phase: 16-v2-1-tech-debt-cleanup*
*Completed: 2026-05-27*
