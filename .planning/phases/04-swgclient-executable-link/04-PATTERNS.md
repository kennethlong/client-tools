# Phase 4: SwgClient Executable Link — Pattern Map

**Mapped:** 2026-05-04
**Files analyzed:** 12 new/modified CMakeLists files + 1 stub config file
**Analogs found:** 12 / 12 (all Phase 4 files have Phase 2/3 analogs; no novel patterns required)

---

## File Classification

| New / Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---------------------|------|-----------|----------------|---------------|
| `src/game/CMakeLists.txt` (modify) | config — aggregator | N/A | `src/engine/CMakeLists.txt` | exact |
| `src/game/client/CMakeLists.txt` (new) | config — aggregator | N/A | `src/engine/client/CMakeLists.txt` | exact |
| `src/game/client/library/CMakeLists.txt` (new) | config — aggregator | N/A | `src/engine/client/library/CMakeLists.txt` | exact |
| `src/game/client/library/swgClientUserInterface/CMakeLists.txt` (new) | config — top-level lib wrapper | N/A | `src/engine/client/library/clientUserInterface/CMakeLists.txt` | exact |
| `src/game/client/library/swgClientUserInterface/src/CMakeLists.txt` (new) | config — large multi-subdir build target, 266 cpp | N/A | `src/engine/client/library/clientUserInterface/src/CMakeLists.txt` | exact |
| `src/game/client/application/CMakeLists.txt` (new) | config — aggregator | N/A | `src/engine/client/CMakeLists.txt` | exact |
| `src/game/client/application/SwgClient/CMakeLists.txt` (new) | config — top-level app wrapper | N/A | `src/engine/client/library/clientGame/CMakeLists.txt` | exact |
| `src/game/client/application/SwgClient/src/CMakeLists.txt` (new) | config — WIN32 executable, full link closure | N/A | `src/engine/client/library/clientGame/src/CMakeLists.txt` (link block) + RESEARCH.md Pattern 3 | role-match |
| `src/game/shared/library/CMakeLists.txt` (modify) | config — aggregator | N/A | `src/engine/shared/library/CMakeLists.txt` | exact |
| `src/game/shared/library/swgSharedNetworkMessages/CMakeLists.txt` (new) | config — top-level lib wrapper | N/A | `src/game/shared/library/swgSharedUtility/CMakeLists.txt` | exact |
| `src/game/shared/library/swgSharedNetworkMessages/src/CMakeLists.txt` (new) | config — small SHARED+PLATFORM static lib, 10 cpp | N/A | `src/game/shared/library/swgSharedUtility/src/CMakeLists.txt` | exact |
| `src/external/3rd/library/libEverQuestTCG/CMakeLists.txt` (new) | config — tiny 3rd-party STATIC lib, 1 cpp | N/A | `src/engine/shared/library/sharedFoundation/CMakeLists.txt` (PATTERN A) | exact |
| `src/external/3rd/library/libEverQuestTCG/src/CMakeLists.txt` (new) | config — win32-only single-cpp static lib | N/A | `src/engine/client/library/clientBugReporting/src/CMakeLists.txt` (win32-only variant) | role-match |

---

## Pattern Assignments

### PATTERN A — Top-Level Library / App CMakeLists.txt (identical to Phase 2/3 pattern)

**Analog:** `src/engine/client/library/clientUserInterface/CMakeLists.txt` (lines 1–7)

Every library or application wrapper at `<name>/CMakeLists.txt` is identical except for the project name:

```cmake
cmake_minimum_required(VERSION 2.8)

project(<Name>)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/public)

add_subdirectory(src)
```

Apply verbatim to:
- `src/game/client/library/swgClientUserInterface/CMakeLists.txt` — `project(swgClientUserInterface)`
- `src/game/shared/library/swgSharedNetworkMessages/CMakeLists.txt` — `project(swgSharedNetworkMessages)`
- `src/external/3rd/library/libEverQuestTCG/CMakeLists.txt` — `project(libEverQuestTCG)`, dir is `include/public`
- `src/game/client/application/SwgClient/CMakeLists.txt` — `project(SwgClient)`, **omit `include_directories`** (exe has no public headers)

**Note for `libEverQuestTCG`:** The `include/public` path exists at `src/external/3rd/library/libEverQuestTCG/include/public/libEverQuestTCG/`. The pattern holds exactly.

**Note for `SwgClient` application:** The application has no `include/public` (it is not a library). The wrapper CMakeLists is simply:

```cmake
cmake_minimum_required(VERSION 2.8)
project(SwgClient)
add_subdirectory(src)
```

---

### PATTERN B — Aggregator CMakeLists.txt (passthrough to subdirs)

