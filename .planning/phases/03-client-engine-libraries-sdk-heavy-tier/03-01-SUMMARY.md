---
phase: 03-client-engine-libraries-sdk-heavy-tier
plan: 01
subsystem: build-system
tags: [cmake, libmozilla, xpcom-stub, ui-library, client-engine, wave0]

# Dependency graph
requires:
  - phase: 02-shared-engine-libraries
    provides: "Phase 2 STATIC targets (sharedFoundation, sharedGame, etc.) — libMozilla and ui link against these"
provides:
  - "libMozilla STATIC no-op stub target (8 namespace-level free functions, no XPCOM headers)"
  - "ui STATIC library (17MB .lib, 100+ SOE UI engine source files)"
  - "engine/client CMake subtree wired in (3-level aggregator chain)"
  - "13 placeholder CMakeLists.txt stubs for client libraries (Plans 02-05 scope)"
affects:
  - 03-02-PLAN
  - 03-03-PLAN
  - 03-04-PLAN
  - 03-05-PLAN

# Tech tracking
tech-stack:
  added:
    - "libMozilla STATIC (XPCOM stub, src/cmake/stubs/libMozilla_stub.cpp)"
    - "ui STATIC (external/3rd/library/ui/src/CMakeLists.txt, 100+ files)"
  patterns:
    - "PATTERN G: inline STATIC target in aggregator CMakeLists (libMozilla in engine/client/library/CMakeLists.txt)"
    - "PATTERN H: 3-level aggregator chain engine/ -> client/ -> library/"
    - "PATTERN J: external/3rd/ library with shared/ + win32/ source split"
    - "unicode include: ours/library/unicode/include (not include/public)"

key-files:
  created:
    - src/cmake/stubs/libMozilla_stub.cpp
    - src/engine/client/CMakeLists.txt
    - src/engine/client/library/CMakeLists.txt
    - src/external/3rd/library/ui/CMakeLists.txt
    - src/external/3rd/library/ui/src/CMakeLists.txt
    - "src/engine/client/library/client{Animation,Audio,BugReporting,DirectInput,Game,Graphics,Object,Particle,SkeletalAnimation,Terrain,TextureRenderer,UserInterface}/CMakeLists.txt (13 placeholder stubs)"
  modified:
    - src/engine/CMakeLists.txt
    - src/external/3rd/library/CMakeLists.txt

key-decisions:
  - "Unicode include path for ui target: ours/library/unicode/include (not include/public — that subdirectory does not exist)"
  - "UICanvasInitialization.cpp excluded from ui PLATFORM_SOURCES: uses DX7 DirectDraw headers and missing canvas generator types (UIDirect3DTextureCanvas.h, UIBitmapCanvasGenerator.h) not present in source tree"
  - "13 client library placeholder CMakeLists.txt stubs created to allow CMake configure to succeed at Wave 0; stubs to be replaced by Plans 02-05"
  - "ui SHARED_SOURCES includes all 6 subdirectories of shared/ (boundary/, core/, image/, loader/, property/, table/) — plan's source list only covered top-level shared/ files"

patterns-established:
  - "libMozilla STATIC target is inline in engine/client/library/CMakeLists.txt (no subdirectory) per PATTERN G"
  - "client/ aggregator follows engine/shared/ pattern exactly"
  - "ui target: no target_link_libraries (transitive symbols resolve at executable link)"
  - "Placeholder stub CMakeLists.txt: comment-only file enabling CMake configure without add_subdirectory of missing src/"

requirements-completed: [LAUNCH-02]

# Metrics
duration: 8min
completed: 2026-05-04
---

# Phase 3 Plan 01: Wave 0 Infrastructure Summary

**libMozilla XPCOM no-op stub + ui SOE UI engine static library + engine/client CMake subtree wired in; all 3 Wave 0 targets build successfully (libMozilla.lib, ui.lib 17MB)**

## Performance

- **Duration:** 8 min
- **Started:** 2026-05-04T17:33:28Z
- **Completed:** 2026-05-04T17:41:10Z
- **Tasks:** 3
- **Files modified:** 17 (2 modified, 15 created)

