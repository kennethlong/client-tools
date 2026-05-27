---
phase: 16-v2-1-tech-debt-cleanup
plan: 01
subsystem: infra
tags: [msbuild, vcxproj, decruft, dead-token, link-deps, soePlatform, lcdui]

# Dependency graph
requires:
  - phase: 14-vivox-removal
    provides: "4bc512b45 analog — same SwgGodClient.vcxproj inline AdditionalDependencies token-purge construct + soePlatform KEEP-list discipline"
  - phase: 15-xpcom-removal
    provides: "d1dcb8447 / 15-04 — already swept lcdui LIBPATH residue from the 4 editor vcxproj (Target 2 is a verified no-op)"
provides:
  - "SwgGodClient.vcxproj Debug AdditionalDependencies with the dead 989crypt.lib token removed (closes the latent-LNK1181-if-built tech-debt item)"
  - "Confirmed-zero D-02 sweep across SwgGodClient.vcxproj for all v2.1-deleted backing libs"
  - "Re-asserted D-04 verify-only: 0 lcdui refs across all 4 editor vcxproj"
affects: [16-02, 16-03, v2.1-milestone-close]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Single-token inline AdditionalDependencies deletion with per-token KEEP-list grep guard (mirror of 4bc512b45)"
    - "Adjacency-trap discipline: live crypto.lib token preserved while removing the look-alike 989crypt.lib"

key-files:
  created:
    - .planning/phases/16-v2-1-tech-debt-cleanup/16-01-SUMMARY.md
  modified:
    - src/game/client/application/SwgGodClient/build/win32/SwgGodClient.vcxproj

key-decisions:
  - "Removed ONLY the single 989crypt.lib token (token + trailing ;) from line ~99 Debug config; Optimized/Release untouched (no 989crypt there)"
  - "Preserved the separate live crypto.lib token (adjacency trap) and all 9 soePlatform KEEP tokens"
  - "D-03: SwgGodClient grep-only verified, never built (Qt MSB8066, out of /t:SwgClient closure)"
  - "D-04: editor vcxproj are verify-only no-ops — already 0 lcdui (swept by 15-04); no edit made"
  - "D-05: doc-staleness items (12-VERIFICATION.md / 13-SUMMARY.md) left out of scope; no doc edits"

patterns-established:
  - "KEEP-list survival is per-token grep-verified (all 9 soePlatform tokens + crypto.lib), not a weak sample"
  - "Verify-only Target asserts grep-zero AND git-status-clean (no edit) as its acceptance evidence"

requirements-completed: []  # none — post-hoc tech-debt closure; traces to v2.1-MILESTONE-AUDIT.md (no product REQ-IDs)

# Metrics
duration: 1min
completed: 2026-05-27
---

# Phase 16 Plan 01: SwgGodClient 989crypt.lib Dead-Token Removal + Editor lcdui Verify Summary

**Removed the dead `989crypt.lib` link token from SwgGodClient.vcxproj (Debug config) while preserving the 9-token soePlatform KEEP-list and the separate live `crypto.lib`; re-confirmed 0 lcdui residue across all 4 editor vcxproj (verify-only no-op).**

## Performance

- **Duration:** ~1 min
- **Started:** 2026-05-27T14:31:39Z
- **Completed:** 2026-05-27T14:32:37Z
- **Tasks:** 2 (1 edit + 1 verify-only)
- **Files modified:** 1 (SwgGodClient.vcxproj)

## Accomplishments
- Deleted the single dead `989crypt.lib;` token from the SwgGodClient Debug `<AdditionalDependencies>` run (line ~99) — closes the latent LNK1181-if-built tech-debt item from `.planning/v2.1-MILESTONE-AUDIT.md`. Backing lib was removed with `stationapi/` in v2.1.
- Verified the D-02 sweep across the whole file: `989crypt` and `stationapi` now 0; `lcdui`/`VideoCapture`/`vivox`/`libMozilla`/`xpcom`/`xul`/`VChatAPI` all confirmed 0 (were already 0).
- Per-token KEEP-list guard PASS: all 9 soePlatform tokens (Base/ChatAPI/ChatMono/CommodityAPI/dbgutil/monapi/Network/rdp/TcpLibrary) survive, and the separate live `crypto.lib` survives (adjacency trap avoided).
- Re-asserted D-04: 0 `lcdui` references across all 4 editor vcxproj (AnimationEditor/ClientEffectEditor/LightningEditor/ParticleEditor) with zero edits to those files.

