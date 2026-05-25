---
phase: 09-stlport-msvc-stl
plan: 00
subsystem: testing
tags: [stlport, validation, baseline, crc, datatable, fps, sort-stability]

requires:
  - phase: 06-launch-login-handshake
    provides: SWG_INSTALL_DIR cmake cache + retail TRE/TOC search paths used by FileStreamer/TreeFile
  - phase: 08-tools-and-canaries
    provides: LabelHashTool console-EXE pattern (Setup* sequence + STLPort link list + LINK_FLAGS_DEBUG /FORCE:MULTIPLE) reused verbatim for the validation helpers
provides:
  - "BEFORE-STL.txt: pre-migration grep snapshot of every STLPort marker (post-Phase-7 counts: hash_map 74, hash_set 20, <hash_map> 54, <hash_set> 14, _STLP_* 0, _STL:: small set)"
  - "before-crc.txt: 100-line persistent + transient hash dump (CrcLowerString::calculateCrc + std::hash<std::string>) for fixed input list"
  - "before-datatable-round-trip.txt: 1288-line deterministic cell dump from datatables/player/radial_menu.iff (322 rows × 4 cols)"
  - "before-sort-stability.txt: 12-line CrcLowerString sort dump via LessAbcOrderReferenceComparator"
  - "before-runtime.txt: STLPort-build FPS + warning baseline from a 98-second VM session against Tatooine"
  - "Three gated validation helpers under .planning/phases/09-stlport-msvc-stl/baseline/ + WHITENGOLD_BUILD_VALIDATION_HELPERS option in src/CMakeLists.txt (OFF by default)"
affects: [09-01, 09-02, 09-03, validation, after-baseline]

tech-stack:
  added: []
  patterns:
    - "Validation helpers as out-of-tree gated subdirectory (add_subdirectory pointed at .planning/phases/.../baseline) — keeps them off the regular build graph but still wired into the same toolchain when the option flips ON"
    - "FILE*/fgets for any helper file I/O — STLPort 4.5.3 basic_filebuf silently fails against the MSVC 2022 runtime (project_stlport_fstream_crash); std::ifstream is unusable in this build chain"
    - "Output to argv-supplied path, not stdout — MemoryManager debug tracker writes to stdout per allocator decrement and corrupts the dump mid-line"
    - "Indirection-vector sort over CrcLowerString to keep instances in place (LessAbcOrderReferenceComparator takes references; MemoryBlockManager-backed types don't move cheaply)"

key-files:
  created:
    - ".planning/phases/09-stlport-msvc-stl/baseline/capture-before-stl.sh"
    - ".planning/phases/09-stlport-msvc-stl/baseline/BEFORE-STL.txt"
    - ".planning/phases/09-stlport-msvc-stl/baseline/known-strings.txt"
    - ".planning/phases/09-stlport-msvc-stl/baseline/std_hash_dump.cpp"
    - ".planning/phases/09-stlport-msvc-stl/baseline/datatable_round_trip.cpp"
    - ".planning/phases/09-stlport-msvc-stl/baseline/sort_stability_dump.cpp"
    - ".planning/phases/09-stlport-msvc-stl/baseline/FirstValidationHelpers.h"
    - ".planning/phases/09-stlport-msvc-stl/baseline/FirstValidationHelpers.cpp"
    - ".planning/phases/09-stlport-msvc-stl/baseline/CMakeLists.txt"
    - ".planning/phases/09-stlport-msvc-stl/baseline/before-crc.txt"
    - ".planning/phases/09-stlport-msvc-stl/baseline/before-datatable-round-trip.txt"
    - ".planning/phases/09-stlport-msvc-stl/baseline/before-sort-stability.txt"
    - ".planning/phases/09-stlport-msvc-stl/baseline/before-runtime.txt"
  modified:
    - "src/CMakeLists.txt (option WHITENGOLD_BUILD_VALIDATION_HELPERS + gated add_subdirectory of the out-of-tree baseline/)"

