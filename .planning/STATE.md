---
gsd_state_version: 1.0
milestone: v2.1
milestone_name: Decruft
status: executing
last_updated: "2026-05-25T15:48:09.027Z"
last_activity: 2026-05-25 -- Phase 13 planning complete
progress:
  total_phases: 4
  completed_phases: 1
  total_plans: 6
  completed_plans: 3
  percent: 50
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-05-25)

**Core value:** Every change must leave the client bootable to character select.
**Current focus:** Phase 12 — orphaned-directory-project-deletes

## Deferred Items (acknowledged at v2.0 close)

NOTE: "Remove dead code (CLEAN-01..04 vs MSBuild)" is now the ACTIVE v2.1 Decruft milestone (Phases 12–15) — no longer merely a backlog item.

Acknowledged and deferred at v2.0 milestone close (2026-05-25):

| Category | Item | Status |
|----------|------|--------|
| debug | cantina-corner-snap | closed (pre-existing engine quirk; not a regression) |
| debug | safecast-null-cast | closed (resolved 2026-05-15) |
| todo | 2026-05-15-cantina-corner-snap-engine-improvement | low — workaround exists |
| todo | 2026-05-15-swgsource-vs-whitengold-tre-asset-diff | low — informational, post-Phase-11 |
| backlog | Dead-code re-removal (CLEAN-01..04) vs MSBuild tree | **PROMOTED to active milestone v2.1 Decruft (Phases 12–15)** |
| backlog | DPVS D3D11 remeasure (SPEC R7); CLEAN-06 ~30 tools | carried to backlog |

## Current Position

Phase: 13
Plan: Not started
Status: Ready to execute
Last activity: 2026-05-25 -- Phase 13 planning complete

**v2.1 Decruft phase plan:**

| Phase | Goal | Requirements | Risk |
|-------|------|--------------|------|
| 12 | Orphaned directory & project deletes (trackIR, stationapi, SwgClientSetup, lcdui) | DECRUFT-01, -02, -03 | low — re-establishes boot baseline |
| 13 | VideoCapture library unlink | DECRUFT-04 | low/medium |
| 14 | Voice chat (Vivox) source removal | DECRUFT-05 | higher — touches live UI + CuiPreferences |
| 15 | XPCOM/Mozilla browser removal + cross-cutting boot gate | DECRUFT-06, -07 | highest — live UI + project + include path + stage copy; owns milestone gate |

## Accumulated Context

### Decisions

**v2.1 Decruft (roadmap):**

