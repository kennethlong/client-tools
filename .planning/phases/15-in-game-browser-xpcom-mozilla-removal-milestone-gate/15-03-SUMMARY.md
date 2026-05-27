---
phase: 15-in-game-browser-xpcom-mozilla-removal-milestone-gate
plan: 03
subsystem: infra
tags: [xpcom, mozilla, libmozilla, swg-sln, vendored-tree, decruft, link-gate, supply-chain]

requires:
  - phase: 15-in-game-browser-xpcom-mozilla-removal-milestone-gate
    provides: 15-01 (source + SwgClient inline link tokens removed) + 15-02 (.rsp/editor residue) — both confirmed by the Wave-1 merge gate before the irreversible delete
provides:
  - libMozilla.vcxproj dropped from swg.sln (11 locations); vendored libMozilla/ tree (1,866 files) deleted; repo-wide XPCOM grep-zero
affects: [15-04]

tech-stack:
  added: []
  patterns: [GUID-keyed sln project drop via Node line filter (decl+EndProject pair + config-platform + dependency back-refs)]

key-files:
  modified:
    - src/build/win32/swg.sln
  deleted:
    - "src/external/3rd/library/libMozilla/ (1,866 files — build/ + include/ + src/, incl. XPCOM SDK headers nsIWebBrowser*.h + .xpt components)"

key-decisions:
  - "Vendored tree deleted LAST (Wave 2), after 15-01 inline include-dir purge + 15-02 .rsp/editor purge — so no AdditionalIncludeDirectories dangles. Wave-1 merge gate re-grepped repo-wide first: the ONLY surviving Mozilla refs were inside the doomed tree + swg.sln (0 elsewhere)."
  - "All 11 swg.sln GUID {C6C1E14A-...} locations dropped (Project decl + EndProject + 6 config-platform + 9 ProjectDependency back-refs incl. SwgClient :726). Pitfall 4 avoided: no dangling GUID left behind."
  - "Build gate is a forced relink (delete exe + rebuild): the prebuilt libMozilla.lib was already unlinked at SwgClient.vcxproj in 15-01, so dropping the project + deleting the source tree leaves the link green — confirmed Debug AND Release."

patterns-established: []

requirements-completed: [DECRUFT-06]

duration: ~5min (incl. ~1.5min Debug+Release relink)
completed: 2026-05-26
---

# Phase 15-03: Drop libMozilla from swg.sln + Delete Vendored Tree — Summary

**`libMozilla.vcxproj` removed from `swg.sln` at all 11 GUID locations and the entire 1,866-file vendored `libMozilla/` tree (XPCOM/Gecko SDK headers + `.xpt` components) deleted; repo-wide XPCOM grep-zero holds and SwgClient still links clean Debug AND Release.**

## Performance
- **Duration:** ~5 min (Debug relink 0:37, Release relink 0:54)
- **Completed:** 2026-05-26
- **Tasks:** 1
- **Files:** 1 modified (swg.sln), 1,866 deleted

## Accomplishments
- **Wave-1 merge gate:** repo-wide `rg` confirmed 15-01 + 15-02 landed completely — every surviving Mozilla/WebBrowser ref was inside the doomed `libMozilla/` tree or `swg.sln`; 0 elsewhere (editor/SwgClient/.vcxproj rechecked clean).
- Dropped `libMozilla.vcxproj` from `swg.sln`: Project decl + EndProject + 6 `GlobalSection(ProjectConfigurationPlatforms)` entries + 9 `ProjectDependency` back-refs (17 lines; GUID `{C6C1E14A-DEDB-48C6-B028-0C9E4EFF5DC4}`).
- `git rm -r` the vendored `src/external/3rd/library/libMozilla/` tree (1,866 files).

## Task Commits
ONE commit: `chore(15-03): drop libMozilla from swg.sln + delete vendored XPCOM tree (1,866 files)`.

## Verification
- **Criterion #1b:** `rg "C6C1E14A-...|libMozilla" swg.sln` → 0 (rg exit 1); SwgClient project + 127 EndProjects intact (sln well-formed). ✅
- **Criterion #1a:** repo-wide `rg -i "libMozilla|xpcom|xul|nsIWebBrowser|nspr4|plc4|profdirserviceprovider|SwgCuiWebBrowser|TUIWebBrowser|WebBrowserManager" src --glob '!*.filters'` → 0. ✅
- **Criterion #3 (D-13):** Debug link log `unresolved external symbol` == 0 (SwgClient_d.exe 70.1 MB); Release == 0 (SwgClient_r.exe 28.4 MB). Optimized NOT gated (DEF-14-01). ✅
- Tree absent on disk; 1,866 deletions staged.

## Decisions Made
See `key-decisions` frontmatter.

## Deviations from Plan
None — plan executed exactly as written.

## Issues Encountered
None. (The Wave-1 merge gate's first run showed a false "STOP" due to a forward-slash exclusion filter not matching Windows backslash `rg` paths; re-ran with a backslash-normalized filter → confirmed PASS. No actual gap.)

## Next Phase Readiness
- The full XPCOM/Mozilla removal set (15-01..15-03) is on disk. 15-04 can run the milestone gate: lcdui A1 cleanup + P12-P15 residue sweep + Debug/Release link-grep + the dual-renderer boot gate (DECRUFT-07).

---
*Phase: 15-in-game-browser-xpcom-mozilla-removal-milestone-gate*
*Completed: 2026-05-26*
