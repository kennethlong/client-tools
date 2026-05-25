# Build Guide — whitengold SwgClient

## What this guide covers

This is the deep build reference for the whitengold CMake port. For the quickstart
(prerequisites + two-command build), see the [root README](../README.md).

## Architecture overview

SwgClient is the Windows-only client binary for Star Wars Galaxies (NGE era, ~2010).
The build system is a CMake + Visual Studio 2022 port of the original Visual Studio
2005 solution (`src/build/win32/swg.sln`). The original solution declared ~70
dependent `.vcproj` projects; the CMake equivalent is a parallel tree of
`CMakeLists.txt` files authored alongside the existing `.vcproj` files (never
replacing them).

### Dependency graph and tier order

The ~70-project graph is organized into six waves based on dependency depth.
CMake resolves them in order; building any target in a later wave triggers all
earlier-wave targets first.

| Wave | Libs |
|------|------|
| Wave 0 (foundations) | archive, crypto, fileInterface, localization, localizationArchive, singleton, unicode, unicodeArchive (external/ours), udplibrary, sharedFoundationTypes, stlport_vc143_compat shim, libMozilla stub, ui |
| Wave 1 (shared engine) | sharedDebug, sharedThread, sharedSynchronization, sharedMath, sharedMathArchive, sharedFoundation, sharedFile, sharedCompression, sharedRandom, sharedRegex, sharedImage, sharedLog, sharedXml, sharedIoWin, sharedMessageDispatch, sharedMemoryManager, sharedNetwork, sharedNetworkMessages, sharedObject, sharedTerrain, sharedPathfinding, sharedGame, sharedUtility |
| Wave 2 (Tier 6 client) | clientAnimation, clientObject, clientTextureRenderer, clientDirectInput, clientBugReporting, clientParticle |
| Wave 3 (Tier 7 HIGH RISK) | clientGraphics (DX9+DPVS+Bink), clientAudio (Miles+VideoCapture) |
| Wave 4 (Tier 8+9) | clientSkeletalAnimation, clientTerrain, clientUserInterface (Mozilla XPCOM stub) |
| Wave 5 (integrators) | clientGame (343 cpp), libEverQuestTCG, swgSharedNetworkMessages, swgClientUserInterface (266 cpp), SwgClient executable |

## SDK landscape

All third-party SDKs are vendored under `src/external/3rd/library/`. No environment
variables are needed. DXSDK_DIR, MILES_DIR, MOZILLA_DIR are NOT required — every
`Find*.cmake` module in `src/cmake/win32/` defaults its search to the vendored path.

| SDK | Vendored path | Link strategy | Runtime |
|-----|--------------|---------------|---------|
| DirectX 9 | src/external/3rd/library/directx9/{include,lib} | Static import libs | d3d9.dll from Windows (system) |
| Miles Sound System | src/external/3rd/library/miles/lib/win/Mss32.lib | Static import lib | Mss32.dll staged to build/bin/Debug/ |
| DPVS (Umbra occlusion) | src/external/3rd/library/dpvs/lib/win32-x86 | Static lib (dpvs.lib / dpvsd.lib) | dpvs.dll + dpvsd.dll staged |
| Bink Video | src/external/3rd/library/bink/{include,lib/win32} | Import lib; loaded via LoadLibrary | binkw32.dll staged |
| Vivox Voice | src/external/3rd/library/vivoxSharedWrapper/lib | Static lib (debug/release variants) | vivoxsdk.dll + vivoxplatform.dll + ortp.dll + alut.dll + libsndfile-1.dll + wrap_oal.dll staged |
| Mozilla XPCOM | src/external/3rd/library/libMozilla/include | Headers only — NOT linked | xpcom.dll + xul.dll + nspr4.dll + plc4.dll + plds4.dll + nss3.dll + nssckbi.dll + smime3.dll + softokn3.dll + ssl3.dll + js3250.dll + gksvggdiplus.dll staged for chrome; libMozilla::init is a stub returning true |
| STLPort 4.5.3 | src/external/3rd/library/stlport453/{stlport,lib/win32} | Prebuilt vc71 static libs | No runtime DLL (static) |
| libxml2 | src/external/3rd/library/libxml2-2.6.7.win32/lib | Static lib (debug/release variants) | None |
| PCRE | src/external/3rd/library/pcre/4.1/win32/lib/libpcre.a | Static lib | None |
| zlib | src/external/3rd/library/zlib/lib/win32/zlib.lib | Static lib | None |
| Boost | src/external/3rd/library/boost/ | Header-only INTERFACE | None |
| lcdui (Logitech G15) | src/external/3rd/library/lcdui/{include,lib} | Static lib | None; gated by config |
| TrackIR | src/external/3rd/library/trackIR/NPClient.h | Header-only, dead code | None |

