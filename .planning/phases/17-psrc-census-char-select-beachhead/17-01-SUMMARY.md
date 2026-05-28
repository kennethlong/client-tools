---
phase: 17-psrc-census-char-select-beachhead
plan: 01
subsystem: renderer
tags: [d3d11, d3d9, clientGraphics, ShaderImplementation, ConfigClientGraphics, psrc, census, char-select, hlsl, asm, iff]

# Dependency graph
requires:
  - phase: 11-d3d11-plugin
    provides: "compilePixelShaderFromHlsl helper (unused for assets until Plan 02 wires it); D3DReflect VS precedent; SRV slots 0..7 bound; PerMaterialCB struct + updatePS upload"
  - phase: 12-visual-baseline
    provides: "matched-pair COMPARISON dir + naming convention (docs/research/phase12-baseline/COMPARISON.md) — mirrored by docs/research/phase17-char-select/COMPARISON.md (D-05)"
provides:
  - "PSRC source retention on ShaderImplementationPassPixelShaderProgram (m_psrcText/m_psrcLen) — Plan 02 reads this to recompile //hlsl PSRC into a real D3D11 PS"
  - "Shared ConfigClientGraphics psrcCensus flag (KEPT, not throwaway — Phase 20 re-runs the census on an open-world boot per D-03)"
  - "Flag-gated PSRC language census emission to BOTH DEBUG_REPORT_LOG_PRINT AND stage/psrc-census.tsv (HIGH-5 file sink, captures under explorer-launched boots)"
  - "char-select COMPARISON dir + naming convention scaffold (docs/research/phase17-char-select/COMPARISON.md) — D-05"
  - "Plan-01-only HEAD sha recorded for census-boot auditability (R3-01d)"
affects: [17-02-psrc-recompile-char-select, 17-03-reflection-driven-constants, 20-world-extension, char-select-validation]

# Tech tracking
tech-stack:
  added: []  # zero new deps; all in-tree
  patterns:
    - "PSRC retain ownership shape mirrors VS m_text (new char[len+1] via Iff::read_string(void), null-terminated; delete[] in dtor + reload with LOW-3 re-null AFTER delete)"
    - "Shared ConfigClientGraphics KEY_BOOL flag gating any diagnostic that must NOT touch the D3D9 path when off"
    - "File-sink mirror of Direct3d11_VertexShaderData.cpp:546-575 (fopen_s + fwrite + fclose) — one-shot diagnostic dump per record"
    - "R3-01c first-non-whitespace HLSL classification (skip BOM + whitespace, then literal lowercase-first-7 == \"//hlsl \" — matches ShaderBuilder PixelShaderProgramView.cpp:301-308)"
    - "R3-01a TSV header bytes via sizeof(literal) - 1 — never hand-counted (eliminates NUL-write risk)"

key-files:
  created:
    - "docs/research/phase17-char-select/COMPARISON.md (D-05 char-select COMPARISON scaffold; aggregation point for stage/psrc-census.tsv per HIGH-5; records the Plan-01-only HEAD sha per R3-01d)"
  modified:
    - "src/engine/client/library/clientGraphics/src/shared/ShaderImplementation.h (added m_psrcText/m_psrcLen public members on ShaderImplementationPassPixelShaderProgram)"
    - "src/engine/client/library/clientGraphics/src/shared/ShaderImplementation.cpp (ctor init list + dtor free + reload free/re-null + load_0000 retain-with-cap + flag-gated census log+file emission with R3-01c classification + R3-01a header bytes)"
    - "src/engine/client/library/clientGraphics/src/shared/ConfigClientGraphics.h (added getPsrcCensus() declaration)"
    - "src/engine/client/library/clientGraphics/src/shared/ConfigClientGraphics.cpp (added ms_psrcCensus member, KEY_BOOL(psrcCensus, false) registration on [ClientGraphics], getter definition)"

