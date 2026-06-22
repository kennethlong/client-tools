---
phase: 38-utinni-advertised-client-coverage-completion
plan: 04
subsystem: infra
tags: [utinni, getenginehookpoints, contract-resync, epa-08, dx11-evidence, handback, final-build, dumpbin, win32]

# Dependency graph
requires:
  - phase: 38-03
    provides: "94-row table + 94-name .inc + UTINNI_HOOKPOINTS_VERSION 2 + the Bucket-4 confirm-or-OMIT ledger + the accumulated EPA-08 handback items (groundScene::g_instance OMIT, client::writeCrashLog/setupStartDataInstall NONEXISTENT, cuiChatWindow::ctor DEFER, Issue #11)"
provides:
  - "utinni_engine_hookpoints.{h,inc} re-synced byte-identical (sha256-verified) into D:/Code/Utinni/UtinniCore/swg/ at version 2 (overwrote the 37-03 v1 copies); Utinni repo NOT committed by this phase"
  - "the dated EPA-08 handback doc .planning/handoff/2026-06-22-utinni-provider-coverage-handback.md: advertised 16-row ledger + version 2 + LOCKED DX11 evidence + the full consumer-side worklist + OMIT/DEFER/NONEXISTENT ledger"
  - "final canonical 5-target Debug+Release build evidence: 0 unresolved external symbol; undecorated GetEngineHookPoints on _d (ord 52) + _r (ord 51); static_assert (94==94) holds"
affects: [utinni]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Cross-repo shared-contract sync = byte-identical cp into D:/Code/Utinni/UtinniCore/swg/ + sha256/cmp verification; the consumer repo is NOT committed from this repo (each side commits its own; the X-macro .inc is the single name source so provider+consumer expand the same file)"
    - "EPA-08 = documentation-only DX11 readiness handback (D-01 LOCKED): transcribe the dumpbin + create/destroy ordering proof; NO provider code change; the 0xC0000005 is consumer-side"

key-files:
  created:
    - .planning/handoff/2026-06-22-utinni-provider-coverage-handback.md
  modified: []

key-decisions:
  - "Contract re-synced byte-identical into D:/Code/Utinni/UtinniCore/swg/ (the 37-03 sync path; v1->v2 overwrite). sha256 + cmp confirm byte-identity on both .h and .inc. The Utinni repo shows the 2 files modified-but-uncommitted; this phase does NOT commit there (separate repo, commits its own side) per D-03 / the plan."
  - "EPA-08 is evidence-only (D-01 LOCKED): the handback transcribes the gl11 create/destroy ordering proof (device+context FIRST line 435, swapChain LAST line 574, destroy resets swapChain FIRST line 684) -> all-three-live across the swapChain's whole non-null lifetime -> the 0xC0000005 target=0x34 is CONSUMER-side, no provider change."
  - "Boot-smoke + live inject-smoke recorded as the MAINTAINER's remaining step (BUILD-GATE-autonomous execution mode); NOT run, NOT claimed here. The build-gate (0 unresolved + undecorated dumpbin export + static_assert 94==94) is the autonomous pass condition for this final wave."
  - "_o/Optimized SAFESEH LNK1281 is PRE-EXISTING and NOT in the canonical 5-target set (Direct3d11;Direct3d9;Direct3d9_ffp;Direct3d9_vsps;SwgClient) -> not a gate; validation bar = Debug+Release per AGENTS.md + 38-CONTEXT.md."

requirements-completed: [EPA-07, EPA-08]

# Metrics
duration: 14min
completed: 2026-06-22
---

# Phase 38 Plan 04: Contract Re-sync + EPA-08 Handback + Final Build Summary

