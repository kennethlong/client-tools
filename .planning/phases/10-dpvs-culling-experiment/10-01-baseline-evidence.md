# Phase 10 Wave 0 — Baseline Evidence

**Recorded:** 2026-05-11T05:00:00Z
**Tree:** koogie-msvc-cpp20-base @ ae2652da89d9563e07b0153209a415fd5d461955

## Build Smoke

- Command: `msbuild src/build/win32/swg.sln -t:SwgClient -p:Configuration=Debug -p:Platform=Win32 -p:PlatformToolset=v145 -verbosity:minimal -m`
- MSBuild: VS 2026 Community v18.5.4 (`D:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe`).
- Toolset: v145 (C++20).
- Exit code: 1 (NOT a real build failure — see "Build Smoke — Detailed Reading" below).
- Log: `10-01-baseline-build.log` (79 lines).
- Errors count (in-scope): **0**.
- Errors count (out-of-scope; pre-existing): 2 (both `MSB3073` from the same Koogie-leftover post-build copy step on `SwgClient.vcxproj`; SwgClient.exe was already produced when these fired).
- Warnings count (informational): 8 (`MSB8012` TargetPath-vs-OutputFile mismatches on the 4 EXE-producing projects — Direct3d9, Direct3d9_vsps, Direct3d9_ffp, SwgClient — and pre-existing from the Koogie merge).

### Build Smoke — Detailed Reading

The plan's `<verify><automated>` line builds the whole solution (`swg.sln`).
That fails with editor/tool errors (SwgGodClient, MayaExporter,
ParticleEditor, ConversationEditor, SwgSpaceQuestEditor, etc.), but **all
editors and tools are explicitly out-of-scope per `PROJECT.md` §Out of Scope
("Editor / tool builds ... deferred until SwgClient itself ships")**. The
Phase 9 closeout invocation pattern (see `build-log-replan3-01.txt`) used
`/t:SwgClient` to scope the build to the in-scope target tree — Phase 10
follows the same convention (Rule 1 deviation: align the build command with
the actual project scope; documented for the SUMMARY).

After scoping to `-t:SwgClient`, the build log is **clean for the
in-scope target**:

- 60+ static `.lib` projects build cleanly (lines 3–63 of the log).
- DPVS plugin DLL builds cleanly (line 44).
- `Direct3d9.dll`, `Direct3d9_vsps.dll`, `Direct3d9_ffp.dll` build (lines 66, 69, 72).
- **`SwgClient.vcxproj -> SwgClient.exe`** linked successfully (line 75).
- Then `MSB3073` fires on the post-build `copy "D:\SWG\client-tools\..."
  "D:\SWG\SWGSource Client v3.0\"` step because those paths exist on
  Koogie's dev machine, not on this one. Pre-existing from the Koogie merge
  (`479d35df3`); identical failure in `build-log-replan3-01.txt` from Phase 9 closeout.

Per the scope-boundary rule in the executor instructions
(`get-shit-done/workflows/execute-plan.md` §Deviation Rules), pre-existing
out-of-scope failures are deferred, not fixed in this plan. The Koogie
post-build copy is logged for `deferred-items.md` if a future plan needs to
unblock the editor builds (Phase 12+ candidate).

### Artifact integrity

| Artifact | Path | Size | mtime |
|----------|------|------|-------|
| Compile output | `src/compile/win32/SwgClient/Debug/SwgClient_d.exe` | 72,541,696 bytes | 2026-05-10 19:31:24 |
| Stage  | `stage/SwgClient_d.exe` | 72,541,696 bytes | 2026-05-10 19:32:18 |

Both files match by size; mtime predates this build because the build was
incremental — nothing in SwgClient's dep closure changed between Phase 9
closeout (commit `460f4540d`) and the start of Phase 10 Wave 0 (commit
`ae2652da8`). The msbuild run traversed all targets and considered them
up-to-date; that is the correct outcome for a baseline-green check.

## Boot Smoke

- Date: 2026-05-10
- Operator: Kenny
- Client: `D:/Code/swg-client-v2/stage/SwgClient_d.exe`
- Server: SWGSource VM 192.168.1.200:44453
- GPU: NVIDIA GeForce RTX 3060
- Driver: 94.6.2f.0.7e (Windows registry format; recorded verbatim per operator report)
- Result: **PASS — char-select reached cleanly; Tatooine zone-in also confirmed PASS.**
- Notes: First-launch clean — no login retries needed (the "back-out + retry login
  twice" first-launch flakiness noted in Phase 9 closeout did NOT recur this
  session). Operator reported reaching Tatooine directly, with no relogins.
  The Phase 9 closeout regression check (Tatooine still loads against the
  SWGSource VM at 192.168.1.200) holds — no regression introduced by the
  Wave 0 scaffolding commits (no source-edits land in Wave 0).

Procedure (executed by Kenny per Task 5):

1. Confirm SWGSource VM at 192.168.1.200 is up (Oracle started, stationchat
   running, ant build current; per memory `project_vm_server_setup.md`). ✓
2. Launch `D:/Code/swg-client-v2/stage/SwgClient_d.exe`. ✓
3. Log in with the test account; reach character-select screen. ✓ (clean, no
   relogins required)
4. Select a Tatooine character and zone in (Phase 9 closeout regression check). ✓
5. Quit cleanly. ✓
6. Result/Notes lines populated above with the verified outcome. ✓

## Phase 9 Closeout Reference

Per `.planning/phases/09-stlport-msvc-stl/09-02-SUMMARY.md` (in whitengold
research-history tree at `D:/Code/swg-client/.planning/...`; the `.planning/`
phase 9 records did not migrate into v2), the v2 tree was confirmed
building cleanly and **zoning to Tatooine** on 2026-05-10 (commit
`460f4540d`, log `D:/Code/swg-client-v2/stage/log-replan3-02.txt`, evidence
screenshot `evidence/09-02-tatooine.png`). This Wave 0 smoke re-confirms the
same state immediately before Phase 10 Wave 1 lands the first source-edits
(the DPVS instrumentation).

Specifically:

- **Build identity:** SwgClient.exe size (72,541,696 bytes) and mtime match
  the Phase 9 closeout binary — no in-scope project rebuilt this run.
- **Out-of-scope pre-existing failures unchanged:** the Koogie post-build
  copy fails identically (same `MSB3073`, same paths) in
  `build-log-replan3-01.txt` (Phase 9 closeout) and `10-01-baseline-build.log`
  (this Phase 10 baseline). No regression introduced.
- **Runtime contract preserved:** stage/ DLLs are 2010-leak set per memory
  `project_2016_dll_runtime_invalidated.md`; staging produced by the Phase 9
  closeout build; no changes since.

If the boot-smoke result in Task 5 is anything other than "char-select
reached, optionally Tatooine zoned-in clean," Wave 1 instrumentation does
NOT start — root-cause first.
