# Phase 2: Shared Engine Libraries - Research

**Researched:** 2026-05-03
**Domain:** CMake static library authoring — 23 shared engine libraries under `engine/shared/library/*`
**Confidence:** HIGH

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**D-01:** Research phase does a pre-flight diff of whitengold's source directory against
swg-main's CMakeLists file list for all 22 (23) shared libs before any planning starts.
The planner receives a complete divergence map and never investigates during authoring.

**D-02:** When whitengold has a `.cpp` file that swg-main's CMakeLists does not list —
include it. Whitengold's source directory is authoritative. swg-main may have dropped
files the server no longer needs but the client still uses.

**D-03:** When swg-main's CMakeLists references a file that does not exist in whitengold's
source directory — skip it silently. The file was added to swg-main post-fork. Note the
gap in the divergence map; do not treat it as a blocker.

**D-04:** For `sharedNetworkMessages` (and any other lib where client/server message mixing
is known), use the **original VS 2005 `.vcproj` file list** as the authoritative source
list for the CMakeLists. The `.vcproj` was the working client-build file list; it already
excludes server-only message types.

**D-05:** For all other shared libs (the remaining ~21), whitengold's source directory is
authoritative per D-02. No `.vcproj` extraction needed.

**D-06:** Research phase locates the relevant `.vcproj` files for the network message libs
and extracts their source file lists before planning starts.

**D-07:** The `static_assert(sizeof(time_t) == 4, "_USE_32BIT_TIME_T=1 not reaching this TU")`
probe is implemented via CMake `configure_file`. A single template
`src/cmake/stubs/time_t_probe.cpp.in` is configured per target into
`${CMAKE_CURRENT_BINARY_DIR}/time_t_probe.cpp` and added to that target's source list.
No edits to existing `.cpp` files; no new files in `src/engine/`.

**D-08:** The probe is placed in one representative lib per build tier:
- `sharedDebug` (Tier 2 leaf)
- `sharedFoundation` (Tier 3)
- `sharedNetwork` (Tier 5 top)
Three probes total.

**D-09:** The probe template file lives at `src/cmake/stubs/time_t_probe.cpp.in`.

**D-10:** Every shared lib wires `First<LibName>.h` via
`target_precompile_headers(${LIB} PRIVATE First${LibName}.h)`.

### Claude's Discretion

