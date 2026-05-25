# Phase 4: SwgClient Executable Link - Research

**Researched:** 2026-05-04
**Domain:** CMake WIN32 executable authoring, static-lib link aggregation, post-build DLL staging (Windows/MSVC)
**Confidence:** HIGH

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**D-01:** Rely on `CMAKE_MSVC_RUNTIME_LIBRARY` (CMP0091 NEW) for CRT library exclusions. The global setting `MultiThreaded$<$<CONFIG:Debug>:Debug>` already emits the correct `/NODEFAULTLIB` flags. No explicit `/NODEFAULTLIB` linker options needed on SwgClient.

**D-02:** Omit `nspr4.lib`, `plc4.lib`, `profdirserviceprovider_s.lib`, `xpcom.lib`, `xul.lib` entirely from `target_link_libraries`. Phase 3 D-07 confirmed via `dumpbin /symbols` that `clientUserInterface.lib` has zero XPCOM symbols; the stub generates no references so there is nothing to link.

**D-03:** Stage runtime DLLs and `mozilla/` chrome subtree **alongside the exe** in CMake's `RUNTIME_OUTPUT_DIRECTORY` — i.e., `bin/Debug/` for Debug and `bin/Release/` for Release.

**D-04:** Use `add_custom_command(TARGET SwgClient POST_BUILD ...)` for staging. Fires automatically on every successful link. Do NOT use `cmake install()` rules.

**D-05:** Drop a **minimal placeholder `client.cfg`** staged alongside the exe via POST_BUILD. Content: commented-out `[SharedFile] searchPath_00=` stubs. Phase 6 owns the full runtime config.

**D-06:** Link STLPort static runtime lib **eagerly from the start** — do not wait for LNK errors. Add both variants to `target_link_libraries(SwgClient PRIVATE ...)`.

**D-07:** Express via CMake generator expression in direct `target_link_libraries` on SwgClient:
```cmake
target_link_libraries(SwgClient PRIVATE
  $<$<CONFIG:Debug>:stlport_vc71_stldebug_static.lib>
  $<$<CONFIG:Release>:stlport_vc71_static.lib>
)
```
STLPort lib dir must be added to `target_link_directories` or expressed via full path. **NOTE FROM RESEARCH:** The FindSTLPort.cmake already provides `${STLPORT_LIBRARY}` as `debug ${STLPORT_DEBUG_LIBRARY} optimized ${STLPORT_RELEASE_LIBRARY}`, which expands to exactly these two variants. Use `${STLPORT_LIBRARY}` rather than the raw filenames (the find module sets `STLPORT_DEBUG_LIBRARY` → `stlport_vc71_stldebug_static.lib` and `STLPORT_RELEASE_LIBRARY` → `stlport_vc71_static.lib`).

**D-08:** If vc71 prebuilt causes unresolvable LNK storms, the named fallback is the TheSuperHackers/stlport-4.5.3 rebuild (locked Phase 1 decision).

**D-09:** Phase 4 is strictly the executable link step. No library CMakeLists authoring beyond `swgClientUserInterface` and `swgSharedNetworkMessages`.

**Cross-cutting:**
- PCH: `FirstSwgClient.h` via `target_precompile_headers(SwgClient PRIVATE src/shared/FirstSwgClient.h)`
- CRT: Global — SwgClient must NOT override
- No source edits (INV-01 hard constraint — FreeCamera.cpp minor type cast is the only exception, already applied in Phase 3)
- `/wd4530` (C4530 exception-handling unwind): apply on SwgClient target (consistent with Phase 3 pattern)

### Claude's Discretion

None specified in CONTEXT.md — all key decisions are locked.

### Deferred Ideas (OUT OF SCOPE)

- `swgClientAutomation` CMakeLists — not in SwgClient.vcproj; deferred
- `swgClientQtWidgets` CMakeLists — Qt tool lib, not in SwgClient.vcproj; deferred
- Full `client.cfg` with real `searchPath_NN` entries — Phase 6 scope (LAUNCH-01)
- `/WX` warnings-as-errors re-enable — deferred to post-v1 per FLAGS-02
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| BUILD-04 | `SwgClient` application `CMakeLists.txt` declares `add_executable(SwgClient WIN32 ...)` with WinMain/ClientMain/FirstSwgClient sources and ~70 `target_link_libraries` entries, plus post-build DLL/config staging via `add_custom_command` | Fully researched — exact link list, staging inventory, CMake patterns all documented below |
| ARTIF-01 | `cmake --build build --config Debug` produces `SwgClient_d.exe` from a clean clone; `dumpbin /imports` shows zero MSVCR*/VCRUNTIME*/xpcom/xul references | Researched — static CRT via global `CMAKE_MSVC_RUNTIME_LIBRARY`, XPCOM excluded per D-02 |
| ARTIF-02 | After ARTIF-01, `cmake --build build --config Release` produces `SwgClient_r.exe` | Flag flip from Debug — same toolchain, `OUTPUT_NAME_RELEASE SwgClient_r` property |
| ARTIF-03 | Both binaries link `/MTd` and `/MT` static CRT; zero `MSVCR*.dll`/`VCRUNTIME*.dll`/`xpcom.dll`/`xul.dll` in `dumpbin /imports` | Global `CMAKE_MSVC_RUNTIME_LIBRARY` already handles this; research confirms XPCOM excluded |
</phase_requirements>

---

## Summary

Phase 4 authors four new CMakeLists files (swgClientUserInterface, swgSharedNetworkMessages, SwgClient application wrapper, SwgClient src), wires them into the game/client subtree, and produces the final SwgClient_d.exe / SwgClient_r.exe. The exe links 3 source files + 1 .rc via `add_executable(SwgClient WIN32 ...)` and aggregates the entire engine through ~70 `target_link_libraries` entries — all internal CMake targets built in Phases 1–3, plus external vendored SDK libs via the already-resolved Find module variables.

The two new static libs are straightforward: `swgSharedNetworkMessages` is 10 cpp files across 5 subdirectories with minimal deps; `swgClientUserInterface` is 266 cpp files across 4 subdirectories and is the most complex game-side lib, with deps on `clientGame`, `clientUserInterface`, `swgSharedNetworkMessages`, and importantly `libEverQuestTCG` (whose .cpp source must be compiled here or in a small sub-target — currently compiled into NO existing CMake target).