## Task Commits

Each task was committed atomically:

1. **Task 1: Remove dead 989crypt.lib token from SwgGodClient.vcxproj (D-01/D-02/D-03)** - `577f68def` (chore)
2. **Task 2: Verify-only — confirm 0 lcdui in 4 editor vcxproj (D-04) + record D-05 scope fence** - no commit (verify-only, no file change; result recorded here)

**Plan metadata:** (this SUMMARY + STATE/ROADMAP) committed separately as the final docs commit.

## Files Created/Modified
- `src/game/client/application/SwgGodClient/build/win32/SwgGodClient.vcxproj` - Removed the single dead `989crypt.lib;` token from the Debug AdditionalDependencies run (line ~99). 1 insertion, 1 deletion (one line). Optimized (~143) and Release (~185) dep-lists untouched.

## Decisions Made
None beyond the locked CONTEXT decisions (D-01..D-05) — plan executed exactly as written. The edit was a strict single-token deletion mirroring analog commit `4bc512b45`.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None. One cosmetic note: the `Network\.lib` KEEP-list grep returned 2 hits (the soePlatform `Network.lib` token plus the unrelated `sharedNetwork.lib` engine token, which also contains the `Network.lib` substring) — both are legitimately present; the soePlatform `Network.lib` token survives as required.

## Verification Evidence (grep-only — D-03, no build)
- `rg -i "989crypt|stationapi" SwgGodClient.vcxproj` → exit 1 (0 matches) — PASS
- `rg -i "lcdui|VideoCapture|vivox|libMozilla|xpcom|xul|VChatAPI" SwgGodClient.vcxproj` → exit 1 (0 matches) — PASS (D-02 confirm-zero)
- Per-token KEEP-list: Base.lib=1, ChatAPI.lib=1, ChatMono.lib=1, CommodityAPI.lib=1, dbgutil.lib=1, monapi.lib=1, Network.lib=2 (incl. sharedNetwork), rdp.lib=1, TcpLibrary.lib=1 — all 9 present, PASS
- `rg -o "crypto\.lib" SwgGodClient.vcxproj` → present (exit 0) — live token survives, PASS
- `rg -ni "lcdui"` across all 4 editor vcxproj → exit 1 (0 matches), git status clean for those files — PASS (D-04)
- SwgGodClient NOT built (D-03 honored)
- D-05: no edits to 12-VERIFICATION.md or 13-SUMMARY.md

## Working-Tree Safety
- Only `SwgGodClient.vcxproj` was staged/committed (explicit-path staging; no `git add -A`/`.`).
- Koogie's Direct3d9 WIP (vcxproj/cpp) and all untracked research docs/tools remain untouched and uncommitted.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Plan 16-02 (Target 3a/3b — `SwgCuiHudAction.cpp` dead `finalUrl` block + `CuiPreferences` voice-volume API) is unblocked and independent; those edits ARE in the SwgClient link closure and will need the Debug+Release link-grep + boot gate.
- This plan's edit is out of the `/t:SwgClient` closure, so it does not affect the SwgClient build/boot gate.

## Self-Check: PASSED

- `16-01-SUMMARY.md` exists — FOUND
- Task 1 commit `577f68def` — FOUND
- Docs commit `37898675c` — FOUND
- Post-commit re-grep `989crypt|stationapi` in SwgGodClient.vcxproj → 0 (exit 1) — PASS
- Koogie Direct3d9 WIP still uncommitted/untouched — confirmed

---
*Phase: 16-v2-1-tech-debt-cleanup*
*Completed: 2026-05-27*