## Build output layout

After a successful `cmake --build build --config Debug`, the output directory
`build/bin/Debug/` contains:

```
SwgClient_d.exe           (34 MB — the client binary)
SwgClient_d.pdb           (debug symbols)
Mss32.dll                 (Miles Sound System)
binkw32.dll               (Bink Video codec)
dpvs.dll, dpvsd.dll       (DPVS occlusion — both configs staged)
vivoxsdk.dll              (Vivox voice)
vivoxplatform.dll
ortp.dll, alut.dll, libsndfile-1.dll, wrap_oal.dll
xpcom.dll, xul.dll        (Mozilla 1.x runtime — chrome only, not linked)
nspr4.dll, plc4.dll, plds4.dll
nss3.dll, nssckbi.dll, smime3.dll, softokn3.dll, ssl3.dll, js3250.dll
gksvggdiplus.dll
gl05_r.dll, gl05_o.dll    (DirectX 9 renderer plug-in — Release and Optimized variants)
gl06_r.dll, gl06_o.dll
gl07_r.dll, gl07_o.dll
msvcr71.dll               (MSVC 7.1 CRT — needed by some pre-built third-party libs)
dbghelp_6.3.17.0.dll      (crash dump helper)
mozilla/                  (Mozilla chrome subtree — UI browser resources)
client.cfg                (placeholder config; edit searchPath_NN for TRE assets)
```

The staging is performed by a `POST_BUILD add_custom_command` in
`src/game/client/application/SwgClient/src/CMakeLists.txt`. No `cmake --install`
step is required.

## CRT and static linking

All engine and game targets link against the static CRT: `/MTd` (Debug) and
`/MT` (Release). This is set globally via `CMAKE_MSVC_RUNTIME_LIBRARY` in
`src/CMakeLists.txt`. Verification: `dumpbin /imports SwgClient_d.exe` shows
zero references to `MSVCR*.dll`, `VCRUNTIME*.dll`, or `MSVCRTD.dll`.

## Known build quirks and porting decisions

### STLPort vc71 vs MSVC 2022 ABI mismatch

STLPort 4.5.3 ships only vc6/vc7/vc71 prebuilt variants (no vc8/VS 2005 variant,
and certainly no vc14x/VS 2022 variant). The vc71 prebuilt uses `int` as the
`_Mbstatet` struct; MSVC 2022 uses a different ABI for `std::mbstate_t`. The
`stlport_vc143_compat` minimal stub resolves this: it provides `seekoff`,
`seekpos`, `codecvt::id`, and `/alternatename` exception-bridge symbols.
Location: `build/stlport453/include/` — this directory is staged as part of the
build and must be in the include path before the STLPort headers.

A second shim is `build/stlport453/include/typeinfo.h` — required for MSVC 2022
compatibility with STLPort's typeinfo interop.

### Two minimal C++ source edits (INV-01 upheld with exceptions)

The v1 invariant is "no C++ source edits." Two files required minimal edits that
have zero behavioral change:

1. `src/engine/client/library/clientGame/src/shared/scene/FreeCamera.cpp`
   - Problem: `std::min` call with mixed `double`/`float` arguments — STLPort
     strict template matching rejects this under MSVC 2022.
   - Fix: Added `static_cast<real>(...)` to make the argument types match.
   - Change: One line. Zero behavioral change.