**Analog:** `src/engine/CMakeLists.txt` (1-liner), `src/engine/client/CMakeLists.txt` (1-liner), `src/game/shared/CMakeLists.txt` (1-liner)

All aggregator CMakeLists files that merely descend into child directories:

```cmake
add_subdirectory(<child>)
```

**`src/game/CMakeLists.txt` (modify — add one line):**

Current content (line 1):
```cmake
add_subdirectory(shared)
```

Modified content:
```cmake
add_subdirectory(shared)
add_subdirectory(client)    # Phase 4 — game-side client libs + SwgClient exe
```

**`src/game/client/CMakeLists.txt` (new):**
```cmake
add_subdirectory(library)
add_subdirectory(application)
```

**`src/game/client/library/CMakeLists.txt` (new):**
```cmake
add_subdirectory(swgClientUserInterface)
```

**`src/game/client/application/CMakeLists.txt` (new):**
```cmake
add_subdirectory(SwgClient)
```

**`src/game/shared/library/CMakeLists.txt` (modify — add one line):**

Current content (line 1):
```cmake
add_subdirectory(swgSharedUtility)
```

Modified content:
```cmake
add_subdirectory(swgSharedNetworkMessages)    # Phase 4 — game-side shared network messages
add_subdirectory(swgSharedUtility)
```

**Note:** `swgSharedNetworkMessages` is declared before `swgSharedUtility` so the lib is available when the client subtree configures later.

---

### PATTERN C — swgSharedNetworkMessages src/CMakeLists.txt (small SHARED+PLATFORM static lib)

**Primary analog:** `src/game/shared/library/swgSharedUtility/src/CMakeLists.txt` (lines 1–52)
**Secondary analog:** `src/engine/client/library/clientUserInterface/src/CMakeLists.txt` (SHARED/PLATFORM split structure)

**Verified source files** (10 cpp total, 5 shared subdirs + 1 win32):

```
src/shared/core/SetupSwgSharedNetworkMessages.cpp    (+ .h)
src/shared/combat/CombatActionCompleteMessage.cpp    (+ .h)
src/shared/combat/MessageQueueCombatAction.cpp       (+ .h)
src/shared/combat/MessageQueueCombatDamage.cpp       (+ .h)
src/shared/consent/ConsentRequestMessage.cpp         (+ .h)
src/shared/permissionList/PermissionListCreateMessage.cpp  (+ .h)
src/shared/permissionList/PermissionListModifyMessage.cpp  (+ .h)
src/shared/survey/ResourceListForSurveyMessage.cpp   (+ .h)
src/shared/survey/SurveyMessage.cpp                  (+ .h)
src/win32/FirstSwgSharedNetworkMessages.cpp
```

**PCH header:** `src/shared/core/FirstSwgSharedNetworkMessages.h` (verified at `src/game/shared/library/swgSharedNetworkMessages/src/shared/core/FirstSwgSharedNetworkMessages.h`)

**Full src/CMakeLists.txt pattern:**

```cmake
set(SHARED_SOURCES
    shared/core/FirstSwgSharedNetworkMessages.h
    shared/core/SetupSwgSharedNetworkMessages.cpp
    shared/core/SetupSwgSharedNetworkMessages.h

    shared/combat/CombatActionCompleteMessage.cpp
    shared/combat/CombatActionCompleteMessage.h
    shared/combat/MessageQueueCombatAction.cpp
    shared/combat/MessageQueueCombatAction.h
    shared/combat/MessageQueueCombatDamage.cpp
    shared/combat/MessageQueueCombatDamage.h

    shared/consent/ConsentRequestMessage.cpp
    shared/consent/ConsentRequestMessage.h

    shared/permissionList/PermissionListCreateMessage.cpp
    shared/permissionList/PermissionListCreateMessage.h
    shared/permissionList/PermissionListModifyMessage.cpp
    shared/permissionList/PermissionListModifyMessage.h

    shared/survey/ResourceListForSurveyMessage.cpp
    shared/survey/ResourceListForSurveyMessage.h
    shared/survey/SurveyMessage.cpp
    shared/survey/SurveyMessage.h
)

if(WIN32)
    set(PLATFORM_SOURCES
        win32/FirstSwgSharedNetworkMessages.cpp
    )
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/win32)
else()
    set(PLATFORM_SOURCES "")
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/shared
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/core
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedNetworkMessages/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundation/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundationTypes/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedMath/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedObject/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedGame/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedDebug/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedUtility/include/public
    ${SWG_EXTERNALS_SOURCE_DIR}/ours/library/archive/include
    ${SWG_EXTERNALS_SOURCE_DIR}/ours/library/unicode/include
)

add_library(swgSharedNetworkMessages STATIC
    ${SHARED_SOURCES}
    ${PLATFORM_SOURCES}
)

target_link_libraries(swgSharedNetworkMessages
    sharedNetworkMessages
    sharedFoundation
    sharedMath
    archive
)

target_compile_options(swgSharedNetworkMessages PRIVATE /wd4530)

target_precompile_headers(swgSharedNetworkMessages PRIVATE
    shared/core/FirstSwgSharedNetworkMessages.h
)
```