key-decisions:
  - "Retain happens UNCONDITIONALLY (so Plan 02's recompile lane always has the text); only census EMISSION is flag-gated. Default false so D3D9 path is behaviorally unchanged when off."
  - "Census log sink (DEBUG_REPORT_LOG_PRINT) AND file sink (stage/psrc-census.tsv) — file sink is the gating artifact because explorer-launched boots cannot see the debug print sink (HIGH-5)."
  - "Sanity cap on PSRC chunk length = 1 MiB (LOW-3 / T-17-01 / ASVS V5); over-cap warns + skips retain (no allocation)."
  - "m_psrcLen stores the IFF chunk-bytes-consumed length (INCLUDES the trailing NUL — Iff.cpp:1606); downstream consumers MUST use strlen(m_psrcText) for the source-text length (R3-01b)."
  - "Classification skips UTF-8 BOM + leading whitespace BEFORE the lowercase-first-7 == \"//hlsl \" compare (R3-01c); mirrors ShaderBuilder's first-line semantics."
  - "TSV header byte count derived from sizeof(literal) - 1, never hand-counted (R3-01a) — eliminates NUL-write risk."

patterns-established:
  - "Pattern: PSRC retain ownership shape on PS-program struct (mirrors VS m_text/m_textLength; new char[len+1] via Iff::read_string; delete[] in dtor + reload; re-null AFTER delete on reload to guard against dangling pointer)"
  - "Pattern: shared-clientGraphics diagnostic gating via ConfigClientGraphics::KEY_BOOL on the [ClientGraphics] section — NOT the plugin's [Direct3d11]/ConfigDirect3d11 (clientGraphics cannot link the plugin)"
  - "Pattern: dual-sink diagnostic emission (log + file) for any artifact that must survive an explorer-launched boot"

requirements-completed: []  # plan ENABLES CHAR-01/02/03 (Plan 02 satisfies CHAR-01; Plan 03 satisfies CHAR-02/03) but SATISFIES none directly — see plan's enables: [CHAR-01, CHAR-02, CHAR-03]

# Metrics
duration: 31min
completed: 2026-05-28
---

# Phase 17 Plan 01: PSRC Census + Char-Select Beachhead — PARTIAL SUMMARY (Tasks 1+2 committed; Task 3 at human-verify checkpoint)

**Retained the discarded TAG_PSRC source text on ShaderImplementationPassPixelShaderProgram and added a flag-gated PSRC language census at the load site emitting to BOTH DEBUG_REPORT_LOG_PRINT AND stage/psrc-census.tsv — Plan 02's recompile lane now has the source text, and the gating HLSL:asm ratio artifact is captured even under explorer-launched boots. Char-select COMPARISON scaffold + naming convention established. Census boot itself is the Task 3 human-verify checkpoint Kenny must perform to populate the artifact.**

## Performance

- **Duration:** ~31 min (Task 1 + Task 2 + COMPARISON scaffold; Task 3 checkpoint pending Kenny)
- **Started:** 2026-05-28T16:45:54Z
- **Completed (Tasks 1+2 + scaffold):** 2026-05-28T17:17:52Z
- **Tasks committed:** 2 of 3 (Task 3 is a `checkpoint:human-verify` — Kenny owns the boot)
- **Files modified:** 4 (plus 1 new scaffold doc)

## Accomplishments

- **m_psrcText / m_psrcLen retain members** added to `ShaderImplementationPassPixelShaderProgram` (public, mirroring the VS `m_text` / `m_textLength` ownership shape) — Plan 02 reads these directly.
- **psrcCensus shared config flag** registered on the `[ClientGraphics]` section via `KEY_BOOL` with a `getPsrcCensus()` getter; default false so the D3D9 path is behaviorally untouched when off. KEPT (not throwaway) per D-03 — Phase 20 re-runs the census on an open-world boot.
- **`load_0000` PSRC retain** via `Iff::read_string(void)` (DIV-1 pin; allocates `new char[len+1]`, returns null-terminated buffer the caller owns). Guarded by:
  - **Empty-PSRC** path (LOW-4) — leaves `m_psrcText = nullptr`, census skip.
  - **1 MiB sanity cap** (LOW-3 / T-17-01 / ASVS V5) — over-cap warns + skips retain (no allocation).
