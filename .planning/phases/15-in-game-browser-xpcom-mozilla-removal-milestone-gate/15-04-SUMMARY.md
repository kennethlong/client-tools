---
phase: 15-in-game-browser-xpcom-mozilla-removal-milestone-gate
plan: 04
subsystem: infra
tags: [decruft, milestone-gate, lcdui, residue-sweep, dual-renderer, boot-gate, rasterMajor, decruft-07]

requires:
  - phase: 15-in-game-browser-xpcom-mozilla-removal-milestone-gate
    provides: 15-01..15-03 (full XPCOM/Mozilla removal set on disk) — the milestone gate runs only after the complete removal set
provides:
  - lcdui P12 residue cleaned (repo-wide lcdui grep == 0 except removal comment)
  - full v2.1 milestone residue sweep (P12-P15) == 0 / KEEP-listed
  - dual-renderer boot gate PASS (DECRUFT-07) — closes v2.1 Decruft
affects: [v2.1-milestone-close, v2.2-visual-parity]

tech-stack:
  added: []
  patterns: [milestone-closing residue sweep with KEEP-list; human-verify boot gate as the only ordinal-shift backstop]

key-files:
  modified:
    - src/game/client/library/swgClientUserInterface/build/win32/swgClientUserInterface.vcxproj
    - src/game/client/application/SwgGodClient/build/win32/SwgGodClient.vcxproj
    - "5 editor .vcxproj (Animation/ClientEffect/Lightning/Particle/Swoosh) — DEVIATION, dead lcdui\\lib residue"
    - "stage/client_d.cfg (gitignored — left at rasterMajor=11; not committed)"

key-decisions:
  - "lcdui A1 cleanup: removed include-path/lib-dir strings only; KEPT the SwgCuiG15Lcd.cpp ClCompile + .h ClInclude + the .cpp/.h on disk (live #ifdef-USE_LCD no-op stubs that ClientMain.cpp:361 + SwgCuiHud.cpp:678/686/1140 still link against). G15 is NOT a sweep token."
  - "DEVIATION (flagged): extended the A1 lcdui purge to 5 editor .vcxproj. The plan scoped A1 to swgClientUserInterface + SwgGodClient only, but the milestone sweep surfaced dead lcdui\\lib search-dirs in the editors (P12 residue the plan under-inventoried). D-12.1 requires lcdui repo-wide == 0, so cleaning them was necessary; editors don't build (Qt pre-broken), deletions-only, zero risk — same class 15-02 cleaned for Mozilla."
  - "Boot gate is the ONLY check that catches a TUIWebBrowser ordinal-shift regression (link + grep cannot). Exercised HUD/radial/IME focus + style-heavy UI pages under BOTH renderers."

patterns-established: []

requirements-completed: [DECRUFT-06, DECRUFT-07]

duration: ~35min (incl. ~25min Debug+Release recompile + human boot gate)
completed: 2026-05-26
---

# Phase 15-04: v2.1 Decruft Milestone Gate — Summary

**lcdui P12 residue cleaned, the full P12-P15 milestone residue sweep passes (== 0 / KEEP-listed), SwgClient links clean Debug+Release, and the client boots to character select under BOTH renderers (rasterMajor=5 and =11) with correct HUD/radial/IME focus + styling — v2.1 Decruft is closed.**

## Performance
- **Duration:** ~35 min (Debug recompile 14:03, Release 11:30, + human boot gate)
- **Completed:** 2026-05-26
- **Tasks:** 2 (Task 1 auto; Task 2 human-verify checkpoint)
- **Files:** 7 .vcxproj modified (+ gitignored client_d.cfg)

## Accomplishments
- **Task 1 (auto):**
  - A1 lcdui residue: stripped the 3 lcdui include-dir segments from swgClientUserInterface.vcxproj (×3 configs) + lcdui\Debug/lcdui\lib from SwgGodClient.vcxproj; KEPT SwgCuiG15Lcd.cpp/.h + their ClCompile/ClInclude (live stub symbols) + 989crypt.lib.
  - Milestone residue sweep (D-12.1): XPCOM family == 0; vivox == 0 (excl ChatAPI2/libs); lcdui == 0 except SwgCuiG15Lcd.cpp:26 removal comment; trackIR/SwgClientSetup → KEEP-listed survivors only (removal comments, checkTrackIr label, LaunchMeFirst launcher orphan); stationapi/videocapture/AudioCapture == 0. soePlatform/libs + ChatAPI2 + 989crypt preserved.
  - Build gate (D-13): SwgClient Debug AND Release link clean (unresolved external symbol == 0, grep-confirmed; Optimized EXEMPT per DEF-14-01).
- **Task 2 (human-verify boot gate, DECRUFT-07):** user confirmed char-select under rasterMajor=5 (D3D9) AND =11 (D3D11), no crash/assert, correct HUD/radial/IME focus + style-heavy UI page styling (the D-02 TUIWebBrowser ordinal-shift backstop), no browser surface. client_d.cfg left at rasterMajor=11.

## Task Commits
- Task 1 (lcdui cleanup + sweep): `d1dcb8447`
- Task 2 (boot gate): no code change (gitignored client_d.cfg left at rasterMajor=11); plan metadata + SUMMARY committed separately.

## Verification
- A1: `rg -i lcdui` over swgClientUserInterface.vcxproj + SwgGodClient.vcxproj → 0; SwgCuiG15Lcd.cpp/.h on disk + ClCompile/ClInclude kept (2). ✅
- Milestone sweep: XPCOM family == 0; vivox == 0; lcdui repo-wide == 0 except removal comment; P12/P13 KEEP-listed survivors only. ✅
- Build: Debug + Release `unresolved external symbol` == 0. ✅
- Boot gate (DECRUFT-07): char-select under rasterMajor=5 AND =11, correct focus/styling, no browser surface (human-confirmed). client_d.cfg = rasterMajor=11. ✅

## Decisions Made
See `key-decisions` frontmatter.

## Deviations from Plan
- **Extended A1 lcdui purge to 5 editor .vcxproj** (Animation/ClientEffect/Lightning/Particle/Swoosh): the plan scoped A1 to swgClientUserInterface + SwgGodClient only and claimed "lcdui over src == 0 after A1," but the milestone sweep surfaced dead `lcdui\lib` search-dirs in 5 editors (P12 residue the plan under-inventoried; NpcEditor + Viewer had none). D-12.1 requires lcdui repo-wide == 0, so cleaning them was necessary. Deletions-only, editors don't build (Qt pre-broken), zero risk — the same dead-residue class 15-02 cleaned for Mozilla.
- **Impact:** none beyond the intended grep-zero; no scope creep into other subsystems.

## Issues Encountered
- The plan's A1 KEEP-list note ("lcdui over src == 0 after A1") was inaccurate — under-inventoried the editor `lcdui\lib` residue (handled via the deviation above).

## Caveat (acknowledged, NOT a regression)
- D3D11 still has substantial visual-parity work outstanding (asset PS pipeline / eyes / mini-map) — this is the **v2.2 Visual Parity** milestone, a pre-existing deferral tracked since Phase 11, **not** a Decruft regression. DECRUFT-07's bar is "boots to character select under both renderers" — met.

## Next Phase Readiness
- v2.1 Decruft is the final milestone phase (Phase 15 of 12-15). DECRUFT-01..07 satisfied across Phases 12-15. Ready for milestone close (`/gsd:complete-milestone`). Next milestone: v2.2 Visual Parity.

---
*Phase: 15-in-game-browser-xpcom-mozilla-removal-milestone-gate*
*Completed: 2026-05-26*