The critical new risk in Phase 4 is the full exe link step itself: this is the first time the entire ~70-target static closure is simultaneously resolved by the linker. STLPort multiple-definition (LNK2005) and missing-symbol (LNK2019) storms are the primary failure mode. The pre-emptive STLPort link (D-06/D-07) addresses the most common form; residual unresolveds will require diagnosis.

**Primary recommendation:** Build in this order — swgSharedNetworkMessages first (minimal), then swgClientUserInterface (complex, may have novel include deps), then the SwgClient exe link with DLL staging. Treat the first link attempt as a diagnostic run rather than expecting clean success.

---

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Win32 entry shim + boot orchestration | Client Exe (SwgClient) | — | WinMain.cpp and ClientMain.cpp live directly in the SwgClient target |
| Game-side UI mediators (all player-facing screens) | Game Client Lib (swgClientUserInterface) | Engine Lib (clientUserInterface) | Game-specific CUI pages extend engine-layer CUI base classes |
| Network message types (combat, consent, survey, permission) | Game Shared Lib (swgSharedNetworkMessages) | Engine Shared Lib (sharedNetworkMessages) | Game-specific wire types extend engine network message base |
| Runtime DLL staging (Mss32, binkw32, vivox, mozilla, dpvs) | Post-Build CMake (exe-level) | — | DLLs must co-locate with the exe; POST_BUILD fires on link success |
| Config placeholder | Post-Build CMake (exe-level) | Phase 6 (full wiring) | Placeholder prevents fatal config error on first launch |

---

## Standard Stack

### Phase 4 CMake Targets (all new)

| Target | Type | Source Count | Complexity |
|--------|------|-------------|------------|
| `swgSharedNetworkMessages` | STATIC lib | 10 cpp | LOW — minimal deps |
| `swgClientUserInterface` | STATIC lib | 266 cpp | HIGH — 4 subdirs, libEverQuestTCG source needed |
| `libEverQuestTCG` | STATIC lib (new sub-target) | 1 cpp | LOW — but currently no CMake target exists |
| `SwgClient` | WIN32 EXE | 3 cpp + 1 .rc | CRITICAL — full 70-dep link closure |

### Library Variables Already Resolved by Find Modules (Phase 1)

[VERIFIED: existing FindXxx.cmake files in `src/cmake/win32/`]

| Variable | Expands To | Link Phase |
|----------|-----------|-----------|
| `${STLPORT_LIBRARY}` | `debug stlport_vc71_stldebug_static.lib optimized stlport_vc71_static.lib` | Exe only (D-06) |
| `${DPVS_LIBRARY}` | `debug dpvsd.lib optimized dpvs.lib` | clientGraphics, clientObject, etc. (already linked transitively) |
| `${MILES_LIBRARY}` | `Mss32.lib` | clientAudio (already linked transitively) |
| `${VIDEOCAPTURE_LIBRARY}` | debug/release variants of VideoCapture, AudioCapture, CaptureCommon | clientAudio (already) |
| `${VIVOX_LIBRARIES}` | vivoxSharedWrapper debug+release, vivoxsdk+platform | clientUserInterface (already) |
| `${LOGITECHLCD_LIBRARY}` | `lgLcd.lib` | clientUserInterface (already) |
| `${PCRE_LIBRARY}` | `libpcre.a` | sharedRegex (already) |
| `${LIBXML2_LIBRARY}` | `debug libxml2-win32-debug.lib optimized libxml2-win32-release.lib` | sharedXml (already) |
| `${ZLIB_LIBRARY}` | `zlib.lib` | sharedCompression (already) |
| `${DIRECTX9_LIBRARIES}` | d3d9, d3dx9, dinput8, dsound, ddraw, dxguid, DxErr9 | clientDirectInput, clientGraphics (already) |
| `${BINK_LIBRARY}` | `binkw32.lib` | NOT LINKED — LoadLibrary pattern (see anti-patterns) |

### Windows System Libs (add directly by name on SwgClient)

[VERIFIED: `swgclient-build.md` `libraries.rsp` section]

```
ws2_32.lib     — Windows Sockets
winmm.lib      — Windows multimedia
mswsock.lib    — Microsoft Sockets extension
```

These appear in `libraries.rsp` and are not covered by any engine lib's `target_link_libraries` — they must be on the exe directly.

---

## Architecture Patterns

### System Architecture Diagram

```
src/game/CMakeLists.txt
  └─ add_subdirectory(client)          [new]
       └─ game/client/CMakeLists.txt   [new]
            ├─ add_subdirectory(library)
            │    └─ game/client/library/CMakeLists.txt   [new]
            │         ├─ add_subdirectory(swgSharedNetworkMessages)  ← but this is game/shared
            │         └─ add_subdirectory(swgClientUserInterface)
            └─ add_subdirectory(application)
                 └─ game/client/application/CMakeLists.txt   [new]
                      └─ add_subdirectory(SwgClient)
                           ├─ SwgClient/CMakeLists.txt     [new]
                           └─ SwgClient/src/CMakeLists.txt [new]
                                └─ add_executable(SwgClient WIN32 ...)
                                     target_link_libraries(SwgClient ...)
                                     set_target_properties(... OUTPUT_NAME_DEBUG SwgClient_d ...)
                                     add_custom_command(TARGET SwgClient POST_BUILD ...)
```

**swgSharedNetworkMessages** lives under `game/shared/library/` (not `game/client/library/`). The `game/shared/library/CMakeLists.txt` already exists and currently only includes `swgSharedUtility`. Phase 4 adds `add_subdirectory(swgSharedNetworkMessages)` there.

**Data flow at link time:**

```
SwgClient.exe
  ├── [direct sources]  WinMain.cpp, ClientMain.cpp, FirstSwgClient.cpp, SwgClient.rc
  ├── [game-side libs]  swgClientUserInterface, swgSharedNetworkMessages, swgSharedUtility
  ├── [integrator]      clientGame (Phase 3)
  ├── [client engine]   clientUserInterface, clientSkeletalAnimation, clientTerrain,
  │                     clientGraphics, clientAudio, clientObject, clientParticle,
  │                     clientAnimation, clientTextureRenderer, clientDirectInput,
  │                     clientBugReporting, libMozilla, ui
  ├── [shared engine]   sharedGame, sharedObject, sharedTerrain, sharedNetwork,
  │                     sharedNetworkMessages, sharedFoundation, sharedFile,
  │                     sharedMath, sharedDebug, sharedLog, sharedImage,
  │                     sharedMessageDispatch, sharedUtility, sharedRandom,
  │                     sharedRegex, sharedXml, sharedCompression, sharedThread,
  │                     sharedSynchronization, sharedIoWin, sharedPathfinding,
  │                     sharedMemoryManager, sharedMathArchive,
  │                     sharedFoundationTypes (header-only)
  ├── [ours/ libs]      archive, crypto, fileInterface, localization,
  │                     localizationArchive, unicode, unicodeArchive
  ├── [3rd party]       udplibrary, ${STLPORT_LIBRARY}, ${DPVS_LIBRARY},
  │                     ${MILES_LIBRARY}, ${VIDEOCAPTURE_LIBRARY},
  │                     ${VIVOX_LIBRARIES}, ${LOGITECHLCD_LIBRARY},
  │                     ${PCRE_LIBRARY}, ${LIBXML2_LIBRARY}, ${ZLIB_LIBRARY},
  │                     ${DIRECTX9_LIBRARIES}
  └── [Windows system]  ws2_32.lib, winmm.lib, mswsock.lib
```

