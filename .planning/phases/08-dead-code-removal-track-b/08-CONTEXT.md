# Phase 8: Dead Code Removal — Track B - Context

**Gathered:** 2026-05-07
**Status:** Ready for planning

<domain>
## Phase Boundary

Wire every non-Linux Windows-buildable editor/tool application into the CMake graph — authoring `CMakeLists.txt` for each — so they build as executables under the existing `cmake --build build --config Debug` invocation. This covers all application directories (`engine/client/application/`, `engine/shared/application/`, `game/client/application/`, `game/server/application/`, `external/ours/application/`) for tools that have `win32/` or cross-platform `shared/` source. No new functionality; build-system authoring only.

Scope is BROADER than CLEAN-06's named ~35 tools: wire ALL non-Linux tools in those directories including unlisted ones (MayaExporter, ClientCacheFileBuilder, CrashReporter, DebugWindow, etc.), skipping only tools with no Windows source.

</domain>

<decisions>
## Implementation Decisions

### Qt GUI Tools — Vendored Qt 3.3.4

- **D-01:** Use vendored Qt 3.3.4 at `src/external/3rd/library/qt/3.3.4/` (consistent with DX9/Miles/STLPort/DPVS vendored SDK pattern). Do NOT require a system Qt install.
- **D-02:** Write `src/cmake/win32/FindQt3.cmake` pointing to the vendored `bin/`, `include/`, and `lib/` paths. Follow the existing `FindMiles.cmake` / `FindDPVS.cmake` / `FindSTLPort.cmake` pattern.
- **D-03:** Author custom CMake helper macros (`qt3_wrap_cpp`, `qt3_wrap_ui`) that invoke `moc.exe` and `uic.exe` from `src/external/3rd/library/qt/3.3.4/bin/` on source files with `Q_OBJECT` and `.ui` files respectively. These replace CMake's built-in Qt5 `qt5_wrap_*` macros — those are incompatible with Qt3.
- **D-04:** Stage Qt 3.3.4 runtime DLLs alongside tool executables via POST_BUILD `copy_if_different` (same pattern as Mss32.dll / dpvs.dll staging for SwgClient).
- **D-05:** The ~13 Qt GUI tools in scope: ParticleEditor, AnimationEditor, Viewer, WorldSnapshotViewer, LightningEditor, SoundEditor, NpcEditor, ClientEffectEditor, DatabaseObjectViewer, ShipComponentEditor, SwooshEditor, TemplateEditor, QuestEditor, RemoteDebugTool, BugTool, CharacterInfoTool, Headless (and any unlisted tool in `engine/client/application/` with `.ui` files).

### Linux-Only Tools — Skip

- **D-06:** Skip any tool whose `src/` directory contains only `linux/` and/or `Makefile.am` with no `win32/` subdirectory. Do NOT author a CMakeLists.txt for them in Phase 8.
- **D-07:** Confirmed Linux-only tools to skip: `ArmorExporterTool`, `DataTableTool`, `WeaponExporterTool`, `CoreWeaponExporterTool` (in `engine/shared/application/`), plus any `game/server/application/` tools (SwgGameServer, SwgDatabaseServer, PhonyApp) that have only Linux source. Verify win32/ presence before wiring any tool.

### MayaExporter — Vendored Maya 8

- **D-08:** Wire `MayaExporter` using vendored Maya 8 SDK at `src/external/3rd/library/maya8/` (latest vendored version).
- **D-09:** Write `src/cmake/win32/FindMaya.cmake` pointing to `maya8/include` and `maya8/lib`. Follow `FindMiles.cmake` pattern. If Maya 8 headers cause compile errors, fall back to `maya7/` at the planner's discretion.

### Expanded Tool Scope

- **D-10:** Wire ALL non-Linux tools in all application directories — not just CLEAN-06's named list. This includes unlisted tools (MayaExporter, ClientCacheFileBuilder, CrashReporter, DebugWindow, Miff, ViewIff, TextureBuilder, ShaderBuilder, SoePix, UiBuilder, etc.) if they have Windows source.
- **D-11:** Discovery rule: if `src/win32/` exists OR `src/shared/` contains a `main()` entry point, the tool is in scope. If neither condition holds and there is no `src/` content at all, skip with a comment.

### Wave Grouping (4 Plans)

- **D-12:** Wave 1 (Plan 08-01): `engine/shared/application/` — CLI tools: LabelHashTool, StringFileTool, WordCountTool, plus any shared/ console apps. Also write `FindQt3.cmake` and `FindMaya.cmake` in this plan as scaffold for later waves.
- **D-13:** Wave 2 (Plan 08-02): `engine/client/application/` — all Qt GUI tools plus any non-Qt client tools (Headless, RemoteDebugTool, BugTool, CharacterInfoTool, and unlisted tools like CrashReporter, DebugWindow, Miff, etc.). MayaExporter is in this wave.
- **D-14:** Wave 3 (Plan 08-03): `game/client/application/` (SwgGodClient, SwgCsTool, SwgHeadlessClient) and `game/server/application/` editors (SwgConversationEditor, SwgSpaceQuestEditor, SwgSpaceZoneEditor, SwgDraftSchematicEditor, SwgBattlefieldTool, and any non-Linux extras like SwgNameGenerator, SwgSchematicXmlParser, SwgContentBuilder if they have Windows source).
- **D-15:** Wave 4 (Plan 08-04): `external/ours/application/` (LocalizationTool, LocalizationToolCon) + boot regression verify — `cmake --build build --config Debug` clean, `SwgClient_d.exe` boots to character select.

### Boot Regression Gate

