---
gsd_state_version: 1.0
milestone: v2.0
milestone_name: milestone
status: executing
stopped_at: Phase 11 context gathered
last_updated: "2026-05-16T04:07:30.331Z"
last_activity: 2026-05-16 -- Phase 11 planning complete
progress:
  total_phases: 5
  completed_phases: 1
  total_plans: 16
  completed_plans: 8
  percent: 50
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-05-07)

**Core value:** Every change must leave the client bootable to character select.
**Current focus:** Phase 10 — dpvs-culling-experiment

## Current Position

Phase: 10 (dpvs-culling-experiment) — ✓ COMPLETE
Plan: 7 of 7 ✓
Status: Ready to execute
Last activity: 2026-05-16 -- Phase 11 planning complete
Verdict: `remove` globally per Option α. Long-form record: docs/recon/10-dpvs-profiling.md. Phase 11 reconsideration: ROADMAP criterion #6.

Progress: [██████████] 100% (Phase 10)

## Accumulated Context

### Decisions

**Phase 8:**

- Plan 08-02 finding: this leaked tree's tool source code is **mid-NGE-refactor**. Many tool sources reference engine APIs that have since been removed/renamed in the engine itself. CMake authoring alone cannot complete CLEAN-06; targeted source-modernization plans are needed.
- Plan 08-02 partition: 1 building (DebugWindow), 10 MFC-blocked (Phase 9 unblocks), 4 NGE-source-mismatch, 1 missing-SDK (PIX), 2 build-system-gap (Miff/CrashReporter), 1 needs-link-wire (DllExport), 12 Qt batch. (commit eb549831d)
- Plan 08-01: 5 MFC tools (Turf, WordCountTool, StringFileTool, DataLintRspBuilder, TreeFileRspBuilder) DEFERRED to Phase 9 — STLPort 4.5.3 cannot coexist with `<afxwin.h>` on the same translation unit under MSVC 14.44. Per-tool CMakeLists.txt authored and committed; commented out in tier parent until Phase 9 swaps STLPort for MSVC STL. (commit c4fe90f84)
- Plan 08-01: New `perforce_tzname_compat` OBJECT lib provides `__tzname` data symbol for pre-UCRT Perforce libsupp.lib, isolated from STLPort's time.h shim that redefines it as a function. (commit c4fe90f84)
- VS 2022 component "C++ MFC for v143 build tools (x86 & x64)" installed as one-time tooling install — required for any future MFC tool builds, even though MFC tools currently deferred.
- TreeFileBuilder source patched: `borrowCompressor`/`returnCompressor` API removed in NGE refactor; replaced with direct `ZlibCompressor` instantiation. (deviation, commit c4fe90f84)

Decisions carried forward from v1:

- v1 complete: CMake + VS 2022 build, SwgClient_d.exe, boots to character select + ground scene against SWGSource StellaBellum VM at 192.168.1.200
- STLPort vc71/MSVC 2022 ABI compat shims in place (stlport_vc143_compat.cpp, /FORCE:MULTIPLE) — these are the primary removal target in Phase 9
- Voice chat (Vivox) disabled via config + ~20 ms_voiceChatManagerInstalled guards — full source removal is Phase 7 CLEAN-03
- In-game browser (XPCOM/Mozilla) stubbed (libMozilla::init returns true) — full removal is Phase 7 CLEAN-04; this enables /Zc:wchar_t drop in Phase 9
- /Zc:wchar_t- currently required by XPCOM PRUnichar ABI — can be dropped after Phase 7 CLEAN-04
- allowTearing=true (prevents D3D9 vsync hang on WDDM/Win11) — stays in client.cfg
- lookUpCallStackNames=false (prevents DbgHelp stall) — stays in client.cfg
- SWGSource TRE mismatches handled with graceful guards (POB missing files, ContrailData cast, NebulaManagerClient null)
- [Phase 9]: Phase-9 Wave 1 (replan-2): runtime contract locked to v145 toolchain + 2016 SWGSource v3.0 DLLs + Tatooine zone-in (CONTEXT D-08/D-10); 8 task commits 898bc9a89..28d0281ce on 35b872357 base; zero src/ edits; boot gate STAGED-PENDING awaits user-run re-validation
- [Phase 9]: Phase-9 phase-wide DLL-restage protocol: SwgClient/src/CMakeLists.txt POST_BUILD copies 2010 leak DLLs from exe/win32/ over staged 2016 DLLs every build. Every wave's boot gate must re-stage 2016 DLLs after final build, or fix the root cause via a CMake source edit (Wave 2/3 candidate)
- [Phase 9 — 2026-05-09 boot gate REGRESSION]: CONTEXT D-08 ("v145 + 2016 DLL runtime contract") invalidated by Probe E + Probe G. v145 EXE + 2016 SWGSource v3.0 DLLs FATALs during graphics setup with "format arg out of range: value/max 1793887061/16" (callstack alternating EXE / DLL-at-`~0x6BE90000`); v145 EXE + 2010 leak DLLs runs cleanly. The user's working `build/bin/Debug/` (v143 + 2010 leak DLLs per Probe B MD5 verification) is the actually-empirically-validated runtime, NOT v145 + 2016. Replan-3 needed. See `.planning/phases/09-stlport-msvc-stl/.continue-here-replan3.md`. Three architectural options surfaced (Option A: switch to v143 + 2010 leak — recommended; Option B: forensics; Option C: pull Phase 11 forward).
- [Phase 9 — 2026-05-09 Option D empirically validated]: Koogie tree builds clean via VS 2026/v145 (`swg.sln`), boots to char-select against SWGSource VM, FATALs at world-load on `creation/default_pc_equipment.iff/LOEQ/0000/PTMP/ITEM: exiting chunk but not at the end of it` (same gap whitengold v1 guards with `exitChunk(true)` at `CuiCharacterLoadoutManager.cpp:162`). v2 working tree at `D:/Code/swg-client-v2/` on `koogie-msvc-cpp20-base` with merge anchor `479d35df3` (`--no-ff` merge of `koogie/MSVC-CPP20-Upgrade`). Option D adopted: Koogie supplies modernization; whitengold supplies SWGSource compat guards.
- [Phase 9 — 2026-05-10 replan-3 CONTEXT captured]: D-08..D-12 REVOKED. D-13..D-19 binding (Option D phase scope, working tree topology at `D:/Code/swg-client-v2/`, `.planning/` migration plan, two-plan structure 09-01 merge-anchor / 09-02 compat-guard-port, bisect-first compat-guard scope, batched upstream PR cadence post-Tatooine-PASS). STL-01..04 mechanically satisfied by Koogie merge `479d35df3`; STL-05 (Tatooine zone-in clean) is the only remaining requirement and is the 09-02 boot gate. `.planning/` stays in whitengold through Phase 9 closeout; top-level files copy to v2 as final 09-02 step.
- [Phase 10 — 2026-05-11 Wave 1 complete (plan 10-02)]: GPU-timing plumbing across renderer-plugin DLL boundary. Three new Gl_api function pointers (dpvsGpuTimingBegin/End/PollResult) appended after pix* entries; engine-side Graphics::* forwarders dispatch through `ms_api->` (matching verified pix* idiom -- NOT `ms_glApi.` as plan template suggested; Rule 1 deviation); plugin-side double-buffered IDirect3DQuery9 pool (N=3) lazy-creates on first Begin, reads slot (issueFrame-2)%N per RESEARCH.md Pitfall 1, handles disjoint flag per Pitfall 2; explicit dpvsGpuTimingShutdownPool() called from Direct3d9Namespace::remove() before ms_device->Release() (Rule 2 COM-ordering safety); div-by-zero guard on d.frequency==0 (Rule 2 driver-bug defense). All new code carries THROWAWAY/D-15 comment markers (9 occurrences across 4 files) for Wave 5 cleanup grep target. Build verified -t:Direct3d9 + -t:clientGraphics + -t:SwgClient; gl05_d.dll (4.5MB) + clientGraphics.lib (12.1MB) + SwgClient_d.exe (72.5MB) all relink cleanly. Pre-existing Koogie post-build MSB3073 + Direct3d9.vcxproj MSB8012 warnings remain out-of-scope (Wave 0 deferred-items precedent). Commits: 0e1e25a6d (Task 1 engine surface), 9bd97c459 (Task 2 plugin impl + build log force-added).
- [Phase 9 — 2026-05-10 CLOSED via Option D]: STL-01..STL-05 all satisfied. STL-01..STL-04 mechanically satisfied by Koogie merge anchor `479d35df3` (CONTEXT.md D-14); STL-05 satisfied by IFF compat-guard port (v2 commit `460f4540dfb09acf50b41e37e49038229b18d3bc` on `koogie-msvc-cpp20-base`, port-forwarding whitengold `dd78832c4d5ad116ee049619e8c39a844597bd34`) plus Tatooine zone-in PASS against SWGSource VM 192.168.1.200 (evidence: `evidence/09-02-tatooine.png` 1,089,854 bytes; runtime log `D:/Code/swg-client-v2/stage/log-replan3-02.txt` 7.2 MB, zero FATAL). Task 3 bisect-first NO-OP (D-18): ContrailData / NebulaManager / POB candidates remained unported because the IFF guard alone delivered Tatooine clean. Active working tree: `D:/Code/swg-client-v2/` branch `koogie-msvc-cpp20-base`. Post-Phase-9 followthrough: upstream PR series to SWG-Source/master per D-19 + memory `project_swg_source_upstreaming.md`; runtime stability long-tail (ExceptionHandler crash after ~11 min in-world) deferred to future `/gsd-debug` session; first-launch login flakiness observed (back-out + retry login twice before repeatable) — not an STL-05 blocker.
- [Phase 10 — 2026-05-11 Wave 2 complete (plan 10-03)]: DpvsProfileInstrumentation engine module landed in 3 atomic commits (3fb4c9804 public header; f55da0cef CSV writer + run-label sanitizer + DebugFlag overlay + ExitChain teardown; 1f5cd24f1 vcxproj wire + SetupClientGraphics install hook + Rule 3 RenderWorld::getDisableOcclusionCulling getter). Static-only class with anonymous-namespace state; AbstractFile* via StdioFile in append-binary; run-label sanitizer covers CSV-cell + filename + path-traversal + formula-injection in one pass; two-gate per-frame onFrameEnd (ms_installed first, drain GPU pool, then ms_captureActive for row write) per RESEARCH.md Pitfall 4 intent. CSV header byte-for-byte matches tools/dpvs-profile/analysis.py EXPECTED_HEADER. DebugFlag-driven overlay via DEBUG_REPORT_PRINT gated by [ClientGraphics/Dpvs] reportInstrumentation. Build verified -t:clientGraphics EXIT=0 (clientGraphics.lib 12.3 MB, +195 KB vs Wave 1) and -t:SwgClient SwgClient_d.exe 72.5 MB (+10 KB vs Wave 1; pre-existing Koogie post-build MSB3073 the only failure). Rule 3 auto-fix: RenderWorld::getDisableOcclusionCulling getter added here (MSVC compile vs link surface mismatch -- plan author expected link error); plan 10-04 Task 1 no-op for that specific edit. 19 THROWAWAY/D-15 markers across 5 affected files.
- [Phase Phase 10]: Phase 10 Wave 3 complete (plan 10-04): DpvsProfileInstrumentation hook wiring landed in 4 atomic commits (cca3dcebc RenderWorld GPU/CPU bracket; 2688a0bdd Game::run onFrameEnd hook + CuiIoWin _DEBUG F10/F11 intercept; bf464e3ee /setrunlabel console command; d8dbd4076 SwgClient build log force-add). Pre-landed Rule 3 detection: RenderWorld::getDisableOcclusionCulling getter was already committed in Wave 3 (1f5cd24f1) -- Task 1 Part A skipped as no-op per Wave 3 SUMMARY documentation. SwgClient_d.exe rebuilt at 72,552,448 bytes (+512 bytes vs Wave 3 baseline). Pre-existing Koogie post-build MSB3073 the only failure (out-of-scope per Wave 0/1/2/3 precedent). Task 5 (checkpoint:human-verify smoke session) DEFERRED to Wave 5's capture session per prompt key_context. Wave 5 (plan 10-05) unblocked: capture protocol drives F10/F11/setrunlabel + 6 CSV files for verdict aggregation.
- [Phase 10 — 2026-05-14 Wave 4 complete (plan 10-05)]: Manual capture session across 3 client launches against SWGSource VM at 192.168.1.200. SCENE-CONDITIONAL verdict: `remove for outdoor, keep for indoor`. Delivered 17 CSVs / 6,072 frames across 4 scenarios — mosEisley plaza (sparse: 6 ON / 3 OFF), starport (dense: 1 ON / 1 OFF), walking (moving camera: 2 ON / 1 OFF), cantinaInterior (indoor: 1 ON / 2 OFF). All 3 outdoor scenes independently voted `remove`: DPVS CPU cost ~940 µs/frame at plaza → ~1,592 µs/frame at starport (scale with scene complexity); OFF wins by 0.94-2.13 ms median (3.3-9.1%). Cantina interior voted `keep`: DPVS SAVES ~940 µs CPU/frame in tight geometry-dense cells; ON wins by 0.62 ms median (2.2%). Indoor result emerged from session-3 follow-up after session-2 only captured indoor OFF; the missing ON pass reversed a mid-session hypothesis that DPVS could be removed everywhere. Confirmed: ROADMAP Phase 10 success criteria #3 framing was right — outdoor and indoor paths diverge cleanly at RenderWorld.cpp:908 and :911 (cullingParameters bitmask); single resolveVisibility() call at :1062 is shared. Caveats: (1) gpu_us=0 across all frames — Wave 1 timestamp-query pool silently failing, CPU side drove clean verdict, GPU side cost unmeasured (deferred follow-up); (2) v2 launches with DPVS:OFF as default state confirmed across both session-2 and session-3 relaunches (probably persisted via local_machine_options.iff), opposite of session 1; recovered via rename + run_label-column patch using dpvs_occlusion_flag as authoritative truth; (3) Shuttle takeoff caused 147 ms max in starport-OFF transient, walking-OFF and cantina-ON have similar single-frame outliers, p95/p99 unaffected. New stage file: `[ClientGraphics/Dpvs] reportInstrumentation=true` block added to client_d.cfg. Plan 10-06 (Wave 5) unblocked: scene-conditional source edits — strip OCCLUSION_CULLING from outdoor path at RenderWorld.cpp:908, RETAIN on indoor path at :911. SafeCast.h:29 dialog-twice issue tracked separately in `.planning/todos/pending/2026-05-14-safecast-null-dynamic-cast-world-load.md` — not a Phase 10 blocker.

