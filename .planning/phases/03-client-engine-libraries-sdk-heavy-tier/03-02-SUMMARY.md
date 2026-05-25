---
phase: 03-client-engine-libraries-sdk-heavy-tier
plan: 02
subsystem: build-system
tags: [cmake, client-engine, tier6, dpvs, directx9, dinput, clientAnimation, clientBugReporting, clientDirectInput, clientObject, clientParticle, clientTextureRenderer]

# Dependency graph
requires:
  - phase: 03-01
    provides: "Wave 0 infrastructure: engine/client CMake subtree, 13 placeholder stubs, libMozilla.lib, ui.lib"
  - phase: 02
    provides: "Phase 2 STATIC targets (sharedFoundation, sharedGame, sharedObject, etc.)"
provides:
  - "clientAnimation STATIC target (.lib in Debug and Release)"
  - "clientBugReporting STATIC target (.lib in Debug and Release)"
  - "clientDirectInput STATIC target (.lib in Debug and Release) — D-11 DX9 proof"
  - "clientObject STATIC target (.lib in Debug and Release) — DPVS proof"
  - "clientTextureRenderer STATIC target (configure succeeds; build deferred until clientGraphics from Plan 03-03)"
  - "clientParticle STATIC target (configure succeeds; build deferred until clientGraphics from Plan 03-03)"
affects:
  - 03-03-PLAN

# Tech tracking
tech-stack:
  added:
    - "clientAnimation STATIC (16 cpp, shared/core/ + shared/playback/ + win32/)"
    - "clientTextureRenderer STATIC (12 cpp, shared/ + win32/, deps on clientGraphics deferred)"
    - "clientBugReporting STATIC (5 cpp, win32/ only — PATTERN C)"
    - "clientDirectInput STATIC (6 cpp, shared/ + win32/, vendored DX9 dinput.h)"
    - "clientObject STATIC (43 cpp, 7 subdirs, DPVS include + link)"
    - "clientParticle STATIC (30 cpp, shared/ + win32/, DPVS + clientGraphics deferred)"
  patterns:
    - "PATTERN A: top-level CMakeLists.txt wrapper (project + include/public + add_subdirectory)"
    - "PATTERN B: src/CMakeLists.txt with SHARED_SOURCES + PLATFORM_SOURCES split"
    - "PATTERN C: win32-only library (clientBugReporting — all cpp in PLATFORM_SOURCES)"
    - "PATTERN D: external SDK include + link (DIRECTX9_INCLUDE_DIR, DPVS_INCLUDE_DIRS)"
    - "include/private path required for clientAnimation (StopActionTemplate.h — private header not in include/public)"
    - "blat194/src/win32 include for clientBugReporting (Blat SMTP tool dependency)"
    - "archive/include (not archive/include/public) for SlotIdArchive.h dependency in clientObject"
    - "sharedIoWin/include/public needed by clientDirectInput and clientObject"

key-files:
  created:
    - src/engine/client/library/clientAnimation/src/CMakeLists.txt
    - src/engine/client/library/clientTextureRenderer/src/CMakeLists.txt
    - src/engine/client/library/clientBugReporting/src/CMakeLists.txt
    - src/engine/client/library/clientDirectInput/src/CMakeLists.txt
    - src/engine/client/library/clientObject/src/CMakeLists.txt
    - src/engine/client/library/clientParticle/src/CMakeLists.txt
  modified:
    - src/engine/client/library/clientAnimation/CMakeLists.txt
    - src/engine/client/library/clientTextureRenderer/CMakeLists.txt
    - src/engine/client/library/clientBugReporting/CMakeLists.txt
    - src/engine/client/library/clientDirectInput/CMakeLists.txt
    - src/engine/client/library/clientObject/CMakeLists.txt
    - src/engine/client/library/clientParticle/CMakeLists.txt

key-decisions:
  - "clientAnimation requires include/private path (include/private/clientAnimation/StopActionTemplate.h is a private header used by SetupClientAnimation.cpp as 'clientAnimation/StopActionTemplate.h')"
  - "clientObject requires clientGraphics/include/public in its include_directories (source code directly includes clientGraphics headers like Camera.h, RenderWorld.h, DebugPrimitive.h)"
  - "archive/include (not archive/include/public) is the correct path for Archive/Archive.h — this library uses a non-public directory layout"
  - "blat194/src/win32 include path needed for clientBugReporting (ToolBugReporting.cpp includes blat.h from vendored Blat SMTP tool)"
  - "sharedIoWin/include/public needed by both clientDirectInput (IoWinManager.h) and clientObject (IoWin.def)"
  - "D-13 NOT required: no C4005/C2146 DX9/Windows SDK conflict occurred in clientDirectInput — vendored dinput.h resolves cleanly without WIN32_LEAN_AND_MEAN"
  - "sharedMemoryManager/include/public added to all Tier 6 targets (FirstSharedFoundation.h pulls in MemoryManager.h)"

