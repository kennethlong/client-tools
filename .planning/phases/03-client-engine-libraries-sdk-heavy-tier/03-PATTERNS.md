# Phase 3: Client Engine Libraries (SDK-Heavy Tier) — Pattern Map

**Mapped:** 2026-05-04
**Files analyzed:** 41 (27 new CMakeLists.txt files across 13 libs + 2 infra CMakeLists modifications + 2 external aggregator modifications + libMozilla inline target + ui target pair + libMozilla_stub.cpp)
**Analogs found:** 41 / 41 (all files have Phase 2 analogs; no novel patterns required)

---

## File Classification

| New / Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---------------------|------|-----------|----------------|---------------|
| `src/engine/CMakeLists.txt` (modify) | config — aggregator | N/A | `src/engine/shared/CMakeLists.txt` | exact |
| `src/engine/client/CMakeLists.txt` (new) | config — aggregator | N/A | `src/engine/shared/CMakeLists.txt` | exact |
| `src/engine/client/library/CMakeLists.txt` (new) | config — aggregator + INTERFACE stubs | N/A | `src/engine/shared/library/CMakeLists.txt` | exact |
| `src/external/3rd/library/CMakeLists.txt` (modify) | config — aggregator | N/A | same file (current state) | exact |
| `src/external/3rd/library/ui/CMakeLists.txt` (new) | config — top-level lib wrapper | N/A | `src/engine/shared/library/sharedFoundation/CMakeLists.txt` | exact |
| `src/external/3rd/library/ui/src/CMakeLists.txt` (new) | config — large multi-subdir build target | N/A | `src/engine/shared/library/sharedNetworkMessages/src/CMakeLists.txt` | role-match |
| `src/cmake/stubs/libMozilla_stub.cpp` (new) | utility — XPCOM no-op stub | N/A | `src/cmake/stubs/time_t_probe.cpp.in` (conceptually); `src/external/3rd/library/libMozilla/src/win32/libMozilla.h` (API source) | partial |
| `src/engine/client/library/clientAnimation/CMakeLists.txt` | config — top-level lib wrapper | N/A | `src/engine/shared/library/sharedFoundation/CMakeLists.txt` | exact |
| `src/engine/client/library/clientAnimation/src/CMakeLists.txt` | config — build target, no external SDK | N/A | `src/engine/shared/library/sharedCompression/src/CMakeLists.txt` (simple SDK dep pattern) | role-match |
| `src/engine/client/library/clientObject/CMakeLists.txt` | config — top-level lib wrapper | N/A | `src/engine/shared/library/sharedFoundation/CMakeLists.txt` | exact |
| `src/engine/client/library/clientObject/src/CMakeLists.txt` | config — build target, DPVS dep | N/A | `src/engine/shared/library/sharedXml/src/CMakeLists.txt` (SDK include + link pattern) | exact |
| `src/engine/client/library/clientTextureRenderer/CMakeLists.txt` | config — top-level lib wrapper | N/A | `src/engine/shared/library/sharedFoundation/CMakeLists.txt` | exact |
| `src/engine/client/library/clientTextureRenderer/src/CMakeLists.txt` | config — build target, no external SDK | N/A | `src/engine/shared/library/sharedCompression/src/CMakeLists.txt` | role-match |
| `src/engine/client/library/clientDirectInput/CMakeLists.txt` | config — top-level lib wrapper | N/A | `src/engine/shared/library/sharedFoundation/CMakeLists.txt` | exact |
| `src/engine/client/library/clientDirectInput/src/CMakeLists.txt` | config — build target, DX9 include dep | N/A | `src/engine/shared/library/sharedRegex/src/CMakeLists.txt` (external SDK include + link pattern) | exact |
| `src/engine/client/library/clientBugReporting/CMakeLists.txt` | config — top-level lib wrapper | N/A | `src/engine/shared/library/sharedFoundation/CMakeLists.txt` | exact |
| `src/engine/client/library/clientBugReporting/src/CMakeLists.txt` | config — win32-only sources (no SHARED_SOURCES cpp) | N/A | `src/engine/shared/library/sharedXml/src/CMakeLists.txt` + win32-only variant | role-match |
| `src/engine/client/library/clientParticle/CMakeLists.txt` | config — top-level lib wrapper | N/A | `src/engine/shared/library/sharedFoundation/CMakeLists.txt` | exact |
| `src/engine/client/library/clientParticle/src/CMakeLists.txt` | config — build target, DPVS dep | N/A | `src/engine/shared/library/sharedXml/src/CMakeLists.txt` | exact |
| `src/engine/client/library/clientGraphics/CMakeLists.txt` | config — top-level lib wrapper | N/A | `src/engine/shared/library/sharedFoundation/CMakeLists.txt` | exact |
| `src/engine/client/library/clientGraphics/src/CMakeLists.txt` | config — build target, DX9 + DPVS + Bink includes, DPVS link | N/A | `src/engine/shared/library/sharedXml/src/CMakeLists.txt` (multi-SDK variant) | role-match |
| `src/engine/client/library/clientAudio/CMakeLists.txt` | config — top-level lib wrapper | N/A | `src/engine/shared/library/sharedFoundation/CMakeLists.txt` | exact |
| `src/engine/client/library/clientAudio/src/CMakeLists.txt` | config — win32-only sources, Miles + VideoCapture SDK deps | N/A | `src/engine/shared/library/sharedXml/src/CMakeLists.txt` | role-match |
| `src/engine/client/library/clientSkeletalAnimation/CMakeLists.txt` | config — top-level lib wrapper | N/A | `src/engine/shared/library/sharedFoundation/CMakeLists.txt` | exact |
| `src/engine/client/library/clientSkeletalAnimation/src/CMakeLists.txt` | config — build target, DPVS dep, many subdirs | N/A | `src/engine/shared/library/sharedTerrain/src/CMakeLists.txt` (multi-subdir + SDK dep) | exact |
| `src/engine/client/library/clientTerrain/CMakeLists.txt` | config — top-level lib wrapper | N/A | `src/engine/shared/library/sharedFoundation/CMakeLists.txt` | exact |
| `src/engine/client/library/clientTerrain/src/CMakeLists.txt` | config — build target, DPVS dep, multiple subdirs | N/A | `src/engine/shared/library/sharedTerrain/src/CMakeLists.txt` | exact |
| `src/engine/client/library/clientUserInterface/CMakeLists.txt` | config — top-level lib wrapper | N/A | `src/engine/shared/library/sharedFoundation/CMakeLists.txt` | exact |
| `src/engine/client/library/clientUserInterface/src/CMakeLists.txt` | config — build target, multi-SDK (Mozilla, DX9, Vivox, lcdui) | N/A | `src/engine/shared/library/sharedNetworkMessages/src/CMakeLists.txt` (large + multi-dep) | role-match |
| `src/engine/client/library/clientGame/CMakeLists.txt` | config — top-level lib wrapper | N/A | `src/engine/shared/library/sharedFoundation/CMakeLists.txt` | exact |
| `src/engine/client/library/clientGame/src/CMakeLists.txt` | config — integrator, 343 files, 22 subdirs, all SDK deps | N/A | `src/engine/shared/library/sharedNetworkMessages/src/CMakeLists.txt` (large multi-subdir) | role-match |

