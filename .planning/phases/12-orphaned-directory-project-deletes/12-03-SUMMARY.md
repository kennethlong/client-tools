---
phase: 12-orphaned-directory-project-deletes
plan: 03
subsystem: build
tags: [msbuild, swg.sln, rsp, lcdui, lgLcd, g15, EZ_LCD, mfc, dead-code-removal, decruft]

requires:
  - phase: 12-orphaned-directory-project-deletes
    provides: swg.sln project-block removal validated by 12-02; dual-renderer boot baseline from 12-01
provides:
  - lcdui (Logitech G15 LCD subsystem) fully removed — live UI source de-wired, swg.sln + all .rsp + inline vcxproj purged, both dirs deleted
  - Phase 12 dual-renderer boot baseline closed for v2.1 Decruft
affects: [decruft, 13, 14, 15]

tech-stack:
  added: []
  patterns:
    - "Live-source de-wire via #ifdef-guard compile-out (remove the #define, keep no-op method stubs so callers link)"
    - "Build-input authority: inline .vcxproj fields are authoritative; .rsp files are vestigial (not @-referenced)"

key-files:
  created: []
  modified:
    - src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiG15Lcd.cpp
    - src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHud.cpp
    - src/build/win32/swg.sln
    - src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj
    - src/game/client/application/SwgClient/build/win32/libraries.rsp
    - src/game/client/application/SwgClient/build/win32/libraryPaths.rsp
    - src/game/client/library/swgClientUserInterface/build/win32/includePaths.rsp
    - "src/engine/client/application/{Animation,ClientEffect,Lightning,Particle,Swoosh}Editor/build/win32/*.vcxproj + *.rsp"
    - src/game/client/application/SwgGodClient/build/win32/SwgGodClient.vcxproj + *.rsp

key-decisions:
  - "Inline vcxproj deps (lcdui.lib/lgLcd.lib) were the real build inputs, not the .rsp files — removed inline or the dir-delete would LNK1181"
  - "Validation bar reduced full-solution -> /t:SwgClient: editor tools are pre-broken on Qt .ui custom-build (MSB8066), independent of lcdui"

patterns-established:
  - "Confirm .rsp consumption before trusting it as the build input (grep vcxproj for @*.rsp)"
  - "swg.sln dependency lines for a removed project must all go, else solution-parse references a missing GUID"

requirements-completed: [DECRUFT-03]

duration: ~50min
completed: 2026-05-25
---

# Phase 12 / Plan 03: lcdui removal Summary

**Fully removed the Logitech G15 LCD subsystem — de-wired the two live UI files to no-op stubs, unwired lcdui from swg.sln (own block + all 7 deps) and every .rsp + inline vcxproj dep across SwgClient + 6 editors, and deleted both directories — SwgClient builds clean (exit 0, 0 unresolved) and boots to character select with working HUD/input under both renderers.**

## Performance

- **Duration:** ~50 min
- **Completed:** 2026-05-25
- **Tasks:** 3 (2 auto + 1 human boot gate)
- **Files modified:** 25 (2 source + swg.sln + SwgClient vcxproj/2 rsp + swgClientUI rsp + 6 editor/SwgGodClient vcxproj + 12 editor/SwgGodClient rsp); 2 directories (44 files) deleted

## Accomplishments
- **Source de-wire:** removed `EZ_LCD.h` include + `#define USE_LCD` from `SwgCuiG15Lcd.cpp` and `EZ_LCD.h` from `SwgCuiHud.cpp`. All `CEzLcd`/`s_lcd` usage is `#ifdef USE_LCD`-guarded → undefining `USE_LCD` compiles it out; `initializeLcd`/`updateLcd`/`remove` remain valid no-op stubs so `SwgCuiHud`/`ClientMain` callers keep linking.
- **swg.sln:** removed lcdui's own Project block + 6 ProjectConfigurationPlatforms lines + all 7 `{20D2AEE7}` ProjectDependency lines (SwgClient + 6 editors). Project/EndProject balanced (128/128).
- **Build inputs:** removed `lcdui.lib`/`lgLcd.lib` + lcdui search paths from SwgClient's inline vcxproj (all 3 configs); removed `lgLcd.lib`/`lcdui.lib` inline deps from the 6 editor/SwgGodClient vcxproj; purged lcdui/lgLcd from 15 `.rsp` files.
- **Deleted** `lcdui_src/` (wrapper) + `lcdui/` (Logitech SDK: lglcd.h + lgLcd.lib).
- Build: `/t:SwgClient` exit 0, 0 unresolved externals. Grep-zero for `20D2AEE7|lcdui|lgLcd` over `.sln` + `.rsp`.
- Dual-renderer boot gate PASSED — character select + HUD + input under `rasterMajor=5` and `=11`; no UI-init regression.

## Task Commits