### v2 Phase Plan

| Phase | Goal | Key Requirements | Status |
|-------|------|-----------------|--------|
| 7 | Dead code Track A: deletes + CMake unlinks + Vivox + XPCOM | CLEAN-01..05 | Complete (2026-05-07) |
| 8 | Dead code Track B: ~40 tools wired to CMake | CLEAN-06 | Closed-as-scoped (12 wired, ~30 deferred to Phase 9 + 12.x) |
| 9 | STLPort → MSVC STL | STL-01..05 | Complete (2026-05-10 via Option D: Koogie merge 479d35df3 + IFF guard port 460f4540d) |
| 10 | DPVS culling experiment | DPVS-01..02 | ✓ Complete 2026-05-15. Verdict = `remove` (Option α). DPVS occlusion culling permanently disabled at RenderWorld.cpp:909/913; runtime toggle + config key deleted; measurement instrumentation removed (-726 lines across 12 files). Phase 11 reconsideration in ROADMAP criterion #6. Verdict doc: docs/recon/10-dpvs-profiling.md |
| 11 | D3D11 renderer plugin | D3D11-01..05 | Not started |

### Pending Todos

1 pending:

- [Sync community compat fixes from SWG-Source/client-tools master](todos/pending/2026-05-08-sync-swg-source-community-compat.md) — future milestone (not v2 Phase 1)

