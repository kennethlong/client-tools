---
phase: 09-stlport-msvc-stl
plan: 01
subsystem: build-system
tags: [stlport, msvc-stl, baseline, runtime-anchor, v145, dll-abi, boot-gate]

# Dependency graph
requires:
  - phase: 09-stlport-msvc-stl
    provides: Wave-0 baselines (BEFORE-STL.txt, before-runtime.txt, before-crc.txt, before-sort-stability.txt, before-datatable-round-trip.txt)
provides:
  - v145 runtime contract anchor — every later Phase 09 wave's boot gate diffs against this state
  - 2016 SWGSource v3.0 DLL set staged in build-v145/bin/Debug/ with MD5 manifest
  - dumpbin /imports symbol baseline for the unmodified 35b872357 SwgClient_d.exe under v145 toolchain
  - Full-tree warnings baseline (warnings-wave-1.txt) for Wave 2-6 diffs
  - auto_ptr usage audit (30 hits across active build graph; rationale for keeping _HAS_AUTO_PTR_ETC=1)
  - ours/library + soePlatform/stationapi public-API STL-surface audit (zero ABI risk found)
  - Boost-subset MSVC v145 compile clean witness (with _HAS_AUTO_PTR_ETC=1)
  - StlCompat.h consumer include-path audit (17 OK-explicit, 1 NEEDS-CMAKE in active graph + 2 deferred-tools)
  - Phase-wide DLL-restage protocol — every wave's boot gate must re-stage 2016 DLLs after the wave's last build, because SwgClient POST_BUILD copies from exe/win32/ (2010 leak) by default
affects: [09-02, 09-03, 09-04, 09-05, 09-06]

# Tech tracking
tech-stack:
  added:
    - 2016 SWGSource v3.0 community-rebuilt proprietary plugin DLL set as runtime baseline
    - VS 2026 / MSVC v14.51 / v145 toolchain build-v145/ as the canonical build dir
  patterns:
    - "DLL-restage protocol: any time a SwgClient build runs, the POST_BUILD copy from exe/win32/ overrides the staged 2016 DLLs and re-stages 2010 leak DLLs; re-apply 2016 stage before any boot test"
    - "MD5-manifest-as-contract: dll-baseline-2016.txt fixes the binding ABI fingerprints; mid-phase MD5 drift is a halt event"
    - "Pre-migration symbol baseline (before-imports-replan2.txt): every later wave's STLPort-symbol gate diffs against this snapshot"

key-files:
  created:
    - .planning/phases/09-stlport-msvc-stl/baseline/before-imports-replan2.txt
    - .planning/phases/09-stlport-msvc-stl/baseline/dll-baseline-2016.txt
    - .planning/phases/09-stlport-msvc-stl/baseline/warnings-wave-1.txt
    - .planning/phases/09-stlport-msvc-stl/baseline/wave1-auto-ptr-usage.txt
    - .planning/phases/09-stlport-msvc-stl/baseline/wave1-ours-stl-surface.txt
    - .planning/phases/09-stlport-msvc-stl/baseline/wave1-3rd-stl-surface.txt
    - .planning/phases/09-stlport-msvc-stl/baseline/boost-smoke-test.cpp
    - .planning/phases/09-stlport-msvc-stl/baseline/compile-boost-smoke.bat
    - .planning/phases/09-stlport-msvc-stl/baseline/wave1-boost-smoke.txt
    - .planning/phases/09-stlport-msvc-stl/baseline/wave1-stlcompat-consumers.txt
    - .planning/phases/09-stlport-msvc-stl/baseline/stlcompat-consumer-probe.ps1
  modified:
    - (none — Wave 1 contract: zero src/ edits; only baseline/ artifacts and build-v145/bin/Debug/ DLL slots, the latter not under git)

