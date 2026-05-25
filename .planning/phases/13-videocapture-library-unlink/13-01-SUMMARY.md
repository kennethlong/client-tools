---
phase: 13-videocapture-library-unlink
plan: 01
subsystem: build
tags: [msbuild, swg.sln, link-gate, FORCE, VERBOSE, videocapture, audiocapture, picn20, vcxproj, dead-code-removal, decruft]

requires:
  - phase: 12-orphaned-directory-project-deletes
    provides: dual-renderer boot baseline; build-input authority pattern (inline .vcxproj authoritative, .rsp vestigial)
provides:
  - VideoCapture middleware ATOMICALLY unlinked from SwgClient — the one live caller (CuiIoWin.cpp), the clientGraphics SwgVideoCapture wrapper, and all 15 capture lib tokens + 8 videocapture library-search paths removed in ONE commit (D-01)
  - SwgClient links clean Debug (blocking-primary) AND Release (confirmation) with the capture family gone — D-06 link-log gate satisfied
  - LOCKED, reusable Debug+Release link-log capture command + 3-condition Select-String gate (recorded verbatim below for Plan 13-03 Task 2 reuse)
affects: [13-02, 13-03, decruft]

tech-stack:
  added: []
  patterns:
    - "D-06 /FORCE false-pass guard: gate on grep of the linker /VERBOSE output (Searching ...lib present AND unresolved external == 0 AND LNK1181 == 0), NEVER on MSBuild exit code"
    - "Atomic link-unit removal (D-01): caller + wrapper + lib inputs land in ONE commit so the tree never holds a reference without a provider, even mid-step"
    - "Release link verbosity: ShowProgress=LinkVerbose (= linker /VERBOSE) gives the Release config the same Searching-lib capture the Debug config gets via AdditionalOptions /VERBOSE"

key-files:
  created: []
  modified:
    - src/engine/client/library/clientUserInterface/src/shared/core/CuiIoWin.cpp
    - src/engine/client/library/clientGraphics/build/win32/clientGraphics.vcxproj
    - src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj
  deleted:
    - src/engine/client/library/clientGraphics/src/shared/SwgVideoCapture.cpp
    - src/engine/client/library/clientGraphics/src/shared/SwgVideoCapture.h
    - src/engine/client/library/clientGraphics/include/public/clientGraphics/SwgVideoCapture.h

key-decisions:
  - "D-01 honored: caller (CuiIoWin.cpp) + wrapper (SwgVideoCapture.*) + lib inputs (SwgClient.vcxproj <Link>) removed in ONE atomic commit (bb2e101b8) — no stubs, no half-state."
  - "D-06 honored: pass/fail is the 3-condition link-log grep, not MSBuild exit. Debug is blocking-primary (live capture path compiles only under #if PRODUCTION==0); Release is required confirmation."
  - "DEVIATION (in scope): Release <Link> ShowProgress NotSet -> LinkVerbose. The plan assumed BOTH link logs would carry the /VERBOSE 'Searching ...lib' marker, but only the Debug config had /VERBOSE (via AdditionalOptions); the Release config had ShowProgress=NotSet and emitted zero markers. Flipping Release to LinkVerbose (= /VERBOSE) makes the locked command produce a gate-valid Release log. Committed permanently because Plan 13-03 reuses this exact command for the phase gate. binkw32/vivox/zlib/dpvs/crypto untouched."

patterns-established:
  - "Locked link-log capture + 3-condition gate (see below) — the canonical DECRUFT link-verification harness, reused by 13-03 and available to Phases 14-15."
---

# Plan 13-01 Summary — Atomic VideoCapture Link-Unit Removal

## What was built

The ATOMIC unlink of the VideoCapture middleware from SwgClient (RESEARCH edit-order steps 1-4,
D-01), landed as ONE commit (`bb2e101b8`):

1. **Caller** — `CuiIoWin.cpp`: removed `#include "clientGraphics/SwgVideoCapture.h"` and the
   `#if PRODUCTION==0 / VideoCapture::SingleUse::run(); / #endif` block between `IoWin::draw()` and
   `Bloom::postSceneRender()` (both neighbors left intact).
2. **Wrapper compilation** — `clientGraphics.vcxproj`: dropped the `SwgVideoCapture.cpp`
   ClCompile + `SwgVideoCapture.h` ClInclude items and the `...\external\3rd\library\videocapture`
   segment from `<AdditionalIncludeDirectories>` in all 3 configs.
3. **Wrapper files** — deleted `SwgVideoCapture.cpp`, `SwgVideoCapture.h`, and the public
   re-include header.
4. **Lib inputs** — `SwgClient.vcxproj`: excised the 15 capture lib tokens
   (`VideoCapture_debug/release`, `AudioCapture`, `CaptureCommon_debug/release`,
   `ImageCapture_debug/release`, `Smart_debug/release`, `SoeUtil_debug/release`,
   `ZlibUtil_debug/release`, `picn20md`, `picn20m`) and the 8 `videocapture\...\lib\win32`
   search paths from all 3 `<Link>` configs. `binkw32.lib`, `vivox*`, `zlib.lib`, `dpvs.lib`,
   `crypto.lib` preserved (negative control confirmed: `binkw32.lib` still present, `vivox` still present).

## Verification — D-06 link gate (PASS)

Gate is the grep of linker `/VERBOSE` output, NOT MSBuild exit code (the Debug linker is
`/FORCE`-armed; exit 0 is not proof). All three conditions met on the SAME log, per config:

