---
phase: 15-in-game-browser-xpcom-mozilla-removal-milestone-gate
plan: 02
subsystem: infra
tags: [xpcom, mozilla, libmozilla, rsp, vcxproj, editors, swggodclient, decruft, grep-zero, no-build]

requires:
  - phase: 15-in-game-browser-xpcom-mozilla-removal-milestone-gate
    provides: 15-01 removed the source-lib inline .vcxproj link/include edits (this plan owns the .rsp + editor/SwgGodClient .vcxproj — zero file overlap)
provides:
  - Every Mozilla-family token purged from the .rsp files + the 7 editor + SwgGodClient .vcxproj (DECRUFT-06 criterion #1 residue half)
affects: [15-03, 15-04]

tech-stack:
  added: []
  patterns: [exact-content token filter for vcxproj + whole-line filter for .rsp — no MSYS-sed backslash trap]

key-files:
  modified:
    - "5 .rsp: SwgClient includePaths/libraryPaths/libraries, swgClientUserInterface includePaths, clientGame includePaths"
    - "7 editors x (.rsp x3 + inline .vcxproj): Animation/ClientEffect/Lightning/Npc/Particle/Swoosh/Viewer"
    - "SwgGodClient: libraries/includePaths/libraryPaths .rsp + SwgGodClient.vcxproj (the only editor linking libMozilla.lib inline)"

key-decisions:
  - "Deletions-only, ZERO build: the .rsp are vestigial (inline .vcxproj is authoritative — 15-01's scope) and the editors are pre-broken on Qt (out-of-scope build targets). Mirrors Phase 14 14-02."
  - "REVIEW FIX honored: SwgClient's OWN libraries.rsp DID carry the 5 XPCOM libs (the CONTEXT/RESEARCH D-08 'not in any libraries*.rsp' claim was wrong) — purged here so 15-03's repo-wide grep-zero holds."
  - "KEEP: 989crypt.lib (SwgGodClient live stationapi dep), lcdui residue (P12 — 15-04 scope), Qt/ui/soePlatform/miles/bink — all preserved."

patterns-established:
  - "Whole-line .rsp filter (drop a line iff it is one of the 5 XPCOM libs or contains libMozilla) + ;-token .vcxproj filter — surgical, identity for every non-Mozilla line."

requirements-completed: [DECRUFT-06]

duration: ~3min
completed: 2026-05-26
---

# Phase 15-02: `.rsp` + Editor/SwgGodClient Mozilla Residue Purge — Summary

**Every Mozilla-family token (`libMozilla`/`xpcom`/`xul`/`nspr4`/`plc4`/`profdirserviceprovider_s`) removed from the 5 vestigial/live-lib `.rsp` files and the 7 editor + SwgGodClient `.vcxproj` — deletions-only, no build — so the repo-wide criterion #1 grep-zero holds outside the vendored tree (deleted in 15-03).**

## Performance
- **Duration:** ~3 min (no build)
- **Completed:** 2026-05-26
- **Tasks:** 3
- **Files:** 37 modified (65 `.rsp` lines + 175 `.vcxproj` tokens dropped)

## Accomplishments
- **Task 1:** purged the 5 `.rsp` (SwgClient includePaths/libraryPaths/**libraries** [5 XPCOM libs — the missed file], swgClientUserInterface includePaths, clientGame includePaths).
- **Task 2:** purged all 7 editors (`libraries.rsp` 5 libs + `includePaths.rsp` + `libraryPaths.rsp` + inline `.vcxproj` deps/include-dir/lib-dir on 3 configs).
- **Task 3:** purged SwgGodClient `.rsp` ×3 + `.vcxproj` (incl. the inline `libMozilla.lib` at Debug); kept 989crypt.lib + lcdui residue.

## Task Commits
Committed as ONE commit (deletions-only, per plan): `chore(15-02): purge Mozilla residue from .rsp + editor/SwgGodClient vcxproj`.

## Verification
- grep-zero `libMozilla|xpcom|xul|nspr4|plc4|profdirserviceprovider` across all 5 `.rsp` + 7 editor build dirs + SwgGodClient build dir → 0 (rg exit 1). ✅
- KEEP: editor `ui.lib`/`qt` → 2; SwgGodClient `989crypt` → 1, `lcdui` → 3 (deliberately left for 15-04). ✅
- No build (deletions-only; editors pre-broken on Qt). The SwgClient link gate is 15-01 (done) + 15-03/15-04.

## Decisions Made
See `key-decisions` frontmatter.

## Deviations from Plan
None — plan executed exactly as written.

## Issues Encountered
None.

## Next Phase Readiness
- Wave 1 complete (15-01 + 15-02). 15-03's Wave-1 merge gate will re-grep repo-wide to confirm the ONLY surviving Mozilla refs are inside the vendored `libMozilla/` tree + `swg.sln` (both removed by 15-03) before the irreversible tree delete.

---
*Phase: 15-in-game-browser-xpcom-mozilla-removal-milestone-gate*
*Completed: 2026-05-26*
