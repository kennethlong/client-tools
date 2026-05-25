---
phase: 9
slug: stlport-msvc-stl
status: draft
nyquist_compliant: true
wave_0_complete: true
created: 2026-05-08
---

# Phase 9 — Validation Strategy

> Per-phase validation contract for the STLPort 4.5.3 → MSVC STL migration. Derived from RESEARCH.md §Validation Architecture and CONTEXT.md §D-04. Build + dumpbin + manual VM boot are the test framework — no formal unit-test framework exists in this codebase.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Build verification (`cmake --build`) + symbol scan (`dumpbin /imports`) + manual boot test against SWGSource VM. No xUnit-style framework in tree. |
| **Config file** | none — builds + manual VM session are the test |
| **Quick run command** | `cmake --build build --config Debug --target SwgClient` |
| **Full suite command** | `cmake --build build --config Debug` (full graph; in Wave 2 this also exercises the re-enabled MFC canaries) |
| **Estimated runtime** | ~3–6 min full graph; ~30–90 s incremental per library |

---

## Sampling Rate

- **After every task commit:** `cmake --build build --config Debug --target <library-being-edited>` (per-lib feedback, ~30–90 s)
- **After every plan wave:** `cmake --build build --config Debug` (full graph) + boot SwgClient_d.exe to character-select against SWGSource VM
- **Before `/gsd-verify-work`:** Full suite green AND all four FINAL validation commands pass (dumpbin clean, hash determinism diff, round-trip persist diff, sort stability + frame-time + leak-count baseline diff)
- **Max feedback latency:** ~90 s per library compile; ~6 min per wave full-graph

---

## Per-Task Verification Map

> Concrete task IDs are assigned by `/gsd-plan-phase` (planner). The mapping below pre-allocates per-wave categories so each task generated has a slot. Acceptance commands are taken verbatim from RESEARCH.md §Validation Architecture.

| Task slot | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|-----------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 09-W0-01 | (pre-W1) | 0 | STL-01..05 | — | Capture BEFORE-STL grep snapshot | baseline | bash script in `baseline/capture-before-stl.sh` (Wave 0) | ❌ W0 | ⬜ pending |
| 09-W0-02 | (pre-W1) | 0 | STL-05 | — | Capture runtime baseline (FPS + leak count) | manual | manual VM session, write `baseline/before-runtime.txt` | ❌ W0 | ⬜ pending |
| 09-W0-03 | (pre-W1) | 0 | STL-02, STL-05 | T-9-01 | Capture hash determinism baseline | baseline | LabelHashTool over fixed input list → `baseline/before-crc.txt` | ❌ W0 | ⬜ pending |
| 09-W0-04 | (pre-W1) | 0 | STL-05 | T-9-01 | Capture DataTable round-trip baseline | baseline | dump retail datatable cells → `baseline/before-datatable-round-trip.txt` | ❌ W0 | ⬜ pending |
| 09-W1-NN | 01 | 1 | STL-01, STL-02 | — | engine/shared STLPort gone; StlCompat.h alias compiles | unit | `cmake --build build --config Debug --target sharedFoundation && ... per-lib` | ✅ | ⬜ pending |
| 09-W1-end | 01 | 1 | STL-05 | — | Boot to character select after Wave 1 | manual | `cmake --build build --config Debug && build\bin\Debug\SwgClient_d.exe` + manual VM session | ✅ | ⬜ pending |
| 09-W2-NN | 02 | 2 | STL-01, STL-02 | — | engine/client + game STLPort gone | unit | `cmake --build build --config Debug --target SwgClient` | ✅ | ⬜ pending |
| 09-W2-canary | 02 | 2 | STL-05 | — | MFC canaries (Turf + Viewer) compile | unit | `cmake --build build --config Debug --target Turf && cmake --build build --config Debug --target Viewer` | ❌ W0* | ⬜ pending |
| 09-W2-end | 02 | 2 | STL-05 | — | Boot to character select after Wave 2 | manual | `cmake --build build --config Debug && build\bin\Debug\SwgClient_d.exe` + manual VM session | ✅ | ⬜ pending |
| 09-W3-flag | 03 | 3 | STL-03, STL-04 | — | `/Zc:wchar_t-` and `/FORCE:MULTIPLE` absent from CMake | unit | `grep -rn "/Zc:wchar_t-\|FORCE:MULTIPLE\|stlport_vc71_static.lib\|find_package(STLPort\|WHITENGOLD_USE_STLPORT_HEADERS\|stlport_vc143_compat" src/ --include="CMakeLists.txt" --include="*.cmake"` returns ZERO | ✅ | ⬜ pending |
| 09-W3-typedef | 03 | 3 | STL-03 | — | `unicode_char_t` is `typedef wchar_t` in BOTH definition sites | unit | `grep -n "typedef.*unicode_char_t" src/external/ours/library/unicode/src/shared/Unicode.h src/engine/shared/library/sharedFoundation/src/shared/StlForwardDeclaration.h` | ✅ | ⬜ pending |
| 09-W3-end | 03 | 3 | STL-05 | — | Final boot + dumpbin clean | manual + unit | full-graph build, dumpbin clean, manual VM boot | ✅ | ⬜ pending |
| 09-FINAL-1 | (post-W3) | F | STL-05 | — | dumpbin shows zero stlport symbols | unit | `dumpbin /imports build\bin\Debug\SwgClient_d.exe \| findstr /i "stlport"` returns ZERO | ✅ | ⬜ pending |
| 09-FINAL-2 | (post-W3) | F | STL-02 | T-9-01 | Hash determinism — CRC column byte-identical | unit | `diff baseline/before-crc.txt baseline/after-crc.txt` (CRC column only) | ✅ | ⬜ pending |
| 09-FINAL-3 | (post-W3) | F | STL-05 | T-9-01 | DataTable round-trip byte-identical | unit | `diff baseline/before-datatable-round-trip.txt baseline/after-datatable-round-trip.txt` | ✅ | ⬜ pending |
| 09-FINAL-4 | (post-W3) | F | STL-05 | T-9-02 | Sort stability — pre/post `std::sort` of CrcLowerString array match | unit | 50-line C++ helper writes pre/post dumps; `diff` returns empty | ❌ W0 | ⬜ pending |
| 09-FINAL-5 | (post-W3) | F | STL-05 | T-9-03 | Frame-time within ±10%, exit-leak count exact match | manual | manual VM session, capture `after-runtime.txt`, diff against `before-runtime.txt` | ✅ | ⬜ pending |