- **Dtor + reload ownership** — `delete[] m_psrcText` in both; reload re-nulls `m_psrcText`/`m_psrcLen` AFTER `delete[]` so a failed mid-reload cannot leave a dangling pointer.
- **Flag-gated census emission** at `load_0000` end:
  - **R3-01c classification:** skip UTF-8 BOM (`EF BB BF`) + leading whitespace BEFORE the lowercase-first-7 `== "//hlsl "` compare (matches ShaderBuilder `PixelShaderProgramView.cpp:301-308` first-line semantics — a BOM/newline-prefixed HLSL PSRC must NOT misclassify as asm).
  - **Profile token scan** from offset + 7 forward (cap 31 chars + NUL).
  - **strstr counts** for `#include`, `register(s`, `register(c`.
  - **Log sink:** `DEBUG_REPORT_LOG_PRINT` (visible under console launch).
  - **File sink (HIGH-5):** per-record `fopen_s` + `fwrite` + `fclose` to `stage/psrc-census.tsv`. Header row written on first emit (probe via `fopen rb`). **R3-01a:** header byte count derived from `sizeof(literal) - 1`, never hand-counted.
- **TAG_PEXE block at :2960-2964 byte-for-byte unchanged** (D-06 D3D9 parity invariant).
- **Char-select COMPARISON scaffold** created at `docs/research/phase17-char-select/COMPARISON.md` with the `{anchor}_{renderer}_{shot}.jpg` naming convention mirroring `docs/research/phase12-baseline/COMPARISON.md`, the three anchor categories (`sul_eye`, `sul_head`, `single_stage`), and the aggregation slots ready to be populated by Kenny's census boot.
- **Plan-01-only HEAD sha recorded** (R3-01d): `5eea334742f9e070ed7364cead452cbfac9df866` — to be referenced when Kenny runs the census boot so the artifact is auditable as having been captured against a Plan-01-only binary (not a Plan-02-recompile-on-top binary).

## Task Commits

1. **Task 1: add PSRC-retain member + psrcCensus shared config flag** — `3667b3b06` (feat)
2. **Task 2: retain PSRC text + flag-gated census at load_0000 (log + file sinks)** — `5eea33474` (feat)
3. **Task 3: census boot + COMPARISON aggregation** — **PENDING** (checkpoint:human-verify; Kenny boots, populates COMPARISON.md from `stage/psrc-census.tsv`)

**Plan metadata commit:** _(not yet — SUMMARY + COMPARISON scaffold commit being made now; final metadata commit pending checkpoint resume)_

## Files Created/Modified

### Created
- `docs/research/phase17-char-select/COMPARISON.md` — D-05 matched-pair scaffold + HLSL:asm ratio aggregation table + per-shader census table slot + deferred-feed flags slot + the Plan-01-only HEAD sha for R3-01d auditability.

### Modified
- `src/engine/client/library/clientGraphics/src/shared/ShaderImplementation.h` — added public `m_psrcText`/`m_psrcLen` members on `ShaderImplementationPassPixelShaderProgram` (PS struct at :626; new members at :689-690).
- `src/engine/client/library/clientGraphics/src/shared/ShaderImplementation.cpp`:
  - **Ctor init list** (at :2751-2757): `m_psrcText(nullptr)` + `m_psrcLen(0)` added after `m_exe(NULL)`.
  - **Dtor** (at ~:2789): `delete[] m_psrcText` + re-null added alongside `delete[] m_exe`.
  - **Reload** (at ~:2840): `delete[] m_psrcText` added; `m_psrcText = nullptr` + `m_psrcLen = 0` set AFTER `delete[]` (LOW-3 guard).
  - **`load_0000`** (at ~:2899-2986 + ~:2971-3084): PSRC chunk retain with 1 MiB cap + empty-chunk skip; flag-gated census emission (log + TSV file) with R3-01c classification and R3-01a TSV header.
  - **Includes added:** `<cstdio>` (fopen_s/fwrite/fclose/_snprintf_s), `<cstring>` (strstr/memcmp/strncpy_s), `sharedDebug/Report.h` (DEBUG_REPORT_LOG_PRINT — already reachable transitively via existing includes; made explicit for clarity).