requirements-completed: [LAUNCH-02]

# Metrics
duration: 30min
completed: 2026-05-04
---

# Phase 3 Plan 02: Tier 6 Client Engine Libraries Summary

**6 Tier 6 CMakeLists pairs authored; 4 of 6 .lib artefacts produced immediately; DX9 dinput.h and DPVS header resolution both proven**

## Performance

- **Duration:** 30 min
- **Started:** 2026-05-04
- **Completed:** 2026-05-04
- **Tasks:** 3
- **Files modified/created:** 12

## Accomplishments

- Created 12 CMakeLists.txt files (6 PATTERN A wrappers + 6 src/ targets) replacing all 6 Tier 6 placeholder stubs
- Produced 4 .lib artefacts immediately in both Debug and Release: clientAnimation.lib, clientBugReporting.lib, clientDirectInput.lib, clientObject.lib
- D-11 PROVEN: vendored DX9 dinput.h resolves cleanly from DIRECTX9_INCLUDE_DIR without Windows SDK conflict (no C4005/C2146 errors; no WIN32_LEAN_AND_MEAN needed)
- DPVS PROVEN: dpvs*.hpp headers resolve via DPVS_INCLUDE_DIRS (both interface/ and implementation/include/ paths) in clientObject
- clientTextureRenderer and clientParticle configure successfully; build deferred until Plan 03-03 builds clientGraphics
- Phase 2 baseline verified: sharedGame still builds after adding 6 new targets to CMake graph

## Task Commits

Each task was committed atomically:

1. **Task 1: clientAnimation and clientTextureRenderer CMakeLists pairs** - `8d103f483` (feat)
2. **Task 2: clientBugReporting and clientDirectInput CMakeLists pairs (D-11 DX9 proof)** - `8e3382266` (feat)
3. **Task 3: clientObject and clientParticle CMakeLists pairs (DPVS proof)** - `13e988d53` (feat)

## Libs Produced

| Library | Debug | Release | Notes |
|---------|-------|---------|-------|
| clientAnimation.lib | YES | YES | 16 cpp, include/private needed |
| clientBugReporting.lib | YES | YES | 5 cpp win32-only (PATTERN C) |
| clientDirectInput.lib | YES | YES | DX9 dinput.h from vendored path (D-11) |
| clientObject.lib | YES | YES | 43 cpp, DPVS headers resolved |
| clientTextureRenderer.lib | deferred | deferred | depends on clientGraphics (Plan 03-03) |
| clientParticle.lib | deferred | deferred | depends on clientGraphics (Plan 03-03) |

## DX9 Proof (D-11)

clientDirectInput compiled successfully with `${DIRECTX9_INCLUDE_DIR}` in include_directories. The vendored `src/external/3rd/library/directx9/include/dinput.h` resolved without any Windows SDK macro conflict. No WIN32_LEAN_AND_MEAN or NOMINMAX definitions were needed (D-13 NOT triggered). clientDirectInput.lib produced in both Debug and Release.

## DPVS Proof

clientObject compiled successfully with `${DPVS_INCLUDE_DIRS}` (which expands to both `interface/` and `implementation/include/`). `dpvs.hpp`, `dpvsModel.hpp`, `dpvsObject.hpp` resolved from the vendored DPVS tree. clientObject.lib produced in both Debug and Release.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] sharedMemoryManager include path missing from all Tier 6 targets**
- **Found during:** Task 1 (clientAnimation build — C1083 on MemoryManager.h via FirstSharedFoundation.h)
- **Issue:** Plan's include_directories lists did not include `sharedMemoryManager/include/public`. FirstSharedFoundation.h (the PCH) transitively pulls in MemoryManager.h, requiring this path.
- **Fix:** Added `${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedMemoryManager/include/public` to all 6 Tier 6 src/CMakeLists.txt files.
- **Commit:** 8d103f483 (Task 1), 8e3382266 (Task 2), 13e988d53 (Task 3)