## Accomplishments

- Created `libMozilla_stub.cpp` with all 8 namespace-level no-op implementations — XPCOM symbol elimination complete (D-04/D-05)
- Wired `src/engine/client/` subtree into CMake graph via 3-level aggregator chain with libMozilla inline STATIC target and all 13 Phase 3 library subdirectories in tier order (D-11)
- Built `ui.lib` (17MB) from 100+ SOE UI engine source files — required include path fixes and one file exclusion (UICanvasInitialization.cpp)

## Task Commits

Each task was committed atomically:

1. **Task 1: Create libMozilla_stub.cpp (D-04/D-05)** - `ee226afa4` (feat)
2. **Task 2: Wire engine/client subtree into CMake graph (PATTERN H)** - `dc84f52d7` (feat)
3. **Task 3: Create ui library CMakeLists pair + wire into external/3rd (PATTERN J)** - `836db4a85` (feat)

## Files Created/Modified

- `src/cmake/stubs/libMozilla_stub.cpp` — XPCOM no-op stub; all 8 libMozilla namespace free functions
- `src/engine/CMakeLists.txt` — Modified: added `add_subdirectory(client)`
- `src/engine/client/CMakeLists.txt` — New: `add_subdirectory(library)` aggregator
- `src/engine/client/library/CMakeLists.txt` — New: libMozilla inline STATIC + STLPort guard + 13 tier-ordered add_subdirectory calls
- `src/external/3rd/library/CMakeLists.txt` — Modified: added `add_subdirectory(ui)`
- `src/external/3rd/library/ui/CMakeLists.txt` — New: project(ui) wrapper with include_directories(include)
- `src/external/3rd/library/ui/src/CMakeLists.txt` — New: 100+ file STATIC target (SHARED_SOURCES covering 6 subdirs + PLATFORM_SOURCES for win32/)
- `src/engine/client/library/client*/CMakeLists.txt` — 13 placeholder stubs (Plans 02-05 will replace)

## Decisions Made

- Unicode include path for `ui` target is `ours/library/unicode/include` not `include/public` — the public subdirectory does not exist in this library (verified by filesystem check)
- `UICanvasInitialization.cpp` excluded from `ui` PLATFORM_SOURCES: file uses DX7 DirectDraw 7 (`d3d.h`, `ddraw.h`) and references four missing header files (`UIDirect3DTextureCanvas.h`, `UIBitmapCanvasGenerator.h`, `UIDDSCanvasGenerator.h`, `UITGACanvasGenerator.h`) that do not exist in the source tree
- 13 client library placeholder CMakeLists.txt stubs created to prevent CMake configure failure — the plan's 13 `add_subdirectory()` calls require CMakeLists.txt presence even though Plans 02-05 author the full targets

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] ui shared/ source list was incomplete — 6 subdirectory trees missed**
- **Found during:** Task 3 (Create ui library CMakeLists pair)
- **Issue:** Plan's SHARED_SOURCES listed only top-level `shared/*.cpp` files. Actual `shared/` directory contains 6 subdirectories (boundary/, core/, image/, loader/, property/, table/) with many additional .cpp files. Plan only listed ~20 top-level shared files; actual count is 40+ files in subdirectories.
- **Fix:** Verified actual directory structure and included all 6 subdirectory trees in SHARED_SOURCES with correct paths (e.g., `shared/boundary/UIBoundary.cpp`, `shared/core/SetupUi.cpp`, etc.)
- **Files modified:** `src/external/3rd/library/ui/src/CMakeLists.txt`
- **Committed in:** 836db4a85 (Task 3 commit)

