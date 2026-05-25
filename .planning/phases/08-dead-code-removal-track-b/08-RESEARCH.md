# Phase 8: Dead Code Removal — Track B - Research

**Researched:** 2026-05-07
**Domain:** CMake build-system authoring — wiring ~50+ tool and editor projects into the existing whitengold CMake graph
**Confidence:** HIGH (codebase fully verified; all tool directories inspected)

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Qt GUI Tools — Vendored Qt 3.3.4**
- D-01: Use vendored Qt 3.3.4 at `src/external/3rd/library/qt/3.3.4/`. No system Qt.
- D-02: Write `src/cmake/win32/FindQt3.cmake` pointing to vendored `bin/`, `include/`, `lib/`. Follow FindMiles.cmake pattern.
- D-03: Author custom CMake helpers `qt3_wrap_cpp` / `qt3_wrap_ui` invoking `moc.exe` / `uic.exe` from `src/external/3rd/library/qt/3.3.4/bin/`. Use `add_custom_command(OUTPUT ...)`.
- D-04: Stage `qt-mt334.dll` via POST_BUILD `copy_if_different`.
- D-05: Qt GUI tools in scope: ParticleEditor, AnimationEditor, Viewer, WorldSnapshotViewer, LightningEditor, SoundEditor, NpcEditor, ClientEffectEditor, DatabaseObjectViewer, ShipComponentEditor, SwooshEditor, TemplateEditor, QuestEditor, RemoteDebugTool, BugTool, CharacterInfoTool, Headless (and any tool with `.ui` files).

**Linux-Only Tools — Skip**
- D-06: Skip any tool whose `src/` directory contains only `linux/` and/or `Makefile.am` with no `win32/` subdirectory. Do not author CMakeLists.txt for them.
- D-07: Confirmed Linux-only tools to skip: `ArmorExporterTool`, `DataTableTool`, `WeaponExporterTool`, `CoreWeaponExporterTool` (engine/shared/application/), plus game/server tools with only Linux source. Verify `win32/` presence before wiring.

**MayaExporter — Vendored Maya 8**
- D-08: Wire MayaExporter using Maya 8 SDK at `src/external/3rd/library/maya8/`.
- D-09: Write `src/cmake/win32/FindMaya.cmake` pointing to `maya8/include` and `maya8/lib`. Follow FindMiles.cmake pattern. Fall back to maya7/ at planner's discretion if Maya 8 headers cause errors.