---

## Pattern Assignments

### PATTERN A — Top-Level Library CMakeLists.txt (all 13 libs + ui)

**Analog:** `src/engine/shared/library/sharedFoundation/CMakeLists.txt` (lines 1–7)

Every library wrapper at `<libName>/CMakeLists.txt` is identical except for the project name:

```cmake
cmake_minimum_required(VERSION 2.8)

project(<LibName>)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/public)

add_subdirectory(src)
```

Apply this verbatim for all 13 client libs and for `ui` (with `project(ui)`).

**Note for `ui`:** The `include/public` directory exists at `src/external/3rd/library/ui/include/`. Use the same pattern; the `add_subdirectory(src)` descends into `src/` which holds `shared/` and `win32/`.

---

### PATTERN B — src/CMakeLists.txt: Standard SHARED + PLATFORM split

**Analog:** `src/engine/shared/library/sharedXml/src/CMakeLists.txt` (lines 1–53)

Full structure (adapt lib name, source list, include dirs, and link deps):

```cmake
set(SHARED_SOURCES
    shared/core/First<LibName>.h
    shared/core/Setup<LibName>.cpp
    shared/core/Setup<LibName>.h
    # ... all shared/*.cpp and *.h files listed explicitly ...
)

if(WIN32)
    set(PLATFORM_SOURCES
        win32/First<LibName>.cpp
    )
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/win32)
else()
    set(PLATFORM_SOURCES "")
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/shared
    ${SWG_ENGINE_SOURCE_DIR}/client/library/<LibName>/src/shared
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundation/include/public
    # ... other engine deps ...
    # ... external SDK include vars as needed (see PATTERN E) ...
)

add_library(<LibName> STATIC
    ${SHARED_SOURCES}
    ${PLATFORM_SOURCES}
)

target_link_libraries(<LibName>
    # engine deps
    # external SDK libs (see PATTERN E)
)

target_precompile_headers(<LibName> PRIVATE
    shared/core/First<LibName>.h
)
```

