# Phase 8: Dead Code Removal — Track B - Pattern Map

**Mapped:** 2026-05-07
**Files analyzed:** 17 distinct file types / categories (Find modules, parent CMakeLists, per-tool CMakeLists, library CMakeLists)
**Analogs found:** 17 / 17

---

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `src/cmake/win32/FindQt3.cmake` | config (Find module) | batch (SDK discovery) | `src/cmake/win32/FindMiles.cmake` | exact |
| `src/cmake/win32/FindMaya.cmake` | config (Find module) | batch (SDK discovery) | `src/cmake/win32/FindMiles.cmake` + `FindDirectX9.cmake` | exact |
| `src/engine/shared/CMakeLists.txt` (edit) | config (parent chain) | batch | `src/engine/client/CMakeLists.txt` | exact |
| `src/engine/client/CMakeLists.txt` (edit) | config (parent chain) | batch | `src/engine/client/CMakeLists.txt` (self, add line) | exact |
| `src/game/CMakeLists.txt` (edit) | config (parent chain) | batch | `src/engine/client/CMakeLists.txt` | exact |
| `src/external/ours/CMakeLists.txt` (edit) | config (parent chain) | batch | `src/engine/client/CMakeLists.txt` | exact |
| `src/engine/shared/application/CMakeLists.txt` (new) | config (tier parent) | batch | `src/game/client/application/CMakeLists.txt` | exact |
| `src/engine/client/application/CMakeLists.txt` (new) | config (tier parent) | batch | `src/game/client/application/CMakeLists.txt` | exact |
| `src/game/server/CMakeLists.txt` (new/edit) | config (tier parent) | batch | `src/engine/client/CMakeLists.txt` | exact |
| `src/game/server/application/CMakeLists.txt` (new) | config (tier parent) | batch | `src/game/client/application/CMakeLists.txt` | exact |
| `src/external/ours/application/CMakeLists.txt` (new) | config (tier parent) | batch | `src/game/client/application/CMakeLists.txt` | exact |
| Console-EXE tool CMakeLists.txt (LabelHashTool, Md5sum, TreeFileBuilder, TreeFileExtractor, etc.) | config (per-tool) | batch | `src/game/client/application/SwgClient/src/CMakeLists.txt` | role-match |
| WIN32-EXE tool CMakeLists.txt (SwgGodClient, SwgHeadlessClient, SwgConversationEditor, AnimationEditor, etc.) | config (per-tool) | batch | `src/game/client/application/SwgClient/src/CMakeLists.txt` | exact |
| Qt-GUI tool CMakeLists.txt (ParticleEditor, ShipComponentEditor, SoundEditor, NpcEditor, etc.) | config (per-tool, codegen) | batch | `src/game/client/application/SwgClient/src/CMakeLists.txt` + POST_BUILD pattern | role-match (no Qt analog exists yet) |
| DLL tool CMakeLists.txt (MayaExporter, Headless, Direct3d9, DebugWindow, SoePix) | config (per-tool, shared lib) | batch | `src/game/client/application/SwgClient/src/CMakeLists.txt` (structure only) | role-match |
| `sharedStatusWindow/CMakeLists.txt` (new library) | config (library) | batch | `src/engine/shared/library/sharedFoundation/src/CMakeLists.txt` | exact |
| `sharedTemplate/CMakeLists.txt`, `sharedTemplateDefinition/CMakeLists.txt` (new libraries) | config (library) | batch | `src/engine/shared/library/sharedCompression/CMakeLists.txt` + src/CMakeLists.txt | exact |

---

## Pattern Assignments

### `src/cmake/win32/FindQt3.cmake` (config, SDK discovery)

**Analog:** `src/cmake/win32/FindMiles.cmake` (lines 1-14)

**Complete analog — FindMiles.cmake** (lines 1-14):
```cmake
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

**Debug/release library variant pattern** — `src/cmake/win32/FindDPVS.cmake` (lines 11-22):
```cmake
find_library(DPVS_DEBUG_LIBRARY
	NAMES dpvsd
	PATHS ${SWG_EXTERNALS_FIND}/dpvs/lib/win32-x86
)

find_library(DPVS_RELEASE_LIBRARY
	NAMES dpvs
	PATHS ${SWG_EXTERNALS_FIND}/dpvs/lib/win32-x86
)