key-decisions:
  - "Runtime contract is locked to v145 toolchain + 2016 SWGSource v3.0 DLL set + Tatooine zone-in (per CONTEXT D-08 / D-10); every later wave's boot gate diffs against this anchor"
  - "ours/library + soePlatform/Base + soePlatform/Singleton + soePlatform/MonAPI public-API surfaces expose ZERO STL containers across 78 audited headers — Phase 9 STL ABI shift cannot reach these libs through public headers"
  - "stationapi vendored directory is ABSENT in this tree (server-side LoginServer references it but client-side include graph has no path); validateStationKey=0 mitigation is doubly redundant for Phase 9 client scope"
  - "boost subset (config.hpp, smart_ptr.hpp, static_assert.hpp, utility.hpp) compiles clean against MSVC v145 with _HAS_AUTO_PTR_ETC=1 — D-07 verified empirically; the flag is required because boost/smart_ptr.hpp unconditionally references std::auto_ptr"
  - "auto_ptr deprecation flag _HAS_AUTO_PTR_ETC=1 is kept through Phase 9 — 30 active-build-graph references found, all in tools/3rd-party/comments; ZERO in client critical-path tree; auto_ptr removal deferred to post-Phase-9 follow-up"
  - "StlCompat.h consumer include-path resolution: 17/20 already covered by per-target include_directories(... sharedFoundation/include/public ...); 1 active-graph augmentation needed in localization/src/CMakeLists.txt (Wave 2 group 2.11); 2 deferred-tool entries (UiBuilder, LocalizationTool) flagged for post-Phase-9 MFC re-enablement plan"
  - "DLL-restage protocol added to phase contract — SwgClient CMakeLists.txt has POST_BUILD steps copying 2010 leak DLLs from exe/win32/ over the staged 2016 DLLs every build; every later wave must re-stage 2016 DLLs before its boot gate"

patterns-established:
  - "Wave 1 anchor template: build-v145 verify → DLL stage with MD5 manifest → dumpbin baseline → audit pass set → boot gate (re-validation, not exploration)"
  - "MD5-manifest-as-contract: any future runtime-baseline change requires updating dll-baseline-2016.txt as the same atomic commit"
  - "Audit-file taxonomy: [OK-tpl]/[OK-int]/[OK-ptr]/[OK-explicit]/[OK-link]/[NEEDS-CMAKE]/[RISK] — used in wave1-ours-stl-surface.txt, wave1-3rd-stl-surface.txt, wave1-stlcompat-consumers.txt"
  - "Smoke-test-with-flags: boost-smoke-test.cpp + compile-boost-smoke.bat as a reusable pattern — hand-compile a representative TU under the real build's flags to verify a 3rd-party-library compatibility decision"

requirements-completed: []
# Note: STL-05 (the requirement assigned to this wave) is NOT marked complete in this commit.
# STL-05's success condition is the Tatooine boot gate PASSING. Wave 1 stages the boot gate
# but does not run it (autonomous: false). When the user runs the test and signals "approved",
# a follow-up commit can mark STL-05 complete. If "regression", STL-05 stays incomplete and
# Phase 09 needs re-triage.

# Metrics
duration: 28min
completed: 2026-05-09
---

# Phase 9 Plan 01: Replan-2 Wave 1 — Runtime Baseline Anchor Summary

**v145 toolchain + 2016 SWGSource v3.0 DLL runtime contract locked; pre-migration symbol/warning baselines + four audit files captured; boot gate STAGED awaiting user-run Tatooine zone-in re-validation.**

## Performance

- **Duration:** 28 min
- **Started:** 2026-05-09T02:41:11Z
- **Completed:** 2026-05-09T03:09:13Z (auto-task phase; checkpoint pending)
- **Tasks:** 8 of 9 complete (Task 9 is the human-verify checkpoint)
- **Files created:** 11 (10 in baseline/, 0 in src/)
- **Files modified:** 0 (zero src/ edits — Wave 1 contract honored)

## Accomplishments

