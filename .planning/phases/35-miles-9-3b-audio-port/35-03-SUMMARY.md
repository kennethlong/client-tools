---
phase: 35-miles-9-3b-audio-port
plan: 03
subsystem: audio
tags: [miles, mss, audio, x64, link, vcxproj, postbuild, redist, dumpbin, force-link-gate]

# Dependency graph
requires:
  - phase: 35-miles-9-3b-audio-port
    provides: "35-01 vendored mss32.lib + mss64.lib (miles-9.3b/lib/win) + redist/redist64 sets; 35-02 ported the room_type/codec-probe call sites and removed the Phase-33 x64 Miles disables, making AIL_* LIVE on x64"
  - phase: 33-x64-build-platform-d3d9-renderers
    provides: "the first full x64 link (gl05/06/07 + SwgClient); Phase-33 had dropped all Miles libs from the x64 link block"
provides:
  - "SwgClient.vcxproj Miles lib dir repointed to miles-9.3b/lib/win across all 5 link blocks (Win32 Debug/Optimized/Release + x64 Debug/Release); the dual miles/ + miles-7.2e/ lib-dir token is gone"
  - "mss64.lib added to both x64 link blocks (re-arms the now-live AIL_* externals); Win32 keeps mss32.lib"
  - "Win32 PostBuild (Debug/Release/Optimized) sources miles-9.3b/redist with a 9.3b-only mssds3d.flt sentinel; x64 PostBuild (Debug/Release) stages miles-9.3b/redist64 to stage-x64/miles"
  - "PROVEN: x64 SwgClient links 0 unresolved + imports mss64.dll (dumpbin /imports) with AIL_* resolved from miles-9.3b\\lib\\win\\mss64.lib (/VERBOSE:LIB); Win32 links 0 unresolved + 0 C2660"
  - "full 9.3b provider set physically staged per platform (stage/miles + stage-x64/miles), 0 .m3d after the stale-7.2e repair"
affects: [35-04, miles-audio-port, x64-audio-boot]

# Tech tracking
tech-stack:
  added: []
  patterns: [per-config-lib-repoint, force-link-grep-plus-dumpbin-provenance, sentinel-guarded-9.3b-postbuild, explicit-stale-stage-delete, buildprojectreferences-false-link-isolation]

key-files:
  created:
    - .planning/phases/35-miles-9-3b-audio-port/35-03-SUMMARY.md
    - .planning/phases/35-miles-9-3b-audio-port/deferred-items.md
  modified:
    - src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj

key-decisions:
  - "Win32 sentinel keyed on mssds3d.flt (9.3b-only-vs-clean, NOT a 7.2e detector); the real stale-7.2e repair is the explicit Remove-Item stage\\miles before the build (concern 3)"
  - "x64 SwgClient link driven directly with /p:BuildProjectReferences=false because the full /t:SwgClient x64 build aborts on a pre-existing gl07 (Direct3d9_vsps) PostBuild Sysnative copy failure — out of scope, logged to deferred-items"
  - "Optimized|Win32 got a NEW PostBuild block (it had none) mirroring Debug with _o naming (Cursor MEDIUM)"
  - "PROVENANCE proven two ways under /FORCE: dumpbin /imports SwgClient_d.exe shows mss64.dll, and /VERBOSE:LIB shows 'Loaded mss64.lib(mss64.dll)' from miles-9.3b\\lib\\win (concern 2)"

patterns-established:
  - "per-config-lib-repoint: verify the miles-9.3b\\lib\\win token PER link block (5 configs), not a whole-file grep -c, so a Debug-only edit cannot pass falsely"
  - "force-link-grep-plus-dumpbin-provenance: under /FORCE, unresolved==0 proves RESOLUTION not PROVENANCE; pair it with dumpbin /imports (mss64.dll) + /VERBOSE:LIB lib-path grep + dumpbin /headers machine-type"
  - "explicit-stale-stage-delete: a 9.3b-only .flt sentinel CANNOT repair a stale Win32 7.2e stage (7.2e ships the same .flt names); only Remove-Item stage\\miles repairs it"