**Expanded Tool Scope**
- D-10: Wire ALL non-Linux tools in all application directories (not just CLEAN-06's named list). Unlisted tools (MayaExporter, CrashReporter, DebugWindow, Miff, ViewIff, Direct3d9, etc.) are in scope if they have Windows source.
- D-11: Discovery rule: if `src/win32/` exists OR `src/shared/` contains a `main()` entry point, the tool is in scope.

**Wave Grouping (4 Plans)**
- D-12: Wave 1 (08-01): `engine/shared/application/` — CLI tools + FindQt3.cmake + FindMaya.cmake scaffolding.
- D-13: Wave 2 (08-02): `engine/client/application/` — Qt GUI tools, non-Qt client tools, MayaExporter.
- D-14: Wave 3 (08-03): `game/client/application/` (SwgGodClient, SwgCsTool, SwgHeadlessClient) + `game/server/application/` editors.
- D-15: Wave 4 (08-04): `external/ours/application/` (LocalizationTool, LocalizationToolCon) + boot regression verify.

**Boot Regression Gate**
- D-16: Verify `SwgClient_d.exe` still compiles and boots to character select at end of Wave 4. Tools must not cause linker collisions in the SwgClient build. `/FORCE:MULTIPLE` stays on SwgClient only.

### Claude's Discretion

None specified — all decisions are locked.

### Deferred Ideas (OUT OF SCOPE)

- Qt upgrade to Qt5/Qt6: needs `.ui` format migration; track as future milestone.
- Linux tool builds: DataTableTool, ArmorExporterTool, WeaponExporterTool, CoreWeaponExporterTool, SwgGameServer, SwgDatabaseServer — deferred to a future Linux-capable phase. Leave a `CMakeLists.txt.linux-TODO` comment in each skipped tool directory.
</user_constraints>

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| CLEAN-06 | ~40 orphaned editor/tool projects wired into CMake; `cmake --build build --config Debug` builds all tool targets; SwgClient_d.exe continues to boot | Complete tool inventory below maps every directory to WIRE/SKIP/SPECIAL. Dependency analysis identifies blockers and missing CMake coverage. |
</phase_requirements>

---

## Summary

Phase 8 is a pure build-system authoring task: write `CMakeLists.txt` files for every non-Linux tool and editor application directory so they compile as part of the existing `cmake --build build --config Debug` invocation. No C++ source edits. No new features.

The research uncovered the complete tool inventory across all five application directories. Of the 72 application directories examined, approximately 50 are WIRE candidates (have Windows source), 9 are confirmed Linux-only skips, and 13 are special cases (flat-src, Java-only, DLL targets, or tools blocked by missing CMake coverage for their dependency libraries). Four distinct target types appear: console executables (`add_executable`), WIN32 GUI executables (`add_executable WIN32`), MFC executables (also `add_executable WIN32`), and DLLs (`add_library SHARED`).

Six critical blockers affect tools in Wave 3 and the engine/shared tier: (1) `serverGame`, `serverScript`, `serverDatabase`, and related server engine libraries have no CMakeLists.txt in this repo — tools that include their headers cannot link without first adding CMake coverage for those ~761 server-side .cpp files; (2) Oracle OCI has headers only (no `.lib` in repo), blocking SwgDatabaseServer; (3) Java JNI has headers and win32 platform files but no import `.lib`, blocking SwgGameServer; (4) `sharedTemplateDefinition` and `sharedTemplate` are missing CMakeLists.txt but are Windows-buildable; (5) MayaExporter depends on `sharedStatusWindow` (buildable, missing CMakeLists.txt) and `sharedDatabaseInterface` (Oracle-backed, complex); (6) `TemplateCompiler` and `TemplateDefinitionCompiler` require both Perforce client API headers (`clientapi.h`) and `sharedTemplateDefinition`.

**Primary recommendation:** Author all four wave plans strictly following the SwgClient CMakeLists.txt pattern. Wire the clean tools first (Waves 1 and 2). For blocked tools (SwgGameServer, SwgDatabaseServer, ArmorExporterTool, WeaponExporterTool, CoreWeaponExporterTool, PlanetWatcher), stub out their CMakeLists.txt with `if(FALSE)` guards or leave linux-TODO files — do not add server CMake libraries in Phase 8 (that is Phase 9/v3 scope).

---

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Qt 3.3.4 moc/uic code generation | CMake configure time | — | moc.exe/uic.exe invoked via add_custom_command at configure/build time before compilation |
| Tool executable linking | CMake application CMakeLists.txt | Engine library CMakeLists.txt (already built) | Each tool links a subset of already-wired static engine libs |
| Qt DLL staging | POST_BUILD copy_if_different | — | Same pattern as Mss32.dll staging in SwgClient |
| Maya plugin DLL build | add_library(SHARED) in CMakeLists.txt | FindMaya.cmake | Maya plugin is a DLL with initializePlugin/uninitializePlugin exports |
| Tier CMakeLists.txt parent hooks | engine/client/CMakeLists.txt etc. | Root src/CMakeLists.txt | Parent files need add_subdirectory(application) added |
| Server library coverage (blockers) | Deferred — not Phase 8 scope | — | ~761 server .cpp files; Oracle+JVM dependencies; out of scope |

---

## Standard Stack

### Core CMake Patterns (already established — follow these)
| Pattern | File | Purpose |
|---------|------|---------|
| Vendored SDK Find module | `src/cmake/win32/FindMiles.cmake` | Template for FindQt3.cmake and FindMaya.cmake |
| Application executable | `src/game/client/application/SwgClient/src/CMakeLists.txt` | Canonical add_executable + target_link_libraries + POST_BUILD |
| Root flags and find_package chain | `src/CMakeLists.txt` | All tools inherit these; do not re-declare |
| Compat shim include | `src/cmake/compat/` | Inherited via root include_directories(BEFORE) |

### New Find Modules Needed (Wave 1)
| Module | Path | Vendored SDK | Models After |
|--------|------|-------------|--------------|
| FindQt3.cmake | `src/cmake/win32/FindQt3.cmake` | `src/external/3rd/library/qt/3.3.4/` | FindMiles.cmake |
| FindMaya.cmake | `src/cmake/win32/FindMaya.cmake` | `src/external/3rd/library/maya8/` | FindMiles.cmake |

### New Library CMakeLists.txt Needed (prerequisite for some tools)
| Library | Path | Used By | Status |
|---------|------|---------|--------|
| sharedStatusWindow | `src/engine/shared/library/sharedStatusWindow/` | MayaExporter | Has win32/ source; buildable; needs CMakeLists.txt authored |
| sharedTemplate | `src/engine/shared/library/sharedTemplate/` | ArmorExporterTool (blocked anyway), TemplateCompiler | Has shared+win32 source; ~64 .cpp files |
| sharedTemplateDefinition | `src/engine/shared/library/sharedTemplateDefinition/` | TemplateCompiler, TemplateDefinitionCompiler | Has shared/ source; linux/ dir has only dummy.cpp; buildable |

### External SDK Verification
| SDK | Path | Key Files Verified | Notes |
|-----|------|--------------------|-------|
| Qt 3.3.4 | `src/external/3rd/library/qt/3.3.4/` | `bin/moc.exe`, `bin/uic.exe`, `lib/qt-mt334.lib`, `lib/qt-mt334.dll`, `include/` | Single release .lib — no debug variant. `qt-mt334.dll` also in `exe/win32/`. |
| Maya 8 | `src/external/3rd/library/maya8/` | `include/maya/`, `lib/OpenMaya.lib`, `lib/Foundation.lib` etc. | No debug lib variants — release-only |
| Maya 7 | `src/external/3rd/library/maya7/` | `include/`, `lib/` | Fallback per D-09 |
| Perforce P4 API | `src/external/3rd/library/perforce/` | `include/clientapi.h`, `lib/win32/libclient.lib`, `lib/win32/librpc.lib`, `lib/win32/libsupp.lib` | Used by TemplateCompiler, TemplateDefinitionCompiler, P4Qt, MayaExporter |
| TinyXML | `src/external/3rd/library/tinyxml/` | `include/tinyxml.h`, `lib/tinyxml.lib`, `lib/tinyxmld.lib` | Has debug variant; used by SwgSchematicXmlParser |
| ATI Compress | `src/external/3rd/library/ati_compress/` | `ATI_Compress.h`, `ATI_Compress.lib` | Used by MayaExporter |
| Alienbrain 123 | `src/external/3rd/library/alienbrain123/` | `Include/`, `Lib/`, `Dll/` | Used by MayaExporter (optional feature) |

---

## Complete Tool Inventory

### engine/client/application/ (Wave 2)

| Tool | Source Layout | Target Type | Entry Point | Qt (.ui count) | Verdict |
|------|--------------|-------------|-------------|----------------|---------|
| AnimationEditor | src/win32/ + src/shared/ | WIN32 EXE | `WinMain` | Yes (5 .ui files) | WIRE |
| BugTool | src/win32/ | WIN32 EXE | `WinMain` | Yes (1 .ui file) | WIRE |
| CharacterInfoTool | src/com/sony/... | Java JAR | N/A — pure Java | No | SKIP (Java, no C++) |
| ClientCacheFileBuilder | flat (no src/) | MFC WIN32 EXE | CWinApp | No | WIRE (flat src) |
| ClientEffectEditor | src/win32/ | WIN32 EXE | `WinMain` | Yes (1 .ui file) | WIRE |
| CrashReporter | src/win32/ | Console EXE | `int main()` | No | WIRE |
| CreateShaderTemplate | src/shared/ | Console EXE | `int main()` | No | WIRE |
| DatabaseObjectViewer | flat (no src/) | MFC WIN32 EXE | CWinApp | No | WIRE (flat src) |
| DebugWindow | src/win32/ | DLL | dllexport (no DllMain exposed) | No | WIRE (add_library SHARED) |
| Direct3d9 | src/win32/ + src/shared/ | DLL | DllMain in SetupDll.cpp | No | WIRE (add_library SHARED) |
| DllExport | src/win32/ | DLL | DllMain | No | WIRE (add_library SHARED) |
| Headless | src/shared/ | DLL | DllMain in SetupDll.cpp | No | WIRE (add_library SHARED) |
| LightningEditor | src/win32/ | Console EXE (Qt) | `int main()` | Yes (1 .ui file) | WIRE |
| MayaExporter | src/win32/ + src/shared/ | DLL (Maya plugin) | `initializePlugin`/`uninitializePlugin` | No | WIRE (add_library SHARED; complex deps) |
| Miff | src/win32/ | Console EXE | `int main()` | No | WIRE |
| NpcEditor | src/win32/ | Console EXE (Qt) | `int main()` | Yes (7 .ui files) | WIRE |
| ParticleEditor | src/win32/ | Console EXE (Qt) | `int main()` | Yes (15 .ui files) | WIRE |
| QuestEditor | src/win32/ | Console EXE (Qt) | `int main()` | Yes (3 .ui files) | WIRE |
| RemoteDebugTool | src/shared/ + src/win32/ | Console EXE (Qt) | `int main()` | No | WIRE |
| SetBrightnessContrastGamma | flat (no src/) | MFC WIN32 EXE | CWinApp | No | WIRE (flat src) |
| ShaderBuilder | src/win32/ | MFC WIN32 EXE | CWinApp | No | WIRE |
| ShipComponentEditor | src/win32/ | Console EXE (Qt) | `int main()` | Yes (18 .ui files) | WIRE |
| SoePix | flat (no src/) | DLL (PIX plugin) | .def exports | No | WIRE (add_library SHARED; flat src) |
| SoundEditor | src/win32/ | Console EXE (Qt) | `int main()` | Yes (5 .ui files) | WIRE |
| SwooshEditor | src/win32/ | Console EXE (Qt) | `int main()` | Yes (1 .ui file) | WIRE |
| TemplateEditor | src/shared/ | Console EXE (Qt) | `int main()` (src/shared/main.cpp) | Yes (7 .ui files) | WIRE |
| TerrainEditor | src/win32/ | MFC WIN32 EXE | CWinApp | No | WIRE |
| TextureBuilder | src/win32/ | MFC WIN32 EXE | CWinApp | No | WIRE |
| UiBuilder | flat (no src/) | MFC WIN32 EXE | CWinApp | No | WIRE (flat src) |
| ViewIff | src/win32/ | MFC WIN32 EXE | CWinApp | No | WIRE |
| Viewer | src/win32/ | MFC WIN32 EXE | CWinApp | No | WIRE (heavy client engine deps) |
| WorldSnapshotViewer | src/win32/ | MFC WIN32 EXE | CWinApp | No | WIRE |

**Qt tools (need moc/uic generation):** AnimationEditor, BugTool, ClientEffectEditor, LightningEditor, NpcEditor, ParticleEditor, QuestEditor, RemoteDebugTool, ShipComponentEditor, SoundEditor, SwooshEditor, TemplateEditor

### engine/shared/application/ (Wave 1)

| Tool | Source Layout | Target Type | Entry Point | Verdict | Blockers |
|------|--------------|-------------|-------------|---------|----------|
| AddressToLine | no src/ | — | — | SKIP | No source |
| ArmorExporterTool | src/shared/ + src/linux/ | Console EXE | `int main()` | SKIP per D-07 (serverGame dep, not in CMake) | serverGame -> serverScript -> JNI |
| CoreWeaponExporterTool | src/shared/ + src/linux/ | Console EXE | `int main()` | SKIP per D-07 (serverGame dep) | serverGame BLOCKER |
| DataLintRspBuilder | src/win32/ | MFC EXE | CWinApp | WIRE | — |
| DataTableTool | src/shared/ + src/linux/ | Console EXE | `int main()` | SKIP per D-07 | Per D-07 decision |
| LabelHashTool | src/shared/ | Console EXE | `int main()` | WIRE | Minimal deps: sharedFoundation, sharedCompression, sharedDebug, sharedThread |
| Md5sum | src/shared/ | Console EXE | `int main()` | WIRE | Minimal deps |
| P4Qt | src/shared/ | Console EXE (Qt) | `int main()` | WIRE | Qt 3.3.4 + Perforce p4api (libclient.lib, librpc.lib, libsupp.lib) |
| PlanetWatcher | src/win32/ | Console EXE (Qt) | `int main()` | SKIP (serverGame dep) | serverGame BLOCKER; many serverGame/*.h includes |
| StringFileTool | src/win32/ | MFC WIN32 EXE | CWinApp | WIRE | — |
| TemplateCompiler | src/shared/ + src/win32/ + src/linux/ | Console EXE | `int main()` | WIRE (needs new libs) | Needs sharedTemplate + sharedTemplateDefinition CMakeLists.txt + Perforce headers |
| TemplateDefinitionCompiler | src/shared/ + src/win32/ + src/linux/ | Console EXE | `int main()` | WIRE (needs new libs) | Needs sharedTemplateDefinition CMakeLists.txt + Perforce headers |
| TreeFileBuilder | src/shared/ | Console EXE | `int main()` | WIRE | sharedFile, sharedFoundation, sharedCompression, sharedThread |
| TreeFileExtractor | src/shared/ | Console EXE | `int main()` | WIRE | sharedFile, sharedFoundation |
| TreeFileRspBuilder | src/win32/ | Console EXE | `int main()` | WIRE | — |
| Turf | src/shared/ + src/win32/ + src/linux/ | Console EXE | `int main()` (shared) | WIRE | sharedTerrain, sharedObject, sharedFoundation + friends |
| UpdateLocalizedStrings | src/win32/ | Console EXE | `int main()` | WIRE | Localization libs |
| VersionNumber | no src/ | — | — | SKIP | No source |
| WeaponExporterTool | src/shared/ + src/linux/ | Console EXE | `int main()` | SKIP per D-07 (serverGame dep) | serverGame BLOCKER |
| WordCountTool | src/win32/ | MFC WIN32 EXE | CWinApp | WIRE | — |
| hash_plugin | no src/ | — | — | SKIP | No source |

### game/client/application/ (Wave 3)

| Tool | Source Layout | Target Type | Entry Point | Qt | Verdict |
|------|--------------|-------------|-------------|-----|---------|
| LaunchMeFirst | no src/ | — | — | — | SKIP |
| SwgClient | src/win32/ | WIN32 EXE | `WinMain` | No | Already wired — no change |
| SwgCsTool | src/win32/ (.cs files) | C# WinForms | N/A | No | SKIP (C# project; `.csproj` exists; not a CMake target) |
| SwgGodClient | flat src/ (no win32/ subdir) | Console EXE (Qt) | `int main()` | Yes (16 .ui files in src/shared/ui/) | WIRE (flat src layout; all .cpp directly in src/) |
| SwgHeadlessClient | src/win32/ + src/shared/ | WIN32 EXE | `WinMain` | No | WIRE |

**SwgGodClient special layout note:** All source .cpp/.h files are directly in `src/` (flat), with `.ui` files in `src/shared/ui/`. No `win32/` subdirectory. Use `file(GLOB ...)` from `src/*.cpp` and `src/shared/*.cpp`.

### game/server/application/ (Wave 3)

| Tool | Source Layout | Target Type | Entry Point | Verdict | Blockers |
|------|--------------|-------------|-------------|---------|----------|
| PhonyApp | src/linux/ only | — | — | SKIP (linux-only) | — |
| SwgBattlefieldTool | src/shared/ | Console EXE | `int main()` | WIRE | sharedFile, sharedFoundation, sharedUtility only |
| SwgContentBuilder | src/win32/ | MFC WIN32 EXE | CWinApp | WIRE | sharedFile, sharedFoundation, sharedUtility, sharedThread |
| SwgConversationEditor | src/win32/ | MFC WIN32 EXE | CWinApp | WIRE | shared engine libs only (sharedFile, sharedFoundation, sharedUtility, sharedThread, sharedCompression) |
| SwgDatabaseServer | src/win32/ + src/shared/ | WIN32 EXE | `WinMain` | SKIP (Oracle OCI blocker) | serverDatabase not in CMake; Oracle OCI has no `.lib` in repo |
| SwgDraftSchematicEditor | src/win32/ | MFC WIN32 EXE | CWinApp | WIRE | Shared engine libs only |
| SwgGameServer | src/win32/ + src/shared/ | WIN32 EXE | `WinMain` | SKIP (JVM/serverGame blocker) | serverGame + serverScript + JNI not in CMake; Java import lib missing |
| SwgNameGenerator | src/shared/ + src/win32/ | Console EXE | `int main()` | WIRE | sharedFoundation, sharedDebug, sharedFile, sharedRandom, sharedThread, sharedUtility |
| SwgSchematicXmlParser | flat src/ | Console EXE | `int main()` | WIRE | TinyXML (have lib) |
| SwgSpaceQuestEditor | src/win32/ | MFC WIN32 EXE | CWinApp | WIRE | shared engine libs + localization |
| SwgSpaceZoneEditor | src/win32/ | MFC WIN32 EXE | CWinApp | WIRE | shared engine libs + sharedUtility/DataTable |

### external/ours/application/ (Wave 4)

| Tool | Source Layout | Target Type | Entry Point | Qt | Verdict |
|------|--------------|-------------|-------------|-----|---------|
| LagOMatic | no src/ | — | — | — | SKIP |
| LocalizationTool | src/shared/ + src/win32/ | Console EXE (Qt) | `int main()` | Yes (3 .ui files: ui_EditingWidget.ui etc.) | WIRE |
| LocalizationToolCon | src/shared/ + src/win32/ | Console EXE | `int main()` | No | WIRE |

---

## Architecture Patterns

### System Architecture Diagram

```
src/CMakeLists.txt  (root — compiler flags, find_package chain, inherited by all)
    │
    ├── external/CMakeLists.txt
    │   ├── external/3rd/CMakeLists.txt        (vendored 3rd-party — already done)
    │   └── external/ours/CMakeLists.txt
    │       ├── external/ours/library/          (already wired)
    │       └── external/ours/application/     ← Phase 8 Wave 4: add_subdirectory here
    │           ├── LocalizationTool/CMakeLists.txt  ← NEW
    │           └── LocalizationToolCon/CMakeLists.txt  ← NEW
    │
    ├── engine/CMakeLists.txt
    │   ├── engine/shared/CMakeLists.txt
    │   │   ├── engine/shared/library/          (already wired)
    │   │   └── engine/shared/application/     ← Phase 8 Wave 1: new CMakeLists.txt here
    │   │       ├── LabelHashTool/CMakeLists.txt  ← NEW (and ~9 more)
    │   │       ├── sharedStatusWindow/         ← NEW library CMakeLists.txt (MayaExporter dep)
    │   │       ├── sharedTemplate/             ← NEW library CMakeLists.txt
    │   │       └── sharedTemplateDefinition/   ← NEW library CMakeLists.txt
    │   └── engine/client/CMakeLists.txt
    │       ├── engine/client/library/          (already wired)
    │       └── engine/client/application/     ← Phase 8 Wave 2: add_subdirectory here
    │           ├── AnimationEditor/CMakeLists.txt  ← NEW (and ~25 more)
    │           └── ...
    │
    └── game/CMakeLists.txt
        ├── game/shared/CMakeLists.txt          (already wired)
        ├── game/client/CMakeLists.txt
        │   ├── game/client/library/            (already wired)
        │   └── game/client/application/CMakeLists.txt
        │       └── [add SwgGodClient, SwgHeadlessClient] ← Phase 8 Wave 3 edits
        └── game/server/CMakeLists.txt         ← Phase 8 Wave 3: add_subdirectory(application)
            └── game/server/application/       ← NEW CMakeLists.txt + tool subdirs
```

**Data flow:** `cmake --build build --config Debug` traverses the add_subdirectory chain, compiling tool sources against the already-built engine library static archives. Qt tools additionally require the moc/uic pre-generation step that fires via add_custom_command before compilation.

### Recommended Project Structure (per-tool CMakeLists.txt)

```
ToolName/
    CMakeLists.txt              ← authored in Phase 8
    src/
        win32/                  ← standard layout: source files here
            FirstToolName.cpp
            main.cpp or WinMain.cpp or ToolName.cpp (CWinApp)
        shared/                 ← optional cross-platform source
```

**Flat-src layout (for ClientCacheFileBuilder, DatabaseObjectViewer, SetBrightnessContrastGamma, UiBuilder, SoePix, SwgSchematicXmlParser):**
```
ToolName/                       ← CMakeLists.txt in parent, sources are siblings
    CMakeLists.txt
    ToolName.cpp
    StdAfx.cpp
    ...
```

### Pattern 1: Console Executable (no Qt)

```cmake
# Source: follows SwgClient/src/CMakeLists.txt pattern (VERIFIED: codebase)
set(SOURCES
    shared/FirstLabelHashTool.cpp
    shared/LabelHashTool.cpp
    shared/LabelHashTool.h
)

add_executable(LabelHashTool ${SOURCES})

target_include_directories(LabelHashTool PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/shared
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundation/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedDebug/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedCompression/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedThread/include/public
)

target_link_libraries(LabelHashTool PRIVATE
    sharedFoundation
    sharedDebug
    sharedCompression
    sharedThread
    ${STLPORT_LIBRARY}
    legacy_stdio_definitions
)

target_precompile_headers(LabelHashTool PRIVATE
    shared/FirstLabelHashTool.h
)
```

### Pattern 2: WIN32 Executable (WinMain or MFC CWinApp)

```cmake
# Source: follows SwgClient WIN32 flag pattern (VERIFIED: codebase)
add_executable(AnimationEditor WIN32
    ${SOURCES}
)
# MFC tools: also add /D_AFXDLL or link against mfc140 — see Pattern notes below
```

**MFC tools note:** Tools using `CWinApp` (TerrainEditor, ShaderBuilder, Viewer, TextureBuilder, StringFileTool, etc.) need MFC. Under VS 2022, MFC via `add_executable WIN32` and linking `Mfc140.lib` / `Mfc140d.lib` (or using `/MD` with dynamic MFC). MSVC 2022 ships MFC; use `target_link_libraries(... mfc140 mfcs140)` for release or `mfc140d mfcs140d` for debug. The Static CRT rule (`/MT`/`/MTd`) may conflict with dynamic MFC — plan must resolve this per tool. [ASSUMED — verify actual MFC linkage requirement against build output]

### Pattern 3: DLL Target

```cmake
# Source: SetupDll.cpp/DllMain pattern observed in Headless, Direct3d9 (VERIFIED: codebase)
add_library(Headless SHARED
    ${SOURCES}
)
set_target_properties(Headless PROPERTIES
    OUTPUT_NAME_DEBUG   Headless_d
    OUTPUT_NAME_RELEASE Headless_r
)
```

### Pattern 4: Qt Tool (moc + uic generation)

```cmake
# Source: D-03 pattern from CONTEXT.md; moc.exe/uic.exe verified at qt/3.3.4/bin/ (VERIFIED: codebase)

# -- In FindQt3.cmake --
set(QT3_MOC_EXECUTABLE "${QT3_BIN_DIR}/moc.exe")
set(QT3_UIC_EXECUTABLE "${QT3_BIN_DIR}/uic.exe")

# Helper macro: qt3_wrap_cpp(outvar src1.h src2.h ...)
macro(qt3_wrap_cpp outvar)
    foreach(_header ${ARGN})
        get_filename_component(_base ${_header} NAME_WE)
        set(_moc "${CMAKE_CURRENT_BINARY_DIR}/moc_${_base}.cpp")
        add_custom_command(
            OUTPUT  ${_moc}
            COMMAND ${QT3_MOC_EXECUTABLE} -o ${_moc} ${_header}
            DEPENDS ${_header}
            VERBATIM
        )
        list(APPEND ${outvar} ${_moc})
    endforeach()
endmacro()

# Helper macro: qt3_wrap_ui(outvar form1.ui form2.ui ...)
macro(qt3_wrap_ui outvar)
    foreach(_ui ${ARGN})
        get_filename_component(_base ${_ui} NAME_WE)
        set(_h  "${CMAKE_CURRENT_BINARY_DIR}/ui_${_base}.h")
        set(_cpp "${CMAKE_CURRENT_BINARY_DIR}/ui_${_base}.cpp")
        add_custom_command(
            OUTPUT  ${_h} ${_cpp}
            COMMAND ${QT3_UIC_EXECUTABLE} -o ${_h} ${_ui}
            COMMAND ${QT3_UIC_EXECUTABLE} -impl ${_h} -o ${_cpp} ${_ui}
            DEPENDS ${_ui}
            VERBATIM
        )
        list(APPEND ${outvar} ${_h} ${_cpp})
    endforeach()
endmacro()

# -- In tool CMakeLists.txt --
find_package(Qt3 REQUIRED)

file(GLOB UI_FILES "${CMAKE_CURRENT_SOURCE_DIR}/ui/*.ui")
file(GLOB MOC_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/src/win32/*.h")

qt3_wrap_ui(GENERATED_UI ${UI_FILES})
qt3_wrap_cpp(GENERATED_MOC ${MOC_HEADERS})   # only headers with Q_OBJECT

add_executable(ParticleEditor
    ${SOURCES}
    ${GENERATED_UI}
    ${GENERATED_MOC}
)

target_include_directories(ParticleEditor PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}    # for ui_*.h generated files
    ${QT3_INCLUDE_DIR}
    ...
)

target_link_libraries(ParticleEditor PRIVATE
    ${QT3_LIBRARIES}
    ...
)

# POST_BUILD: stage qt-mt334.dll
add_custom_command(TARGET ParticleEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${QT3_BIN_DIR}/qt-mt334.dll" "$<TARGET_FILE_DIR:ParticleEditor>/qt-mt334.dll"
    VERBATIM
)
```

### Pattern 5: FindQt3.cmake Template

```cmake
# Source: follows FindMiles.cmake pattern (VERIFIED: codebase)
set(_QT3_ROOT ${SWG_EXTERNALS_FIND}/qt/3.3.4)

find_path(QT3_INCLUDE_DIR
    PATHS ${_QT3_ROOT}/include
    NAMES qapplication.h
)

find_library(QT3_LIBRARIES
    NAMES qt-mt334
    PATHS ${_QT3_ROOT}/lib
)

set(QT3_BIN_DIR ${_QT3_ROOT}/bin)
set(QT3_MOC_EXECUTABLE ${QT3_BIN_DIR}/moc.exe)
set(QT3_UIC_EXECUTABLE ${QT3_BIN_DIR}/uic.exe)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Qt3 DEFAULT_MSG QT3_INCLUDE_DIR QT3_LIBRARIES)

mark_as_advanced(QT3_INCLUDE_DIR QT3_LIBRARIES)
```

**Note on debug variant:** Qt 3.3.4 ships only `qt-mt334.lib` — no separate debug lib. Use the same lib for both Debug and Release builds. The DLL `qt-mt334.dll` is already present in `exe/win32/` and in `lib/qt-mt334.dll` (same file). [VERIFIED: inspected qt/3.3.4/lib/]

### Pattern 6: FindMaya.cmake Template

```cmake
# Source: follows FindMiles.cmake pattern (VERIFIED: codebase)
set(_MAYA8_ROOT ${SWG_EXTERNALS_FIND}/maya8)

find_path(MAYA_INCLUDE_DIR
    PATHS ${_MAYA8_ROOT}/include
    NAMES maya/MFnPlugin.h
)

find_library(MAYA_FOUNDATION_LIBRARY
    NAMES Foundation
    PATHS ${_MAYA8_ROOT}/lib
)

find_library(MAYA_OPENMAYA_LIBRARY
    NAMES OpenMaya
    PATHS ${_MAYA8_ROOT}/lib
)

set(MAYA_LIBRARIES ${MAYA_FOUNDATION_LIBRARY} ${MAYA_OPENMAYA_LIBRARY})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Maya DEFAULT_MSG MAYA_INCLUDE_DIR MAYA_FOUNDATION_LIBRARY)

mark_as_advanced(MAYA_INCLUDE_DIR MAYA_FOUNDATION_LIBRARY MAYA_OPENMAYA_LIBRARY)
```

### Pattern 7: Parent CMakeLists.txt edits (new add_subdirectory calls)

The following parent files need `add_subdirectory(application)` added — they currently only have `add_subdirectory(library)` or are empty:

| Parent file | Current state | Phase 8 addition |
|-------------|---------------|-----------------|
| `src/engine/client/CMakeLists.txt` | `add_subdirectory(library)` only | Add `add_subdirectory(application)` |
| `src/engine/shared/CMakeLists.txt` | `add_subdirectory(library)` only | Add `add_subdirectory(application)` |
| `src/game/server/CMakeLists.txt` | **Empty** | Add `add_subdirectory(application)` |
| `src/external/ours/CMakeLists.txt` | `add_subdirectory(library)` only | Add `add_subdirectory(application)` |
| `src/game/client/application/CMakeLists.txt` | `add_subdirectory(SwgClient)` only | Add SwgGodClient, SwgHeadlessClient |

Also: `src/game/CMakeLists.txt` currently has `add_subdirectory(shared)` + `add_subdirectory(client)` but NOT `add_subdirectory(server)`. Phase 8 must add it. [VERIFIED: codebase]

### Anti-Patterns to Avoid

- **Inheriting `/FORCE:MULTIPLE` on tool targets:** This flag is SwgClient-only. Tool CMakeLists.txt must not set it unless the tool also links both stlport_vc71 and stlport_vc143_compat. [VERIFIED: CONTEXT.md code_context]
- **Using `qt5_wrap_cpp` / `qt5_wrap_ui`:** These are Qt5 CMake built-ins; incompatible with Qt 3.3.4. Use custom `qt3_wrap_cpp` / `qt3_wrap_ui` macros instead. [VERIFIED: D-03]
- **Globbing Q_OBJECT headers without filtering:** moc.exe should only be run on headers that actually contain `Q_OBJECT`. Globbing all headers will produce compile errors on generated moc_ files. Filter by checking which headers have Q_OBJECT, or use explicit lists.
- **Adding tool targets to `add_subdirectory` before the parent chain is wired:** `game/server/CMakeLists.txt` is empty — must add `add_subdirectory(application)` there before the application CMakeLists.txt will be processed.
- **Using static MFC with `/MT` CRT:** Static MFC (`/MT`) requires `mfcs140.lib`/`mfcs140d.lib`. MFC tools must use this variant or switch to dynamic MFC (`/MD` + `mfc140.lib`). The root CMakeLists.txt sets `MultiThreaded$<$<CONFIG:Debug>:Debug>` (static CRT). Verify MFC link mode for each tool at plan time. [ASSUMED — verify against actual build output]

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Qt 3.3.4 moc/uic invocation | Custom Python/CMake wrapper | `qt3_wrap_cpp`/`qt3_wrap_ui` macros in FindQt3.cmake | Edge cases: dependency tracking, DEPENDS clause, multiple outputs per .ui |
| Maya plugin DLL entry | Custom DllMain stub | Maya SDK `initializePlugin` + `add_library SHARED` | Maya loads the plugin by name; entry point signature is fixed by SDK |
| Qt runtime DLL staging | Manual file copy | POST_BUILD `copy_if_different` | Matches established pattern; generator expressions handle Debug/Release correctly |
| Vendored SDK discovery | Hardcoded paths in tool CMakeLists.txt | FindXxx.cmake modules | Consistent with all other SDKs; centralizes path changes |

---

## Common Pitfalls

### Pitfall 1: moc.exe header filter — running moc on all headers

**What goes wrong:** `qt3_wrap_cpp` called on headers that lack `Q_OBJECT` generates empty moc_ files. These compile fine but add noise. Worse: if a generated moc_ file includes a class hierarchy that triggers a different Q_OBJECT moc, you get double-definition linker errors.

**Why it happens:** The macro receives a glob of all .h files. Only those with `Q_OBJECT` need moc treatment.

**How to avoid:** Either use explicit header lists (preferred for small tools), or add a filter step that checks `grep -l Q_OBJECT`.

**Warning signs:** Linker errors for `vtable for class X` defined in multiple translation units.

### Pitfall 2: Qt .ui files generate both .h AND .cpp — both must be listed

**What goes wrong:** `uic.exe` for Qt 3.3 produces two output files per .ui: `ui_Foo.h` (widget declaration) and `ui_Foo.cpp` (implementation). If only the .h is in the source list, link fails with "undefined reference to BaseMainWindow constructor."

**Why it happens:** Qt 5+ uic only produces a header; Qt 3.3 uic produces both. The CMake `qt3_wrap_ui` macro must emit both outputs.

**How to avoid:** The `qt3_wrap_ui` macro above emits both `-o ${_h}` and `-impl ${_h} -o ${_cpp}`. Include both in the target sources.

**Warning signs:** Unresolved symbol errors on Base* class constructors from the .ui files.

### Pitfall 3: Flat-src tools (no src/ subdirectory) have different glob paths

**What goes wrong:** For ClientCacheFileBuilder, DatabaseObjectViewer, SetBrightnessContrastGamma, UiBuilder, SoePix, and SwgSchematicXmlParser — source files live directly in the tool's root directory (no `src/win32/` subdirectory). A CMakeLists.txt that looks for `${CMAKE_CURRENT_SOURCE_DIR}/src/win32/*.cpp` will find nothing.

**Why it happens:** These tools predate the standard `src/win32/` layout convention.

**How to avoid:** Use `file(GLOB ...)` from `${CMAKE_CURRENT_SOURCE_DIR}/*.cpp` and exclude `StdAfx.cpp` if it exists (MSVC handles precompiled headers separately).

**Warning signs:** Empty source list → add_executable with no sources → CMake configure error.

### Pitfall 4: MFC static vs. dynamic CRT conflict

**What goes wrong:** Root CMakeLists.txt sets `CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"` (i.e., `/MT`/`/MTd`). Static MFC (`mfcs140.lib`) also requires `/MT`. Dynamic MFC (`mfc140.lib`) requires `/MD`. Using mismatched variants produces dozens of LNK2005 multiple-definition errors for CRT symbols.

**Why it happens:** The root CRT policy is correct for game code. MFC tools must use the matching static MFC variant.

**How to avoid:** Link `mfcs140` (release) / `mfcs140d` (debug) for MFC tools, matching the `/MT`/`/MTd` root setting. Or override the CRT per-target with `set_target_properties(... MSVC_RUNTIME_LIBRARY MultiThreaded<$<...>>)` if a tool needs dynamic MFC.

**Warning signs:** LNK2005 errors for `__acrt_iob_func`, `__stdio_common_*`, `malloc` etc.

### Pitfall 5: SwgGodClient flat src/ layout — precompiled header path

**What goes wrong:** SwgGodClient source files are directly in `src/` (not `src/win32/`). The precompiled header `FirstSwgGodClient.h` is in `src/` too. `target_precompile_headers` must point to the correct path, and `include_directories` must include `src/` as well as `src/shared/`.

**Why it happens:** SwgGodClient uses a non-standard flat layout confirmed in CONTEXT.md D-specifics.

**How to avoid:** Use `file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/shared/*.cpp")` and add `target_include_directories(... ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/shared)`.

**Warning signs:** "Cannot open source file 'FirstSwgGodClient.h'" at precompile step.

### Pitfall 6: game/server/CMakeLists.txt is empty — the chain breaks silently

**What goes wrong:** `src/game/server/CMakeLists.txt` is currently empty. `src/game/CMakeLists.txt` has `add_subdirectory(server)` already — but the empty server CMakeLists.txt means the application subtree is never reached. Adding tool CMakeLists.txt without first adding `add_subdirectory(application)` to the server CMakeLists.txt means the tools are silently ignored.

**Why it happens:** The server build was never part of the CMake graph in Phase 1-7.

**How to avoid:** Wave 3 must first add `add_subdirectory(application)` to `src/game/server/CMakeLists.txt`, then create `src/game/server/application/CMakeLists.txt` with `add_subdirectory(...)` for each tool.

**Warning signs:** No server tool targets appear in the VS solution or `cmake --build` output; no error is reported.

### Pitfall 7: serverGame/serverScript/Oracle/JVM blocked tools — fail silently if wired

**What goes wrong:** If SwgGameServer, SwgDatabaseServer, ArmorExporterTool, WeaponExporterTool, CoreWeaponExporterTool, PlanetWatcher are added to the CMake graph without their missing library prerequisites, the configure step will succeed but the build will fail with "cannot open include file: serverGame/GameServer.h" or linker errors for serverGame symbols.

**Why it happens:** These tools include engine/server library headers that have no CMakeLists.txt; the include paths are not set up for them.

**How to avoid:** Per D-06/D-07, skip these tools in Phase 8. Leave a `CMakeLists.txt.linux-TODO` comment or a guarded `if(FALSE) ... endif()` stub in each directory explaining the blocker. Do not add them to the `add_subdirectory` call in the parent.

**Warning signs:** Configure succeeds, build fails immediately on the first tool with a missing serverGame include.

### Pitfall 8: Direct3d9 DLL output name may collide with the pre-built gl05_*.dll

**What goes wrong:** The pre-built `gl05_r.dll` and `gl05_o.dll` in `exe/win32/` are the production Direct3d9 renderer plugin. Building Direct3d9 from source in Phase 8 with `OUTPUT_NAME Direct3d9` produces `Direct3d9.dll` — a different name — which won't be picked up by SwgClient (which expects `gl05_r.dll`/`gl05_o.dll`/`gl05_d.dll`). If Phase 8 accidentally stages it as `gl05_d.dll`, it may overwrite the working pre-built and break SwgClient.

**Why it happens:** The naming convention between source and binary differs; Phase 11 (D3D11) is where the DLL naming is canonicalized.

**How to avoid:** In Phase 8, build Direct3d9 as `Direct3d9.dll` (not staged to the client output dir). Phase 11 will resolve the output name. Don't add a POST_BUILD that stages it over the `gl05_*.dll` pre-builds.

---

## Runtime State Inventory

This is a build-system-only phase (no renames, no data migrations). No runtime state is affected.

**Nothing found in any category** — verified: Phase 8 authors CMakeLists.txt files only. No database records, service configs, OS registrations, secrets, or installed packages reference the tools being wired.

---

## Environment Availability

| Dependency | Required By | Available | Version/Path | Fallback |
|------------|------------|-----------|-------------|----------|
| Visual Studio 2022 / MSVC | All tool builds | Assumed present (from v1 build) | — | — |
| CMake 3.27+ | Build orchestration | Assumed present (from v1 build) | — | — |
| Qt 3.3.4 moc.exe / uic.exe | All Qt GUI tools | VERIFIED present | `src/external/3rd/library/qt/3.3.4/bin/moc.exe` | — |
| qt-mt334.lib | Qt GUI tools link | VERIFIED present | `src/external/3rd/library/qt/3.3.4/lib/qt-mt334.lib` | — |
| qt-mt334.dll | Qt GUI tools runtime | VERIFIED present | `src/external/3rd/library/qt/3.3.4/lib/qt-mt334.dll` (also `exe/win32/`) | — |
| Maya 8 headers + libs | MayaExporter | VERIFIED present | `src/external/3rd/library/maya8/include/` + `lib/` | Maya 7 fallback at maya7/ |
| Perforce P4 API | TemplateCompiler, TemplateDefinitionCompiler, P4Qt, MayaExporter | VERIFIED present | `src/external/3rd/library/perforce/include/clientapi.h`, `lib/win32/*.lib` | — |
| TinyXML | SwgSchematicXmlParser | VERIFIED present | `src/external/3rd/library/tinyxml/lib/tinyxml.lib` + debug variant | — |
| ATI Compress | MayaExporter | VERIFIED present | `src/external/3rd/library/ati_compress/ATI_Compress.lib` | — |
| Alienbrain SDK | MayaExporter (optional feature) | VERIFIED present | `src/external/3rd/library/alienbrain123/` | Skip alienbrain source files |
| Oracle OCI libs | SwgDatabaseServer | NOT present | headers only at `src/external/3rd/library/oracle/include/` | BLOCKER — skip SwgDatabaseServer |
| Java JNI import lib | SwgGameServer | NOT present | headers at `src/external/3rd/library/java/include/` — no .lib | BLOCKER — skip SwgGameServer |
| serverGame CMakeLists.txt | SwgGameServer, ArmorExporterTool, PlanetWatcher | NOT present | 343 .cpp in `engine/server/library/serverGame/src/` — no CMakeLists.txt | BLOCKER — skip these tools |
| MFC libraries | MFC GUI tools (TerrainEditor, etc.) | Present in VS 2022 install | mfcs140.lib / mfcs140d.lib | — |

**Missing dependencies with no fallback (blockers):**
- Oracle OCI .lib — blocks SwgDatabaseServer (per D-07 skip list — confirmed correct)
- Java JNI import .lib — blocks SwgGameServer (per D-07 skip list — confirmed correct)
- serverGame CMakeLists.txt — blocks SwgGameServer, ArmorExporterTool, WeaponExporterTool, CoreWeaponExporterTool, PlanetWatcher

**Missing dependencies with fallback:**
- sharedStatusWindow CMakeLists.txt — blocks MayaExporter link; authored as part of Wave 2 (source exists and is buildable)
- sharedTemplate / sharedTemplateDefinition CMakeLists.txt — blocks TemplateCompiler and TemplateDefinitionCompiler link; authored as part of Wave 1 scaffolding

---

## Code Examples

### FindMiles.cmake reference (FindQt3.cmake models after this)
```cmake
# VERIFIED: src/cmake/win32/FindMiles.cmake
find_path(MILES_INCLUDE_DIR
    PATHS ${SWG_EXTERNALS_FIND}/miles/include
    NAMES Mss.h
)
find_library(MILES_LIBRARY
    NAMES Mss32
    PATHS ${SWG_EXTERNALS_FIND}/miles/lib/win
)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Miles DEFAULT_MSG MILES_INCLUDE_DIR MILES_LIBRARY)
mark_as_advanced(MILES_INCLUDE_DIR MILES_LIBRARY)
```

### SwgClient POST_BUILD DLL staging reference
```cmake
# VERIFIED: src/game/client/application/SwgClient/src/CMakeLists.txt
add_custom_command(TARGET SwgClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/Mss32.dll" "$<TARGET_FILE_DIR:SwgClient>/Mss32.dll"
    VERBATIM
)
# Same pattern for qt-mt334.dll staging
```

### Root compiler flags (inherited by tools — do not redeclare)
```cmake
# VERIFIED: src/CMakeLists.txt
set(CMAKE_CXX_FLAGS_DEBUG "... /DWIN32 /D_DEBUG /DUDP_LIBRARY /DDEBUG_LEVEL=2 /DPRODUCTION=0 /D_USE_32BIT_TIME_T=1 /D_MBCS /Zc:wchar_t- ...")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")  # /MT or /MTd
set(CMAKE_EXE_LINKER_FLAGS "... /NODEFAULTLIB:libc.lib /NODEFAULTLIB:stlport_vc71_static.lib /SAFESEH:NO")
```

### MayaExporter add_library SHARED pattern
```cmake
# Based on Headless/Direct3d9 DllMain pattern (VERIFIED: codebase)
add_library(MayaExporter SHARED
    ${WIN32_SOURCES}
    ${SHARED_SOURCES}
)
set_target_properties(MayaExporter PROPERTIES
    SUFFIX ".mll"    # Maya plugin extension convention
)
target_link_libraries(MayaExporter PRIVATE
    ${MAYA_LIBRARIES}
    sharedFoundation sharedFile sharedMath sharedCollision sharedDebug
    sharedThread sharedCompression sharedImage sharedRandom
    clientGraphics clientSkeletalAnimation
    sharedStatusWindow      # NEW CMakeLists.txt needed
    ${STLPORT_LIBRARY}
    # ATI, Alienbrain, Perforce if alienbrain importer included
)
```

---

## State of the Art

| Old Approach | Current Approach | Impact |
|--------------|-----------------|--------|
| VS 2005 `.vcproj` per tool | CMake `CMakeLists.txt` per tool | Phase 8 authors the modern equivalent |
| Pre-built `exe/win32/*.exe` binaries | Source-built executables via CMake | Phase 8 enables rebuilding from source |
| No CMake coverage for application tier | Per-tier `CMakeLists.txt` chain | Phase 8 completes the CMake graph for client/shared/server tiers |

**Deprecated/outdated:**
- `.vcproj` files: none exist in the source tree for tools (only the solution `swg.sln` at `src/build/win32/` references them). Phase 8 replaces them with CMakeLists.txt. [VERIFIED: no .vcproj found under engine/client/application/]

---

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | MFC tools must link `mfcs140`/`mfcs140d` (static MFC) to match the `/MT`/`/MTd` root CRT setting | Common Pitfalls #4, Architecture Patterns | If dynamic MFC works, the link flags in each MFC tool CMakeLists.txt need changing |
| A2 | `Q_OBJECT`-filtered header approach prevents moc double-definition errors | Common Pitfalls #1 | If Qt 3.3 moc handles non-Q_OBJECT headers gracefully, filtering is unnecessary but harmless |
| A3 | Direct3d9 DLL output name should NOT be set to `gl05_*.dll` in Phase 8 | Common Pitfalls #8 | If the pre-built gl05_*.dll is corrupted or wrong, building Direct3d9 under a different name defers the fix to Phase 11 |
| A4 | SwgGameServer and SwgDatabaseServer should be skipped per D-07 even though they have `src/win32/` — their server lib dependencies are blockers | Tool Inventory | If Phase 8 decides to stub out server libs, these tools could be wired with linker stubs; risk is incomplete build |
| A5 | ArmorExporterTool, WeaponExporterTool, CoreWeaponExporterTool, DataTableTool skipped per D-07 | Tool Inventory | These have `shared/main()` and could theoretically build on Windows with server lib coverage; D-07 explicitly defers them |
| A6 | Maya 8 lib single-variant (no debug) is used for both Debug and Release builds of MayaExporter | Environment Availability | If Maya 8 symbols are mismatched, fall back to Maya 7 per D-09 |

---

## Open Questions

1. **MFC CRT linkage for tools with CWinApp**
   - What we know: Root CMakeLists.txt sets static CRT (`/MT`/`/MTd`). Many tools use MFC (TerrainEditor, Viewer, ShaderBuilder, StringFileTool, TextureBuilder, etc.).
   - What's unclear: Whether VS 2022 ships static MFC libs (`mfcs140.lib`) by default or requires workload component selection. Whether the MFC tools actually build with static CRT or need `/MD`.
   - Recommendation: Plan 08-02 task for MFC tools should first attempt static MFC link; if LNK2005 errors appear, switch per-tool to dynamic MFC with override.

2. **DatabaseObjectViewer, SetBrightnessContrastGamma, UiBuilder, ClientCacheFileBuilder — MFC apps with flat src layout — which engine libs do they need?**
   - What we know: These use `CWinApp` (MFC). Their .cpp files have no engine `#include` headers visible in the flat directory.
   - What's unclear: Whether they depend on engine libs at all or compile standalone with just MFC + Win32.
   - Recommendation: Inspect each tool's .cpp includes in Wave 2 planning task; expect MFC-only apps with no engine lib dependencies.

3. **TemplateCompiler Perforce dep scope**
   - What we know: TemplateCompiler includes `clientapi.h` (Perforce) and `sharedTemplateDefinition`. The Perforce lib has `lib/win32/*.lib`.
   - What's unclear: Whether Perforce is used at compile time (always included) or only in a conditional code path that can be guarded out.
   - Recommendation: Wire TemplateCompiler with Perforce include path and lib; if build fails cleanly, it compiles with it. Perforce integration is a tool feature, not a blocker.

4. **MayaExporter sharedDatabaseInterface scope**
   - What we know: `DatabaseImporter.cpp` includes `sharedDatabaseInterface/DBServer.h`. The `sharedDatabaseInterface` library has Oracle OCI in its implementation but the header chain for `DbServer.h` may be self-contained.
   - What's unclear: Whether linking sharedDatabaseInterface requires Oracle OCI (missing) or just compiles the shared/core/ files which use abstract interfaces.
   - Recommendation: Attempt to author sharedDatabaseInterface CMakeLists.txt using only `src/shared/` sources (not `src_oci/`). If link fails due to Oracle symbols, exclude `DatabaseImporter.cpp` from MayaExporter's source list with a comment.

---

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | None — build-system phase; validation is cmake --build success + SwgClient_d.exe boot |
| Config file | n/a |
| Quick run command | `cmake --build build --config Debug 2>&1 \| Select-String "error"` |
| Full suite command | `cmake --build build --config Debug` followed by launch SwgClient_d.exe to character select |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | Notes |
|--------|----------|-----------|-------------------|-------|
| CLEAN-06 SC-1 | CMakeLists.txt exists for every tool in scope | Build artifact check | `cmake --build build --config Debug` (tools appear in VS solution) | Manual verify target count |
| CLEAN-06 SC-2 | All tool targets compile and link without error | Build | `cmake --build build --config Debug 2>&1 \| Select-String " error "` | Per wave |
| CLEAN-06 SC-3 | SwgClient_d.exe continues to boot to character select | Smoke test | Launch SwgClient_d.exe manually | Manual — per Wave 4 |

### Sampling Rate
- **Per wave commit:** `cmake --build build --config Debug` on the tools wired in that wave
- **Phase gate:** Full Debug build clean + SwgClient_d.exe boot before `/gsd-verify-work`

### Wave 0 Gaps
None — this is a CMake-authoring phase with no test framework to bootstrap.

---

## Security Domain

Security enforcement is enabled per config.json. This phase is build-system authoring only — no new runtime code paths, no authentication, no network endpoints, no user input processing, no data storage. ASVS categories V2–V6 are not applicable.

| ASVS Category | Applies | Rationale |
|---------------|---------|-----------|
| V2 Authentication | No | No auth code modified |
| V3 Session Management | No | No session code modified |
| V4 Access Control | No | No access control paths modified |
| V5 Input Validation | No | No runtime input processing; CMake script only |
| V6 Cryptography | No | No crypto code modified |

---

## Sources

### Primary (HIGH confidence)
- Codebase inspection — all tool directories under `src/engine/client/application/`, `src/engine/shared/application/`, `src/game/client/application/`, `src/game/server/application/`, `src/external/ours/application/` — source layouts, entry points, Qt usage, and dependency includes verified by direct file read
- `src/game/client/application/SwgClient/src/CMakeLists.txt` — canonical executable pattern
- `src/cmake/win32/FindMiles.cmake`, `FindSTLPort.cmake`, `FindDPVS.cmake` — Find module patterns
- `src/CMakeLists.txt` — root flags and find_package chain
- `src/external/3rd/library/qt/3.3.4/` — Qt SDK structure verified (moc.exe, uic.exe, qt-mt334.lib, qt-mt334.dll present)
- `src/external/3rd/library/maya8/` — Maya 8 SDK structure verified (include/maya/, lib/*.lib present, release-only)
- `src/external/3rd/library/perforce/` — Perforce P4 API verified (include/clientapi.h, lib/win32/*.lib)
- `src/external/3rd/library/tinyxml/` — TinyXML verified (tinyxml.lib + tinyxmld.lib)
- `.planning/phases/08-dead-code-removal-track-b/08-CONTEXT.md` — all locked decisions

### Secondary (MEDIUM confidence)
- Engine/server library directory inspection — serverGame, serverScript, serverDatabase, serverUtility lack CMakeLists.txt; Oracle OCI is headers-only; Java JNI has headers but no import .lib

### Tertiary (LOW confidence)
- MFC static/dynamic CRT linkage behavior — conventional knowledge, not tested against VS 2022 MSVC with `/MT` against MFC tools in this specific build setup [A1]

---

## Metadata

**Confidence breakdown:**
- Tool inventory (WIRE/SKIP/SPECIAL): HIGH — every directory inspected directly
- Source layouts and entry points: HIGH — directly read from source files
- Qt/Maya/Perforce SDK verification: HIGH — directory listing confirmed
- Missing CMake lib coverage (serverGame etc.): HIGH — confirmed no CMakeLists.txt
- MFC CRT linkage details: LOW — conventional; not verified against actual build

**Research date:** 2026-05-07
**Valid until:** 2026-06-07 (stable codebase — no external dependencies that change)
