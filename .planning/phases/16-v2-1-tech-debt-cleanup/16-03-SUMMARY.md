---
phase: 16-v2-1-tech-debt-cleanup
plan: 03
subsystem: build
tags: [regression-gate, link-gate, boot-smoke, d3d11, decruft, verification-only]

# Dependency graph
requires:
  - phase: 16-v2-1-tech-debt-cleanup
    provides: "Plan 16-01 (SwgGodClient dead-token removal, grep-only) + Plan 16-02 (in-closure UI-lib source edits) — the removals this gate validates"
provides:
  - "D-08 link gate: SwgClient Debug + Release relinked clean (0 unresolved external symbol in both link logs) via swg.sln /t:SwgClient"
  - "D-08 boot gate: human-verified rasterMajor=11 (D3D11) boot past character-select into world (UpTime ~670s) with clean init"
affects: []

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Link-log grep gate (not msbuild exit code): /FORCE (LNK4075) can exit 0 while masking unresolved externals, so the D-08 gate greps the captured link log for 'unresolved external symbol' == 0"
    - "Forced relink: delete stage/SwgClient_{d,r}.exe before building so a real relink occurs (not a stale-binary no-op)"
    - "Build THROUGH swg.sln /t:SwgClient so ProjectDependencies rebuild the edited clientUserInterface / swgClientUserInterface libs before the SwgClient relink (avoids linking a stale .lib)"

key-files:
  created: []
  modified: []   # verification-only plan — no source edits

key-decisions:
  - "D-08 boot gate signed off as PASSED: the D3D11 build booted, reached character-select, and ran ~11 min in-world on Tatooine — char-select under D3D11 reached, init clean. (Operator approved 2026-05-27.)"
  - "An in-world Options-window crash was observed but is a CONFIRMED pre-existing defect UNRELATED to phase 16 — see Issues Encountered + the captured todo. It is outside the D-08 gate criteria (char-select boot + clean init) and outside the phase-16 diff."

patterns-established:
  - "No-behavior-change phase gate: a single rasterMajor=11 boot replaces the full =5/=11 dual-renderer matrix when the phase is removal-only"

requirements-completed: []

# Metrics
duration: ~15min (build + human boot)
completed: 2026-05-27
---

# Phase 16 Plan 03: D-08 link + boot regression gate Summary

**Proved the Plan-02 dead-code removals did not break SwgClient: Debug + Release relinked clean (0 unresolved externals in both link logs, Optimized EXEMPT), and a rasterMajor=11 (D3D11) boot reached character-select and ran ~11 minutes in-world — operator-confirmed, no init regression.**

## Performance

- **Duration:** ~15 min (incremental relink of 2 configs + human boot smoke)
- **Completed:** 2026-05-27
- **Tasks:** 2 (1 automated link gate, 1 human-verify boot checkpoint)
- **Files modified:** 0 (verification-only plan)

## Accomplishments
- **D-08 link gate (Task 1) — PASS.** Deleted `stage/SwgClient_d.exe` + `stage/SwgClient_r.exe`, then built `SwgClient` Debug and Release through `src/build/win32/swg.sln /t:SwgClient` (so the solution `ProjectDependencies` rebuilt the edited `clientUserInterface` / `swgClientUserInterface` libs first), each with `/nodeReuse:false`, via the full VS18 MSBuild path. Captured each build to its own log.
  - Debug link log: **0** `unresolved external symbol`.
  - Release link log: **0** `unresolved external symbol`.
  - Both exes deleted-then-recreated → a real relink occurred (not a stale binary).
  - Optimized config correctly **not built** (EXEMPT — pre-existing DEF-14-01 SAFESEH LNK1281).
  - Only LNK *warnings* present, both pre-existing and unrelated to this cleanup: `LNK4075` (/INCREMENTAL ignored due to /FORCE) and `LNK4217` (`_xmlFree` import from libxml2/sharedXml). No `error LNK`, both configs report "Build succeeded."
  - This confirms no removal in Plan 02 took a live symbol with it (zero callers of the voice-volume API; the `finalUrl` block was genuinely dead).
- **D-08 boot gate (Task 2) — PASS (operator-confirmed).** The freshly relinked `stage/SwgClient_d.exe` launched under `rasterMajor=11` (D3D11; `client_d.cfg:37`, no cfg edit needed), connected to the SWGSource VM, and reached character-select. The session ran ~670s (~11 min) in-world on Tatooine before an unrelated crash — far past the char-select gate. Init was clean; renderer was D3D11.