- [2026-05-25] v2.1 framed as **cleanup-only**: re-apply the orphaned CLEAN-01..04 removals to the active MSBuild tree. Visual Parity reordered to v2.2 (cleanup-first shrinks surface area before upstream imports). Reference diff template: the original whitengold (swg-client) **Phase 07** removal commits, retargeted CMake → MSBuild.
- [2026-05-25] Phase ordering is **risk-gradient, low-first**: pure deletes (Phase 12) → lib unlink (Phase 13) → live-source surgery (Phases 14 Vivox, 15 XPCOM). Re-establishes the boot baseline before the riskier source removals.
- [2026-05-25] DECRUFT-07 (dual-renderer boot gate) is a cross-cutting milestone gate owned by the final phase (15) but **verified incrementally** after every removal in Phases 12–15 — mirrors v2.0 CLEAN-05.
- [2026-05-25] XPCOM removal is a **clean unlink**: the Mozilla↔`/Zc:wchar_t` interlock was already resolved in Phase 9 (Option D went `char16_t`), so no ABI-boundary blocker remains.
- [2026-05-25] Bink video codec removal explicitly OUT of scope (active cutscene codec, higher blast radius than the dormant modules); v2.1 is dead-code-only.

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
- Voice chat (Vivox) disabled via config + ~20 ms_voiceChatManagerInstalled guards — full source removal is v2.1 Phase 14 (DECRUFT-05)
- In-game browser (XPCOM/Mozilla) stubbed (libMozilla::init returns true) — full removal is v2.1 Phase 15 (DECRUFT-06); the /Zc:wchar_t interlock is already gone (Phase 9 char16_t)
- allowTearing=true (prevents D3D9 vsync hang on WDDM/Win11) — stays in client.cfg
- lookUpCallStackNames=false (prevents DbgHelp stall) — stays in client.cfg
- SWGSource TRE mismatches handled with graceful guards (POB missing files, ContrailData cast, NebulaManagerClient null)
- [Phase 9]: Phase-9 Wave 1 (replan-2): runtime contract locked to v145 toolchain + 2016 SWGSource v3.0 DLLs + Tatooine zone-in (CONTEXT D-08/D-10); 8 task commits 898bc9a89..28d0281ce on 35b872357 base; zero src/ edits; boot gate STAGED-PENDING awaits user-run re-validation
- [Phase 9]: Phase-9 phase-wide DLL-restage protocol: SwgClient/src/CMakeLists.txt POST_BUILD copies 2010 leak DLLs from exe/win32/ over staged 2016 DLLs every build. Every wave's boot gate must re-stage 2016 DLLs after final build, or fix the root cause via a CMake source edit (Wave 2/3 candidate)
- [Phase 9 — 2026-05-09 boot gate REGRESSION]: CONTEXT D-08 ("v145 + 2016 DLL runtime contract") invalidated by Probe E + Probe G. v145 EXE + 2016 SWGSource v3.0 DLLs FATALs during graphics setup with "format arg out of range: value/max 1793887061/16" (callstack alternating EXE / DLL-at-`~0x6BE90000`); v145 EXE + 2010 leak DLLs runs cleanly. The user's working `build/bin/Debug/` (v143 + 2010 leak DLLs per Probe B MD5 verification) is the actually-empirically-validated runtime, NOT v145 + 2016. Replan-3 needed. See `.planning/phases/09-stlport-msvc-stl/.continue-here-replan3.md`. Three architectural options surfaced (Option A: switch to v143 + 2010 leak — recommended; Option B: forensics; Option C: pull Phase 11 forward).
- [Phase 9 — 2026-05-09 Option D empirically validated]: Koogie tree builds clean via VS 2026/v145 (`swg.sln`), boots to char-select against SWGSource VM, FATALs at world-load on `creation/default_pc_equipment.iff/LOEQ/0000/PTMP/ITEM: exiting chunk but not at the end of it` (same gap whitengold v1 guards with `exitChunk(true)` at `CuiCharacterLoadoutManager.cpp:162`). v2 working tree at `D:/Code/swg-client-v2/` on `koogie-msvc-cpp20-base` with merge anchor `479d35df3` (`--no-ff` merge of `koogie/MSVC-CPP20-Upgrade`). Option D adopted: Koogie supplies modernization; whitengold supplies SWGSource compat guards.
- [Phase 9 — 2026-05-10 replan-3 CONTEXT captured]: D-08..D-12 REVOKED. D-13..D-19 binding (Option D phase scope, working tree topology at `D:/Code/swg-client-v2/`, `.planning/` migration plan, two-plan structure 09-01 merge-anchor / 09-02 compat-guard-port, bisect-first compat-guard scope, batched upstream PR cadence post-Tatooine-PASS). STL-01..04 mechanically satisfied by Koogie merge `479d35df3`; STL-05 (Tatooine zone-in clean) is the only remaining requirement and is the 09-02 boot gate. `.planning/` stays in whitengold through Phase 9 closeout; top-level files copy to v2 as final 09-02 step.
- [Phase 9 — 2026-05-10 CLOSED via Option D]: STL-01..STL-05 all satisfied. STL-01..STL-04 mechanically satisfied by Koogie merge anchor `479d35df3` (CONTEXT.md D-14); STL-05 satisfied by IFF compat-guard port (v2 commit `460f4540dfb09acf50b41e37e49038229b18d3bc` on `koogie-msvc-cpp20-base`, port-forwarding whitengold `dd78832c4d5ad116ee049619e8c39a844597bd34`) plus Tatooine zone-in PASS against SWGSource VM 192.168.1.200 (evidence: `evidence/09-02-tatooine.png` 1,089,854 bytes; runtime log `D:/Code/swg-client-v2/stage/log-replan3-02.txt` 7.2 MB, zero FATAL). Task 3 bisect-first NO-OP (D-18): ContrailData / NebulaManager / POB candidates remained unported because the IFF guard alone delivered Tatooine clean. Active working tree: `D:/Code/swg-client-v2/` branch `koogie-msvc-cpp20-base`.
- [Phase 10 — 2026-05-14 CLOSED]: SCENE-CONDITIONAL verdict `remove for outdoor, keep for indoor` (17 CSVs / 6,072 frames across 4 scenarios). DPVS occlusion culling permanently disabled on the outdoor path at RenderWorld.cpp; portal traversal retained for indoor cells. Verdict doc: docs/recon/10-dpvs-profiling.md. (Full Wave-by-wave detail archived in milestones/v2.0-* and prior STATE revisions.)

**Phase 11:**

- [Phase 11 — 2026-05-24 CLOSED (PASS-WITH-DEFERRALS)]: D3D11 renderer plugin operational. 17-plan / ~45-iteration cascade. gl11_d.dll (1,889,280 bytes) satisfies the Gl_api table; D3D11 + D3D9 both buildable+selectable via rasterMajor. Visible-textured-Tatooine + visual parity achieved at Plan 11-09.15 Iter-39C (Iter-38B matrix-transpose [col-vec engine vs row-vec bytecode → XMMatrixTranspose] + Iter-39C per-pass blend wiring). SPEC R6 verdict PASS-WITH-DEFERRALS (docs/recon/11-d3d11-screenshots/comparison-notes.md); SPEC R7 DPVS remeasure (Plan 11-10) DEFERRED to post-Phase-11 (needs clean draw surface). CORRECTION: the "mini-map circular (Iter-44B win)" claim was FALSE — D3D11 radar never round (Phase 12-family PS-gen work, now v2.2); see memory project-phase11-minimap-never-round. **8 visual-parity entry points catalogued in 11-SUMMARY.md** — chief blocker is the asset PS pipeline (engine PEXE bytecode rejected by CreatePixelShader → dynamic PS samples slot 0 only → eyes/head/mini-map/particles wrong); then Pass::apply constant uploads, stencil state mapping, gamma LUT pass. (Full per-plan Phase 11 detail archived in milestones/v2.0-ROADMAP.md + 11-SUMMARY.md; trimmed from this STATE revision for context budget.)

