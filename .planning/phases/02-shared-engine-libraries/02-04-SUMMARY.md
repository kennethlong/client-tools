---
phase: 02-shared-engine-libraries
plan: 04
subsystem: build-system
tags: [cmake, tier4-libs, sharedCompression, sharedRegex, sharedImage, sharedIoWin, sharedMessageDispatch, zlib, pcre, vs2022]

# Dependency graph
requires:
  - phase: 02-shared-engine-libraries
    plan: 03
    provides: Tier 3 foundation libs, sharedMemoryManager include path pattern
provides:
  - sharedCompression STATIC lib (build/lib/Debug/sharedCompression.lib) — Tier 4 zlib-backed BitStream, Compressor, Lz77, ZlibCompressor
  - sharedRegex STATIC lib (build/lib/Debug/sharedRegex.lib) — Tier 4 PCRE wrapper
  - sharedImage STATIC lib (build/lib/Debug/sharedImage.lib) — Tier 4 Image, ImageFormat, TargaFormat
  - sharedIoWin STATIC lib (build/lib/Debug/sharedIoWin.lib) — Tier 4 IoWin input window manager (IoWin.def W-only resource)
  - sharedMessageDispatch STATIC lib (build/lib/Debug/sharedMessageDispatch.lib) — Tier 4 Emitter, Receiver, Transceiver, MessageManager
affects:
  - 02-05 (sharedNetwork depends on sharedMessageDispatch)
  - 02-06 (top-tier libs depend on sharedCompression via sharedFile)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "PATTERN A top-level entry for all 5 new libs"
    - "PATTERN B compiled lib with SHARED_SOURCES + PLATFORM_SOURCES — sharedImage, sharedIoWin, sharedMessageDispatch"
    - "PATTERN G external dep linkage — sharedCompression links ${ZLIB_LIBRARY}, sharedRegex links ${PCRE_LIBRARY}"
    - "sharedMemoryManager/include/public required by all whitengold Tier 4 libs — PCH chain FirstSharedFoundation.h -> MemoryManager.h"
    - "IoWin.def listed in SHARED_SOURCES — CMake treats .def as resource, not passed to C++ compiler; matches swg-main"
    - "sharedIoWin has no include_directories(win32) entry — mirrors swg-main exactly (win32-only source named directly)"

key-files:
  created:
    - src/engine/shared/library/sharedCompression/CMakeLists.txt
    - src/engine/shared/library/sharedCompression/src/CMakeLists.txt
    - src/engine/shared/library/sharedRegex/CMakeLists.txt
    - src/engine/shared/library/sharedRegex/src/CMakeLists.txt
    - src/engine/shared/library/sharedImage/CMakeLists.txt
    - src/engine/shared/library/sharedImage/src/CMakeLists.txt
    - src/engine/shared/library/sharedIoWin/CMakeLists.txt
    - src/engine/shared/library/sharedIoWin/src/CMakeLists.txt
    - src/engine/shared/library/sharedMessageDispatch/CMakeLists.txt
    - src/engine/shared/library/sharedMessageDispatch/src/CMakeLists.txt
  modified:
    - src/engine/shared/library/CMakeLists.txt (5 new add_subdirectory calls)

key-decisions:
  - "sharedMemoryManager/include/public added to all Tier 4 libs — PCH chain FirstShared*.h -> FirstSharedFoundation.h -> MemoryManager.h requires this include path; swg-main omits it because swg-main does not use sharedMemoryManager, whitengold does"
  - "IoWin.def listed in SHARED_SOURCES — CMake includes .def files in the target source list as resource/module-definition files; they are not compiled as C++; matches swg-main treatment"
  - "sharedIoWin: no win32 include_directories — all platform sources named directly without relying on implicit directory inclusion; mirrors swg-main pattern precisely"
  - "sharedImage, sharedIoWin, sharedMessageDispatch: no target_link_libraries — true Tier 4 mid-tier libs that expose APIs without linking engine libs themselves"