requirements-completed: [AUDIO-01, AUDIO-02]

# Metrics
duration: ~18min
completed: 2026-06-18
---

# Phase 35 Plan 03: Re-arm the Miles 9.3b Link (mss64.lib add + per-platform redist staging) Summary

**Repointed SwgClient.vcxproj's Miles lib dir to miles-9.3b across all 5 link blocks, added mss64.lib to both x64 link blocks (Phase-33 had dropped it), and staged the full 9.3b provider set per platform via sentinel-guarded PostBuilds — then proved BOTH platforms link with 0 unresolved external symbols AND that the x64 AIL_* externals resolved from the real miles-9.3b\\lib\\win\\mss64.lib (dumpbin /imports mss64.dll + /VERBOSE:LIB), after deleting the stale 7.2e stage that the Win32 sentinel cannot detect.**

## Performance

- **Duration:** ~18 min
- **Started:** 2026-06-18T~16:05Z
- **Completed:** 2026-06-18
- **Tasks:** 3
- **Files modified:** 1 (SwgClient.vcxproj)

## Accomplishments

- **Lib repoint + mss64.lib add (Task 1):** replaced the dual `miles\lib\win;miles-7.2e\lib\win` token with the single `miles-9.3b\lib\win` dir in all 5 link blocks (Win32 Debug/Optimized/Release + x64 Debug/Release); added `mss64.lib` to both x64 AdditionalDependencies. Verified PER-CONFIG (concern 2): 5 configs carry the 9.3b lib dir, 0 old tokens survive, 2 x64 blocks carry mss64.lib, Win32 keeps mss32.lib.
- **PostBuild repoint + x64 staging (Task 2):** Win32 Debug/Release PostBuild now source `miles-9.3b\redist` guarded by a 9.3b-only `mssds3d.flt` sentinel (the three 7.2e `.m3d`/`mssmp3` guards collapsed into one); ADDED an Optimized|Win32 PostBuild block (previously absent, Cursor MEDIUM) mirroring Debug with `_o` naming; ADDED x64 Debug+Release PostBuild blocks sourcing `miles-9.3b\redist64` to `stage-x64\miles` guarded by `mss64ds3d.flt`. No 7.2e `.m3d` sentinel or 7.2e redist source survives.
- **Stale-stage delete + clean build + dual-platform link (Task 3):** Remove-Item'd the stale 7.2e `stage\miles` (had Msseax.m3d/msssoft.m3d) + empty `stage-x64\miles`; deleted the clientAudio Debug objs for BOTH platforms (the stale x64 Audio.obj from Phase-33 stubs — concern 10) and the SwgClient exes (force relink). Built `/t:SwgClient` Debug for both platforms.
- **RESOLUTION GATE green (both platforms):** x64 + Win32 each grep `unresolved external symbol` == 0, `LNK1181` == 0, `error C`/`fatal` == 0. Win32 additionally `C2660` == 0 — the room_type call sites now compile against the real 9.3b prototypes, proving 35-02's port is correct on Win32 (the C2660 list 35-01 surfaced is gone).
- **PROVENANCE GATE green (concern 2):** `dumpbin /imports stage-x64\SwgClient_d.exe` shows `mss64.dll` (count 1) — positive proof an AIL_* path resolved through mss64.lib, not a /FORCE tombstone; the `/VERBOSE:LIB` log shows `Searching ...miles-9.3b\lib\win\mss64.lib:` → `Loaded mss64.lib(mss64.dll)` (×8). Staged-DLL machine types: `stage-x64\miles\mss64.dll` = 8664 (x64), `stage\miles\mss32.dll` = 14C (x86). Exe machine = 8664.
- **Providers staged + .m3d cleared:** full x64 set (mss64.dll, mp3/ogg .asi, dsp/eax/ds3d/dolby/srs .flt, binkawin64.asi) in stage-x64/miles; full Win32 set (+ mssvoice.asi) in stage/miles; 0 .m3d in either (the stale-7.2e repair worked).

## Task Commits