key-decisions:
  - "Persistent CRC = CrcLowerString::calculateCrc(const char *) — verified against Crc.h. Crc::calculateLowerString does NOT exist in this codebase; the plan's symbol assumption was wrong. Same function will be called for after-crc.txt."
  - "DataTable IFF substituted: datatables/player/radial_menu.iff (322 rows × 4 cols) replaces the plan's misc/quest_task_data.iff because the user's retail install ships the latter only inside .tre archives. Same path will be used for after-datatable-round-trip.txt."
  - "Helpers always write output to a path supplied as argv, never to stdout. MemoryManager's allocator tracker writes 'MM::remove ...' lines to stdout each time it tears down — would corrupt the dump mid-line and was observed doing so in the first iteration."
  - "All file I/O uses C-runtime FILE*/fgets/fopen, never STLPort std::ifstream. project_stlport_fstream_crash (basic_filebuf codecvt vtable mismatch vs MSVC 2022 UCRT) makes STLPort fstream silently fail in this build chain."
  - "Shared PCH (FirstValidationHelpers.h → sharedFoundation/FirstSharedFoundation.h) used by all three helpers, mirroring LabelHashTool's FirstLabelHashTool pattern. /FORCE:MULTIPLE LINK_FLAGS_DEBUG/RELEASE applied to silence the LNK4006 stlport_vc71 vs vc143-compat duplicate-symbol warnings."
  - "Helpers' link list mirrors LabelHashTool — over-linked slightly (sharedLog, sharedNetworkMessages, sharedMessageDispatch) rather than risk under-linking. Cost is zero for an off-by-default helper."
  - "_STLP_* count is genuinely 0 outside stlport453/ in this post-Phase-7 codebase. The plan's acceptance criterion '_STLP_* > 0' was based on incorrect assumption about the codebase state. The 0->0 baseline is still a valid post-migration witness (no _STLP_* may be introduced)."

patterns-established:
  - "Out-of-tree validation helpers via gated cmake option: keep validation/diagnostic targets in .planning/phases/<phase>/baseline/ + add_subdirectory from src/CMakeLists.txt under an option(...) — bit-identical regular build by default, full toolchain access when needed"
  - "Pre-migration baseline witnesses for ABI-changing migrations: every persisted on-disk format + every transient hash + every ordering-dependent algorithm needs its own deterministic dump artifact captured BEFORE the first source edit"
  - "Runtime baseline capture via [SharedFoundation] reportClock + logClock + [SharedLog] logTarget=file:...{reportLog}: lifts Clock::debugReport per-frame samples into a parseable log file without needing DebugView or a debugger"

requirements-completed:
  - STL-05

duration: ~2.5 hours (Tasks 1+2 inline, Task 3 manual VM session)
completed: 2026-05-08
---

# Phase 9 Plan 00: Wave 0 baselines Summary

**Five pre-migration witness artifacts captured (grep snapshot, persistent + transient hash dump, DataTable IFF round-trip, sort-stability dump, runtime FPS + warning counts) plus three gated validation helpers wired through a new `WHITENGOLD_BUILD_VALIDATION_HELPERS` cmake option — locked-in diff targets for Wave 3's D-04 verification gates.**

## Performance

- **Duration:** ~2.5 hours wall (Tasks 1+2 ~90 min inline; Task 3 manual VM session ~10 min capture + analysis)
- **Started:** 2026-05-08 morning
- **Completed:** 2026-05-08
- **Tasks:** 3
- **Files created:** 13
- **Files modified:** 1 (`src/CMakeLists.txt`)

## Accomplishments

