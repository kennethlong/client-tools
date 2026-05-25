# Deep-dive: `SwgClient.vcproj`

> What does the SwgClient build, what does it depend on, and what would
> actually block a build today?

## Source layout

`src/game/client/application/SwgClient/`

```
build/
  win32/
    SwgClient.vcproj            ← Visual Studio 2005 project, format 8.00
    settings.rsp                ← compiler flag flags
    includePaths.rsp            ← include directories
    libraryPaths.rsp            ← linker library directories
    libraries.rsp               ← always-linked libs
    libraries_d.rsp             ← Debug-only libs
    libraries_o.rsp             ← Optimized-only libs
    libraries_r.rsp             ← Release-only libs
    ignoreLibraries.rsp         ← libs to exclude (always)
    ignoreLibraries_d.rsp       ← Debug ignore list
    ignoreLibraries_o.rsp       ← Optimized ignore list
src/
  shared/
    FirstSwgClient.h            ← precompiled-header umbrella
  win32/
    WinMain.cpp                 ← Win32 entry point (~135 lines)
    ClientMain.cpp              ← engine bootstrap (~405 lines)
    ClientMain.h
    FirstSwgClient.cpp          ← precompiled-header source
    SwgClient.rc                ← Windows resources
    icon1.ico, icon2.ico
    resource.h
```

The `.vcproj` itself compiles only **three C++ files** and one `.rc` —
`WinMain.cpp`, `ClientMain.cpp`, `FirstSwgClient.cpp`. Everything else is
inherited transitively through `swg.sln` project dependencies.

## What it builds — three configurations

`SwgClient.vcproj` declares three configurations under `<Configurations>`:

| Configuration | Output binary | Optimisation | Defines | RuntimeLibrary | Notes |
| --- | --- | --- | --- | --- | --- |
| `Debug\|Win32` | `SwgClient_d.exe` | `Optimization="0"` (`/Od`) | `WIN32;_DEBUG;_MBCS;DEBUG_LEVEL=2;_CRT_SECURE_NO_DEPRECATE=1;_USE_32BIT_TIME_T=1;_WINDOWS` | `1` (MTd-DLL, `/MDd`) | `BasicRuntimeChecks="3"`, `MinimalRebuild="TRUE"` |
| `Optimized\|Win32` | `SwgClient_o.exe` | `Optimization="3"` (`/Ox` full opt) | `WIN32;_DEBUG;_MBCS;DEBUG_LEVEL=1;_CRT_SECURE_NO_DEPRECATE=1;_USE_32BIT_TIME_T=1;_WINDOWS` | `1` (MTd-DLL) | `EnableIntrinsicFunctions="TRUE"`, `FavorSizeOrSpeed="1"` (favour speed), `OmitFramePointers="FALSE"` |
| `Release\|Win32` | `SwgClient_r.exe` | `Optimization="2"` (`/O2`) | `WIN32;NDEBUG;_MBCS;DEBUG_LEVEL=0;_CRT_SECURE_NO_DEPRECATE=1;_USE_32BIT_TIME_T=1;_WINDOWS` | `0` (MT static, `/MT`) | `StringPooling="TRUE"`, `OmitFramePointers="TRUE"` |

All three are 32-bit Win32 (`<Platform Name="Win32"/>`), `SubSystem="2"`
(Windows GUI, not console).

The two prebuilt artefacts that ship in the repo are
`exe/win32/SwgClient_o.exe` and `exe/win32/SwgClient_r.exe`. There's no
`SwgClient_d.exe` shipped — Debug was dev-only.

Output directories (per configuration):

```
compile/win32/SwgClient/<config>/SwgClient_<x>.exe
compile/win32/SwgClient/<config>/SwgClient_<x>.pdb
```

## Compiler settings worth noting

From `SwgClient.vcproj` and `settings.rsp`:

- `CharacterSet="2"` — **MBCS, not Unicode**. The whole codebase uses ANSI
  Win32 APIs.
- `TreatWChar_tAsBuiltInType="FALSE"` — `wchar_t` is treated as
  `unsigned short`. This is critical: it's done so the Mozilla XPCOM libraries
  link correctly. Modern VS defaults to TRUE, which would break the link
  against `xpcom.lib` etc.