**PCH path variants** (not all libs use `shared/core/`):
- `clientAnimation`: `shared/core/FirstClientAnimation.h`
- `clientObject`: `shared/core/FirstClientObject.h`
- `clientTextureRenderer`: `shared/FirstClientTextureRenderer.h`
- `clientDirectInput`: `shared/FirstClientDirectInput.h`
- `clientBugReporting`: `shared/FirstClientBugReporting.h`
- `clientParticle`: `shared/FirstClientParticle.h`
- `clientGraphics`: `shared/FirstClientGraphics.h`
- `clientAudio`: `shared/FirstClientAudio.h`
- `clientSkeletalAnimation`: `shared/core/FirstClientSkeletalAnimation.h`
- `clientTerrain`: `shared/core/FirstClientTerrain.h`
- `clientUserInterface`: `shared/core/FirstClientUserInterface.h`
- `clientGame`: `shared/core/FirstClientGame.h`

---

### PATTERN C — Win32-Only Libraries (all cpp in win32/, no shared cpp)

**Analog:** `src/engine/shared/library/sharedXml/src/CMakeLists.txt` — adapt for the case where `SHARED_SOURCES` contains only `.h` files and all `.cpp` files go into `PLATFORM_SOURCES`.

Applies to: `clientBugReporting` (5 cpp in win32/) and `clientAudio` (20 cpp in win32/).

```cmake
set(SHARED_SOURCES
    shared/First<LibName>.h
    # headers only — no .cpp files here
)

if(WIN32)
    set(PLATFORM_SOURCES
        win32/First<LibName>.cpp
        win32/Foo.cpp
        win32/Foo.h
        # ... all win32/*.cpp listed here ...
    )
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/win32)
else()
    set(PLATFORM_SOURCES "")
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/shared
    ${SWG_ENGINE_SOURCE_DIR}/client/library/<LibName>/src/shared
    # ... engine deps ...
    # ... SDK includes ...
)

add_library(<LibName> STATIC
    ${SHARED_SOURCES}
    ${PLATFORM_SOURCES}
)

target_link_libraries(<LibName>
    # ...
)

target_precompile_headers(<LibName> PRIVATE
    shared/First<LibName>.h
)
```

**clientBugReporting note:** The PCH header `FirstClientBugReporting.h` lives at `src/shared/FirstClientBugReporting.h` (one level above `src/shared/core/`). The win32 PCH `.cpp` is at `src/win32/FirstClientBugReporting.cpp`. Only include paths needed are `sharedFoundation` and `sharedDebug` — no external SDK headers.

---

### PATTERN D — External SDK Include + Link (single SDK, e.g. DPVS)

**Analog:** `src/engine/shared/library/sharedXml/src/CMakeLists.txt` (lines 24–48)
**Secondary analog:** `src/engine/shared/library/sharedRegex/src/CMakeLists.txt` (lines 22–42)

The SDK variable goes into `include_directories(...)` and `target_link_libraries(...)`:

```cmake
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/shared
    # ... engine deps ...
    ${DPVS_INCLUDE_DIRS}    # expands to interface/ AND implementation/include/ — use the full var
)

add_library(<LibName> STATIC ${SHARED_SOURCES} ${PLATFORM_SOURCES})

target_link_libraries(<LibName>
    ${DPVS_LIBRARY}         # expands to: debug dpvsd.lib optimized dpvs.lib
    sharedFoundation
    # ...
)

target_precompile_headers(<LibName> PRIVATE shared/core/First<LibName>.h)
```

**Apply to (DPVS):** `clientObject`, `clientParticle`, `clientGraphics`, `clientSkeletalAnimation`, `clientTerrain`, `clientGame`

**`${DPVS_LIBRARY}` expands to:** `debug ${DPVS_DEBUG_LIBRARY} optimized ${DPVS_RELEASE_LIBRARY}` — i.e., CMake selects `dpvsd.lib` for Debug and `dpvs.lib` for Release automatically. This satisfies D-02.

---

### PATTERN E — SDK Include + Link Reference Table

All `find_package` calls are already in `src/CMakeLists.txt`. Phase 3 libs consume these variables directly. Do NOT add new `find_package` calls in any library CMakeLists.

| SDK Variable | Use In `include_directories` | Use In `target_link_libraries` | Applies To |
|---|---|---|---|
| `${DPVS_INCLUDE_DIRS}` | YES — both paths in one var | `${DPVS_LIBRARY}` | clientObject, clientParticle, clientGraphics, clientSkeletalAnimation, clientTerrain, clientGame |
| `${DIRECTX9_INCLUDE_DIR}` | YES | `${DIRECTX9_DINPUT8_LIBRARY}` `${DIRECTX9_DXGUID_LIBRARY}` | clientDirectInput |
| `${DIRECTX9_INCLUDE_DIR}` | YES | none (d3d9.lib not needed at static-lib stage) | clientGraphics |
| `${BINK_INCLUDE_DIR}` | YES (for BinkDLL.h type decls) | NONE — LoadLibrary pattern, do NOT link binkw32.lib | clientGraphics |
| `${MILES_INCLUDE_DIR}` | YES | `${MILES_LIBRARY}` | clientAudio |
| `${VIDEOCAPTURE_ROOT}` (= `${SWG_EXTERNALS_FIND}/videocapture`) | YES — as include root so `AudioCapture/AudioCapture.h` resolves | `${VIDEOCAPTURE_LIBRARY}` | clientAudio |
| `${MOZILLA_PUBLIC_INCLUDE_DIR}` | YES | target `libMozilla` (stub) | clientUserInterface, clientGame, libMozilla stub target |
| `${VIVOX_INCLUDE_DIRS}` | YES | `${VIVOX_LIBRARIES}` | clientUserInterface |
| `${LOGITECHLCD_INCLUDE_DIR}` | YES | `${LOGITECHLCD_LIBRARY}` | clientUserInterface |
| `${SWG_EXTERNALS_FIND}/ui/include` | YES | target `ui` | clientUserInterface, clientGame |
| `${SWG_EXTERNALS_FIND}/ui/src/win32` | YES (for `_precompile.h`) | — | clientUserInterface, clientGame |

**NEVER use `${MOZILLA_PRIVATE_INCLUDE_DIR}` or `${MOZILLA_INCLUDE_DIRS}` (the combined set).** Only `${MOZILLA_PUBLIC_INCLUDE_DIR}` — the private path contains real XPCOM headers that fail under modern MSVC.

**NEVER link `${BINK_LIBRARY}`.** Bink uses `LoadLibrary`/`GetProcAddress` at runtime. The include path is needed for header type declarations only.

---

### PATTERN F — Multi-SDK Target (clientGraphics)

**Analog:** `src/engine/shared/library/sharedXml/src/CMakeLists.txt` — extended with multiple SDK includes and the D-12 C4530 suppression.

