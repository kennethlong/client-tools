---
phase: 31-64-bit-correctness-foundation
plan: 09
subsystem: infra
tags: [x64, llp64, bits-02, bits-03, width-reconciliation, size_t, ULONG_PTR, convergence-cap, winsock]

# Dependency graph
requires:
  - phase: 31-01
    provides: the scratch Debug|x64 compile-only harness (compile-all.ps1 / x64-compile.props / in-scope-tus.txt)
  - phase: 31-05
    provides: the uint32_t wire-stable width-reconciliation pattern (Archive count put/get)
  - phase: 31-08
    provides: the AutoDelta* cascade clearance that UNMASKED this residual class-(B) surface (DEF-31-08-UNMASKED)
provides:
  - "4 genuine class-(B) width members fixed x64-clean (CuiCombatManager getFirstToken size_t cursor; MeshConstructionHelper 5x VALIDATE_RANGE size_t literal; TcpClient/TcpServer IOCP completionKey ULONG_PTR)"
  - "evidence-based reclassification of the sharedTemplateDefinition Unicode cluster as pre-broken-tool-tier residue (link closure proof)"
  - "confirmation + exclusion of the two harness-only non-defects (Direct3d9 #error FFP/VSPS; 5x winsock C2371 header-order)"
  - "the CAPPED -Scope all convergence verdict: Phase-31 class-(B) source work COMPLETE (0 NEW class-(B); 31-06 Task 2/3 can resume; NO 31-10)"
affects: [31-06, 33-x64-build-platform, 35-miles-audio]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Width reconciliation touches ONLY local length/count/cursor args (parse cursor size_t, IOCP key ULONG_PTR) — never an on-the-wire/serialized field"
    - "Link-closure triage (ProjectReference + AdditionalDependencies .lib audit) as the decision criterion for runtime-boot-path vs pre-broken-tool-tier scope"
    - "HARD-CAP convergence: exhaustive per-failing-TU classification (class-(A) residue / confirmed harness artifact / NEW class-(B)); a NEW layer relocates to Phase 33, not a new increment"

key-files:
  created: []
  modified:
    - src/engine/client/library/clientUserInterface/src/shared/core/CuiCombatManager.cpp
    - src/engine/client/library/clientSkeletalAnimation/src/shared/appearance/MeshConstructionHelper.cpp
    - src/engine/shared/library/sharedNetwork/src/win32/TcpClient.cpp
    - src/engine/shared/library/sharedNetwork/src/win32/TcpServer.cpp
    - .planning/phases/31-64-bit-correctness-foundation/deferred-items.md

key-decisions:
  - "CuiCombatManager::fillSpamOrder: widen pos unsigned int -> size_t (parse cursor, not serialized) so it binds getFirstToken's size_t& endpos on x64 — minimal-churn over an unsigned-int overload"
  - "MeshConstructionHelper: change the VALIDATE_RANGE_INCLUSIVE_EXCLUSIVE literal 0U -> static_cast<size_t>(0) so the single-type template deduces against the size_t index/.size() args (DEBUG range-check macro only)"
  - "TcpClient/TcpServer: IOCP completionKey unsigned long int -> ULONG_PTR for GetQueuedCompletionStatus's PULONG_PTR arg — local IOCP key, write-only, never on the wire"
  - "sharedTemplateDefinition Unicode cluster: RECLASSIFIED tool-only out-of-scope by link evidence (sharedTemplateDefinition.lib links ONLY into ShipComponentEditor/TemplateCompiler/TemplateDefinitionCompiler, never SwgClient) — files left UNEDITED, NOT force-fixed"
  - "Direct3d9 #error + 5x winsock C2371: CONFIRMED harness-only non-defects, EXCLUDED with proof (real vcxprojs define FFP/VSPS; WindowsWrapper.h WIN32_LEAN_AND_MEAN suppresses winsock)"
  - "HARD CAP honored: 0 NEW class-(B) unmasked -> Phase-31 class-(B) source COMPLETE, NO 31-10 authored or recommended"

patterns-established:
  - "Local-only width reconciliation (size_t/ULONG_PTR) for x64 overload-resolution failures, asserting no serialized field changes"
  - "Link-closure evidence (ProjectReference / .lib AdditionalDependencies) as the authoritative runtime-vs-tool scope gate per AGENTS.md editor/tool tier"

requirements-completed: [BITS-02, BITS-03]

# Metrics
duration: 95min
completed: 2026-06-16
---

# Phase 31 Plan 09: Capped Convergence Gap-Closure Increment Summary