- Pre-migration STLPort marker grep snapshot (`BEFORE-STL.txt`, 409 lines): post-Phase-7 codebase counts confirm hash_map 74 / hash_set 20 / `<hash_map>` 54 / `<hash_set>` 14 / `_STLP_*` 0 outside stlport453/. Wave 1 sweep can now diff against this.
- Three out-of-tree validation helpers built and gated behind `WHITENGOLD_BUILD_VALIDATION_HELPERS` (default OFF): `validation_helper_std_hash_dump`, `validation_helper_datatable_round_trip`, `validation_helper_sort_stability_dump`. Helpers OFF reconfigure verified clean (2.1s).
- Three deterministic baselines captured under STLPort 4.5.3:
  - `before-crc.txt` — 100 lines, `<input>\t0x<crc8>\t<std::hash>` (CrcLowerString::calculateCrc + std::hash<std::string>)
  - `before-datatable-round-trip.txt` — 1288 lines from `datatables/player/radial_menu.iff` (322 rows × 4 cols)
  - `before-sort-stability.txt` — 12 lines, CrcLowerString sorted by LessAbcOrderReferenceComparator (ABC order)
- Runtime baseline (`before-runtime.txt`) captured from a 98-second VM session, 3476 Clock samples, with character-select / in-Tatooine split: 59.45 fps median pre-Tatooine, 58.06 fps median in-zone, 1378 DEBUG_WARNING entries (1380 in initial run; 1378 in demoMode=false re-capture run — small per-session variance from audio sample-id init). Leak-count = 0 under the codebase's own notALeak definition (re-captured with `demoMode=false` override; warnings popup fired confirming the gate, leaks popup did NOT — meaning `MemoryManagerNamespace::report(true)` returned 0).

## Task Commits

1. **Task 1: capture-before-stl.sh + grep snapshot** — `1fdb220e8` (test)
2. **Task 2: validation helpers + 3 baselines** — `3bbbc4e34` (test)
3. **Task 3: manual VM runtime baseline (FPS + warnings)** — `62079e15a` (test)

## Files Created/Modified

- `.planning/phases/09-stlport-msvc-stl/baseline/capture-before-stl.sh` — verbatim grep script from RESEARCH.md §Validation Architecture §1, plus SUMMARY block at end
- `.planning/phases/09-stlport-msvc-stl/baseline/BEFORE-STL.txt` — full grep dump (56 KB, 409 lines), pre-migration snapshot
- `.planning/phases/09-stlport-msvc-stl/baseline/known-strings.txt` — 100 representative TRE asset paths (appearance/, creature/, terrain/, misc/, string/, object/, shader/, sound/) used as fixed input for hash baseline (and after-crc.txt)
- `.planning/phases/09-stlport-msvc-stl/baseline/std_hash_dump.cpp` — emits `<input>\t0x<crc8>\t<std::hash>` per line, FILE*-based I/O, reused std::string scratch buffer to minimise allocator pressure
- `.planning/phases/09-stlport-msvc-stl/baseline/datatable_round_trip.cpp` — DataTable::load + per-cell dispatch on getBasicType() (DT_Int %d, DT_Float %.6f, DT_String); SetupSharedFile + DataTableManager::install required for FileStreamer
- `.planning/phases/09-stlport-msvc-stl/baseline/sort_stability_dump.cpp` — heap-allocated CrcLowerString instances, std::sort(idx.begin(), idx.end(), lambda(LessAbcOrderReferenceComparator))
- `.planning/phases/09-stlport-msvc-stl/baseline/FirstValidationHelpers.{h,cpp}` — shared PCH wrapping FirstSharedFoundation.h (LabelHashTool pattern)
- `.planning/phases/09-stlport-msvc-stl/baseline/CMakeLists.txt` — three add_executable targets, shared VH_INCLUDES + VH_LIBS, target_precompile_headers, LINK_FLAGS_DEBUG/RELEASE /FORCE:MULTIPLE
- `.planning/phases/09-stlport-msvc-stl/baseline/before-{crc,datatable-round-trip,sort-stability,runtime}.txt` — five baseline witnesses
- `src/CMakeLists.txt` — appended `option(WHITENGOLD_BUILD_VALIDATION_HELPERS ... OFF)` + gated `add_subdirectory(${CMAKE_SOURCE_DIR}/../.planning/phases/09-stlport-msvc-stl/baseline ${CMAKE_BINARY_DIR}/validation_helpers)`

