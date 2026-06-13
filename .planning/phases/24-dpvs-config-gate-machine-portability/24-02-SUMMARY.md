---
phase: 24-dpvs-config-gate-machine-portability
plan: 02
subsystem: infra
tags: [miles-audio, redist, vendored-binaries, postbuild, msbuild, clientAudio, machine-portability]

# Dependency graph
requires:
  - phase: 24-01
    provides: "phase context (DPVS config-gate sibling plan in the same wave; independent of this Miles-portability work)"
provides:
  - "Vendored Miles redist reconciled to the runtime-verified stage/miles byte-set + 2 newly-added .m3d 3D providers (Msseax.m3d, msssoft.m3d)"
  - "SwgClient postbuild Miles copy repointed from a machine-specific external path to the repo-relative vendored redist, with a full-codec-set (.m3d-keyed) repair guard on both Debug and Release blocks"
  - "Audio::install D-12 one-shot Miles codec-absence startup warning + AudioNamespace::s_milesCodecRedistAvailable flag that suppresses the per-sample warning flood"
affects: [machine-portability, fresh-clone-build, PORT-01, audio, 24-03-setup-script]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Repo-relative vendored-binary staging via additive postbuild xcopy guarded on the most-likely-missing markers (the .m3d providers) so stale/partial stages are repaired, not skipped"
    - "Loud-once-then-quiet failure surfacing: a single install-time presence probe sets a global flag that gates a per-sample warning site to eliminate log floods"

key-files:
  created:
    - "src/external/3rd/library/miles-7.2e/redist/Msseax.m3d (143,872 B — EAX 3D audio provider, was absent)"
    - "src/external/3rd/library/miles-7.2e/redist/msssoft.m3d (79,360 B — software 3D audio provider, was absent)"
  modified:
    - "src/external/3rd/library/miles-7.2e/redist/{mssogg.asi, mssmp3.asi, msseax.flt, mssdsp.flt, mssds3d.flt} (overwritten with proven stage/miles Oct-2017 bytes)"
    - "src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj (Debug + Release postbuild Miles copy)"
    - "src/engine/client/library/clientAudio/src/win32/Audio.cpp (install probe + flood-guard flag)"

key-decisions:
  - "redist = proven stage/miles Oct-2017 bytes + 2 .m3d (A1 reconciliation adopted; the May-2025 redist vintage was a different Miles point-release and is discarded)"
  - "Postbuild repair guard keyed on the .m3d providers (Msseax.m3d/msssoft.m3d), not mssmp3.asi, so a pre-reconciliation stage that has mssmp3.asi but lacks the .m3d set is re-staged (REVIEW Cursor HIGH / Codex MEDIUM)"
  - "Codec presence probe uses fopen (already used in Audio.cpp:1136) for a toolchain-independent file-existence check, building probe paths as redistDirectory + \\\\ + filename to match Miles load resolution"
  - "mssdolby.flt and msssrs.flt left in redist as-is (plan action only specified overwriting divergent files + adding .m3d; acceptance criteria do not require their removal)"

patterns-established:
  - "Pattern: vendored binary set sourced from a runtime-verified working copy, with byte-sizes recorded in the plan's A1 table as the provenance record"
  - "Pattern: per-sample diagnostic warnings gated on an install-time availability flag to prevent floods on a known-bad subsystem state"

requirements-completed: [PORT-01]

# Metrics
duration: ~15 min
completed: 2026-06-12
---

# Phase 24 Plan 02: Machine-Portable Miles Audio Redist Summary

**Vendored the runtime-verified Miles codec redist (proven stage/miles bytes + 2 newly-added .m3d 3D providers) into git, repointed the SwgClient postbuild from a machine-specific external path to the in-repo redist with a self-repairing .m3d-keyed guard, and made Miles codec absence loud-once-then-quiet (one Audio::install startup WARNING + an s_milesCodecRedistAvailable flag that silences the 141k-line per-sample flood).**

## Performance

- **Duration:** ~15 min (Tasks 1-3 code work; Task 4 is a pending human boot-verify checkpoint)
- **Started:** 2026-06-12 (worktree session)
- **Completed:** 2026-06-12 (code complete; checkpoint pending)
- **Tasks:** 3 of 4 (Task 4 = blocking human-verify checkpoint, cannot run in a worktree — see Next Phase Readiness)
- **Files modified:** 9 (7 redist binaries, 1 vcxproj, 1 Audio.cpp)