set(DPVS_INCLUDE_DIRS ${DPVS_INCLUDE_DIR} ${DPVS_IMPLEMENTATION_INCLUDE_DIR})
set(DPVS_LIBRARY debug ${DPVS_DEBUG_LIBRARY} optimized ${DPVS_RELEASE_LIBRARY})
```

**Concrete FindQt3.cmake to author** — copy Miles pattern, adapt paths. Qt 3.3.4 has only a single `qt-mt334.lib` (no debug variant), so no `debug`/`optimized` split needed:
```cmake
# SWG_EXTERNALS_FIND resolves to src/external/3rd/library (set in src/CMakeLists.txt line 24)
set(_QT3_ROOT ${SWG_EXTERNALS_FIND}/qt/3.3.4)

find_path(QT3_INCLUDE_DIR
    PATHS ${_QT3_ROOT}/include
    NAMES qapplication.h
)

find_library(QT3_LIBRARIES
    NAMES qt-mt334
    PATHS ${_QT3_ROOT}/lib
)

set(QT3_BIN_DIR   ${_QT3_ROOT}/bin)
set(QT3_MOC_EXECUTABLE ${QT3_BIN_DIR}/moc.exe)
set(QT3_UIC_EXECUTABLE ${QT3_BIN_DIR}/uic.exe)

# Helper: run moc.exe on headers with Q_OBJECT
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

# Helper: run uic.exe on .ui files — produces both ui_Foo.h AND ui_Foo.cpp (Qt 3 uic)
macro(qt3_wrap_ui outvar)
    foreach(_ui ${ARGN})
        get_filename_component(_base ${_ui} NAME_WE)
        set(_h   "${CMAKE_CURRENT_BINARY_DIR}/ui_${_base}.h")
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

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Qt3 DEFAULT_MSG QT3_INCLUDE_DIR QT3_LIBRARIES)

mark_as_advanced(QT3_INCLUDE_DIR QT3_LIBRARIES)
```

**Key facts:**
- `SWG_EXTERNALS_FIND` = `src/external/3rd/library` — set in `src/CMakeLists.txt` line 24
- `qt-mt334.dll` is already present at both `src/external/3rd/library/qt/3.3.4/lib/qt-mt334.dll` and `exe/win32/qt-mt334.dll`
- The macros live in FindQt3.cmake itself (not a separate file) so `find_package(Qt3)` makes them available to all tool CMakeLists.txt
- Do NOT use `qt5_wrap_cpp` / `qt5_wrap_ui` — those are CMake built-ins for Qt5 only

---

### `src/cmake/win32/FindMaya.cmake` (config, SDK discovery)

**Analog:** `src/cmake/win32/FindDirectX9.cmake` (lines 1-27) — multiple library variant

**Complete analog — FindDirectX9.cmake** (lines 1-27):
```cmake
find_path(DIRECTX9_INCLUDE_DIR
	PATHS ${SWG_EXTERNALS_FIND}/directx9/include
	NAMES d3d9.h d3dx9.h
)

find_library(DIRECTX9_D3D9_LIBRARY NAMES d3d9 PATHS ${SWG_EXTERNALS_FIND}/directx9/lib)
find_library(DIRECTX9_D3DX9_LIBRARY NAMES d3dx9 PATHS ${SWG_EXTERNALS_FIND}/directx9/lib)
...