| Config | (a) `Searching ...lib` markers | (b) `unresolved external symbol` | (c) `LNK1181`/`cannot open input file` | Verdict |
|--------|-------------------------------:|---------------------------------:|---------------------------------------:|---------|
| **Debug** (blocking-primary) | 2138 | 0 | 0 | ✅ |
| **Release** (confirmation)   | 2104 | 0 | 0 | ✅ |

The SwgClient Release `link.exe` command line was inspected directly: it carries `binkw32.lib`
and `vivox*` and **zero** capture-family lib tokens. `SwgClient_r.exe` re-linked and staged to
`stage/SwgClient_r.exe` (2026-05-25). Only benign LNK warnings remain (LNK4099 missing-PDB,
LNK4217 locally-defined-symbol-imported) — same family as Debug, not capture-related.

> Residue note: 6 `CL.exe /I ...\videocapture` compile lines remain in the build (clientGame +
> clientAudio) — these are include paths owned by **Plan 13-02** (not yet executed) and are INERT
> to the SwgClient link. Plan 13-03 owns the post-Wave-1-merge repo-wide grep.

## LOCKED link-log capture command + gate (verbatim — Plan 13-03 reuses this)

**Environment note:** run from a context where MSBuild is resolvable. Bare `msbuild` requires a VS
Developer prompt / Developer PowerShell; otherwise invoke the full path
`"D:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"`.
Add `/nodeReuse:false` so build nodes exit cleanly (orphaned reused nodes hold output-file locks
and silently truncate later builds — cost ~real time this plan; see Deviations/Pitfalls).

**Stale-obj mitigation (Pitfall 5) — run BEFORE each verification link:**
```
Remove-Item -ErrorAction SilentlyContinue src\compile\win32\clientGraphics\Debug\SwgVideoCapture.obj, src\compile\win32\clientGraphics\Release\SwgVideoCapture.obj
```

**Build + capture (PowerShell, from repo root):**
```
$msb = "D:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"
& $msb src\build\win32\swg.sln /t:SwgClient /p:Configuration=Debug   /p:Platform=Win32 /verbosity:detailed /nodeReuse:false 2>&1 | Out-File -Encoding utf8 link-debug.log
& $msb src\build\win32\swg.sln /t:SwgClient /p:Configuration=Release /p:Platform=Win32 /verbosity:detailed /nodeReuse:false 2>&1 | Out-File -Encoding utf8 link-release.log
```
> To FORCE the link to re-run (when the .exe is already up to date), delete the output first:
> `Remove-Item -ErrorAction SilentlyContinue src\compile\win32\SwgClient\<cfg>\SwgClient_*.exe`.

**3-condition gate (all on the SAME log file; NOT MSBuild exit code):**
```
# (a) /VERBOSE captured — guards against a false-green empty/skipped-link log
(Select-String -Path link-<cfg>.log -Pattern 'Searching .*\.lib').Count -gt 0
# (b) zero unresolved external
(Select-String -Path link-<cfg>.log -Pattern 'unresolved external symbol').Count -eq 0
# (c) zero missing-lib (deleted-lib-still-referenced; distinct from unresolved)
(Select-String -Path link-<cfg>.log -Pattern 'LNK1181|cannot open input file').Count -eq 0
```
Bash cross-reference: `grep -c 'Searching .*\.lib'` (>0), `grep -c 'unresolved external symbol'` (0),
`grep -Ec 'LNK1181|cannot open input file'` (0).

## Deviations / Pitfalls encountered

- **Release config lacked /VERBOSE** (plan assumption gap): fixed by ShowProgress NotSet -> LinkVerbose
  in SwgClient.vcxproj Release `<Link>` (committed; 13-03 depends on it). Debug already had `/VERBOSE`
  via `<AdditionalOptions>/SAFESEH:NO /VERBOSE</AdditionalOptions>` (pre-existing). Note: the Release
  linker is NOT `/FORCE`-armed (no `<ForceFileOutput>`), so a Release unresolved-external would hard-fail
  anyway — the marker is the false-green guard.
- **Recovery from a transient API 500** that interrupted the first executor mid-verification: its edits
  + Debug build were already correct and committed-worthy; this run re-captured the Release confirmation,
  committed the atomic unit, and authored this SUMMARY.
- **Orphaned MSBuild nodes + a hung `cl` (821 CPU-s) held build outputs locked**, truncating/failing 3
  build attempts ("Device or resource busy"). Resolved by killing stray MSBuild/cl/link processes and
  adding `/nodeReuse:false`. **Do not run the locked command from Git Bash** — MSYS path conversion
  mangles `/t:`/`/p:` switches (MSB1008); use PowerShell.

## Self-Check: PASSED

- [x] D-01 atomic: caller + wrapper + lib inputs in ONE commit (`bb2e101b8`), no stubs
- [x] Wrapper files deleted (3); clientGraphics.vcxproj + CuiIoWin.cpp clean (grep == 0)
- [x] SwgClient.vcxproj capture tokens/paths == 0; binkw32 + vivox preserved (negative control)
- [x] Debug link gate: 2138 / 0 / 0  •  Release link gate: 2104 / 0 / 0
- [x] Locked command + gate recorded verbatim for Plan 13-03
- [x] Only plan-owned files committed (unrelated Direct3d9/docs working-tree changes untouched)