- `src/engine/client/library/clientGraphics/src/shared/ConfigClientGraphics.h` — added `static bool getPsrcCensus()` declaration.
- `src/engine/client/library/clientGraphics/src/shared/ConfigClientGraphics.cpp` — added `bool ms_psrcCensus` member, `KEY_BOOL(psrcCensus, false)` registration in `install()`, `getPsrcCensus()` getter.

## Decisions Made

None — plan executed exactly as written. All Round 3 amendments (R3-01a/b/c/d) folded in per the planner's specification. All LOW-3/LOW-4 guards (empty-PSRC, 1 MiB cap, re-null-after-delete) implemented as planned.

## Deviations from Plan

None — plan executed exactly as written for Tasks 1 + 2.

**Build environment note (NOT a deviation, just a recorded resolution):** The repo's `PlatformToolset=v145` requires VS18 BuildTools at `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe`, NOT the default VS2022 Community at `C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe` (the latter only ships v150/v160/v170). First build attempt with the VS2022 MSBuild produced `MSB8020: build tools for v145 cannot be found`. Switching to the VS18 BuildTools path resolved cleanly. This is a Koogie-MSBuild-tree environment fact, not a project deviation.

## Issues Encountered

**1. Initial PowerShell build script logger race.** The first attempt at the Task 1 build used `Tee-Object` to capture console output to the same file path as MSBuild's `/fl` file logger; this raced on the file handle and one of the parallel `/m` MSBuild nodes returned `MSB4104: Failed to write to log file`. Resolved by separating the console redirect (via `> ... 2>&1` from the shell) from the file logger path. The actual compile was unaffected — only the log was lost. Re-run with a single logger destination per file produced clean output for both Tasks 1 + 2.

## Pre-existing warnings out of scope (not introduced by this plan)

Four `warning C4840: non-portable use of class 'PersistentCrcString' as an argument to a variadic function` entries in `ShaderImplementation.cpp` at lines that resolved to baseline `:2145`, `:2277`, `:2780`, `:2829` (now shifted by Task 1+2 insertions to `:2145`, `:2277`, `:2785`, `:2837`) — these are existing `DEBUG_FATAL` invocations passing `m_fileName` (the `PersistentCrcString` object) instead of `m_fileName.getString()` to the variadic `%s` formatter. They predate Plan 17-01 and were NOT introduced or amplified by this commit. Captured here only so reviewers don't mis-attribute them to the PSRC work.

## User Setup Required

**Task 3 is a `checkpoint:human-verify` — Kenny boots the client. There is no automated path through the boot itself.**

### Kenny's checklist for the Task 3 census boot