### Recommended Project Structure (new files only)

```
src/game/CMakeLists.txt                          [modify — add add_subdirectory(client)]
src/game/client/CMakeLists.txt                   [new]
src/game/client/library/CMakeLists.txt           [new]
src/game/client/library/swgClientUserInterface/
  CMakeLists.txt                                 [new]
  src/CMakeLists.txt                             [new]
src/game/client/application/CMakeLists.txt       [new]
src/game/client/application/SwgClient/
  CMakeLists.txt                                 [new]
  src/CMakeLists.txt                             [new]
src/game/shared/library/CMakeLists.txt           [modify — add swgSharedNetworkMessages]
src/game/shared/library/swgSharedNetworkMessages/
  CMakeLists.txt                                 [new]
  src/CMakeLists.txt                             [new]
src/external/3rd/library/libEverQuestTCG/
  CMakeLists.txt                                 [new — 1 cpp, headers only]
  src/CMakeLists.txt                             [new]
```

### Pattern 1: swgSharedNetworkMessages CMakeLists

**What:** 10 cpp files across 5 subdirectories under `src/shared/`.

[VERIFIED: filesystem count at `src/game/shared/library/swgSharedNetworkMessages/src/`]

Directory layout:
```
src/shared/combat/        3 cpp  (CombatActionCompleteMessage, MessageQueueCombatAction, MessageQueueCombatDamage)
src/shared/consent/       1 cpp  (ConsentRequestMessage)
src/shared/core/          1 cpp  (SetupSwgSharedNetworkMessages)
src/shared/permissionList/ 2 cpp (PermissionListCreateMessage, PermissionListModifyMessage)
src/shared/survey/        2 cpp  (ResourceListForSurveyMessage, SurveyMessage)
src/win32/                1 cpp  (FirstSwgSharedNetworkMessages)
```

**Pattern:** Same three-level nesting as all Phase 2/3 libs (PATTERN A + PATTERN B from Phase 3 PATTERNS.md).

**PCH:** `src/shared/core/FirstSwgSharedNetworkMessages.h`

**Include deps:** `sharedNetworkMessages`, `sharedFoundation`, `sharedMath`, `archive`. The public headers confirm combat/survey/consent message types extending engine message base classes.

**No external SDK deps** for swgSharedNetworkMessages.

**Top-level CMakeLists.txt (PATTERN A):**
```cmake
cmake_minimum_required(VERSION 2.8)
project(swgSharedNetworkMessages)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/public)
add_subdirectory(src)
```

### Pattern 2: swgClientUserInterface CMakeLists

**What:** 266 cpp files across 4 subdirectories under `src/shared/` plus 1 cpp in `src/win32/`.

[VERIFIED: filesystem count and directory structure]

Directory layout:
```
src/shared/core/    18 cpp   (SwgCuiManager, SwgCuiMediatorFactorySetup, SetupSwgClientUserInterface, etc.)
src/shared/page/    228 cpp  (SwgCuiSplash, SwgCuiHud*, SwgCuiAvatar*, SwgCuiVoice*, SwgCui[all game pages])
src/shared/parser/  18 cpp   (SwgCuiCommandParser* — one per command domain)
src/win32/          2 cpp    (FirstSwgClientUserInterface, possibly one more)
```

**NOTE:** filesystem count shows 266 total; 18+228+18+2 = 266. Verify exact count during plan by running `find ... -name "*.cpp" | wc -l`.