1. **Task 1: repoint Miles lib dir to miles-9.3b + add mss64.lib (x64)** - `2b8431d5a` (build)
2. **Task 2: repoint Win32 PostBuild to 9.3b redist + add x64 redist64 staging** - `dd5a7dd96` (build)
3. **Task 3: delete stale stages, clean clientAudio, build+link both platforms, prove provenance** - no source change (build-only; the deliverable is the green dual-platform link + staged 9.3b binaries + the gitignored build logs)

**Plan metadata:** (final commit) `docs(35-03): complete Miles 9.3b link re-arm plan`

## Files Created/Modified

- `src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj` - 5-config lib repoint to miles-9.3b/lib/win + mss64.lib in both x64 link blocks; Win32 Debug/Release/Optimized PostBuild repointed to 9.3b redist (9.3b sentinel); x64 Debug/Release PostBuild add redist64 -> stage-x64/miles staging
- `.planning/phases/35-miles-9-3b-audio-port/deferred-items.md` - records the pre-existing gl07 Sysnative PostBuild defect (out of scope)

## Per-config verification (concern 2)

| Gate | Result |
|------|--------|
| `miles-9.3b\lib\win` (per-config, want >=5 + each block read) | 5 (Win32 Debug 147, Optimized 214, Release 260, x64 Debug 324, Release 381) |
| old `miles\lib\win` / `miles-7.2e\lib\win` token | 0 |
| `mss64.lib` (x64 Debug 322 + Release 379, both read) | 2 |
| `mss32.lib` (Win32 kept) | 3 |
| `miles-9.3b\redist` (Win32 PostBuild Debug/Release/Optimized) | 6 (comment + xcopy ×3) |
| `miles-9.3b\redist64` (x64 Debug/Release PostBuild) | 4 |
| `stage-x64\miles` (x64 staging target) | 4 |
| `Msseax.m3d`/`msssoft.m3d` sentinel | 0 |
| `miles-7.2e\redist` source | 0 |

## Build/link gates (Task 3, all PASS)

| Gate | x64 | Win32 |
|------|-----|-------|
| `unresolved external symbol` | 0 | 0 |
| `LNK1181` | 0 | 0 |
| `error C` / `fatal error` | 0 | 0 |
| `C2660` (room_type port proof) | n/a | 0 |
| exe produced + staged | SwgClient_d.exe 55.9MB | SwgClient_d.exe 70.1MB |

**PROVENANCE (x64):** `dumpbin /imports` mss64.dll count = 1; `/VERBOSE:LIB` = `Loaded mss64.lib(mss64.dll)` from `miles-9.3b\lib\win`; exe machine 8664; staged mss64.dll 8664; staged mss32.dll 14C.

**Staged providers:** stage-x64/miles = {mss64.dll, mss64mp3.asi, mss64ogg.asi, mss64dsp.flt, mss64eax.flt, mss64ds3d.flt, mss64dolby.flt, mss64srs.flt, binkawin64.asi}; stage/miles = {mss32.dll, mssmp3.asi, mssogg.asi, mssdsp.flt, msseax.flt, mssds3d.flt, mssdolby.flt, msssrs.flt, mssvoice.asi, binkawin.asi}. `.m3d` count = 0 in both.

## Decisions Made