- `WarningLevel="4"` and `WarnAsError="TRUE"` — strict compile gate. This
  is a huge porting hazard on modern compilers, which add new warnings every
  release.
- `UsePrecompiledHeader="3"` (`/Yc/Yu`) through `FirstSwgClient.h`.
  `FirstSwgClient.h` itself is a single file:
  ```cpp
  #ifndef INCLUDED_FirstSwgClient_H
  #define INCLUDED_FirstSwgClient_H
  #include "sharedFoundation/FirstSharedFoundation.h"
  #include "_precompile.h"
  #endif
  ```
- `RuntimeTypeInfo="TRUE"` — RTTI enabled (the codebase relies on `dynamic_cast`).
- `ForceConformanceInForLoopScope="TRUE"` — modern for-loop scope.
- `Detect64BitPortabilityProblems="FALSE"` — they explicitly turned this off.
  The codebase never targets 64-bit on the client.
- `_USE_32BIT_TIME_T=1` — pinned 32-bit `time_t` for compatibility with
  STLPort 4.5.3.
- `settings.rsp`:
  ```
  windows
  noPchDirectory
  incremental_o_no
  dbgInfo_r_pdb
  debugInline
  ```
  These flags drive whatever build wrapper the original SOE pipeline used.

## Source files actually compiled

```
src/win32/WinMain.cpp          — 135 lines
src/win32/ClientMain.cpp       — 405 lines
src/win32/FirstSwgClient.cpp   — pch source
src/win32/SwgClient.rc         — Windows resources (icons, version block)
```

Plus the precompiled-header umbrella `src/shared/FirstSwgClient.h`.

That's it. **No engine code is in this `.vcproj`.** All of it comes in via
sibling `.vcproj` projects linked through `swg.sln`.

### `WinMain.cpp` — the entry shim

`src/game/client/application/SwgClient/src/win32/WinMain.cpp:124`

```cpp
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
  if (!SetUserSelectedMemoryManagerTarget())
    SetDefaultMemoryManagerTargetSize();
  return ClientMain(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
}
```

`SetUserSelectedMemoryManagerTarget` reads the env var
`SWGCLIENT_MEMORY_SIZE_MB`. The default size is 3/4 of total physical RAM,
clamped to `[250 MB, 750 MB]` in dev or `[250 MB, 2000 MB]` in production.

### `ClientMain.cpp` — the boot order

`src/game/client/application/SwgClient/src/win32/ClientMain.cpp` calls (in
order) the install/setup functions of every engine subsystem:

1. `SetupSharedThread::install()`
2. `SetupSharedDebug::install(4096)`
3. `SetupSharedFoundation::install(...)` with config file `client.cfg`,
   window name, default frame rate 60, mini-dump writer.
4. `SetupSharedCompression::install(...)` (zlib threads)
5. `SetupSharedRegex::install()`
6. `SetupSharedFile::install(true, skuBits)` — the TRE search path.
   `skuBits` is computed from `Game::getGameFeatureBits()` against
   `ClientGameFeature::Base / SpaceExpansionRetail / Episode3ExpansionRetail /
   TrialsOfObiwanRetail`.
7. `installConfigFileOverride()` — reads `misc/override.cfg` from a TRE
   archive if it exists.
8. `SetupSharedMath`, `SetupSharedUtility`, `SetupSharedRandom`,
   `SetupSharedLog("SwgClient")`, `SetupSharedImage`, `SetupSharedNetwork`
   (default client setup), `SetupSharedNetworkMessages`,
   `SetupSwgSharedNetworkMessages`.
9. `SetupSharedObject` — slot id manager + customisation + movement table data.
10. `SetupSharedGame` — game scheduler, mount-valid scale ranges, debug-bad-strings hook.
11. `CommoditiesAdvancedSearchAttribute::install()`,
    `SwgCuiAuctionFilter::buildAttributeFilterDisplayString()`.
12. `SetupSharedTerrain` (game data).
13. `SetupSharedXml`, `SetupSharedPathfinding`.
14. `SetupClientAudio::install()` — Miles boot.
15. `SetupClientGraphics::install(...)` with screen 1024×768. **Mozilla is
    initialised here**:
    ```cpp
    sPath += "\\mozilla";
    libMozilla::init(Os::getWindow(), sPath.c_str());
    ```
