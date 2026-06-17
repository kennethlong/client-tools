---
phase: 33-x64-build-platform-d3d9-renderers
plan: 04
subsystem: build
tags: [x64, msbuild, vcxproj, swg.sln, dpvs, bits-02, width-truncation, winsock, miles, bink, platform-add]

# Dependency graph
requires:
  - phase: 33-x64-build-platform-d3d9-renderers
    plan: 01
    provides: "x64-platform.props (config-split Debug|x64/Release|x64, N1 /we4311/4312 guardrail) + tinyxml.vcxproj (authored, to be registered)"
provides:
  - "x64 platform in swg.sln: Debug|x64 + Release|x64 solution configs + per-boot-project mappings (62 engine/3rd-party .Build.0; 4 plugin/exe ActiveCfg-only; editors excluded; tinyxml registered)"
  - "x64 Debug|x64 + Release|x64 configs on the full boot-path engine StaticLibrary tier (~56) + dpvs/zlib/ui/udplibrary, importing x64-platform.props with isolated \\x64\\ OutDirs"
  - "dpvs x64-from-source: !_M_X64 CPU-detect guard + UPTR 64-bit + truncation pass (builds Debug|x64 + Release|x64 clean)"
  - "3 D3D9 plugin .vcxproj x64 ProjectConfiguration entries (no Link block -- Plan 05 owns it)"
  - "BITS-02 x64 width fixes in 8 source TUs (winsock SOCKET, Bink/Miles handles, crypto/ui pointer casts) -- Win32 ABI preserved"
affects: [33-05]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "x64 vcxproj add: ImportGroup-imports x64-platform.props + per-project ClCompile (includes/PCH) + isolated compile/win32/<proj>/x64/<cfg>/ OutDir; additive (no Win32 block touched)"
    - "pointer-width handle fix idiom: U32/unsigned-int handle storing a pointer -> UINTa/size_t/intptr_t (x86-identical, x64-correct)"
    - "SOCKET-shadow fix: guard local typedef + pull canonical winsock2.h (matches SDK UINT_PTR)"

key-files:
  created: []
  modified:
    - src/build/win32/swg.sln
    - src/external/3rd/library/dpvs/implementation/include/dpvsPrivateDefs.hpp
    - src/external/3rd/library/dpvs/implementation/msvc8/dpvs.vcxproj
    - src/external/3rd/library/dpvs/implementation/sources/dpvsImpObject.cpp
    - src/external/3rd/library/dpvs/implementation/sources/dpvsMemory.cpp
    - src/external/3rd/library/dpvs/implementation/sources/dpvsRecursionSolver.cpp
    - src/engine/shared/library/sharedNetwork/src/win32/Sock.h
    - src/external/3rd/library/udplibrary/UdpLibrary.hpp
    - src/external/ours/library/crypto/src/shared/original/misc.cpp
    - src/external/ours/library/crypto/src/shared/original/misc.h
    - src/external/3rd/library/ui/src/shared/table/UITableModel.cpp
    - src/external/3rd/library/ui/src/win32/UILoader.cpp
    - src/external/3rd/library/ui/src/win32/UITabbedPane.cpp
    - src/engine/client/library/clientGraphics/src/Bink/BinkTreeFileIO.cpp
    - src/engine/client/library/clientGame/src/shared/test/WaterTestAppearance.cpp
    - src/engine/client/library/clientAudio/src/win32/Audio.cpp
    - "~56 engine StaticLibrary .vcxproj + zlib/ui/udplibrary.vcxproj (x64 configs)"
    - "3 D3D9 plugin .vcxproj (x64 ProjectConfiguration only)"

key-decisions:
  - "dpvs built x64 as StaticLibrary from in-tree source (D-02, NOT the Restoration dll fallback) -- the !_M_X64 guard alone was insufficient: DPVS_CPU_NAME and UPTR (typedef unsigned int) also needed x64 handling"
  - "x64-platform.props imported via <ImportGroup Label=\"PropertySheets\"> (matching tinyxml.vcxproj), not inlined -- the per-project ClCompile (includes/PCH) + <Lib> OutputFile stay in each .vcxproj's own x64 ItemDefinitionGroup"
  - "sharedTemplateDefinition EXCLUDED (tool-only, 0 refs in SwgClient.vcxproj AdditionalDependencies -- confirmed)"
  - "clientAudio Miles AIL_file_*_callback ABI fixed HERE (U32->UINTa) so the lib compiles x64; the runtime AIL_* #if _M_X64 stub remains Plan 05's job. The handle is a synthetic counter, not a pointer, so only the callback signature changed"
  - "SwgClient gets .Debug|x64.ActiveCfg only (per plan) even though its vcxproj has no Debug|x64 config yet -- MSBuild tolerates ActiveCfg-to-undeclared; Plan 05 adds the ProjectConfiguration + Link + promotes to .Build.0"