1. **Task 1: De-wire lcdui from live UI source** — `f4a7fb228` (chore)
2. **Task 2: Unwire swg.sln + .rsp + inline vcxproj, delete dirs** — `f99c645dd` (chore)
3. **Task 3: Dual-renderer + HUD/input boot gate** — human-verified PASS (operator-run).

## Files Created/Modified
- `SwgCuiG15Lcd.cpp` / `SwgCuiHud.cpp` — EZ_LCD.h + USE_LCD removed; LCD methods compile out to no-ops
- `swg.sln` — lcdui project block + config + 7 dependency lines removed
- `SwgClient.vcxproj` — lcdui.lib/lgLcd.lib deps + lcdui search paths removed (inline)
- SwgClient/swgClientUserInterface/6-editor/SwgGodClient `.rsp` — lcdui/lgLcd lines purged
- 6 editor/SwgGodClient `.vcxproj` — inline lgLcd.lib/lcdui.lib deps removed (lcdui\lib search PATHS left as inert /LIBPATH)

## Decisions Made
- Kept `SwgCuiG15Lcd` class + methods (no-op stubs) per minimal-surgery approach; `disableG15Lcd` cfg key left in place (inert, out of scope).
- Removed all 7 swg.sln dependency lines (not just SwgClient's): a removed project's GUID left in any project's ProjectDependencies makes the solution parse reference a missing project.

## Deviations from Plan

### Auto-fixed Issues

**1. [Blocking — incomplete plan map, same class as 12-01 989crypt] .rsp files are not the build input; inline vcxproj is**
- **Found during:** Task 2 (build-graph mapping)
- **Issue:** Plan's Task 2 targeted the `.rsp` files, but no `.vcxproj` `@`-references any `.rsp` — the inline vcxproj `AdditionalDependencies`/`AdditionalLibraryDirectories` are authoritative. `lcdui.lib`+`lgLcd.lib` were inline in SwgClient (all 3 configs) and the 6 editor/SwgGodClient vcxproj. Cleaning only `.rsp` then deleting `lcdui/` would have failed the link with `LNK1181` (cannot open lcdui.lib/lgLcd.lib).
- **Fix:** Removed the inline deps + lcdui search paths from SwgClient.vcxproj and the inline lgLcd.lib/lcdui.lib deps from the 6 editor/SwgGodClient vcxproj, in addition to the planned `.rsp` purges.
- **Verification:** `/t:SwgClient` exit 0, 0 unresolved; grep-zero over `.sln`+`.rsp`.
- **Committed in:** f99c645dd

**2. [Validation-bar reduction] full-solution build is unachievable (pre-existing Qt breakage)**
- **Found during:** Task 2 (baseline editor-target build, lcdui still present)
- **Issue:** Plan's validation bar was a green FULL-solution build (to catch the 6 out-of-closure editors). A baseline build of those 6 editor targets FAILED on `error MSB8066: Custom build for '...BaseMainWindow.ui...' exited with code 255` (AnimationEditor) — a Qt `.ui` uic/moc custom-build failure, pre-existing and independent of lcdui (lcdui itself built fine). So a green full-solution build is impossible today.
- **Fix:** Reduced the achievable validation bar to `/t:SwgClient` clean + dual-renderer boot gate (consistent with 12-01/12-02). Still removed lcdui/lgLcd from the editor files so they carry no dangling refs; their pre-existing Qt breakage is out of scope (tracked separately as a follow-up).
- **Verification:** `/t:SwgClient` exit 0; boot gate PASS.
- **Committed in:** f99c645dd

---

**Total deviations:** 2 (both build-truth discoveries; neither changed the removal intent — lcdui is fully gone and the client builds + boots).
**Impact on plan:** Removal completed correctly. The full-solution validation bar was the only unmet plan item, and it was unmeetable for reasons unrelated to lcdui (pre-existing Qt editor breakage). Reviewed/accepted by the operator at the boot gate.

## Issues Encountered
- Editor tools (AnimationEditor et al.) do not build today due to a pre-existing Qt `.ui` custom-build failure (MSB8066) — noted as a separate follow-up, not a Phase 12 regression.

## User Setup Required
None.

## Next Phase Readiness
- **DECRUFT-01/-02/-03 all complete** — Phase 12 closes the dual-renderer boot baseline for v2.1 Decruft.
- Phase 13 (DECRUFT-04, VideoCapture unlink) is unblocked. Note for 13/14/15: `.rsp` files are vestigial — edit the inline `.vcxproj` build inputs, and read the link output for unresolved externals (the project links under `/FORCE`, so exit 0 alone is not proof of a clean link).
- Koogie's uncommitted Direct3d9 working-tree changes left untouched.

---
*Phase: 12-orphaned-directory-project-deletes*
*Completed: 2026-05-25*
