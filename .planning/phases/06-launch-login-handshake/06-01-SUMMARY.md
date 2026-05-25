---
phase: 06-launch-login-handshake
plan: 01
subsystem: infra
tags: [cmake, configure_file, client-config, tre-search-paths, login-server]

# Dependency graph
requires:
  - phase: 04-swgclient-executable-link
    provides: POST_BUILD staging infrastructure (add_custom_command) and placeholder client.cfg
provides:
  - src/cmake/config/client.cfg.in template with @SWG_INSTALL_DIR@ substitution for all TRE/TOC paths
  - SWG_INSTALL_DIR CACHE PATH variable in top-level CMakeLists.txt
  - configure_file() wiring in SwgClient CMakeLists.txt producing bin/<config>/client.cfg
  - Deletion of Phase 4 placeholder client.cfg (one source of truth established)
affects:
  - 06-02 (boot debugging to MVB-1 - reads from staged client.cfg)
  - 06-03 (login handshake to MVB-3 - loginServerAddress0/Port0 keys required)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - configure_file(@ONLY) with @VAR@ substitution for machine-specific paths in config files
    - CACHE PATH variable for per-user install roots exposed to cmake-gui and -D overrides
    - Single source of truth: template in src/ + configure_file output in build/ + POST_BUILD copy to bin/

key-files:
  created:
    - src/cmake/config/client.cfg.in
  modified:
    - src/CMakeLists.txt
    - src/game/client/application/SwgClient/src/CMakeLists.txt
  deleted:
    - src/game/client/application/SwgClient/client.cfg

key-decisions:
  - "D-01: Single source of truth at src/cmake/config/client.cfg.in; configure_file + POST_BUILD are the only producers of bin/<config>/client.cfg"
  - "D-02: @SWG_INSTALL_DIR@ placeholder for TRE/TOC paths; CACHE PATH default = D:/Code/SWGSource Client v3.0"
  - "D-05: [Station] session key is intentionally absent from template (dead SOE Station SSO must not activate)"
  - "D-09: [SharedDebug/InstallTimer] enabled=true (verified key name from InstallTimer.cpp:34, not the boot doc typo)"
  - "Rule 1 auto-fix: removed 'sessionId' and 'installTimer' literal text from template comments (acceptance criteria required full absence)"

patterns-established:
  - "Pattern: configure_file(@ONLY) + CACHE PATH for machine-specific config at build time (mirrors sharedDebug time_t_probe.cpp.in precedent)"
  - "Pattern: POST_BUILD copy_if_different reads from CMAKE_CURRENT_BINARY_DIR (configure_file output), not CMAKE_CURRENT_SOURCE_DIR (tracked template)"

requirements-completed: [LAUNCH-01, CONFIG-01, CONFIG-02, CONFIG-03]

# Metrics
duration: 45min
completed: 2026-05-05
---

# Phase 6 Plan 01: client.cfg CMake Template and SWG_INSTALL_DIR Wiring Summary

**CMake configure_file() template wires all six CONFIG/LAUNCH requirements into a single source-of-truth client.cfg.in with @SWG_INSTALL_DIR@ substitution for TRE/TOC paths, loopback LoginServer, and dead-service disable keys**

## Performance

- **Duration:** ~45 min
- **Started:** 2026-05-05T00:00:00Z
- **Completed:** 2026-05-05
- **Tasks:** 3 of 3
- **Files modified:** 3 (modified) + 1 (created) + 1 (deleted)

## Accomplishments

- Authored `src/cmake/config/client.cfg.in` encoding all CONFIG-01/02/03 and LAUNCH-01 requirements; `@SWG_INSTALL_DIR@` placeholder resolves 4 TOC + 2 TRE paths at configure time
- Added `SWG_INSTALL_DIR CACHE PATH` variable to top-level `src/CMakeLists.txt` with default `D:/Code/SWGSource Client v3.0` and NOT EXISTS warning (T-06-06 mitigation)
- Wired `configure_file()` call + updated `POST_BUILD copy_if_different` source in `SwgClient/src/CMakeLists.txt`; deleted Phase 4 placeholder; verified Debug + Release builds produce correct `bin/<config>/client.cfg`

## Task Commits

Each task was committed atomically:

1. **Task 1: Author client.cfg.in template** - `1bfbd174e` (feat)
2. **Task 2: Add SWG_INSTALL_DIR cache variable** - `1c507afd7` (feat)
3. **Task 3: Wire configure_file + update POST_BUILD; delete placeholder** - `86d38c7c0` (feat)

## Files Created/Modified

- `src/cmake/config/client.cfg.in` - New CMake template file; encodes all six Phase 6 requirements; substituted by configure_file(@ONLY) at configure time
- `src/CMakeLists.txt` - Added `set(SWG_INSTALL_DIR ... CACHE PATH ...)` + NOT EXISTS WARNING block after WHITENGOLD_USE_STLPORT_HEADERS block
- `src/game/client/application/SwgClient/src/CMakeLists.txt` - Added configure_file() call before POST_BUILD block; updated POST_BUILD copy_if_different source from `CMAKE_CURRENT_SOURCE_DIR/../client.cfg` to `CMAKE_CURRENT_BINARY_DIR/client.cfg`; updated comment
- `src/game/client/application/SwgClient/client.cfg` - DELETED (Phase 4 placeholder; one source of truth now lives in src/cmake/config/client.cfg.in)

## Decisions Made