16. `VideoList::install(Audio::getMilesDigitalDriver())` — Bink uses Miles
    for its audio side.
17. `SetupClientDirectInput::install(hInstance, Os::getWindow(), DIK_LCONTROL,
    Graphics::isWindowed)`.
18. `SetupClientObject`, `SetupClientAnimation`,
    `SetupClientSkeletalAnimation`, `SetupClientTextureRenderer`,
    `SetupClientTerrain`, `SetupClientParticle`.
19. `SetupClientGame` — pulls in clientGame.
20. `CuiManager::setImplementationInstallFunctions(SwgCuiManager::install,
    remove, update)`.
21. `SetupClientBugReporting`, `SetupSharedIoWin`,
    `SetupSwgClientUserInterface`, `SwgCuiG15Lcd::initializeLcd()`.
22. **Game loop**:
    `SetupSharedFoundation::callbackWithExceptionHandling(Game::run);`
23. After Game::run returns: `CuiWorkspace::saveAllSettings()`,
    `CuiSettings::save()`, `CuiChatHistory::save()`,
    `CurrentUserOptionManager::save()`, `LocalMachineOptionManager::save()`.
24. NPE conversion path: if the player is on a trial/rental subscription,
    triggers `Game::externalCommand("npe_continue")` to launch a web URL.
25. Teardown: `SetupSharedFoundation::remove()`,
    `SetupSharedThread::remove()`, `libMozilla::release()`,
    `CloseHandle(semaphore)`.

A single-instance semaphore (`"SwgClientInstanceRunning"`) blocks a second
launch unless `[SwgClient] allowMultipleInstances=1` is set (always on in
non-PRODUCTION builds).

### `installConfigFileOverride()` (pattern worth noting)

```cpp
AbstractFile * const abstractFile = TreeFile::open("misc/override.cfg",
    AbstractFile::PriorityData, true);
if (abstractFile) {
  const int length = abstractFile->length();
  byte * const data = abstractFile->readEntireFileAndClose();
  IGNORE_RETURN(ConfigFile::loadFromBuffer(reinterpret_cast<char const *>(data), length));
  delete[] data;
  delete abstractFile;
}
```

The client looks for `misc/override.cfg` *inside* the TRE search path — i.e.
servers can ship config overrides via the patcher.

## Internal SOE library dependencies

Resolved via `swg.sln`'s GUID `<ProjectDependencies>` block (the SwgClient
solution lists ~70 dependent projects). The headers reach into:

**Engine (12 client + 22 shared):**
```
clientAnimation, clientAudio, clientBugReporting, clientDirectInput,
clientGame, clientGraphics, clientObject, clientParticle,
clientSkeletalAnimation, clientTerrain, clientTextureRenderer,
clientUserInterface

sharedCompression, sharedDebug, sharedFile, sharedFoundation,
sharedFoundationTypes, sharedGame, sharedImage, sharedIoWin, sharedLog,
sharedMath, sharedMemoryManager, sharedMessageDispatch, sharedNetwork,
sharedNetworkMessages, sharedObject, sharedPathfinding, sharedRandom,
sharedRegex, sharedTerrain, sharedThread, sharedUtility, sharedXml
```

**Game-side (2):**
```
swgClientUserInterface, swgSharedNetworkMessages
```

**SOE in-house "ours" libs (6):**
```
archive, fileInterface, localization, localizationArchive, unicode, unicodeArchive
```

Notable scale references (from filesystem counts):
- `clientGame`: 343 .cpp / 683 .h
- `clientGraphics`: 68 .cpp / 144 .h
- `clientAudio`: 20 .cpp / 40 .h
- `sharedNetwork`: 30 .cpp / 43 .h
- `sharedFoundation`: 52 .cpp / 146 .h

None of these projects appear in `SwgClient.vcproj`'s `<Files>` list; they
must be linked via `swg.sln` dependencies.

## External libraries linked directly

From `libraries.rsp` (always linked):
```
ws2_32.lib       — Windows Sockets
winmm.lib        — Windows multimedia
dinput8.lib      — DirectInput 8
dsound.lib       — DirectSound
dxguid.lib       — DirectX GUIDs
libpcre.a        — Perl-Compatible Regex (static)
libxml2-win32-release.lib
mss32.lib        — Miles Sound System
mswsock.lib      — Microsoft Sockets
lgLcd.lib        — Logitech LCD UI
nspr4.lib        — Mozilla NSPR
plc4.lib         — Mozilla PLC
profdirserviceprovider_s.lib  — Mozilla profile dir
xpcom.lib        — Mozilla XPCOM
xul.lib          — Mozilla XUL
```