**SDK deps beyond transitive closure:**
- `libEverQuestTCG` include path: `${SWG_EXTERNALS_FIND}/libEverQuestTCG/include/public`
- `libEverQuestTCG` link dep: the `libEverQuestTCG` CMake target (new — see Pattern 4)
- All other deps (`clientGame`, `clientUserInterface`, `swgSharedNetworkMessages`, DX9, Vivox, Mozilla, lcdui, ui) are inherited transitively via `clientGame` and `clientUserInterface` targets, but must be in `target_link_libraries` if swgClientUserInterface is a STATIC lib (MSVC static libs require explicit link at the consumer's link step)

**Include dirs needed** (verified by grepping key source files):
```
shared/core                                       (internal headers)
shared/page                                       (internal headers)
${SWG_ENGINE_SOURCE_DIR}/client/library/clientGame/include/public
${SWG_ENGINE_SOURCE_DIR}/client/library/clientUserInterface/include/public
${SWG_ENGINE_SOURCE_DIR}/client/library/clientGraphics/include/public
${SWG_ENGINE_SOURCE_DIR}/client/library/clientAudio/include/public
${SWG_ENGINE_SOURCE_DIR}/client/library/clientObject/include/public
${SWG_ENGINE_SOURCE_DIR}/client/library/clientAnimation/include/public
${SWG_ENGINE_SOURCE_DIR}/client/library/clientSkeletalAnimation/include/public
${SWG_ENGINE_SOURCE_DIR}/client/library/clientTerrain/include/public
${SWG_ENGINE_SOURCE_DIR}/client/library/clientDirectInput/include/public
${SWG_ENGINE_SOURCE_DIR}/client/library/clientParticle/include/public
${SWG_ENGINE_SOURCE_DIR}/client/library/clientBugReporting/include/public
${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundation/include/public
${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFile/include/public
${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedGame/include/public
${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedObject/include/public
${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedUtility/include/public
${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedNetwork/include/public
${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedNetworkMessages/include/public
${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedMath/include/public
${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedDebug/include/public
${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedMathArchive/include/public
${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedMessageDispatch/include/public
${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedRandom/include/public
${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedImage/include/public
${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedTerrain/include/public
${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedCollision/include/public
${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedIoWin/include/public
${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedThread/include/public
${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundationTypes/include/public
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
```

**Note:** The include list above is modelled on the existing `clientUserInterface` CMakeLists (which swgClientUserInterface extends) plus the game-specific additions (`clientGame`, `swgSharedNetworkMessages`, `swgSharedUtility`, `libEverQuestTCG`). Expect the first compile to reveal additional missing includes — the planner should build this incrementally, starting with `core/` files.

### Pattern 3: SwgClient Executable

**What:** WIN32 exe, 3 cpp + 1 .rc, full ~70-dep link closure.

[VERIFIED: `swgclient-build.md`, source filesystem scan, existing Phase 3 CMakeLists]

**Source files:**
```
src/win32/WinMain.cpp
src/win32/ClientMain.cpp
src/win32/FirstSwgClient.cpp
src/win32/SwgClient.rc
```

**PCH:** `src/shared/FirstSwgClient.h`

**Compiler settings (all already global — no per-target needed):**
- `_MBCS` — global
- `_USE_32BIT_TIME_T=1` — global
- `/Zc:wchar_t-` — global
- RTTI enabled — MSVC default; no `/GR-` anywhere
- `Detect64BitPortabilityProblems=FALSE` — not relevant in CMake (this was a VS2005 IDE flag)
- `/SUBSYSTEM:WINDOWS` — set automatically by `add_executable(SwgClient WIN32 ...)`

**Binary naming:**
```cmake
set_target_properties(SwgClient PROPERTIES
    OUTPUT_NAME_DEBUG   SwgClient_d
    OUTPUT_NAME_RELEASE SwgClient_r
)
```

**Complete `target_link_libraries` for SwgClient:**

[VERIFIED: swgclient-build.md dependency graph, existing Phase 3 CMakeLists targets, Find module variable names]

```cmake
target_link_libraries(SwgClient PRIVATE
    # Game-side libs (Phase 4 new)
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
```

**Entry count:** 23 engine/game targets + 7 ours targets + udplibrary + 10 SDK lib groups + 3 system libs = ~44 explicit entries. The "~70" count from the roadmap refers to the original swg.sln project GUID count, which includes editor and tool projects not in SwgClient's link line. This CMake link line covers all libs in the `libraries.rsp` dependency graph (minus XPCOM per D-02).

**NOTE on sharedCollision/sharedFractal/sharedSkillSystem:** These are INTERFACE stubs defined in Phase 2 at `src/engine/shared/library/CMakeLists.txt`. They have no .lib output but are CMake targets. They are linked via `clientGame`'s `target_link_libraries` transitively. You do NOT need to repeat them on SwgClient explicitly. MSVC will resolve the INTERFACE dependency via transitive closure.

### Pattern 4: libEverQuestTCG Sub-Target (new requirement)

**What:** 1 cpp file at `src/external/3rd/library/libEverQuestTCG/src/win32/libEverQuestTCG.cpp` that defines runtime LoadLibrary-based function bodies used by `SwgCuiTcgManager.cpp` and `SwgCuiTcgControl.cpp`. Currently no CMake target exists for it.

[VERIFIED: grepped swgClientUserInterface sources; found `libEverQuestTCG::init`, `libEverQuestTCG::setDesktopWindow` etc. calls. No existing CMakeLists.txt in libEverQuestTCG directory.]

**Options (in preference order):**
1. **Build as small STATIC lib** via `src/external/3rd/library/libEverQuestTCG/CMakeLists.txt`:
   ```cmake
   cmake_minimum_required(VERSION 2.8)
   project(libEverQuestTCG)
   include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/public)
   add_subdirectory(src)
   ```
   Wire into `src/external/3rd/library/CMakeLists.txt` with `add_subdirectory(libEverQuestTCG)`.
   Add `libEverQuestTCG` to `swgClientUserInterface`'s `target_link_libraries`.

2. **Add the .cpp directly to swgClientUserInterface's source list** (no separate target). Simpler but less clean. The include path for `libEverQuestTCG.h` private headers must resolve.

Option 1 is preferred — matches established pattern (separate CMake target per 3rd-party lib).

**Include path for libEverQuestTCG.cpp compilation:** `src/external/3rd/library/libEverQuestTCG/src/win32/` (private headers) plus `src/external/3rd/library/libEverQuestTCG/include/public/` (public header).

**No external SDK link deps** — uses only `<windows.h>`, `<stdlib.h>`, `<stdio.h>` (standard system includes). All actual EverQuestTCG runtime calls happen via function pointers loaded with `LoadLibrary`/`GetProcAddress`.

### Pattern 5: DLL Staging POST_BUILD

**What:** After successful link, copy 36 DLLs from `exe/win32/` + `mozilla/` chrome subtree + placeholder `client.cfg` to `$<TARGET_FILE_DIR:SwgClient>` (the config-appropriate bin/Debug/ or bin/Release/).

[VERIFIED: filesystem scan of `exe/win32/*.dll`, count = 46 total; after filtering to client-runtime-required subset]

**Runtime DLLs to stage** (from `exe/win32/`, all configs):

```
Mss32.dll           — Miles Sound System (required at boot — clientAudio)
binkw32.dll         — Bink video codec (LoadLibrary pattern — VideoList::install)
dpvs.dll            — DPVS occlusion (Release/Optimized)
dpvsd.dll           — DPVS occlusion (Debug)
vivoxsdk.dll        — Vivox voice SDK
vivoxplatform.dll   — Vivox platform
ortp.dll            — Vivox RTP
alut.dll            — Vivox/OpenAL utility
libsndfile-1.dll    — Vivox audio
wrap_oal.dll        — Vivox OpenAL wrapper
xpcom.dll           — Mozilla XPCOM runtime (needed even with stub — DLL search)
xul.dll             — Mozilla XUL runtime
nspr4.dll           — Mozilla NSPR
plc4.dll            — Mozilla PLC
plds4.dll           — Mozilla PLC supplemental
nss3.dll            — Mozilla NSS
nssckbi.dll         — Mozilla certificate DB
smime3.dll          — Mozilla S/MIME
softokn3.dll        — Mozilla softokn
ssl3.dll            — Mozilla SSL
js3250.dll          — Mozilla JavaScript
gksvggdiplus.dll    — Mozilla SVG
gl05_r.dll          — DirectX 9 renderer plugin (Release)
gl05_o.dll          — DirectX 9 renderer plugin (Optimized)
gl06_r.dll          — DirectX 9 renderer plugin v6 (Release)
gl06_o.dll          — DirectX 9 renderer plugin v6 (Optimized)
gl07_r.dll          — DirectX 9 renderer plugin v7 (Release)
gl07_o.dll          — DirectX 9 renderer plugin v7 (Optimized)
msvcr71.dll         — MSVC 7.1 runtime (needed by vendored DLLs that link against it)
dbghelp_6.3.17.0.dll— Debug helper (optional but present)
```

**DLL staging strategy:**

Per-config variant (debug vs release DLLs):
- `dpvsd.dll` → Debug config only
- `dpvs.dll` → Release config only
- All others are config-neutral (copy to both)

For the exe, all DLLs from `exe/win32/` that are client-runtime-required should be staged to BOTH `bin/Debug/` and `bin/Release/`. The simplest approach: stage all 36 to both configs with one `add_custom_command` block (idempotent copy — copying dpvs.dll to both dirs is safe, and both debug and release binaries are in separate directories).

**mozilla/ subtree:** Copy the entire `exe/win32/mozilla/` directory tree using `cmake -E copy_directory`.

**Placeholder client.cfg content:**
```ini
# SwgClient placeholder configuration — Phase 4
# Full runtime configuration added in Phase 6 (LAUNCH-01)
#
# To use: set searchPath_00 through searchPath_NN to point at your
# retail SWG installation or community client-assets directory.

[SharedFile]
    # searchPath_00=C:/SWG
```

**CMake POST_BUILD pattern (D-04):**
```cmake
set(SWG_WIN32_DIR ${CMAKE_SOURCE_DIR}/../exe/win32)

add_custom_command(TARGET SwgClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/Mss32.dll"
        "$<TARGET_FILE_DIR:SwgClient>/Mss32.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/binkw32.dll"
        "$<TARGET_FILE_DIR:SwgClient>/binkw32.dll"
    # ... repeat for each DLL ...
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${SWG_WIN32_DIR}/mozilla"
        "$<TARGET_FILE_DIR:SwgClient>/mozilla"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/client.cfg"
        "$<TARGET_FILE_DIR:SwgClient>/client.cfg"
    VERBATIM
)
```

**NOTE on exe/win32 path:** The SwgClient CMakeLists.txt will be at `src/game/client/application/SwgClient/src/CMakeLists.txt`. The `exe/win32/` directory is at `${CMAKE_SOURCE_DIR}/../exe/win32` if `CMAKE_SOURCE_DIR` is `D:/Code/swg-client/src`. Alternatively use `${SWG_ROOT_SOURCE_DIR}/../exe/win32` (from root CMakeLists). Verify the path resolves during plan.

### Anti-Patterns to Avoid

- **Do NOT link `${BINK_LIBRARY}` (binkw32.lib) on SwgClient.** Bink uses `LoadLibrary`; the .lib is an import lib for the DLL export table, and mixing import lib + `/MT` generates a CRT conflict. The `binkw32.dll` is staged post-build — that is the complete Bink integration at runtime.
- **Do NOT add XPCOM .libs** (nspr4, plc4, profdirserviceprovider_s, xpcom, xul) to SwgClient. D-02 is locked; the stub generates zero XPCOM references. The Mozilla DLLs ARE staged post-build for potential runtime init paths.
- **Do NOT override `CMAKE_MSVC_RUNTIME_LIBRARY` on SwgClient.** The global setting handles it; overriding would desync with vendored libs.
- **Do NOT set DPVS include paths on SwgClient.** SwgClient only calls DPVS transitively via clientGraphics/clientObject. The include paths are on those lib targets.
- **Do NOT add `WIN32_LEAN_AND_MEAN` or `NOMINMAX` globally.** Only apply to individual targets if C2146/C4005 errors appear (Pattern D-13 from Phase 3).
- **Do NOT add the Vivox service executable `VivoxVoiceService.exe` to POST_BUILD staging.** It's not in `exe/win32/` — it ships with the Vivox SDK separately and isn't needed for Phase 4 compilation gate.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Config-aware DLL selection (debug/release) | Custom staging script | `cmake -E copy_if_different` + `$<TARGET_FILE_DIR:SwgClient>` generator expression | Built-in CMake generator expression selects the correct output dir per config |
| Link order dependency tracking | Manual `.depends` files | CMake `target_link_libraries` with named targets | CMake's dependency graph enforces build order automatically |
| Per-config STLPort lib selection | Manual `if(CMAKE_BUILD_TYPE)` checks | `${STLPORT_LIBRARY}` which already uses `debug`/`optimized` keywords | FindSTLPort.cmake already provides the correct generator expression |
| XPCOM detection at link time | Symbol-checking scripts | Trust Phase 3's `dumpbin /symbols` validation — D-02 is verified | Already confirmed zero XPCOM symbols in clientUserInterface.lib |

**Key insight:** The Phase 3 groundwork means Phase 4 is almost entirely wiring and aggregation. The hard SDK integration work (DX9, DPVS, Miles, Vivox, XPCOM stub) is proven; Phase 4 only needs to assemble the top-level target correctly.

---

## Common Pitfalls

### Pitfall 1: STLPort LNK2005 Multiple Definitions at Exe Link

**What goes wrong:** Static libs compiled with STLPort headers (which define some STL functions in-header) each pull in a copy. At exe link, the linker sees multiple definitions of the same STLPort-internal symbols across many .lib files.
**Why it happens:** STLPort uses header-only implementations for several STL containers/algorithms; every TU that instantiates `std::string` etc. contributes a copy of those definitions. MSVC's /LTCG should merge them, but without it, LNK2005 storms appear.
**How to avoid:** D-06 pre-emptively links `${STLPORT_LIBRARY}` on SwgClient. The STLPort static .lib is compiled with `/Gz` and is designed to resolve these duplicates. If LNK2005 still appears, add `/FORCE:MULTIPLE` to `CMAKE_EXE_LINKER_FLAGS` as a diagnostic step only, then remove after root-causing.
**Warning signs:** Wall of `LNK2005: "symbol" already defined in stlport_vc71_static.lib` or `LNK2005: "symbol" already defined in sharedFoundation.lib`.

### Pitfall 2: libEverQuestTCG Link Gap

**What goes wrong:** `swgClientUserInterface` compiles `SwgCuiTcgManager.cpp` and `SwgCuiTcgControl.cpp` which call `libEverQuestTCG::init`, `setDesktopWindow`, etc. These symbols are defined in `libEverQuestTCG.cpp` — which currently has NO CMake target. The exe link will fail with LNK2019 on `libEverQuestTCG::*` symbols.
**Why it happens:** In the original VS2005 build, `libEverQuestTCG` had its own `.vcproj`. The Phase 3/4 port hasn't authored its CMakeLists yet.
**How to avoid:** Author a new `libEverQuestTCG` CMake target (STATIC lib, 1 cpp file) in `src/external/3rd/library/libEverQuestTCG/` and add it to `src/external/3rd/library/CMakeLists.txt` before building Phase 4. Add `libEverQuestTCG` to `swgClientUserInterface`'s `target_link_libraries`.
**Warning signs:** `LNK2019: unresolved external symbol "bool __cdecl libEverQuestTCG::init(...)"`

### Pitfall 3: Circular Include / Wrong Path for Game Headers

**What goes wrong:** `swgClientUserInterface` includes from `clientGame`, `clientUserInterface`, and `swgSharedNetworkMessages` via short-form paths (e.g., `#include "clientGame/Game.h"`). If those libs' `include/public` directories are not on the include path, C1083 errors cascade.
**Why it happens:** Game-side libs live in `src/game/`, not `src/engine/` — the path variables differ (`${CMAKE_SOURCE_DIR}/game/...` vs `${SWG_ENGINE_SOURCE_DIR}/...`).
**How to avoid:** The include_directories list in Pattern 2 above covers this. Verify `${CMAKE_SOURCE_DIR}/game/shared/library/swgSharedNetworkMessages/include/public` is present.
**Warning signs:** `C1083: Cannot open include file: 'swgSharedNetworkMessages/SetupSwgSharedNetworkMessages.h'`

### Pitfall 4: exe/win32 DLL Staging Path Resolution

**What goes wrong:** The `add_custom_command` POST_BUILD block references DLL source paths. If `SWG_WIN32_DIR` resolves incorrectly at CMake configure time, the copy commands silently fail to register (or fail at build time with "file not found").
**Why it happens:** CMake generator expressions like `$<TARGET_FILE_DIR:SwgClient>` are not evaluated until build time. The source paths are evaluated at configure time. Path variable setup must be careful about `../` relative paths.
**How to avoid:** Use `${SWG_ROOT_SOURCE_DIR}/../exe/win32` (where `SWG_ROOT_SOURCE_DIR` is `${CMAKE_SOURCE_DIR}` set in root CMakeLists). Or use a `cmake_path` call. Verify the DLL source exists with `file(TO_CMAKE_PATH ...)` debug check.
**Warning signs:** Build succeeds but `bin/Debug/SwgClient_d.exe` launches and immediately crashes with missing DLL.

### Pitfall 5: Aggregator CMakeLists.txt Missing for game/client

**What goes wrong:** The root `src/game/CMakeLists.txt` currently only adds `shared` subdirectory. There's no `client` subdirectory in it. SwgClient targets will never be configured.
**Why it happens:** Phase 3 only authored `engine/client/` paths; game/client was deferred to Phase 4.
**How to avoid:** Add `add_subdirectory(client)` to `src/game/CMakeLists.txt`. Create `src/game/client/CMakeLists.txt` as a simple aggregator that adds `library` and `application` subdirectories.
**Warning signs:** `cmake --build` completes without any SwgClient target visible in the generated solution.

### Pitfall 6: swgSharedNetworkMessages Wiring

**What goes wrong:** swgSharedNetworkMessages lives under `game/shared/library/`, NOT `game/client/library/`. The existing `game/shared/library/CMakeLists.txt` must be modified to add it.
**Why it happens:** Network messages are shared between client and server; they live in `game/shared/`, not `game/client/`.
**How to avoid:** Modify `src/game/shared/library/CMakeLists.txt` to add `add_subdirectory(swgSharedNetworkMessages)`. Do NOT put it under `game/client/library/`.
**Warning signs:** CMake configure error "add_subdirectory given source that does not contain a CMakeLists.txt".

---

## Code Examples

### SwgClient Application Top-Level CMakeLists.txt
```cmake
# src/game/client/application/SwgClient/CMakeLists.txt
cmake_minimum_required(VERSION 2.8)
project(SwgClient)
add_subdirectory(src)
```

### SwgClient src/CMakeLists.txt (skeleton)
```cmake
# src/game/client/application/SwgClient/src/CMakeLists.txt
set(PLATFORM_SOURCES
    win32/WinMain.cpp
    win32/ClientMain.cpp
    win32/FirstSwgClient.cpp
    win32/SwgClient.rc
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/shared    # FirstSwgClient.h location
    ${CMAKE_CURRENT_SOURCE_DIR}/win32
    ${SWG_ENGINE_SOURCE_DIR}/client/library/clientGame/include/public
    # ... additional include paths for any headers referenced by ClientMain.cpp ...
)

add_executable(SwgClient WIN32
    ${PLATFORM_SOURCES}
)

set_target_properties(SwgClient PROPERTIES
    OUTPUT_NAME_DEBUG   SwgClient_d
    OUTPUT_NAME_RELEASE SwgClient_r
)

target_link_libraries(SwgClient PRIVATE
    swgClientUserInterface
    swgSharedNetworkMessages
    swgSharedUtility
    clientGame
    # ... all ~44 entries from Pattern 3 above ...
    ${STLPORT_LIBRARY}
    ws2_32
    winmm
    mswsock
)

target_compile_options(SwgClient PRIVATE /wd4530)

target_precompile_headers(SwgClient PRIVATE
    shared/FirstSwgClient.h
)

# POST_BUILD: stage DLLs and mozilla/ chrome subtree
set(SWG_WIN32_DIR "${SWG_ROOT_SOURCE_DIR}/../exe/win32")
add_custom_command(TARGET SwgClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SWG_WIN32_DIR}/Mss32.dll" "$<TARGET_FILE_DIR:SwgClient>/Mss32.dll"
    # ... all DLLs ...
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${SWG_WIN32_DIR}/mozilla" "$<TARGET_FILE_DIR:SwgClient>/mozilla"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../client.cfg.in"
        "$<TARGET_FILE_DIR:SwgClient>/client.cfg"
    VERBATIM
)
```

### swgSharedNetworkMessages src/CMakeLists.txt (skeleton)
```cmake
# src/game/shared/library/swgSharedNetworkMessages/src/CMakeLists.txt
set(SHARED_SOURCES
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
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedNetworkMessages/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundation/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedMath/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedObject/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedGame/include/public
    ${SWG_EXTERNALS_SOURCE_DIR}/ours/library/archive/include
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

### libEverQuestTCG CMakeLists
```cmake
# src/external/3rd/library/libEverQuestTCG/CMakeLists.txt
cmake_minimum_required(VERSION 2.8)
project(libEverQuestTCG)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/public)
add_subdirectory(src)