**Deviations from analog (`swgSharedUtility/src/CMakeLists.txt`):**
- `swgSharedUtility` uses `${SWG_OURS}/localization/include` — `swgSharedNetworkMessages` uses `archive` instead
- `swgSharedNetworkMessages` adds `target_link_libraries` and `target_compile_options` (swgSharedUtility had neither — it has no link deps in the current authored version)
- PCH path is `shared/core/FirstSwgSharedNetworkMessages.h` (in `core/` subdir), not `shared/FirstSwgSharedUtility.h` (flat)

---

### PATTERN D — libEverQuestTCG src/CMakeLists.txt (win32-only single-cpp STATIC lib)

**Primary analog:** `src/engine/client/library/clientBugReporting/src/CMakeLists.txt` (win32-only PLATFORM_SOURCES, no SHARED_SOURCES cpp)

**Verified source files** (1 cpp + 1 private h in win32/, 1 public h in include/public/):
```
src/win32/libEverQuestTCG.cpp     — sole implementation file
src/win32/libEverQuestTCG.h       — private impl header
include/public/libEverQuestTCG/libEverQuestTCG.h  — public API header (referenced via top-level include_directories in PATTERN A)
```

**Full src/CMakeLists.txt pattern:**

```cmake
if(WIN32)
    set(PLATFORM_SOURCES
        win32/libEverQuestTCG.cpp
        win32/libEverQuestTCG.h
    )
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/win32)
else()
    set(PLATFORM_SOURCES "")
endif()

add_library(libEverQuestTCG STATIC
    ${PLATFORM_SOURCES}
)

# No target_link_libraries — only <windows.h>, <stdlib.h>, <stdio.h>, <assert.h>, <malloc.h>
# No target_precompile_headers — no First*.h in this external lib
```

**Wire into `src/external/3rd/library/CMakeLists.txt` (modify — add one line):**

Current content (lines 1–6):
```cmake
if(WHITENGOLD_USE_STLPORT_HEADERS)
    include_directories(BEFORE ${STLPORT_INCLUDE_DIR})
endif()

add_subdirectory(udplibrary)
add_subdirectory(ui)
```

Modified content:
```cmake
if(WHITENGOLD_USE_STLPORT_HEADERS)
    include_directories(BEFORE ${STLPORT_INCLUDE_DIR})
endif()

add_subdirectory(udplibrary)
add_subdirectory(ui)
add_subdirectory(libEverQuestTCG)    # Phase 4 — TCG LoadLibrary wrapper (1 cpp)
```

**Why no PCH:** `libEverQuestTCG.cpp` is a 3rd-party vendored file; no `First*.h` exists in that tree. The win32 `.cpp` is the whole lib. Do not add a PCH for this target.

---

### PATTERN E — swgClientUserInterface CMakeLists.txt (top-level wrapper)

**Analog:** `src/engine/client/library/clientUserInterface/CMakeLists.txt` (lines 1–7) — identical structure

```cmake
cmake_minimum_required(VERSION 2.8)

project(swgClientUserInterface)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/public)

add_subdirectory(src)
```

No deviations from the analog.

---

### PATTERN F — swgClientUserInterface src/CMakeLists.txt (266-cpp multi-subdir game UI lib)

**Primary analog:** `src/engine/client/library/clientUserInterface/src/CMakeLists.txt` (full content, lines 1–438)
**Secondary analog:** `src/engine/client/library/clientGame/src/CMakeLists.txt` (link block, lines 826–862)

**Directory layout** (verified by filesystem scan):
```
src/shared/core/     18 cpp  (SwgCuiManager, SwgCuiMediatorFactorySetup, SetupSwgClientUserInterface,
                              ConfigSwgClientUserInterface, SwgCuiActions, SwgCuiAttachmentList,
                              SwgCuiAvatarCreationHelper, SwgCuiBuffUtils, SwgCuiContainerProvider*,
                              SwgCuiLockableMediator, SwgCuiServerData, SwgCuiTcgManager,
                              SwgCuiWebBrowserManager + headers)
src/shared/page/     230 cpp (all game UI pages — SwgCuiHud*, SwgCuiAvatar*, SwgCuiVoice*, etc.)
src/shared/parser/   17 cpp  (SwgCuiCommandParser* — one per command domain)
src/win32/           1 cpp   (FirstSwgClientUserInterface.cpp)
Total: 266 cpp files
```