From `libraries_d.rsp` (Debug-only):
```
dpvsd.lib                       — DPVS occlusion (debug)
vivoxSharedWrapper_debug.lib
VideoCapture_debug.lib
ImageCapture_debug.lib
Smart_debug.lib
SoeUtil_debug.lib
ZlibUtil_debug.lib
picn20md.lib                    — PICTools (debug)
```

From `libraries_o.rsp` (Optimized):
```
dpvs.lib
vivoxSharedWrapper_debug.lib    ← note: still pulls debug Vivox in opt builds
VideoCapture_release.lib
ImageCapture_release.lib
Smart_release.lib
SoeUtil_release.lib
ZlibUtil_release.lib
picn20m.lib
```

From `libraries_r.rsp` (Release):
```
dpvs.lib
vivoxSharedWrapper_release.lib
VideoCapture_release.lib
ImageCapture_release.lib
Smart_release.lib
SoeUtil_release.lib
ZlibUtil_release.lib
picn20m.lib
```

From `ignoreLibraries.rsp` (always):
```
libc
MSVCRT
```

From `ignoreLibraries_d.rsp` and `ignoreLibraries_o.rsp`:
```
LIBCMT
MSVCRTD
```

The Release linker `IgnoreDefaultLibraryNames="libc,MSVCRT"` (only the static
flags) since it's an `/MT` static-CRT build.

### Existence verification

Every linked `.lib` was confirmed on disk during the investigation. Summary:

| Library directory | Files present |
| --- | --- |
| `external/3rd/library/directx9/lib` | `d3d9.lib`, `d3dx9.lib`, `d3dx9d.dll`, `d3dx9dt.lib`, `ddraw.lib`, `dinput8.lib`, `dsetup.lib`, `dsound.lib`, `DxErr9.lib`, `dxguid.lib` (plus `d3dx.lib`) |
| `external/3rd/library/dpvs/lib/win32-x86` | `dpvs.lib`, `dpvsd.lib`, `dpvsd.pdb` |
| `external/3rd/library/libxml2-2.6.7.win32/lib` | `libxml2-win32-debug.lib`, `libxml2-win32-release.lib` |
| `external/3rd/library/miles/lib/win` | `Mss32.lib` |
| `external/3rd/library/pcre/4.1/win32/lib` | `libpcre.a` |
| `external/3rd/library/stlport453/lib/win32` | 12 `.lib` variants — vc6/vc7/vc71 × release/debug × static/stldebug. **Not VS-2005 (vc8).** |
| `external/3rd/library/zlib/lib/win32` | `zlib.lib` |
| `external/3rd/library/lcdui/lib` | `lgLcd.lib` |
| `external/3rd/library/libMozilla/include/private/lib/release` | `nspr4.lib`, `plc4.lib`, `profdirserviceprovider_s.lib`, `xpcom.lib`, `xul.lib` |
| `external/3rd/library/vivoxSharedWrapper/lib` | `vivoxSharedWrapper_Debug.lib`, `vivoxSharedWrapper_Release.lib` |
| `external/3rd/library/videocapture/AudioCapture/lib/win32` | `AudioCapture.lib` |
| `external/3rd/library/videocapture/VideoCapture/lib/win32` | `VideoCapture_debug.lib`, `VideoCapture_release.lib` |
| `external/3rd/library/videocapture/ImageCapture/lib/win32` | `ImageCapture_debug.lib`, `ImageCapture_release.lib` |
| `external/3rd/library/videocapture/Smart/lib/win32` | `Smart_debug.lib`, `Smart_release.lib` |
| `external/3rd/library/videocapture/ZlibUtil/lib/win32` | `ZlibUtil_debug.lib`, `ZlibUtil_release.lib` |
| `external/3rd/library/videocapture/SoeUtil/lib/win32` | `SoeUtil_debug.lib`, `SoeUtil_release.lib` |
| `external/3rd/library/videocapture/PICTools/Lib/win32` | `picn20m.lib`, `picn20md.lib` |

