---
phase: 08-dead-code-removal-track-b
plan: 03
subsystem: build-system
tags: [cmake, partial, deferred, game-tools, mfc]

requires:
  - Plan 08-01 (FindQt3.cmake, FindMaya.cmake)
provides:
  - SwgBattlefieldTool console exe target
  - SwgSchematicXmlParser console exe target (TinyXML)
  - game/server/ + game/server/application/ tier parent CMakeLists
  - All 14 game tools classified by blocker for downstream re-enablement
affects:
  - src/game/CMakeLists.txt
  - src/game/client/application/CMakeLists.txt
  - src/game/server/CMakeLists.txt (NEW)
  - src/game/server/application/CMakeLists.txt (NEW)
  - src/game/server/application/SwgBattlefieldTool/CMakeLists.txt (NEW)
  - src/game/server/application/SwgSchematicXmlParser/CMakeLists.txt (NEW)

key-decisions:
  - SwgGodClient (Qt, 58 cpps + 16 .ui + 39 Q_OBJECT) deferred to Phase 12.1 Qt batch.
  - SwgHeadlessClient deferred — essentially a clone of SwgClient with a different graphics plugin; pair with the deferred Headless plugin DLL in Phase 12.x.
  - SwgNameGenerator deferred — engine API NameGenerator constructor signature changed; needs source patch.
  - 5 MFC editors (SwgConversationEditor et al.) deferred to Phase 9 (STLPort migration).

requirements-completed: []
requirements-deferred:
  - CLEAN-06 — partial; remaining game tools require source-modernization or Phase 9.

duration: "single session, ~15 min triage + per-tool diagnostics"
completed: 2026-05-08
---

# Phase 8 Plan 3: game/ Tool Survey Summary

Plan 08-03 surveyed `game/client/application/` and `game/server/application/`
(15 directories total). Two tools build cleanly; the rest follow the same
NGE-era source/API mismatch + MFC ↔ STLPort patterns documented in Plan 08-02.

## Result

| Status | Count | Tools |
|---|---|---|
| Building | 2 | SwgBattlefieldTool, SwgSchematicXmlParser |
| Deferred — NGE source/API mismatch | 2 | SwgNameGenerator, SwgHeadlessClient |
| Deferred — MFC ↔ STLPort (Phase 9) | 5 | SwgContentBuilder, SwgConversationEditor, SwgDraftSchematicEditor, SwgSpaceQuestEditor, SwgSpaceZoneEditor |
| Deferred — Qt 3 batch (Phase 12.1) | 1 | SwgGodClient |
| Skipped per Plan 08-03 D-15 | 5 | SwgGameServer (JNI), SwgDatabaseServer (Oracle), PhonyApp (Linux), LaunchMeFirst (no source), SwgCsTool (C#) |

## Self-Check: PARTIAL — 2/9 named buildable tools

SwgClient_d.exe still builds — no regression.

## Deviations

**[Rule 4]** Same architectural finding as Plan 08-02: tool source code mid-NGE-refactor; CMake authoring alone insufficient. Continued the deferral pattern from 08-02 in the tier parent.

## Next Phase Readiness

Wave 4 (Plan 08-04) can proceed to close out the phase. Per-category follow-ups (Phase 9, Phase 12.x) capture the remaining work.
