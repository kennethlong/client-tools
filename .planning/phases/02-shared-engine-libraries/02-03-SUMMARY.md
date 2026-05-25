---
phase: 02-shared-engine-libraries
plan: 03
subsystem: build-system
tags: [cmake, tier3-libs, sharedThread, sharedMath, sharedFile, sharedFoundation, sharedXml, time-t-probe, stlport, vs2022]

# Dependency graph
requires:
  - phase: 02-shared-engine-libraries
    plan: 02
    provides: Tier 2 leaf libs, sharedMemoryManager, time_t probe template
provides:
  - sharedThread STATIC lib (build/lib/Debug/sharedThread.lib) — Tier 3 thread primitives
  - sharedMath STATIC lib (build/lib/Debug/sharedMath.lib) — Tier 3-4 math primitives (circular dep with sharedFile resolved at link time)
  - sharedFile STATIC lib (build/lib/Debug/sharedFile.lib) — Tier 3 TreeFile reader (load-bearing)
  - sharedFoundation STATIC lib (build/lib/Debug/sharedFoundation.lib) — Tier 3 foundation (Os, Clock, ConfigFile, NetworkId, ExitChain)
  - sharedXml STATIC lib (build/lib/Debug/sharedXml.lib) — Tier 4 libxml2-backed XML reader
  - D-08 time_t probe: second instance confirms _USE_32BIT_TIME_T=1 reaches Tier 3 (sharedFoundation)
affects:
  - 02-04 through 02-06 (all higher-tier libs depend on these Tier 3 libs)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "PATTERN A top-level entry for all 5 new libs (cmake_minimum + project + include_dirs + add_subdirectory)"
    - "PATTERN B compiled lib with SHARED_SOURCES + PLATFORM_SOURCES — sharedThread, sharedFile, sharedFoundation, sharedXml"
    - "PATTERN D time_t probe via configure_file + target_sources — sharedFoundation as Tier 3 representative"
    - "PATTERN F multi-subdir source enumeration — sharedMath (shared/ + core/ + debug/)"
    - "PATTERN G external dep linkage with P6 iconv guard — sharedXml links LIBXML2_LIBRARY, guards ICONV_INCLUDE_DIR with if(NOT WIN32)"
    - "sharedFile + sharedMath circular dep — forward-reference in target_link_libraries; CMake resolves at generate time, MSVC linker resolves at archive-link time"
    - "STLPort _stdio_file.h UCRT patch — guard _MSC_VER < 1900 block + modern-MSVC no-op stub for opaque FILE struct"

key-files:
  created:
    - src/engine/shared/library/sharedThread/CMakeLists.txt
    - src/engine/shared/library/sharedThread/src/CMakeLists.txt
    - src/engine/shared/library/sharedMath/CMakeLists.txt
    - src/engine/shared/library/sharedMath/src/CMakeLists.txt
    - src/engine/shared/library/sharedFile/CMakeLists.txt
    - src/engine/shared/library/sharedFile/src/CMakeLists.txt
    - src/engine/shared/library/sharedFoundation/CMakeLists.txt
    - src/engine/shared/library/sharedFoundation/src/CMakeLists.txt
    - src/engine/shared/library/sharedXml/CMakeLists.txt
    - src/engine/shared/library/sharedXml/src/CMakeLists.txt
  modified:
    - src/engine/shared/library/CMakeLists.txt (5 new add_subdirectory calls)
    - src/cmake/win32/FindSTLPort.cmake (_stdio_file.h UCRT patch added)
    - src/engine/shared/library/sharedFoundation/src/win32/PlatformGlue.cpp (snprintf guard)