## Include paths (header-only / project-built)

From `includePaths.rsp`:

Engine includes (path roots):
```
clientAnimation/include/public, clientAudio/.../public,
clientBugReporting, clientDirectInput, clientGame, clientGraphics,
clientObject, clientParticle, clientSkeletalAnimation, clientTerrain,
clientTextureRenderer, clientUserInterface

sharedCompression, sharedDebug, sharedFile, sharedFoundation,
sharedFoundationTypes, sharedGame, sharedImage, sharedIoWin, sharedLog,
sharedMath, sharedMemoryManager, sharedMessageDispatch, sharedNetwork,
sharedNetworkMessages, sharedObject, sharedPathfinding, sharedRandom,
sharedRegex, sharedTerrain, sharedThread, sharedUtility, sharedXml

(game side)
swgClientUserInterface, swgSharedNetworkMessages

(SOE in-house)
external/ours/library/archive, fileInterface, localization,
localizationArchive, unicode, unicodeArchive
```

External include paths (third-party):
```
external/3rd/library/boost
external/3rd/library/directx9/include
external/3rd/library/libMozilla/include/public
external/3rd/library/ui/include
external/3rd/library/stlport453/stlport
```

Project-local:
```
src/shared
src/win32
```

### `boost` is tiny

`src/external/3rd/library/boost/boost/` contains exactly **four files**:
```
config.hpp      — 27,605 bytes
smart_ptr.hpp   — 15,905 bytes
static_assert.hpp — 3,583 bytes
utility.hpp     —  3,999 bytes
```

The codebase never used full Boost — they cherry-picked. Mostly
`boost::shared_ptr`/`boost::scoped_ptr` and a few utility macros.

### `ui` is in-house

`external/3rd/library/ui/include` — despite the path, this is SOE's
**custom UI engine** (281 headers, 126 .cpp). Built by its own sibling
project. Not third-party.

### `libMozilla` public header is a redirect

`external/3rd/library/libMozilla/include/public/libMozilla/libMozilla.h` is
41 bytes:
```cpp
#include "../../src/win32/libMozilla.h"
```

The actual implementation is in the same tree under `src/win32/` and is built
by a sibling `libMozilla.vcproj`.

## Indirect / runtime-loaded dependencies

The `.vcproj` does not link these — they're loaded at runtime via
`LoadLibrary` (or are required by the linked DLLs themselves). All ship as
DLLs in `exe/win32/`:

- **Bink** (`bink/lib/win32/binkw32.dll`, `binkw32.lib`)
  - Loaded via `Bink::bindBink()` in `BinkDLL.h` — function pointers, not
    static link.
  - Headers: `bink.h`, `rad3d.h`, `radbase.h` (3 files in
    `external/3rd/library/bink/include/`).
- **Vivox runtime** (`external/3rd/library/vivox/lib/`)
  - DLLs: `vivoxsdk.dll`, `vivoxplatform.dll`, `ortp.dll`, `alut.dll`,
    `libsndfile-1.dll`, `wrap_oal.dll`, `VivoxVoiceService.exe`.
  - Pulled in by `vivoxSharedWrapper`.
- **Mozilla XPCOM runtime** (`exe/win32/`):
  ```
  xpcom.dll, xul.dll, nspr4.dll, plc4.dll, plds4.dll, nss3.dll,
  nssckbi.dll, smime3.dll, softokn3.dll, ssl3.dll, js3250.dll,
  gksvggdiplus.dll
  ```
  Plus `exe/win32/mozilla/` chrome tree.
- **MSVC 7.1 CRT** (`exe/win32/msvcr71.dll`)
  - This is a notable mismatch. The `.vcproj` is VS 2005 (which would emit
    binaries linked against `msvcr80.dll`), but the prebuilt EXEs in
    `exe/win32/` use `msvcr71.dll`, the VS 2003 (.NET 2003) CRT. The original
    team built with VC++ 7.1; the VS 2005 settings are a later port.

## What's missing — what would actually block a build today