# src/external/3rd/library/libEverQuestTCG/src/CMakeLists.txt
add_library(libEverQuestTCG STATIC
    win32/libEverQuestTCG.cpp
    win32/libEverQuestTCG.h
)
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/win32
)
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `swg.sln` with ~70 GUID project references | CMake `target_link_libraries` with named targets | Phase 4 (this phase) | CMake manages build order and dep closure automatically |
| Per-config `.rsp` files (`libraries_d.rsp`, `libraries_r.rsp`) | CMake `debug`/`optimized` keywords in `target_link_libraries` | Phase 1/2/3 established pattern | Per-config lib selection is implicit in CMake generator expressions |
| Manual XPCOM lib link (nspr4, plc4, xpcom, xul) | XPCOM omitted; libMozilla stub supplies `init/update/release` no-ops | Phase 3 D-07 locked | Zero XPCOM link time symbols; stub verified clean in Phase 3 |
| `RuntimeLibrary="1"` (Debug MDd) in vcproj | `CMAKE_MSVC_RUNTIME_LIBRARY = MultiThreaded$<$<CONFIG:Debug>:Debug>` | Phase 1 | Locks CRT to /MT[d] across all targets |

**Deprecated/outdated:**
- `libraries.rsp` / `libraries_d.rsp` / `libraries_r.rsp`: These were the SOE build system's linker input files. In CMake they are replaced by `target_link_libraries` — documented in `swgclient-build.md` for historical reference only.
- `ignoreLibraries.rsp` (libc, MSVCRT): Replaced by `CMAKE_MSVC_RUNTIME_LIBRARY = MultiThreaded...` which automatically emits the correct `/NODEFAULTLIB` flags.