# Metrics
duration: 4min
completed: 2026-05-04
---

# Phase 2 Plan 04: Tier 4 Mid-Tier Libraries Summary

**10 new CMakeLists files for 5 Tier 4 mid-tier libs — sharedCompression (zlib), sharedRegex (PCRE), sharedImage, sharedIoWin (with IoWin.def), sharedMessageDispatch — all building as static .libs under VS 2022 with the established UCRT/STLPort compat infrastructure from Plans 01-03**

## Performance

- **Duration:** 4 min
- **Started:** 2026-05-04T13:27:53Z
- **Completed:** 2026-05-04T13:31:37Z
- **Tasks:** 2
- **Files modified:** 11 (10 new CMakeLists + 1 updated library CMakeLists)

## Accomplishments

- sharedCompression CMakeLists pair authored — PATTERN G external dep linkage against ${ZLIB_LIBRARY}; includes ${ZLIB_INCLUDE_DIR}; sharedSynchronization include path for cross-headers
- sharedRegex CMakeLists pair authored — PATTERN G external dep linkage against ${PCRE_LIBRARY}; includes ${PCRE_INCLUDE_DIR}; WIN32-only RegexServices.cpp/h in PLATFORM_SOURCES
- sharedImage CMakeLists pair authored — PATTERN B no-link-deps lib; includes fileInterface/include/public for AbstractFile/AbstractFileFactory used by image loaders
- sharedIoWin CMakeLists pair authored — IoWin.def in SHARED_SOURCES (W-only file, CMake resource handling); no win32 include_dir entry (matches swg-main)
- sharedMessageDispatch CMakeLists pair authored — PATTERN B no-link-deps lib; Emitter/Receiver/Transceiver/MessageManager classes; required by sharedNetwork (Plan 05)
- All 5 Tier 4 libs build successfully as static .libs
- Full `cmake --build build --config Debug --parallel 8` succeeds — all 26 .lib files (16 Phase 2 + 10 Phase 1) retained
- Phase 2 cumulative lib count: 16 (5 new + 11 from Plans 01-03)

## Task Commits

1. **Task 1: sharedCompression + sharedRegex CMakeLists pairs** — `1b8896a26` (feat)
2. **Task 2: sharedImage + sharedIoWin + sharedMessageDispatch CMakeLists pairs** — `fa480859b` (feat)

## Files Created/Modified

- `src/engine/shared/library/sharedCompression/CMakeLists.txt` — PATTERN A top-level entry
- `src/engine/shared/library/sharedCompression/src/CMakeLists.txt` — PATTERN G external dep, links ${ZLIB_LIBRARY}, includes sharedMemoryManager
- `src/engine/shared/library/sharedRegex/CMakeLists.txt` — PATTERN A top-level entry
- `src/engine/shared/library/sharedRegex/src/CMakeLists.txt` — PATTERN G external dep, links ${PCRE_LIBRARY}, includes sharedMemoryManager
- `src/engine/shared/library/sharedImage/CMakeLists.txt` — PATTERN A top-level entry
- `src/engine/shared/library/sharedImage/src/CMakeLists.txt` — PATTERN B no-link-deps, includes fileInterface, includes sharedMemoryManager
- `src/engine/shared/library/sharedIoWin/CMakeLists.txt` — PATTERN A top-level entry
- `src/engine/shared/library/sharedIoWin/src/CMakeLists.txt` — PATTERN B no-link-deps, IoWin.def in SHARED_SOURCES, no win32 include_dir, includes sharedMemoryManager
- `src/engine/shared/library/sharedMessageDispatch/CMakeLists.txt` — PATTERN A top-level entry
- `src/engine/shared/library/sharedMessageDispatch/src/CMakeLists.txt` — PATTERN B no-link-deps, includes sharedMemoryManager
- `src/engine/shared/library/CMakeLists.txt` — 5 new add_subdirectory calls (sharedCompression, sharedRegex, sharedImage, sharedIoWin, sharedMessageDispatch)