**2. [Rule 1 - Bug] clientAnimation include/private path missing**
- **Found during:** Task 1 (clientAnimation build — C1083 on clientAnimation/StopActionTemplate.h)
- **Issue:** `SetupClientAnimation.cpp` includes `clientAnimation/StopActionTemplate.h`. This header exists only at `include/private/clientAnimation/StopActionTemplate.h` (not in `include/public`). The plan's include list did not include the `include/private` directory.
- **Fix:** Added `${SWG_ENGINE_SOURCE_DIR}/client/library/clientAnimation/include/private` to clientAnimation/src/CMakeLists.txt.
- **Commit:** 8d103f483 (Task 1)

**3. [Rule 1 - Bug] Missing include paths in clientAnimation (fileInterface, sharedRandom, sharedUtility)**
- **Found during:** Task 1 (PlaybackScriptTemplateList.cpp — TreeFile.h needing fileInterface; PriorityPlaybackScriptManager.cpp needing sharedRandom)
- **Fix:** Added `fileInterface/include/public`, `sharedRandom/include/public`, `sharedUtility/include/public` to clientAnimation/src/CMakeLists.txt.
- **Commit:** 8d103f483

**4. [Rule 1 - Bug] blat194/src/win32 include path missing from clientBugReporting**
- **Found during:** Task 2 (clientBugReporting build — C1083 on blat.h)
- **Issue:** `ToolBugReporting.cpp` includes `blat.h` from the vendored Blat SMTP email tool at `src/external/3rd/library/blat194/src/win32/`. Plan described clientBugReporting as using "only Windows system headers" (D-15), but Blat is a third-party tool.
- **Fix:** Added `${SWG_EXTERNALS_FIND}/blat194/src/win32` to clientBugReporting/src/CMakeLists.txt.
- **Commit:** 8e3382266

**5. [Rule 1 - Bug] sharedIoWin include path missing from clientDirectInput**
- **Found during:** Task 2 (DirectInput.cpp — C1083 on sharedIoWin/IoWinManager.h)
- **Fix:** Added `${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedIoWin/include/public` to clientDirectInput/src/CMakeLists.txt.
- **Commit:** 8e3382266

**6. [Rule 1 - Bug] clientObject missing clientGraphics/include/public**
- **Found during:** Task 3 (clientObject build — C1083 on clientGraphics/DebugPrimitive.h, Camera.h, etc.)
- **Issue:** Plan description stated clientObject does not depend on clientGraphics at include level. However, several source files (MouseCursor.cpp, ShadowBlobManager.cpp, ReticleManager.cpp, etc.) directly include clientGraphics headers. clientGraphics/include/public is an include-only dependency at Tier 6 (the library itself will be linked from Plan 03-03).
- **Fix:** Added `${SWG_ENGINE_SOURCE_DIR}/client/library/clientGraphics/include/public` to clientObject/src/CMakeLists.txt include_directories (no change to target_link_libraries).
- **Commit:** 13e988d53

**7. [Rule 1 - Bug] archive include path uses archive/include not archive/include/public**
- **Found during:** Task 3 (clientObject build — SlotIdArchive.h needing Archive/Archive.h)
- **Issue:** The `archive` library uses `include/` (not `include/public`) as its root, so the correct path is `ours/library/archive/include`.
- **Fix:** Used `${SWG_EXTERNALS_SOURCE_DIR}/ours/library/archive/include` in clientObject/src/CMakeLists.txt.
- **Commit:** 13e988d53

**8. [Rule 1 - Bug] sharedIoWin include path missing from clientObject (IoWin.def)**
- **Found during:** Task 3 (MouseCursor.cpp — C1083 on sharedIoWin/IoWin.def)
- **Fix:** Added `${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedIoWin/include/public` to clientObject/src/CMakeLists.txt.
- **Commit:** 13e988d53

## Known Stubs

- `clientTextureRenderer` and `clientParticle`: CMakeLists pairs are complete with correct source lists and dependencies. Build will succeed once `clientGraphics` is built in Plan 03-03. These are not stubs — the CMakeLists are fully authored.

## Threat Flags

No new security-relevant surface introduced. All Tier 6 targets are static libraries in the build system graph. No network endpoints, auth paths, or file access patterns added.

## Self-Check: PASSED

All 12 CMakeLists.txt files verified to exist. All 4 immediately buildable .lib artefacts (Debug + Release) verified. All 3 task commits verified in git log.

---
*Phase: 03-client-engine-libraries-sdk-heavy-tier*
*Completed: 2026-05-04*