## Decisions Made

See `key-decisions` in frontmatter. Notable:
- The plan's `Crc::calculateLowerString` symbol does not exist; verified actual API is `CrcLowerString::calculateCrc(const char *)` from `CrcLowerString.h:71`. Same function MUST be used for after-crc.txt.
- DataTable IFF substituted to `datatables/player/radial_menu.iff` (D:/Code/SWGSource Client v3.0/datatables/player/radial_menu.iff). The plan's `misc/quest_task_data.iff` is not standalone in this user's retail install — it lives inside a .tre archive. Same path MUST be used for after-datatable-round-trip.txt.
- All helper file I/O is FILE*-based; STLPort std::ifstream silently fails on this MSVC 2022 build chain (documented project_stlport_fstream_crash).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule: API mismatch — plan vs codebase] Wrong CRC symbol**
- **Found during:** Task 2 (std_hash_dump.cpp authoring)
- **Issue:** Plan named `Crc::calculateLowerString` as the persistent hash; that symbol does not exist in this codebase.
- **Fix:** Used `CrcLowerString::calculateCrc(const char *)` instead — the real persistent-hash function defined in `CrcLowerString.h:71` (calls into `Crc::calculate` after lowercasing).
- **Files modified:** `std_hash_dump.cpp`
- **Verification:** Helper compiles + dumps 100 deterministic CRC values for known input list.
- **Committed in:** `3bbbc4e34` (Task 2)

**2. [Rule: API mismatch — plan vs codebase] Wrong comparator parameter form**
- **Found during:** Task 2 (sort_stability_dump.cpp authoring)
- **Issue:** Plan's helper called `std::sort(ptrs.begin(), ptrs.end(), CrcLowerString::LessAbcOrderReferenceComparator{})` over a `vector<const CrcLowerString*>`, but the comparator's `operator()` takes `const CrcLowerString&` not pointers (CrcLowerString.h:64). Would not compile.
- **Fix:** Sort an indirection vector of indices via lambda that dereferences and passes by reference: `std::sort(idx.begin(), idx.end(), [&arr,&cmp](int a,int b){ return cmp(*arr[a], *arr[b]); })`. Avoids any CrcLowerString copy/move under the MemoryBlockManager.
- **Files modified:** `sort_stability_dump.cpp`
- **Committed in:** `3bbbc4e34` (Task 2)

**3. [Rule: artifact-vs-environment mismatch] Recommended IFF not standalone**
- **Found during:** Task 2 (datatable_round_trip.cpp run)
- **Issue:** Plan recommended `misc/quest_task_data.iff` from SWG_INSTALL_DIR; user's retail install (`D:/Code/SWGSource Client v3.0/`) ships it only inside .tre archives. The misc/ folder has only `object_template_crc_string_table.iff` and `quest_crc_string_table.iff` — neither is a DataTable.
- **Fix:** Substituted `datatables/player/radial_menu.iff` (322 rows × 4 cols, mixed Int/Float/String types). Locked in this same path for the after-baseline.
- **Files modified:** `before-datatable-round-trip.txt` (now sourced from radial_menu.iff)
- **Committed in:** `3bbbc4e34` (Task 2)