See `key-decisions` in frontmatter. Headline: the explicit `Remove-Item stage\miles` is the ONLY thing that repaired the stale 7.2e Win32 stage (the 9.3b sentinel can't detect it — concern 3); the x64 link was isolated with `/p:BuildProjectReferences=false` to step past a pre-existing gl07 PostBuild failure; provenance was proven two ways under /FORCE per concern 2.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] x64 SwgClient link blocked by a pre-existing gl07 (Direct3d9_vsps) PostBuild Sysnative copy failure**
- **Found during:** Task 3 (first full `/t:SwgClient /p:Platform=x64` build)
- **Issue:** the full x64 SwgClient target aborts with `MSB3073 :VCEnd exited with code 1` in the gl07 PostBuildEvent — `copy /Y "%SystemRoot%\Sysnative\d3dcompiler_47.dll"`. `Sysnative` is a 32-bit-process alias to System32; the VS18 MSBuild runs 64-bit, so the path does not resolve. This is a Phase-33 (`9531ba73b`) defect in `Direct3d9_vsps.vcxproj`, NOT in any Miles/SwgClient file touched by this plan, and is therefore OUT OF SCOPE.
- **Fix (workaround, not a fix to the defect):** the gl05/06/07 x64 import libs were already current (Phase 33, Jun 17) and clientAudio x64 recompiled cleanly this run, so the SwgClient x64 link was driven directly via `MSBuild SwgClient.vcxproj /p:BuildProjectReferences=false` — which links the already-built plugin libs without re-running their PostBuilds. The SwgClient resolution + provenance gates all passed. The underlying gl07 Sysnative defect is logged to `deferred-items.md` for a future plan (suggested fix: `%SystemRoot%\System32` or an `if not exist` guard).
- **Files modified:** none (build-invocation choice; defect lives in a Phase-33 file, left untouched)
- **Verification:** x64 SwgClient_d.exe produced 55.9MB, 0 unresolved, imports mss64.dll, machine 8664
- **Committed in:** n/a (no source change; recorded in deferred-items.md)

---

**Total deviations:** 1 (Rule 3 - blocking, handled by link-invocation isolation; underlying out-of-scope defect deferred)
**Impact on plan:** None on the plan's deliverable — both platforms link clean and provenance is proven. The deferred gl07 defect does not affect the Miles port; it only affects a from-clean full-solution x64 build (the gl0x DLLs already exist and stage correctly).

## Issues Encountered

- **MSYS mangles `dumpbin /headers` / `/imports`:** running dumpbin from Git Bash interpreted `/headers` as a Unix path (`LNK1181 cannot open input file 'C:\Program Files\Git\headers'`). Re-ran all dumpbin provenance checks from PowerShell (per AGENTS.md "run from PowerShell, MSYS mangles /flag args"). Build logs are UTF-16LE (BOM fffe) — analysis used `tr -d '\000'` before grep.
- **The Win32 bash `grep -c 'miles-9.3b\\lib\\win'` returned 0** due to bash backslash escaping on Windows; the ripgrep-based Grep tool confirmed 5 (correct). Used the Grep tool for all path-with-backslash gates.

## Commit-sequencing compliance (concern 6 / HIGH)

This plan's clean x64 link is the gate that UNBLOCKS pushing 35-02 (which removed the Phase-33 stubs, making AIL_* live on x64). Per `<commit_sequencing>`: 35-02 (`d59682a54`, `17c18d99e`, `b933a61e2`) + 35-03 (`2b8431d5a`, `dd5a7dd96`, + this metadata commit) form a LINKING pair and may be pushed TOGETHER now that Task 3's resolution + provenance gates are green. **No push was performed by this executor** (per AGENTS.md "commit only when the user asks; do not push unless asked") — the user pushes when ready; do NOT push 35-02 without 35-03.

## Next Phase Readiness

- **35-04 ready:** both platforms link clean against 9.3b, the full provider set is staged next to each exe, and the x64 provenance is proven. 35-04 (human UAT) can boot SwgClient_d.exe (Debug reads client_d.cfg) under stage/ (Win32) and stage-x64/ (x64), confirm `disableMiles=false`, and verify audio/reverb with no warning-flood.
- **Carry-forward:** the gl07 Sysnative x64 PostBuild defect (deferred-items.md) should be fixed before relying on a from-clean full-solution x64 build; it does not block 35-04 because the x64 stage already has all gl0x DLLs + d3dcompiler_47.dll + the Miles providers + the SwgClient exe.
- **Editor vcxprojs** (8 files) still reference the old miles tree — deferred TODO from 35-01, not a Phase-35 gate.

## Self-Check: PASSED

- All 3 files verified present (35-03-SUMMARY.md, deferred-items.md, SwgClient.vcxproj)
- Both task commits verified in git log (`2b8431d5a`, `dd5a7dd96`)
- Task 3 produced no source commit by design (build-only; gates green, binaries staged, logs gitignored)

---
*Phase: 35-miles-9-3b-audio-port*
*Completed: 2026-06-18*