**PCH header:** `src/shared/core/FirstSwgClientUserInterface.h` (verified at `src/game/client/library/swgClientUserInterface/src/shared/core/FirstSwgClientUserInterface.h`)

**File enumeration strategy:** Use explicit `SHARED_SOURCES` lists for `core/` and `parser/` (18 + 17 = 35 files — manageable). For `page/` (230 files), use `file(GLOB_RECURSE PAGE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/shared/page/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/shared/page/*.h")` per RESEARCH.md rationale (stable, bounded set; acceptable per Phase 3 precedent for large page dirs).

**include_directories pattern** (copied from `clientUserInterface/src/CMakeLists.txt` lines 353–408 then augmented with game-side paths):

The swgClientUserInterface include list is a **superset** of `clientUserInterface`'s include list, adding:
- `${SWG_ENGINE_SOURCE_DIR}/client/library/clientGame/include/public`
- `${CMAKE_SOURCE_DIR}/game/shared/library/swgSharedNetworkMessages/include/public`
- `${CMAKE_SOURCE_DIR}/game/shared/library/swgSharedUtility/include/public`
- `${SWG_EXTERNALS_FIND}/libEverQuestTCG/include/public` (already in clientUserInterface per line 407)

Full include_directories block to use (extending the verified clientUserInterface analog):

```cmake
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/shared
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/core
    ${SWG_ENGINE_SOURCE_DIR}/client/library/clientGame/include/public
    ${SWG_ENGINE_SOURCE_DIR}/client/library/clientUserInterface/include/public
    ${SWG_ENGINE_SOURCE_DIR}/client/library/clientUserInterface/src/shared
    ${SWG_ENGINE_SOURCE_DIR}/client/library/clientGraphics/include/public
    ${SWG_ENGINE_SOURCE_DIR}/client/library/clientGraphics/include/private
    ${SWG_ENGINE_SOURCE_DIR}/client/library/clientGraphics/src/shared
    ${SWG_ENGINE_SOURCE_DIR}/client/library/clientObject/include/public
    ${SWG_ENGINE_SOURCE_DIR}/client/library/clientAnimation/include/public
    ${SWG_ENGINE_SOURCE_DIR}/client/library/clientSkeletalAnimation/include/public
    ${SWG_ENGINE_SOURCE_DIR}/client/library/clientTerrain/include/public
    ${SWG_ENGINE_SOURCE_DIR}/client/library/clientAudio/include/public
    ${SWG_ENGINE_SOURCE_DIR}/client/library/clientParticle/include/public
    ${SWG_ENGINE_SOURCE_DIR}/client/library/clientDirectInput/include/public
    ${SWG_ENGINE_SOURCE_DIR}/client/library/clientBugReporting/include/public
    ${SWG_ENGINE_SOURCE_DIR}/client/library/clientTextureRenderer/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundation/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedMemoryManager/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFile/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedGame/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedObject/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedUtility/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedNetwork/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedNetworkMessages/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundationTypes/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedDebug/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedMath/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedMathArchive/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedTerrain/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedCollision/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedRandom/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedSynchronization/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedImage/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedMessageDispatch/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedSkillSystem/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedIoWin/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedThread/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedCommandParser/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedInputMap/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedLog/include/public
    ${CMAKE_SOURCE_DIR}/game/shared/library/swgSharedUtility/include/public
    ${CMAKE_SOURCE_DIR}/game/shared/library/swgSharedNetworkMessages/include/public
    ${SWG_EXTERNALS_SOURCE_DIR}/ours/library/localization/include
    ${SWG_EXTERNALS_SOURCE_DIR}/ours/library/localizationArchive/include/public
    ${SWG_EXTERNALS_SOURCE_DIR}/ours/library/unicode/include
    ${SWG_EXTERNALS_SOURCE_DIR}/ours/library/unicodeArchive/include/public
    ${SWG_EXTERNALS_SOURCE_DIR}/ours/library/fileInterface/include/public
    ${SWG_EXTERNALS_SOURCE_DIR}/ours/library/singleton/include
    ${SWG_EXTERNALS_SOURCE_DIR}/ours/library/archive/include
    ${MOZILLA_PUBLIC_INCLUDE_DIR}
    ${DIRECTX9_INCLUDE_DIR}
    ${VIVOX_INCLUDE_DIRS}
    ${LOGITECHLCD_INCLUDE_DIR}
    ${SWG_EXTERNALS_FIND}/ui/include
    ${SWG_EXTERNALS_FIND}/ui/src/win32
    ${SWG_EXTERNALS_FIND}/boost
    ${SWG_EXTERNALS_FIND}/libEverQuestTCG/include/public
)
```