```cmake
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/shared
    ${CMAKE_CURRENT_SOURCE_DIR}/Bink          # Bink/ subdir in clientGraphics src
    ${SWG_ENGINE_SOURCE_DIR}/client/library/clientGraphics/src/shared
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundation/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFile/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedDebug/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedObject/include/public
    ${DIRECTX9_INCLUDE_DIR}
    ${DPVS_INCLUDE_DIRS}
    ${BINK_INCLUDE_DIR}
)

add_library(clientGraphics STATIC ${SHARED_SOURCES} ${PLATFORM_SOURCES})

target_link_libraries(clientGraphics
    ${DPVS_LIBRARY}
    sharedFoundation
    sharedFile
    sharedDebug
    sharedObject
)

target_compile_options(clientGraphics PRIVATE /wd4530)    # D-12

target_precompile_headers(clientGraphics PRIVATE shared/FirstClientGraphics.h)

# D-13: If DX9/WinSDK conflicts appear (C2146/C4005), activate:
# target_compile_definitions(clientGraphics PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
```

---

### PATTERN G — INTERFACE Stubs in Library Aggregator (inline, no subdirectory)

**Analog:** `src/engine/shared/library/CMakeLists.txt` (lines 7–22)

Used in Phase 2 for `sharedCollision`, `sharedFractal`, `sharedSkillSystem`. Apply same pattern in `src/engine/client/library/CMakeLists.txt` for `libMozilla`:

```cmake
# libMozilla XPCOM stub — builds only src/cmake/stubs/libMozilla_stub.cpp.
# Real src/external/3rd/library/libMozilla/src/win32/libMozilla.cpp is NOT compiled (D-05).
add_library(libMozilla STATIC
    ${CMAKE_SOURCE_DIR}/cmake/stubs/libMozilla_stub.cpp
)
target_include_directories(libMozilla PUBLIC
    ${MOZILLA_PUBLIC_INCLUDE_DIR}
)
# Do NOT add MOZILLA_PRIVATE_INCLUDE_DIR — it contains real XPCOM headers (D-07).
```

Then immediately below: `add_subdirectory(clientAnimation)` through `add_subdirectory(clientGame)` in tier order.

---

### PATTERN H — Aggregator CMakeLists.txt (src/engine/shared/library/ pattern)

**Analog:** `src/engine/shared/library/CMakeLists.txt` (lines 1–46) and `src/engine/shared/CMakeLists.txt`

**`src/engine/CMakeLists.txt` (modify — add one line):**
```cmake
add_subdirectory(shared)
add_subdirectory(client)    # ADD THIS
```

**`src/engine/client/CMakeLists.txt` (new):**
```cmake
add_subdirectory(library)
```

**`src/engine/client/library/CMakeLists.txt` (new):**
```cmake
if(WHITENGOLD_USE_STLPORT_HEADERS)
    include_directories(BEFORE ${STLPORT_INCLUDE_DIR})
endif()

# libMozilla XPCOM stub (inline target — no subdirectory)
add_library(libMozilla STATIC
    ${CMAKE_SOURCE_DIR}/cmake/stubs/libMozilla_stub.cpp
)
target_include_directories(libMozilla PUBLIC
    ${MOZILLA_PUBLIC_INCLUDE_DIR}
)

# Tier 6 (basic, no HIGH RISK SDK)
add_subdirectory(clientAnimation)
add_subdirectory(clientTextureRenderer)
add_subdirectory(clientBugReporting)
add_subdirectory(clientDirectInput)
add_subdirectory(clientObject)
add_subdirectory(clientParticle)

# Tier 7 HIGH RISK (DX9 + DPVS + Miles)
add_subdirectory(clientGraphics)
add_subdirectory(clientAudio)

# Tier 8
add_subdirectory(clientSkeletalAnimation)
add_subdirectory(clientTerrain)

# Tier 9
add_subdirectory(clientUserInterface)

# Integrator
add_subdirectory(clientGame)
```

**`src/external/3rd/library/CMakeLists.txt` (modify — add one line):**
```cmake
if(WHITENGOLD_USE_STLPORT_HEADERS)
    include_directories(BEFORE ${STLPORT_INCLUDE_DIR})
endif()

add_subdirectory(udplibrary)
add_subdirectory(ui)    # ADD THIS
```

---

### PATTERN I — Large Multi-Subdir Build Target (sharedNetworkMessages pattern)

