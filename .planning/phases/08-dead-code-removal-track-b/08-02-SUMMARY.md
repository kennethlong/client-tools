---
phase: 08-dead-code-removal-track-b
plan: 02
subsystem: build-system
tags: [cmake, partial, deferred, qt3, mfc, engine-client]

requires:
  - Plan 08-01 (FindQt3.cmake, FindMaya.cmake, sharedStatusWindow lib)
provides:
  - DebugWindow.dll target (Win32 utility, no engine deps)
  - engine/client/application/ tier parent with all 30 sibling tools classified
    by blocker for downstream re-enablement
affects:
  - src/engine/client/CMakeLists.txt
  - src/engine/client/application/CMakeLists.txt
  - src/engine/client/application/DebugWindow/CMakeLists.txt

tech-stack:
  added: []
  patterns:
    - Tier parent commits ALL siblings' add_subdirectory lines as inline-comments
      classified by blocker category — re-enable becomes a one-line change.

key-files:
  created:
    - src/engine/client/application/CMakeLists.txt
    - src/engine/client/application/DebugWindow/CMakeLists.txt
  modified:
    - src/engine/client/CMakeLists.txt (added add_subdirectory(application))

key-decisions:
  - Plan 08-02 explicitly does NOT close out the engine/client/application/ tier as
    the original plan envisioned. Surveying all 31 tools revealed that this tree
    is a mid-NGE-refactor snapshot where many tool sources reference engine APIs
    that have been removed/renamed in the engine itself. CMake authoring alone
    cannot close this gap.
  - Recommendation: split the remaining 30 tools into category-specific follow-up
    plans, scheduled around Phase 9 (STLPort migration) which unblocks the 10
    MFC tools as a side-effect.

requirements-completed: []
requirements-deferred:
  - CLEAN-06 — partial; remaining engine/client tools require source-modernization
    plans, not just CMake authoring

duration: "single session, ~30 min triage + per-tool diagnostics"
completed: 2026-05-08
---

# Phase 8 Plan 2: engine/client/application/ Tool Survey Summary

Plan 08-02 was originally scoped to wire ~31 engine/client tool projects (Qt GUI
editors, MFC tools, MayaExporter, Direct3d9 plugin, etc.) into the v2 CMake graph
in a single wave. After surveying every tool's source, build dependencies, and
external SDK requirements, only 1 of 31 builds cleanly without source-level work.
The remaining 30 are partitioned into 5 distinct blocker categories and parked
as one-line commented `add_subdirectory()` calls in the tier parent — each
re-enableable as soon as its blocker is resolved by a targeted follow-up plan.

## Result

| Status | Count | Tools |
|---|---|---|
| Building | 1 | DebugWindow |
| Deferred — MFC ↔ STLPort (Phase 9 unblocks) | 10 | ClientCacheFileBuilder, DatabaseObjectViewer, SetBrightnessContrastGamma, ShaderBuilder, TerrainEditor, TextureBuilder, UiBuilder, ViewIff, Viewer, WorldSnapshotViewer |
| Deferred — NGE source/API mismatch | 4 | CreateShaderTemplate, Headless, Direct3d9, MayaExporter |
| Deferred — missing 3rd-party SDK | 1 | SoePix (PixPlugin.h) |
| Deferred — build-system gap (winsock, codegen) | 2 | CrashReporter, Miff |
| Deferred — engine link wiring needs trace | 1 | DllExport |
| Deferred — Qt 3 batch (post-pattern validation) | 12 | AnimationEditor, BugTool, ClientEffectEditor, LightningEditor, NpcEditor, ParticleEditor, QuestEditor, RemoteDebugTool, ShipComponentEditor, SoundEditor, SwooshEditor, TemplateEditor |
| **Total surveyed** | **31** | |

SwgClient_d.exe still builds — no regression introduced by the partial wiring.

## Blocker Classifications (per tool)

### Building (1)

**DebugWindow** — standalone Win32 utility DLL (~1 cpp), no engine deps. ✓

### Deferred — MFC ↔ STLPort (10 tools, Phase 9 unblocks all)