set(DIRECTX9_LIBRARIES
	${DIRECTX9_D3D9_LIBRARY}
	${DIRECTX9_D3DX9_LIBRARY}
	...
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(DirectX9 DEFAULT_MSG DIRECTX9_INCLUDE_DIR DIRECTX9_D3D9_LIBRARY DIRECTX9_D3DX9_LIBRARY)

mark_as_advanced(...)
```

**Concrete FindMaya.cmake to author** — Maya 8 has multiple import libs (Foundation.lib, OpenMaya.lib, OpenMayaAnim.lib, OpenMayaFX.lib, OpenMayaUI.lib, Image.lib):
```cmake
set(_MAYA8_ROOT ${SWG_EXTERNALS_FIND}/maya8)

find_path(MAYA_INCLUDE_DIR
    PATHS ${_MAYA8_ROOT}/include
    NAMES maya/MFnPlugin.h
)

find_library(MAYA_FOUNDATION_LIBRARY  NAMES Foundation  PATHS ${_MAYA8_ROOT}/lib)
find_library(MAYA_OPENMAYA_LIBRARY    NAMES OpenMaya    PATHS ${_MAYA8_ROOT}/lib)
find_library(MAYA_OPENMAYANIM_LIBRARY NAMES OpenMayaAnim PATHS ${_MAYA8_ROOT}/lib)
find_library(MAYA_IMAGE_LIBRARY       NAMES Image        PATHS ${_MAYA8_ROOT}/lib)

set(MAYA_LIBRARIES
    ${MAYA_FOUNDATION_LIBRARY}
    ${MAYA_OPENMAYA_LIBRARY}
    ${MAYA_OPENMAYANIM_LIBRARY}
    ${MAYA_IMAGE_LIBRARY}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Maya DEFAULT_MSG MAYA_INCLUDE_DIR MAYA_FOUNDATION_LIBRARY)

mark_as_advanced(MAYA_INCLUDE_DIR MAYA_FOUNDATION_LIBRARY MAYA_OPENMAYA_LIBRARY MAYA_OPENMAYANIM_LIBRARY MAYA_IMAGE_LIBRARY)
```

**Key facts:**
- Maya 8 libs are release-only (no debug variant) — use the same libs for Debug and Release builds
- Maya fallback: if Maya 8 headers cause compile errors, substitute `maya8` → `maya7` throughout
- MayaExporter output is a `.mll` file (Maya plugin); set `SUFFIX ".mll"` via `set_target_properties`
- `SWG_EXTERNALS_FIND` = `src/external/3rd/library`

---

### Parent CMakeLists.txt edits — adding `add_subdirectory(application)` (config, parent chain)

**Analog:** `src/engine/client/CMakeLists.txt` (line 1) and `src/game/client/application/CMakeLists.txt` (line 1)

**Current state of each parent and the required edit:**

`src/engine/client/CMakeLists.txt` — currently:
```cmake
add_subdirectory(library)
```
Phase 8 adds:
```cmake
add_subdirectory(library)
add_subdirectory(application)
```

`src/engine/shared/CMakeLists.txt` — currently:
```cmake
add_subdirectory(library)
```
Phase 8 adds:
```cmake
add_subdirectory(library)
add_subdirectory(application)
```

`src/game/CMakeLists.txt` — currently (lines 1-2):
```cmake
add_subdirectory(shared)
add_subdirectory(client)
```
Phase 8 adds:
```cmake
add_subdirectory(shared)
add_subdirectory(client)
add_subdirectory(server)
```

`src/external/ours/CMakeLists.txt` — currently:
```cmake
add_subdirectory(library)
```
Phase 8 adds:
```cmake
add_subdirectory(library)
add_subdirectory(application)
```

`src/game/client/application/CMakeLists.txt` — currently:
```cmake
add_subdirectory(SwgClient)
```
Phase 8 adds:
```cmake
add_subdirectory(SwgClient)
add_subdirectory(SwgGodClient)
add_subdirectory(SwgHeadlessClient)
```

`src/game/server/CMakeLists.txt` — currently empty (file may not exist). Phase 8 creates it:
```cmake
add_subdirectory(application)
```

---

### New tier-parent CMakeLists.txt files (config, tier parent)

**Analog:** `src/game/client/application/CMakeLists.txt` (line 1) — pure list of `add_subdirectory` calls.

**Pattern** — each file is only `add_subdirectory` calls, one per WIRE tool, with SKIP tools noted in comments:

`src/engine/shared/application/CMakeLists.txt` (new):
```cmake
# Wave 1 tools — engine/shared/application/
add_subdirectory(LabelHashTool)
add_subdirectory(Md5sum)
add_subdirectory(TreeFileBuilder)
add_subdirectory(TreeFileExtractor)
add_subdirectory(TreeFileRspBuilder)
add_subdirectory(DataLintRspBuilder)
add_subdirectory(StringFileTool)
add_subdirectory(WordCountTool)
add_subdirectory(UpdateLocalizedStrings)
add_subdirectory(P4Qt)
add_subdirectory(TemplateCompiler)
add_subdirectory(TemplateDefinitionCompiler)
add_subdirectory(Turf)
# SKIP: AddressToLine — no src/ directory
# SKIP: VersionNumber — no src/ directory
# SKIP: hash_plugin — no src/ directory
# SKIP: ArmorExporterTool — serverGame blocker (no CMakeLists.txt; JNI/Oracle dep)
# SKIP: CoreWeaponExporterTool — serverGame blocker
# SKIP: DataTableTool — Linux-only per D-07
# SKIP: WeaponExporterTool — serverGame blocker
# SKIP: PlanetWatcher — serverGame blocker
```

`src/engine/client/application/CMakeLists.txt` (new — currently does not exist):
```cmake
# Wave 2 tools — engine/client/application/
add_subdirectory(AnimationEditor)
add_subdirectory(BugTool)
add_subdirectory(ClientCacheFileBuilder)
add_subdirectory(ClientEffectEditor)
add_subdirectory(CrashReporter)
add_subdirectory(CreateShaderTemplate)
add_subdirectory(DatabaseObjectViewer)
add_subdirectory(DebugWindow)
add_subdirectory(Direct3d9)
add_subdirectory(DllExport)
add_subdirectory(Headless)
add_subdirectory(LightningEditor)
add_subdirectory(MayaExporter)
add_subdirectory(Miff)
add_subdirectory(NpcEditor)
add_subdirectory(ParticleEditor)
add_subdirectory(QuestEditor)
add_subdirectory(RemoteDebugTool)
add_subdirectory(SetBrightnessContrastGamma)
add_subdirectory(ShaderBuilder)
add_subdirectory(ShipComponentEditor)
add_subdirectory(SoePix)
add_subdirectory(SoundEditor)
add_subdirectory(SwooshEditor)
add_subdirectory(TemplateEditor)
add_subdirectory(TerrainEditor)
add_subdirectory(TextureBuilder)
add_subdirectory(UiBuilder)
add_subdirectory(ViewIff)
add_subdirectory(Viewer)
add_subdirectory(WorldSnapshotViewer)
# SKIP: CharacterInfoTool — pure Java JAR; no C++ source
```

`src/game/server/application/CMakeLists.txt` (new):
```cmake
# Wave 3 tools — game/server/application/
add_subdirectory(SwgBattlefieldTool)
add_subdirectory(SwgContentBuilder)
add_subdirectory(SwgConversationEditor)
add_subdirectory(SwgDraftSchematicEditor)
add_subdirectory(SwgNameGenerator)
add_subdirectory(SwgSchematicXmlParser)
add_subdirectory(SwgSpaceQuestEditor)
add_subdirectory(SwgSpaceZoneEditor)
# SKIP: PhonyApp — Linux-only source
# SKIP: SwgDatabaseServer — Oracle OCI blocker (no .lib in repo)
# SKIP: SwgGameServer — JVM/serverGame blocker (no import .lib; serverGame has no CMakeLists.txt)
```

`src/external/ours/application/CMakeLists.txt` (new):
```cmake
# Wave 4 tools — external/ours/application/
add_subdirectory(LocalizationTool)
add_subdirectory(LocalizationToolCon)
# SKIP: LagOMatic — no src/ directory
```

---

### Console-EXE per-tool CMakeLists.txt (e.g., LabelHashTool, Md5sum, TreeFileBuilder)

**Analog:** `src/game/client/application/SwgClient/src/CMakeLists.txt` — structure only (source list, include_directories, add_executable, target_link_libraries, target_precompile_headers)

**Source layout for this pattern:** tool has `src/shared/` and optionally `src/win32/` subdirectories.

**Pattern** (LabelHashTool as representative, lines drawn from SwgClient analog):
```cmake
# src/engine/shared/application/LabelHashTool/CMakeLists.txt

set(SHARED_SOURCES
    src/shared/FirstLabelHashTool.cpp
    src/shared/LabelHashTool.cpp
    src/shared/LabelHashTool.h
)

add_executable(LabelHashTool
    ${SHARED_SOURCES}
)

target_include_directories(LabelHashTool PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shared
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
    src/shared/FirstLabelHashTool.h
)
```

**Key differences from SwgClient:**
- No `WIN32` keyword on `add_executable` (console app)
- No `LINK_FLAGS_DEBUG "/FORCE:MULTIPLE"` — that is SwgClient-only
- No `set_target_properties(... OUTPUT_NAME_DEBUG ...)` unless tool uses it
- `target_include_directories` with `PRIVATE` scope (not bare `include_directories`)
- Link only the libs the tool actually includes; start from SwgClient's list and trim
- `${STLPORT_LIBRARY}` and `legacy_stdio_definitions` appear in all targets
- Tool source root is `CMakeLists.txt` adjacent to `src/` (not nested inside `src/` like SwgClient)

**Variants for cross-platform tools** (shared/ + win32/ layout, e.g. Turf, TemplateCompiler):
```cmake
set(SHARED_SOURCES
    src/shared/FirstTurf.cpp
    src/shared/Turf.cpp
    ...
)

if(WIN32)
    set(PLATFORM_SOURCES
        src/win32/SomePlatformFile.cpp
    )
    target_include_directories(Turf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/win32)
else()
    set(PLATFORM_SOURCES "")
endif()

add_executable(Turf ${SHARED_SOURCES} ${PLATFORM_SOURCES})
```

---

### WIN32-EXE per-tool CMakeLists.txt (SwgGodClient, SwgHeadlessClient, SwgConversationEditor, AnimationEditor)

**Analog:** `src/game/client/application/SwgClient/src/CMakeLists.txt` (lines 75-87)

**WIN32 flag pattern** (lines 75-87 from SwgClient):
```cmake
add_executable(SwgClient WIN32
    ${PLATFORM_SOURCES}
)

set_target_properties(SwgClient PROPERTIES
    OUTPUT_NAME_DEBUG   SwgClient_d
    OUTPUT_NAME_RELEASE SwgClient_r
    LINK_FLAGS_DEBUG "/FORCE:MULTIPLE"   # SwgClient ONLY — do not copy this flag
)
```

**Tool pattern** (without `/FORCE:MULTIPLE`):
```cmake
add_executable(SwgGodClient WIN32
    ${SOURCES}
)

set_target_properties(SwgGodClient PROPERTIES
    OUTPUT_NAME_DEBUG   SwgGodClient_d
    OUTPUT_NAME_RELEASE SwgGodClient_r
)
```

**SwgGodClient flat-src special case** — all sources directly in `src/`, `.ui` files in `src/shared/ui/`:
```cmake
file(GLOB SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/shared/*.cpp"
)
file(GLOB UI_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/shared/ui/*.ui")

target_include_directories(SwgGodClient PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shared
    ${CMAKE_CURRENT_BINARY_DIR}   # for ui_*.h generated files
    ...
)

target_precompile_headers(SwgGodClient PRIVATE
    src/FirstSwgGodClient.h       # flat layout; not src/win32/
)
```

**MFC WIN32 EXE tools** (SwgConversationEditor, SwgDraftSchematicEditor, TerrainEditor, ShaderBuilder, etc.) — add MFC libs matching the `/MT`/`/MTd` root CRT setting:
```cmake
add_executable(SwgConversationEditor WIN32
    ${WIN32_SOURCES}
)

target_link_libraries(SwgConversationEditor PRIVATE
    sharedFoundation
    sharedFile
    sharedUtility
    sharedThread
    sharedCompression
    ${STLPORT_LIBRARY}
    legacy_stdio_definitions
    debug   mfcs140d mfcs140ud   # static MFC debug   — matches /MTd root setting
    optimized mfcs140 mfcs140u   # static MFC release — matches /MT root setting
)
```

---

### Qt-GUI per-tool CMakeLists.txt (ParticleEditor, ShipComponentEditor, SoundEditor, NpcEditor, etc.)

**Analog:** `src/game/client/application/SwgClient/src/CMakeLists.txt` (POST_BUILD pattern, lines 196-237) — no direct Qt analog exists; pattern synthesized from FindQt3.cmake macros + SwgClient POST_BUILD.

**POST_BUILD DLL staging pattern** (SwgClient lines 198-205):
```cmake
add_custom_command(TARGET SwgClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/Mss32.dll" "$<TARGET_FILE_DIR:SwgClient>/Mss32.dll"
    VERBATIM
)
```

**Full Qt tool pattern** (ParticleEditor as representative):
```cmake
# src/engine/client/application/ParticleEditor/CMakeLists.txt

find_package(Qt3 REQUIRED)   # makes qt3_wrap_cpp / qt3_wrap_ui available

file(GLOB WIN32_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/win32/*.cpp")
file(GLOB UI_FILES       "${CMAKE_CURRENT_SOURCE_DIR}/src/win32/*.ui")  # or ui/ subdir — inspect per tool
file(GLOB MOC_HEADERS    "${CMAKE_CURRENT_SOURCE_DIR}/src/win32/*.h")   # filter to Q_OBJECT only (see pitfall 1)

qt3_wrap_ui(GENERATED_UI   ${UI_FILES})
qt3_wrap_cpp(GENERATED_MOC ${MOC_HEADERS})

add_executable(ParticleEditor
    ${WIN32_SOURCES}
    ${GENERATED_UI}
    ${GENERATED_MOC}
)

target_include_directories(ParticleEditor PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}      # ui_*.h lands here
    ${QT3_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/win32
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundation/include/public
    ...
)

target_link_libraries(ParticleEditor PRIVATE
    ${QT3_LIBRARIES}
    sharedFoundation
    sharedDebug
    sharedThread
    ${STLPORT_LIBRARY}
    legacy_stdio_definitions
)

target_precompile_headers(ParticleEditor PRIVATE
    src/win32/FirstParticleEditor.h
)

# Stage Qt runtime DLL — same pattern as Mss32.dll in SwgClient
set(_QT3_DLL "${SWG_EXTERNALS_FIND}/qt/3.3.4/lib/qt-mt334.dll")
add_custom_command(TARGET ParticleEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${_QT3_DLL}" "$<TARGET_FILE_DIR:ParticleEditor>/qt-mt334.dll"
    VERBATIM
)
```

**Critical: Qt 3.3 uic produces two outputs per .ui file.** The `qt3_wrap_ui` macro in FindQt3.cmake must emit both `ui_Foo.h` and `ui_Foo.cpp` and both must be in the source list.

**Critical: moc.exe filter.** Only pass headers that actually contain `Q_OBJECT` to `qt3_wrap_cpp`. Use an explicit list rather than a glob to avoid generating empty moc_ files that cause vtable linker errors.

---

### DLL per-tool CMakeLists.txt (MayaExporter, Headless, Direct3d9, DebugWindow, SoePix, DllExport)

**Analog:** `src/game/client/application/SwgClient/src/CMakeLists.txt` (structure); `set_target_properties` OUTPUT_NAME pattern.

**DLL / shared library pattern:**
```cmake
# MayaExporter — Maya plugin DLL (.mll extension)
find_package(Maya REQUIRED)

file(GLOB WIN32_SOURCES  "${CMAKE_CURRENT_SOURCE_DIR}/src/win32/*.cpp")
file(GLOB SHARED_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/shared/*.cpp")

add_library(MayaExporter SHARED
    ${WIN32_SOURCES}
    ${SHARED_SOURCES}
)

set_target_properties(MayaExporter PROPERTIES
    SUFFIX ".mll"
    OUTPUT_NAME_DEBUG   MayaExporter_d
    OUTPUT_NAME_RELEASE MayaExporter_r
)

target_include_directories(MayaExporter PRIVATE
    ${MAYA_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/win32
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shared
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundation/include/public
    ...
)

target_link_libraries(MayaExporter PRIVATE
    ${MAYA_LIBRARIES}
    sharedFoundation
    sharedFile
    sharedMath
    sharedCollision
    sharedDebug
    sharedThread
    sharedCompression
    sharedImage
    sharedRandom
    clientGraphics
    clientSkeletalAnimation
    sharedStatusWindow
    ${STLPORT_LIBRARY}
    legacy_stdio_definitions
)
```

**Generic DLL pattern** (Headless, DebugWindow — no special suffix):
```cmake
add_library(Headless SHARED
    ${SOURCES}
)
set_target_properties(Headless PROPERTIES
    OUTPUT_NAME_DEBUG   Headless_d
    OUTPUT_NAME_RELEASE Headless_r
)
```

**Direct3d9 naming note:** Do NOT set output name to `gl05_*.dll` in Phase 8. Build as `Direct3d9.dll`. Phase 11 canonicalizes the naming. Do not POST_BUILD stage it over the existing pre-built `gl05_*.dll` files.

---

### New library CMakeLists.txt files (sharedStatusWindow, sharedTemplate, sharedTemplateDefinition)

**Analog:** `src/engine/shared/library/sharedCompression/CMakeLists.txt` (lines 1-8) — top-level library wrapper; and `src/engine/shared/library/sharedFoundation/src/CMakeLists.txt` (lines 1-178) — src-level `add_library(STATIC)` with WIN32 platform split.

**Top-level library CMakeLists.txt** (sharedCompression pattern, lines 1-8):
```cmake
cmake_minimum_required(VERSION 2.8)

project(sharedCompression)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/public)

add_subdirectory(src)
```

**Src-level library CMakeLists.txt** (sharedFoundation src pattern, lines 98-163):
```cmake
set(SHARED_SOURCES
    shared/Foo.cpp
    shared/Foo.h
    ...
)

if(WIN32)
    set(PLATFORM_SOURCES
        win32/FooWin32.cpp
        win32/FooWin32.h
    )
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/win32)
else()
    set(PLATFORM_SOURCES "")
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/shared
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundation/include/public
    ...
)

add_library(sharedStatusWindow STATIC
    ${SHARED_SOURCES}
    ${PLATFORM_SOURCES}
)

target_link_libraries(sharedStatusWindow
    sharedFoundation
    sharedDebug
)

target_precompile_headers(sharedStatusWindow PRIVATE
    shared/FirstSharedStatusWindow.h
)
```

**Notes:**
- `sharedTemplate` has ~64 .cpp files in `shared/` — use explicit list (not glob) matching the existing library pattern
- `sharedTemplateDefinition` has `linux/` directory containing only `dummy.cpp`; on WIN32 skip it (the `if(WIN32)` guard handles this)
- These new library CMakeLists.txt must be added to their parent `engine/shared/library/CMakeLists.txt` via `add_subdirectory(sharedStatusWindow)` etc. — check which libraries are already listed there

---

### Flat-src per-tool CMakeLists.txt (ClientCacheFileBuilder, DatabaseObjectViewer, SetBrightnessContrastGamma, UiBuilder, SoePix, SwgSchematicXmlParser)

**Analog:** `src/game/client/application/SwgClient/src/CMakeLists.txt` (structure only)

**Key difference:** Source files are in the tool root (no `src/` subdirectory). Use `CMAKE_CURRENT_SOURCE_DIR` directly:

```cmake
# Flat-src MFC tool (ClientCacheFileBuilder, DatabaseObjectViewer, etc.)
file(GLOB SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
# Optionally exclude StdAfx.cpp from the glob and add it explicitly as a PCH source:
list(FILTER SOURCES EXCLUDE REGEX ".*StdAfx\\.cpp$")

add_executable(ClientCacheFileBuilder WIN32
    ${SOURCES}
    StdAfx.cpp
)

target_include_directories(ClientCacheFileBuilder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ClientCacheFileBuilder PRIVATE
    # Inspect .cpp includes — most flat MFC tools have no engine lib deps
    legacy_stdio_definitions
    debug   mfcs140d mfcs140ud
    optimized mfcs140 mfcs140u
)

target_precompile_headers(ClientCacheFileBuilder PRIVATE StdAfx.h)
```

---

## Shared Patterns

### Root CMake Variables (inherited by all tools — do not redeclare)

**Source:** `src/CMakeLists.txt` (lines 19-27, 35-46)

```cmake
# These variables are set in src/CMakeLists.txt and available to all subdirectory CMakeLists.txt:
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/win32")  # line 19
set(SWG_ENGINE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/engine)      # line 22
set(SWG_EXTERNALS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external) # line 23
set(SWG_EXTERNALS_FIND ${CMAKE_CURRENT_SOURCE_DIR}/external/3rd/library)  # line 24
set(SWG_OURS ${CMAKE_CURRENT_SOURCE_DIR}/external/ours/library)    # line 25
set(SWG_GAME_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/game)          # line 26

# CRT policy — /MT Debug, /MT Release:
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")  # line 35

# Linker flags — inherited:
set(CMAKE_EXE_LINKER_FLAGS "... /NODEFAULTLIB:libc.lib /NODEFAULTLIB:stlport_vc71_static.lib /SAFESEH:NO")  # line 46

# STLPort headers prepended:
include_directories(BEFORE ${STLPORT_INCLUDE_DIR})  # line 58

# Compat shim prepended:
include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/compat)  # line 88
```

**Do NOT redeclare these in per-tool CMakeLists.txt.** They are inherited via the subdirectory chain.

### STLPort Link Pattern

**Source:** `src/game/client/application/SwgClient/src/CMakeLists.txt` (lines 156-157)

```cmake
target_link_libraries(ToolName PRIVATE
    ...
    ${STLPORT_LIBRARY}           # always last among engine libs; defined in FindSTLPort.cmake
    legacy_stdio_definitions     # always paired with STLPORT_LIBRARY
)
```

`STLPORT_LIBRARY` expands to `stlport_vc143_compat debug stlport_vc71_stldebug_static optimized stlport_vc71_static` (set by `FindSTLPort.cmake` lines 135-139). Include it in every tool that links engine libs.

### POST_BUILD DLL Staging Pattern

**Source:** `src/game/client/application/SwgClient/src/CMakeLists.txt` (lines 196-237)

```cmake
set(SWG_WIN32_DIR "${SWG_ROOT_SOURCE_DIR}/../exe/win32")  # points to exe/win32/

add_custom_command(TARGET ToolName POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/SomeDll.dll" "$<TARGET_FILE_DIR:ToolName>/SomeDll.dll"
    VERBATIM
)
```

Use `$<TARGET_FILE_DIR:ToolName>` (generator expression) so it resolves correctly for both Debug and Release configurations. Use `copy_if_different` (not `copy`) to avoid unnecessary rebuilds.

Apply this pattern for:
- Qt GUI tools: stage `qt-mt334.dll` from `${SWG_EXTERNALS_FIND}/qt/3.3.4/lib/qt-mt334.dll`
- Any tool needing runtime DLLs from `exe/win32/` at the planner's discretion

### target_precompile_headers Pattern

**Source:** `src/game/client/application/SwgClient/src/CMakeLists.txt` (lines 177-179)

```cmake
target_precompile_headers(ToolName PRIVATE
    path/to/FirstToolName.h
)
```

- Path is relative to the CMakeLists.txt location
- For standard layout: `src/win32/FirstToolName.h` or `src/shared/FirstToolName.h`
- For flat-src tools: `StdAfx.h` or `FirstToolName.h`
- For SwgGodClient flat layout: `src/FirstSwgGodClient.h`

### /FORCE:MULTIPLE exclusion rule

**Source:** `src/game/client/application/SwgClient/src/CMakeLists.txt` (lines 83-87)

```cmake
# SwgClient ONLY:
set_target_properties(SwgClient PROPERTIES
    LINK_FLAGS_DEBUG "/FORCE:MULTIPLE"
)
```

Tool CMakeLists.txt MUST NOT include `LINK_FLAGS_DEBUG "/FORCE:MULTIPLE"` unless the tool also links both `stlport_vc71_static.lib` AND `stlport_vc143_compat` simultaneously. No Phase 8 tool requires this.

---

## No Analog Found

All files have close analogs in the codebase. No entries in this section.

The only true "no prior analog" situation is Qt tool CMakeLists.txt — no Qt CMakeLists.txt exists anywhere in the repo yet. However, the FindQt3.cmake macros (`qt3_wrap_cpp`/`qt3_wrap_ui`) synthesize the pattern from D-03 + the existing POST_BUILD DLL staging pattern, so the planner has full concrete instructions above.

---

## Metadata

**Analog search scope:** `src/cmake/win32/`, `src/game/client/application/SwgClient/src/`, `src/engine/shared/library/sharedFoundation/src/`, `src/engine/shared/library/sharedCompression/`, `src/engine/client/CMakeLists.txt`, `src/engine/shared/CMakeLists.txt`, `src/game/client/application/CMakeLists.txt`, `src/game/CMakeLists.txt`, `src/external/ours/CMakeLists.txt`, `src/CMakeLists.txt`

**Files scanned:** 12 files read in full; 8 Find modules inspected

**Pattern extraction date:** 2026-05-07
