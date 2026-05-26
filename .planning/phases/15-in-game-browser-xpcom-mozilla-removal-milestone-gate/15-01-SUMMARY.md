---
phase: 15-in-game-browser-xpcom-mozilla-removal-milestone-gate
plan: 01
subsystem: ui
tags: [xpcom, mozilla, libmozilla, webbrowser, decruft, vcxproj, uitypeid, tcg, link-gate, v145]

requires:
  - phase: 14-voice-chat-vivox-source-removal
    provides: atomic closed-chain delete pattern (14-01) + /FORCE link-grep gate + DEF-14-01 Optimized-exempt precedent
provides:
  - XPCOM/Mozilla browser surface removed from source + the SwgClient link (DECRUFT-06 source/link half)
  - 3 SwgCuiWebBrowser* units deleted (the only libMozilla:: consumers)
  - TUIWebBrowser deleted outright from UITypeID.h (no placeholder; D-02 SAFE-TO-DELETE-OUTRIGHT)
  - SwgClient.vcxproj Mozilla-family link tokens/dirs/includes removed (3 configs)
affects: [15-02, 15-03, 15-04, decruft-milestone-gate]

tech-stack:
  added: []
  patterns: [atomic non-buildable-intermediate delete unit (one commit after build gate); token-level vcxproj purge via exact-content script]

key-files:
  created: []
  modified:
    - src/external/3rd/library/ui/src/shared/core/UITypeID.h
    - src/game/client/library/swgClientUserInterface/src/shared/core/SwgCuiManager.cpp
    - src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserUI.cpp
    - src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHud.cpp
    - src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHudAction.cpp
    - src/game/client/library/swgClientUserInterface/src/shared/core/SwgCuiTcgManager.cpp
    - src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiTcgControl.h
    - src/engine/client/library/clientUserInterface/src/shared/core/CuiIoWin.cpp
    - src/engine/client/library/clientUserInterface/src/shared/core/IMEManager.cpp
    - src/engine/client/library/clientGame/src/shared/core/Game.cpp
    - src/game/client/library/swgClientUserInterface/build/win32/swgClientUserInterface.vcxproj
    - src/engine/client/library/clientGame/build/win32/clientGame.vcxproj
    - src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj
  deleted:
    - src/game/client/library/swgClientUserInterface/src/shared/core/SwgCuiWebBrowserManager.cpp + .h
    - src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiWebBrowserWidget.cpp + .h
    - src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiWebBrowserWindow.cpp + .h
    - src/game/client/library/swgClientUserInterface/include/public/swgClientUserInterface/SwgCuiWebBrowser{Manager,Widget,Window}.h

key-decisions:
  - "TUIWebBrowser deleted outright with NO ordinal-preserving placeholder (D-02): unlike the Phase-14 CR-01 enum, the UITypeID ordinal is never serialized (string-keyed .ui loader, no WebBrowser TypeName). The *Style-family ordinal shift is the boot-gate backstop (15-04)."
  - "TCG browser tie severed by unregistering navigateProc/navigateWithPostDataProc entirely (D-04/D-04a): libEverQuestTCG CallbackTable static-inits null + SWGTCG.dll absent, so the null members are never read. libEverQuestTCG + all other TCG infra preserved (27 refs intact)."
  - "Edited inline .vcxproj (authoritative), not the vestigial .rsp (15-02's scope). Mozilla tokens removed via exact-content token filter (split on ; / drop libMozilla|xpcom|xul|nspr4|plc4|profdirserviceprovider_s / rejoin) — avoids the MSYS-sed backslash trap."
  - "Build gated on Debug + Release link-grep (unresolved external symbol == 0), NOT MSBuild exit 0 (/FORCE masks). Optimized NOT gated (DEF-14-01 SAFESEH LNK1281, pre-existing)."

patterns-established:
  - "Atomic non-buildable-intermediate unit: de-wire + enum-delete + source-delete + link-unlink are uncompilable in isolation; one commit only after the whole change links clean."

requirements-completed: [DECRUFT-06]

duration: ~55min (incl. ~28min Debug+Release builds + msbuild-path detour)
completed: 2026-05-26
---

# Phase 15-01: Atomic XPCOM/Mozilla Browser-Surface Removal — Summary

**The in-game web-browser surface (3 SwgCuiWebBrowser* units, the TUIWebBrowser RTTI enum, every live caller + the 5 IsA sites, the TCG browser tie, and the SwgClient Mozilla-family link tokens) is removed as one atomic, build-gated change; SwgClient links clean Debug AND Release with zero unresolved externals.**

## Performance
- **Duration:** ~55 min (Debug build 12:03, Release build 15:57; plus an msbuild-toolset detour — see Issues)
- **Completed:** 2026-05-26
- **Tasks:** 3 (executed as ONE atomic unit per plan design)
- **Files:** 13 modified, 9 deleted