### Blockers/Concerns

- Phase 7 Track A must complete and boot-verify before Track B (Phase 8) starts — user explicitly requested this ordering
- Phase 9 STL swap depends on Phase 7 XPCOM removal (PRUnichar/wchar_t ABI boundary gone)
- Phase 11 D3D11 is estimated 2-4 months of work; phases 7-8 are the immediate focus

## Deferred Items

Items carried from v1 close:

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| Build warnings | Nested CMake minimum-version deprecation warnings | Deferred | Phase 1 |
| Build warnings | `crypto` C4530 exception-unwind warnings | Deferred | Phase 1 |
| Runtime | 156k DEBUG_WARNINGs at exit (mostly per-frame nebula lightning) | Deferred | v1 post-launch |
| Runtime | 8,152 memory leaks at exit (singletons/caches, expected) | Deferred | v1 post-launch |

## Session Continuity

Last session: 2026-05-16T00:02:51.049Z
Stopped at: Phase 11 context gathered
Resume: /clear then `/gsd-debug` (start with cantina corner-snap since it has the strongest lead: OutputDebugString in Report.cpp:145; run Kenny's Release-build binary-search test first per the todo's pre-fix-test protocol; then apply IsDebuggerPresent() wrap if confirmed). OR `/gsd-debug` for SafeCast.h:29 if you want to tackle that first. Both todos in `.planning/todos/pending/`. Deferred long-tails (unchanged): SafeCast.h:29 dialog-twice on world load (`.planning/todos/pending/2026-05-14-safecast-null-dynamic-cast-world-load.md`, next session via /gsd-debug); post-Phase-9 upstream PR series for the IFF guard (commit `460f4540d`) per D-19; ExceptionHandler crash after ~11 min in-world (future `/gsd-debug`); first-launch login flakiness; pre-existing Koogie post-build copy MSB3073; pre-existing Direct3d9.vcxproj MSB8012 TargetName/OutputFile mismatch (Phase 11 candidate).