## Accomplishments
- Reconciled `src/external/3rd/library/miles-7.2e/redist/` to the proven Oct-2017 `stage/miles` byte-set (mssogg.asi 41,472 → 99,840; mssmp3.asi 95,744 → 94,208; the 3 `.flt` filters to stage sizes) and added the two absent 3D providers `Msseax.m3d` (143,872 B) + `msssoft.m3d` (79,360 B). `AudioCapture.flt` and the zero-byte `index.html` deliberately excluded.
- Repointed both the Debug and Release `PostBuildEvent` Miles copies to xcopy from the repo-relative `src\external\3rd\library\miles-7.2e\redist`, dropping the machine-specific `D:\Code\SWGSource Client v3.0\miles` source (the PORT-01 violation). The copy is now guarded on the `.m3d` providers so a stale/partial `stage/miles` is repaired, not skipped.
- Added a one-shot Miles codec/provider presence probe in `Audio::install` (after `AIL_set_redist_directory`/`AIL_startup`) that emits exactly one `WARNING` and sets `s_milesCodecRedistAvailable = false` when codecs are absent, and gated the per-sample `DEBUG_WARNING` flood site at the old `:1525` on that flag.

## Task Commits

Each task was committed atomically:

1. **Task 1: Reconcile vendored redist to proven stage/miles bytes + add 2 .m3d providers** - `c0ff0119a` (feat)
2. **Task 2: Repoint SwgClient postbuild Miles copy to vendored redist with full-codec-set guard (Debug + Release)** - `3bdd38e52` (feat)
3. **Task 3: D-12 one-shot Miles codec-absence startup warning + flood-suppression flag in Audio.cpp** - `58b4e2204` (feat)

**Task 4 (checkpoint:human-verify, blocking):** NOT executed — requires an MSBuild build + game launch + audio/log inspection, which cannot run inside a parallel worktree agent. Documented for the orchestrator/user to run post-merge (see Next Phase Readiness).

## Files Created/Modified
- `src/external/3rd/library/miles-7.2e/redist/Msseax.m3d` - EAX 3D audio provider (newly added, 143,872 B)
- `src/external/3rd/library/miles-7.2e/redist/msssoft.m3d` - software 3D audio provider (newly added, 79,360 B)
- `src/external/3rd/library/miles-7.2e/redist/mssogg.asi` - Ogg Vorbis decoder, overwritten with proven 99,840 B
- `src/external/3rd/library/miles-7.2e/redist/mssmp3.asi` - MP3 decoder, overwritten with proven 94,208 B
- `src/external/3rd/library/miles-7.2e/redist/{msseax,mssdsp,mssds3d}.flt` - 3D filters, overwritten with proven 59,392 / 56,832 / 12,800 B
- `src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj` - both postbuild Miles copies repointed to the vendored redist with the .m3d-keyed repair guard; exe/pdb copy lines unchanged
- `src/engine/client/library/clientAudio/src/win32/Audio.cpp` - `s_milesCodecRedistAvailable` flag, install-time codec-presence probe + one-shot warning, flood-site guard

## Decisions Made
- **redist = proven stage/miles bytes + 2 .m3d.** The existing redist held a different (May-2025) Miles point-release; the runtime-verified set is the Oct-2017 `stage/miles` bytes (the 2026-06-12 audio fix), so those bytes are canonical. Sourced from the main checkout's `stage/miles/` (the worktree had no `stage/miles`).
- **Repair guard keyed on the .m3d providers, not mssmp3.asi.** A pre-reconciliation stage can have `mssmp3.asi` yet lack the reconciled bytes / the new `.m3d` set; guarding on `Msseax.m3d`/`msssoft.m3d` (the files most likely absent) means `xcopy /Y` re-stages and repairs it (REVIEW Cursor HIGH / Codex MEDIUM).
- **fopen for the presence probe.** `fopen`/`FILE` is already used at `Audio.cpp:1136`, so the probe needs no new includes and is toolchain-independent. Probe paths are built as `redistDirectory + "\\" + filename` to resolve the same way Miles loads them (REVIEW/Codex path-resolution note).
- **mssdolby.flt / msssrs.flt left in place.** The plan's Task 1 action only directs overwriting the divergent files and adding the 2 `.m3d`; its acceptance criteria do not require removing these two pre-existing filters, so they were left as-is (the 9-file manifest in `<interfaces>` is described as the intended set, but no remove action was specified).