| # | Blocker | Why |
| --- | --- | --- |
| 1 | **Visual Studio 2005** itself, or a clean upgrade path | Project format `Version="8.00"` is VS 2005-specific. VS 2022 will offer to upgrade; the upgrade reliably succeeds for the wrapper but fails on sister libs (changed default flags, deprecated CRT, removed `iostream.h`-style headers, MFC/ATL relocation). |
| 2 | **Pre-2010 Windows Platform SDK** that pairs with the bundled DX9 SDK | The DX9 headers reference `wtypes.h`/`unknwn.h` from the era's SDK. Newer SDKs work *most* of the time but break on `dinputd.h` / `ddraw.h`. |
| 3 | **The `swg.sln` project graph must build in order** | `SwgClient.vcproj` itself has no `<References>` or project links inline — it relies on the ~70 GUID dependencies in `swg.sln`. Opening just this vcproj produces a wall of `LNK2019`s. You must use `swg.sln`. |
| 4 | **STLPort 4.5.3 library naming has to match your compiler** | The `lib/` folder has six variants (vc6/vc7/vc71 × release/debug). VS 2005 (vc8) **isn't there**. The original team built with VC++ 7.1 (.NET 2003); the .vcproj's VS-2005 settings are a later port. You'd need to rebuild STLPort 4.5.3 against your toolset, or ship the older toolset. |
| 5 | **`TreatWChar_tAsBuiltInType="FALSE"`** is non-default | If you flip it to TRUE (default in modern VS) you'll get unresolved externals against XPCOM, which was built with `wchar_t = unsigned short`. Either leave it FALSE or rebuild XPCOM, which you don't have source for in this repo. |
| 6 | **`WarnAsError="TRUE"` + level 4 + a 2026-era compiler** | Newer MSVCs add warnings that didn't exist in 2010. Expect dozens of new errors per file in `clientGame` once you open it. Standard fix is global `/wd` suppression — meaningful porting effort. |
| 7 | **Sony Station / soePlatform integration** | This client doesn't link `stationapi` directly, but `clientGame` does (via `sharedNetwork` / login flow). The headers/.libs in `external/3rd/library/{stationapi,soePlatform}` exist (267 headers, 24 libs in soePlatform!) — they will link — but they then phone home at runtime to dead SOE endpoints. |
| 8 | **`client.cfg`** with valid LoginServer | The shipped one in `exe/win32/client.cfg` points at SOE's retired hosts. Replace before launching. |
| 9 | **`.tre` asset archives** | `ClientMain.cpp` calls `SetupSharedFile::install(true, skuBits)` and immediately tries to open `misc/override.cfg` via `TreeFile::open`. With no `.tre` files reachable on the search path, you crash in setup before the window even appears. Not in the repo. |
| 10 | **A working `data/` tree on the search path** | Beyond `.tre`, the engine wants `data/sku.0/...` paths. Same gap as for the server — sourced from a retail install or `SWG-Source/client-assets`. |

## What is NOT actually missing (common misconceptions)

The leak is more complete on the client than on the server:

- DirectX 9 SDK headers + import libs are in-tree.
- DPVS (Umbra occlusion) source + libs are in-tree.
- Bink, Miles, Vivox, Logitech LCD, libxml2, PCRE, zlib — all linkable.
- `libMozilla` (the SOE wrapper around Mozilla XPCOM) is fully present,
  including the XPCOM `.lib` files and DLLs.
- The custom UI engine (`external/3rd/library/ui`) is full source.
- The proprietary SOE UDP library (`udplibrary`) is full source — this is
  the wire protocol, so it has to be there for client/server symmetry.

## TL;DR

`SwgClient.vcproj` is a **3-file Win32 shell** that, through the `swg.sln`
project graph, links the entire SOE engine (~12 client libs, ~22 shared libs,
~6 in-house "ours" libs) plus a stack of vendored 3rd-party SDKs (DirectX9,
DPVS, Miles, Vivox, libMozilla XPCOM, Logitech LCD, PCRE, libxml2, zlib,
Bink-via-DLL, STLPort).

Surprisingly, **almost every external dependency is physically present** in
`src/external/3rd/library/`. The realistic blockers are:

1. The 2005-era toolchain (VS 2005 + matching Platform SDK + MSVC 7.1 CRT
   mismatch with the prebuilt EXEs).
2. The strict warning gates (`WarnAsError`, `wchar_t` ABI).
3. Building `swg.sln` (not just this one project).
4. At runtime: missing `.tre` asset archives, dead SOE auth, no working
   `client.cfg`.