**Analog:** `src/engine/shared/library/sharedNetworkMessages/src/CMakeLists.txt` (338 files, 9 subdirs)
**Secondary analog:** `src/engine/shared/library/sharedTerrain/src/CMakeLists.txt` (multi-subdir + SDK dep)

Apply to: `clientGame` (343 files, 22 subdirs) and `clientUserInterface` (152 files, 4 subdirs) and `ui` (126 files, 2 subdirs).

The structure is identical to PATTERN B — just with a longer `SHARED_SOURCES` list covering all subdirectories:

```cmake
set(SHARED_SOURCES
    shared/core/FirstClientGame.h
    shared/core/Foo.cpp
    shared/core/Foo.h

    shared/animation/Bar.cpp
    shared/animation/Bar.h

    shared/appearance/Baz.cpp
    # ... all 343 files across 22 subdirs ...

    shared/space/Qux.cpp
    shared/space/Qux.h
)

if(WIN32)
    set(PLATFORM_SOURCES
        win32/FirstClientGame.cpp
    )
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/win32)
else()
    set(PLATFORM_SOURCES "")
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/shared
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/core
    # ... all engine lib include/public dirs ...
    # ... all SDK include vars (DPVS, MOZILLA_PUBLIC, UI lib) ...
)

add_library(clientGame STATIC
    ${SHARED_SOURCES}
    ${PLATFORM_SOURCES}
)

target_link_libraries(clientGame
    # all Phase 3 libs + Phase 2 libs listed explicitly (no transitive inference)
    clientGraphics clientAudio clientObject clientAnimation
    clientSkeletalAnimation clientTerrain clientParticle
    clientTextureRenderer clientDirectInput clientUserInterface
    sharedFoundation sharedFile sharedGame sharedObject sharedTerrain
    sharedNetwork sharedNetworkMessages sharedUtility sharedMath
    libMozilla ui
    ${DPVS_LIBRARY}
)

target_compile_options(clientGame PRIVATE /wd4530)    # D-12

target_precompile_headers(clientGame PRIVATE shared/core/FirstClientGame.h)
```

**clientGame subdir file counts (from RESEARCH.md — use as completeness check):**
- `shared/core/` = 65 files
- `shared/object/` = 59 files
- `shared/objectTemplate/` = 41 files
- `shared/playback/` = 38 files
- `shared/space/` = 33 files
- `shared/scene/` = 15 files
- Remaining 18 subdirs = ~92 files combined

---

### PATTERN J — ui Static Library (external/3rd path, SOE-authored)

**Analog:** `src/external/3rd/library/udplibrary/CMakeLists.txt` (external/3rd style) combined with PATTERN B structure.

The `ui` lib lives at `src/external/3rd/library/ui/`. It follows the same PATTERN A / PATTERN B split but uses different base paths.

**`src/external/3rd/library/ui/CMakeLists.txt`:**
```cmake
cmake_minimum_required(VERSION 2.8)

project(ui)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(src)
```

**`src/external/3rd/library/ui/src/CMakeLists.txt`:**
```cmake
set(SHARED_SOURCES
    shared/UIBaseObject.cpp
    shared/UIBaseObject.h
    # ... all 100+ shared/*.cpp files ...
)

if(WIN32)
    set(PLATFORM_SOURCES
        win32/_precompile.h
        win32/UiReport.cpp
        # ... win32-specific files ...
    )
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/win32)
else()
    set(PLATFORM_SOURCES "")
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/shared
    ${CMAKE_CURRENT_SOURCE_DIR}/win32         # for _precompile.h resolution
    ${SWG_EXTERNALS_FIND}/ui/include          # public UI headers
    ${SWG_EXTERNALS_FIND}/ui/src/win32        # _precompile.h redirect target
)

add_library(ui STATIC
    ${SHARED_SOURCES}
    ${PLATFORM_SOURCES}
)

target_precompile_headers(ui PRIVATE ...)    # if ui has a First*.h; else omit
```

