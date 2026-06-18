---
phase: 33-x64-build-platform-d3d9-renderers
plan: 05
subsystem: build
tags: [x64, msbuild, vcxproj, swg.sln, link, miles, bink, dllexport, d3dx, force-link-grep, bits-02, first-x64-link]

# Dependency graph
requires:
  - phase: 33-x64-build-platform-d3d9-renderers
    plan: 01
    provides: "x64-platform.props + the LIFT import libs (libxml2-x64/pcre-x64/jpeg62-x64) + tinyxml x64 (tinyxmld_STL.lib/tinyxmld.lib)"
  - phase: 33-x64-build-platform-d3d9-renderers
    plan: 04
    provides: "x64 across the boot-path lib tier (62/62 compile x64); the 3 plugin x64 ProjectConfiguration entries (ActiveCfg-only); the clientAudio Miles callback-ABI fix"
provides:
  - "The FIRST fully-linking x64 client: SwgClient_d.exe + gl05/06/07_d.dll all dumpbin machine 8664, staged to stage-x64/"
  - "gl05/06/07 + SwgClient x64 Link blocks (D3DX/bink/Miles dropped, MachineX64, x64 OutDirs, LIFT import libs, stage-x64 postbuild)"
  - "Miles clientAudio subsystem disabled under #if _M_X64 (ZERO AIL_* symbols in the x64 objs; boots silent); Bink -> binkw64.dll on x64"
  - "DllExport x64 port (the plugin<->host engine-symbol bridge): __asm int 3 -> __debugbreak; Debug|x64/Release|x64 configs"
  - "swg.sln promotes the 4 plugin/exe GUIDs to .Build.0 on Debug|x64 + Release|x64 (+ DllExport x64 mapping)"
affects: [33-06]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "plugin x64 link block: drop d3dx9.lib, MachineX64, libjpeg->jpeg62-x64.lib, d3d9/d3dcompiler/ddraw from the Windows SDK x64, drop the DXSDK directx9\\lib + dsetup/DxErr9 (x86-only), stage-x64\\ postbuild + Sysnative d3dcompiler_47.dll"
    - "subsystem-disable via root early-return + x64 no-op AIL_* macros = zero external symbols for an x86-only static lib (Mss32) on x64, proven by dumpbin /symbols | grep == 0"
    - "x86 inline-asm export-stub DLL ported to x64 by mapping `__asm int 3` -> `__debugbreak()` (the never-called trap)"

key-files:
  created: []
  modified:
    - src/engine/client/library/clientAudio/src/win32/Audio.cpp
    - src/engine/client/library/clientAudio/src/win32/SoundObject3d.cpp
    - src/engine/client/library/clientGraphics/src/Bink/BinkVideo.cpp
    - src/engine/client/application/Direct3d9/build/win32/Direct3d9.vcxproj
    - src/engine/client/application/Direct3d9/build/win32/Direct3d9_ffp.vcxproj
    - src/engine/client/application/Direct3d9/build/win32/Direct3d9_vsps.vcxproj
    - src/engine/client/application/Direct3d9/src/win32/Direct3d9.cpp
    - src/engine/client/application/Direct3d9/src/win32/Direct3d9.h
    - src/engine/client/application/Direct3d9/src/win32/Direct3d9_LightManager.cpp
    - src/engine/client/application/DllExport/src/win32/DllExport.cpp
    - src/engine/client/application/DllExport/build/win32/DllExport.vcxproj
    - src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj
    - src/build/win32/swg.sln

key-decisions:
  - "Miles subsystem-disable via x64 no-op AIL_* macros + Audio::install() root early-return (NOT a Restoration mss64 lift): install() returns false before any Miles state is created so s_installed stays false and every consumer (gated on s_installed) no-ops -> no null/garbage HSAMPLE/HSTREAM/HDIGDRIVER escapes; the macros only let the now-unreachable Miles code compile to ZERO external AIL_* symbols (dumpbin proven == 0)"
  - "DllExport (the Win32-only x86-asm export-stub DLL that bridges plugin<->host engine symbols) HAD to be ported to x64, not excluded: the plugins resolve their engine dllimport refs against DllExport.lib at link + DelayLoad it at runtime. Port = __asm int 3 -> __debugbreak() (never-called trap) + add x64 configs. Restoration's x64 gl05 imports DllExport.dll too, confirming the model"
  - "dxerr9 (DXGetErrorString9) + dsetup + DxErr9.lib are legacy DXSDK x86-only with no x64 lib -> dropped on x64; FATAL_DX_HR falls back to a raw-HRESULT string on x64 (still loud)"
  - "The Windows SDK supplies x64 d3d9/d3dcompiler/ddraw/dinput8/dxguid/dsound -> the DXSDK directx9\\lib dir is off the x64 lib path"