2. `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiAllTargets.cpp`
   - Problem: MSVC C++17 tokenizes `""sv` as a UDL (user-defined literal) suffix.
     STLPort has no `operator""sv` so this triggers a compile error.
   - Fix: Added a single space between the string literal and `sv`.
   - Change: One character (space). Zero behavioral change.

Both edits were logged as planning flags and recorded in `PROJECT.md`.

### Mozilla XPCOM stub

The in-game web browser (Trading Card Game overlay + help pages) requires Mozilla
XPCOM. Modern MSVC cannot safely compile the old XPCOM surface. The community-
standard approach: `src/cmake/stubs/libMozilla_stub.cpp` defines `libMozilla::init`
as a stub returning `true` and `libMozilla::release` as a no-op. The client does
NOT link against `xul.lib`/`xpcom.lib`/`nspr4.lib`/`plc4.lib`. The Mozilla DLLs
are staged for any future chrome requirements but the in-game browser is
non-functional in v1.

### UICanvasInitialization.cpp excluded

`src/engine/shared/library/ui/src/shared/UICanvasInitialization.cpp` is excluded
from the `ui` target. It references a canvas generator interface that does not
exist in the leak tree (DX7-era legacy). Removing it does not affect UI rendering.

### SwgVideoCapture.cpp excluded

`src/engine/client/library/clientAudio/src/win32/SwgVideoCapture.cpp` is excluded.
It references `SoeUtil/StrongTypedef.h` template partial specializations that are
incompatible with the MSVC v143 toolset. The video capture code path is not required
for the v1 build.

## Compile flags

The following flags are set globally in `src/CMakeLists.txt` and apply to every
target:

| Flag | Purpose |
|------|---------|
| `/Zc:wchar_t-` | wchar_t is unsigned short (Mozilla XPCOM ABI compatibility) |
| `_USE_32BIT_TIME_T=1` | Pins 32-bit time_t for STLPort 4.5.3 compatibility |
| `_MBCS` | Multi-byte character set (ANSI Win32 APIs throughout) |
| `DEBUG_LEVEL=2` (Debug) / `DEBUG_LEVEL=0` (Release) | Controls debug assertion verbosity |
| `PRODUCTION=0` (Debug) / `PRODUCTION=1` (Release) | Retail vs dev mode |
| `/wd4244 /wd4996 /wd4018 /wd4351` | Suppress known-benign legacy warnings |
| `/MP` | Parallel compilation (per-project) |
| `/Ob1` | Function-level inlining only |
| `/FC` | Full source path in diagnostics |
| `/MT` (Release) / `/MTd` (Debug) | Static CRT linkage |

`/WX` (warnings-as-errors) is explicitly NOT enabled in v1.

## Runtime asset setup (Phase 6 configuration)

The built exe requires a retail SWG install for game assets (`.tre` archives).
After building, edit `build/bin/Debug/client.cfg`:

```ini
[SharedFile]
searchPath0=C:/SWG
searchPath1=C:/SWG/patch

[ClientGame]
loginServerAddress0=127.0.0.1
loginServerPort0=44453

[Station]
gameFeatures=15

[ClientGame]
freeChaseCameraMaximumDistance=75.0
```

Replace `C:/SWG` with the path to your retail Star Wars Galaxies installation.
`gameFeatures=15` enables all four SKU bits (base game + Jump to Lightspeed +
Rage of the Wookiees + Trials of Obi-Wan). `loginServerAddress0` points to the
local SWG-Source VM.

Full Phase 6 configuration (boot sequence, login handshake, character select) is
covered in the Phase 6 planning documents.

Note: `exe/linux/loginServer.cfg` in the repo contains historical SOE DSNs and
hostnames. These are not live credentials and are not referenced by the CMake
build. They are preserved as reference material only.

## Reference links

- SWG-Source umbrella: https://github.com/SWG-Source
- StellaBellum VM setup: https://github.com/SWG-Source/swg-main/wiki/StellaBellum-Development-VM-Setup
- Boot sequence (17 phases): docs/recon/05-client-boot-sequence.md
- Phase 6 pre-flight findings: docs/recon/08-phase6-preflight.md
- swg-main diff findings: docs/recon/07-swg-source-diff-findings.md

---