**2. [Rule 3 - Blocking] 13 placeholder CMakeLists.txt stubs required to allow CMake configure**
- **Found during:** Task 3 (CMake configure verification)
- **Issue:** Plan specified all 13 `add_subdirectory(client*)` calls in `engine/client/library/CMakeLists.txt` but did not create CMakeLists.txt files for those subdirectories (they're Plans 02-05 scope). CMake configure fails with "does not contain a CMakeLists.txt file" for each.
- **Fix:** Created minimal comment-only CMakeLists.txt stub for each of the 13 client libraries. Each stub is a single comment line indicating Plans 02-05 will replace it.
- **Files modified:** 13 stub files under `src/engine/client/library/`
- **Committed in:** 836db4a85 (Task 3 commit)

**3. [Rule 1 - Bug] ui PLATFORM_SOURCES contained 2 wrong file paths**
- **Found during:** Task 3 (CMake generate step)
- **Issue:** Plan listed `win32/UiMemoryBlockManager.h` and `win32/UiReport.h` in PLATFORM_SOURCES. These files do not exist in `src/win32/` — they are in `src/shared/core/` and `include/` respectively.
- **Fix:** Removed both non-existent win32/ paths from PLATFORM_SOURCES.
- **Files modified:** `src/external/3rd/library/ui/src/CMakeLists.txt`
- **Committed in:** 836db4a85 (Task 3 commit)

**4. [Rule 1 - Bug] unicode include path wrong in ui CMakeLists**
- **Found during:** Task 3 (build attempt — C1083 on UnicodeUtils.h)
- **Issue:** Plan specified `${SWG_EXTERNALS_SOURCE_DIR}/ours/library/unicode/include/public` but the `public/` subdirectory does not exist under `unicode/include/`. UnicodeUtils.h is directly in `unicode/include/`.
- **Fix:** Changed include path to `${SWG_EXTERNALS_SOURCE_DIR}/ours/library/unicode/include`.
- **Files modified:** `src/external/3rd/library/ui/src/CMakeLists.txt`
- **Committed in:** 836db4a85 (Task 3 commit)

**5. [Rule 1 - Bug] UICanvasInitialization.cpp uses missing DX7 canvas headers**
- **Found during:** Task 3 (ui build — C1083 on UIDirect3DTextureCanvas.h)
- **Issue:** `UICanvasInitialization.cpp` includes `<d3d.h>` (DX7), `<ddraw.h>` (DX7), and four missing headers: `UIDirect3DTextureCanvas.h`, `UIBitmapCanvasGenerator.h`, `UIDDSCanvasGenerator.h`, `UITGACanvasGenerator.h`. None of these exist in the source tree — they are legacy DX7 canvas factory implementations not present in the ~2010 NGE-era tree.
- **Fix:** Excluded `UICanvasInitialization.cpp` from PLATFORM_SOURCES with an explanatory comment.
- **Files modified:** `src/external/3rd/library/ui/src/CMakeLists.txt`
- **Committed in:** 836db4a85 (Task 3 commit)

---

**Total deviations:** 5 auto-fixed (3 Rule 1 bugs, 1 Rule 3 blocking, 1 Rule 3 blocking)
**Impact on plan:** All auto-fixes were necessary for correctness — incomplete source lists and wrong include paths. No scope creep. ui.lib builds at 17MB.

## Issues Encountered

- CMake generate step discovered `include/UIDirect3DTextureCanvas.h` is a redirect to a non-existent `src/win32/` file. Root cause: the `include/` header is a project-level forward declaration that expects implementation files not shipped in this source tree release (DX7-era canvas factory). Excluded the single .cpp file that pulled this chain in; all other ui files compile cleanly.

## Known Stubs

- 13 `src/engine/client/library/client*/CMakeLists.txt` stub files: comment-only placeholders. Each will be replaced by a full STATIC target definition in Plans 02-05. These stubs are intentional — they gate CMake configure success at Wave 0.

## Threat Flags

No new security-relevant surface introduced. libMozilla stub has no network code, no auth, no user data. ui is SOE-authored UI engine with no network calls.

## Next Phase Readiness

- Wave 0 complete: libMozilla.lib and ui.lib build; CMake configure clean
- Plans 02-05 can now author full `client*.lib` targets by replacing the 13 placeholder stubs
- All CMake variables (MOZILLA_PUBLIC_INCLUDE_DIR, DPVS_INCLUDE_DIRS, etc.) verified resolvable at configure time

## Self-Check

Verified after writing SUMMARY.md.

---
*Phase: 03-client-engine-libraries-sdk-heavy-tier*
*Completed: 2026-05-04*