**4 genuine class-(B) x64 width members fixed (size_t parse cursor + size_t range-check literal + ULONG_PTR IOCP key), the sharedTemplateDefinition Unicode cluster reclassified tool-only by link evidence, both harness-only non-defects confirmed + excluded, and the CAPPED -Scope all convergence test declares Phase-31 class-(B) source work COMPLETE (0 NEW class-(B); 55->51 failing TUs; NO 31-10).**

## Performance

- **Duration:** ~95 min (dominated by the 2218-TU -Scope all sweep)
- **Started:** 2026-06-16T04:50:00Z (approx)
- **Completed:** 2026-06-16T00:40:00Z (system clock; sweep-bound)
- **Tasks:** 3
- **Files modified:** 5 (4 source + deferred-items.md)

## Accomplishments

- **Task 1 — 4 genuine class-(B) width members fixed x64-clean** (each TU exit 0, 0 C2665/C2664/C2672 in the scratch harness):
  - `CuiCombatManager::fillSpamOrder` — `pos` `unsigned int` -> `size_t` so the `getFirstToken(str, pos, size_t& endpos, ...)` call binds on x64 (an `unsigned int&` can't bind a `size_t&`). `pos` is a parse cursor compared against `npos`, never serialized.
  - `MeshConstructionHelper` — 5x `VALIDATE_RANGE_INCLUSIVE_EXCLUSIVE(0U, <size_t idx>, <size_t .size()>)` -> `static_cast<size_t>(0)`. The macro is a single-type template (`template <class T>`, all args `const T&`); the `0U` (unsigned int) against `size_t` args made the type undeducible (C2672). DEBUG-only range check (NOP in release).
  - `TcpClient` / `TcpServer` — IOCP `completionKey` `unsigned long int` -> `ULONG_PTR` for `GetQueuedCompletionStatus`'s `PULONG_PTR` arg (C2664). Local, write-only key; not on the wire.
- **Task 2 — Unicode cluster reclassified tool-only (exactly one branch, evidence-based):** the link-closure triage proved `sharedTemplateDefinition.lib` (the only thing that compiles Filename/TemplateData/TpfFile) is ProjectReferenced ONLY by the pre-broken `ShipComponentEditor` / `TemplateCompiler` / `TemplateDefinitionCompiler` tools; `SwgClient.vcxproj` has 0 references and the lib is absent from its `AdditionalDependencies` closure (so is `sharedTemplate.lib`). Files left UNEDITED; removed from effective scope (DEF-31-09-UNICODE-TOOLONLY).
- **Task 3 — harness non-defects confirmed + CAPPED convergence:**
  - Direct3d9 `#error must define FFP, VSPS, or both` confirmed a scratch-config artifact (real `Direct3d9.vcxproj`=`FFP;VSPS`, `_ffp`=`FFP`, `_vsps`=`VSPS`); excluded, no edit.
  - 5x winsock `C2371 'SOCKET' redefinition` confirmed an include-order artifact: a minimal probe with `WIN32_LEAN_AND_MEAN` (exactly as `WindowsWrapper.h` sets before `#include <windows.h>`) compiles EXIT 0 + `WINDOWS_H_DID_NOT_PULL_WINSOCK` on this SDK; sharedNetwork is in the shipped bootable 32-bit client. Excluded, Phase-33 re-check.
  - `-Scope all` (2218 TUs): **55 -> 51 failing TUs**; exhaustive classification = 41 class-(A) residue + 7 harness artifacts + 3 reclassified tool-only Unicode + **0 NEW class-(B)**.

## Task Commits

1. **Task 1: genuine class-(B) width members** - `feaddc08e` (fix)
2. **Task 2: reclassify Unicode cluster tool-only** - `81b19c345` (docs)
3. **Task 3: harness artifacts + convergence verdict** - `79f5bc84a` (docs)

**Plan metadata:** (final docs commit — STATE.md/ROADMAP.md/SUMMARY.md)

## Files Created/Modified

- `src/engine/.../clientUserInterface/.../CuiCombatManager.cpp` - `pos` size_t parse cursor (getFirstToken bind)
- `src/engine/.../clientSkeletalAnimation/.../MeshConstructionHelper.cpp` - 5x size_t range-check literal
- `src/engine/.../sharedNetwork/.../TcpClient.cpp` - IOCP completionKey ULONG_PTR
- `src/engine/.../sharedNetwork/.../TcpServer.cpp` - IOCP completionKey ULONG_PTR
- `.planning/.../deferred-items.md` - DEF-31-09-UNICODE-TOOLONLY + DEF-31-09-HARNESS-ARTIFACTS + DEF-31-09-CONVERGENCE
- `.planning/research/scope-all-31-09-final.out` - the authoritative -Scope all snapshot (untracked research artifact, matching the 31-07/31-08 pattern)

## Decisions Made

See `key-decisions` frontmatter. The load-bearing one: the HARD CAP held — category (iii) NEW class-(B) is empty, so Phase-31 class-(B) source work is COMPLETE and NO 31-10 is authored or recommended.

## Triage Verdict (Task 2 — required record)

**TOOL-ONLY -> reclassified out-of-scope.** Link evidence:
- `sharedTemplateDefinition.vcxproj` is ProjectReferenced (link dep) by EXACTLY 3 targets, all pre-broken tools: `ShipComponentEditor.vcxproj`, `TemplateCompiler.vcxproj`, `TemplateDefinitionCompiler.vcxproj`.
- `SwgClient.vcxproj`: 0 ProjectReferences; `sharedTemplateDefinition.lib` NOT in `<AdditionalDependencies>` (neither is `sharedTemplate.lib`). `clientGame.vcxproj`: 0 references.
- `sharedTemplate.vcxproj` references it only as an `AdditionalIncludeDirectories` header path (not a link dep), and `sharedTemplate.lib` is itself not in SwgClient's link closure — no transitive runtime link.

Per AGENTS.md ("Editor tools ... pre-broken ... Validation bar = /t:SwgClient clean + dual-renderer boot"), the char16_t/wchar_t cluster is tool-tier residue, NOT a runtime boot-path class-(B) defect. Files UNEDITED.

## Before/After + Per-Failure Classification (Task 3 — required record)

**Failing-TU count: 31-08 baseline = 55 -> 31-09 = 51** (the 4 Task-1 fixes cleared). The C42xx worklist filter is byte-identical (75 C4244 + 4 C4311 + 4 C4312, 0 C4235); the Task-1 fixes were C2665/C2664/C2672 (overload resolution), which that filter never showed.

| Category | Count | Detail |
|----------|-------|--------|
| (i) class-(A) residue — UNTOUCHED | 41 | 38 C4244-only (`__int64`/`size_t`->narrower D-07/N2 count/distance + STL `_Elem`/`_Ty` noise), Bink `BinkVideo.cpp`+`BinkTreeFileIO.cpp` (P33), `WaterTestAppearance.cpp` (P33), Miles `Audio.cpp` (P35) |
| (ii) confirmed harness artifact — EXCLUDED | 7 | 5x winsock C2371 (`UdpSock`/`NetworkHandler`/`Service`/`Sock`/`Connection`); 2x Direct3d9 `#error` (`Direct3d9.cpp` C1189 + `Direct3d9_StaticShaderData.cpp` C4716) |
| (reclassified) tool-only Unicode | 3 | `Filename.cpp`/`TemplateData.cpp`/`TpfFile.cpp` |
| **(iii) NEW genuine class-(B)** | **0** | — |

**HARD CAP RESULT:** category (iii) empty -> **Phase-31 class-(B) source work COMPLETE**, D-02 x64-clean bar met for all in-scope class-(B) source, **31-06 Task 2/3 can resume**, **NO 31-10**.

## Deviations from Plan

None - plan executed exactly as written. All three tasks resolved on their primary branch (Task 1 fixed all 4 members; Task 2 took the tool-only reclassification branch with evidence; Task 3 confirmed both harness artifacts and the cap held). No deviation rules triggered.

## Wire-Safety Assertion

No on-the-wire / serialized field width was changed. Task-1 touched only: a parse cursor (`size_t pos`), a DEBUG range-check literal (`static_cast<size_t>(0)`), and a local IOCP completion key (`ULONG_PTR completionKey`, write-only). The serialization-sensitive AutoDelta*/Archive count paths (31-05/31-08) and the SAFE fixed-width paths are untouched.

## Issues Encountered

- The `-Scope all` Tee snapshot is UTF-16LE (PowerShell default); the full per-TU detail with exit codes lives in `worklist.log` (UTF-8). Classification was done against `worklist.log` (51 failing TUs with full error signatures); `scope-all-31-09-final.out` holds the filtered C42xx worklist. Both saved.

## Next Phase Readiness

- **31-06 Task 2 (full 32-bit build) + Task 3 (dual-renderer boot smoke) can resume** — the D-02 x64-clean bar is met for all in-scope class-(B) source.
- Carried forward (NOT class-(B), NOT Phase 31): class-(A) C4244 count/distance + STL noise + `WaterTestAppearance` (Phase 33); Bink (Phase 33 X64-04); Miles `Audio.cpp` (Phase 35); the two harness-only artifacts re-verified by the real 5-target build / x64 platform add (Phase 33). The tool-tier Unicode cluster is an editor-revival concern only (out of v3.0 scope).

---
*Phase: 31-64-bit-correctness-foundation*
*Completed: 2026-06-16*