**Pre-boot prep (Claude has already done the source-side work; the host stage tree is Kenny's responsibility):**

1. **Verify Plan-01-only HEAD** (R3-01d): on the build tree used to produce the host-stage `SwgClient_d.exe`, confirm `git rev-parse HEAD == 5eea334742f9e070ed7364cead452cbfac9df866` (the Task 2 commit). If at a later HEAD (e.g. Plan 02 has shipped), check out this SHA, rebuild, deploy `SwgClient_d.exe` to host `stage/`.
2. **Edit host `D:/Code/swg-client-v2/stage/client_d.cfg`** — under the existing `[ClientGraphics]` section (line 34), add:
   ```ini
   psrcCensus=true
   ```
   Keep `rasterMajor=11` (already set on line 37). **Debug exe reads `client_d.cfg`, NOT `client.cfg`** (memory feedback_debug_exe_reads_client_d_cfg) — edit the right one.
3. **Delete any pre-existing `stage/psrc-census.tsv`** so the header row is written fresh on first emit.
4. **Boot `SwgClient_d.exe`** to char-select (login flow → character select screen).
5. **Capture matched-pair screenshots** at char-select default pose:
   - `sul_eye_d3d11_NNNN.jpg`
   - `sul_head_d3d11_NNNN.jpg` (whichever `sul_*_head.sht` the census shows loaded)
   - `single_stage_d3d11_NNNN.jpg` (a single-stage body/clothing material the census shows loaded — Kenny picks one)
   - Then toggle `rasterMajor=5` and re-boot for the `*_d3d9_NNNN.jpg` ground-truth pair.
   Save into `docs/research/phase17-char-select/`.

**Post-boot aggregation (after Kenny resumes):**

6. Continuation agent reads `stage/psrc-census.tsv`, aggregates the HLSL:asm ratio, populates the per-shader table + lane-mix decision in `docs/research/phase17-char-select/COMPARISON.md`, records the Plan-01-only HEAD sha + the named anchors + whether asm appears + whether any deferred constant feed (scroll/fog/stencil) appears (gates Plan 03 scope).
7. **Reset `psrcCensus=false`** in `stage/client_d.cfg` (instrumentation kept in code, off by default — D-03).
8. Continuation agent finalizes this SUMMARY's Task 3 section and the COMPARISON.md Findings, then commits the metadata.

## Checkpoint state (for orchestrator)

**Type:** `checkpoint:human-verify` (Task 3)
**Plan:** 17-01
**Progress:** 2 of 3 tasks committed; 1 pending (the human-verify boot)

The orchestrator surfaces this checkpoint to Kenny. He boots the binary, populates `stage/psrc-census.tsv` + screenshot pairs, then resumes. A fresh continuation agent will read the TSV, finalize this SUMMARY's Task 3 section + the COMPARISON.md Findings, and ship the final metadata commit.

## Self-Check: PASSED

- [x] `src/engine/client/library/clientGraphics/src/shared/ShaderImplementation.h` exists and contains `m_psrcText` (grep verified line 689).
- [x] `src/engine/client/library/clientGraphics/src/shared/ShaderImplementation.cpp` exists and contains `iff.read_string()`, `m_psrcText(nullptr)`, `delete [] m_psrcText` ×2, `m_psrcText = nullptr`, `getPsrcCensus()` guard, `"//hlsl "`, `0xEF` BOM byte, `hlslStart` (9 occurrences), `psrc-census.tsv` (3 occurrences), `sizeof(header) - 1`, and 0 hand-counted `fwrite(..., 61, fp)` patterns.
- [x] `src/engine/client/library/clientGraphics/src/shared/ConfigClientGraphics.cpp` exists and contains `ms_psrcCensus`, `KEY_BOOL(psrcCensus`, `getPsrcCensus`.
- [x] `src/engine/client/library/clientGraphics/src/shared/ConfigClientGraphics.h` exists and contains `getPsrcCensus`.
- [x] `docs/research/phase17-char-select/COMPARISON.md` exists with the anchor table, naming convention, Plan-01-only HEAD sha pin (R3-01d), and aggregation slots ready to populate post-boot.
- [x] Commit `3667b3b06` exists in `git log --oneline --all`.
- [x] Commit `5eea33474` exists in `git log --oneline --all`.
- [x] Debug build link: 0 `unresolved external symbol` (grep `stage/build-task2.msbuild.log`).
- [x] Debug build: 0 `error C[0-9]+|error LNK[0-9]+|error MSB[0-9]+`.
- [x] `SwgClient_d.exe` (70 MB) produced in `<worktree>/stage/`.
- [ ] **PENDING (Task 3 checkpoint):** `stage/psrc-census.tsv` populated by Kenny's census boot; aggregated HLSL:asm ratio recorded in COMPARISON.md; matched-pair screenshots captured under both `rasterMajor=5` and `=11`; D-06 dual-renderer boot gate confirmed.

---
*Phase: 17-psrc-census-char-select-beachhead*
*Plan: 01 (Tasks 1+2 committed 2026-05-28; Task 3 awaiting human-verify checkpoint)*