**target_link_libraries pattern** (extends `clientUserInterface/src/CMakeLists.txt` lines 416–431 with game-side additions):

```cmake
add_library(swgClientUserInterface STATIC
    ${SHARED_SOURCES}
    ${PAGE_SOURCES}
    ${PARSER_SOURCES}
    ${PLATFORM_SOURCES}
)

target_link_libraries(swgClientUserInterface
    libEverQuestTCG
    libMozilla
    ${VIVOX_LIBRARIES}
    ${LOGITECHLCD_LIBRARY}
    clientGame
    clientUserInterface
    clientGraphics
    clientObject
    clientAnimation
    sharedFoundation
    sharedFile
    sharedGame
    sharedObject
    sharedUtility
    sharedNetwork
    sharedNetworkMessages
    swgSharedNetworkMessages
    swgSharedUtility
    ui
)

target_compile_options(swgClientUserInterface PRIVATE /wd4530)

target_precompile_headers(swgClientUserInterface PRIVATE
    shared/core/FirstSwgClientUserInterface.h
)
```

**Deviations from `clientUserInterface/src/CMakeLists.txt` analog:**
- Adds `${CMAKE_SOURCE_DIR}/game/shared/library/swgSharedNetworkMessages/include/public` and `swgSharedUtility/include/public` to include_directories (game-side paths)
- Adds `${SWG_ENGINE_SOURCE_DIR}/client/library/clientGame/include/public` (clientGame is a dep here, not in clientUserInterface)
- Adds `libEverQuestTCG`, `clientGame`, `swgSharedNetworkMessages`, `swgSharedUtility` to target_link_libraries
- Uses GLOB_RECURSE for the 230-file `page/` subdirectory (explicitly lists `core/` and `parser/`)

---

### PATTERN G — SwgClient src/CMakeLists.txt (WIN32 executable, full ~44-entry link closure)

**Primary analog for structure:** `src/engine/client/library/clientGame/src/CMakeLists.txt` (lines 745–862) — source list, include_directories, add_library → replace with add_executable, target_link_libraries
**Primary analog for link pattern:** RESEARCH.md Pattern 3 (verified against `swgclient-build.md` dependency graph)

**Verified source files** (3 cpp + 1 .rc + 1 .h, all in `src/win32/`):
```
src/win32/WinMain.cpp
src/win32/ClientMain.cpp
src/win32/ClientMain.h
src/win32/FirstSwgClient.cpp
src/win32/SwgClient.rc
```

**PCH header:** `src/shared/FirstSwgClient.h` (verified at `src/game/client/application/SwgClient/src/shared/FirstSwgClient.h`)

**Full src/CMakeLists.txt pattern:**