### Pending Todos

1 pending:

- [Sync community compat fixes from SWG-Source/client-tools master](todos/pending/2026-05-08-sync-swg-source-community-compat.md) — future milestone (not v2.1 Decruft)

### Blockers/Concerns

- v2.1 Phase ordering is intentional: Phase 12 (low-risk deletes) MUST boot-verify before the riskier Phase 14/15 source surgery — re-establishes the dual-renderer baseline first.
- Every v2.1 removal step is boot-gated under BOTH `rasterMajor=5` (D3D9) and `=11` (D3D11). Debug exe reads `client_d.cfg` (not `client.cfg`) — set rasterMajor there for each smoke (memory note feedback_debug_exe_reads_client_d_cfg).
- Vivox (Phase 14) touches live `CuiPreferences` — strip voice keys carefully so no remaining caller references a removed key.
- **[Phase 12 — /FORCE false-pass]** SwgClient links under `/FORCE`, which downgrades unresolved externals (LNK2019) to WARNINGS and still emits a binary with exit 0. So `MSBuild exit 0` is NOT proof of a clean link — every removal-step build gate MUST grep the link output for `unresolved external symbol` (must be 0). 12-01 caught two plan defects this way: stationapi's `989crypt.lib` was a live dep (not stale); and dropping `ClientHeadTracking.cpp` orphaned its callers' symbols (fixed by stubbing the .cpp in-build, not dropping it). Apply the same scrutiny to 12-02/12-03 build gates.

## Deferred Items

Items carried from v1 close:

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| Build warnings | Nested CMake minimum-version deprecation warnings | Deferred (historical CMake tree) | Phase 1 |
| Build warnings | `crypto` C4530 exception-unwind warnings | Deferred | Phase 1 |
| Runtime | 156k DEBUG_WARNINGs at exit (mostly per-frame nebula lightning) | Deferred | v1 post-launch |
| Runtime | 8,152 memory leaks at exit (singletons/caches, expected) | Deferred | v1 post-launch |

## Session Continuity

Last session: 2026-05-25T15:07:37.540Z
Resume (2026-05-25): **v2.1 Decruft roadmap CREATED** (Phases 12–15; DECRUFT-01..07 mapped 100%). v2.0 Modernisation shipped + tagged `v2.0`. Repo: swg-client-v2 (MSBuild/Koogie) is the single source of truth.

**v2.1 Decruft — the plan:**
Re-apply the orphaned CLEAN-01..04 removals to the active MSBuild tree, ordered low-risk-first:

1. **Phase 12** (DECRUFT-01/-02/-03) — delete trackIR/stationapi dirs + drop SwgClientSetup.vcxproj + lcdui.vcxproj from swg.sln + purge lcdui `.rsp` refs. Re-establishes the dual-renderer boot baseline.
2. **Phase 13** (DECRUFT-04) — unlink `VideoCapture_debug.lib` from SwgClient `libraries_d.rsp` (+ release `.rsp`); purge source/include residue.
3. **Phase 14** (DECRUFT-05) — remove Vivox: unlink `vivoxSharedWrapper_debug.lib`; delete `CuiVoiceChatManager`/`SwgCuiVoiceFlyBar`/`CuiVoiceChatEventHandler`; strip voice keys from `CuiPreferences`.
4. **Phase 15** (DECRUFT-06 + DECRUFT-07) — remove XPCOM/Mozilla: drop `libMozilla.vcxproj` from swg.sln; remove `libMozilla/include/public` from `includePaths.rsp`; delete `CuiWebBrowser*`/`UIWebBrowserWidget`/XPCOM bridge; drop staged Mozilla DLLs. Then run the milestone-wide dual-renderer boot gate (DECRUFT-07).

Reference diff template: the original whitengold (swg-client) **Phase 07** removal commits (CLEAN-01..05), retargeted CMake → MSBuild. **Invariant:** every step boots to character select under both `rasterMajor=5` and `=11`.

**Next action:** `/gsd-plan-phase 12` to decompose Phase 12 into executable plans.

Known unrelated long-tail (deferred): 0x087a armor_marauder async crash (cross-client, retry works); ~11-min ExceptionHandler crash; first-launch login flakiness. Koogie's uncommitted Direct3d9.cpp Utinni vtable probe in the working tree is a separate sidequest — leave untouched. v2.2 Visual Parity (asset PS pipeline blocker) is the next milestone after Decruft; read `docs/research/phase12-baseline/COMPARISON.md` when it opens.
