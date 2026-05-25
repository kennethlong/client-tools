---
phase: 08-dead-code-removal-track-b
plan: 01
subsystem: build-system
tags: [cmake, find-modules, mfc, qt3, stlport, perforce]

requires:
  - Phase 7 complete (Track A — voice chat + XPCOM removed)
provides:
  - FindQt3.cmake (Qt 3.3.4 SDK + qt3_wrap_cpp/qt3_wrap_ui macros)
  - FindMaya.cmake (Maya 8 SDK)
  - 8 wired engine/shared/application/ tool targets
  - 3 new static library targets (sharedStatusWindow, sharedTemplate, sharedTemplateDefinition)
  - perforce_tzname_compat OBJECT lib (Perforce P4API UCRT bridge)
affects:
  - src/CMakeLists.txt
  - src/cmake/win32/Find{Qt3,Maya}.cmake
  - src/cmake/stubs/perforce_tzname_compat.c
  - src/engine/shared/{,library/,application/}CMakeLists.txt
  - src/engine/shared/library/{sharedStatusWindow,sharedTemplate,sharedTemplateDefinition}/{,src/}CMakeLists.txt
  - 13 src/engine/shared/application/<Tool>/CMakeLists.txt files
  - src/engine/shared/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp (NGE API restoration)

tech-stack:
  added:
    - Qt 3.3.4 vendored SDK now CMake-discoverable via find_package(Qt3 REQUIRED)
    - Maya 8 vendored SDK now CMake-discoverable via find_package(Maya REQUIRED)
  patterns:
    - $<TARGET_OBJECTS:stlport_vc143_compat> in tool add_executable lists
    - LINK_FLAGS_DEBUG /FORCE:MULTIPLE for tools that link engine libs (matches SwgClient pattern)
    - find_package(Qt3) macros qt3_wrap_cpp/qt3_wrap_ui generate moc/ui artifacts via custom commands

key-files:
  created:
    - src/cmake/win32/FindQt3.cmake
    - src/cmake/win32/FindMaya.cmake
    - src/cmake/stubs/perforce_tzname_compat.c
    - src/engine/shared/application/CMakeLists.txt
    - src/engine/shared/library/sharedStatusWindow/CMakeLists.txt
    - src/engine/shared/library/sharedStatusWindow/src/CMakeLists.txt
    - src/engine/shared/library/sharedTemplate/CMakeLists.txt
    - src/engine/shared/library/sharedTemplate/src/CMakeLists.txt
    - src/engine/shared/library/sharedTemplateDefinition/CMakeLists.txt
    - src/engine/shared/library/sharedTemplateDefinition/src/CMakeLists.txt
    - src/engine/shared/application/LabelHashTool/CMakeLists.txt
    - src/engine/shared/application/Md5sum/CMakeLists.txt
    - src/engine/shared/application/TreeFileBuilder/CMakeLists.txt
    - src/engine/shared/application/TreeFileExtractor/CMakeLists.txt
    - src/engine/shared/application/TreeFileRspBuilder/CMakeLists.txt
    - src/engine/shared/application/DataLintRspBuilder/CMakeLists.txt
    - src/engine/shared/application/StringFileTool/CMakeLists.txt
    - src/engine/shared/application/WordCountTool/CMakeLists.txt
    - src/engine/shared/application/UpdateLocalizedStrings/CMakeLists.txt
    - src/engine/shared/application/P4Qt/CMakeLists.txt
    - src/engine/shared/application/TemplateCompiler/CMakeLists.txt
    - src/engine/shared/application/TemplateDefinitionCompiler/CMakeLists.txt
    - src/engine/shared/application/Turf/CMakeLists.txt
  modified:
    - src/CMakeLists.txt (added perforce_tzname_compat OBJECT lib + STLPort isolation)
    - src/engine/shared/CMakeLists.txt (added add_subdirectory(application))
    - src/engine/shared/library/CMakeLists.txt (added 3 new library subdirs)
    - src/engine/shared/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp (replaced removed borrowCompressor API)

key-decisions:
  - 5 MFC tools (TreeFileRspBuilder, DataLintRspBuilder, StringFileTool, WordCountTool, Turf) deferred to Phase 9 — STLPort 4.5.3 cannot coexist with <afxwin.h> on the same translation unit under MSVC 14.44. Per-tool CMakeLists.txt authored and in tree but commented out in the tier parent.
  - MFC for v143 build tools installed as new VS 2022 component (one-time tooling install) — required for any future MFC tool builds.
  - P4Qt link list extended with ws2_32 + wsock32 (Perforce librpc.lib needs Winsock at link time).
  - perforce_tzname_compat: new OBJECT lib supplying __tzname as data symbol, with empty INCLUDE_DIRECTORIES so STLPort's time.h (which redefines __tzname as a function) does not pollute it.
  - All tool targets carry $<TARGET_OBJECTS:stlport_vc143_compat> + LINK_FLAGS_DEBUG /FORCE:MULTIPLE — same compat pattern SwgClient uses to bridge prebuilt vc71 STLPort against MSVC 2022.