## Deviations from Plan

None - plan executed exactly as written.

The only adjustments were environmental, not plan deviations:
- The plan's `<verify>` PowerShell commands hardcode `cd "D:/Code/swg-client-v2"` (the main checkout). This agent runs in a worktree, so the same checks were run worktree-rooted with identical assertions — all passed (Task 1: byte-set + exclusions OK; Task 2: 6 redist refs / 0 external refs / both .m3d guards present; Task 3: flag + startup warning + flood guard present).
- The Task 2 verify regex `[regex]::Matches($p,'...\miles')` raises a .NET "Unrecognized escape sequence \m" error (`\m` is not a valid regex escape — a latent bug in the plan's verify snippet). The same acceptance criteria were proven instead with literal `String.Contains`/substring counting, which is the semantically-correct check (the criteria are about literal path substrings, not regex). Result was identical: criteria pass.

## Issues Encountered
- The worktree is a fresh checkout of HEAD and has no `stage/miles/` directory. The proven byte-set was read from the main checkout's absolute path `D:/Code/swg-client-v2/stage/miles/` (per the environment notes in the execution prompt). This is the documented expected source and not a problem.

## User Setup Required
None - no external service configuration required. (The portability fix itself removes the prior machine-specific external dependency.)

## Next Phase Readiness

**Code complete; Task 4 boot-verify is pending and BLOCKING for plan sign-off.** Task 4 is a `checkpoint:human-verify` that cannot run inside a parallel worktree agent (no game launch / audio judgement possible here). After the orchestrator merges this wave, a human should run the Task 4 verification:

1. **Postbuild + repair path:** Delete/rename `stage/miles`, rebuild SwgClient (Debug) via `$env:MSBUILD` — confirm the postbuild repopulated `stage/miles` from the vendored redist (`.asi/.flt/.m3d/.dll` present; `Msseax.m3d` = 143,872 B). Then test repair: in a `stage/miles` that has `mssmp3.asi` but is missing `Msseax.m3d`, rebuild → confirm the `.m3d` guard re-xcopies.
2. **Audio plays:** Launch `stage/SwgClient_d.exe` (rasterMajor=11). Confirm music + in-world audio play (not just UI rollovers), and the log shows NO 141k "Error loading and allocating the sample" flood.
3. **Failure-injection:** Rename `stage/miles` to `stage/miles_off`, launch — confirm the client boots (audio half-dead acceptable), the log shows EXACTLY ONE Miles-absence startup WARNING and NO per-sample flood (the `s_milesCodecRedistAvailable` guard fires). Restore `stage/miles` afterward.

If all three pass, PORT-01 (Miles half) is satisfied. The DPVS config-gate (24-01) and the cfg-templating setup script (24-03) are the remaining Phase 24 portability/config work; 24-03's `setup-client.ps1` will provide the second (setup-time) codec-validation gate (D-12a) referenced by this plan's startup probe.

---
*Phase: 24-dpvs-config-gate-machine-portability*
*Completed: 2026-06-12 (code); Task 4 human boot-verify pending*

## Self-Check: PASSED

- Created files exist on disk: `Msseax.m3d`, `msssoft.m3d`, `SwgClient.vcxproj`, `Audio.cpp` — all FOUND.
- Commit hashes present in git log: `c0ff0119a`, `3bdd38e52`, `58b4e2204` — all FOUND.
- Task 1 verify (worktree-rooted): byte-set + exclusions → OK.
- Task 2 verify (literal-substring, regex-escape bug bypassed): redistCount=6 (≥4), extCount=0, m3dGuard=True → OK.
- Task 3 verify (worktree-rooted): flag=True, startupWarn=True, floodGuard=True → OK.
- Only caveat: Task 4 (human boot-verify) is unexecuted-by-design in a worktree; flagged BLOCKING in Next Phase Readiness.