key-decisions:
  - "sharedMath circular dep with sharedFile resolved at link time — both targets declared in same build tree; CMake forward-reference accepted; MSVC multi-pass archive linker resolves"
  - "PlatformGlue.cpp snprintf guarded with #if !(_MSC_VER >= 1900) — UCRT provides snprintf as __forceinline, C2084 (function already has body) avoided without source restructure"
  - "Clock.cpp timezone=_timezone via target_compile_definitions — no source edit; POSIX 'timezone' global mapped to UCRT '_timezone' symbol in sharedFoundation translation units"
  - "STLPort _stdio_file.h patched in FindSTLPort.cmake — sharedXml includes <iostream> via XmlTreeNode.h which triggers STLPort streambuf -> _stdio_file.h accessing opaque _iobuf fields; guarded for modern UCRT with no-op stub"
  - "W-only files included per D-02: PositionVertexIndexer.cpp/h, VectorRgba.cpp/h (sharedMath/shared/), core/Hsv.cpp/h (sharedMath/shared/core/), IndentedFileWriter.cpp/h (sharedFile/shared/), GameControllerMessage.def (sharedFoundation/shared/)"
  - "sharedMemoryManager/include/public added to sharedThread and sharedMath include paths — PCH chain FirstShared*.h -> FirstSharedFoundation.h -> MemoryManager.h requires this at compile time"

# Metrics
duration: 55min
completed: 2026-05-04
---

# Phase 2 Plan 03: Tier 3 Foundation Triangle + sharedXml Summary

**10 new CMakeLists files for 5 Tier 3 foundation libs — sharedThread, sharedMath, sharedFile, sharedFoundation (with D-08 Tier 3 time_t probe), and sharedXml (with P6 iconv guard) — all building as static .libs under VS 2022 with three targeted UCRT/STLPort compat fixes**

## Performance

- **Duration:** 55 min
- **Started:** 2026-05-04T14:00:00Z
- **Completed:** 2026-05-04T14:55:00Z
- **Tasks:** 3
- **Files modified:** 13 (10 new CMakeLists + 2 compat source edits + 1 FindSTLPort patch)

## Accomplishments

- sharedThread CMakeLists pair authored — mirrors swg-main verbatim, links sharedSynchronization + ${CMAKE_THREAD_LIBS_INIT}
- sharedMath CMakeLists pair authored — multi-subdir (shared/ + core/ + debug/) with all 3 W-only whitengold files included (PositionVertexIndexer, VectorRgba, core/Hsv per D-02)
- sharedFile CMakeLists pair authored — W-only IndentedFileWriter.cpp/h included per D-02; links sharedCompression/sharedDebug/sharedFoundation/sharedMath/fileInterface
- sharedFoundation CMakeLists pair authored — W-only GameControllerMessage.def included per D-02; D-08 Tier 3 time_t probe (`configure_file` + `target_sources`); time_t probe confirms `_USE_32BIT_TIME_T=1` propagates to Tier 3 TUs
- sharedXml CMakeLists pair authored — PATTERN G with Pitfall-6 iconv guard (`if(NOT WIN32)`); PCH path is `shared/core/FirstSharedXml.h` (subdir-nested)
- Circular dep sharedFile <-> sharedMath resolved cleanly: both `target_link_libraries` forward-references satisfied at CMake generate time; MSVC multi-pass archive linker resolves at lib-level link
- D-08 time_t probe count: 2 / 3 (Tier 2 = sharedDebug done in Plan 02; Tier 3 = sharedFoundation done this plan; Tier 5 = sharedNetwork in Plan 05)
- Phase 2 cumulative lib count: 11 (swgSharedUtility + 5 Tier 2 + 5 Tier 3 this plan)
- Full `cmake --build build --config Debug --parallel 8` succeeds — all 11 Phase 2 libs retained

## Task Commits

1. **Task 1: sharedThread + sharedMath CMakeLists pairs** — `f003bb67c` (feat)
2. **Task 2: sharedFile + sharedFoundation CMakeLists pairs** — `5f90ccda7` (feat)
3. **Task 3: sharedXml CMakeLists pair + STLPort _stdio_file.h patch** — `64be5f84e` (feat)

## Files Created/Modified