## Task Commits

Verification-only plan — no source commits. Build outputs are transient (`$env:TEMP\swgclient-link-{debug,release}.log`); the relinked binaries land in `stage/` via Koogie's post-build copy. This SUMMARY + tracking updates are the plan's only committed artifacts.

## Files Created/Modified
None (verification-only). Build produced `stage/SwgClient_d.exe` (Debug) + `stage/SwgClient_r.exe` (Release) via relink.

## Decisions Made
- **Executed inline by the orchestrator, not via gsd-executor.** The plan requires a PowerShell MSBuild invocation (memory `project_decruft_removal_build_graph_gotchas`: build from PowerShell, not Git Bash); the `gsd-executor` subagent has Bash but not PowerShell, and Task 2 is an interactive human checkpoint. Driving it inline played to the available tooling.
- **D-08 boot gate signed off despite an in-world crash** because the crash is a confirmed pre-existing, unrelated defect (below) and the gate criteria (reach character-select under D3D11, clean init) were objectively met (the client ran ~11 min in-world).

## Deviations from Plan
None to the plan's gate logic. The plan's PowerShell recipe was followed (full msbuild path, `/nodeReuse:false`, delete-exe-first, swg.sln `/t:SwgClient`, Debug+Release only, log-grep gate). Execution venue was inline-orchestrator rather than a spawned executor (rationale above).

## Issues Encountered
- **Pre-existing Options-window crash (NOT a phase-16 regression).** While in-world, opening the Options window FATAL'd at `CuiMediator.cpp:1487`: *"Unable to find CodeData property 'checkShowToolbarCommandCooldownTimer' from [/GroundHUD.OptMain.comp.target.ui] for [SwgCuiOptUi]"*.
  - **Root cause:** `SwgCuiOptUi.cpp:219` calls `getCodeDataObject(TUICheckbox, checkbox, "checkShowToolbarCommandCooldownTimer")` — the C++ Options UI requires a checkbox the loaded `.ui` layout doesn't define. A **code↔UI-data mismatch** introduced by feature commit `d1b3c0eaf "Add Toolbar Cooldown Timer Display Support"`, which predates phase 16.
  - **Why it is NOT phase 16:** the entire phase-16 diff is 4 files (`CuiPreferences.cpp/.h`, `SwgGodClient.vcxproj`, `SwgCuiHudAction.cpp`) — none is `SwgCuiOptUi`/`SwgCuiOpt`/`CuiMediator`/any `.ui`. The link succeeded with 0 unresolved externals (no live symbol removed; voice-volume API had zero callers incl. in the Options UI). `SwgCuiHudAction.cpp(892)` in the stack is the generic HUD-action dispatcher that *opens* Options, ~280 lines before the deleted `finalUrl` block (~1170-1189) and in a different TU from the crash.
  - **Disposition:** captured as `.planning/todos/pending/2026-05-27-options-toolbar-cooldown-ui-data-mismatch-crash.md`. Out of Decruft/phase-16 scope.

## Known Stubs
None.

## User Setup Required
None.

## Next Phase Readiness
- Phase 16 (v2.1 Tech-Debt Cleanup) removal+gate work is complete. All Phase 16 grep-zero gates (Plans 01 + 02) are green and the D-08 link + boot gate passed.
- Protected Koogie Direct3d9 WIP + untracked research docs in the working tree were left untouched/uncommitted throughout.

## Self-Check: PASSED

- FOUND: `.planning/phases/16-v2-1-tech-debt-cleanup/16-03-SUMMARY.md`
- VERIFIED: Debug link log `unresolved external symbol` count == 0
- VERIFIED: Release link log `unresolved external symbol` count == 0
- VERIFIED: `stage/SwgClient_d.exe` + `stage/SwgClient_r.exe` deleted then re-created (real relink)
- VERIFIED: Optimized config not built (EXEMPT)
- VERIFIED (operator): rasterMajor=11 D3D11 boot reached character-select, clean init (~11 min in-world before unrelated pre-existing crash)
- Protected Koogie Direct3d9 WIP + untracked research docs: untouched/uncommitted

---
*Phase: 16-v2-1-tech-debt-cleanup*
*Completed: 2026-05-27*