```cmake
set(PLATFORM_SOURCES
    win32/WinMain.cpp
    win32/ClientMain.cpp
    win32/ClientMain.h
    win32/FirstSwgClient.cpp
    win32/SwgClient.rc
)

if(WIN32)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/win32)
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/shared        # FirstSwgClient.h location
    ${SWG_ENGINE_SOURCE_DIR}/client/library/clientGame/include/public
    ${SWG_ENGINE_SOURCE_DIR}/client/library/clientUserInterface/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundation/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFile/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedDebug/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedMemoryManager/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundationTypes/include/public
    ${CMAKE_SOURCE_DIR}/game/client/library/swgClientUserInterface/include/public
    ${CMAKE_SOURCE_DIR}/game/shared/library/swgSharedUtility/include/public
    ${SWG_EXTERNALS_SOURCE_DIR}/ours/library/localization/include
    ${SWG_EXTERNALS_SOURCE_DIR}/ours/library/unicode/include
)

add_executable(SwgClient WIN32
    ${PLATFORM_SOURCES}
)

set_target_properties(SwgClient PROPERTIES
    OUTPUT_NAME_DEBUG   SwgClient_d
    OUTPUT_NAME_RELEASE SwgClient_r
)

target_link_libraries(SwgClient PRIVATE
    # Game-side libs (Phase 4)
    swgClientUserInterface
    swgSharedNetworkMessages
    swgSharedUtility

    # Engine integrator (Phase 3)
    clientGame

    # Client engine libs (Phase 3)
    clientUserInterface
    clientSkeletalAnimation
    clientTerrain
    clientGraphics
    clientAudio
    clientObject
    clientParticle
    clientAnimation
    clientTextureRenderer
    clientDirectInput
    clientBugReporting
    libMozilla
    ui

    # Shared engine libs (Phase 2)
    sharedGame
    sharedObject
    sharedTerrain
    sharedNetwork
    sharedNetworkMessages
    sharedFoundation
    sharedFile
    sharedMath
    sharedMathArchive
    sharedDebug
    sharedLog
    sharedImage
    sharedMessageDispatch
    sharedUtility
    sharedRandom
    sharedRegex
    sharedXml
    sharedCompression
    sharedThread
    sharedSynchronization
    sharedIoWin
    sharedPathfinding
    sharedMemoryManager

    # SOE ours/ libs (Phase 1)
    archive
    crypto
    fileInterface
    localization
    localizationArchive
    unicode
    unicodeArchive

    # 3rd party (Phase 1 / vendored)
    udplibrary
    libEverQuestTCG
    ${STLPORT_LIBRARY}
    ${DPVS_LIBRARY}
    ${MILES_LIBRARY}
    ${VIDEOCAPTURE_LIBRARY}
    ${VIVOX_LIBRARIES}
    ${LOGITECHLCD_LIBRARY}
    ${PCRE_LIBRARY}
    ${LIBXML2_LIBRARY}
    ${ZLIB_LIBRARY}
    ${DIRECTX9_LIBRARIES}

    # Windows system libs
    ws2_32
    winmm
    mswsock
)

target_compile_options(SwgClient PRIVATE /wd4530)

target_precompile_headers(SwgClient PRIVATE
    shared/FirstSwgClient.h
)

# POST_BUILD: stage runtime DLLs, mozilla/ chrome subtree, and placeholder client.cfg
set(SWG_WIN32_DIR "${SWG_ROOT_SOURCE_DIR}/../exe/win32")

add_custom_command(TARGET SwgClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/Mss32.dll" "$<TARGET_FILE_DIR:SwgClient>/Mss32.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/binkw32.dll" "$<TARGET_FILE_DIR:SwgClient>/binkw32.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/dpvs.dll" "$<TARGET_FILE_DIR:SwgClient>/dpvs.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/dpvsd.dll" "$<TARGET_FILE_DIR:SwgClient>/dpvsd.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/vivoxsdk.dll" "$<TARGET_FILE_DIR:SwgClient>/vivoxsdk.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/vivoxplatform.dll" "$<TARGET_FILE_DIR:SwgClient>/vivoxplatform.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/ortp.dll" "$<TARGET_FILE_DIR:SwgClient>/ortp.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/alut.dll" "$<TARGET_FILE_DIR:SwgClient>/alut.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/libsndfile-1.dll" "$<TARGET_FILE_DIR:SwgClient>/libsndfile-1.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/wrap_oal.dll" "$<TARGET_FILE_DIR:SwgClient>/wrap_oal.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/xpcom.dll" "$<TARGET_FILE_DIR:SwgClient>/xpcom.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/xul.dll" "$<TARGET_FILE_DIR:SwgClient>/xul.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/nspr4.dll" "$<TARGET_FILE_DIR:SwgClient>/nspr4.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/plc4.dll" "$<TARGET_FILE_DIR:SwgClient>/plc4.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/plds4.dll" "$<TARGET_FILE_DIR:SwgClient>/plds4.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/nss3.dll" "$<TARGET_FILE_DIR:SwgClient>/nss3.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/nssckbi.dll" "$<TARGET_FILE_DIR:SwgClient>/nssckbi.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/smime3.dll" "$<TARGET_FILE_DIR:SwgClient>/smime3.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/softokn3.dll" "$<TARGET_FILE_DIR:SwgClient>/softokn3.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/ssl3.dll" "$<TARGET_FILE_DIR:SwgClient>/ssl3.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/js3250.dll" "$<TARGET_FILE_DIR:SwgClient>/js3250.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/gksvggdiplus.dll" "$<TARGET_FILE_DIR:SwgClient>/gksvggdiplus.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/gl05_r.dll" "$<TARGET_FILE_DIR:SwgClient>/gl05_r.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/gl05_o.dll" "$<TARGET_FILE_DIR:SwgClient>/gl05_o.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/gl06_r.dll" "$<TARGET_FILE_DIR:SwgClient>/gl06_r.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/gl06_o.dll" "$<TARGET_FILE_DIR:SwgClient>/gl06_o.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/gl07_r.dll" "$<TARGET_FILE_DIR:SwgClient>/gl07_r.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/gl07_o.dll" "$<TARGET_FILE_DIR:SwgClient>/gl07_o.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/msvcr71.dll" "$<TARGET_FILE_DIR:SwgClient>/msvcr71.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${SWG_WIN32_DIR}/mozilla" "$<TARGET_FILE_DIR:SwgClient>/mozilla"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../client.cfg"
        "$<TARGET_FILE_DIR:SwgClient>/client.cfg"
    VERBATIM
)
```