- `src/engine/shared/library/sharedThread/CMakeLists.txt` — PATTERN A top-level entry
- `src/engine/shared/library/sharedThread/src/CMakeLists.txt` — PATTERN B compiled lib, links sharedSynchronization
- `src/engine/shared/library/sharedMath/CMakeLists.txt` — PATTERN A top-level entry
- `src/engine/shared/library/sharedMath/src/CMakeLists.txt` — PATTERN F multi-subdir (shared/ + core/ + debug/), W-only files, links sharedFile + sharedRandom
- `src/engine/shared/library/sharedFile/CMakeLists.txt` — PATTERN A top-level entry
- `src/engine/shared/library/sharedFile/src/CMakeLists.txt` — PATTERN B compiled lib, W-only IndentedFileWriter, links 5 deps
- `src/engine/shared/library/sharedFoundation/CMakeLists.txt` — PATTERN A top-level entry
- `src/engine/shared/library/sharedFoundation/src/CMakeLists.txt` — PATTERN B compiled lib, W-only GameControllerMessage.def, D-08 time_t probe, timezone=_timezone compile def
- `src/engine/shared/library/sharedXml/CMakeLists.txt` — PATTERN A top-level entry
- `src/engine/shared/library/sharedXml/src/CMakeLists.txt` — PATTERN G external dep, P6 iconv guard, subdir PCH path
- `src/engine/shared/library/CMakeLists.txt` — 5 new add_subdirectory calls
- `src/cmake/win32/FindSTLPort.cmake` — _stdio_file.h UCRT patch (_MSC_VER >= 1900 stub)
- `src/engine/shared/library/sharedFoundation/src/win32/PlatformGlue.cpp` — snprintf definition guarded for modern UCRT

## Decisions Made

- **sharedFile/sharedMath circular dep via forward-references:** Matches swg-main pattern verbatim. CMake `target_link_libraries` accepts forward references resolved at generate time. MSVC's archive linker is multi-pass and handles circular static lib dependencies. No special CMake workaround needed.
- **PlatformGlue.cpp snprintf guard vs. CMake flag:** Used `#if !(_MSC_VER >= 1900)` guard directly in PlatformGlue.cpp rather than a per-file compile flag. This is the minimal invasive approach — one line, same fix as swg-main would need. UCRT VS2015+ provides `snprintf` as `__forceinline` in `<stdio.h>`; re-defining it as an out-of-line function triggers C2084.
- **Clock.cpp timezone via target_compile_definitions (no source edit):** `timezone` (POSIX global) is not available in UCRT by default — only `_timezone` (prefixed form). Added `target_compile_definitions(sharedFoundation PRIVATE timezone=_timezone)` in CMakeLists rather than editing Clock.cpp, preserving the no-source-edit principle for shared/ files.
- **STLPort _stdio_file.h stub approach:** sharedXml's `XmlTreeNode.h` includes `<iostream>` which through STLPort's include chain reaches `_stdio_file.h`. The file uses internal `_iobuf` struct fields that UCRT removed. Added a no-op stub for `_MSC_VER >= 1900` that satisfies the compiler without any functional file I/O. Phase 2 libs do not use file-backed stream I/O at runtime.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] PlatformGlue.cpp C2084 — snprintf already has a body (UCRT VS2015+)**
- **Found during:** Task 2 (sharedFoundation build)
- **Issue:** `sharedFoundation/src/win32/PlatformGlue.cpp` defines `snprintf()` as an out-of-line function. VS 2022 + Win SDK 26100 UCRT defines `snprintf` as `__forceinline` in `<stdio.h>`. Attempting to re-define it triggers C2084 "function already has a body". Cannot be suppressed via `/wd` flags.
- **Fix:** Added `#if !defined(_MSC_VER) || (_MSC_VER < 1900)` guard around the snprintf definition. Pre-VS2015 toolsets (the original target) still get the definition; VS2015+ UCRT toolsets skip it.
- **Files modified:** `src/engine/shared/library/sharedFoundation/src/win32/PlatformGlue.cpp`
- **Committed in:** `5f90ccda7`