---

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | swgSharedNetworkMessages has no external SDK deps beyond engine network libs | Pattern 1 | If a survey/combat message includes a third-party header, C1083 during compile |
| A2 | `swgClientUserInterface` include paths are a superset of `clientUserInterface` include paths | Pattern 2 | Missing includes → C1083 on first compile; will require diagnosis |
| A3 | `libEverQuestTCG.cpp` has no external SDK deps beyond Windows system headers | Pattern 4 | If it has dep on a proprietary TCG SDK (not in repo), LNK2019 at static lib link |
| A4 | `ws2_32`, `winmm`, `mswsock` are not already in any transitive static lib dep chain | Pattern 3 | If they are already linked, LNK2005 duplicate def (harmless but noisy) |
| A5 | All 36 runtime DLLs needed for Phase 4 are present in `exe/win32/` | DLL Staging | Missing DLLs would cause runtime crash but not link failure |
| A6 | The `sharedMathArchive` library target exists and is correctly wired | Standard Stack | LNK2019 on math archive symbols if missing from graph |

**A3 deep-dive:** The libEverQuestTCG.cpp uses LoadLibrary to load `CoreClient.dll` at runtime (standard pattern for this SDK type). The include `libEverQuestTCG.h` only references Windows types. The .cpp should compile cleanly with just Windows headers. [VERIFIED: read file to line 140, all includes are `<windows.h>`, `<stdlib.h>`, `<stdio.h>`, `<assert.h>`, `<malloc.h>`]

