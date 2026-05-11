---
gsd_state_version: 1.0
milestone: v2.0
milestone_name: milestone
status: executing
stopped_at: "Phase 10 Wave 1 (plan 10-02) COMPLETE -- GPU-timing plumbing across renderer-plugin DLL boundary landed in 2 atomic commits (0e1e25a6d engine surface; 9bd97c459 Direct3d9 plugin impl). Three new Gl_api function pointers (dpvsGpuTimingBegin/End/PollResult) + double-buffered IDirect3DQuery9 timestamp pool (N=3) in Direct3d9 plugin + thin Graphics::* forwarders. Build green at -t:Direct3d9 / -t:clientGraphics / -t:SwgClient (relink confirms engine forwarders resolve). Engine still sees zero IDirect3DQuery9 references -- only POD types cross the DLL boundary per RESEARCH.md Pattern 4. 3 auto-fix deviations applied (Rule 1 ms_api-> idiom correction; Rule 2 d.frequency==0 div-by-zero guard; Rule 2 explicit ShutdownPool function + remove() call-site for COM ordering)."
last_updated: "2026-05-11T04:40:18Z"
last_activity: 2026-05-11
progress:
  total_phases: 5
  completed_phases: 0
  total_plans: 7
  completed_plans: 2
  percent: 28
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-05-07)

**Core value:** Every change must leave the client bootable to character select.
**Current focus:** Phase 10 — dpvs-culling-experiment

## Current Position

Phase: 10 (dpvs-culling-experiment) — EXECUTING
Plan: 3 of 7
Status: Ready to execute (Wave 1 complete; Wave 2 DpvsProfileInstrumentation module next)
Last activity: 2026-05-11

Progress: [██░░░░░░░░] 28%

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

### v2 Phase Plan

| Phase | Goal | Key Requirements | Status |
|-------|------|-----------------|--------|
| 7 | Dead code Track A: deletes + CMake unlinks + Vivox + XPCOM | CLEAN-01..05 | Complete (2026-05-07) |
| 8 | Dead code Track B: ~40 tools wired to CMake | CLEAN-06 | Closed-as-scoped (12 wired, ~30 deferred to Phase 9 + 12.x) |
| 9 | STLPort → MSVC STL | STL-01..05 | Complete (2026-05-10 via Option D: Koogie merge 479d35df3 + IFF guard port 460f4540d) |
| 10 | DPVS culling experiment | DPVS-01..02 | In Progress (2/7 plans complete: 10-01 Wave 0 scaffolding + 10-02 Wave 1 GPU-timing plumbing) |
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

Last session: 2026-05-11T04:40:18Z
Stopped at: Phase 10 Wave 1 (plan 10-02) COMPLETE. GPU-timing plumbing across renderer-plugin DLL boundary landed in 2 atomic commits (0e1e25a6d engine surface; 9bd97c459 Direct3d9 plugin impl). Build green at -t:Direct3d9 / -t:clientGraphics / -t:SwgClient. Wave 2 (plan 10-03) `DpvsProfileInstrumentation.{h,cpp}` engine module is unblocked — Graphics::dpvsGpuTimingBegin/End/PollResult are public static methods ready to be consumed by the singleton CSV writer + overlay + ExitChain teardown that lands next.
Resume: /clear then `/gsd-execute-phase 10` (Wave 2: plan 10-03 -- DpvsProfileInstrumentation engine module). Deferred long-tails (unchanged): post-Phase-9 upstream PR series for the IFF guard (commit `460f4540d`) per D-19; ExceptionHandler crash after ~11 min in-world (future `/gsd-debug`); first-launch login flakiness; pre-existing Koogie post-build copy MSB3073 (logged in Wave 0 + Wave 1 SUMMARIES); pre-existing Direct3d9.vcxproj MSB8012 TargetName/OutputFile mismatch (Phase 11 candidate to revisit if Direct3d11.dll lives alongside).