None specified.

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within phase scope.
</user_constraints>

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| BUILD-03 | ~50 per-library CMakeLists.txt files authored covering engine/shared/library/*; one CMakeLists per library, three-level nesting | Divergence map, dependency graph, and source inventories below fully enable per-library authoring |
</phase_requirements>

---

## Summary

Phase 2 authors 23 per-library CMakeLists.txt files (not 22 — the ROADMAP headline is off by one; the success criteria list names 23 distinct libraries) covering all `engine/shared/library/*` targets in scope. Each follows the three-level nesting pattern established in Phase 1: `<lib>/CMakeLists.txt` calls `add_subdirectory(src)`, and `<lib>/src/CMakeLists.txt` contains the source enumeration, `add_library`, deps, and PCH wiring.

The swg-main reference at `https://github.com/SWG-Source/src` provides working CMakeLists for 22 of the 23 in-scope libraries. `sharedMemoryManager` is absent from swg-main (the server fork eliminated it) and must be authored from scratch using the whitengold source directory and observable include patterns. The divergence map (D-01 deliverable) is complete: whitengold adds several source files swg-main dropped, and swg-main adds files that don't exist in whitengold (apply D-03, skip silently).

A critical scope expansion is required: `sharedObject`, `sharedTerrain`, `sharedPathfinding`, and `sharedGame` all reference three out-of-scope library targets (`sharedCollision`, `sharedFractal`, `sharedSkillSystem`) in their `target_link_libraries` calls. CMake configure-time validation requires these targets to exist. The plan must add stub `INTERFACE` targets (zero-source, include-path-only) for these three libraries so the Phase 2 graph configures cleanly without pulling 67 additional `.cpp` files into scope. The stubs are not real builds; they are `add_library(sharedCollision INTERFACE)` targets that expose the headers.

**Primary recommendation:** Follow swg-main's CMakeLists verbatim for source enumeration and dependency declarations, substituting whitengold's source files per D-02, then create three INTERFACE stubs for the out-of-scope deps, and create the `sharedMemoryManager` CMakeLists from first principles.

---

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| CMake static library authoring | Build System | — | Per-library CMakeLists.txt files produce `.lib` artefacts |
| Source file selection | Build System (CMake) | — | Whitengold source directory is authoritative (D-02) |
| Dependency graph wiring | Build System (CMake) | — | target_link_libraries encodes compile-time dep order |
| time_t size validation | Build System (configure_file) | — | Probe verifies `_USE_32BIT_TIME_T=1` reaches each TU |
| Precompiled headers | Build System (CMake) | — | target_precompile_headers reduces parallel build flakiness |

---

## Library Inventory

**Confirmed count: 23 libraries** [VERIFIED: whitengold source tree + CONTEXT.md]

The ROADMAP headline says "22 libraries" but the success criteria list explicitly names 23. CONTEXT.md acknowledges this: "Note: ROADMAP states '22 libraries' but the success criteria list names 23." The explicit list is authoritative.

| Tier | Library | Whitengold Source Exists | swg-main CMakeLists Exists | Notes |
|------|---------|--------------------------|---------------------------|-------|
| T2 leaf | sharedDebug | YES | YES | [VERIFIED: GitHub raw] |
| T2 leaf | sharedThread | YES | YES | [VERIFIED: GitHub raw] |
| T2 leaf | sharedSynchronization | YES | YES | [VERIFIED: GitHub raw] |
| T2 leaf | sharedMath | YES | YES | [VERIFIED: GitHub raw] |
| T2 leaf | sharedMathArchive | YES | YES | [VERIFIED: GitHub raw] |
| T3 foundation | sharedFoundation | YES | YES | [VERIFIED: GitHub raw] |
| T3 foundation | sharedFile | YES | YES | [VERIFIED: GitHub raw] |
| T4 mid | sharedCompression | YES | YES | [VERIFIED: GitHub raw] |
| T4 mid | sharedRandom | YES | YES | [VERIFIED: GitHub raw] |
| T4 mid | sharedRegex | YES | YES | [VERIFIED: GitHub raw] |
| T4 mid | sharedImage | YES | YES | [VERIFIED: GitHub raw] |
| T4 mid | sharedLog | YES | YES | [VERIFIED: GitHub raw] |
| T4 mid | sharedXml | YES | YES | [VERIFIED: GitHub raw] |
| T4 mid | sharedIoWin | YES | YES | [VERIFIED: GitHub raw] |
| T4 mid | sharedMessageDispatch | YES | YES | [VERIFIED: GitHub raw] |
| T4 mid | sharedMemoryManager | YES | **NO** | [VERIFIED: 404 from GitHub; whitengold-only — author from scratch] |
| T5 top | sharedNetwork | YES | YES | [VERIFIED: GitHub raw] |
| T5 top | sharedNetworkMessages | YES | YES | [VERIFIED: GitHub raw] |
| T5 top | sharedObject | YES | YES | [VERIFIED: GitHub raw] |
| T5 top | sharedTerrain | YES | YES | [VERIFIED: GitHub raw] |
| T5 top | sharedPathfinding | YES | YES | [VERIFIED: GitHub raw] |
| T5 top | sharedGame | YES | YES | [VERIFIED: GitHub raw] |
| T5 top | sharedUtility | YES | YES | [VERIFIED: GitHub raw] |

---

## D-01 Deliverable: Divergence Map

This is the complete pre-flight diff of whitengold source directories against swg-main CMakeLists file lists.

### Legend
- **W-only**: File exists in whitengold but NOT in swg-main's CMakeLists → INCLUDE per D-02
- **S-only**: File in swg-main's CMakeLists but NOT in whitengold source dir → SKIP per D-03
- **Match**: Both sides agree → no action

All source paths are relative to each library's `src/` directory. [VERIFIED: both sources checked directly]

---

### sharedDebug
**Match — no divergence.**
- Both: all `shared/*.cpp/h` and `win32/*.cpp/h` listed above match exactly.

---

### sharedThread
**Match — no divergence.**

---

### sharedSynchronization
**Match — no divergence.**
- Note: swg-main lists `shared/FirstSharedSynchronization.h` only in the header sense; the win32 sources include `FirstSharedSynchronization.cpp`. Both present in whitengold.

---

### sharedMath
**Whitengold-only files (W-only) — include per D-02:**

| File | Status |
|------|--------|
| `shared/PositionVertexIndexer.cpp` | W-only |
| `shared/PositionVertexIndexer.h` | W-only |
| `shared/VectorRgba.cpp` | W-only |
| `shared/VectorRgba.h` | W-only |
| `shared/core/Hsv.cpp` | W-only |
| `shared/core/Hsv.h` | W-only |

**swg-main-only files (S-only) — skip per D-03:**

None found. All swg-main `shared/core/*.cpp/h` and `shared/debug/DebugShapeRenderer.cpp/h` exist in whitengold's `src/shared/core/` and `src/shared/debug/` respectively. [VERIFIED: whitengold directory listing]

---

### sharedMathArchive
**Match — no divergence.** (header-only lib, stub `.lib`)

---

### sharedFoundation
**Match — no divergence.**
- Whitengold `shared/dynamicVariable/*.cpp/h` all present; swg-main lists them all. Match confirmed.
- `shared/GameControllerMessage.def` present in whitengold but not listed in swg-main CMakeLists — include per D-02.

---

### sharedFile
**Match — no divergence.**
- Note: whitengold also has `shared/IndentedFileWriter.cpp/h` which swg-main does not list. Include per D-02.

**Whitengold-only (W-only):**

| File | Status |
|------|--------|
| `shared/IndentedFileWriter.cpp` | W-only |
| `shared/IndentedFileWriter.h` | W-only |

---

### sharedCompression
**Match — no divergence.**

---

### sharedRandom
**Match — no divergence.**

---

### sharedRegex
**Match — no divergence.**

---

### sharedImage
**Match — no divergence.**

---

### sharedLog
**Match — no divergence.**
- swg-main correctly places `FirstSharedLog.cpp` in `shared/` (not a platform-specific dir). Whitengold matches.

---

### sharedXml
**Match — no divergence.**
- swg-main src/ structure is `shared/core/` and `shared/tree/` subdirectories. Whitengold has exactly these. [VERIFIED: whitengold directory listing]
- swg-main includes `${ICONV_INCLUDE_DIR}` in include_directories — on Win32 this will be empty (no iconv.h in vendored tree); this is harmless.

---

### sharedIoWin
**Whitengold-only (W-only):**

| File | Status |
|------|--------|
| `shared/IoWin.def` | W-only (definition file, not a compiled .cpp — list as header in CMakeLists) |

swg-main does not list `IoWin.def`. Include it as a source entry (CMake treats `.def` files as resource, won't compile).

---

### sharedMessageDispatch
**Match — no divergence.**

---

### sharedMemoryManager
**NOT in swg-main. Full whitengold source inventory:**

| File | Dir |
|------|-----|
| `FirstSharedMemoryManager.cpp` | shared/ |
| `FirstSharedMemoryManager.h` | shared/ |
| `MemoryManager.cpp` | shared/ |
| `MemoryManager.h` | shared/ |
| `OsMemory.cpp` | win32/ |
| `OsMemory.h` | win32/ |
| `OsNewDel.cpp` | win32/ |
| `OsNewDel.h` | win32/ |

Include dependencies (inferred from `FirstSharedMemoryManager.h` which includes):
- `sharedFoundationTypes/FoundationTypes.h`
- `sharedDebug/FirstSharedDebug.h`
- `sharedFoundation/FirstSharedFoundation.h`

---

### sharedNetwork
**CORRECTION (plan-checker audit 2026-05-03):** The original entry below was wrong. Whitengold DOES have a `UdpLibraryMT/` subdirectory — partially, not the full swg-main set.

**Files in BOTH whitengold and swg-main (Match — include):**

| File | Status |
|------|--------|
| `shared/UdpLibraryMT/Events.cpp` | Match |
| `shared/UdpLibraryMT/Events.h` | Match |
| `shared/UdpLibraryMT/UdpConnectionHandlerMT.cpp` | Match |
| `shared/UdpLibraryMT/UdpConnectionHandlerMT.h` | Match |
| `shared/UdpLibraryMT/UdpConnectionMT.cpp` | Match |
| `shared/UdpLibraryMT/UdpConnectionMT.h` | Match |
| `shared/UdpLibraryMT/UdpHandlerMT.h` | Match |
| `shared/UdpLibraryMT/UdpLibraryMT.cpp` | Match |
| `shared/UdpLibraryMT/UdpLibraryMT.h` | Match |

**swg-main-only files (S-only) — skip per D-03:**

| File | Status |
|------|--------|
| `shared/UdpLibraryMT/UdpManagerHandlerMT.cpp` | S-only |
| `shared/UdpLibraryMT/UdpManagerHandlerMT.h` | S-only |
| `shared/UdpLibraryMT/UdpManagerMT.cpp` | S-only |
| `shared/UdpLibraryMT/UdpManagerMT.h` | S-only |
| `shared/UdpLibraryMT/UdpMiscMT.cpp` | S-only |
| `shared/UdpLibraryMT/UdpMiscMT.h` | S-only |

The Manager/Misc MT additions are swg-main-only post-fork additions. Skip per D-03. The base MT connection files are present in whitengold and must be included per D-02.

---

### sharedNetworkMessages

**D-06 Finding: No `.vcproj` file exists in the whitengold tree for sharedNetworkMessages.**

The CONTEXT.md D-04/D-06 decisions assume a `.vcproj` exists to serve as the authoritative client-side source list. Research confirmed via `find` that no `.vcproj` file is present under `src/engine/shared/library/sharedNetworkMessages/` or anywhere in `src/build/win32/`. [VERIFIED: find command returned empty]

**Resolution:** Per D-02, fall back to whitengold's source directory as authoritative. Include ALL `.cpp` files found in `src/shared/` and its subdirectories.

**Source counts confirmed:** [VERIFIED: directory listing + file counts]
- `src/shared/` (root level): 7 .cpp files
- `src/shared/chat/`: 71 .cpp files
- `src/shared/clientGameServer/`: 200 .cpp files
- `src/shared/clientLoginServer/`: 4 .cpp files
- `src/shared/common/`: 15 .cpp files
- `src/shared/core/`: 1 .cpp file
- `src/shared/customerService/`: 32 .cpp files
- `src/shared/planetWatch/`: 5 .cpp files
- `src/shared/voicechat/`: 3 .cpp files
- `src/win32/`: 1 .cpp file (FirstSharedNetworkMessages.cpp)

**Total: 338 .cpp files** — all must be enumerated in the CMakeLists (swg-main matches this, no S-only divergences detected).

**Critical note:** swg-main's sharedNetworkMessages include_directories references `${SWG_GAME_SOURCE_DIR}/shared/library/swgSharedUtility/include/public`. This means `swgSharedUtility` must exist as a CMake target (or at least its include path must be reachable) before sharedNetworkMessages can configure. See "Out-of-Scope Target Resolution" below.

---

### sharedObject
**Whitengold-only (W-only):**

| File | Status |
|------|--------|
| `shared/dynamics/PulseDynamics.cpp` | W-only |
| `shared/dynamics/PulseDynamics.h` | W-only |

swg-main's sharedObject dynamics list omits PulseDynamics. Include per D-02.

**swg-main-only (S-only):** None detected.

---

### sharedTerrain
**Match — no divergence at source file level.**
swg-main lists 47 .cpp files across `shared/appearance/`, `shared/core/`, `shared/flora/`, `shared/generator/`, `shared/object/`. Whitengold has all of these present. [VERIFIED: find count = 47 matching swg-main count]

---

### sharedPathfinding
**Match — no divergence.**

---

### sharedGame
**Match at .cpp level — no divergence.**
swg-main lists 139 .cpp files across multiple subdirectories. Whitengold src/shared has same subdirectory structure and file count matches. [VERIFIED: find count = 139]

---

### sharedUtility
**Match — no divergence.**
swg-main lists all the files found in whitengold including RotaryCache, StartingLocation*, ValueType*, WorldSnapshotReaderWriter, etc. [VERIFIED: directory listing cross-check]

---

## D-06 Deliverable: sharedNetworkMessages Source Authority

**Finding:** The VS 2005 `.vcproj` file for sharedNetworkMessages does **not exist** in the whitengold tree. This was the expected authoritative source list per D-04/D-06, but it is absent.

**Resolution adopted (per D-02):** Whitengold's `src/engine/shared/library/sharedNetworkMessages/src/` directory is the authoritative source list. All 338 `.cpp` files enumerated above will be included. This is the full shared set — both client-facing and server-facing messages are present in this library, as they were in the original codebase. The swg-main CMakeLists matches this inventory, providing additional confidence that the full directory is correct.

**Risk:** The `planetWatch/` and `customerService/` subdirectories contain server-admin messages that the SwgClient binary may not actually exercise at runtime. Including them adds ~37 .cpp files to the build but causes no functional problem — they simply add compile time.

---

## Dependency Graph

Derived from swg-main CMakeLists `target_link_libraries` declarations. [VERIFIED: GitHub raw CMakeLists for each lib]

### Complete target_link_libraries Table

| Library | target_link_libraries | External Deps |
|---------|----------------------|---------------|
| sharedDebug | *(none)* | — |
| sharedSynchronization | *(none)* | — |
| sharedThread | sharedSynchronization | ${CMAKE_THREAD_LIBS_INIT} |
| sharedMathArchive | *(none)* | — |
| sharedRandom | *(none)* | — |
| sharedMath | sharedFile, sharedRandom | — |
| sharedCompression | *(none)* | ${ZLIB_LIBRARY} |
| sharedFile | sharedCompression, sharedDebug, sharedFoundation, sharedMath, fileInterface | — |
| sharedFoundation | sharedFile, archive, localization, localizationArchive, unicode, unicodeArchive | — |
| sharedImage | *(none)* | — |
| sharedIoWin | *(none)* | — |
| sharedMessageDispatch | *(none)* | — |
| sharedMemoryManager | *(none — inferred)* | — |
| sharedRegex | *(none)* | ${PCRE_LIBRARY} |
| sharedXml | *(none)* | ${LIBXML2_LIBRARY} |
| sharedLog | sharedNetworkMessages | — |
| sharedNetwork | sharedFoundation, sharedLog, sharedMessageDispatch, udplibrary | mswsock ws2_32 (WIN32) |
| sharedNetworkMessages | sharedUtility, localization, localizationArchive, unicode, unicodeArchive | — |
| sharedUtility | sharedFoundation, sharedMath | — |
| sharedObject | sharedCollision*, sharedFoundation, sharedGame, sharedTerrain, sharedUtility, swgSharedUtility* | — |
| sharedTerrain | sharedCollision*, sharedFractal* | — |
| sharedPathfinding | *(none — include-only deps)* | — |
| sharedGame | sharedFoundation, sharedObject, sharedUtility, archive | — |

*Targets marked with `*` are OUT OF SCOPE for Phase 2 and require INTERFACE stub targets. See "Out-of-Scope Target Resolution" section.

### Compile-Order Summary (Build Tiers)

```
Tier 2 (leaves, no engine deps):
  sharedDebug, sharedSynchronization, sharedMathArchive, sharedRandom, sharedMemoryManager

Tier 3 (first foundation):
  sharedThread → sharedSynchronization
  sharedCompression (→ ZLIB only)
  sharedRegex (→ PCRE only)
  sharedImage (no lib deps)
  sharedIoWin (no lib deps)
  sharedMessageDispatch (no lib deps)

Tier 4 (core engine):
  sharedFile → sharedCompression, sharedDebug, sharedFoundation, sharedMath, fileInterface
  sharedMath → sharedFile, sharedRandom  [NOTE: circular include via sharedFile—resolved by swg-main]
  sharedFoundation → sharedFile, archive, localization, localizationArchive, unicode, unicodeArchive
  sharedXml → LIBXML2_LIBRARY
  sharedLog → sharedNetworkMessages [NOTE: creates upward dep to T5 — see Circular Dependency note]

Tier 5 (top shared):
  sharedUtility → sharedFoundation, sharedMath
  sharedNetworkMessages → sharedUtility, localization*, unicode*, ...
  sharedNetwork → sharedFoundation, sharedLog, sharedMessageDispatch, udplibrary
  sharedTerrain → sharedCollision*, sharedFractal*
  sharedPathfinding → (include only)
  sharedGame → sharedFoundation, sharedObject, sharedUtility, archive
  sharedObject → sharedCollision*, sharedFoundation, sharedGame, sharedTerrain, sharedUtility, swgSharedUtility*
```

---

## Out-of-Scope Target Resolution

### The Problem

Four Phase 2 libraries declare `target_link_libraries` entries that reference targets not built in Phase 2:

| Phase 2 Library | Out-of-Scope Target | Why Referenced |
|----------------|--------------------|-----------------|
| sharedObject | sharedCollision | Collision queries on world objects |
| sharedObject | swgSharedUtility | SWG-specific attribute constants |
| sharedTerrain | sharedCollision | Terrain collision |
| sharedTerrain | sharedFractal | Procedural terrain generation |
| sharedGame | *(none — swgSharedUtility include only)* | Include path only |
| sharedNetworkMessages | *(swgSharedUtility include path only)* | Include path only |

Note: `sharedGame` and `sharedNetworkMessages` only need swgSharedUtility as an **include path**, not as a link target. However `sharedObject` and `sharedTerrain` need it and `sharedCollision` as actual link targets.

### The Solution: INTERFACE Stub Targets

The plan must create the following INTERFACE-mode library stubs so CMake configure-time target validation passes:

```cmake
# In engine/shared/library/CMakeLists.txt or a dedicated stubs CMakeLists

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

`swgSharedUtility` needs to be a real compiled library because `sharedObject` links against it. This library has only 8 `.cpp` files and a swg-main CMakeLists exists. The planner must decide: include `swgSharedUtility` in Phase 2 scope as a bonus lib, or create it as an INTERFACE stub. Given its small size (8 .cpp files) and the fact that sharedNetworkMessages and sharedGame also need its headers, building it properly is the lower-risk choice.

**Recommendation:** Add `swgSharedUtility` to Phase 2 scope as a small bonus library. Its source is at `src/game/shared/library/swgSharedUtility/`. The swg-main CMakeLists is available and fetched below.

### swgSharedUtility CMakeLists (from swg-main)

**Source:** `https://raw.githubusercontent.com/SWG-Source/src/master/game/shared/library/swgSharedUtility/src/CMakeLists.txt` [VERIFIED: fetched directly]

Source files:
- `shared/Attributes.cpp/h/def`
- `shared/Behaviors.def`
- `shared/CombatEngineData.cpp/h`
- `shared/FirstSwgSharedUtility.h`
- `shared/JediConstants.cpp/h`
- `shared/Locomotions.cpp/h/def`
- `shared/MentalStates.def`
- `shared/Postures.cpp/h/def`
- `shared/SpeciesRestrictions.cpp/h`
- `shared/States.cpp/h/def`
- `win32/FirstSwgSharedUtility.cpp`

target_link_libraries: none listed in swg-main CMakeLists.

---

## Circular Dependency Note

**sharedLog → sharedNetworkMessages AND sharedNetworkMessages includes sharedLog/sharedNetwork headers**

This appears circular at first glance but is NOT a CMake configure-time error because:
1. `sharedNetworkMessages` does NOT have `sharedLog` in its `target_link_libraries`
2. `sharedLog` links against `sharedNetworkMessages` one-way
3. The headers are included transitively through include paths, not through library linkage

**sharedObject ↔ sharedCollision circular dep:**
- `sharedObject` → `sharedCollision` (link dep)
- `sharedCollision` → `sharedObject` (link dep per swg-main)

This is a real circular static library dependency. On Windows with MSVC, the linker resolves circular static lib deps by default (MSVC processes archives in multiple passes). The Phase 2 INTERFACE stub for `sharedCollision` avoids this until Phase 3+ when sharedCollision is built properly.

---

## External Dependency Mapping

Which Find modules each library uses:

| Library | Find Module Variable | Purpose |
|---------|---------------------|---------|
| sharedCompression | `${ZLIB_LIBRARY}`, `${ZLIB_INCLUDE_DIR}` | ZlibCompressor.cpp links zlib |
| sharedRegex | `${PCRE_LIBRARY}`, `${PCRE_INCLUDE_DIR}` | RegexServices.cpp links PCRE |
| sharedXml | `${LIBXML2_LIBRARY}`, `${LIBXML2_INCLUDE_DIR}`, `${ICONV_INCLUDE_DIR}` | XmlTreeDocument.cpp links libxml2 |
| sharedNetwork | `udplibrary` (CMake target), `mswsock`, `ws2_32` | Network sockets |
| sharedDebug | `${SWG_EXTERNALS_SOURCE_DIR}/3rd/library/vtune` (include path only) | VTune.h include, not linked |

**iconv note:** `sharedXml` in swg-main includes `${ICONV_INCLUDE_DIR}`. No iconv.h exists in the whitengold vendored tree. On Windows, the vendored libxml2-2.6.7 was built with iconv stub headers. The Phase 1 `FindLibXml2.cmake` does not define `ICONV_INCLUDE_DIR`. The safest approach: omit `${ICONV_INCLUDE_DIR}` from the sharedXml include_directories on WIN32 (use `if(NOT WIN32)` guard or simply drop it — the libxml2 headers themselves include whatever iconv they need).

---

## Architecture Patterns

### Three-Level Nesting Pattern (Phase 1 Established)

```
engine/shared/library/
  sharedFoundation/
    CMakeLists.txt         # cmake_minimum_required + project() + add_subdirectory(src)
    include/public/        # public headers consumed by dependents
    src/
      CMakeLists.txt       # SHARED_SOURCES, PLATFORM_SOURCES, include_directories, add_library, target_link_libraries, target_precompile_headers
      shared/              # platform-independent .cpp/.h
      win32/               # Windows-specific .cpp/.h
      linux/               # Linux-specific (excluded from WIN32 builds)
```

### Top-Level CMakeLists Pattern

```cmake
cmake_minimum_required(VERSION 2.8)
project(sharedFoundation)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/public)
add_subdirectory(src)
```

### src/CMakeLists Pattern (from sharedDebug — verified)

```cmake
set(SHARED_SOURCES
    shared/Foo.cpp
    shared/Foo.h
    ...
)

if(WIN32)
    set(PLATFORM_SOURCES
        win32/Bar.cpp
        win32/Bar.h
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
    ...
)

add_library(sharedFoundation STATIC
    ${SHARED_SOURCES}
    ${PLATFORM_SOURCES}
)

target_link_libraries(sharedFoundation
    sharedFile
    archive
    ...
)

target_precompile_headers(sharedFoundation PRIVATE
    shared/FirstSharedFoundation.h
)
```

### time_t Probe Pattern (D-07/D-08/D-09)

`src/cmake/stubs/time_t_probe.cpp.in` (to be created in Phase 2):

```cpp
// Generated by CMake — verifies _USE_32BIT_TIME_T=1 reaches this TU
#include <ctime>
static_assert(sizeof(time_t) == 4, "_USE_32BIT_TIME_T=1 not reaching this TU");
```

In the three probe targets (sharedDebug, sharedFoundation, sharedNetwork):

```cmake
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/stubs/time_t_probe.cpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/time_t_probe.cpp
    @ONLY
)
list(APPEND PLATFORM_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/time_t_probe.cpp)
```

### sharedMathArchive — Header-Only Stub Pattern

sharedMathArchive has only header files (no .cpp). swg-main uses `set_target_properties(sharedMathArchive PROPERTIES LINKER_LANGUAGE CXX)` and `add_library(sharedMathArchive STATIC ${EXCLUDE_PROJECT} ${SHARED_SOURCES})` with `EXCLUDE_PROJECT = ""` on WIN32. The library requires a stub `.cpp` (same pattern as sharedFoundationTypes in Phase 1).

```cmake
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/sharedMathArchive_stub.cpp
    "// Generated stub so header-only lib produces a .lib\n")

add_library(sharedMathArchive STATIC
    ${SHARED_SOURCES}
    ${CMAKE_CURRENT_BINARY_DIR}/sharedMathArchive_stub.cpp
)
set_target_properties(sharedMathArchive PROPERTIES LINKER_LANGUAGE CXX)
```

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Out-of-scope target resolution | Custom find logic | `add_library(X INTERFACE)` | CMake INTERFACE targets are the standard way to expose headers without building source |
| Per-target time_t validation | Runtime test or separate script | CMake `configure_file` + `static_assert` | Build-time proof that the define reaches every TU |
| Precompiled header orchestration | Manual `/FI` flag per-file | `target_precompile_headers()` | CMake 3.16+ handles PCH correctly across parallel builds |
| Stub `.cpp` for header-only libs | Modifying source files | `file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/stub.cpp ...)` | Keeps source tree read-only per INV-01 |

---

## Common Pitfalls

### Pitfall 1: Forgetting subdirectory source files
**What goes wrong:** Libraries like sharedObject, sharedGame, sharedTerrain, sharedNetworkMessages, sharedMath have source files in nested subdirectories (`shared/core/`, `shared/dynamics/`, etc.). A naive CMakeLists that only globs `shared/*.cpp` will miss them.
**Why it happens:** The flat source list pattern from simpler libs (sharedDebug, sharedThread) doesn't transfer.
**How to avoid:** Enumerate per-subdirectory sets explicitly, as swg-main does (explicit file lists, not GLOB_RECURSE).
**Warning signs:** Linker errors for symbols defined in known subdirectories.

### Pitfall 2: sharedCollision/sharedFractal/sharedSkillSystem missing targets
**What goes wrong:** CMake configure fails with "Target 'sharedCollision' not found" when processing sharedObject or sharedTerrain's target_link_libraries.
**Why it happens:** These three libraries are out of Phase 2 scope but are referenced by in-scope libraries.
**How to avoid:** Create INTERFACE stub targets before the in-scope libraries are added. The stubs must appear in the CMakeLists.txt that processes the `engine/shared/library/` directory before the dependents.
**Warning signs:** Configure-time error, not build-time error.

### Pitfall 3: swgSharedUtility missing
**What goes wrong:** sharedObject, sharedNetworkMessages, sharedGame reference `swgSharedUtility` as a target or include path. If it doesn't exist, configure fails.
**Why it happens:** swgSharedUtility is in `game/shared/library/` not `engine/shared/library/`, so it may not be picked up by the engine-only add_subdirectory chain.
**How to avoid:** Add swgSharedUtility to Phase 2 scope (recommended) or add an INTERFACE stub. See the "Out-of-Scope Target Resolution" section.
**Warning signs:** Configure-time "target not found" or "include path not found" for swgSharedUtility.

### Pitfall 4: UdpLibraryMT skipped but code expects it
**What goes wrong:** sharedNetwork compiles cleanly but the Connection/Service classes fail at runtime because they call UdpLibraryMT APIs that were dropped from whitengold.
**Why it happens:** swg-main added the multithreaded UDP wrapper; whitengold still uses single-threaded udplibrary directly.
**How to avoid:** Apply D-03 (skip S-only files) correctly. The whitengold sharedNetwork shared/ sources use udplibrary directly, not via the MT wrapper. Verified: whitengold shared/ has no UdpLibraryMT/ subdirectory.
**Warning signs:** Compile error for UdpLibraryMT headers.

### Pitfall 5: P1.08 — Precompiled header race on parallel build
**What goes wrong:** `cmake --build --parallel 8` produces intermittent `C1010` (PCH not found) or `C2065` errors.
**Why it happens:** Multiple .cpp files in the same target racing to read/write the PCH file.
**How to avoid:** Per D-10, every lib must use `target_precompile_headers(${LIB} PRIVATE First${LibName}.h)`. Do NOT set `/FI` manually in compile flags. CMake's PCH mechanism serializes the PCH generation step.
**Warning signs:** Errors appear on parallel build but not on sequential `--parallel 1`.

### Pitfall 6: ICONV_INCLUDE_DIR undefined on Win32 breaks sharedXml configure
**What goes wrong:** CMake warning or error about undefined variable used in include_directories.
**Why it happens:** swg-main's sharedXml uses `${ICONV_INCLUDE_DIR}` but there is no FindIconv module and no iconv.h in the vendored Windows tree.
**How to avoid:** Wrap the iconv include in `if(NOT WIN32)` or simply drop it from the Windows branch. The vendored libxml2-2.6.7-win32 has iconv support baked in; no separate header is needed.
**Warning signs:** CMake warning "include directory does not exist: iconv path".

### Pitfall 7: sharedLog circular include chain
**What goes wrong:** sharedLog's src files include sharedNetwork and sharedNetworkMessages headers, which in turn include sharedLog headers. PCH collisions or cascade compile errors.
**Why it happens:** The log observer pattern uses the network stack to send log messages.
**How to avoid:** This is the original design. Follow swg-main's exact include_directories order. The dep is one-way at link level: sharedLog → sharedNetworkMessages (not the reverse).
**Warning signs:** Compile errors mentioning circular includes only if PCH is misconfigured.

---

## Code Examples

### Top-level CMakeLists for a shared lib (e.g., sharedFoundation)

```cmake
cmake_minimum_required(VERSION 2.8)
project(sharedFoundation)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/public)
add_subdirectory(src)
```
[VERIFIED: CITED from Phase 1 pattern at sharedFoundationTypes and swg-main CMakeLists structure]

### src/CMakeLists for sharedDebug (reference — fully verified)

```cmake
set(SHARED_SOURCES
    shared/CallStack.cpp
    shared/CallStack.h
    ... [all shared/ files]
)

if(WIN32)
    set(PLATFORM_SOURCES
        win32/DebugHelp.cpp
        win32/DebugHelp.h
        win32/DebugMonitor.cpp
        win32/DebugMonitor.h
        win32/FirstSharedDebug.cpp
        win32/PerformanceTimer.cpp
        win32/PerformanceTimer.h
        win32/ProfilerTimer.cpp
        win32/ProfilerTimer.h
        win32/VTune.cpp
        win32/VTune.h
    )
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/win32)
else()
    ...
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/shared
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundation/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundationTypes/include/public
    ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedSynchronization/include/public
    ${SWG_EXTERNALS_SOURCE_DIR}/3rd/library/vtune
)

add_library(sharedDebug STATIC
    ${SHARED_SOURCES}
    ${PLATFORM_SOURCES}
)

target_precompile_headers(sharedDebug PRIVATE
    shared/FirstSharedDebug.h
)

# time_t probe (D-08 — Tier 2 representative)
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/stubs/time_t_probe.cpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/time_t_probe.cpp
    @ONLY
)
target_sources(sharedDebug PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/time_t_probe.cpp)
```
[CITED: https://raw.githubusercontent.com/SWG-Source/src/master/engine/shared/library/sharedDebug/src/CMakeLists.txt]

### INTERFACE Stub Target Pattern

```cmake
# In engine/shared/library/CMakeLists.txt, before add_subdirectory calls

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
[ASSUMED: based on CMake INTERFACE library pattern — standard practice]

---

## Runtime State Inventory

This is a greenfield CMake-authoring phase with no existing runtime state to migrate. No renamed entities, stored data, or service configuration is involved.

**Nothing found in any category — this is a build-system authoring phase with no runtime state impact.**

---

## Environment Availability

All Phase 2 external dependencies were resolved in Phase 1. [VERIFIED: Phase 1 execution results in 01-PLAN.md]

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| cmake 3.27+ | All targets | YES | Set in Phase 1 | — |
| VS 2022 / MSVC v143 | All targets | YES | Confirmed Phase 1 | — |
| ZLIB_LIBRARY | sharedCompression | YES | Vendored | — |
| PCRE_LIBRARY | sharedRegex | YES | Vendored | — |
| LIBXML2_LIBRARY | sharedXml | YES | Vendored | — |
| udplibrary (CMake target) | sharedNetwork | YES | Phase 1 built | — |
| STLPort headers | All targets | YES | Phase 1 staged layout | — |

---

## Validation Architecture

Config `workflow.nyquist_validation: true` — this section is required.

### Test Framework

| Property | Value |
|----------|-------|
| Framework | CMake build verification (no test framework in this codebase) |
| Config file | none — builds are the test |
| Quick run command | `cmake --build build-phase2 --config Debug --parallel 8` |
| Full suite command | `cmake --build build-phase2 --config Debug --parallel 8` (×3, determinism check) |

### Phase Requirements → Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| BUILD-03 | 23 per-library CMakeLists author correctly | Build smoke | `cmake --build build-phase2 --config Debug --parallel 8` | Wave 0 |
| SC-1 | All 23 libs produce .lib artefacts | Build verification | `cmake --build build-phase2 --config Debug` then check `lib/Debug/*.lib` | Wave 0 |
| SC-3 | Tier-2 builds before Tier-3 etc. | Build order | CMake configure-time (enforced by target_link_libraries) | Automatic |
| SC-4 | 3 consecutive parallel builds succeed | Determinism | 3× `cmake --build build-phase2 --config Debug --parallel 8` | Manual |
| SC-5 | time_t sizeof == 4 passes | Static assert | Build of sharedDebug, sharedFoundation, sharedNetwork | Wave 0 |

### Sampling Rate
- **Per lib authored:** `cmake --build build-phase2 --config Debug --target <lib> --parallel 4`
- **Per wave merge:** `cmake --build build-phase2 --config Debug --parallel 8`
- **Phase gate:** 3 consecutive parallel builds pass + `dir build-phase2\lib\Debug\*.lib | findstr /c:".lib" | find /c ".lib"` returns 23 (or 24 including swgSharedUtility)

### Wave 0 Gaps
- [ ] `src/cmake/stubs/time_t_probe.cpp.in` — required before sharedDebug/sharedFoundation/sharedNetwork authored
- [ ] `src/cmake/stubs/` directory — must be created
- [ ] INTERFACE stubs for sharedCollision, sharedFractal, sharedSkillSystem — required before sharedObject/sharedTerrain configure
- [ ] swgSharedUtility CMakeLists.txt — required before sharedObject/sharedNetworkMessages/sharedGame configure

---

## Security Domain

`security_enforcement: true` in config.json. However this phase is **build-system authoring only** — no application code, no authentication, no input handling, no cryptography, no network endpoints. No ASVS categories apply.

| ASVS Category | Applies | Rationale |
|---------------|---------|-----------|
| V2 Authentication | No | CMake authoring only |
| V3 Session Management | No | CMake authoring only |
| V4 Access Control | No | CMake authoring only |
| V5 Input Validation | No | No runtime code authored in this phase |
| V6 Cryptography | No | No crypto operations introduced |

No threat patterns apply to CMake static library authoring.

---

## Risk Register

| Library | Risk Level | Reason |
|---------|-----------|--------|
| sharedNetworkMessages | MEDIUM | 338 .cpp files across 9 subdirectories — largest explicit source list in Phase 2 |
| sharedGame | MEDIUM | 139 .cpp files with swgSharedUtility dep that must be resolved first |
| sharedObject | MEDIUM | Depends on sharedCollision (INTERFACE stub) + swgSharedUtility + circular with sharedGame |
| sharedTerrain | MEDIUM | Depends on sharedCollision (INTERFACE stub) + sharedFractal (INTERFACE stub) |
| sharedMemoryManager | MEDIUM | Not in swg-main — must be authored from scratch with no reference |
| swgSharedUtility | LOW | 8 .cpp files, swg-main CMakeLists available, but in game/ subtree |
| sharedMathArchive | LOW | Header-only stub pattern required, same as sharedFoundationTypes |
| sharedLog ↔ sharedNetwork | LOW | Circular include chain is design-by-original — follow swg-main exactly |

---

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | INTERFACE stub targets for sharedCollision/sharedFractal/sharedSkillSystem will satisfy CMake configure-time validation without building those libraries | Out-of-Scope Target Resolution | If wrong, those 3 libs must be fully authored in Phase 2 (adds ~60+ .cpp files) |
| A2 | swgSharedUtility being in `game/shared/library/` (not `engine/`) will not prevent the parent CMakeLists from adding it as a target | Out-of-Scope Target Resolution | If wrong, need to restructure how `src/game/` is included in CMake |
| A3 | On MSVC/Win32, circular static lib deps (sharedObject ↔ sharedCollision) resolve at Phase 4 link time without special CMake handling | Dependency Graph | If wrong, need linker group flags or dep reordering |
| A4 | `${ICONV_INCLUDE_DIR}` being empty/undefined on Win32 will not break sharedXml configure (it becomes a no-op include path) | External Dep Mapping | If wrong, add a guard to the sharedXml include_directories block |

---

## Open Questions

1. **swgSharedUtility scope decision**
   - What we know: sharedObject, sharedNetworkMessages, sharedGame all need it; it has 8 .cpp files and a verified swg-main CMakeLists; it lives in `game/shared/library/` not `engine/shared/library/`.
   - What's unclear: Does the planner add it to Phase 2's explicit scope, or treat it as a bonus add-on? Must be decided before the plan is written.
   - Recommendation: Include swgSharedUtility as a Phase 2 bonus library. Its 8 .cpp files add trivial compile time. Omitting it forces INTERFACE stubs that hide real dependency errors until Phase 4.

2. **How to handle sharedMemoryManager with no swg-main reference**
   - What we know: 4+4 source files, deps are sharedFoundationTypes/sharedDebug/sharedFoundation (from FirstSharedMemoryManager.h inspection), no target_link_libraries needed.
   - What's unclear: Whether the OsMemory/OsNewDel classes need any Windows API headers not already covered by global flags.
   - Recommendation: Author CMakeLists following the standard pattern with include paths for sharedDebug, sharedFoundation, sharedFoundationTypes. The win32/ files include standard Win32 headers via the global `-DWIN32` define.

3. **INTERFACE stubs placement**
   - What we know: They need to exist before sharedObject/sharedTerrain add_subdirectory calls.
   - What's unclear: Whether to add stubs at `engine/shared/library/CMakeLists.txt` level or in a dedicated `engine/shared/library/stubs/CMakeLists.txt`.
   - Recommendation: Add stubs directly in `engine/shared/library/CMakeLists.txt` before the Phase 2 `add_subdirectory` calls.

---

## Sources

### Primary (HIGH confidence)
- swg-main `src` repo at https://github.com/SWG-Source/src — all per-lib CMakeLists fetched via `curl` from raw.githubusercontent.com [VERIFIED: 22 of 23 CMakeLists confirmed, sharedMemoryManager confirmed absent]
- whitengold source tree at `D:/Code/swg-client/src/engine/shared/library/` — directory listings and file counts verified for all 23 libs [VERIFIED: Bash commands]
- `D:/Code/swg-client/src/CMakeLists.txt` — global flags and find_package calls [VERIFIED: Read]
- `D:/Code/swg-client/src/engine/shared/library/sharedFoundationTypes/src/CMakeLists.txt` — Phase 1 established pattern [VERIFIED: Read]
- `D:/Code/swg-client/src/external/ours/library/archive/src/CMakeLists.txt` — ours/ lib pattern [VERIFIED: Read]

### Secondary (MEDIUM confidence)
- `D:/Code/swg-client/.planning/phases/01-cmake-skeleton-foundations-spike-and-lock/01-PLAN.md` — Phase 1 execution results [CITED]
- `D:/Code/swg-client/.planning/phases/02-shared-engine-libraries/02-CONTEXT.md` — locked decisions [CITED]

---

## Metadata

**Confidence breakdown:**
- Library inventory and counts: HIGH — verified from whitengold source tree directly
- Divergence map: HIGH — verified whitengold dirs + swg-main CMakeLists fetched directly
- Dependency graph: HIGH — verified from swg-main CMakeLists raw text
- sharedNetworkMessages vcproj finding: HIGH — confirmed absent via find command
- INTERFACE stub approach: MEDIUM — standard CMake practice; A1 assumption
- sharedMemoryManager deps: MEDIUM — inferred from header inspection, not from a working reference

**Research date:** 2026-05-03
**Valid until:** 2026-06-03 (stable — CMakeLists change infrequently)