requirements-completed:
  - CLEAN-06 (partially — 8 of ~12 named tools building; 5 deferred to Phase 9)

duration: "across multiple sessions (5 May → 7 May, 2026)"
completed: 2026-05-07
---

# Phase 8 Plan 1: engine/shared/application/ Tool CMake Wiring Summary

Wired the engine/shared/application/ tool tier into the v2 CMake graph by authoring two new vendored-SDK Find modules (Qt 3.3.4, Maya 8), three new static-library `CMakeLists.txt` files for previously-unbuilt engine libraries (`sharedStatusWindow`, `sharedTemplate`, `sharedTemplateDefinition`), and 13 per-tool `CMakeLists.txt` files. Eight tools build cleanly under `cmake --build build --config Debug`; five MFC tools are wired but gated behind a Phase 9 STLPort migration marker after `<afxwin.h>` was found to be incompatible with STLPort 4.5.3 on modern MSVC.

## Result

- 8/13 wave-1 tools building: `LabelHashTool`, `Md5sum`, `TreeFileBuilder`, `TreeFileExtractor`, `UpdateLocalizedStrings`, `TemplateCompiler`, `TemplateDefinitionCompiler`, `P4Qt`
- 5/13 wave-1 tools deferred to Phase 9 (MFC ↔ STLPort): `TreeFileRspBuilder`, `DataLintRspBuilder`, `StringFileTool`, `WordCountTool`, `Turf`
- `SwgClient_d.exe` continues to build — no regression
- Full Debug build of every now-enabled target exits 0 (only LNK4006/LNK4088 informational warnings, expected from the v1 STLPort dual-link pattern)

## Tasks

| # | Task | Commits |
|---|------|---------|
| 1 | Find modules + parent chain wiring + 3 library CMakeLists | `bfab8a1f7` |
| 2 | 13 per-tool CMakeLists.txt + Perforce/UCRT compat shim + library include fixes | `c4fe90f84` |

Two atomic commits, ~937 lines added, 27 lines removed in commit 2.

## Self-Check: PASSED

| Acceptance Criterion (from PLAN.md) | Result |
|--|--|
| `FindQt3.cmake` present, `find_package(Qt3)` resolves | PASS |
| `FindMaya.cmake` present, `find_package(Maya)` resolves | PASS |
| `engine/shared/application/` wired via `add_subdirectory(application)` | PASS |
| `sharedStatusWindow`/`sharedTemplate`/`sharedTemplateDefinition` produce static libs | PASS (each is a target) |
| `cmake --build build --config Debug --target LabelHashTool` exits 0 | PASS |
| `cmake --build build --config Debug --target TreeFileBuilder` exits 0 | PASS |
| `cmake --build build --config Debug --target TemplateCompiler` exits 0 | PASS |
| `cmake --build build --config Debug --target Turf` exits 0 | DEFERRED (MFC, gated behind Phase 9) |
| MFC tools (`DataLintRspBuilder` etc.) contain `mfcs140d` | PASS (link list intact, target gated) |
| `P4Qt/CMakeLists.txt` contains `find_package(Qt3 REQUIRED)` | PASS |
| `SwgClient_d.exe` continues to build | PASS — no regression |

## Deviations from Plan