## Decisions Made

- **sharedMemoryManager include path required by all Tier 4 libs:** The PCH chain `FirstShared<Lib>.h` -> `FirstSharedFoundation.h` -> `MemoryManager.h` requires `sharedMemoryManager/include/public` in every lib's include paths. swg-main omits this because swg-main does not have sharedMemoryManager (it was removed); whitengold retains it. This pattern was established in Plan 03 for Tier 3 libs and extends uniformly to all Tier 4 libs.
- **IoWin.def treatment:** Listed in SHARED_SOURCES without any special handling. CMake recognizes `.def` extension as a linker module-definition file and includes it in the target without passing it to cl.exe. Matches swg-main treatment verbatim.
- **sharedIoWin: no include_directories(win32):** The win32 PLATFORM_SOURCES entry (`win32/FirstSharedIoWin.cpp`) doesn't depend on any headers that are exclusively in the win32 subdirectory; the sole win32 file only includes the First header from shared/. swg-main also omits the win32 include_dir for this lib specifically.
- **Three libs with no target_link_libraries:** sharedImage, sharedIoWin, sharedMessageDispatch are true Tier 4 mid-tier libs that provide APIs consumed by higher tiers but don't link lower-tier engine libs themselves. This is the correct dependency discipline — all linkage happens in the consuming libs.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Missing sharedMemoryManager/include/public in sharedCompression and sharedRegex**
- **Found during:** Task 1 (sharedCompression build)
- **Issue:** PCH compilation failed with C1083 "Cannot open include file: 'sharedMemoryManager/MemoryManager.h'". The plan's template for sharedCompression/src/CMakeLists.txt did not include `${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedMemoryManager/include/public`. The include path is required because the PCH chain `FirstSharedFoundation.h` -> `MemoryManager.h` is activated by `target_precompile_headers`.
- **Fix:** Added `${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedMemoryManager/include/public` to both sharedCompression and sharedRegex include_directories. Also proactively added it to all three Task 2 libs (sharedImage, sharedIoWin, sharedMessageDispatch) to prevent the same failure.
- **Files modified:** `sharedCompression/src/CMakeLists.txt`, `sharedRegex/src/CMakeLists.txt` (plus proactive fix in sharedImage, sharedIoWin, sharedMessageDispatch)
- **Committed in:** `1b8896a26` (sharedCompression, sharedRegex), `fa480859b` (remaining three)

---

**Total deviations:** 1 auto-fixed (Rule 1 bug — missing include path)
**Impact on plan:** Minimal. Single include path addition required by whitengold's sharedMemoryManager retention. Consistent with Plan 03 pattern established for Tier 3 libs.

## Known Stubs

None — no UI components or data paths in this plan. All CMakeLists files are build system configuration.

## Threat Flags

None — this plan creates only CMake build system files. No network endpoints, auth paths, file access patterns, or schema changes introduced.

## Self-Check: PASSED

All files verified:
- `src/engine/shared/library/sharedCompression/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedCompression/src/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedRegex/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedRegex/src/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedImage/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedImage/src/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedIoWin/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedIoWin/src/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedMessageDispatch/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedMessageDispatch/src/CMakeLists.txt` — FOUND
- `build/lib/Debug/sharedCompression.lib` — FOUND
- `build/lib/Debug/sharedRegex.lib` — FOUND
- `build/lib/Debug/sharedImage.lib` — FOUND
- `build/lib/Debug/sharedIoWin.lib` — FOUND
- `build/lib/Debug/sharedMessageDispatch.lib` — FOUND

All 2 task commits verified in git log:
- `1b8896a26` feat(02-04): sharedCompression + sharedRegex CMakeLists pairs — FOUND
- `fa480859b` feat(02-04): sharedImage + sharedIoWin + sharedMessageDispatch CMakeLists pairs — FOUND

---
*Phase: 02-shared-engine-libraries*
*Completed: 2026-05-04*