**2. [Rule 1 - Bug] Clock.cpp C2065 — 'timezone' undeclared in UCRT**
- **Found during:** Task 2 (sharedFoundation build)
- **Issue:** `sharedFoundation/src/shared/Clock.cpp` line 135 uses POSIX `timezone` global. UCRT exposes this as `_timezone` (prefixed), not `timezone`. The POSIX name is not available without `_POSIX_SOURCE` or similar. Error C2065 "undeclared identifier".
- **Fix:** Added `target_compile_definitions(sharedFoundation PRIVATE timezone=_timezone)` in sharedFoundation's `src/CMakeLists.txt` under `if(WIN32)`. No C++ source edit to Clock.cpp — the macro definition is injected at compile time through CMake.
- **Files modified:** `src/engine/shared/library/sharedFoundation/src/CMakeLists.txt`
- **Committed in:** `5f90ccda7`

**3. [Rule 3 - Blocking] STLPort _stdio_file.h C2039 — _iobuf fields absent in UCRT**
- **Found during:** Task 3 (sharedXml build)
- **Issue:** `sharedXml/src/shared/tree/XmlTreeNode.h` includes `<iostream>` and `<sstream>`. STLPort 4.5.3 `<iostream>` pulls in `_streambuf.h` → `_stdio_file.h`, which for `_STLP_MSVC` branches accesses internal `_iobuf` fields (`_base`, `_ptr`, `_cnt`). Modern UCRT (10.0.26100) fully opacified the `FILE`/`_iobuf` struct — these fields no longer exist. Multiple C2039 "is not a member of _iobuf" errors.
- **Fix:** Updated `FindSTLPort.cmake` to patch the staged `_stdio_file.h`: (a) narrow the outer `#if` condition from `defined (_STLP_MSVC)` to `(defined (_STLP_MSVC) && (_MSC_VER < 1900))` so VS2015+ falls through; (b) add a modern-MSVC stub (`_MSC_VER >= 1900`) with no-op accessor functions that don't touch the opaque struct; (c) guard the inner typedef/inline block similarly. The no-op stub is sufficient because no Phase 2 lib uses file-backed stream I/O at runtime.
- **Files modified:** `src/cmake/win32/FindSTLPort.cmake`
- **Committed in:** `64be5f84e`

---

**Total deviations:** 3 auto-fixed (2 Rule 1 bugs, 1 Rule 3 blocking)
**Impact on plan:** All fixes necessary for compilation. Two involve minimal source/CMake edits consistent with the no-source-edit principle. The _stdio_file.h STLPort patch is a build-system-only change that will benefit all future libs that include `<iostream>` or `<fstream>`.

## Known Stubs

None — no UI components or data paths in this plan. All CMakeLists files are build system configuration.

## Threat Flags

None — this plan creates only CMake build system files and minimal source compatibility fixes. No network endpoints, auth paths, file access patterns, or schema changes introduced.

## Self-Check: PASSED

All files verified:
- `src/engine/shared/library/sharedThread/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedThread/src/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedMath/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedMath/src/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedFile/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedFile/src/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedFoundation/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedFoundation/src/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedXml/CMakeLists.txt` — FOUND
- `src/engine/shared/library/sharedXml/src/CMakeLists.txt` — FOUND
- `build/lib/Debug/sharedThread.lib` — FOUND
- `build/lib/Debug/sharedMath.lib` — FOUND
- `build/lib/Debug/sharedFile.lib` — FOUND
- `build/lib/Debug/sharedFoundation.lib` — FOUND
- `build/lib/Debug/sharedXml.lib` — FOUND

All 3 task commits verified in git log:
- `f003bb67c` feat(02-03): sharedThread + sharedMath CMakeLists pairs — FOUND
- `5f90ccda7` feat(02-03): sharedFile + sharedFoundation CMakeLists pairs — FOUND
- `64be5f84e` feat(02-03): sharedXml CMakeLists pair + STLPort _stdio_file.h patch — FOUND

---
*Phase: 02-shared-engine-libraries*
*Completed: 2026-05-04*