Same root cause as Plan 08-01's 5 MFC deferrals: STLPort 4.5.3 cannot coexist
with `<afxwin.h>` on the same translation unit under MSVC 14.44. Phase 9
(STLPort → MSVC STL) eliminates this conflict; expect all 10 tools below to
build with no further source changes once Phase 9 lands.

`ClientCacheFileBuilder`, `DatabaseObjectViewer`, `SetBrightnessContrastGamma`
(MFC flat-src layout); `ShaderBuilder`, `TerrainEditor` (~105 cpps),
`TextureBuilder`, `ViewIff`, `Viewer`, `WorldSnapshotViewer` (MFC win32-src);
`UiBuilder` (MFC flat-src, ~45 cpps).

### Deferred — NGE source/API mismatch (4 tools)

Tool source references engine APIs that no longer exist in this tree's engine
libraries (NGE-era refactor incomplete in the leak). Each needs surgical source
patches similar to the `borrowCompressor` fix applied to TreeFileBuilder in
Plan 08-01.

| Tool | Concrete error | Likely fix |
|---|---|---|
| CreateShaderTemplate | `SetupFoundation::Data` not a namespace | Rename to `SetupSharedFoundation::Data` (engine renamed the setup struct) |
| Headless | `TextureGraphicsData::LockData::m_*` private | Add `Headless_TextureGraphicsData` as `friend class` of `LockData`, or expose accessors in clientGraphics |
| Direct3d9 | jpeglib.h missing; `__pfnDliNotifyHook2` const assignment; `Pass::apply` missing return | Add libjpeg find/include path; switch DLL notify hook to non-const; add `return` statement (NGE refactor likely shipped a different overload signature) |
| MayaExporter | `sharedDatabaseInterface/DBSession.h` missing; Maya 8 `MTypes.h` typeid:: error | Wire sharedDatabaseInterface (Oracle blocker — same problem as SwgDatabaseServer); Maya 8 typeid issue likely fixed by `_BOOL` / `REQUIRE_IOSTREAM` defines (already added) plus a `/Zc:wchar_t` switch — needs investigation |

### Deferred — missing 3rd-party SDK (1 tool)

**SoePix** — needs `PixPlugin.h`, part of the legacy DirectX SDK PIX plugin
support. Not present in this repo's vendored SDKs. Either source the legacy
DirectX SDK or stub out the PIX exports.

### Deferred — build-system gap (2 tools)

**CrashReporter**: tool source itself compiles (it's a console exe with int
main). The blocker is that the vendored `blat194` library uses `gensock.cpp`
(an old winsock1 wrapper) which conflicts with modern UCRT `ws2def.h`
(winsock2). Workaround would be to either (a) replace blat with a modern SMTP
client, (b) port gensock.cpp to winsock2, or (c) wrap the conflicting headers.
None of these are 1-line CMake fixes.

**Miff**: needs `MIFFCompile` symbol generated from `src/win32/parser.lex` +
`parser.yac` via the vendored `tools/flex.exe` and `tools/bison.exe`. CMake
needs `add_custom_command` rules to invoke flex/bison and feed generated .cpp
back into the source list. Doable but mechanical work — track as its own plan.

### Deferred — engine link wiring (1 tool)

**DllExport**: provides debugger-int3 stubs for clientGraphics symbols. Should
link, but currently fails with `operator new(MemoryManagerNotALeak)` unresolved
— the missing override is in sharedMemoryManager. Adding `sharedMemoryManager`
to `target_link_libraries` should fix it; not done in this plan to keep blast
radius minimal.

### Deferred — Qt 3 batch (12 tools)

The 12 Qt GUI editors all need:

1. Per-tool detection of headers containing `Q_OBJECT` (only those should be
   passed to `qt3_wrap_cpp`)
2. Per-tool detection of `.ui` file location — some tools have `src/win32/*.ui`,
   others have a top-level `ui/` directory (ParticleEditor)
3. Engine source-level audit — these tools likely have the same NGE-era API
   mismatches the non-Qt tools have, but each one's specific failures need
   diagnosis