*Status legend: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*
*"File Exists" column: ✅ = artifact path exists pre-Wave 1; ❌ W0 = Wave 0 task creates it.*

*Note on "Threat Ref" column: T-9-01 = persisted hash divergence causing TRE asset CRC mismatch; T-9-02 = sort instability changing rendering / serialization order; T-9-03 = allocator ABI shift causing leak-count drift. See Security Domain in RESEARCH.md.*

---

## Wave 0 Requirements

Wave 0 = artifact creation and baseline capture before Wave 1 begins. All four BEFORE-STL captures live under `.planning/phases/09-stlport-msvc-stl/baseline/`.

- [ ] `.planning/phases/09-stlport-msvc-stl/baseline/` — directory created
- [ ] `baseline/BEFORE-STL.txt` — code/symbol grep snapshot (script + output) covering: `_STL::`, `_STLP_*`, `hash_map`, `hash_set`, `auto_ptr`, `binder1st`, `binder2nd`, `ptr_fun`, `mem_fun`, `<hash_map>`, `<hash_set>`, `<sgi/`. Excludes `stlport453/` directory.
- [ ] `baseline/before-runtime.txt` — manual VM session capture of `character_select_fps`, `mos_eisley_fps`, `exit_leak_count`, `exit_warning_count` against pre-Phase-9 STLPort build.
- [ ] `baseline/before-crc.txt` — ~100 known input strings × 2 columns (CRC32 from `Crc::calculateWithToLower` via existing LabelHashTool, plus `std::hash<std::string>` from a 30-line helper). Use first 100 lines of `publish/data_<patch>.txt` as the input set.
- [ ] `baseline/before-datatable-round-trip.txt` — load `misc/quest_task_data.iff` (or another small known retail datatable, ~20 columns), dump every cell as text via DataTable's existing accessors. 50-line helper acceptable.
- [ ] `baseline/before-sort-stability.txt` — 50-line C++ helper: pin a known CrcLowerString array, run `std::sort` with `LessAbcOrderReferenceComparator`, dump indices in sorted order.
- [ ] No new test framework or library install required — every Wave 0 artifact reuses existing tools (LabelHashTool, DataTable accessors, raw `std::sort`).

*If any Wave 0 capture fails to record (e.g., LabelHashTool can't be located), planner must add a Wave 0 task to create the missing helper before Wave 1 starts.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Boot to character select against SWGSource VM (every wave) | STL-05 | No automated client-vs-server harness exists; SWGSource VM startup requires manual Oracle + ant build per session (see user memory `project_vm_server_setup.md`) | 1) Start SWGSource VM, run Oracle startup + stationchat + ant build per memory protocol. 2) `cmake --build build --config Debug && build\bin\Debug\SwgClient_d.exe`. 3) Confirm character-select screen renders. 4) Note any new fatals or warnings in client_console.log. |
| Frame-time + exit-leak-count baseline diff (after Wave 3) | STL-05 | Same as boot — requires running client | 1) Boot SwgClient_d.exe against VM. 2) Reach character-select; capture FPS over 30 s. 3) Enter Mos Eisley plaza; capture FPS over 60 s. 4) Exit gracefully. 5) Read exit-leak-count + exit-warning-count from log. 6) Diff against `baseline/before-runtime.txt`; tolerate ±10% on FPS, exact match on leak count modulo expected singletons. |
| MFC canary build (Wave 2) | STL-05 (canary signal that STLPort is genuinely gone, not just its symbols) | The canary is "does Turf + Viewer compile," which IS automated — but the act of re-enabling them in the tier parents is a one-line uncomment per file (deferred since Phase 8) | Edit `engine/shared/application/CMakeLists.txt` and `engine/client/application/CMakeLists.txt` to uncomment the deferred `add_subdirectory(Turf)` and `add_subdirectory(Viewer)` lines. Then `cmake --build build --config Debug --target Turf && cmake --build build --config Debug --target Viewer`. |

---

## Validation Sign-Off

- [ ] All non-baseline tasks have `<automated>` verify command in PLAN.md
- [ ] Sampling continuity: no 3 consecutive task commits without an automated verify (every per-library STLPort removal commit has its `cmake --build --target <lib>` checkpoint)
- [ ] Wave 0 covers all MISSING references (5 baselines listed above)
- [ ] No watch-mode flags
- [ ] Feedback latency < 90 s per library compile, < 6 min per full-graph wave gate
- [ ] `nyquist_compliant: true` set in frontmatter once Wave 0 baselines are captured and per-task verify map is populated by planner

**Approval:** wave-0 baselines captured 2026-05-08 (per 09-00-SUMMARY.md commits 1fdb220e8 / 3bbbc4e34 / 62079e15a). Per-task verify map populated by 09-01 / 09-02 / 09-03 / 09-04 plans. Nyquist-compliant: every code-producing task has an `<automated>` verify command; baseline witnesses for the four D-04 final gates exist on disk under `baseline/`.