requirements-completed: [X64-01, X64-04]

# Metrics
duration: ~110min
completed: 2026-06-17
---

# Phase 33 Plan 04: x64 Build Platform Add Summary

**Stood up the x64 platform surface: Debug|x64 + Release|x64 in swg.sln with per-boot-project mappings (62 engine/3rd-party buildable, 4 plugin/exe ActiveCfg-only, editors excluded, tinyxml registered) and across the full boot-path static-lib tier (~57 libs importing x64-platform.props with isolated \\x64\\ OutDirs). dpvs builds x64 from source (guard + UPTR-64 + truncation pass). The entire boot-path lib tier compiles /p:Platform=x64 clean (62/62, 0 error C) after 8 BITS-02 width fixes; the 32-bit build is provably untouched — Plan 05 (first x64 link) is unblocked.**

## Performance
- **Duration:** ~110 min
- **Tasks:** 3
- **Files modified:** 79 (1 sln + ~60 vcxproj + ~16 source + 1 dpvs hpp)

## Accomplishments

### Task 1 — dpvs guard + config + truncation pass; sharedMath reference (28a8e967d)
- **dpvs Pitfall-4 guard:** `dpvsPrivateDefs.hpp` guards `DPVS_CPU_X86` on `!_M_X64` so the x86 inline asm is off on x64 → the portable-C LUT fallbacks compile. The guard alone was NOT sufficient (the plan's stated risk understated it): `DPVS_CPU_NAME` is set inside the `DPVS_CPU_X86` block (so x64 hit `#error Unrecognized CPU name` — fixed by defining `DPVS_CPU_NAME "X64"`), and `typedef unsigned int UPTR` truncated every pointer↔UPTR cast on x64 (fixed to `unsigned __int64` on x64, which also cleared the `sizeof(UPTR)>=sizeof(void*)` CT_ASSERT). Residual truncation pass (3 sites): `MatrixCache::Entry` padding kept `sizeof==64`; RecursionSolver hash + Memory debug-guard tags cast through `UPTR`.
- **dpvs.vcxproj:** Debug|x64 + Release|x64 StaticLibrary configs (was DynamicLibrary on Win32), import x64-platform.props, isolated `\x64\` OutDir, `DPVS_BUILD_LIBRARY` define. Builds **from source** (D-02, no Restoration dll fallback needed) — both Debug|x64 and Release|x64 clean, 0 `__asm` reached.
- **sharedMath:** the proven x64 StaticLibrary reference block (import props, isolated OutDir). Built x64 clean; Win32 lib untouched (distinct OutDir).

### Task 2 — x64 across the lib tier + plugin ProjectConfigs + width fixes (cbfe631be)
- **~56 engine StaticLibrary .vcxprojs + zlib/ui/udplibrary:** Debug|x64 + Release|x64 added mechanically (a scratch Python transform mirroring the sharedMath/tinyxml pattern), each importing x64-platform.props with an isolated `compile/win32/<proj>/x64/<cfg>/` OutDir. Strictly additive — `git diff` shows no Win32 block/OutDir change (the single per-file "deletion" is the `Microsoft.Cpp.props` import line relocating, not a content change).
- **3 D3D9 plugin .vcxprojs:** x64 ProjectConfiguration + Configuration PropertyGroups ONLY (reviews fix #9) — `grep MachineX64 == 0`, no Link block (Plan 05).
- **8 BITS-02 x64 width fixes** (surfaced by `/we4311/4312` + SDK header widths on x64; all preserve the Win32 ABI):
  | TU | Defect | Fix |
  |----|--------|-----|
  | sharedNetwork `Sock.h` + udplibrary `UdpLibrary.hpp` | local `typedef unsigned int SOCKET` shadows the SDK `UINT_PTR SOCKET` → C2371 "different basic types" on x64 | pull canonical `winsock2.h` / guard the typedef on `!_WINSOCK*API_` + pointer-width |
  | crypto `misc.cpp`/`misc.h` | alignment-test `(unsigned int)ptr % N` | cast through `size_t` (alignment still correct) |
  | ui `UITableModel.cpp` | `reinterpret_cast<int>(pointer)` model handle | `static_cast<int>(reinterpret_cast<intptr_t>(...))` (low-32 handle) |
  | ui `UILoader.cpp` | `ptrdiff_t` ambiguous for the UI stream `<<` | `static_cast<int>` |
  | ui `UITabbedPane.cpp` | `_asm nop` (x64-illegal) | `__noop` |
  | clientGraphics `BinkTreeFileIO.cpp` | file handle `U32` round-trips an `AbstractFile*` | handle type `U32 -> UINTa` + UINTa error sentinel |
  | clientGame `WaterTestAppearance.cpp` | sort-key `reinterpret_cast<int>(ptr)` | via `intptr_t` |
  | clientAudio `Audio.cpp` | Miles `AIL_file_*_callback` ABI uses `U32` not `UINTa` (Mss.h) | 4 callback signatures `U32 -> UINTa` (handle is a synthetic counter, not a pointer) |
- **blat.vcxproj:** x64 include dirs (`src/win32`, `GENSOCK`) + `GENSOCK_STATIC_LINK` define.
- **Result:** the full boot-path lib set compiles `/p:Platform=x64` clean — **62/62 libs, 0 `error C`** (final verification pass with `$(SolutionDir)` defined). Win32 non-regression spot-verified on every source-modified lib (crypto/ui/udplibrary/clientGraphics/clientAudio/sharedNetwork all build Win32 EXIT 0).

### Task 3 — swg.sln x64 configs + mappings + tinyxml registration (f80153e72)
- `SolutionConfigurationPlatforms`: `Debug|x64` + `Release|x64`.
- **62 engine-lib + 3rd-party GUIDs** (incl. dpvs/zlib/ui/udplibrary/tinyxml): `.ActiveCfg` + `.Build.0` (buildable x64 now).
- **3 D3D9 plugin GUIDs + SwgClient:** `.ActiveCfg` ONLY, **NO `.Build.0`** (reviews fix #9 — prevents a between-wave MSB8020; Plan 05 promotes them once their Link blocks exist).
- **Editors:** no x64 mapping (excluded from an x64 solution build).
- **tinyxml.vcxproj** (Plan 01) registered as a new Project + boot-path x64 mapping.
- sln verified well-formed: `sharedMath` + `tinyxml` build `/p:Platform=x64` via `swg.sln` (EXIT 0); BOM + CRLF byte-preserved, pure-additive vs the prior sln.

## dpvs disposition
**Built x64 from in-tree SOE source (D-02), NOT the Restoration `dpvs.dll` fallback.** The from-source path succeeded after the guard + `DPVS_CPU_NAME "X64"` + `UPTR -> unsigned __int64` + a 3-site truncation pass. No lifted binary introduced (T-33-09 mitigated by build-from-source).

## Libs that needed an in-place x64 source fix
dpvs (CPU detect + UPTR + 3 truncation sites), sharedNetwork + udplibrary (SOCKET), crypto, ui (3 files incl. an `_asm`), clientGraphics (Bink handle), clientGame (WaterTest), clientAudio (Miles callback ABI). All other ~50 libs compiled x64 with the config-add alone (Phase 31 had already made the tree x64-clean).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] dpvs needed more than the !_M_X64 guard (DPVS_CPU_NAME + UPTR)**
- **Found during:** Task 1 (dpvs x64 build)
- **Issue:** After the `!_M_X64` guard, x64 hit `#error Unrecognized CPU name` (DPVS_CPU_NAME is defined inside the now-skipped DPVS_CPU_X86 block) and then hundreds of C4311/C4312 from `typedef unsigned int UPTR` (32-bit). The plan anticipated "a self-contained truncation pass" but framed the guard as sufficient.
- **Fix:** Define `DPVS_CPU_NAME "X64"` in the guard's `#else`; make `UPTR` `unsigned __int64` on x64 (clears the bulk + the `sizeof(UPTR)>=sizeof(void*)` CT_ASSERT); 3 residual cast/padding fixes.
- **Committed in:** 28a8e967d

**2. [Rule 1 - Bug] SOCKET width defect in udplibrary + sharedNetwork (C2371 on x64)**
- **Found during:** Task 2 (sharedNetwork x64 build)
- **Issue:** `udplibrary/UdpLibrary.hpp` and `sharedNetwork/Sock.h` each `typedef unsigned int SOCKET`, shadowing the Windows SDK `UINT_PTR SOCKET`. On x64 these are different widths → C2371 "different basic types" hard error wherever a winsock header is also pulled in (Connection/NetworkHandler/Service). Not anticipated by the plan (these libs were "config-add only" per Phase-31 closure).
- **Fix:** udplibrary — guard the typedef on `!_WINSOCK*API_` and make it `_WIN64`-pointer-width; sharedNetwork — pull the canonical `winsock2.h`. Win32 ABI unchanged (UINT_PTR == unsigned int on Win32).
- **Committed in:** cbfe631be

**3. [Rule 1 - Bug] Bink/Miles/crypto/ui/WaterTest pointer-width defects**
- **Found during:** Task 2 (full lib-tier x64 build)
- **Issue:** Several in-scope libs carried unaudited pointer-in-int defects the x64 `/we4311/4312` surfaced (Bink file handle, Miles callback ABI, crypto alignment casts, ui handle/asm, WaterTest sort key). These are the documented Phase-31 class-(A)/3rd-party carry-forwards (WaterTest, Bink, Miles) plus crypto/ui not in Phase-31 scope.
- **Fix:** width-correct each (UINTa/size_t/intptr_t), `_asm nop -> __noop`. All preserve Win32 behavior.
- **Committed in:** cbfe631be

**Note (not a deviation):** archive/blat "failed" in the first standalone pass only because they use `$(SolutionDir)` in their include paths — building them with `/p:SolutionDir=...` (as the sln does) resolves clean. The final verification pass set SolutionDir; 62/62 clean.

**Total deviations:** 3 auto-fixed (all Rule 1/3 width/blocking, no architectural change). The dpvs depth and the SOCKET/Bink/Miles fixes are the substantive ones; none changed on-the-wire data or Win32 behavior.

## Threat surface scan
No new network/auth/crypto/file trust-boundary surface introduced. The SOCKET/Miles/Bink edits are width-correctness on existing code paths. dpvs built from source (T-33-09 mitigated). No threat flags.

## Notes for Plan 05
- **SwgClient x64:** add its Debug|x64/Release|x64 ProjectConfiguration + the x64 `<Link>` block (drop d3dx*/binkw32.lib; swap import-lib names to the -x64 libs per 33-01; `MachineX64`; `stage-x64\` postbuild), then PROMOTE the 4 plugin/exe sln GUIDs from `.ActiveCfg` to add `.Build.0`.
- **Plugins:** add the x64 `<Link>` block (drop d3dx9.lib, `MachineX64`, x64 OutDir, `stage-x64\` postbuild, x64 d3dcompiler_47.dll) — the ProjectConfigurations already exist.
- **Miles:** clientAudio compiles x64 now (callback ABI fixed), but the runtime `AIL_*` `#if _M_X64` stub (boot-silent) is still Plan 05's; the real Miles 9.3b port is Phase 35.
- **Bink:** `BinkVideo.cpp` `binkw32.dll -> binkw64.dll` `#if _M_X64` + stage binkw64.dll is Plan 05.
- All x64 libs land under `compile/win32/<proj>/x64/<cfg>/` — point SwgClient's x64 `AdditionalLibraryDirectories` there.

## User Setup Required
None.

## Self-Check: PASSED
- swg.sln contains "Debug|x64" — verified (62 .Build.0 + SC row).
- dpvsPrivateDefs.hpp contains "_M_X64" — verified (grep == 2).
- All 3 task commits present: 28a8e967d, cbfe631be, f80153e72.
- Full boot-path lib tier compiles x64: 62/62, 0 error C (build-x64-libs-debug.log).

---
*Phase: 33-x64-build-platform-d3d9-renderers*
*Completed: 2026-06-17*