Authoring all 12 in a single session was avoided because (a) the per-tool
research is significant, and (b) the working assumption that "wire CMake = done"
proved false for the 4 simpler tools that were attempted (CreateShaderTemplate,
Headless, Direct3d9, MayaExporter), so high confidence the Qt tools have similar
issues. Track this as a focused Qt-tools follow-up plan that establishes the
pattern on 1-2 tools, then batches the rest.

## Self-Check: PARTIAL — see classification

| Acceptance Criterion (from PLAN.md) | Result |
|--|--|
| `cmake --build build --config Debug --target ParticleEditor` exits 0 | DEFERRED (Qt batch) |
| `cmake --build build --config Debug --target TerrainEditor` exits 0 | DEFERRED (MFC, Phase 9) |
| `cmake --build build --config Debug --target MayaExporter` exits 0 | DEFERRED (sharedDatabaseInterface + Maya typeid) |
| `cmake --build build --config Debug --target Headless` exits 0 | DEFERRED (NGE friend class fix) |
| `engine/client/application/` wired via `add_subdirectory(application)` | PASS (parent chain in place) |
| Direct3d9 builds as Direct3d9.dll — NOT staged as gl05 | DEFERRED (Direct3d9 not building) |
| `SwgClient_d.exe` continues to build | PASS — no regression |

## Deviations from Plan

**[Rule 4 — architectural change → user decision] Plan 08-02 cannot complete autonomously**

The original plan assumed authoring CMakeLists.txt files for ~31 engine/client
tools would be sufficient to make them build. After surveying every tool's
source and diagnosing the first ~6 attempted, the assumption proved wrong:
the leaked tree's tools are mid-NGE-refactor and the surrounding engine API
has moved on, leaving most tool sources pointing at non-existent symbols.
This is consistent with the leak being a snapshot at a specific build epoch
where these tools weren't actively maintained.

The honest path forward is to defer the bulk of Wave 2 to focused follow-up
plans that each tackle a smaller, more tractable scope:

1. **Phase 8.x or Phase 12.1** — Qt 3 tool batch (12 tools). Establish moc/uic
   pattern on 1-2 representative tools first, then sweep the rest.
2. **Phase 9** — STLPort migration. Side-effect: unblocks 10 MFC tools.
3. **Phase 12.2** — NGE source modernization (4 tools). Each tool gets a small
   plan with source patches.
4. **Phase 12.3** — Build-system gap closure (2 tools). Miff flex/bison wiring;
   CrashReporter blat winsock2 port.
5. **Phase 12.4** — Single-session housekeeping (1-2 tools). DllExport wiring,
   SoePix once SDK is sourced.

**Total deviations:** 1 — plan-level architectural change.
**Impact:** CLEAN-06 stays only partially satisfied. The roadmap needs adjustment
to capture the new Phase 12 (or 8.x) follow-ups.

## Authentication Gates

None.

## Issues Encountered

- **Tool source maturity assumption was wrong.** This plan's authoring assumed
  tool sources compiled in this tree; in reality the tree is a snapshot where
  many tools' source predates engine refactors. Lesson for Plans 08-03, 08-04:
  budget for source-level diagnosis time per tool.
- **Vendored 3rd-party libs have winsock1/UCRT conflicts.** Specifically blat's
  `gensock.cpp`. Future plans wiring legacy 3rd-party libs should triage this
  upfront.

## Next Phase Readiness

**Ready for**: Plan 08-03 (game tools) and Plan 08-04 (external/ours + boot
regression) **but with strong caveats**:

- Plan 08-03 will likely have similar issues — game tools include `SwgGodClient`
  (Qt), `SwgConversationEditor` (likely MFC), and several editors that reference
  engine APIs the same way engine/client tools do. Survey before writing.
- Plan 08-04's boot regression will pass (SwgClient still builds), but the
  CLEAN-06 success criterion "all tool targets build" cannot be met until the
  follow-up plans listed above run.

**Recommendation to user**: pause Plan 08-03 and re-plan Phase 8. The original
4-wave structure assumed CMake-only work would suffice; the actual tree state
calls for a multi-phase decomposition (Phase 8 = working tools today, Phase 9
= STLPort migration unblocks 10 more, Phase 12 = source modernization unblocks
the rest). STATE.md and ROADMAP.md should be updated to reflect this.