**[Rule 1 — bug] borrowCompressor API removal in TreeFileBuilder.cpp**
Found during: Task 2 (TreeFileBuilder build).
Issue: `TreeFile::SearchTree::borrowCompressor` and `returnCompressor` are no longer in the engine API (NGE-era refactor removed them — present only in this leaked tree's tool but not in the engine).
Fix: Replaced with direct `new ZlibCompressor()` / `delete compressor` (CT_zlib is the only compressor type the tool exercises).
Files modified: `src/engine/shared/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp` (~7 lines).
Verification: `cmake --build build --config Debug --target TreeFileBuilder` exits 0.
Commit hash: `c4fe90f84`.

**[Rule 1 — missing critical] Perforce libsupp.lib needs `__tzname` data symbol**
Found during: Task 2 (TemplateCompiler / TemplateDefinitionCompiler / P4Qt linking).
Issue: vendored `perforce/lib/win32/libsupp.lib` was built against pre-UCRT CRT and references `__tzname` as a data symbol. Modern UCRT exposes it only as a function via `time.h`, and STLPort's `time.h` shim redefines it as a function — both lead to LNK2001 unresolved external.
Fix: New OBJECT library `perforce_tzname_compat` compiles `src/cmake/stubs/perforce_tzname_compat.c` with empty `INCLUDE_DIRECTORIES` (so STLPort's `time.h` is bypassed) and exports `__tzname` as a `char *[2]` data symbol. Tools that link Perforce libs include `$<TARGET_OBJECTS:perforce_tzname_compat>` in their `add_executable` source list.
Files modified: `src/CMakeLists.txt` (root), `src/cmake/stubs/perforce_tzname_compat.c` (new), `src/engine/shared/application/{TemplateCompiler,TemplateDefinitionCompiler,P4Qt}/CMakeLists.txt`.
Commit hash: `c4fe90f84`.

**[Rule 1 — missing critical] P4Qt missing Winsock libs**
Found during: Task 2 (P4Qt link).
Issue: `librpc.lib(nettcp.obj)` references `_ntohs`, `_gethostbyaddr`, `_getsockname` — Winsock APIs.
Fix: Added `ws2_32` + `wsock32` to P4Qt's `target_link_libraries`.
Commit hash: `c4fe90f84`.

**[Rule 1 — missing critical] `sharedStatusWindow` source layout — `win32/` not `shared/`**
Found during: Task 1 (initial sharedStatusWindow library compile).
Issue: Original `src/CMakeLists.txt` referenced `shared/Foo.cpp` files, but actual sources live under `win32/` (only `FirstSharedStatusWindow.h` exists in `shared/`).
Fix: Switch source list to `win32/*.cpp`. Same correction also applied include directories (added many engine + ours/* paths needed by the actual sources).
Commit hash: `c4fe90f84`.

**[Rule 1 — missing critical] `sharedTemplateDefinition` PCRE link missing**
Found during: Task 1 (sharedTemplateDefinition library compile).
Issue: `TemplateData.cpp` and `TpfFile.cpp` use `pcre_compile`/`pcre_exec` but link list omitted PCRE.
Fix: Added `${PCRE_LIBRARY}`, `sharedRegex`, `sharedRandom`, `sharedUtility` and `target_compile_definitions(... PRIVATE PCRE_STATIC)`.
Commit hash: `c4fe90f84`.

**[Rule 4 — architectural change → user gate] 5 MFC tools deferred to Phase 9**
Found during: Task 2 (after MFC component installed via VS Installer).
Issue: With MFC headers present, `<afxwin.h>` pulls in MSVC's modern `<type_traits>`. STLPort 4.5.3 is on the include path before the system include path, so when `<map>`/`<vector>`/etc. are subsequently pulled by the engine PCH, STLPort's `_STL::_Is_memfunptr` partial specialization tries to use `enable_if` — which is in MSVC `std::`, not STLPort `_STL::`. Even with `_EXPORT_STD=` predefined to fix the first parse error, subsequent C2061 errors surface with no clean workaround short of removing STLPort entirely.
Fix: Per user decision — keep authored CMakeLists for the 5 MFC tools, but comment out their `add_subdirectory()` lines in the tier parent with a `DEFERRED to Phase 9` block. Phase 9 (STLPort → MSVC STL) eliminates this conflict; re-enable then.
Files modified: `src/engine/shared/application/CMakeLists.txt` (only the parent — per-tool files unchanged).
Commit hash: `c4fe90f84`.

**Total deviations:** 6 — 5 auto-fixed (Rule 1 missing/bug) + 1 user-gated architectural decision (Rule 4 → defer 5 MFC tools to Phase 9).

**Impact:** Plan 08-01 satisfies CLEAN-06 partially. 8 of the named tools build; 5 are wired but parked behind a one-line CMake comment until Phase 9 lands. Wave 2 (Plan 08-02) is unblocked since `FindQt3.cmake`, `FindMaya.cmake`, and `sharedStatusWindow` (the MayaExporter prerequisite) are all present.

## Authentication Gates

None.

## Issues Encountered

- **MFC tooling install required**: VS 2022 was missing the "C++ MFC for v143 build tools" component. User installed it mid-execution (one-time, ~5 min). Without it the 5 MFC tools error out with `Cannot open include file: 'afxwin.h'` even before hitting the STLPort interaction.
- **STLPort ↔ MFC incompatibility**: confirmed and characterized. Path forward is Phase 9.

## Next Phase Readiness

Ready for **Plan 08-02** (Wave 2 — `engine/client/application/` Qt GUI tools + `MayaExporter` + non-Qt client tools). All Phase-8 prerequisites this plan was supposed to deliver — `FindQt3.cmake` macros, `FindMaya.cmake`, `sharedStatusWindow` static lib — are in place. `MayaExporter` will additionally need the Maya 8 SDK headers; verify on first encounter.