requirements-completed: [X64-01, X64-02, X64-04]

# Metrics
duration: ~140min
completed: 2026-06-17
---

# Phase 33 Plan 05: First Full x64 Link Summary

**The integration gate is cleared: the first fully-linking x64 client exists. gl05/06/07 + SwgClient all link as machine-x64 binaries (dumpbin 8664) with the x64 link log grepping `unresolved external symbol` == 0 (the /FORCE trap cleared) — D3DX dropped (Plans 02/03/Ph32), the x64 lib tier wired (Plan 04), the LIFT import libs + tinyxml swapped in (Plan 01), Miles disabled to ZERO AIL_* symbols (boots silent, Phase 35 owns the real port), Bink on binkw64, and the DllExport plugin<->host symbol bridge ported to x64. Win32 stage/ binaries are provably untouched (machine 14C).**

## Performance
- **Duration:** ~140 min
- **Tasks:** 3
- **Files modified:** 13

## Accomplishments

### Task 1 — Miles subsystem disabled on x64 (symbol-level proof) + Bink swap (47665f7e1)
- **Miles disabled at the root:** `Audio::install()` early-returns `false` under `#if _M_X64` BEFORE any Miles state is created (`s_digitalDevice2d` stays NULL, `s_installed` stays false). Every public play/alter/query/room-type entry point is gated on `if (s_installed)`, so all become no-op shells — NO null/garbage HSAMPLE/HSTREAM/HDIGDRIVER handle reaches any consumer (this is the reviews-fix-#3 subsystem-disable, not per-call return-success).
- **Zero AIL_* symbols:** x64 no-op `AIL_*` macros (covering the ~75 functions called outside `install()`) let the now-unreachable Miles code compile WITHOUT emitting external symbols. **PROVEN: `dumpbin /symbols <every clientAudio x64 .obj> | grep AIL_` == 0** (including Audio.obj + SoundObject3d.obj) — so the x86-only Mss32.lib is not needed at link.
- **SoundObject3d::alter()** AIL_set_listener_3D_* gated under `!_M_X64`. **BinkVideo** `s_dllName` -> `binkw64.dll` under `#if _M_X64` (dynamic load, no import lib). The 32-bit (`!_M_X64`/`#else`) audio + Bink paths are byte-unchanged. A `Phase 35` marker records the deferred real port (D:\Code\milesss-v9.3b\win\sdk\).

### Task 2 — gl05/06/07 x64 link blocks + DllExport x64 port (9531ba73b)
- **3 plugin .vcxprojs:** Debug|x64 + Release|x64 ItemDefinitionGroups (import x64-platform.props, isolated `\x64\` OutDir, FirstDirect3d9 PCH `Create` on x64), x64 Link blocks: **d3dx9.lib DROPPED**, `MachineX64`, libjpeg->`jpeg62-x64.lib`, d3d9/d3dcompiler/ddraw from the Windows SDK x64, no x86 BaseAddress, PostBuild -> `stage-x64\` + the x64 System32 `d3dcompiler_47.dll` (Sysnative). gl06 = FFP-only (no d3dcompiler); gl05/gl07 carry d3dcompiler.
- **DllExport x64 port (Rule 3 blocking dep):** DllExport is the plugin<->host engine-symbol bridge — an export-stub DLL whose 91 bodies are `__asm int 3` traps (never called; the real engine binds from SwgClient.exe at runtime). Ported the trap to a portable `DLLEXPORT_UNREACHABLE_TRAP` (`__debugbreak()` on x64, byte-identical `__asm int 3` on x86) and added Debug|x64/Release|x64 DynamicLibrary configs, so the plugins resolve their engine `dllimport` refs against `DllExport.lib`.
- **BITS-02 x64 width fixes (Rule 1):** `FIELD_OFFSET` cast through `size_t` (Direct3d9_LightManager.cpp, 5 sites); `pBits` pointer arithmetic via `size_t` (Direct3d9.cpp:2753); dxerr9 (no x64 lib) gated out + an x64 HRESULT fallback for `FATAL_DX_HR` (Direct3d9.h).
- **All 3 plugins link x64:** 0 unresolved, machine 8664. Win32 blocks additive-only.

### Task 3 — SwgClient x64 link block + the FIRST FULL x64 LINK (e2ae460ef)
- **SwgClient.vcxproj:** Debug|x64 + Release|x64 Application configs. x64 Link drops `d3dx/d3dx9/d3dx9dt` + `binkw32.lib` + `Mss32/mss32` + `dsetup.lib` + `DxErr9.lib` (DXSDK x86-only); swaps the LIFT deps (`libxml2-x64`/`pcre-x64`/`jpeg62-x64`); keeps tinyxml as `tinyxmld_STL.lib` + `tinyxmld.lib` (reviews fix #5) from the tinyxml x64 OutDir; repoints EVERY engine/3rd lib dir to `compile/win32/<lib>/x64/<cfg>`; drops the DXSDK `directx9\lib` (SDK supplies x64 d3d9/d3dcompiler/ddraw); `MachineX64`; removes `_USE_32BIT_TIME_T` on x64 (UCRT C1189 with _WIN64); retains the `/FORCE` flags; PostBuild -> `stage-x64\` (no Miles redist).
- **swg.sln (reviews fix #9):** promoted the 4 ActiveCfg-only x64 GUIDs (Direct3d9/_ffp/_vsps + SwgClient) to `.Debug|x64.Build.0` / `.Release|x64.Build.0` (+ added the DllExport x64 mapping). BOM + CRLF byte-preserved.
- **THE FIRST FULL x64 LINK:** `swg.sln /t:Direct3d9;Direct3d9_ffp;Direct3d9_vsps;SwgClient /p:Configuration=Debug /p:Platform=x64` -> **Build succeeded; `grep -c "unresolved external symbol" build-33-05-x64.log` == 0** (the /FORCE trap cleared — ForceFileOutput + /SAFESEH:NO + /VERBOSE active, so the 0-grep is the meaningful acceptance, not exit code), `LNK1181` == 0. SwgClient_d.exe (53.4 MB) + gl05/06/07_d.dll all `dumpbin /headers` machine **8664 (x64)**, staged to `stage-x64\`.

## The /FORCE link result
`unresolved external symbol == 0`. The first x64 link came up clean on the FIRST attempt once the chased issues below were resolved — no residual D3DX symbol, no missing LIFT lib, no AIL_* the Miles disable missed. The symbols that had to be chased were all toolchain/DXSDK-x64-availability, not engine-code defects.

## Symbols / libs chased to reach 0 unresolved
1. **DllExport (Task 2):** initially excluded on x64 (x86 asm) -> 74 unresolved engine `dllimport` symbols (Clock::frameTime, DebugFlags::registerFlag, ...). Resolved by porting DllExport to x64 (the plugin<->host bridge), not by excluding it.
2. **`DXGetErrorString9` + `DirectDrawCreate` (Task 2):** dxerr9 (x86-only) -> x64 HRESULT fallback; ddraw.lib re-added from the Windows SDK x64.
3. **`dsetup.lib` + `DxErr9.lib` (Task 3):** legacy DXSDK x86-only -> dropped from the SwgClient x64 deps (the directx9\lib dir is off the x64 path).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] DllExport.dll had to be ported to x64 (the plugin<->host engine-symbol bridge)**
- **Found during:** Task 2 (first gl05 x64 link -> 74 unresolved engine dllimport symbols)
- **Issue:** The plan treated the plugin link as "drop d3dx9, swap jpeg" and did not anticipate that the plugins resolve their engine `dllimport` symbols against `DllExport.lib` (DllExport.dll, a Win32-only x86-`__asm` export-stub DLL, is ProjectReferenced + DelayLoaded). Excluding it on x64 left 74 unresolved externals. 33-CONTEXT classified DllExport as "Win32-only, NOT the x64 ship target" but Restoration's x64 gl05 imports DllExport.dll, confirming the model needs it.
- **Fix:** Ported DllExport.cpp's never-called `__asm int 3` traps to a portable `DLLEXPORT_UNREACHABLE_TRAP` (`__debugbreak()` on x64) and added Debug|x64/Release|x64 configs to DllExport.vcxproj. Mechanical port of an unreachable trap, no behavior change.
- **Files:** DllExport.cpp, DllExport.vcxproj
- **Commit:** 9531ba73b

**2. [Rule 1 - Bug] BITS-02 pointer-truncation defects in the plugin source (surfaced by /we4311/4312 on the first x64 compile)**
- **Found during:** Task 2 (gl05 x64 compile)
- **Issue:** The plugin source had never compiled x64; `FIELD_OFFSET` (local macro casting a member pointer to `(int)`) truncated under /we4311 (Direct3d9_LightManager.cpp), and a `(unsigned int)lockedRect.pBits` pointer-arithmetic cast truncated (Direct3d9.cpp:2753).
- **Fix:** cast through `size_t`/pointer-width before narrowing (offset values are small and unchanged on both x86 and x64).
- **Files:** Direct3d9_LightManager.cpp, Direct3d9.cpp
- **Commit:** 9531ba73b

**3. [Rule 3 - Blocking] DXSDK x86-only libs (dxerr9/DxErr9.lib/dsetup.lib) have no x64 lib**
- **Found during:** Task 2/3 (link)
- **Issue:** `DXGetErrorString9` (dxerr9), `DxErr9.lib`, and `dsetup.lib` are legacy DXSDK x86-only; the x64 link had no source for them.
- **Fix:** gate the dxerr9 include + an x64 HRESULT fallback for `FATAL_DX_HR` (Direct3d9.h); add `ddraw.lib` from the Windows SDK x64; drop `dsetup.lib`/`DxErr9.lib` from the SwgClient x64 deps.
- **Files:** Direct3d9.h, Direct3d9{,_ffp,_vsps}.vcxproj, SwgClient.vcxproj
- **Commit:** 9531ba73b / e2ae460ef

**4. [Rule 1 - Bug] `_USE_32BIT_TIME_T` is x64-illegal (UCRT C1189 with _WIN64)**
- **Found during:** Task 3 (first SwgClient x64 compile)
- **Issue:** The cloned-from-Win32 SwgClient x64 ClCompile carried `_USE_32BIT_TIME_T=1`, which the UCRT corecrt.h hard-errors on under `_WIN64`.
- **Fix:** stripped the flag from the two x64 ClCompile blocks only (Win32 blocks untouched). x64-platform.props already omits it (A2-TIME-T).
- **Files:** SwgClient.vcxproj
- **Commit:** e2ae460ef

**Total deviations:** 4 auto-fixed (2 Rule 3 blocking toolchain/dependency, 2 Rule 1 width/flag bugs). None architectural; none changed Win32 behavior or on-the-wire data. The DllExport x64 port is the substantive one (a plan gap the plan did not foresee).

## Threat surface scan
No new network/auth/crypto/file trust-boundary surface. binkw64.dll loads by bare name into the controlled stage-x64/ dir (T-33-11 accept, unchanged from Win32). The Miles disable removes, not adds, a code path on x64 (T-33-12 accept). No threat flags.

## Notes for Plan 06 (stage + boot validation)
- The first x64 client is in `stage-x64/`: SwgClient_d.exe + gl05/06/07_d.dll (all machine 8664) + d3dcompiler_47.dll. Plan 06 stages binkw64.dll + boot-validates under rasterMajor 5/6/7.
- **Audio is silent by design on x64** (Miles disabled) — boot to char-select must tolerate no audio. **The real Miles 9.3b port is Phase 35** (SDK at D:\Code\milesss-v9.3b\win\sdk\; NOT a Restoration mss64 lift).
- The x64 build uses `stage-x64/`; the shipped 32-bit `stage/` is untouched (machine 14C) — keep them separate for the 32-bit non-regression baseline.
- The Release|x64 link blocks exist but were NOT exercised this plan (only Debug|x64 was built). Plan 06 (or a later plan) should run the Release|x64 link if a Release x64 client is needed.

## User Setup Required
None.

## Self-Check: PASSED

- SUMMARY.md present on disk — verified.
- All 4 x64 binaries present (SwgClient_d.exe + gl05/06/07_d.dll), dumpbin machine 8664 — verified.
- All 3 task commits present in git: 47665f7e1, 9531ba73b, e2ae460ef — verified.
- First x64 link log greps `unresolved external symbol` == 0 — verified.

---
*Phase: 33-x64-build-platform-d3d9-renderers*
*Completed: 2026-06-17*
