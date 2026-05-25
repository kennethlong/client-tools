# Phase 2: Shared Engine Libraries — Pattern Map

**Mapped:** 2026-05-03
**Files analyzed:** 48 (23 library-level `CMakeLists.txt` + 23 `src/CMakeLists.txt` + 1 `src/cmake/stubs/time_t_probe.cpp.in` + 1 `src/engine/shared/library/CMakeLists.txt` update)
**Analogs found:** 47 / 48 (`sharedMemoryManager` has no swg-main reference — authored from scratch)

---

## File Classification

Every file to be created is a CMake build-system file (role: **config**, data flow: **batch** / build orchestration).

| New File | Role | Data Flow | Closest Analog | Match Quality |
|----------|------|-----------|----------------|---------------|
| `src/cmake/stubs/time_t_probe.cpp.in` | config (CMake stub) | batch | `src/engine/shared/library/sharedFoundationTypes/src/CMakeLists.txt` (file(WRITE stub pattern) | role-match |
| `engine/shared/library/CMakeLists.txt` (update) | config | batch | `src/engine/shared/library/CMakeLists.txt` (current Phase 1 file) | exact |
| `sharedDebug/CMakeLists.txt` | config (top-level lib) | batch | `sharedFoundationTypes/CMakeLists.txt` | exact |
| `sharedDebug/src/CMakeLists.txt` | config (src-level lib) | batch | `archive/src/CMakeLists.txt` + sharedFoundationTypes/src (PCH variant) | exact |
| `sharedThread/CMakeLists.txt` | config (top-level lib) | batch | `sharedFoundationTypes/CMakeLists.txt` | exact |
| `sharedThread/src/CMakeLists.txt` | config (src-level lib) | batch | `archive/src/CMakeLists.txt` | exact |
| `sharedSynchronization/CMakeLists.txt` | config (top-level lib) | batch | `sharedFoundationTypes/CMakeLists.txt` | exact |
| `sharedSynchronization/src/CMakeLists.txt` | config (src-level lib) | batch | `archive/src/CMakeLists.txt` | exact |
| `sharedMath/CMakeLists.txt` | config (top-level lib) | batch | `sharedFoundationTypes/CMakeLists.txt` | exact |
| `sharedMath/src/CMakeLists.txt` | config (src-level lib, subdirs) | batch | `archive/src/CMakeLists.txt` | exact |
| `sharedMathArchive/CMakeLists.txt` | config (top-level lib) | batch | `sharedFoundationTypes/CMakeLists.txt` | exact |
| `sharedMathArchive/src/CMakeLists.txt` | config (header-only stub) | batch | `sharedFoundationTypes/src/CMakeLists.txt` | exact |
| `sharedFoundation/CMakeLists.txt` | config (top-level lib) | batch | `sharedFoundationTypes/CMakeLists.txt` | exact |
| `sharedFoundation/src/CMakeLists.txt` | config (src-level lib + time_t probe) | batch | `archive/src/CMakeLists.txt` + probe pattern | exact |
| `sharedFile/CMakeLists.txt` | config (top-level lib) | batch | `sharedFoundationTypes/CMakeLists.txt` | exact |
| `sharedFile/src/CMakeLists.txt` | config (src-level lib) | batch | `archive/src/CMakeLists.txt` | exact |
| `sharedCompression/CMakeLists.txt` | config (top-level lib) | batch | `sharedFoundationTypes/CMakeLists.txt` | exact |
| `sharedCompression/src/CMakeLists.txt` | config (src-level, external dep) | batch | `archive/src/CMakeLists.txt` | exact |
| `sharedRandom/CMakeLists.txt` | config (top-level lib) | batch | `sharedFoundationTypes/CMakeLists.txt` | exact |
| `sharedRandom/src/CMakeLists.txt` | config (src-level lib) | batch | `archive/src/CMakeLists.txt` | exact |
| `sharedRegex/CMakeLists.txt` | config (top-level lib) | batch | `sharedFoundationTypes/CMakeLists.txt` | exact |
| `sharedRegex/src/CMakeLists.txt` | config (src-level, external dep) | batch | `archive/src/CMakeLists.txt` | exact |
| `sharedImage/CMakeLists.txt` | config (top-level lib) | batch | `sharedFoundationTypes/CMakeLists.txt` | exact |
| `sharedImage/src/CMakeLists.txt` | config (src-level lib) | batch | `archive/src/CMakeLists.txt` | exact |
| `sharedLog/CMakeLists.txt` | config (top-level lib) | batch | `sharedFoundationTypes/CMakeLists.txt` | exact |
| `sharedLog/src/CMakeLists.txt` | config (src-level lib) | batch | `archive/src/CMakeLists.txt` | exact |
| `sharedXml/CMakeLists.txt` | config (top-level lib) | batch | `sharedFoundationTypes/CMakeLists.txt` | exact |
| `sharedXml/src/CMakeLists.txt` | config (src-level, external dep, subdir sources) | batch | `archive/src/CMakeLists.txt` | exact |
| `sharedIoWin/CMakeLists.txt` | config (top-level lib) | batch | `sharedFoundationTypes/CMakeLists.txt` | exact |
| `sharedIoWin/src/CMakeLists.txt` | config (src-level lib) | batch | `archive/src/CMakeLists.txt` | exact |
| `sharedMessageDispatch/CMakeLists.txt` | config (top-level lib) | batch | `sharedFoundationTypes/CMakeLists.txt` | exact |
| `sharedMessageDispatch/src/CMakeLists.txt` | config (src-level lib) | batch | `archive/src/CMakeLists.txt` | exact |
| `sharedMemoryManager/CMakeLists.txt` | config (top-level lib) | batch | `sharedFoundationTypes/CMakeLists.txt` | role-match (no swg-main ref) |
| `sharedMemoryManager/src/CMakeLists.txt` | config (src-level lib, win32-only) | batch | `archive/src/CMakeLists.txt` | role-match (no swg-main ref) |
| `sharedNetwork/CMakeLists.txt` | config (top-level lib) | batch | `sharedFoundationTypes/CMakeLists.txt` | exact |
| `sharedNetwork/src/CMakeLists.txt` | config (src-level, win32 socket libs + time_t probe) | batch | `archive/src/CMakeLists.txt` + udplibrary | exact |
| `sharedNetworkMessages/CMakeLists.txt` | config (top-level lib) | batch | `sharedFoundationTypes/CMakeLists.txt` | exact |
| `sharedNetworkMessages/src/CMakeLists.txt` | config (src-level, 338 files, 9 subdirs) | batch | `localization/src/CMakeLists.txt` (multi-set pattern) | exact |
| `sharedObject/CMakeLists.txt` | config (top-level lib) | batch | `sharedFoundationTypes/CMakeLists.txt` | exact |
| `sharedObject/src/CMakeLists.txt` | config (src-level, INTERFACE stub deps) | batch | `localization/src/CMakeLists.txt` | exact |
| `sharedTerrain/CMakeLists.txt` | config (top-level lib) | batch | `sharedFoundationTypes/CMakeLists.txt` | exact |
| `sharedTerrain/src/CMakeLists.txt` | config (src-level, INTERFACE stub deps, 47 files) | batch | `localization/src/CMakeLists.txt` | exact |
| `sharedPathfinding/CMakeLists.txt` | config (top-level lib) | batch | `sharedFoundationTypes/CMakeLists.txt` | exact |
| `sharedPathfinding/src/CMakeLists.txt` | config (src-level lib) | batch | `archive/src/CMakeLists.txt` | exact |
| `sharedGame/CMakeLists.txt` | config (top-level lib) | batch | `sharedFoundationTypes/CMakeLists.txt` | exact |
| `sharedGame/src/CMakeLists.txt` | config (src-level, 139 files, multi-subdir) | batch | `localization/src/CMakeLists.txt` | exact |
| `sharedUtility/CMakeLists.txt` | config (top-level lib) | batch | `sharedFoundationTypes/CMakeLists.txt` | exact |
| `sharedUtility/src/CMakeLists.txt` | config (src-level lib) | batch | `archive/src/CMakeLists.txt` | exact |
| `game/shared/library/swgSharedUtility/CMakeLists.txt` | config (top-level lib, game/ subtree) | batch | `sharedFoundationTypes/CMakeLists.txt` | exact |
| `game/shared/library/swgSharedUtility/src/CMakeLists.txt` | config (src-level lib) | batch | `archive/src/CMakeLists.txt` | exact |

All paths under `src/engine/shared/library/<lib>/` unless otherwise noted.

---

## Pattern Assignments

### PATTERN A: Top-Level `<lib>/CMakeLists.txt`

**Analog:** `D:/Code/swg-client/src/engine/shared/library/sharedFoundationTypes/CMakeLists.txt` (lines 1–7)

**Copy this for ALL 23 `engine/shared/library` top-level CMakeLists + swgSharedUtility:**

```cmake
cmake_minimum_required(VERSION 2.8)

project(<LibName>)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/public)

add_subdirectory(src)
```

Substitute `<LibName>` with the library name exactly (e.g., `sharedDebug`, `sharedFoundation`, `swgSharedUtility`).

**Verified:** Every one of the 23 `engine/shared/library/<lib>/` directories has an `include/public` subdirectory (confirmed via directory scan). The `include_directories` line is always correct.

---

### PATTERN B: Standard `src/CMakeLists.txt` — Compiled Lib with SHARED_SOURCES + PLATFORM_SOURCES

**Analog:** `D:/Code/swg-client/src/external/ours/library/archive/src/CMakeLists.txt` (lines 1–47)

**Core pattern (copy and adapt file lists and include paths):**

```cmake
set(SHARED_SOURCES
    shared/Foo.cpp
    shared/Foo.h
    shared/FirstSharedFoo.h
    # ... enumerate every .cpp/.h in src/shared/ and its subdirectories
)

if(WIN32)
    set(PLATFORM_SOURCES
        win32/Bar.cpp
        win32/Bar.h
        win32/FirstSharedFoo.cpp
    )

    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/win32)
else()
    set(PLATFORM_SOURCES
        linux/Bar.cpp
        linux/Bar.h
    )

    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/linux)
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/shared
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundation/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundationTypes/include/public
    # ... add include paths for each direct dependency
)

add_library(<LibName> STATIC
    ${SHARED_SOURCES}
    ${PLATFORM_SOURCES}
)

target_link_libraries(<LibName>
    sharedFoundation
    # ... add each dep from the dependency graph table below
)

target_precompile_headers(<LibName> PRIVATE
    shared/FirstShared<LibName>.h
)
```

**Key rules:**
- `SHARED_SOURCES` lists files relative to `${CMAKE_CURRENT_SOURCE_DIR}` (i.e., `shared/Foo.cpp`, not absolute paths).
- For libraries with nested subdirectories (sharedMath, sharedGame, sharedObject, sharedTerrain, sharedNetworkMessages), create a separate named set per subdirectory (e.g., `DYNAMICS_SOURCES`, `CORE_SOURCES`) then combine in `add_library`.
- `win32/First<LibName>.cpp` is always the sole Win32 platform source unless the library has additional Win32-specific files.
- `target_precompile_headers` always references the `shared/First<LibName>.h` form (not `win32/`), per D-10.

---

### PATTERN C: Header-Only Lib — Stub `.cpp` Pattern

**Analog:** `D:/Code/swg-client/src/engine/shared/library/sharedFoundationTypes/src/CMakeLists.txt` (lines 1–27)

**Apply to:** `sharedMathArchive/src/CMakeLists.txt`

```cmake
set(SHARED_SOURCES
    shared/FirstSharedMathArchive.h
    shared/QuaternionArchive.h
    shared/SphereArchive.h
    shared/TransformArchive.h
    shared/VectorArchive.h
)

# sharedMathArchive is WIN32-only (no linux/ sources)
# PLATFORM_SOURCES stays empty on all platforms

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/shared
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedMath/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundationTypes/include/public
)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/sharedMathArchive_stub.cpp
    "// Generated by CMake so the header-only sharedMathArchive target emits a .lib.\n")

add_library(sharedMathArchive STATIC
    ${SHARED_SOURCES}
    ${CMAKE_CURRENT_BINARY_DIR}/sharedMathArchive_stub.cpp
)

set_target_properties(sharedMathArchive PROPERTIES LINKER_LANGUAGE CXX)

target_precompile_headers(sharedMathArchive PRIVATE
    shared/FirstSharedMathArchive.h
)
```

No `target_link_libraries` needed (header-only, all deps are include-path-only).

---

### PATTERN D: time_t Probe — `configure_file` Injection

**Analog:** RESEARCH.md §Code Examples — "time_t Probe Pattern" (D-07/D-08/D-09)

**Apply to:** `sharedDebug/src/CMakeLists.txt`, `sharedFoundation/src/CMakeLists.txt`, `sharedNetwork/src/CMakeLists.txt`

**Step 1 — Create the template file once** at `src/cmake/stubs/time_t_probe.cpp.in`:

```cpp
// Generated by CMake — verifies _USE_32BIT_TIME_T=1 reaches this TU
#include <ctime>
static_assert(sizeof(time_t) == 4, "_USE_32BIT_TIME_T=1 not reaching this TU");
```

**Step 2 — In each of the three probe targets' `src/CMakeLists.txt`**, append after the `add_library` call:

```cmake
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/stubs/time_t_probe.cpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/time_t_probe.cpp
    @ONLY
)
target_sources(<LibName> PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/time_t_probe.cpp)
```

Note: `CMAKE_SOURCE_DIR` resolves to `D:/Code/swg-client/src` (the root CMakeLists.txt location). The path `cmake/stubs/time_t_probe.cpp.in` is relative to that root.

---

### PATTERN E: INTERFACE Stub Targets

**Apply to:** `src/engine/shared/library/CMakeLists.txt` — add before all `add_subdirectory` calls

**Analog:** RESEARCH.md §Code Examples — "INTERFACE Stub Target Pattern"

```cmake
# Out-of-scope INTERFACE stubs — expose include paths without building source.
# These satisfy configure-time target validation for sharedObject, sharedTerrain,
# sharedGame, and sharedNetworkMessages without pulling Phase 3+ libs into scope.

add_library(sharedCollision INTERFACE)
target_include_directories(sharedCollision INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/sharedCollision/include/public)

add_library(sharedFractal INTERFACE)
target_include_directories(sharedFractal INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/sharedFractal/include/public)

add_library(sharedSkillSystem INTERFACE)
target_include_directories(sharedSkillSystem INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/sharedSkillSystem/include/public)
```

These three lines must appear **before** the `add_subdirectory(sharedObject)`, `add_subdirectory(sharedTerrain)`, and `add_subdirectory(sharedGame)` calls.

---

### PATTERN F: Multi-Subdirectory Source Enumeration

**Analog:** `D:/Code/swg-client/src/external/ours/library/localization/src/CMakeLists.txt` (multiple named source sets)

**Apply to:** `sharedNetworkMessages`, `sharedGame`, `sharedObject`, `sharedTerrain`, `sharedMath`

Pattern for libraries with sources spread across subdirectories:

```cmake
set(SHARED_SOURCES
    shared/TopLevelFile.cpp
    shared/TopLevelFile.h
)

set(SUBDIR_A_SOURCES
    shared/subdirA/FileA.cpp
    shared/subdirA/FileA.h
)

set(SUBDIR_B_SOURCES
    shared/subdirB/FileB.cpp
    shared/subdirB/FileB.h
)

# ... one set per subdirectory

add_library(<LibName> STATIC
    ${SHARED_SOURCES}
    ${SUBDIR_A_SOURCES}
    ${SUBDIR_B_SOURCES}
    ${PLATFORM_SOURCES}
)
```

For `sharedNetworkMessages` specifically: 9 subdirectory sets are needed:
- `SHARED_SOURCES` — `src/shared/` root (7 .cpp files)
- `CHAT_SOURCES` — `src/shared/chat/` (71 .cpp files)
- `CLIENT_GAME_SERVER_SOURCES` — `src/shared/clientGameServer/` (200 .cpp files)
- `CLIENT_LOGIN_SERVER_SOURCES` — `src/shared/clientLoginServer/` (4 .cpp files)
- `COMMON_SOURCES` — `src/shared/common/` (15 .cpp files)
- `CORE_SOURCES` — `src/shared/core/` (1 .cpp file)
- `CUSTOMER_SERVICE_SOURCES` — `src/shared/customerService/` (32 .cpp files)
- `PLANET_WATCH_SOURCES` — `src/shared/planetWatch/` (5 .cpp files)
- `VOICE_CHAT_SOURCES` — `src/shared/voicechat/` (3 .cpp files)
- `PLATFORM_SOURCES` — `src/win32/FirstSharedNetworkMessages.cpp`

---

### PATTERN G: External Dependency Linkage

**Analog:** `D:/Code/swg-client/src/external/3rd/library/udplibrary/CMakeLists.txt` + `D:/Code/swg-client/src/CMakeLists.txt` (find_package variable names)

**Three libraries need external dep linkage:**

**sharedCompression/src/CMakeLists.txt** — zlib:
```cmake
target_link_libraries(sharedCompression
    ${ZLIB_LIBRARY}
)
# include path:
include_directories(${ZLIB_INCLUDE_DIR})
```

**sharedRegex/src/CMakeLists.txt** — PCRE:
```cmake
target_link_libraries(sharedRegex
    ${PCRE_LIBRARY}
)
# include path:
include_directories(${PCRE_INCLUDE_DIR})
```

**sharedXml/src/CMakeLists.txt** — libxml2 (Win32: no iconv):
```cmake
target_link_libraries(sharedXml
    ${LIBXML2_LIBRARY}
)
# include path — guard iconv for Win32:
include_directories(${LIBXML2_INCLUDE_DIR})
if(NOT WIN32)
    include_directories(${ICONV_INCLUDE_DIR})
endif()
```

**sharedNetwork/src/CMakeLists.txt** — udplibrary + Windows socket libs:
```cmake
target_link_libraries(sharedNetwork
    sharedFoundation
    sharedLog
    sharedMessageDispatch
    udplibrary
)
if(WIN32)
    target_link_libraries(sharedNetwork mswsock ws2_32)
endif()
```

Variable names `ZLIB_LIBRARY`, `ZLIB_INCLUDE_DIR`, `PCRE_LIBRARY`, `PCRE_INCLUDE_DIR`, `LIBXML2_LIBRARY`, `LIBXML2_INCLUDE_DIR` are set by Phase 1 Find modules at `src/cmake/win32/Find*.cmake`.

---

### PATTERN H: sharedMemoryManager (No swg-main Reference — Authored from Scratch)

**Analog:** `D:/Code/swg-client/src/external/ours/library/archive/src/CMakeLists.txt` (closest role-match)

**Source inventory** (from RESEARCH.md §sharedMemoryManager):
- `shared/FirstSharedMemoryManager.cpp` and `.h`
- `shared/MemoryManager.cpp` and `.h`
- `win32/OsMemory.cpp` and `.h`
- `win32/OsNewDel.cpp` and `.h`

**Inferred deps** (from `FirstSharedMemoryManager.h` includes per RESEARCH.md):
- `sharedFoundationTypes` (include path only)
- `sharedDebug` (include path only)
- `sharedFoundation` (include path only)

No `target_link_libraries` deps needed at compile time (pure include deps).

```cmake
set(SHARED_SOURCES
    shared/FirstSharedMemoryManager.cpp
    shared/FirstSharedMemoryManager.h
    shared/MemoryManager.cpp
    shared/MemoryManager.h
)

if(WIN32)
    set(PLATFORM_SOURCES
        win32/OsMemory.cpp
        win32/OsMemory.h
        win32/OsNewDel.cpp
        win32/OsNewDel.h
    )

    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/win32)
else()
    set(PLATFORM_SOURCES "")
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/shared
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundationTypes/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedDebug/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundation/include/public
)

add_library(sharedMemoryManager STATIC
    ${SHARED_SOURCES}
    ${PLATFORM_SOURCES}
)

target_precompile_headers(sharedMemoryManager PRIVATE
    shared/FirstSharedMemoryManager.h
)
```

---

### PATTERN I: swgSharedUtility — Game-Subtree Lib

**Analog:** `D:/Code/swg-client/src/external/ours/library/archive/src/CMakeLists.txt` (same pattern, different path variables)

**Top-level `src/game/shared/library/swgSharedUtility/CMakeLists.txt`:**

```cmake
cmake_minimum_required(VERSION 2.8)

project(swgSharedUtility)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/public)

add_subdirectory(src)
```

**`src/game/shared/library/swgSharedUtility/src/CMakeLists.txt`:**

```cmake
set(SHARED_SOURCES
    shared/Attributes.cpp
    shared/Attributes.def
    shared/Attributes.h
    shared/Behaviors.def
    shared/CombatEngineData.cpp
    shared/CombatEngineData.h
    shared/FirstSwgSharedUtility.h
    shared/JediConstants.cpp
    shared/JediConstants.h
    shared/Locomotions.cpp
    shared/Locomotions.def
    shared/Locomotions.h
    shared/MentalStates.def
    shared/Postures.cpp
    shared/Postures.def
    shared/Postures.h
    shared/SpeciesRestrictions.cpp
    shared/SpeciesRestrictions.h
    shared/States.cpp
    shared/States.def
    shared/States.h
)

if(WIN32)
    set(PLATFORM_SOURCES
        win32/FirstSwgSharedUtility.cpp
    )

    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/win32)
else()
    set(PLATFORM_SOURCES "")
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/shared
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundation/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundationTypes/include/public
)

add_library(swgSharedUtility STATIC
    ${SHARED_SOURCES}
    ${PLATFORM_SOURCES}
)

target_precompile_headers(swgSharedUtility PRIVATE
    shared/FirstSwgSharedUtility.h
)
```

Note: swg-main's swgSharedUtility has no `target_link_libraries` declaration. Uses `SWG_ENGINE_SOURCE_DIR` (defined in root CMakeLists.txt as `src/engine`) since `SWG_GAME_SOURCE_DIR` is `src/game`.

The `src/game/shared/library/` directory needs a `CMakeLists.txt` that adds `swgSharedUtility`, and the game subtree needs to be wired into the root `src/CMakeLists.txt` via `add_subdirectory(game)`.

---

## Dependency Graph Reference Table

The executor must add the following `target_link_libraries` entries per RESEARCH.md §Dependency Graph (verified from swg-main):

| Library | target_link_libraries | Notes |
|---------|----------------------|-------|
| sharedDebug | *(none)* | Leaf lib |
| sharedSynchronization | *(none)* | Leaf lib |
| sharedThread | `sharedSynchronization` + `${CMAKE_THREAD_LIBS_INIT}` | Thread lib from CMake |
| sharedMathArchive | *(none)* | Header-only stub, no link deps |
| sharedRandom | *(none)* | Leaf lib |
| sharedMath | `sharedFile sharedRandom` | Circular include with sharedFile resolved at link level |
| sharedCompression | `${ZLIB_LIBRARY}` | External dep |
| sharedFile | `sharedCompression sharedDebug sharedFoundation sharedMath fileInterface` | |
| sharedFoundation | `sharedFile archive localization localizationArchive unicode unicodeArchive` | |
| sharedImage | *(none)* | Leaf lib |
| sharedIoWin | *(none)* | Leaf lib |
| sharedMessageDispatch | *(none)* | Leaf lib |
| sharedMemoryManager | *(none)* | Leaf lib (inferred — no swg-main ref) |
| sharedRegex | `${PCRE_LIBRARY}` | External dep |
| sharedXml | `${LIBXML2_LIBRARY}` | External dep |
| sharedLog | `sharedNetworkMessages` | One-way dep; not circular at link level |
| sharedNetwork | `sharedFoundation sharedLog sharedMessageDispatch udplibrary` + `mswsock ws2_32` (WIN32) | |
| sharedNetworkMessages | `sharedUtility localization localizationArchive unicode unicodeArchive` | |
| sharedUtility | `sharedFoundation sharedMath` | |
| sharedObject | `sharedCollision sharedFoundation sharedGame sharedTerrain sharedUtility swgSharedUtility` | sharedCollision is INTERFACE stub |
| sharedTerrain | `sharedCollision sharedFractal` | Both are INTERFACE stubs |
| sharedPathfinding | *(none — include-only deps)* | No link deps |
| sharedGame | `sharedFoundation sharedObject sharedUtility archive` | |
| swgSharedUtility | *(none)* | |

---

## Include Path Reference Table

The executor adds `include_directories` for each library's direct dependencies. Use `SWG_ENGINE_SOURCE_DIR` (= `src/engine`) and `SWG_EXTERNALS_SOURCE_DIR` (= `src/external`) and `SWG_GAME_SOURCE_DIR` (= `src/game`):

```
sharedFoundation/include/public     →  ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundation/include/public
sharedFoundationTypes/include/public →  ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundationTypes/include/public
sharedDebug/include/public           →  ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedDebug/include/public
sharedFile/include/public            →  ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFile/include/public
sharedMath/include/public            →  ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedMath/include/public
sharedNetwork/include/public         →  ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedNetwork/include/public
... (same pattern for all Phase 2 libs)
swgSharedUtility/include/public      →  ${SWG_GAME_SOURCE_DIR}/shared/library/swgSharedUtility/include/public
archive (include path)               →  ${SWG_EXTERNALS_SOURCE_DIR}/ours/library/archive/include
fileInterface (include path)         →  ${SWG_EXTERNALS_SOURCE_DIR}/ours/library/fileInterface/include/public
localization (include path)          →  ${SWG_EXTERNALS_SOURCE_DIR}/ours/library/localization/include/public
unicode (include path)               →  ${SWG_EXTERNALS_SOURCE_DIR}/ours/library/unicode/include
udplibrary (include path)            →  ${SWG_EXTERNALS_SOURCE_DIR}/3rd/library/udplibrary
vtune (include path, sharedDebug)    →  ${SWG_EXTERNALS_SOURCE_DIR}/3rd/library/vtune
```

---

## Shared Patterns (Cross-Cutting)

### PCH Wiring — Applied to All 23 Engine Libs + swgSharedUtility

**Source:** `D:/Code/swg-client/src/engine/shared/library/sharedFoundationTypes/src/CMakeLists.txt` (PCH not wired here — sharedFoundationTypes is header-only).
**Reference pattern from:** RESEARCH.md §Architecture Patterns — src/CMakeLists Pattern

```cmake
target_precompile_headers(<LibName> PRIVATE
    shared/First<LibName>.h
)
```

Rule: Always `shared/First<LibName>.h`, never `win32/First<LibName>.cpp`. Applied after `add_library` and `target_link_libraries`.

### Global Flags — Already Propagated from Root

**Source:** `D:/Code/swg-client/src/CMakeLists.txt` lines 43–44

Flags `/D_USE_32BIT_TIME_T=1`, `/DWIN32`, `/D_MBCS`, `/Zc:wchar_t-`, `/D_CRT_SECURE_NO_WARNINGS`, STLPort include are all set globally. Individual `src/CMakeLists.txt` files do NOT re-declare these — they inherit from root via CMake's flag propagation. The time_t probe (PATTERN D) verifies this inheritance.

### Existing Phase 1 Root CMakeLists Variables

**Source:** `D:/Code/swg-client/src/CMakeLists.txt` lines 21–26

Variables guaranteed to be defined:
```cmake
SWG_ROOT_SOURCE_DIR    = D:/Code/swg-client/src
SWG_ENGINE_SOURCE_DIR  = D:/Code/swg-client/src/engine
SWG_EXTERNALS_SOURCE_DIR = D:/Code/swg-client/src/external
SWG_EXTERNALS_FIND     = D:/Code/swg-client/src/external/3rd/library
SWG_OURS               = D:/Code/swg-client/src/external/ours/library
SWG_GAME_SOURCE_DIR    = D:/Code/swg-client/src/game
```

Use these variables — never hardcode paths relative to the lib directory.

### Static MSVC Runtime — Already Set

**Source:** `D:/Code/swg-client/src/CMakeLists.txt` line 35

```cmake
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
```

Individual libs do NOT set CRT policy — it is inherited from root. This sets `/MT` for Release, `/MTd` for Debug globally.

---

## Files With No Close Analog

| File | Role | Data Flow | Reason |
|------|------|-----------|--------|
| `src/cmake/stubs/time_t_probe.cpp.in` | config (CMake template) | batch | No existing CMake `configure_file` templates in whitengold tree; pattern taken from RESEARCH.md D-07 |
| `sharedMemoryManager/src/CMakeLists.txt` | config (src-level lib) | batch | sharedMemoryManager not present in swg-main; deps inferred from header inspection |

Both have sufficient guidance from RESEARCH.md to author without a codebase analog.

---

## `src/engine/shared/library/CMakeLists.txt` — Required Update

**Current content** (`D:/Code/swg-client/src/engine/shared/library/CMakeLists.txt` lines 1–6):

```cmake
if(WHITENGOLD_USE_STLPORT_HEADERS)
    include_directories(BEFORE ${STLPORT_INCLUDE_DIR})
endif()

add_subdirectory(sharedFoundationTypes)
```

**Required Phase 2 update** — add INTERFACE stubs then all 23 `add_subdirectory` calls after the existing Phase 1 content. The executor must preserve the Phase 1 lines and append:

1. INTERFACE stubs block (PATTERN E) — before any Phase 2 `add_subdirectory` calls
2. `add_subdirectory(sharedDebug)` through `add_subdirectory(sharedUtility)` — in dependency order (Tier 2 first, Tier 5 last)
3. `add_subdirectory(sharedMemoryManager)` in Tier 2

The root `src/CMakeLists.txt` also needs `add_subdirectory(game)` to wire `swgSharedUtility` into the build graph. This requires creating:
- `src/game/CMakeLists.txt` → `add_subdirectory(shared)`
- `src/game/shared/CMakeLists.txt` → `add_subdirectory(library)`
- `src/game/shared/library/CMakeLists.txt` → `add_subdirectory(swgSharedUtility)`

---

## Metadata

**Analog search scope:** `D:/Code/swg-client/src/engine/shared/library/`, `D:/Code/swg-client/src/external/ours/library/`, `D:/Code/swg-client/src/external/3rd/library/`, `D:/Code/swg-client/src/`
**Files scanned:** 9 CMakeLists.txt files read directly; directory listings for all 23 libs + swgSharedUtility
**Pattern extraction date:** 2026-05-03
