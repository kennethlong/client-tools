---
phase: 24-dpvs-config-gate-machine-portability
plan: 03
subsystem: infra
tags: [powershell, cfg-template, machine-portability, dpvs, miles, gitignore, docs]

# Dependency graph
requires:
  - phase: 24-01
    provides: "engine-default flips (multi-stream VB default, D3D11 Bloom no-op) so the cfg override keys can be omitted; the occlusionMode config gate the template ships"
  - phase: 24-02
    provides: "machine-portable vendored Miles redist + postbuild repoint that the setup script validates"
provides:
  - "tools/setup/client.cfg.template — tracked cfg source-of-truth with PORT-01 placeholders + PORT-02-cleaned key set"
  - "tools/setup/setup-client.ps1 — cfg generator (TRE-root prompt with cfg-breaking-char rejection, repo-relative override auto-substitution, login-server param, renderer/resolution params, miles validation, next-steps banner)"
  - ".gitignore stale :84 comment corrected (src/cmake/config -> tools/setup)"
  - "docs/build.md Runtime asset setup section rewritten to the setup-client.ps1 workflow"
affects: [phase-26-instrumentation-removal, onboarding, fresh-clone]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Tracked cfg template + PowerShell generator (successor to the dead CMake configure_file story)"
    - "One parameterized template emits both client_d.cfg + client.cfg; the script supplies the per-config differences"

key-files:
  created:
    - tools/setup/client.cfg.template
    - tools/setup/setup-client.ps1
  modified:
    - .gitignore
    - docs/build.md

key-decisions:
  - "One parameterized template emitting both cfgs (RESEARCH Open Q2), with per-TOC/Tree tokens (@TRE_ROOT_TOC0@ etc.) so each search path is independently quoted"
  - "Debug cfg ships occlusionMode=auto (Phase-23 posture), Release cfg ships occlusionMode=off (D-02 shipped Option alpha default); both via the script, not a single template value"
  - "runtimeDisableAsynchronousLoader removed outright in the template (V->R); skipSplash templated to true (matches client.cfg); swg_dev_bundle omitted — final V-key boot verdicts pending the Task 4 human checkpoint"
  - "Generated cfgs written ASCII to stage/ (gitignored); template under tools/setup/ tracked"

patterns-established:
  - "PORT-01 path templating: @TRE_ROOT@ family prompted/param, @OVERRIDE_PATH@ auto-substituted from repo root, @LOGIN_SERVER@/@RASTER_MAJOR@/@RESOLUTION_*@/@SKIP_SPLASH@/@OCCLUSION_MODE@ params"
  - "V5 input validation: Test-Path -PathType Container + reject double-quote/CR/newline before substitution (T-24-07)"

requirements-completed: [PORT-01, PORT-02]

# Metrics
duration: ~20 min (tasks 1-3; Task 4 boot gate pending human checkpoint)
completed: 2026-06-13
---

# Phase 24 Plan 03: Machine-Portable cfg Template + Generator Summary

**Tracked client.cfg.template (PORT-02-cleaned key set with @TRE_ROOT@/@OVERRIDE_PATH@/@LOGIN_SERVER@/@RASTER_MAJOR@ placeholders + occlusionMode=auto) and a setup-client.ps1 generator that validates the TRE root, auto-substitutes the clone's own stage/override path, validates the Miles codec set, and prints a next-steps banner — replacing the dead CMake configure_file story.**

## Performance

- **Duration:** ~20 min (Tasks 1-3 complete; Task 4 boot gate is a blocking human checkpoint)
- **Tasks:** 3 of 4 executed (Task 4 is a `checkpoint:human-verify` requiring built binaries + a fresh-clone boot — cannot run in a parallel worktree)
- **Files created:** 2 (`tools/setup/client.cfg.template`, `tools/setup/setup-client.ps1`)
- **Files modified:** 2 (`.gitignore`, `docs/build.md`)

## Accomplishments