**4. [Rule: missing critical setup] FileStreamer not installed**
- **Found during:** Task 2 (first run of datatable_round_trip helper)
- **Issue:** Original helper called only SetupSharedFoundation + SetupSharedCompression. Iff::open went through TreeFile → TreeFile_SearchNode::loadDirectoryTree → FileStreamer, which FATAL'd: `FileStreamer.cpp(111) : FATAL 12360d0f: not installed`.
- **Fix:** Added `SetupSharedFile::install(false)` + `DataTableManager::install()` after SetupSharedCompression (mirrors `DataTableTool/src/shared/DataTableTool.cpp:117-120`). Also added `data.argc`/`data.argv`/`data.demoMode = true` to the SetupSharedFoundation::Data struct per the same template.
- **Files modified:** `datatable_round_trip.cpp`
- **Committed in:** `3bbbc4e34` (Task 2)

**5. [Rule: STLPort runtime defect] std::ifstream silently fails**
- **Found during:** Task 2 (first iteration of std_hash_dump helper)
- **Issue:** Helper using `std::ifstream` returned `wrote 0 lines` — file opened OK but `std::getline` returned false on first call. Documented as `project_stlport_fstream_crash` (basic_filebuf codecvt vtable mismatch under MSVC 2022 UCRT).
- **Fix:** All helper file I/O converted to C-runtime `FILE*` + `fgets`/`fopen`/`fwrite` — bypasses STLPort fstream entirely.
- **Files modified:** `std_hash_dump.cpp`, `datatable_round_trip.cpp`, `sort_stability_dump.cpp`
- **Committed in:** `3bbbc4e34` (Task 2)

**6. [Rule: stdout pollution from MemoryManager] Output corruption mid-line**
- **Found during:** Task 2 (first successful std_hash_dump run)
- **Issue:** Helper wrote 100 hash lines to stdout via `printf`, but the MemoryManager's allocator tracker also writes `MM::remove <bytes>/<total>=bytes <allocs>/<total>=allocs` to stdout each time a per-target allocator decrements. Observed splitting line 75 mid-hash: `0x2e25219MM::remove 12160/...`.
- **Fix:** Take output path as second argv, write to FILE* the helper opens itself. MemoryManager noise stays on stdout where it was already going; the dump is clean.
- **Files modified:** `std_hash_dump.cpp`, `datatable_round_trip.cpp`, `sort_stability_dump.cpp`
- **Committed in:** `3bbbc4e34` (Task 2)

**7. [Rule: STLPort std header gap] std::uint32_t not in STLPort 4.5.3**
- **Found during:** Task 2 (first build of std_hash_dump)
- **Issue:** Used `std::uint32_t` from `<cstdint>`. STLPort 4.5.3 predates C++11 fixed-width integer types; compile error `'uint32_t': is not a member of '_STL'`.
- **Fix:** Use the engine's `uint32` typedef from `sharedFoundationTypes/FoundationTypes.h` (transitively included via `FirstSharedFoundation.h` PCH). Removed `<cstdint>` include.
- **Files modified:** `std_hash_dump.cpp`
- **Committed in:** `3bbbc4e34` (Task 2)

**8. [Rule: acceptance criterion vs codebase reality] _STLP_* > 0 expectation**
- **Found during:** Task 1 (post-grep verification)
- **Issue:** Plan's Task 1 acceptance criterion required `_STLP_*` summary count > 0. Verified by direct grep: 0 files outside stlport453/. The grep glob is correct; the codebase genuinely has no `_STLP_*` references in client/server code post-Phase-7.
- **Fix:** Documented the 0 count as a valid pre-migration witness (post-migration must also be 0; any non-zero would indicate accidental `_STLP_*` introduction).
- **Files modified:** none (acceptance criterion in plan was the issue, not the artifact)
- **Committed in:** `1fdb220e8` (Task 1) — commit message and 09-00-SUMMARY.md document the deviation

