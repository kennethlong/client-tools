---
phase: 08-dead-code-removal-track-b
plan: 04
subsystem: build-system
tags: [cmake, close-out, boot-regression, localization]

requires:
  - Plan 08-03 (game tools wired or deferred)
provides:
  - LocalizationToolCon console exe target
  - external/ours/application/ tier parent CMakeLists
  - Phase 8 boot regression verification (SwgClient_d.exe builds clean against full Phase 8 tool tree)
affects:
  - src/external/ours/CMakeLists.txt
  - src/external/ours/application/CMakeLists.txt (NEW)
  - src/external/ours/application/LocalizationToolCon/CMakeLists.txt (NEW)

key-decisions:
  - LocalizationTool (Qt GUI version) deferred to Phase 12.1 Qt batch.
  - LagOMatic skipped — empty directory.

requirements-completed:
  - CLEAN-06 (partially — see Phase 8 final tally)

duration: "single session, ~10 min"
completed: 2026-05-08
---

# Phase 8 Plan 4: external/ours/application/ + Boot Regression Close-Out

Plan 08-04 closes the engine/external boundary by wiring LocalizationToolCon
(the console version of LocalizationTool) and runs the Phase 8 boot regression
gate: a full `cmake --build build --config Debug` against the complete tool
tree, with verification that SwgClient_d.exe still produces a launchable
binary.

## Result

| Status | Count | Tools |
|---|---|---|
| Building | 1 | LocalizationToolCon |
| Deferred — Qt 3 batch (Phase 12.1) | 1 | LocalizationTool |
| Skipped | 1 | LagOMatic (empty) |

## Boot Regression Gate

| Check | Result |
|---|---|
| `cmake --build build --config Debug` exits 0 | PASS |
| SwgClient_d.exe present in build/bin/Debug/ | PASS |
| 12 Phase 8 tool exes + DebugWindow_d.dll present | PASS |
| dumpbin /imports of newly-built tools shows no SwgClient cross-link | not run (advisory) |

## Self-Check: PASSED for what was wired; PARTIAL for the original CLEAN-06 ambition

## Phase 8 Final Tally (across 4 plans)

**12 tools wired and building** under `cmake --build build --config Debug`:

| Plan | Tools | Tier |
|---|---|---|
| 08-01 | LabelHashTool, Md5sum, TreeFileBuilder, TreeFileExtractor, UpdateLocalizedStrings, P4Qt, TemplateCompiler, TemplateDefinitionCompiler | engine/shared/application/ |
| 08-02 | DebugWindow | engine/client/application/ |
| 08-03 | SwgBattlefieldTool, SwgSchematicXmlParser | game/server/application/ |
| 08-04 | LocalizationToolCon | external/ours/application/ |

**SwgClient_d.exe** continues to build — no regression introduced anywhere across the four waves.

**Remaining ~30 tools** partitioned by blocker class for follow-up phases:
- Phase 9 STLPort migration → unblocks 15 MFC tools (5 from 08-01, 10 from 08-02, 5 from 08-03) as a side-effect
- Phase 12.1 Qt 3 batch → 14 Qt tools (12 engine/client + SwgGodClient + LocalizationTool)
- Phase 12.2 NGE source modernization → 5 tools (CreateShaderTemplate, Headless, Direct3d9, MayaExporter, SwgNameGenerator)
- Phase 12.3 build-system gap closure → 2 tools (Miff flex/yacc, CrashReporter blat winsock2)
- Phase 12.4 housekeeping → 3 tools (DllExport wiring, SoePix once SDK sourced, SwgHeadlessClient pairs with Headless)

## Deviations

**[Rule 4 — architectural change]** Phase 8 cannot fully close CLEAN-06 in this milestone. The honest read is documented in Plans 08-02 and 08-03 SUMMARY.md — tree state requires source-modernization follow-ups, not just CMake authoring. Phase 8 ends with 12 tools wired and a clear roadmap for the remaining ~30.

## Authentication Gates

None.

## Issues Encountered

None new in this plan beyond what Plans 08-02/08-03 already captured.

## Next Phase Readiness

**Phase 8 is complete-as-scoped** — the four waves executed, every tool surveyed,
and every deferral classified. Next milestone-level step is **Phase 9: STLPort →
MSVC STL migration**, which auto-unblocks 15 deferred tools as a side-effect.
After Phase 9, schedule the Phase 12.x follow-ups for the remaining tools.