- **D-16:** Verify `SwgClient_d.exe` still compiles and boots to character select at the end of Wave 4 (Plan 08-04). Tool CMakeLists.txt additions must not break the SwgClient build or runtime. If a tool causes a linker collision (duplicate symbol from linking swgClientUserInterface into multiple executables), resolve with per-target link scoping.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Requirements
- `.planning/REQUIREMENTS.md` §CLEAN-06 — exact CLEAN-06 requirement text, named tool list, and success criteria (SC-1 through SC-3). Phase 8 expands scope beyond the named list per D-10.
- `.planning/ROADMAP.md` §Phase 8 — goal statement, depends-on, success criteria

### Existing CMake Patterns (reference implementations)
- `src/game/client/application/SwgClient/src/CMakeLists.txt` — canonical `add_executable()` + `target_link_libraries()` + `target_precompile_headers()` + POST_BUILD DLL staging pattern. All tool CMakeLists.txt should follow this structure.
- `src/CMakeLists.txt` — root CMakeLists with compiler flags, `find_package()` chain, and `include_directories(BEFORE)` pattern for STLPort and compat shim. Tool CMakeLists.txt inherit these settings.
- `src/cmake/win32/FindMiles.cmake` — reference Find module pattern (vendored SDK, IMPORTED INTERFACE target). Use this as the template for `FindQt3.cmake` and `FindMaya.cmake`.
- `src/cmake/win32/FindSTLPort.cmake` — another reference Find module (vendored prebuilt libs).

### Vendored SDK Locations
- `src/external/3rd/library/qt/3.3.4/` — Qt 3.3.4: `bin/moc.exe`, `bin/uic.exe`, `include/`, `lib/`. Use for all Qt GUI tools.
- `src/external/3rd/library/qt/4.1.0/` — Qt 4.1.0: also present; not targeted in Phase 8.
- `src/external/3rd/library/maya8/` — Maya 8 SDK: `include/`, `lib/`. Use for MayaExporter.
- `src/external/3rd/library/maya7/` — Maya 7 SDK: fallback if Maya 8 headers cause issues.

### Project State
- `.planning/STATE.md` — accumulated decisions from v1 + Phase 7; compiler flags, STLPort shims, `/FORCE:MULTIPLE` linker flag on SwgClient only

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `FindMiles.cmake`, `FindDPVS.cmake`, `FindSTLPort.cmake`, `FindDirectX9.cmake` — vendored SDK Find module pattern; copy and adapt for `FindQt3.cmake` and `FindMaya.cmake`
- `src/cmake/compat/` — VS 2022 compat shim directory prepended via `include_directories(BEFORE)`; tools inherit this automatically from root CMakeLists

### Established Patterns
- **Per-tier CMakeLists chain:** Root → `engine/` → `engine/client/` → `engine/client/application/`. Phase 8 adds `add_subdirectory(ParticleEditor)` etc. to `engine/client/application/CMakeLists.txt` (currently empty). Same pattern for the other tiers.
- **Source discovery:** No `.vcproj` files in source tree for most tools (`src/build/win32/` directory does not exist in the leak). Source files must be globbed from `src/win32/*.cpp` and `src/shared/*.cpp`. Use `file(GLOB ...)` or explicit lists.
- **SwgClient link deps as starting point:** Most client tools link against a subset of the same libs as SwgClient. Start from the SwgClient `target_link_libraries()` list and trim to only what the tool actually includes.
- **`/FORCE:MULTIPLE` is SwgClient-only:** Only needed because SwgClient links the prebuilt stlport vc71 lib AND the vc143 compat lib. Tool CMakeLists.txt should NOT inherit this flag unless they also link both stlport libs.
- **Linux-only tool identification:** Check for `win32/` subdirectory in `src/`. If absent and no cross-platform `shared/main()` exists, the tool is Linux-only — skip.

### Integration Points
- `engine/client/application/CMakeLists.txt` — currently empty (no `add_subdirectory` calls); Phase 8 adds all engine client tools here
- `engine/shared/application/CMakeLists.txt` — does not yet exist; Phase 8 creates it
- `game/client/application/CMakeLists.txt` — currently only `add_subdirectory(SwgClient)`; Phase 8 adds GodClient, CsTool, HeadlessClient
- `game/server/application/CMakeLists.txt` — does not yet exist; Phase 8 creates it
- `external/ours/application/CMakeLists.txt` — does not yet exist; Phase 8 creates it

</code_context>

<specifics>
## Specific Ideas

- Qt 3.3.4 moc.exe/uic.exe are at `src/external/3rd/library/qt/3.3.4/bin/` — the CMake helpers should invoke them with `add_custom_command(OUTPUT ...)` producing generated `.cpp` / `ui_*.h` files that are then added to the target's source list
- Maya 8 is the default target for MayaExporter; fall back to Maya 7 if Maya 8 headers cause build errors (at planner's discretion, no need to ask user)
- SwgGodClient has a flat `src/` layout (not a `win32/` subdirectory) — source files are directly in `src/game/client/application/SwgGodClient/src/`

</specifics>

<deferred>
## Deferred Ideas

- **Qt upgrade to latest (Qt5/Qt6):** User wants vendored Qt 3.3.4 for Phase 8 but noted a future phase should upgrade to the latest Qt. Requires converting `.ui` files from Qt 3.3 XML format (incompatible with Qt5+ uic). Track as a future modernisation milestone item.
- **Linux tool builds:** DataTableTool, ArmorExporterTool, WeaponExporterTool, CoreWeaponExporterTool, SwgGameServer, SwgDatabaseServer, and any other Linux-only application/ tools should be wired in a future Linux-capable phase (once the Linux build environment is established). Note this in each skipped tool's directory with a `CMakeLists.txt.linux-TODO` or in-comment.

</deferred>

---

*Phase: 8-Dead Code Removal — Track B*
*Context gathered: 2026-05-07*