**Note:** The `_precompile.h` at `ui/include/_precompile.h` is a one-liner redirect: `#include "../src/win32/_precompile.h"`. Both the `include/` and `src/win32/` paths must be in the include directories so the redirect resolves and the inner includes (`UIStlFwd.h`, `UiReport.h`) resolve from the correct location.

---

### PATTERN K — libMozilla_stub.cpp Content

**Source of API:** `src/external/3rd/library/libMozilla/src/win32/libMozilla.h` (direct read — full public API)

**Analog:** There is no existing stub `.cpp` in the tree with the same shape. The pattern is informed by the RESEARCH.md stub template, which was verified against `libMozilla.h`.

```cpp
// src/cmake/stubs/libMozilla_stub.cpp
// STUB: no-op implementation of the libMozilla namespace for Phase 3 build.
// The real implementation (external/3rd/library/libMozilla/src/win32/libMozilla.cpp)
// requires XPCOM headers that cannot compile under modern MSVC (D-05).
// Only Game.cpp calls libMozilla::init, update, release directly.
// createWindow returns nullptr so Window methods are never reached.

#include "libMozilla/libMozilla.h"

namespace libMozilla
{
    bool    init( void * /*pNativeWindow*/, const char * /*szApplicationDirectory*/ ) { return true; }
    void    update() {}
    void    release() {}

    void    enableMemoryCache( bool /*bEnable*/ ) {}
    void    enableDiskCache( bool /*bEnable*/, unsigned /*uMaxSizeKB*/ ) {}
    void    setUserAgent( const char * /*pUserAgent*/ ) {}

    Window *createWindow( unsigned /*width*/, unsigned /*height*/ ) { return 0; }
    void    destroyWindow( Window * /*pWindow*/ ) {}
}
```

**Include path needed to compile the stub:** `${MOZILLA_PUBLIC_INCLUDE_DIR}` — which resolves to `src/external/3rd/library/libMozilla/include/public/` — so `#include "libMozilla/libMozilla.h"` finds `libMozilla/libMozilla.h` under that directory.

**Do NOT include** `${MOZILLA_PRIVATE_INCLUDE_DIR}` in the stub target's include dirs.

---

### PATTERN L — D-12: C4530 Suppression on All Phase 3 Client Targets

**Analog:** Inferred from D-12 decision; no existing example in Phase 2 (crypto was the deferred case).

Add to every Phase 3 client lib's `src/CMakeLists.txt` after `add_library(...)`:

```cmake
target_compile_options(<LibName> PRIVATE /wd4530)
```

Apply to: all 13 libs. Exception: `ui` and `libMozilla` stub may not need it — add defensively to all if any TU triggers the warning.

---

### PATTERN M — D-13: DX9 / Windows SDK Conflict Guard (conditional)

**Apply only if** C2146 or C4005 errors appear in `clientGraphics` or `clientDirectInput`:

```cmake
# D-13: add only to the offending target — NOT globally
target_compile_definitions(<LibName> PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
```

Do NOT add to root CMakeLists or any other target proactively.

---

## Shared Patterns

### Global Compile Definitions (DO NOT re-declare per-target)

**Source:** Root `src/CMakeLists.txt` (already set via `add_compile_definitions`)
**Apply to:** All targets (inherited automatically)

These are already global — each Phase 3 target gets them without any per-target declaration:
- `_USE_32BIT_TIME_T=1`
- `_MBCS`
- `PRODUCTION=0`
- `DEBUG_LEVEL=2`

### Global CRT (DO NOT override per-target)

**Source:** Root `src/CMakeLists.txt` — `CMAKE_MSVC_RUNTIME_LIBRARY` set to `MultiThreaded$<$<CONFIG:Debug>:Debug>`
**Apply to:** All targets automatically — `/MT` Release, `/MTd` Debug

Do NOT add `CMAKE_MSVC_RUNTIME_LIBRARY` or `/MT`/`/MTd` to any individual Phase 3 target.

### STLPort Include Injection (already global)

**Source:** `src/engine/shared/library/CMakeLists.txt` and `src/external/3rd/library/CMakeLists.txt` (lines 1–3)

Pattern already established:
```cmake
if(WHITENGOLD_USE_STLPORT_HEADERS)
    include_directories(BEFORE ${STLPORT_INCLUDE_DIR})
endif()
```