## Accomplishments
- Deleted the 3 `SwgCuiWebBrowser*` units (6 src + 3 public re-includes) — the only `libMozilla::` consumers.
- De-wired every surviving caller (SwgCuiManager install/remove/update; SwgCuiCommandParserUI browser/url/debugBrowserOutput console commands; SwgCuiHud + SwgCuiHudAction; engine CuiIoWin ×2 + IMEManager) and all 5 `IsA(TUIWebBrowser)` sites.
- Severed the TCG browser tie (navigateProc/navigateWithPostDataProc decls+defns+registrations; SwgCuiTcgControl::IsA clause) — `libEverQuestTCG` and all other TCG infra preserved.
- Deleted `TUIWebBrowser` from `UITypeID.h` outright (no placeholder) and the dead `Game.cpp` `#if DEBUG==0 / #include libMozilla` block.
- Removed inline libMozilla include-dirs from `swgClientUserInterface.vcxproj` + `clientGame.vcxproj`, and all Mozilla-family link tokens/lib-dirs/include-dirs from `SwgClient.vcxproj` (3 configs).

## Task Commits
Committed as ONE atomic unit (the de-wire/enum-delete/source-delete/link-unlink are non-buildable in isolation; D-13). Single commit `feat(15-01): remove XPCOM/Mozilla in-game browser surface from source + SwgClient link`.

## Verification
- **Source grep-zero** (canonical callers/TCG/enum/Game.cpp): `SwgCuiWebBrowser|WebBrowserManager|TUIWebBrowser|libMozilla|mozillaBrowserOutput` → 0. ✅
- **Enum**: `TUIWebBrowser` → 0, `TUIRunner` → 1 (enum intact, only the one value removed). ✅
- **Scope-preserve**: `libEverQuestTCG` → 27 in SwgCuiTcgManager; sibling `*Browser` UIs → 6 in SwgCuiHudAction (untouched). ✅
- **vcxproj token grep-zero** (SwgClient + swgClientUserInterface + clientGame): `libMozilla|xpcom|xul|nspr4|plc4|profdirserviceprovider` → 0; `legacy_stdio_definitions`/`soePlatform` kept (5); lcdui kept (3, → 15-04). ✅
- **Build gate (D-13)**: Debug link log `unresolved external symbol` == 0 (SwgClient_d.exe 70.1 MB); Release == 0 (SwgClient_r.exe 28.4 MB). Optimized NOT gated (DEF-14-01). ✅

## Decisions Made
See `key-decisions` frontmatter (TUIWebBrowser outright-delete; TCG callback unregister; inline-vcxproj authoritative; link-grep gate).

## Deviations from Plan
- **browser/url CommandNames decls removed; 2 pre-commented table rows left as-is.** The plan action listed deleting the `browser` decl; the `url` decl was removed alongside it for coherence (both are the dead web-browser console-command names; no live consumer remained after the `/* */` handler block + `#if DEBUG==0` handler were deleted). The two ALREADY-commented table rows (`//{CommandNames::browser,...}`, `//{CommandNames::url,...}`) were left in place per the plan's own `<interfaces>` NOTE — they are inert comments and carry no grep-gate token (`browser`/`url`/`Web Browser` are not in the GATE-XPCOM set). No compile or gate impact (build passed with them present).
- **Impact:** none — all gates pass; no scope creep.

## Issues Encountered
- **MSBuild toolset detour (~10 min):** first build invoked VS 2022 Community's MSBuild → 61× `MSB8020: build tools for v145 cannot be found` (VS 2022 has v143/v170, not v145). The repo's projects target `PlatformToolset=v145` (MSVC 14.50). Correct MSBuild = VS 2026 / VS18 (`C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe`), which resolves v145 via `VC/Auxiliary/Build/Microsoft.VCToolsVersion.v145.default.props`. Memory note updated. The v145→v180 naming gap (MSVC-version vs product-version scheme) is the trap.
- Editing `UITypeID.h` (a core UI header) triggered a large recompile across the UI/engine graph — expected; both configs still linked clean.

## Next Phase Readiness
- 15-02 (.rsp + editor/SwgGodClient residue purge) and 15-03 (drop libMozilla from swg.sln + delete vendored tree) can proceed. 15-03's Wave-1 merge gate will confirm 15-01 + 15-02 landed before the irreversible tree delete.
- The TUIWebBrowser ordinal-shift backstop (HUD/radial/IME focus + style-heavy UI pages under both renderers) is exercised at the 15-04 boot gate.

---
*Phase: 15-in-game-browser-xpcom-mozilla-removal-milestone-gate*
*Completed: 2026-05-26*