---

## Open Questions (RESOLVED)

1. **swgSharedNetworkMessages: does it have a `survey/` subdirectory?**
   - What we know: filesystem shows `survey/` exists with 2 cpp files (ResourceListForSurveyMessage.cpp, SurveyMessage.cpp)
   - What's unclear: the CONTEXT.md listed "combat/, consent/, core/, permissionList/" but missed survey/
   - Recommendation: Use 5 subdirs in the CMakeLists (not 4). The CONTEXT.md count of "10 cpp" matches: 3+1+1+2+2+1=10 including the win32 PCH file.
   - **RESOLVED:** Plan 04-02 uses 5 subdirs including survey/. Source file count confirmed: 10 cpp total.

2. **Does `sharedMathArchive` have a CMake target?**
   - What we know: `clientUserInterface/src/CMakeLists.txt` includes `sharedMathArchive/include/public` — it exists on disk
   - What's unclear: whether a CMake target named `sharedMathArchive` was authored in Phase 2 (the ROADMAP shows 22 shared libs; sharedMathArchive is not in the explicit list)
   - Recommendation: Check `src/engine/shared/library/CMakeLists.txt` for `add_subdirectory(sharedMathArchive)`. If missing, author it in Wave 0 of Phase 4.
   - **RESOLVED:** Confirmed present — `src/engine/shared/library/CMakeLists.txt` contains `add_subdirectory(sharedMathArchive)` (Phase 2 output). No Wave 0 action needed.

3. **`swgClientUserInterface` page count discrepancy:**
   - The CONTEXT.md says "266 cpp" and the filesystem verifies 266. The page directory likely has 228 cpp files (the largest subset). The planner should use `GLOB` to enumerate all files rather than manually listing each of the 228 page files, but only for this lib (GLOB is acceptable for large game-side libs where the set of files is stable and well-bounded).
   - **RESOLVED:** Plan 04-03 uses `file(GLOB_RECURSE ...)` for the page/ subdirectory; core/ and parser/ are listed explicitly. This matches the Pattern E/F from PATTERNS.md.

4. **Vivox service executable `VivoxVoiceService.exe`:**
   - What we know: it's referenced in the Vivox SDK documentation as a separate process
   - What's unclear: whether it must be staged alongside the exe for boot to proceed
   - Recommendation: It's not in `exe/win32/`. Do not stage it. Vivox is gated by config (`enableVoiceChat=0` — Phase 6 CONFIG-03 scope).
   - **RESOLVED:** Confirmed not present in exe/win32/. Plan 04-05 does not stage it. Vivox DLLs (vivoxsdk.dll, vivoxplatform.dll, ortp.dll, alut.dll, libsndfile-1.dll, wrap_oal.dll) staged separately.