- **build-v145/ confirmed buildable** from unmodified `35b872357` source; v145 toolchain + VS 2026 produces `SwgClient_d.exe` with exit code 0; LNK4006/LNK4075/LNK4088 warnings are pre-migration baseline noise (the `/FORCE:MULTIPLE` + `stlport_vc143_compat.lib` shim setup that Phase 9 will dismantle).
- **2016 SWGSource v3.0 DLL set staged** in `build-v145/bin/Debug/`: 12 slots from 5 unique source files, all MD5s verified against canonical fingerprints from CONTEXT D-08; 11 `.original-2010.bak` files preserved for regression isolation; `DllExport.dll` already correct (no `.bak` created).
- **Pre-migration symbol baseline** captured at `baseline/before-imports-replan2.txt` — `dumpbin /imports` against the v145 build of unmodified `35b872357` source. UTF-16 encoded, 40462 bytes; contains `STLPORT_` ordinal-import marker. Every Wave 2-6 STLPort-symbol gate diffs against this snapshot.
- **Full-tree compile-clean baseline** captured at `baseline/warnings-wave-1.txt` — 406440 bytes of stdout+stderr from `cmake --build build-v145 --config Debug` (no `--target` filter); 103 `warning C####` instances; build exit 0. Wave 2-6's warnings gate diffs against this.
- **auto_ptr audit:** 30 active-build-graph references, distributed entirely across MFC-blocked tools (MayaExporter 5, TextureBuilder 11), 3rd-party vendored libs (boost 7, Crypto++ 3), 2 LagOMatic ws2_32, and 2 misc/comment hits. **ZERO `auto_ptr` uses on the client critical path.** `_HAS_AUTO_PTR_ETC=1` decision: kept through Phase 9.
- **ours/library + soePlatform/stationapi public-API STL surface audit:** 78 public headers scanned across 9 lib targets; **ZERO STL container references at the public-API level**. Internal implementations use STL freely (crypto's Crypto++ subset, localization's LocalizationManager) but the static-link boundary doesn't expose those layouts. RISK_FOLLOWUP: NONE.
- **Boost subset compatibility verified:** `boost-smoke-test.cpp` exercises all 4 vendored boost headers (`config.hpp`, `smart_ptr.hpp`, `static_assert.hpp`, `utility.hpp`) against MSVC v145 with `/std:c++17 /MT /EHsc /D_HAS_AUTO_PTR_ETC=1`; cl.exe exit code 0 → CONTEXT D-07 verified empirically. The smoke test is reproducible via the committed `compile-boost-smoke.bat`.
- **StlCompat.h consumer audit:** 20 consumers walked; 17 already have explicit `include_directories(... sharedFoundation/include/public ...)` in their target's `src/CMakeLists.txt`; 1 active-build-graph `[NEEDS-CMAKE]` (localization — known case, Wave 2 group 2.11 augmentation already mapped per reflog `e8b59f704`); 2 deferred-tool `[NEEDS-CMAKE]` flags (UiBuilder, LocalizationTool — not on Phase-9 build graph; flagged for post-Phase-9 MFC re-enablement plan).

## Task Commits

Each auto-task was committed atomically:

1. **Task 1: Verify build-v145 + before-imports-replan2.txt baseline** — `898bc9a89` (test)
2. **Task 2: Stage 2016 SWGSource v3.0 DLL set + dll-baseline-2016.txt manifest** — `bfeb105aa` (test)
3. **Task 3: Clean PCH + warnings-wave-1.txt full-tree baseline** — `fc482ca04` (test)
4. **Task 4: auto_ptr usage audit** — `a94af6d0a` (test)
5. **Task 5: ours/library STL-API-surface audit** — `82b825808` (test)
6. **Task 6: soePlatform + stationapi STL-API-surface audit** — `aa870be47` (test)
7. **Task 7: Boost-subset MSVC compatibility smoke test** — `a137af8f4` (test)
8. **Task 8: StlCompat.h consumer-include-path audit** — `28d0281ce` (test)
9. **Task 9: Tatooine zone-in boot gate (human-verify checkpoint)** — STAGED, AWAITING USER

**Plan metadata commit:** to be authored after the boot gate result is recorded.

## Files Created

- `.planning/phases/09-stlport-msvc-stl/baseline/before-imports-replan2.txt` — v145 SwgClient_d.exe `dumpbin /imports` symbol baseline (UTF-16, 40 KB; contains `STLPORT_` ordinal marker)
- `.planning/phases/09-stlport-msvc-stl/baseline/dll-baseline-2016.txt` — 12-slot DLL MD5 + source-path + restore-procedure manifest
- `.planning/phases/09-stlport-msvc-stl/baseline/warnings-wave-1.txt` — Full-tree build stdout+stderr (UTF-16, 406 KB; 103 `warning C####` instances)
- `.planning/phases/09-stlport-msvc-stl/baseline/wave1-auto-ptr-usage.txt` — 30-hit auto_ptr audit + decision rationale
- `.planning/phases/09-stlport-msvc-stl/baseline/wave1-ours-stl-surface.txt` — ours/library public-header STL-surface audit (zero hits)
- `.planning/phases/09-stlport-msvc-stl/baseline/wave1-3rd-stl-surface.txt` — soePlatform + stationapi public-header STL-surface audit (zero hits)
- `.planning/phases/09-stlport-msvc-stl/baseline/boost-smoke-test.cpp` — D-07 smoke test source
- `.planning/phases/09-stlport-msvc-stl/baseline/compile-boost-smoke.bat` — reproducible cl /c invocation (vcvars32 + flags)
- `.planning/phases/09-stlport-msvc-stl/baseline/wave1-boost-smoke.txt` — captured cl /c output (`exitCode=0`)
- `.planning/phases/09-stlport-msvc-stl/baseline/wave1-stlcompat-consumers.txt` — 20-consumer include-path audit
- `.planning/phases/09-stlport-msvc-stl/baseline/stlcompat-consumer-probe.ps1` — reproducible PS1 walker

