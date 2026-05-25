---
phase: 1
phase_name: CMake Skeleton + Foundations + Spike-and-Lock
plan_id: 01
status: completed
mode: standard
requirements_addressed:
  - BUILD-01
  - BUILD-02
  - FLAGS-01
  - FLAGS-02
  - FLAGS-03
  - ARTIF-03
  - DEV-03
  - DEV-05
  - INV-01
  - INV-02
depends_on: []
files_modified:
  - src/CMakeLists.txt
  - src/cmake/win32/FindBoost.cmake
  - src/cmake/win32/FindDirectX9.cmake
  - src/cmake/win32/FindLibXml2.cmake
  - src/cmake/win32/FindMiles.cmake
  - src/cmake/win32/FindBink.cmake
  - src/cmake/win32/FindDPVS.cmake
  - src/cmake/win32/FindMozilla.cmake
  - src/cmake/win32/FindVivox.cmake
  - src/cmake/win32/FindSTLPort.cmake
  - src/cmake/win32/FindLogitechLCD.cmake
  - src/cmake/win32/FindVideoCapture.cmake
  - external/ours/library/archive/CMakeLists.txt
  - external/ours/library/crypto/CMakeLists.txt
  - external/ours/library/fileInterface/CMakeLists.txt
  - external/ours/library/localization/CMakeLists.txt
  - external/ours/library/localizationArchive/CMakeLists.txt
  - external/ours/library/singleton/CMakeLists.txt
  - external/ours/library/unicode/CMakeLists.txt
  - external/ours/library/unicodeArchive/CMakeLists.txt
  - external/3rd/library/udplibrary/CMakeLists.txt
  - engine/shared/library/sharedFoundationTypes/CMakeLists.txt
  - .gitignore
  - .planning/STATE.md
  - .planning/ROADMAP.md
---

# Phase 1 Plan

Build the CMake skeleton, lock the Phase 1 spike decisions, and produce the
first compile-and-link foundation for the client modernisation port without
touching any C++ source files.

## Goals

1. Stand up the top-level CMake entry points and path variables for the client-only
   build graph.
2. Author the initial `Find*.cmake` modules for the vendored SDKs and clean up the
   known swg-main copy-paste bugs in `FindBoost` and `FindLibXml2`.
3. Build the foundation libraries that everything else depends on.
4. Lock the three Phase 1 decisions:
   - STLPort linkage path
   - DirectX 9 SDK source
   - Mozilla XPCOM disposition
5. Verify that the Phase 1 foundation targets configure and build as static
   libraries without C++ source edits.

## Execution Waves

### Wave 1: CMake Skeleton and Toolchain Policy

Deliverables:

- `src/CMakeLists.txt`
- global toolchain settings for Win32, C++17, `/MT` and `/MTd`
- compile flag policy for `/Zc:wchar_t-`, `_USE_32BIT_TIME_T=1`, `_MBCS`,
  `/MP`, `/Ob1`, `/FC`, and the legacy warning suppressions
- `.gitignore` coverage for generated CMake and IDE artefacts

Tasks:

1. Mirror the swg-main-style top-level CMake shape, but scoped to the client tree.
2. Set the non-negotiable compile flags once at the toolchain level.
3. Keep `/WX` off for Phase 1.
4. Add the Windows-only build guards that keep the graph narrow.
5. Ensure the repo layout leaves the original `.vcproj` files untouched.

### Wave 2: Vendored Find Modules

Deliverables:

- `src/cmake/win32/FindBoost.cmake`
- `src/cmake/win32/FindDirectX9.cmake`
- `src/cmake/win32/FindLibXml2.cmake`
- `src/cmake/win32/FindMiles.cmake`
- `src/cmake/win32/FindBink.cmake`
- `src/cmake/win32/FindDPVS.cmake`
- `src/cmake/win32/FindMozilla.cmake`
- `src/cmake/win32/FindVivox.cmake`
- `src/cmake/win32/FindSTLPort.cmake`
- `src/cmake/win32/FindLogitechLCD.cmake`
- `src/cmake/win32/FindVideoCapture.cmake`

Tasks:

1. Reuse the swg-main patterns only where they are known-good.
2. Replace the known broken `FindBoost` and `FindLibXml2` logic with clean
   repo-specific modules.
3. Keep Boost as a header-only INTERFACE target.
4. Keep libxml2 pinned to the vendored tree.
5. Make the Find modules resolve against `src/external/3rd/library/` by default.

### Wave 3: Foundation Libraries

Deliverables:

- `external/ours/library/*/CMakeLists.txt` for the v1 foundation set
- `external/3rd/library/udplibrary/CMakeLists.txt`
- `engine/shared/library/sharedFoundationTypes/CMakeLists.txt`

Tasks:

1. Build the `ours/` libraries first.
2. Build `udplibrary` as an in-tree static library.
3. Build `sharedFoundationTypes` as the lowest engine-side foundation target.
4. Keep the target layout close to swg-main where it exists, but do not copy
   broken patterns blindly.

### Wave 4: Spike and Lock Decisions

This wave is the Phase 1 risk gate. Do not expand beyond the decision surface
until the following are resolved:

1. STLPort: try the existing vc71 prebuilt first; if it fails under v143, switch
   to the fallback rebuild path.
2. DirectX 9: verify the vendored SDK headers and import libs are usable on the
   current toolchain.
3. Mozilla XPCOM: lock the client-side strategy to a stubbed `libMozilla::init`
   path rather than real XPCOM linkage.

Decision output must be recorded in `PROJECT.md` and reflected in the Phase 1
planning artifacts before broad library authoring continues.

## Verification

The plan is not complete until these checks are satisfied:

1. `cmake -S src -B build-phase1 -G "Visual Studio 17 2022" -A Win32 -T v143,host=x64 -DWHITENGOLD_USE_STLPORT_HEADERS=ON`
   configures successfully.
2. The generated build graph includes the foundation libraries and the initial
   vendored SDK Find modules.
3. The Debug configuration builds the foundation static libraries.
4. The original `.vcproj` files remain present and unmodified.
5. No C++ source files under `src/` are changed in this phase.

## Threat Model

<threat_model>
  <threat>
    <name>CRT mismatch</name>
    <impact>LNK2005 storms or runtime heap corruption if `/MT` and `/MD` mix.</impact>
    <mitigation>Set `CMAKE_MSVC_RUNTIME_LIBRARY` globally and verify imports.</mitigation>
  </threat>
  <threat>
    <name>STLPort ABI mismatch</name>
    <impact>Link failures or subtle runtime problems if the chosen STLPort path is wrong.</impact>
    <mitigation>Lock the STLPort decision early and verify with a minimal link test.</mitigation>
  </threat>
  <threat>
    <name>DirectX 9 header conflict</name>
    <impact>Configure or compile failures against the modern Windows SDK.</impact>
    <mitigation>Prefer the vendored SDK path first and fall back only if required.</mitigation>
  </threat>
  <threat>
    <name>Mozilla XPCOM overreach</name>
    <impact>Large header and ABI surface can sink the phase before the first link.</impact>
    <mitigation>Use the stubbed client-side path and do not link real XPCOM in Phase 1.</mitigation>
  </threat>
  <threat>
    <name>Source-edit creep</name>
    <impact>Touching C++ sources expands risk beyond the milestone contract.</impact>
    <mitigation>Keep Phase 1 limited to build files, config, and planning artifacts.</mitigation>
  </threat>
</threat_model>

## Done When

- The foundation CMake skeleton exists.
- The vendored dependency modules are in place.
- The Phase 1 spike decisions are locked.
- The Phase 1 foundation static libraries build in Debug.
- The planning artifacts and state file reflect the completed plan.

## Execution Results

Completed on 2026-05-04.

Verification performed:

1. Configure passed with VS 2022 Win32 and toolset `v143,host=x64`.
2. Debug build passed with `cmake --build build-phase1 --config Debug --parallel 8`.
3. Release build passed with `cmake --build build-phase1 --config Release --parallel 8`.
4. Static libraries emitted in both Debug and Release: `archive`, `crypto`, `fileInterface`, `localization`, `localizationArchive`, `singleton`, `unicode`, `unicodeArchive`, `udplibrary`, `sharedFoundationTypes`.
5. `archive.vcxproj` verifies static CRT selection: `MultiThreadedDebug` for Debug and `MultiThreaded` for Release.
6. `git diff --name-only -- '*.cpp' '*.cxx' '*.cc' '*.h' '*.hpp' '*.inl' '*.vcproj'` returned empty, confirming no C++ source or original `.vcproj` edits.

Notes:

1. STLPort is retained through a build-local staged include layout. The staged copy includes modern MSVC/UCRT native headers and patches only generated build files, leaving vendored source untouched.
2. DirectX 9 is resolved from the vendored SDK. The first real DX9 compile remains Phase 3.
3. Mozilla is resolved as headers-only for Phase 1; no `xul.lib` or `xpcom.lib` link is introduced.
4. Non-blocking warnings remain: CMake deprecation warnings from old nested minimum versions, and `crypto` warning C4530 because legacy exception unwind semantics are not enabled.