**9. [Rule: capture-method discovery] Leak popup is gated by demoMode=true default**
- **Found during:** Task 3 (post-session log analysis + a follow-up demoMode=false re-capture)
- **Issue:** Plan's expected MemoryManager exit-leak summary never fired and didn't appear in `fps-baseline.log`. Initial assumption was a /SUBSYSTEM:WINDOWS + FileLogObserver-teardown ordering problem.
- **Root cause (discovered via re-capture):** SwgClient/ClientMain.cpp:162 sets `data.demoMode = true` at startup. The leak popup at MemoryManager.cpp:681 is gated by `if (!getDemoMode() && ms_allocations && ms_reportAllocations)` — demoMode=true short-circuits the entire leak path (no count, no popup). The earlier 8152 figure in STATE.md must have come from a build state with demoMode=false implicitly, or from a different binary, and is not reproducible against the post-Phase-7 codebase.
- **Fix:** Added `demoMode=false` to `[SharedFoundation]` in client.cfg (overrides ClientMain.cpp's default). Re-ran the VM session; warnings popup fired ("1378 warnings logged" — confirming the override took effect), but the leaks popup still did NOT fire. The codebase's own notALeak markup (via `AllocatedBlock::setCheckForLeaks(false)`) covers all remaining allocations at exit, so `MemoryManagerNamespace::report(true)` returns 0. Recorded `exit_leak_count: 0` as the pre-migration baseline.
- **Files modified:** `before-runtime.txt`
- **Committed in:** `62079e15a` (Task 3) + fixup commit for the leak-count finding

---

**Total deviations:** 9 auto-fixed (4 plan-vs-codebase API mismatches, 2 STLPort runtime defects, 1 stdout pollution, 1 missing setup, 1 environment mismatch). Plus 1 acceptance-criterion mismatch documented in commits, and 1 capture-method discovery (demoMode gate) which resolved into `exit_leak_count: 0`.

**Impact on plan:** All deviations are necessary corrections to plan-vs-reality drift, none represent scope expansion. All five baseline witnesses are captured as required. The leak-count baseline of 0 is genuinely meaningful (codebase has no remaining unexplained allocations at exit under its own notALeak markup) and Wave 3's after-baseline can compare against it directly using the same `demoMode=false` override.

## Issues Encountered

- **Background-task race on first capture-before-stl.sh run:** the script ran in background and I read BEFORE-STL.txt mid-write (29 bytes of header only). Re-ran in foreground, got the full 56 KB output. No fix needed — the script was correct; my read was premature.
- **MemoryManager output corruption:** a single `MM::remove` line from the allocator-tracking debug machinery split a hash entry mid-line on the first std_hash_dump run. Fixed by writing the dump to an argv-supplied file path instead of stdout.
- **DataTable helper segfaulted on first run:** FileStreamer not installed; root cause was missing SetupSharedFile::install(false) and DataTableManager::install() in the helper init. Modeled fix on DataTableTool's setup sequence.

## User Setup Required

None — all helpers are gated OFF by default, and the cfg edits to capture runtime baselines were applied to `build/bin/Debug/client.cfg` (gitignored, not the template). User can run regular cmake reconfigures + builds without touching anything from this plan.

If the user wants the runtime baseline cfg edits to survive future reconfigures, they should be added to `src/cmake/config/client.cfg.in` — not done here because they are capture-only edits, not normal-play config.

## Next Phase Readiness

- All five baseline artifacts committed and verifiable. Wave 3's D-04 gates can diff against them.
- The locked DataTable IFF target (`datatables/player/radial_menu.iff`) and persistent CRC function (`CrcLowerString::calculateCrc`) MUST match identically when capturing the after-baselines in 09-03 — see Task 7 of plan 09-03.
- For the after-baseline runtime capture, use the same client.cfg overrides (especially `demoMode=false` in `[SharedFoundation]`) so the leak-count + warnings-count comparison stays apples-to-apples.
- Plan 09-01 (Wave 1: engine/shared sweep) is unblocked — it will perform the first STL-ABI-changing source edits, after which no STLPort baseline is recoverable.

---
*Phase: 09-stlport-msvc-stl*
*Completed: 2026-05-08*