- Used `set(... CACHE PATH ...)` not `option()` — PATH type is bool-incompatible; gets folder-picker in cmake-gui; existing project precedent used `option()` for booleans only
- Template uses `[SharedDebug/InstallTimer] enabled=true` (verified from `InstallTimer.cpp:34`) not `[SharedDebug] installTimer=true` (boot doc typo)
- Single self-contained template (no `.include` chains) — SWGSource's layered cfg approach is for end-user separation; for a developer+owner workflow, single file is simpler and reviewable as one diff
- configure_file() in `SwgClient/src/CMakeLists.txt` (not top-level) so `${CMAKE_CURRENT_BINARY_DIR}` in configure_file() output and POST_BUILD copy_if_different source both resolve to the same `build/game/client/application/SwgClient/src/` directory (avoiding Pitfall 5)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Removed 'sessionId' and 'installTimer' literal text from template comments**
- **Found during:** Task 1 (client.cfg.in authoring)
- **Issue:** Plan's EXACT FILE CONTENT included `sessionId` and `installTimer` in explanatory comment text. Acceptance criteria require `findstr /C:"sessionId"` and `findstr /C:"installTimer"` to exit non-zero (full absence). The words appeared in inline comments like "sessionId is INTENTIONALLY OMITTED" and "installTimer=true is wrong". This is also consistent with RESEARCH.md Pitfall 3 which warns against even commenting the forbidden key.
- **Fix:** Rewrote comment text to avoid spelling out the forbidden key names: "The SOE Station SSO key is INTENTIONALLY OMITTED" and "the boot doc typo is wrong" — explanatory without containing the verbatim strings
- **Files modified:** src/cmake/config/client.cfg.in
- **Verification:** Node.js check of all acceptance criteria; both absent-key checks pass
- **Committed in:** 1bfbd174e (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (Rule 1 - Bug)
**Impact on plan:** Auto-fix necessary to meet the plan's own acceptance criteria. No scope creep.

## Issues Encountered

None beyond the Rule 1 auto-fix above.

## Proof of Substitution (SC-4 verification)

The resolved `[SharedFile]` block in `build-phase6-verify2/bin/Debug/client.cfg` after a clean configure + Debug build:

```ini
[SharedFile]
	maxSearchPriority=12
	searchTOC_00_0=D:/Code/SWGSource Client v3.0/sku0_client.toc
	searchTOC_01_1=D:/Code/SWGSource Client v3.0/sku1_client.toc
	searchTOC_02_2=D:/Code/SWGSource Client v3.0/sku2_client.toc
	searchTOC_03_3=D:/Code/SWGSource Client v3.0/sku3_client.toc
	searchTree_00_7=D:/Code/SWGSource Client v3.0/disable_wayfar_dearic_snow.tre
	searchTree_00_8=D:/Code/SWGSource Client v3.0/swgsource_3.0.tre
```

No `@SWG_INSTALL_DIR@` literal remains; all 6 paths fully resolved.

## Success Criteria Confirmation

| SC | Requirement | Status | Evidence |
|----|-------------|--------|---------|
| SC-1 (CONFIG-01) | `[Station] session key` absent | PASS | Node.js check: `sessionId` absent in template and staged cfg |
| SC-2 (CONFIG-02) | `[Station] gameFeatures=15` present | PASS | `gameFeatures=15` in both Debug and Release staged cfg |
| SC-3 (CONFIG-03) | `skipIntro=true`, `disableG15Lcd=true` present | PASS | Both keys present in template and staged cfg |
| SC-4 (LAUNCH-01) | searchTOC_NN_N keys with resolved paths present | PASS | All 4 TOC + 2 TRE paths resolved to `D:/Code/SWGSource Client v3.0/...` |
| SC-5 (D-03) | `loginServerAddress0=127.0.0.1` and `loginServerPort0=44453` | PASS | Both keys present in template and staged cfg |
| SC-6 (D-09) | `[SharedDebug/InstallTimer] enabled=true` block present | PASS | Section header and enabled=true in template and staged cfg |
| SC-7 (D-01) | Phase 4 placeholder deleted | PASS | `src/game/client/application/SwgClient/client.cfg` deleted in commit 86d38c7c0 |
| SC-8 (build) | Clean build (Debug + Release) succeeds | PASS | Both `SwgClient_d.exe` and `SwgClient_r.exe` produced; `bin/<config>/client.cfg` staged |

## Known Stubs

None. All template keys are fully wired; `@SWG_INSTALL_DIR@` is substituted from the CACHE PATH default. No hardcoded empty values or placeholder text flowing to the UI.

## Threat Flags

None new. All threats are enumerated in the plan's `<threat_model>` section and are either mitigated by this plan (T-06-01 session key absence, T-06-05 path-with-spaces, T-06-06 missing-dir warning) or accepted per the project threat model (T-06-02, T-06-03, T-06-04).

## Next Phase Readiness

- `bin/Debug/client.cfg` and `bin/Release/client.cfg` are now fully populated at build time with all required keys
- Plan 02 (boot debugging to MVB-1) can proceed: run `SwgClient_d.exe` from `bin/Debug/`, check `Logs/SwgClient_*.log` for `InstallTimer: ... Game::install` marker
- Plan 03 (login handshake to MVB-3) requires the swg-main VM running on `127.0.0.1:44453` (user's responsibility per D-08)
- No blockers for Plan 02

---
*Phase: 06-launch-login-handshake*
*Completed: 2026-05-05*