Copy this block into `src/engine/client/library/CMakeLists.txt` (the new aggregator). Individual lib `src/CMakeLists.txt` files do NOT need it — the aggregator-level `include_directories` propagates.

### PCH Pattern (mandatory on every lib)

**Source:** `src/engine/shared/library/sharedXml/src/CMakeLists.txt` (lines 50–52)

```cmake
target_precompile_headers(<LibName> PRIVATE
    shared/core/First<LibName>.h
)
```

Exactly one `target_precompile_headers` call per library target, after `add_library`. See PATTERN B for PCH path variants per library.

### Engine Include Path Variables

**Source:** All Phase 2 src/CMakeLists.txt files — established convention for referencing engine library public headers:

```cmake
# shared engine libs:
${SWG_ENGINE_SOURCE_DIR}/shared/library/<depLib>/include/public

# client engine libs (Phase 3 — each lib referencing another Phase 3 lib):
${SWG_ENGINE_SOURCE_DIR}/client/library/<depLib>/include/public

# ours/ external libs:
${SWG_EXTERNALS_SOURCE_DIR}/ours/library/<lib>/include/public

# 3rd party external libs:
${SWG_EXTERNALS_FIND}/<sdkname>/include   (or via Find module variable)
```

`SWG_ENGINE_SOURCE_DIR`, `SWG_EXTERNALS_SOURCE_DIR`, `SWG_EXTERNALS_FIND` are all set in root `src/CMakeLists.txt` and available to all subdirectory CMakeLists.

---

## No Analog Found

All Phase 3 files have Phase 2 structural analogs. The only novel element is the multi-SDK composition in `clientGraphics`, `clientAudio`, and `clientUserInterface` — but each of those is a straightforward composition of existing single-SDK patterns.

| File | Role | Data Flow | Reason |
|------|------|-----------|--------|
| (none) | — | — | All patterns covered by Phase 2 analogs |

---

## Critical Anti-Patterns (from RESEARCH.md — planner must enforce)

| Anti-Pattern | Effect | Guard |
|---|---|---|
| Link `${BINK_LIBRARY}` in clientGraphics | CRT conflict from DLL import lib vs /MT | NEVER add `${BINK_LIBRARY}` to `target_link_libraries` — header includes only |
| Add `${MOZILLA_PRIVATE_INCLUDE_DIR}` to any target | Cascading C1083 on nsCOMPtr.h, nsEmbedCID.h | Only `${MOZILLA_PUBLIC_INCLUDE_DIR}` ever appears in any include path |
| Compile `libMozilla/src/win32/libMozilla.cpp` | XPCOM symbol storm (NS_*, nsresult) | Only `libMozilla_stub.cpp` is ever compiled; the real .cpp is excluded |
| Add STLPort to individual `target_link_libraries` | Premature LNK2005 at static-lib stage | STLPort runtime linking is Phase 4 executable-link only (D-10) |
| Set `WIN32_LEAN_AND_MEAN`/`NOMINMAX` globally | Affects other targets unintentionally | Only on the specific offending target, only if C2146/C4005 appear (D-13) |
| Include only one of `DPVS_INCLUDE_DIRS` components | C1083 on dpvsWrapper.hpp or dpvsCommander.hpp | Always use `${DPVS_INCLUDE_DIRS}` (full variable — both paths) |
| Add `add_subdirectory(client)` after all engine targets configured | Tier ordering breaks — `ui` not found when client libs configure | Wave 0 ensures `src/external/3rd/library/CMakeLists.txt` gets `add_subdirectory(ui)` before `src/engine/CMakeLists.txt` gets `add_subdirectory(client)` — `external/` is processed first per root CMakeLists order |

---

## Metadata

**Analog search scope:** `src/engine/shared/library/*/src/CMakeLists.txt`, `src/external/3rd/library/*/CMakeLists.txt`, `src/external/ours/library/*/src/CMakeLists.txt`, `src/cmake/win32/Find*.cmake`
**Files scanned:** 20 CMakeLists files + 10 Find modules
**Pattern extraction date:** 2026-05-04