**Deviations from `clientGame/src/CMakeLists.txt` analog:**
- `add_executable(SwgClient WIN32 ...)` replaces `add_library(... STATIC ...)` — sets `/SUBSYSTEM:WINDOWS` automatically
- `set_target_properties(SwgClient PROPERTIES OUTPUT_NAME_DEBUG SwgClient_d OUTPUT_NAME_RELEASE SwgClient_r)` — no analog in any Phase 2/3 lib (novel for exe only)
- `${STLPORT_LIBRARY}` is linked explicitly at exe level (D-06/D-07) — never on static lib targets
- `target_link_libraries(SwgClient PRIVATE ...)` uses `PRIVATE` keyword — consistent with exe best practice (vs lib targets which use bare `target_link_libraries`)
- `add_custom_command(TARGET SwgClient POST_BUILD ...)` — novel; no lib target uses POST_BUILD
- Windows system libs (`ws2_32`, `winmm`, `mswsock`) listed explicitly — these appear in `libraries.rsp` and are not transitively covered by any engine lib

**PCH path:** `shared/FirstSwgClient.h` (not `shared/core/` — the PCH lives one level above core, consistent with the source tree layout at `src/shared/FirstSwgClient.h`)

**`${SWG_WIN32_DIR}` resolution:** `SWG_ROOT_SOURCE_DIR` is set in `src/CMakeLists.txt` line 21 as `${CMAKE_CURRENT_SOURCE_DIR}` (= `D:/Code/swg-client/src`). Therefore `${SWG_ROOT_SOURCE_DIR}/../exe/win32` = `D:/Code/swg-client/exe/win32` — correct.

**`client.cfg` source path:** The placeholder `client.cfg` file must be placed at `src/game/client/application/SwgClient/client.cfg` (one level above `src/`, i.e., alongside the top-level `CMakeLists.txt`). The POST_BUILD source reference `${CMAKE_CURRENT_SOURCE_DIR}/../client.cfg` resolves from `src/CMakeLists.txt` context within `SwgClient/src/` to `SwgClient/client.cfg`. Verify this resolves correctly by checking `CMAKE_CURRENT_SOURCE_DIR` in the `src/` subdirectory.

---

## Shared Patterns

### C4530 Suppression (carry from Phase 3)

**Source:** Phase 3 PATTERN L — `src/engine/client/library/clientGame/src/CMakeLists.txt` line 858
**Apply to:** All Phase 4 targets: `swgSharedNetworkMessages`, `swgClientUserInterface`, `SwgClient`

```cmake
target_compile_options(<TargetName> PRIVATE /wd4530)
```

### PCH Pattern (mandatory on every lib target)

**Source:** Phase 3 PATTERN B — `src/engine/client/library/clientUserInterface/src/CMakeLists.txt` lines 435–437

```cmake
target_precompile_headers(<LibName> PRIVATE
    <pch-relative-path>/First<LibName>.h
)
```

Exception: `libEverQuestTCG` has no `First*.h` and no PCH. Do not add one.

### Global CRT / Compile Definitions (DO NOT re-declare per-target)

**Source:** Root `src/CMakeLists.txt` — already global. Inherited automatically by all Phase 4 targets.
- `CMAKE_MSVC_RUNTIME_LIBRARY = MultiThreaded$<$<CONFIG:Debug>:Debug>` — `/MT` / `/MTd`
- `_USE_32BIT_TIME_T=1`, `_MBCS`, `PRODUCTION=0`, `DEBUG_LEVEL=2`
- `/Zc:wchar_t-` — global compiler flag

### STLPort Linkage (exe level only — D-06/D-07)

**Source:** CONTEXT.md D-06/D-07; `src/cmake/win32/FindSTLPort.cmake` — provides `${STLPORT_LIBRARY}`

```cmake
# In SwgClient target_link_libraries only — never on any static lib target
${STLPORT_LIBRARY}    # expands to: debug stlport_vc71_stldebug_static.lib optimized stlport_vc71_static.lib
```

The FindSTLPort.cmake sets `STLPORT_LIBRARY` with the `debug`/`optimized` CMake keywords so per-config selection is automatic.

### Engine Include Path Variables (established Phase 2 convention)

**Source:** `src/CMakeLists.txt` lines 21–25

```cmake
SWG_ROOT_SOURCE_DIR      = D:/Code/swg-client/src
SWG_ENGINE_SOURCE_DIR    = D:/Code/swg-client/src/engine
SWG_EXTERNALS_SOURCE_DIR = D:/Code/swg-client/src/external
SWG_EXTERNALS_FIND       = D:/Code/swg-client/src/external/3rd/library
SWG_OURS                 = D:/Code/swg-client/src/external/ours/library
```