## Files Modified

None. Wave 1's "no `src/` edits" contract was honored exactly — `git diff --stat 35b872357 HEAD -- src/` returns empty after all 8 task commits.

## Decisions Made

See frontmatter `key-decisions`. Key points:

1. **Runtime contract locked to v145 + 2016 DLLs + Tatooine zone-in** per CONTEXT D-08 / D-10 — every later wave's boot gate diffs against this anchor, not against the v143 `build/` legacy state and not against the 2010 leak DLL state.
2. **stationapi vendored directory is ABSENT** in this tree at HEAD; client-side include graph has no path to its headers; the `validateStationKey=0` mitigation is doubly redundant for Phase 9 client scope.
3. **`_HAS_AUTO_PTR_ETC=1` retained through Phase 9** — keeps boost smart_ptr + 30 tool/3rd-party usages compilable; ZERO client-critical-path references means the deprecation doesn't block the migration; auto_ptr removal is a post-Phase-9 follow-up.
4. **One Wave-2/3 CMake augmentation needed beyond expected:** add `${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundation/include/public` to `src/external/ours/library/localization/src/CMakeLists.txt` in the same atomic commit as the LocalizationManager.h `<hash_map>` → `StlCompat.h` sweep (Wave 2 group 2.11). This was already known per CONTEXT canonical_refs; the audit confirms no NEW [NEEDS-CMAKE] cases beyond it.
5. **DLL-restage protocol established as a phase-wide contract addition** — see Deviations §1.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] DLL-restage protocol added: SwgClient POST_BUILD overrides 2016 DLLs**
- **Found during:** Wave-end DLL drift verification (after Task 3's full-tree build re-ran SwgClient POST_BUILD copy)
- **Issue:** `src/game/client/application/SwgClient/src/CMakeLists.txt` lines 196-228 contain `add_custom_command(POST_BUILD)` steps that copy `gl05_r.dll` / `gl05_o.dll` / `gl06_*.dll` / `gl07_*.dll` / `dpvs.dll` / `dpvsd.dll` from `${SWG_WIN32_DIR}=${SWG_ROOT_SOURCE_DIR}/../exe/win32/` (the 2010 SOE leak originals tracked in this repo) into `$<TARGET_FILE_DIR:SwgClient>` (= `build-v145/bin/Debug/`) every time SwgClient builds. After Task 3's full-tree build, all 11 staged 2016 DLLs (DllExport.dll excluded — POST_BUILD doesn't touch it) had been overwritten with their 2010 leak counterparts, and the boot test would have hit the same gl05_d.dll!XXXX access-violation that triggered the original Phase-9 halt.
- **Fix:** (a) Re-applied the 2016 DLL stage immediately before reaching Task 9; (b) Added the **DLL-restage protocol** to the phase contract — every later wave's boot gate must re-stage the 2016 DLL set after its final build and before launching SwgClient_d.exe. This is documented in this SUMMARY's frontmatter `tech-stack.patterns` so Waves 2-6 inherit it.
- **Files modified:** None (operational fix; no source edit required for Wave 1's contract). Future planner action: Wave 2 or Wave 3 may opt to fix the root cause by patching `SwgClient/src/CMakeLists.txt` to either (i) point `SWG_WIN32_DIR` at the SWGSource v3.0 install dir for the 5 proprietary DLLs, or (ii) add a CMake option/`if()` guard that skips the POST_BUILD copy for the 2016-rebuilt subset. Both are source edits and out of Wave 1's "zero src/" contract.
- **Verification:** Re-ran the MD5 verification command from Task 2's `<verify><automated>` clause; all 12 slots PASS.
- **Committed in:** Not committed as code (no source change). Documented here in the SUMMARY for Wave 2-6 to inherit. The DLL slots themselves are not under git — they live in `build-v145/bin/Debug/` which is build-tree state.

**2. [Rule 3 - Blocking] Boost smoke test required `/D_HAS_AUTO_PTR_ETC=1`**
- **Found during:** Task 7 (Boost-subset MSVC compatibility smoke test)
- **Issue:** Initial cl /c invocation per the plan's documented flags (`/std:c++17 /MT /EHsc`, no `_HAS_AUTO_PTR_ETC` macro) failed with C2061 / C2039 errors at `boost/smart_ptr.hpp:150,164,178,183` — boost's `shared_ptr<T>(std::auto_ptr<Y>&)` constructor and `operator=(std::auto_ptr<Y>&)` overload reference `std::auto_ptr` unconditionally, which MSVC v145 + C++17 removes from `<memory>` by default.
- **Fix:** Added `/D_HAS_AUTO_PTR_ETC=1` to the cl /c invocation. This mirrors the real Phase 9 client build (Wave 2 Task 2 adds the same `_HAS_AUTO_PTR_ETC=1` to `src/CMakeLists.txt` for the same reason; see Task 4's audit + the planner's reflog `2bdfa8325` precedent). Updated `boost-smoke-test.cpp` header comment to document the flag requirement, updated `compile-boost-smoke.bat` to add `/D_HAS_AUTO_PTR_ETC=1`. The smoke test now exits 0.
- **Files modified:** `boost-smoke-test.cpp`, `compile-boost-smoke.bat` (both in `.planning/`, not `src/`)
- **Verification:** `cat wave1-boost-smoke.txt` shows clean compile with `exitCode=0`.
- **Committed in:** `a137af8f4`

---

**Total deviations:** 2 auto-fixed (both Rule 3 - Blocking).
**Impact on plan:** Both deviations were blocking issues necessary to honor the plan's success criteria. Neither expanded the scope; #1 added a phase-wide protocol that every wave needs anyway (the alternative is silent regression to the 2010-leak ABI break the phase exists to avoid); #2 mirrored a flag the plan already adds in Wave 2.

## Issues Encountered

- **build-v145 reconfigure required re-quoting `-D` flags.** The first reconfigure attempt with `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` (unquoted) was misparsed by PowerShell as `-D CMAKE_POLICY_VERSION_MINIMUM=3 .5`, yielding "Invalid CMAKE_POLICY_VERSION_MINIMUM value '3'". Resolved by quoting the entire flag (`'-DCMAKE_POLICY_VERSION_MINIMUM=3.5'`). This is a PowerShell-versus-CMake-CLI quoting nuance, not a real CMake issue. Future planners: always single-quote the entire `-D` argument when invoking cmake.exe via PowerShell from bash.
- **Smoke-test cl.exe failed initially due to PATH lacking System32.** The vcvars32.bat call from a bare cmd.exe (without git-bash's PATH inheritance fully mapping System32) couldn't resolve `vswhere.exe` / `findstr` cleanly. Resolved by explicitly setting `PATH=C:\Windows\System32;...` at the top of `compile-boost-smoke.bat`.

## User Setup Required

None for the auto-task phase. **Task 9 (boot gate) requires user-run boot test** — see "Boot Gate Status" below.

## Boot Gate Status

**STATUS: REGRESSION — v145 + 2016 SWGSource DLL pair FATALs during graphics setup; CONTEXT D-08 invalidated; replan-3 needed**

**Update (2026-05-09, continuation agent diagnostic round):** see "Wave 1 Boot Gate Diagnostic Round" section below — the user's reported "sub-second silent exit" was actually a `format arg out of range` FATAL deep in the graphics-plugin init phase. v145 EXE + 2016 DLL pair empirically does NOT boot. v145 EXE + 2010 leak DLL pair empirically DOES boot (probe-isolated). The "v145 + 2016 DLL contract" of CONTEXT D-08 is wrong as stated; replan-3 needed. See `.continue-here-replan3.md`.

### (Original — before regression confirmed)

**STATUS: STAGED — AWAITING USER VERIFICATION**

The Wave 1 anchor wave's only purpose is to lock the runtime contract. All eight auto-tasks ran clean and committed atomically. The Tatooine zone-in re-validation is an explicit `type="checkpoint:human-verify"` gate per the plan's `<task type="checkpoint:human-verify">` block (Task 9).

**Pre-test state (verified just before this SUMMARY was written):**

| Item | Status |
|---|---|
| `build-v145/bin/Debug/SwgClient_d.exe` | Present, dated 2026-05-08 21:51 (v145 build of unmodified 35b872357) |
| All 12 staged DLLs MD5-correct | Verified PASS (re-staged after Task 3's full-tree build POST_BUILD override) |
| `git diff --stat 35b872357 HEAD -- src/` | Empty (zero src/ edits — anchor contract honored) |
| Wave 1 commits since 35b872357 | 8 (all `test(09-01):...` per plan task numbering) |

**Per the plan's <how-to-verify> procedure, the user must:**

1. **Confirm the build state is anchor-clean:**
   ```powershell
   git diff --stat 35b872357 HEAD -- src/
   ```
   Expected: empty output.

2. **Confirm DLL MD5s still match the manifest** (the verify command from Task 2):
   ```powershell
   $expected = @{ 'gl05_d.dll'='f7e92af1757a5aa145cc7c92b78fe6a6'; 'gl05_o.dll'='f7e92af1757a5aa145cc7c92b78fe6a6'; 'gl05_r.dll'='f7e92af1757a5aa145cc7c92b78fe6a6'; 'gl06_d.dll'='f318bb60397f821e49e9b9d4054d184c'; 'gl06_o.dll'='f318bb60397f821e49e9b9d4054d184c'; 'gl06_r.dll'='f318bb60397f821e49e9b9d4054d184c'; 'gl07_d.dll'='e78603544b2f680dae603f0b4ed79a92'; 'gl07_o.dll'='e78603544b2f680dae603f0b4ed79a92'; 'gl07_r.dll'='e78603544b2f680dae603f0b4ed79a92'; 'dpvs.dll'='126d37dfc9d1a545b39a5b4b908e8a90'; 'dpvsd.dll'='126d37dfc9d1a545b39a5b4b908e8a90'; 'DllExport.dll'='f28b8bf7911a59e22e145a5da18f873e' }
   foreach($n in $expected.Keys) {
     $p = "D:/Code/swg-client/build-v145/bin/Debug/$n"
     $actual = (Get-FileHash -Algorithm MD5 -Path $p).Hash.ToLower()
     if ($actual -ne $expected[$n]) { Write-Host "DRIFT: $n actual=$actual" } else { Write-Host "OK: $n" }
   }
   ```

3. **Start the SWGSource StellaBellum VM** per `project_vm_server_setup.md`:
   - Boot VM, set manual IP (typically 192.168.1.200), Oracle startup, stationchat, ant build (if needed), confirm `Test-NetConnection 192.168.1.200 -Port 44453` succeeds.

4. **Launch the client and zone into Tatooine:**
   ```powershell
   D:\Code\swg-client\build-v145\bin\Debug\SwgClient_d.exe
   ```
   Boot path: login → character-select → Play → world stream → standing in Tatooine → mouse-look + walk a few steps. Watch for `0xC0000005` access violations in the VS debugger Output pane referencing any of `gl05_d.dll` / `gl06_d.dll` / `gl07_d.dll` / `dpvs.dll` / `dpvsd.dll` / `DllExport.dll` — those are FAILures.

5. **Type the resume signal in this conversation:**
   - **`approved`** → Wave 1 contract locked; Wave 2 may begin. STATE.md gets advanced; STL-05 gets marked complete; this SUMMARY gets a Boot Gate Result section appended.
   - **`MD5 drift: <name>`** → halt; the staging slot was wrong; re-run Task 2 step 3 for the affected slot.
   - **`VM unreachable`** → environmental block; resolve per memory procedure and retry.
   - **`regression: [details]`** → halt; the 35b872357 baseline does NOT zone into Tatooine cleanly even with the 2016 DLLs; this contradicts CONTEXT D-08 and the entire phase strategy needs re-evaluation. Capture VS debugger stack trace and document in a new `.continue-here-replan3.md`.

**Important reminder for the user:** if you rebuild SwgClient between this SUMMARY and your boot test, the SwgClient POST_BUILD copy will re-stage the 2010 leak DLLs. **Always re-run the 2016 stage script** (steps 2-3 of Task 2's `<action>`, or just `Copy-Item` from `D:/Code/SWGSource Client v3.0/` to `build-v145/bin/Debug/` per the manifest) before launching.

## Next Phase Readiness

**Wave 2 unblocked** once the boot gate is `approved`. Inputs Wave 2 inherits from this wave:

- `before-imports-replan2.txt` symbol-import diff target.
- `dll-baseline-2016.txt` MD5-drift halt fingerprint set.
- `warnings-wave-1.txt` warnings-baseline diff target.
- `wave1-stlcompat-consumers.txt` localization-only [NEEDS-CMAKE] action item for group 2.11.
- `wave1-auto-ptr-usage.txt` rationale for retaining `_HAS_AUTO_PTR_ETC=1` in Wave 2 Task 2's CMake edit.
- DLL-restage protocol must be invoked at the end of Wave 2's boot gate (and every wave thereafter).

**Blockers:** None on the source side. Operational reminder is the DLL-restage protocol — Wave 2 author must include it in Wave 2's `<how-to-verify>` block.

## Wave 1 Boot Gate Diagnostic Round (2026-05-09, continuation agent)

The user reported "Window opens then closes within sub-second. Nothing rendered. No error dialog, no crash popup." against `build-v145/bin/Debug/SwgClient_d.exe`. A continuation agent ran probes A–G and produced these findings; the boot gate result is **REGRESSION**.

### Probes run

| # | Name | Result |
|---|------|--------|
| A | MD5-verify 2016 DLL set in build-v145/bin/Debug/ | PASS — all 12 slots match `dll-baseline-2016.txt` (no DLL drift) |
| B | Diff bin/Debug contents v143 vs v145 | No env gap; both have client.cfg, msvcr71.dll, dbghelp, etc.; **but build/bin/Debug/ DLLs are 2010 LEAK** (gl05_d MD5 `28a995…`, dpvs MD5 `86998e…`, dpvsd MD5 `41e484…`), NOT 2016 SWGSource v3.0 |
| C | client.cfg comparison | Byte-identical between v143 and v145 |
| D | Memory project_vm_server_setup.md scan | "Launch from bin/Debug/" CWD requirement is satisfied in both dirs |
| E | Run v145 EXE with stdout/stderr capture (CWD = build-v145/bin/Debug/) | **Detailed FATAL captured** — see below |
| F | Apply env-gap fix | Skipped — no env gap identified |
| G | Swap 2010 leak DLLs into build-v145, run v145 EXE | **Runs cleanly** to audio runtime; no FATAL — isolates the 2016 DLL set as the proximate cause |

### The FATAL (Probe E)

After ~1.5s of `SetupShared*::install` + `SetupSharedGame::install` + `SetupSharedTerrain::install` + `SetupSharedXml::install` + `SetupSharedPathfinding::install` + `SetupClientAudio::install` all completing cleanly, the EXE crashes with:

```
SwgClient_d.exe: unknown.0
unknown(0x01A8DBF1) : FATAL 0bc11397: format arg out of range: value/max 1793887061/16
  unknown(0x6BEAB2D3) : caller 1
  unknown(0x6BE9678B) : caller 2
  unknown(0x01A2EA35) : caller 3
  unknown(0x01A77A80) : caller 4
  unknown(0x6BE99C87) : caller 5
  unknown(0x6BE992E9) : caller 6
  …
unknown(0x01F0BAAC) : FATAL be5fbc55: ExceptionHandler invoked
```

Exit code 31. The "format arg out of range" message is from the engine's printf-format-arg-position checker — it reads a garbage `%N$` arg position (`0x6AEDD955` = 1793887061) where `N` should be 1..16. This is a **stack/va_list misalignment** between EXE-side caller and DLL-X-side callee at format-string emit time. Callstack frames at `0x6BEAB2D3` / `0x6BE9678B` / `0x6BE99C87` / `0x6BE992E9` are in DLL-X loaded at `~0x6BE90000`, which is NOT one of gl05/06/07 (`0x62A00000`), NOT dpvs/dpvsd (`0x10000000`), NOT mss32 (`0x21100000`), NOT binkw32 (`0x18000000`), NOT qt-mt334 (`0x39D00000`), NOT nspr4 (`0x30000000`). Likely an ASLR-relocated runtime-loaded module (system DLL pulled in by graphics-plugin imports, or a SWGSource-shipped lib not in our manifest).

### The contradicting probe (Probe G)

Same v145 EXE, but with the 2010 leak DLLs (from `*.original-2010.bak` files alongside) swapped in:

- Exit code 124 (SIGTERM at 15s timeout — no crash; the process was alive when killed)
- Output: clean install chain through SetupClientAudio + then a runtime loop emitting "Unable to create a valid sample id. All the samples must be occupied" benign warnings (audio resource exhaustion at runtime, not setup-time)
- No `format arg out of range` FATAL. No ExceptionHandler.

This isolates the FATAL: **v145 EXE + 2016 SWGSource v3.0 DLLs FATALs; v145 EXE + 2010 leak DLLs runs.** The 2016 DLL set is the proximate cause of the FATAL when paired with v145 toolchain output.

### CONTEXT D-08 invalidated

D-08 binds the runtime contract to "v145 + 2016 SWGSource v3.0 DLLs + Tatooine zone-in" and cites a user-run empirical test as evidence (`Result: SwgClient_d.exe boots clean and zones into Tatooine`). After this round:

- `D:/Code/swg-client/build/bin/Debug/SwgClient_d.exe` (the file that actually shipped a Tatooine zone-in confirmation per commit `a994b33b0`) is the **v143 toolchain build** (timestamped 2026-05-08 16:44:40, built from `35b872357` source).
- The DLLs alongside that EXE are **2010 leak originals** (Probe B MD5s confirm).
- I.e., the historical "PASSED" empirical test was v143 + 2010 leak DLLs, NOT v145 + 2016 DLLs as D-08 records.

The v145 + 2016 DLL pair has never been runtime-verified to boot until this round, and it has now been verified NOT to boot. **D-08 is empirically wrong.**

### Replan-3 needed

The replan-2 wave plan has Wave 2..6 each ending with a Tatooine zone-in boot gate against `build-v145/` + 2016 DLLs. None of those gates can fire from the current anchor state because the anchor itself FATALs. **Replan-3 is mandatory.**

Three architectural options surfaced (full rationale + cost in `.continue-here-replan3.md`):

| Option | Move | Recommendation |
|---|---|---|
| A | Switch contract to v143 + 2010 leak DLLs (the user's actually-working config) | **Recommended** — smallest delta; revokes D-08..D-12 only; D-01..D-07 + R-series still binding |
| B | Forensics on the v145 + 2016 DLL FATAL | Possible but 1–3 days of toolchain investigation; outcome uncertain |
| C | Pull Phase 11 (D3D11 renderer plugin) forward into Phase 9 | Original .continue-here-pre-replan2 idea; 2–4 month schedule slip |

### Files touched during diagnostic round

- `D:/Code/swg-client/build-v145/bin/Debug/*.dll` — temporarily swapped to 2010 during Probe G, restored to 2016 (Probe G Step 6 verified MD5s); build-tree state, not git-tracked.
- `.planning/phases/09-stlport-msvc-stl/.continue-here-replan3.md` — new
- `.planning/phases/09-stlport-msvc-stl/09-01-SUMMARY.md` (this file) — Boot Gate Status updated + this section appended
- `.planning/STATE.md` — stopped_at + Last session updated to halt
- **No `src/` edits.** `git diff --stat 35b872357 HEAD -- src/` remains empty (Wave 1 anchor contract preserved end-to-end).

### Boot Gate Result

**REGRESSION** — STL-05 stays unchecked. Plan 09-01 is NOT complete. Replan-3 must run before Wave 2..6 are executable. See `.continue-here-replan3.md` for the full architectural decision.

---

## Self-Check: PASSED

Verified after writing this SUMMARY:

- 11 baseline files exist:
  - FOUND: .planning/phases/09-stlport-msvc-stl/baseline/before-imports-replan2.txt
  - FOUND: .planning/phases/09-stlport-msvc-stl/baseline/dll-baseline-2016.txt
  - FOUND: .planning/phases/09-stlport-msvc-stl/baseline/warnings-wave-1.txt
  - FOUND: .planning/phases/09-stlport-msvc-stl/baseline/wave1-auto-ptr-usage.txt
  - FOUND: .planning/phases/09-stlport-msvc-stl/baseline/wave1-ours-stl-surface.txt
  - FOUND: .planning/phases/09-stlport-msvc-stl/baseline/wave1-3rd-stl-surface.txt
  - FOUND: .planning/phases/09-stlport-msvc-stl/baseline/boost-smoke-test.cpp
  - FOUND: .planning/phases/09-stlport-msvc-stl/baseline/compile-boost-smoke.bat
  - FOUND: .planning/phases/09-stlport-msvc-stl/baseline/wave1-boost-smoke.txt
  - FOUND: .planning/phases/09-stlport-msvc-stl/baseline/wave1-stlcompat-consumers.txt
  - FOUND: .planning/phases/09-stlport-msvc-stl/baseline/stlcompat-consumer-probe.ps1
- 8 task commits exist: FOUND 898bc9a89, bfeb105aa, fc482ca04, a94af6d0a, 82b825808, aa870be47, a137af8f4, 28d0281ce.
- src/ diff vs 35b872357: empty (zero src/ edits).

---
*Phase: 09-stlport-msvc-stl*
*Plan: 01 (Wave 1 — runtime baseline anchor)*
*Authored: 2026-05-09*
*Boot Gate: STAGED-PENDING — awaits user-run Tatooine zone-in re-validation*