- **Task 1 — Cleaned cfg template (PORT-01 + PORT-02):** Authored `tools/setup/client.cfg.template` from the Debug cfg. PORT-01 placeholders: `@TRE_ROOT@` + per-search `@TRE_ROOT_TOC0..3@`/`@TRE_ROOT_SNOW@`/`@TRE_ROOT_SWGSOURCE@`, `@OVERRIDE_PATH@` (repo-relative auto-substitute), `@LOGIN_SERVER@`, `@RASTER_MAJOR@`, `@RESOLUTION_WIDTH@`/`@RESOLUTION_HEIGHT@`, `@SKIP_SPLASH@`, `@OCCLUSION_MODE@`. PORT-02 cleanup: removed `swg_dev_bundle`, `disableMultiStreamVertexBuffers` (24-01 flipped the engine default), `[ClientGame/Bloom]` (24-01 no-op'd the slot), `disableG15Lcd`, `voiceChatEnabled`, `[Direct3d11] reportFrameStats`, the stale CMake "re-run cmake" header, and all Phase-11/17/18 inline essays. Added the new `[ClientGraphics/Dpvs] occlusionMode` knob (D-01). Kept `allowTearing=true` (D3D9 gate) and `reportInstrumentation` (Phase-26 removal).
- **Task 2 — setup-client.ps1 + docs/gitignore:** Generator mirrors the `show-window.ps1` `[CmdletBinding()]/param` form. Validates `$TreRoot` via `Test-Path -PathType Container` AND rejects double-quote/CR/newline (T-24-07 / Codex LOW). Auto-substitutes the repo-relative `stage/override` from `$PSScriptRoot/../..` (no user input). Substitutes login server, rasterMajor (rejects the FATAL `9`), resolution. Writes both `stage/client_d.cfg` (occlusionMode=auto) and `stage/client.cfg` (occlusionMode=off). D-12a: `Test-Path` the miles codec set (`mssmp3.asi`/`mssogg.asi`/`Msseax.m3d`/`msssoft.m3d`), warns without failing. Prints success summary + next-steps banner. Fixed the `.gitignore:84` stale comment and rewrote the `docs/build.md` Runtime asset setup section to the new workflow.
- **Task 3 — V-key template state:** The final decided V-key state was folded into the Task 1 template (no separate edit/commit needed): `runtimeDisableAsynchronousLoader` removed, `skipSplash` templated (no `=false`), `swg_dev_bundle` absent. The boot+play verdicts that justify these removals are captured in the Task 4 human checkpoint (require built binaries on both renderers).

## Functional verification performed (in-worktree)

- Task 1 verify (PowerShell): tokens present, 0 dead-key matches. PASS.
- Task 2 verify (PowerShell): script has CmdletBinding/Test-Path/tokens/cfg-breaking-char reject; docs reference setup-client.ps1; gitignore no longer references src/cmake/config; `git check-ignore` confirms generated cfg ignored + template tracked. PASS.
- Task 2 end-to-end run: ran `setup-client.ps1 -TreRoot "D:/Code/SWGSource Client v3.0/" -NoPrompt -RasterMajor 11`. Result: 0 leftover `@...@` tokens, correct sku TOC paths, `@OVERRIDE_PATH@` resolved to this clone's own repo-relative `stage/override`, `occlusionMode=auto` (Debug) / `off` (Release), `loginServerAddress0=192.168.1.200`, 0 dead keys, miles warning fired correctly. Generated cfgs confirmed gitignored. PASS.
- Task 3 verify (PowerShell): template has no `swg_dev_bundle`, no `skipSplash=false`, 0 `runtimeDisableAsynchronousLoader` key-assignments. PASS.

## Task Commits

1. **Task 1: Author cleaned cfg template** - `51f952909` (feat)
2. **Task 2: setup-client.ps1 + gitignore + build.md** - `494841d7f` (feat)
3. **Task 3: V-key final template state** - folded into `51f952909` (no separate edit required; the template already encodes the final decided key set, verdicts pending Task 4 boot gate)

## Files Created/Modified

- `tools/setup/client.cfg.template` (new) - tracked cfg source-of-truth with PORT-01 placeholders + PORT-02-cleaned keys + occlusionMode knob
- `tools/setup/setup-client.ps1` (new) - cfg generator with TRE-root validation, repo-relative override substitution, login/renderer/resolution params, miles validation, next-steps banner
- `.gitignore` (modified) - corrected the stale :84 comment (src/cmake/config -> tools/setup)
- `docs/build.md` (modified) - Runtime asset setup section rewritten to the setup-client.ps1 workflow

## Decisions Made

- Used distinct per-search-path tokens (`@TRE_ROOT_TOC0@` etc.) rather than a single `@TRE_ROOT@` prefix concatenated in the cfg, so the script emits each path fully-quoted (paths contain spaces and ConfigFile space-splits unquoted values). The bare `@TRE_ROOT@` token is still present (TOCTreePath) per the plan's placeholder contract.
- Debug emits `occlusionMode=auto` and Release emits `off` (the plan permitted either a single value or a per-config split; chose the split so Debug devs keep the Phase-23 posture while the shipped Release default stays Option alpha per D-02).
- Generated cfgs written with `-Encoding ASCII` to match the engine's ConfigFile expectations and avoid a BOM.

## Deviations from Plan

None - plan executed exactly as written. (Task 3 produced no additional template edit because the final V-key state was already authored into the Task 1 template, as the plan's `<done>` for Task 1 anticipated; the boot/play judgments are deferred to the Task 4 checkpoint as the plan specifies.)

## Issues Encountered

None during Tasks 1-3. Task 4 (the dual-renderer + fresh-clone boot gate) cannot execute in a parallel worktree: it requires built binaries, a real fresh clone, and an interactive boot to character select on both rasterMajor=5 and =11. This is a blocking `checkpoint:human-verify` and is returned to the orchestrator for human execution.

## Next Phase Readiness

- Template + generator are complete and functionally verified in-worktree (token substitution, override auto-substitution, miles validation, gitignore/docs correctness).
- **BLOCKING checkpoint (Task 4):** human must run the fresh-clone test (D-10) + dual-renderer boot gate (rasterMajor=5 and =11) to confirm boot-to-character-select with audio, and record the V-key boot+play verdicts (runtimeDisableAsynchronousLoader removal, skipSplash, swg_dev_bundle absence) on both renderers. If any V-key removal regresses, the template must be amended to restore the key with a documenting comment.

## Self-Check: PASSED

- FOUND: tools/setup/client.cfg.template
- FOUND: tools/setup/setup-client.ps1
- FOUND commit: 51f952909 (Task 1)
- FOUND commit: 494841d7f (Task 2)
- .gitignore + docs/build.md modifications committed in 494841d7f

---
*Phase: 24-dpvs-config-gate-machine-portability*
*Completed (Tasks 1-3): 2026-06-13 — Task 4 boot gate pending human checkpoint*
