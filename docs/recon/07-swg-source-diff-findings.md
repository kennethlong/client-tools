# 07 — SWG-Source vs whitengold diff findings

> Findings from comparing `C:\Code\swg-main\src` (community fork, last
> commit 2020-08-26) against `C:\Code\whitengold\src` (Feb 2015 leak),
> after submodules finished cloning.

## TL;DR

**SWG-Source `src` is a server-only fork.** They have stripped every
client-side directory. There is **no community client porting work to
adopt**. Anything we want to do client-side has to be ported from
whitengold without crib notes.

Server-side, however, they've done several years of useful work:
migrated the build to CMake, validated the codebase compiles on
modern clang/gcc, dropped STLPort, swapped vendored libs for system
`find_package` calls. That's transferable knowledge even though the
code itself isn't.

## File-count comparison

| Tree | Files |
| --- | --- |
| `whitengold\src` | 25,024 |
| `swg-main\src` | 7,913 |

swg-main's src is **~32% the size** of whitengold. Almost all of the
deleted content is the client.

## What's missing in swg-main `src`

Confirmed-absent paths (every one verified by `Test-Path`):

| Path | What it is |
| --- | --- |
| `src\game\client\` | (entire subtree) — SwgClient, SwgGodClient, SwgClientSetup, SwgCsTool, LaunchMeFirst, SwgHeadlessClient, swgClientUserInterface, swgClientQtWidgets, swgClientAutomation, swgSharedNetworkMessages |
| `src\engine\client\library\` | (entire subtree) — clientGame (343 .cpp), clientGraphics, clientAudio, clientAnimation, clientSkeletalAnimation, clientObject, clientTerrain, clientParticle, clientUserInterface, clientBugReporting, clientDirectInput, clientTextureRenderer |
| `src\engine\client\application\Direct3d9\` | DX9 plug-in DLL that wraps DirectX 9 |
| `src\external\3rd\library\directx9\` | DirectX 9 SDK headers/libs |
| `src\external\3rd\library\miles\` | Miles audio |
| `src\external\3rd\library\bink\` | Bink video |
| `src\external\3rd\library\vivox\` | Vivox voice SDK |
| `src\external\3rd\library\libMozilla\` | Mozilla XPCOM wrapper |
| `src\external\3rd\library\dpvs\` | Umbra DPVS occlusion |
| `src\external\3rd\library\stlport453\` | STLPort STL replacement |
| `src\external\3rd\library\lcdui*\` | Logitech LCD |
| `src\external\3rd\library\videocapture\` | Pegasus PICTools / SOE capture stack |
| `src\external\3rd\library\trackIR\` | TrackIR SDK |
| `src\external\3rd\library\stationapi\` | extracted to top-level `swg-main\stationapi` submodule |

Also retired from the server side:

| Path | Note |
| --- | --- |
| `src\external\3rd\library\maya*\` | Maya SDKs not needed at server runtime |
| `src\external\3rd\library\alienbrain*\` | Asset-management SDK |
| `src\external\3rd\library\boost\` | Replaced by C++17 standard library |
| `src\external\3rd\library\libxml2-2.6.7.win32\` | Now provided by `find_package(LibXml2)` |
| `src\external\3rd\library\pcre\` | Now `find_package(PCRE)` |
| `src\external\3rd\library\zlib\` (vendored) | Now `find_package(ZLIB)` |

## What `swg-main\src\external\3rd\library\` *does* contain

Six libraries only (sorted):

```
libLeff       — community-added (purpose to verify)
platform      — SOE platform layer
soePlatform   — server SOE service APIs (kept)
udplibrary    — SOE UDP transport (kept — wire protocol)
vtune         — Intel VTune profiler hooks
webAPI        — community-added HTTP API
```

Notably absent from this list: `stationapi` (now its own top-level
submodule). Notable additions: `libLeff`, `webAPI` — both look
community-added rather than original SOE.

## What survived from a client-build planning perspective

Not very useful for porting the client, but useful as context:

- `src\engine\client\application\` has only `application/` subdir +
  a `CMakeLists.txt`. The client engine *library* tree
  (`engine\client\library\`) is gone entirely.
- Two `.vcproj` files survive in the whole src tree, both server-side:
  - `engine\server\application\CentralServer\src\shared\AuctionTransferGameAPI\AuctionTransferGameAPI.vcproj`
  - `external\3rd\library\soePlatform\CTServiceGameAPI\TestClient\TestClient.vcproj`
- Top-level `swg.sln` is gone — replaced by `CMakeLists.txt`.

## What swg-main *did* solve that's transferable

### 1. CMake migration (the patterns are useful even if the targets aren't)

`swg-main\src\CMakeLists.txt` is a 130-line top-level. Notable patterns:

- **C++17 standard:** `set(CMAKE_CXX_STANDARD 17)`. The codebase
  compiles cleanly under C++17 with the right flag set — this is the
  single most encouraging signal for our porting work.
- **Custom `cmake/win32/` and `cmake/linux/` toolchain modules.**
- **System-package finds** for Bison, Flex, JNI, LibXml2, Oracle, PCRE,
  Perl, Threads, ZLIB, CURL — instead of vendoring.
- **They explicitly null out CMake's auto-set optimization flags**
  (line 38–42) and roll their own per-`CMAKE_BUILD_TYPE`.
- **Three Linux compile profiles** controlled by `CMAKE_BUILD_TYPE`:
  `Debug` / `Release` / `RELWITHDEBINFO` (PGO profile gen) /
  `MINSIZEREL` (PGO profile use, LTO, `-fwhole-program-vtables`). They
  found Ofast breaks JNI when profiling — useful gotcha.
- Compiler-specific blocks for `Clang`, `GNU`, `Intel`. Win32 path
  exists but is candidly described in a comment as *"win32 is probably
  very very broken and won't build."*

### 2. Compile flags they kept that we need to keep too

These appear in their Win32 release block — apply identically to a
client port:

```
/Zc:wchar_t-              wchar_t = unsigned short (XPCOM ABI)
-D_USE_32BIT_TIME_T=1     32-bit time_t lock
-D_MBCS                   MBCS strings, not Unicode
-DDEBUG_LEVEL=N           1=opt-debug, 2=debug, 0=release
-DPRODUCTION=N            0=dev, 1=retail
/wd4244 /wd4996           noisy warnings off
/wd4018 /wd4351
/MP /FC                   parallel compile, full path in errors
/Ob1                      function-level inlining only
/MT or /MTd               static CRT
```

### 3. `STLPort` was dropped on the server

The Win32 CMake comment: *"win32 ... will need worked on so that MSVC
uses the vs2013+ STL instead of stlport, and probably will thus need
removal/modification/addition of TONS of the ifdef blocks for WIN32"*

So they **removed STLPort** on the server build (relying on system
libstdc++ on Linux) and openly note that this would propagate as a
sizeable refactor on Win32. **Important signal for client porting:**
keeping STLPort 4.5.3 is workable (it's vendored in whitengold and
ships .libs for vc6/7/7.1) but the long-term direction is dropping it.

### 4. Linux-side actually *works*

The repo has 1,249 commits with active maintenance through 2020. The
server *does* build with clang on Linux. That tells us:

- The C++ in this codebase is fundamentally compatible with modern
  compilers when the flag set is right.
- Ports / forward-fixes tend to be flag tuning + small ifdef churn,
  not deep refactor.
- The server side is the validated exemplar of a successful port.
  Whatever pattern they used (CMake structure, dropped STLPort,
  `find_package` for system libs) is the proven path.

## What this means for the user's plan

The previous plan ("minimum-change to compile, lean on swg-main for
client patches") **needs to be amended.** SWG-Source did not port the
client. We have to.

### Two viable framings for Phase 1

**Framing A — VS-2005-era toolchain, minimum source change.**

- Install Visual Studio 2005 (still downloadable from MSDN/archive).
- Install the matching DirectX 9 SDK (April 2005 era), MSVC 7.1
  redistributable, the era's Platform SDK.
- Open `swg.sln` and let it build as-is.
- Result: an `.exe` that actually has a chance of working without
  modifying a single line of C++.
- Cost: arcane toolchain setup, fragile environment, painful for the
  next person to onboard.

**Framing B — CMake migration, swg-main pattern, modern toolchain.**

- Author a `CMakeLists.txt` for the client tree following swg-main's
  style. Re-use their `cmake/win32/` find modules.
- Build with VS 2022 Community + Windows 11 SDK + a modern DirectX
  toolset.
- Keep STLPort 4.5.3 to start (lowest-risk path); drop it later.
- Keep `/Zc:wchar_t-`, `_USE_32BIT_TIME_T=1`, `_MBCS`.
- Relax `WarnAsError` for the first build.
- Cost: writing CMake. Days, not hours. But every modern dev can build it.
- Benefit: the client side is finally on the same build system as the
  server side (which the user is already running via swg-main).

### Recommendation

**Framing B.** Reasons:

1. The user is already going to run swg-main in a VM — they're already
   in CMake-land for the server. Aligning the client to CMake gives
   one mental model, not two.
2. The "minimal change to compile" framing doesn't fundamentally fail
   — we're still not changing C++ source files in this phase. We're
   *adding* a `CMakeLists.txt` parallel to the existing `.vcproj`s.
   The vcprojs can stay untouched as historical reference.
3. swg-main proves the codebase compiles under C++17 on a modern
   compiler with the right flag set. The client tree has more
   middleware coupling (libMozilla, DX9, Bink) but the core C++ is the
   same code. The pattern transfers.
4. VS 2005 isn't really "minimum change" — it's *minimum source
   change* paid for with *maximum environment change*. Operationally,
   VS 2022 + CMake is less effort overall.

If the user pushes back ("I want literally zero risk on the C++ side"),
Framing A is recoverable — but they should be told the day-1 cost is
the toolchain setup, and the day-90 cost is "nobody else can build
this without that exact VM."

## Specific blockers for client CMake migration

In rough order of effort:

| Blocker | Effort | Notes |
| --- | --- | --- |
| Author top-level `CMakeLists.txt` for client | 1–2 days | Mirror swg-main pattern |
| Author `find_package` modules for vendored libs (DX9, Miles, Bink, Vivox, libMozilla, DPVS, lcdui, STLPort) | 2–3 days | Most are simple `lib + header` finds. Mozilla XPCOM is messier. |
| CMakeLists for each of ~50 sister projects | 3–5 days | Mostly mechanical — every `.vcproj` becomes one CMakeLists.txt. swg-main's existing server-side CMakeLists give us templates. |
| Resolve initial wave of compile errors | unknown | Could be 0 (best case — modern flags work as-is) to weeks (worst case — STL incompatibilities everywhere). swg-main's experience suggests it's closer to "days of flag tuning." |
| Mozilla XPCOM linkage | unknown | The libMozilla wrapper expects pre-2010 XPCOM ABI. May need stubbing rather than real linkage. The swg-main team didn't have this problem because they dropped libMozilla entirely. |
| STLPort vs MSVC STL decision | mid-phase | Recommend **keep STLPort to start.** swg-main has comments suggesting the swap is non-trivial. |

## Concrete Phase 1 scope proposal

**Phase 1 — Modernise client build to CMake on VS 2022, MVB-0 target.**

* **Goal:** `cmake --build build --config Release` produces
  `SwgClient_r.exe`. The `.exe` doesn't have to run.
* **Deliverable:** `whitengold/CMakeLists.txt` + `whitengold/cmake/`
  + per-library / per-app `CMakeLists.txt` files, mirroring the
  existing `.vcproj` graph.
* **In scope:**
  - CMake build for all of `engine/client/library/`,
    `game/client/library/`, plus the `SwgClient` application.
  - `find_package` modules for the 8 vendored SDK trees.
  - Compile-flag tuning (`/Zc:wchar_t-`, MBCS, defines) per swg-main pattern.
  - Relaxing `WarnAsError` initially, then re-tightening.
  - Keeping STLPort 4.5.3.
* **Out of scope (deferred):**
  - Running the client.
  - Changing any C++ source file in `src/`.
  - `client.cfg` customisation.
  - libMozilla stubbing (let it link against the vendored XPCOM .libs
    even if it would crash at runtime).
  - Server side / swg-main integration.
  - TRE asset work.
* **Pass / fail:** does `link.exe` produce an `.exe`? If yes → done.
* **Estimated effort:** 2–4 weeks of focused work.

The user should approve / amend this scope before we kick off
`/gsd-new-project`.

## Closing observations

- **swg-main is 5+ years stale.** Last commit 2020-08-26. If we want
  current upstream community work we'd look at active forks
  (e.g. SWGEmu's ProjectSWG branches, AwakeningSWG). For server VM
  testing, swg-main is fine — it works as documented.
- **The user's VM target is server-side only.** When their VM is up,
  it will provide a private LoginServer + cluster on `127.0.0.1` (or
  a VM IP) that our client points at via `client.cfg`. The two sides
  of the project are properly decoupled.
- **swg-main's `dsrc` and `serverdata` submodules are fully populated**
  (85K and 125K files respectively). If the user's server work needs
  reference content (templates, data tables, world snapshots), pull
  from there rather than guessing.