**The Phase-38 contract (94 endpoints, UTINNI_HOOKPOINTS_VERSION 2) re-synced byte-identical (sha256-verified) into D:/Code/Utinni/UtinniCore/swg/ (v1->v2 overwrite; Utinni NOT committed here); the dated EPA-08 handback doc written transcribing the LOCKED gl11 DX11 ordering proof (no provider change; the 0xC0000005 is consumer-side) plus the full advertised-row ledger and the consumer-side worklist (DX11 tryInstall guard, virtual vtable resolution, installable() skip drops, the cuiChatWindow::ctor DEFER, the 2 NONEXISTENT OMITs, Issue #11); final canonical 5-target Debug+Release build links 0 unresolved with undecorated GetEngineHookPoints on _d (ord 52) + _r (ord 51) and static_assert (94==94) holds. Boot-smoke + live inject-smoke recorded as the maintainer's remaining step.**

## Performance

- **Duration:** ~14 min
- **Started:** 2026-06-22T21:32:00Z
- **Completed:** 2026-06-22T21:46:00Z
- **Tasks:** 1 auto (re-sync + handback) + 1 checkpoint (final build-gate run autonomously; boot-smoke DEFERRED to the maintainer)
- **Files modified:** 1 created (the handback doc); 0 source changes (re-sync is a cross-repo file copy, not a swg-client-v2 source edit)

## Accomplishments

- **Contract re-sync (byte-identical, verified):** `utinni_engine_hookpoints.h` and `.inc` copied into
  `D:/Code/Utinni/UtinniCore/swg/`, overwriting the 37-03 v1 copies. Post-copy `sha256sum` + `cmp`
  confirm byte-identity on BOTH files:
  - `.h`  sha256 `8c7ff88a64fa7434004bfffdd3c7a571be9118b573833c92fd002e7a60b881ed` (identical in both repos)
  - `.inc` sha256 `501d88deb1b5d9903af248b85266b60b57c0657d6874fc56393dc2f2e263f8d6` (identical in both repos)
  Dest now shows `#define UTINNI_HOOKPOINTS_VERSION 2` (was 1). The Utinni repo shows the 2 files
  modified-but-uncommitted — this phase does NOT `git add`/`git commit` there (separate repo).
- **EPA-08 handback doc** written: `.planning/handoff/2026-06-22-utinni-provider-coverage-handback.md`,
  dated 2026-06-22, self-contained, with all four required sections (advertised ledger, version 2,
  DX11 evidence, consumer-side worklist) + the OMIT/DEFER/NONEXISTENT ledger.
- **Final canonical 5-target build** (`Direct3d11;Direct3d9;Direct3d9_ffp;Direct3d9_vsps;SwgClient`)
  Debug AND Release: 0 unresolved external symbol, 0 LNK1181, 0 LNK1120, 0 error C; both SwgClient exes
  relinked (compile-output exes deleted first to force the link gate) + staged.
- **dumpbin /exports** undecorated `GetEngineHookPoints` on both exes (consistent with 38-01/02/03):
  `SwgClient_d.exe` ord **52**, `SwgClient_r.exe` ord **51**.
- **static_assert (94 == 94)** held implicitly — both builds compiled (the count check is compile-time;
  no "drift" diagnostic in either log). Version 2 compiled into both binaries' `s_table.version`.

## Task Commits

1. **Task 1: EPA-08 handback doc + Utinni v2 contract re-sync** - `a94416360` (docs)
2. **Task 2: final build-gate (5-target Debug+Release) + dumpbin** - no source change; build-artifact
   evidence only (the re-sync target dir is the Utinni repo, intentionally not committed here).

**Plan metadata:** (this SUMMARY + STATE/ROADMAP/REQUIREMENTS) committed separately.

## Files Created/Modified

- `.planning/handoff/2026-06-22-utinni-provider-coverage-handback.md` (NEW) — the dated EPA-08 handback
  (advertised ledger, version 2 + sha256-verified re-sync note, DX11 evidence, consumer-side worklist,
  OMIT/DEFER/NONEXISTENT ledger, the maintainer boot-smoke step).
- `D:/Code/Utinni/UtinniCore/swg/utinni_engine_hookpoints.h` + `.inc` (in the SEPARATE Utinni repo) —
  re-synced byte-identical to the swg-client-v2 originals; NOT committed by this phase.

## Build-Gate Evidence (AUTONOMOUS pass condition for this final wave)

- **Canonical 5-target set:** `Direct3d11;Direct3d9;Direct3d9_ffp;Direct3d9_vsps;SwgClient`
  (`src\build\win32\swg.sln`, `$env:MSBUILD` = VS18 BuildTools, `/nodeReuse:false`, `/m:1` serial,
  run from PowerShell; compile-output `SwgClient_d.exe`/`_r.exe` deleted first to force the relink).
- **Debug** (`/p:Configuration=Debug /p:Platform=Win32`): `unresolved external symbol` = **0**,
  LNK1181 = 0, LNK1120 = 0, `error C` = 0. EXIT 0. Relinked
  `src/compile/win32/SwgClient/Debug/SwgClient_d.exe` (mtime 16:33) + staged `stage/SwgClient_d.exe`;
  all 4 gl0X_d.dll staged (16:30). Log `build-38-04-debug.log` (UTF-16LE, decoded via `iconv`).
- **Release** (`/p:Configuration=Release /p:Platform=Win32`): `unresolved external symbol` = **0**,
  LNK1181 = 0, LNK1120 = 0, `error C` = 0. EXIT 0. Relinked
  `src/compile/win32/SwgClient/Release/SwgClient_r.exe` (mtime 16:36) + staged `stage/SwgClient_r.exe`;
  all 4 gl0X_r.dll staged (16:33). Log `build-38-04-release.log`.
- **dumpbin /exports** (VS18 BuildTools x64 dumpbin, run from PowerShell — Git Bash mangles `/exports`):
  - `SwgClient_d.exe` -> `83   52 00B29220 GetEngineHookPoints = _GetEngineHookPoints` (undecorated, ordinal 52)
  - `SwgClient_r.exe` -> `82   51 00700250 GetEngineHookPoints = _GetEngineHookPoints` (undecorated, ordinal 51)
- **static_assert** `(sizeof s_engineHookPoints / sizeof[0]) == UTINNI_REQUIRED_COUNT`: held — both
  builds compiled (94-row table == 94-name `.inc`; no drift diagnostic). Version 2 compiled in.
- **Pre-existing, NOT a gate:** `_o`/Optimized `LNK1281 SAFESEH` — the Optimized flavor is not in the
  canonical 5-target set; validation bar = Debug + Release (AGENTS.md / 38-CONTEXT.md).

**Boot-smoke + live inject-smoke (Task 2 checkpoint:human-verify):** RECORDED as the **maintainer's
remaining step**, NOT run and NOT claimed here (BUILD-GATE-autonomous execution mode for this run):
1. `SwgClient_d.exe` under `rasterMajor=5` to char-select -> `utinni_verifyNoNullNoDup()` PASS for the
   94-name set at version 2 (no null / no dup / name-set equal).
2. `SwgClient_r.exe`/gl11 under `rasterMajor=11` -> window up, no crash (D3D11 + SWGEmu non-regression).
3. The live inject-smoke OUT OF REPO (Utinni injection) -> no `0xC0000005`, `endpoints: resolved 94/94
   by name` (the DX11 overlay leg additionally requires the consumer-side `tryInstall()` device+context
   guard documented in the handback §4a).
The build-gate above is the autonomous pass condition for this final wave.

## Re-sync verification transcript (for the EPA-07 audit)

```
SOURCE .h  sha256: 8c7ff88a64fa7434004bfffdd3c7a571be9118b573833c92fd002e7a60b881ed
DEST   .h  sha256: 8c7ff88a64fa7434004bfffdd3c7a571be9118b573833c92fd002e7a60b881ed   -> cmp: H BYTE-IDENTICAL
SOURCE .inc sha256: 501d88deb1b5d9903af248b85266b60b57c0657d6874fc56393dc2f2e263f8d6
DEST   .inc sha256: 501d88deb1b5d9903af248b85266b60b57c0657d6874fc56393dc2f2e263f8d6  -> cmp: INC BYTE-IDENTICAL
DEST .h version : #define UTINNI_HOOKPOINTS_VERSION 2
Utinni git status: ' M UtinniCore/swg/utinni_engine_hookpoints.h' + '.inc' (modified, NOT committed by this phase)
```

## Decisions Made

- **Byte-exact cross-repo sync, no Utinni commit.** The `.h`/`.inc` were `cp`-copied into
  `D:/Code/Utinni/UtinniCore/swg/` and verified byte-identical (sha256 + cmp). Per D-03 / the plan, the
  Utinni repo is NOT committed from here — the Utinni instance owns its own commit. The X-macro `.inc`
  is the single name source, so provider + consumer expand the same file (T-38-10 tampering mitigate).
- **EPA-08 is evidence-only (D-01 LOCKED).** No provider code change. The handback transcribes the
  dumpbin export proof + the `Direct3d11_Device.cpp` create/destroy ordering (device+context FIRST :435,
  swapChain LAST :574, destroy resets swapChain FIRST :684) establishing the all-three-live invariant;
  the `0xC0000005 target=0x34` is consumer-side (T-38-11 accept).
- **Boot-smoke is the maintainer's pass.** Per the BUILD-GATE-autonomous execution mode, the dual-renderer
  boot + the runtime self-check + the live inject-smoke are recorded as the remaining maintainer step,
  not run by this executor.

## Deviations from Plan

None - plan executed exactly as written. The re-sync, the handback doc, and the final Debug+Release
build + dumpbin all completed as specified; no auto-fixes were needed (no source edits this plan).

## Threat Flags

None — no new security-relevant surface. The two phase-38 trust-boundary threats are satisfied:
T-38-10 (contract drift between repos) mitigated by the byte-identical sha256/cmp-verified copy +
the version bump to 2 + the single-source X-macro `.inc`; T-38-11 (DX11 evidence handback) accepted —
documentation only, no code, the finding is already proven and the 0xC0000005 fix is consumer-side.

## Next Phase Readiness

- **Phase 38 COMPLETE.** Contract at 94 endpoints, `UTINNI_HOOKPOINTS_VERSION == 2`, table/`.inc`/
  static_assert in lockstep, re-synced byte-identical to Utinni, EPA-08 evidence handed back, final
  Debug+Release build green with undecorated dumpbin export proof.
- **Remaining (maintainer / consumer):** the consolidated boot-smoke (gl05 char-select self-check +
  gl11 window-up) + the live inject-smoke; and the Utinni-side worklist in the handback §4 (the DX11
  `tryInstall()` device+context guard + 3-pointer diagnostic, the virtual vtable resolution, dropping
  the `installable()` skips, the `cuiChatWindow::ctor` DEFER decision, Issue #11).
- **Contract state at phase close:** 94 advertised endpoints, version 2, byte-identical in both repos.

## Self-Check: PASSED

- FOUND: .planning/handoff/2026-06-22-utinni-provider-coverage-handback.md
- FOUND: .planning/phases/38-utinni-advertised-client-coverage-completion/38-04-SUMMARY.md
- FOUND: D:/Code/Utinni/UtinniCore/swg/utinni_engine_hookpoints.h (sha256 matches source; version 2)
- FOUND: D:/Code/Utinni/UtinniCore/swg/utinni_engine_hookpoints.inc (sha256 matches source)
- FOUND: stage/SwgClient_d.exe + stage/SwgClient_r.exe (relinked; undecorated GetEngineHookPoints ord 52 / 51)
- FOUND commit a94416360 (Task 1: handback doc + re-sync)

---
*Phase: 38-utinni-advertised-client-coverage-completion*
*Completed: 2026-06-22*