Pattern for game-side paths (new in Phase 4):
```cmake
${CMAKE_SOURCE_DIR}/game/shared/library/<libName>/include/public
${CMAKE_SOURCE_DIR}/game/client/library/<libName>/include/public
```

Note: `CMAKE_SOURCE_DIR` = `D:/Code/swg-client/src` (same as `SWG_ROOT_SOURCE_DIR`).

---

## Execution Order / Wiring Summary

The seven files that must exist before `cmake --configure` succeeds, in dependency order:

1. `src/external/3rd/library/CMakeLists.txt` — add `add_subdirectory(libEverQuestTCG)` (modifies existing)
2. `src/external/3rd/library/libEverQuestTCG/CMakeLists.txt` + `src/CMakeLists.txt` (PATTERN A + D)
3. `src/game/shared/library/CMakeLists.txt` — add `add_subdirectory(swgSharedNetworkMessages)` (modifies existing)
4. `src/game/shared/library/swgSharedNetworkMessages/CMakeLists.txt` + `src/CMakeLists.txt` (PATTERN A + C)
5. `src/game/CMakeLists.txt` — add `add_subdirectory(client)` (modifies existing)
6. `src/game/client/CMakeLists.txt` + `src/game/client/library/CMakeLists.txt` + `src/game/client/application/CMakeLists.txt` (PATTERN B aggregators)
7. `src/game/client/library/swgClientUserInterface/CMakeLists.txt` + `src/CMakeLists.txt` (PATTERN E + F)
8. `src/game/client/application/SwgClient/CMakeLists.txt` + `src/CMakeLists.txt` (PATTERN A + G)
9. `src/game/client/application/SwgClient/client.cfg` (placeholder config file — staged by POST_BUILD)

---

## No Analog Found

All Phase 4 files have structural analogs from Phase 2 or Phase 3. The only novel element is the `add_executable + POST_BUILD` combination in SwgClient's `src/CMakeLists.txt` — but each component has its own existing analog (executable flags from VS project research, POST_BUILD from CMake docs pattern confirmed in RESEARCH.md D-04).

| File | Role | Data Flow | Reason |
|------|------|-----------|--------|
| (none) | — | — | All patterns covered by Phase 2/3 analogs |

---

## Critical Anti-Patterns (carry from Phase 3 + Phase 4 additions)

| Anti-Pattern | Effect | Source |
|---|---|---|
| Link `${BINK_LIBRARY}` (binkw32.lib) on SwgClient | CRT conflict: DLL import lib vs /MT | RESEARCH.md Anti-Patterns; binkw32.dll staged via POST_BUILD instead |
| Add XPCOM libs (nspr4, plc4, xpcom, xul) to SwgClient link line | Contradicts D-02 (Phase 3 verified zero XPCOM symbols) | CONTEXT.md D-02 |
| Override `CMAKE_MSVC_RUNTIME_LIBRARY` on SwgClient | Desyncs CRT from vendored libs | CONTEXT.md D-01 |
| Add STLPort to any static lib `target_link_libraries` | Premature LNK2005 at static-lib stage | Phase 3 PATTERN anti-patterns |
| Put `swgSharedNetworkMessages` under `game/client/library/` | Wrong subtree — it is game/shared | RESEARCH.md Pitfall 6 |
| Omit `add_subdirectory(client)` from `src/game/CMakeLists.txt` | SwgClient target never configured | RESEARCH.md Pitfall 5 |
| Use `${MOZILLA_PRIVATE_INCLUDE_DIR}` anywhere | Cascading C1083 on XPCOM headers | Phase 3 PATTERN E |
| Add PCH to `libEverQuestTCG` | No `First*.h` exists; would require source edit | INV-01 (no source edits) |
| Use `CMAKE_SOURCE_DIR` for engine paths (should be `SWG_ENGINE_SOURCE_DIR`) | Wrong base path — points at `src/`, not `src/engine/` | Phase 2/3 convention |

---

## Metadata

**Analog search scope:** `src/engine/client/library/*/CMakeLists.txt`, `src/engine/client/library/*/src/CMakeLists.txt`, `src/game/shared/library/swgSharedUtility/src/CMakeLists.txt`, `src/external/3rd/library/CMakeLists.txt`, `src/CMakeLists.txt`, Phase 3 PATTERNS.md
**Files scanned:** 16 CMakeLists files + Phase 3 PATTERNS.md + RESEARCH.md
**Pattern extraction date:** 2026-05-04