---

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Visual Studio 2022 MSVC v143 | All C++ compilation | Assumed (Phase 3 completed) | v143 | — |
| CMake 3.27+ | Build system | Assumed (Phase 3 completed) | 3.27+ | — |
| exe/win32/*.dll (36 files) | POST_BUILD staging | Verified present | NGE-era | — |
| exe/win32/mozilla/ subtree | POST_BUILD staging | Verified present | Mozilla 1.x | — |
| src/external/3rd/library/libEverQuestTCG/ | swgClientUserInterface compile | Verified present (cpp + headers) | N/A | — |

**Missing dependencies with no fallback:** None identified.

**Missing dependencies with fallback:** None identified.

---

## Validation Architecture

> nyquist_validation is enabled in `.planning/config.json`.

### Test Framework

| Property | Value |
|----------|-------|
| Framework | None — no test framework present in this project (CLAUDE.md confirms; swgclient-build.md notes "Not detected") |
| Config file | N/A |
| Quick run command | N/A |
| Full suite command | N/A |

### Phase Requirements → Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| BUILD-04 | SwgClient.vcxproj generated with ~70 deps | Build — cmake --build | `cmake --build build --config Debug --target SwgClient` | N/A — build artifact |
| ARTIF-01 | `bin/Debug/SwgClient_d.exe` exists after Debug build | Build gate | `cmake --build build --config Debug && test -f build/bin/Debug/SwgClient_d.exe` | N/A |
| ARTIF-02 | `bin/Release/SwgClient_r.exe` exists after Release build | Build gate | `cmake --build build --config Release && test -f build/bin/Release/SwgClient_r.exe` | N/A |
| ARTIF-03 | Zero MSVCR*/VCRUNTIME*/xpcom/xul in dumpbin /imports | Manual validation | `dumpbin /imports build/bin/Debug/SwgClient_d.exe \| findstr -i "msvcr vcruntime xpcom xul"` | N/A |

### Sampling Rate

- **Per wave commit:** `cmake --build build --config Debug --target SwgClient` (build only)
- **Per wave merge:** Debug + Release builds both succeed, dumpbin /imports check passes
- **Phase gate:** Three consecutive clean parallel builds pass (SC-6 per ROADMAP.md)

### Wave 0 Gaps

No traditional test files needed — this is a build-system phase. All validation is via build artifacts and dumpbin inspection.

---

## Security Domain

> `security_enforcement: true` in `.planning/config.json`; `security_asvs_level: 1`

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no — build system phase only | — |
| V3 Session Management | no — build system phase only | — |
| V4 Access Control | no — build system phase only | — |
| V5 Input Validation | no — no runtime input processed in Phase 4 | — |
| V6 Cryptography | no — no crypto operations | — |

**Phase 4 is purely a build-system authoring phase.** No runtime code is written, no user input is processed, no network connections are made, and no data is persisted. ASVS categories do not apply.

**One security-adjacent note:** The placeholder `client.cfg` staged by POST_BUILD contains commented-out SOE server addresses. Confirm no live credentials or station auth tokens are in the template. The research confirmed `exe/win32/client_configs/login.cfg` only contains internal SOE server DNS names (defunct since 2011) — no passwords, no session tokens. The placeholder is safe to commit.

---

## Sources

### Primary (HIGH confidence)
- `docs/research/swgclient-build.md` — authoritative dependency graph, rsp file contents, compiler settings, DLL inventory
- `src/engine/client/library/clientUserInterface/src/CMakeLists.txt` — reference pattern for swgClientUserInterface include/link deps
- `src/engine/client/library/clientGame/src/CMakeLists.txt` — reference pattern for integrator lib link deps
- `src/cmake/win32/FindSTLPort.cmake` — exact STLPORT_LIBRARY variable names confirmed
- `src/cmake/win32/FindDirectX9.cmake`, `FindMiles.cmake`, `FindDPVS.cmake`, `FindVivox.cmake`, `FindVideoCapture.cmake`, `FindLogitechLCD.cmake`, `FindPCRE.cmake`, `FindLibXml2.cmake`, `FindZLIB.cmake`, `FindBink.cmake` — all variable names verified
- filesystem scan of `src/game/client/library/swgClientUserInterface/src/` — 266 cpp confirmed, 4 subdirs mapped
- filesystem scan of `src/game/shared/library/swgSharedNetworkMessages/src/` — 10 cpp confirmed, 5 subdirs mapped
- filesystem scan of `exe/win32/*.dll` — 46 DLLs found, client-runtime subset identified
- `src/external/3rd/library/libEverQuestTCG/src/win32/libEverQuestTCG.cpp` — read first 150 lines, confirmed Windows-only headers, no external SDK deps

### Secondary (MEDIUM confidence)
- `src/game/client/library/swgClientUserInterface/src/shared/core/SwgCuiManager.cpp` + `SwgCuiTcgManager.cpp` — include audit confirmed swgSharedNetworkMessages and libEverQuestTCG deps
- `.planning/phases/03-client-engine-libraries-sdk-heavy-tier/03-PATTERNS.md` — Phase 3 established patterns carry forward

### Tertiary (LOW confidence)
- Entry count estimate of "~70" from ROADMAP.md — actual CMake target_link_libraries count is ~44 named entries (the ~70 was the VS2005 project GUID count which included editor/tool projects)

---

## Project Constraints (from CLAUDE.md)

- Windows-only, VS 2022, CMake, C++17, MSVC v143 toolset
- Source-edit budget: effectively zero in v1 (FreeCamera.cpp static_cast is the only exception, already applied)
- STLPort 4.5.3 retained — no MSVC STL swap
- Static CRT: `/MT` Release / `/MTd` Debug via global `CMAKE_MSVC_RUNTIME_LIBRARY`
- No deadline — exploratory hobby cadence
- `/WX` (warnings-as-errors) NOT enabled in v1
- Original `.vcproj` files retained side-by-side (INV-02)
- Naming: `SwgClient_d.exe` (Debug), `SwgClient_r.exe` (Release)
- PCH: `First<Name>.h` pattern, `target_precompile_headers` on every target
- No Hungarian notation; PascalCase classes, camelCase methods
- Error handling: FATAL/WARNING macros only (no C++ exceptions in SWG source)

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all library targets and Find module variables verified from existing files
- Architecture: HIGH — patterns established in Phase 3 carry forward directly
- DLL staging inventory: HIGH — filesystem scan of exe/win32/ confirmed
- swgClientUserInterface include list: MEDIUM — modelled on clientUserInterface with additions; first compile may reveal gaps
- libEverQuestTCG link requirement: HIGH — source grep confirmed direct symbol calls, no existing CMake target

**Research date:** 2026-05-04
**Valid until:** 2026-06-04 (stable — no external APIs involved, all vendored)
