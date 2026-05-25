# Deep-dive: Runtime middleware inventory

> Inventory the runtime middleware. For each, where it lives, what API
> surface area is used, link mode (static / DLL), and whether the
> integration is contained (replaceable) or pervasive (rewrite required).

## TL;DR — replaceability scoreboard

| # | Middleware | Side | Link mode | Surface area | Replaceable? |
| --- | --- | --- | --- | --- | --- |
| 1 | **DirectX 9** | Client | DLL plug-in (`gl0X_*.dll`) | Whole renderer | **Contained** — single plug-in |
| 2 | **Bink Video** | Client | DLL via `LoadLibrary` (delayed-bound) | One file (`BinkVideo.cpp`) behind `Video` interface | **Contained** — drop-in |
| 3 | **Miles Sound System** | Client | Static `mss32.lib` + `Mss32.dll` runtime | All of `clientAudio/` calls Miles directly | **Contained** but not facaded |
| 4 | **DPVS (Umbra)** | Client | Static `.lib` (full source vendored) | Used in `clientGraphics/RenderWorld*` (~10 files) | **Contained** to one library |
| 5 | **Vivox** | Client | Static via `vivoxSharedWrapper` + 6 DLLs | 5 files in `clientUserInterface/CuiVoiceChat*` | **Contained** — easiest to rip out |
| 6 | **libMozilla / XPCOM** | Client | Static (`xul.lib`, `xpcom.lib` …) + 12 DLLs | 4 files (`SwgCuiWebBrowser*` + `ClientMain.cpp`) | **Contained** — already a thin facade |
| 7 | **Logitech LCD (lgLcd)** | Client | Static `lgLcd.lib` (full source vendored) | 3 files in `swgClientUserInterface/SwgCuiG15Lcd*` | **Contained** — single feature |
| 8 | **TrackIR** | Client | Header-only / non-existent at runtime | **Effectively absent** — 1 dev file + launcher detection | **Contained** — already vestigial |
| 9 | **PCRE** | Both | Static `libpcre.a` (`PCRE_STATIC`) | 5 call sites; raw `pcre_*` calls leak to consumers | **Contained-ish** — small swap, no facade |
| 10 | **libxml2** | Both | Static (Win32) / shared `.so.2` (Linux) | 3 files in `sharedXml/` only | **Contained** — strict facade |
| 11 | **zlib** | Both | Static `zlib.lib` | 1 file (`ZlibCompressor.cpp`) behind `Compressor` interface | **Contained** — clean facade |
| 12 | **STLPort 4.5.3** | Both | Static `.lib` | **Every** `.cpp` includes it via STL headers | **Pervasive** (build-time, mostly invisible at semantic level) |
| 13 | **boost** | Both | Header-only | Only `config.hpp / smart_ptr.hpp / static_assert.hpp / utility.hpp` | **Trivial** — basically removable |
| 14 | **Pegasus PICTools** | Client | Static (`picn20m.lib`) | Only via `videocapture` stack in non-PROD builds | **Contained** — dev feature only |
| 15 | **Oracle OCI** | Server | Dynamic OCI client | All of `sharedDatabaseInterface/src_oci/` | **Contained** — explicit ports-and-adapters |
| 16 | **IBM Java 1.3 JVM** | Server | DLL via JNI (`jni.h`) | 380+ JNI sites across 80 `ScriptMethods*.cpp` files | **Pervasive** — load-bearing for *all* gameplay |
| 17 | **soePlatform suite** (Session/Chat/CS/CT/VChat) | Server | Static `.libs` per API | Each subsystem in its corresponding server | **Contained per-server** but total surface is large |
| 18 | **stationapi** | Server | Static + DLL fallback | Auth handshake in `LoginServer/SessionApiClient.*` | **Contained** to LoginServer |

> Total contained: 14. Pervasive: 2 (STLPort, JVM). Effectively dead: 1 (TrackIR).

The rest of this doc walks each one.

---

## Client-side runtime middleware

### 1. DirectX 9 — graphics API

* **Where**: includes `src/external/3rd/library/directx9/{include,lib}`. Render-side call sites: **only inside the `Direct3d9` plug-in** (`src/engine/client/application/Direct3d9/`, ~22 .cpp / 26 .h, ~450 KB).
* **Source files**: `Direct3d9.cpp`, `Direct3d9_DynamicIndexBufferData.cpp`, `Direct3d9_DynamicVertexBufferData.cpp`, `Direct3d9_LightManager.cpp`, `Direct3d9_Metrics.cpp`, `Direct3d9_PixelShaderProgramData.cpp`, `Direct3d9_RenderTarget.cpp`, `Direct3d9_ShaderImplementationData.cpp`, `Direct3d9_StateCache.cpp`, `Direct3d9_StaticIndexBufferData.cpp`, `Direct3d9_StaticShaderData.cpp`, `Direct3d9_StaticVertexBufferData.cpp`, `Direct3d9_TextureData.cpp`, `Direct3d9_VertexBufferDescriptorMap.cpp`, `Direct3d9_VertexBufferVectorData.cpp`, `Direct3d9_VertexDeclarationMap.cpp`, `Direct3d9_VertexShaderData.cpp`, plus `MemoryManagerHook.cpp`, `SetupDll.cpp`, `WriteTga.cpp`, `ConfigDirect3d9.cpp`, `FirstDirect3d9.cpp`.
* **API surface**: `IDirect3D9`, `IDirect3DDevice9`, `D3DPRESENT_PARAMETERS`, `D3DPOOL`, `D3DXMatrix*`, vertex/pixel-shader constant tables, `dinput8` (input), `dsound` (sound stub), `ddraw` (legacy fallback).
* **Linking**: `Direct3d9.vcproj` is `ConfigurationType="2"` — **builds as `gl05_d.dll` / `gl05_o.dll` / `gl05_r.dll` / `gl06_*.dll` / `gl07_*.dll` etc.** (the multiple variants in `exe/win32/`). Statically linked against `libjpeg.lib odbc32.lib odbccp32.lib winmm.lib delayimp.lib dxguid.lib d3d9.lib d3dx9.lib ddraw.lib dxerr9.lib`.
* **Integration depth**: **Contained.** The rest of the engine talks to a `Graphics::` abstraction (`clientGraphics`) — the only direct DX9 type leak outside the plug-in is in the launcher (`SwgClientSetup/ClientMachine.cpp`) detecting capabilities. The plug-in itself is the swap unit; in fact multiple plug-in variants already coexist for different shader profiles.
* **Replacement cost**: rewrite one DLL against a current API (D3D11, Vulkan). The whole rest of the client doesn't need to change.
* **Search evidence**: `IDirect3D9|IDirect3DDevice9|D3DPRESENT|D3DPOOL|D3DXMatrix` appears in 10 files — 8 of them are inside the DX9 SDK headers themselves, 1 is the launcher `ClientMachine.cpp`, 1 is in `external/3rd/library/DejaInsight/Demos/TinyEngine/graphics_x360.h` (a sample, not used).

### 2. Bink Video — RAD intro/cinematic codec

* **Where**: `src/external/3rd/library/bink/{include,lib/win32}` (3 headers: `bink.h`, `rad3d.h`, `radbase.h`; `binkw32.lib`, `binkw32.dll`, `binkw32.pdb`). Call sites: **6 files**, all in `src/engine/client/library/clientGraphics/src/Bink/` (`BinkVideo.cpp`, `BinkDLL.h`, `BinkVideo.h`, `BinkTreeFileIO.cpp`, `BinkTreeFileIO.h`, plus the SDK header).
* **API surface**: ~25 `Bink::Bink*` functions wrapped as inline forwarders in `BinkVideo.h` — `BinkOpen`, `BinkPause`, `BinkGoto`, `BinkDoFrame`, `BinkCopyToBuffer`, `BinkCopyToBufferRect`, `BinkRegisterFrameBuffers`, `BinkDX9SurfaceType`, `BinkSetVideoOnOff`, `BinkSetSoundOnOff`, `BinkSetVolume`, `BinkSetPan`, `BinkGetKeyFrame`, `BinkNextFrame`, `BinkWait`, `BinkShouldSkip`, `BinkGetRects`, `BinkGetSummary`, `BinkGetRealtime`, `BinkControlBackgroundIO`, `BinkGetTrackID`, `BinkClose`. Reads bink streams via the SOE `TreeFile` system (custom IO callback in `BinkTreeFileIO`).
* **Linking**: **Late-bound.** `BinkDLL.h` defines:
  ```cpp
  namespace Bink {
      bool bindBink(const char *i_dllname);
      void unbindBink();
      bool isBinkReady();

      namespace RAD { #include "bink.h" }
      using RAD::HBINK; using RAD::BINKFRAMEBUFFERS; ...

      BINK_STORAGE HBINK (BINK_CALL *BinkOpen)(const char *file_name,
                                               BINK_OPEN_FLAGS open_flags);
      BINK_STORAGE void (BINK_CALL *BinkClose)(HBINK bink);
      // ... all functions are pointer typedefs
  }
  ```
  Bink is loaded via `LoadLibrary` and resolved through function pointers — *not* statically linked. The `SwgClient.vcproj` does not list `binkw32.lib`.
* **Integration depth**: **Contained.** Behind the abstract `clientGraphics/Video` interface. Replacing it is implementing one new `Video` subclass.

### 3. Miles Sound System — RAD audio middleware

* **Where**: `src/external/3rd/library/miles/{include,lib/win}` (one big `Mss.h`, 224 KB; `Mss32.lib`). A second copy at `miles-7.2e/` exists but isn't linked. Runtime DLL: `exe/win32/Mss32.dll`.
* **API surface**: 188 raw API tokens (`HSAMPLE`, `H3DSAMPLE`, `HDIGDRIVER`, `HPROVIDER`, `HSTREAM`, `MSS_MC_SPEC`) inside `clientAudio`. Call-site files:
  - `clientAudio/src/win32/Audio.cpp` — the manager (182 token occurrences).
  - `clientAudio/src/win32/Sample2d.h`
  - `clientAudio/src/win32/Sample3d.h`
  - `clientAudio/src/win32/SoundObject3d.h`, `SoundObject3d.cpp`
  - `clientAudio/src/win32/SampleStream.h`
* **Linking**: Statically linked (`mss32.lib` in `SwgClient.vcproj`'s `libraries.rsp`) but the actual decode work happens in `Mss32.dll` at runtime (the .lib is largely an import lib). Exposes a Miles digital driver to clientGame:
  ```cpp
  VideoList::install(Audio::getMilesDigitalDriver());
  ```
  passed to Bink so cinematic audio can mix into the same audio stream.
* **Integration depth**: **Contained but not strictly facaded.** All Miles calls are inside `clientAudio/`, so consumer code uses `Sound2`, `Sound3d`, `SoundTemplate` and never sees a `HSAMPLE`. That said, Miles types do leak into a few **public** `clientAudio` headers (`Sample2d.h`, `Sample3d.h`) — so a swap requires a small header refactor on top of replacing the implementation.
* **Replacement cost**: medium — implement `Audio` and friends against OpenAL/FMOD/Wwise. ~20 .cpp inside one library.
* **Search evidence**: `HSAMPLE|H3DSAMPLE|HDIGDRIVER|HPROVIDER|HSTREAM` appears in 7 files total — 2 are Miles headers (`miles/include/Mss.h`, `miles-7.2e/include/mss.h`), 5 are inside `clientAudio`. **Zero leakage outside `clientAudio`.**

### 4. DPVS — Umbra occlusion culling

* **Where**: `src/external/3rd/library/dpvs/{interface,implementation,lib/win32-x86}`. The full source is vendored (163 files), plus prebuilt `dpvs.lib` / `dpvsd.lib`.
* **API surface**: `Dpvs::Cell`, `Dpvs::Object`, `Dpvs::Camera`, `Dpvs::Visibility` and friends. Used in:
  ```
  clientGraphics/RenderWorld.cpp
  clientGraphics/RenderWorld.h
  clientGraphics/RenderWorldCommander.cpp
  clientGraphics/RenderWorldCommander.h
  clientGraphics/RenderWorldServices.cpp
  clientGraphics/RenderWorldServices.h
  clientGraphics/RenderWorld_CellNotification.cpp
  clientGraphics/RenderWorld_OcclusionNotification.cpp
  clientGraphics/Light.cpp
  clientGraphics/Light.h
  clientGraphics/SimpleAppearance.h
  clientGraphics/IndexedTriangleListAppearance.cpp
  clientGraphics/IndexedTriangleListAppearance.h
  clientGame/test/WaterTestAppearance.cpp
  clientGame/test/WaterTestAppearance.h
  ```
  ~15 files — `RenderWorld` is the central registrar of cells/portals/objects; everything that wants to be culled inherits a notification interface that calls into DPVS.
* **Linking**: Static. Different lib for debug (`dpvsd.lib`) vs everything else (`dpvs.lib`).
* **Integration depth**: **Contained** to `clientGraphics`. Other client code touches portals/cells through `RenderWorld::*` — the abstraction is leaky (DPVS types appear in `RenderWorld.h`) but the call sites are all in one library.
* **Replacement cost**: medium-high — the cell/portal model is wired into `clientGraphics` in a way that mirrors DPVS's data model. A swap to a different culler (or just a frustum-only path) would touch ~10–15 files.

### 5. Vivox — voice chat

* **Where**: `src/external/3rd/library/vivox/{include,lib}` (vendored SDK: `vivoxsdk.dll`, `vivoxplatform.dll`, `ortp.dll`, `alut.dll`, `libsndfile-1.dll`, `wrap_oal.dll`, `VivoxVoiceService.exe`, plus `.lib` companions). Wrapped by `src/external/3rd/library/vivoxSharedWrapper/` (one `.cpp` + headers + Debug/Release `.lib`).
* **Vendored Vivox SDK headers**: `vivox/include/Vxc.h`, `VxcErrors.h`, `VxcEvents.h`, `VxcExports.h`, `VxcRequests.h`, `VxcResponses.h`, `vivox-config.h`, `vivox/include/vxplatform/VxcPlatform.h`, `vxcplatformmain.h`.
* **API surface**: extremely thin — all calls go through `vivoxSharedWrapper::Vivox` from exactly **5 client files**:
  - `clientUserInterface/src/shared/core/CuiVoiceChatGlue.h`
  - `clientUserInterface/src/shared/core/CuiVoiceChatManager.cpp`
  - `clientUserInterface/src/shared/core/CuiVoiceChatManager.h`
  - `clientUserInterface/src/shared/core/CuiVoiceChatEventHandler.cpp`
  - `clientUserInterface/src/shared/core/CuiVoiceChatEventHandler.h`
  No SDK type names (`Vxc*`, `vivoxsdk`) appear anywhere outside `vivoxSharedWrapper` and the SDK headers themselves.
* **Linking**: Static link to the shared wrapper (`vivoxSharedWrapper_Debug.lib` / `_Release.lib`); the wrapper internally talks to the runtime DLLs.
* **Integration depth**: **Contained**, by design — the `vivoxSharedWrapper` was built as a SOE-wide insulating layer (the original target was *EverQuest 2*, hence `Eq2Vivox.h`). Voice can be cleanly disabled or replaced.
* **Replacement cost**: low — gut the wrapper and the 5 client files. There's no game-design dependency on voice at the gameplay layer.

### 6. libMozilla / Mozilla XPCOM — in-game web browser

* **Where**: `src/external/3rd/library/libMozilla/`. The wrapper is full SOE source (its own `.vcproj`); the actual browser runtime is the vendored Mozilla 1.x XPCOM under `libMozilla/include/private/` — a stack of DLLs (`xul.dll`, `xpcom.dll`, `nspr4.dll`, `plc4.dll`, `nss3.dll`, `js3250.dll`, `softokn3.dll`, `ssl3.dll`, `nssckbi.dll`, `gksvggdiplus.dll`) plus a `chrome/` tree, all shipped in `exe/win32/mozilla/`. The `include/public/libMozilla/libMozilla.h` is a 41-byte forward to `src/win32/libMozilla.h`.
* **API surface**: `libMozilla::init(hwnd, path)`, `libMozilla::release()`, page-render and event hooks. Call sites: **only 4** —
  - `clientMain.cpp` (init/release)
  - `swgClientUserInterface/src/shared/page/SwgCuiWebBrowserWidget.cpp`
  - `swgClientUserInterface/src/shared/page/SwgCuiWebBrowserWidget.h`
  - `swgClientUserInterface/src/shared/core/SwgCuiWebBrowserManager.cpp`
  Used by browser-style chrome inside the in-game UI (web pages, NPE nag URL, the Trading-Card-Game integration overlay).
* **Init code in `ClientMain.cpp`**:
  ```cpp
  // We want to use the Mozilla that's shipped with the game, not whatever's on the system
  char szCWD[_MAX_PATH + 1];
  GetCurrentDirectory(_MAX_PATH, szCWD);
  std::string sPath(szCWD);
  sPath += "\\mozilla";
  if (!libMozilla::init(Os::getWindow(), sPath.c_str())) {
      DEBUG_FATAL(true, ("Mozilla init failed.\n"));
  }
  ```
* **Linking**: Static against XPCOM `.libs` (`nspr4.lib plc4.lib xpcom.lib xul.lib profdirserviceprovider_s.lib`). Browser content loaded from the `mozilla/` directory at runtime.
* **Integration depth**: **Contained** — already a thin facade. The rest of the codebase never touches XPCOM directly.
* **Replacement cost**: low — replace with CEF / a stub / nothing. Most modern emulator forks just drop the in-game browser entirely.

### 7. Logitech LCD UI (lgLcd) — G15 keyboard LCD

* **Where**: `src/external/3rd/library/lcdui/lib/lgLcd.lib` + headers. Full Logitech sample source vendored at `src/external/3rd/library/lcdui_src/` (LCDOutput, LCDManager, LCDGfx, EZ_LCD).
* **API surface**: `lgLcd*` C API wrapped behind the `EZ_LCD` C++ class. Game-side call sites: **3 files** — `swgClientUserInterface/SwgCuiG15Lcd.{cpp,h}` and a config flag in `clientUserInterface/ConfigClientUserInterface.{cpp,h}`. `SwgCuiHud` enables/updates it.
* **Linking**: Static `lgLcd.lib`.
* **Integration depth**: **Contained.** Single feature, single implementation file, gated by a config switch (`enableG15Lcd`).
* **Replacement cost**: trivial — delete the SwgCuiG15Lcd file and remove the lib reference.

### 8. TrackIR — head tracking

* **Where**: `src/external/3rd/library/trackIR/include/NPClient.h` (header only; no .lib referenced by SwgClient).
* **API surface**: **Effectively zero in the runtime client.** The only source file that references TrackIR is `src/engine/client/library/clientGame/src/shared/core/ClientHeadTracking.cpp` — and grepping confirms `TrackIR/NPClient` strings are otherwise found only in `SwgClientSetup` (the launcher's hardware-detection page).
* **Linking**: Not in `SwgClient.vcproj`'s linker list at all.
* **Integration depth**: **Vestigial.** A thin file exists for forward-compat but the feature is essentially absent. Most likely a feature that was prototyped (since SOE shipped JTL-era space combat that benefited from head tracking) and shelved.
* **Replacement cost**: nothing to do — already absent in practice.
* **Search evidence**: `TrackIR|trackIR|NPClient` appears in 6 files — 2 in the SDK header, 1 in `clientGame/ClientHeadTracking.cpp`, 3 in `SwgClientSetup`. **Zero in `clientGame`/UI/runtime control flow.**

### 14. Pegasus PICTools / SOE videocapture — JPEG2000 + screen capture

* **Where**: `src/external/3rd/library/videocapture/{AudioCapture,VideoCapture,ImageCapture,Smart,SoeUtil,ZlibUtil,PICTools}/lib/win32` (Debug/Release variants).
* **API surface**: SOE in-house screencapture lib stack (compressed video and image grabs). Pegasus PICTools is the JPEG2000 codec underneath (`picn20m.lib` / `picn20md.lib`).
* **Linking**: Static. Linked unconditionally by SwgClient, but only used in non-PRODUCTION builds (the call site is gated by `#if PRODUCTION == 0` in `clientAudio/Audio.cpp` for `SwgAudioCapture`, and similar guards for video).
* **Integration depth**: **Contained.** Effectively a development feature.
* **Replacement cost**: trivial in production builds — `#ifdef` it out.

---

## Server-side runtime middleware

### 15. Oracle OCI — database client

* **Where**: `src/external/3rd/library/oracle/`, `oracle_win32/ora90/oci/`. Call sites: **all of `src/engine/shared/library/sharedDatabaseInterface/src_oci/`** — `OciQueryImplementation.{cpp,h}`, `OciSession.{cpp,h}`, `OciServer.h`. That's the *only* directory in the source that includes `oci.h` / OCI symbols.
* **API surface**: `OCIEnv`, `OCILogon`, `OCIStmtPrepare`, `OCIDefineByPos`, `OCIBindByName`, `OCIError`, etc. — full OCI usage.
* **Linking**: Dynamic (Oracle Instant Client / OCI client install on the server box).
* **Integration depth**: **Contained** by deliberate ports-and-adapters split. The library has:
  - `sharedDatabaseInterface/src/` (vendor-neutral abstraction: `Bindable`, `Query`, `Session`)
  - `sharedDatabaseInterface/src_oci/` (Oracle implementation)
  Plus standard `build/{linux,shared_unix,win32}/` and `include/public/`. The whole rest of the codebase consumes only the abstraction. SWG-Source has experimented with parallel implementations on this same boundary.
* **Configuration**: `loginServer.cfg` style — `databaseProtocol=OCI`, `DSN=...`, `databaseUID=...`, `databasePWD=...`, `databaseThreads=4`.
* **Replacement cost**: medium — implement a parallel `src_pgsql/` or `src_mysql/` directory. The abstraction was clearly designed for this from day 1.

### 16. IBM Java 1.3 JVM — gameplay scripting host

* **Where**: `src/external/3rd/library/java/` (header stubs). The actual JRE has to be installed at `/usr/java` per `src/BUILDING`. Call sites: **`src/engine/server/library/serverScript/`** — `JavaLibrary.{cpp,h}`, `JNIWrappers.{cpp,h}`, plus **80 `ScriptMethods*.cpp` files** (one per gameplay area: Combat, Ai, Skill, Buff, Mission, Quest, Beast, Container, Faction, Movement, Animation, … all the way through to WorldInfo).
* **API surface**: full JNI — `jni.h`, `JavaVM`, `JNIEnv`, `jclass`, `jmethodID`, `JNI_CreateJavaVM`. **382 occurrences across the 15 surveyed files alone**; the real total across all 80 ScriptMethods is much higher.
* **`JavaLibrary.h` excerpt**:
  ```cpp
  //========================================================================
  //
  // JavaLibrary.h - interface to the JVM via JNI.
  //
  // copyright 2001 Sony Online Entertainment
  //
  //========================================================================

  #include <jni.h>
  #include "serverScript/JNIWrappers.h"
  #include "serverScript/ScriptDictionary.h"
  ```
* **Linking**: JVM loaded as a DLL/`.so` at server startup; JNI's `jvm` symbols resolved via dynamic loading.
* **Integration depth**: **Pervasive.** This is the most load-bearing middleware in the whole tree. *Every gameplay subsystem on the server* dispatches into the JVM via these JNI binders. `combat`, `quest`, `faction`, `cybernetics`, `space_mining`, you name it — all `dsrc/.../script/*.script` content runs in this JVM.
* **Replacement cost**: very high — but possible (and SWG-Source has done it). You can move from IBM Java 1.3 to OpenJDK by:
  1. Adjusting JNI binders for any 1.3 → 8+ behavioural changes (`GetByteArrayElements`, weak refs, exception semantics).
  2. Updating the script compilation pipeline to a modern JDK.
  3. Keeping the *gameplay scripts unchanged* — they consume the SOE-defined Java types, not Java standard-library specifics.
  Replacing the JVM itself with a different scripting host (Lua/JS) would mean rewriting everything in `dsrc/.../script/` — many tens of thousands of lines of gameplay logic. Don't do that.

### 17. soePlatform suite — SOE service APIs

* **Where**: `src/external/3rd/library/soePlatform/` — 8 sub-libraries: `Base`, `ChatAPI2`, `CSAssist`, `CTServiceGameAPI`, `MonAPI`, `Singleton`, `VChatAPI`, `libs`. Total: 267 headers / 190 `.cpp` / 24 prebuilt libs.
* **Per-server breakdown**:

  | Sub-library | Used by | Purpose |
  | --- | --- | --- |
  | `ChatAPI2` | `ChatServer` (only) | Persistent rooms, mail, friends list |
  | `VChatAPI` | `ChatServer` | Voice-chat coordination (Vivox bridge) |
  | `CSAssist` | `CustomerServiceServer` | GM petitions / appeals |
  | `CTServiceGameAPI` | `TransferServer` | Paid character transfers (CT = "Character Transfer") |
  | `MonAPI` | `MetricsServer` / `LogServer` | SOE-internal monitoring |
  | `Base`, `Singleton`, `libs` | Everyone | Shared util across the above |

  Plus the **`Session/CommonAPI`** side referenced by `SessionApiClient.cpp` — auth handshake, used **only by `LoginServer`**.

* **Concrete include sites** (grepping `#include "ChatAPI"|"CSAssistAPI"|"LoginAPI"|"CTService"|"VChatAPI"` under `src/engine/server`):
  ```
  TransferServer/src/shared/CTSAPIClient.h
  ChatServer/src/shared/VChatInterface.h
  ChatServer/src/shared/FirstChatServer.h
  ChatServer/src/shared/ChatServerRoomOwner.h
  ChatServer/src/shared/ChatServerAvatarOwner.h
  ChatServer/src/shared/ChatServer.h / .cpp
  ChatServer/src/shared/ChatInterface.h / .cpp
  ```
* **API surface**: each sub-library has its own dozens-of-classes API (e.g. `ChatAPI::ChatAPI`, `CSAssistAPI::Order`, `CTServiceCustomer`). Each is consumed by exactly one server.
* **Linking**: Static `.lib` per sub-library; some leverage `Mss32` (VChat), `989crypt`, `rdp` (stationapi).
* **Integration depth**: **Contained per-server.** Each server's coupling to soePlatform is bounded — `ChatServer` cannot be ported separately from `ChatAPI2`, but `CentralServer`/`PlanetServer`/`SwgGameServer` don't touch any of it.
* **Replacement cost**: high in aggregate, low per-feature. Community emulators have replaced *all* of this with home-grown C++ services that speak the same wire protocols to the unmodified game servers — that's effectively what `swg-site` (auth) and the `Holocore`-style chat services are.

### 18. stationapi — SOE Station authentication client

* **Where**: `src/external/3rd/library/stationapi/`
  - 8 headers (`stationapi.h`, `stationrequest.h`, `stationmsg.h`, `StationAPISession.h`, `extend_rdp.h`, `PackClass.h`, `order.h`)
  - 8 `.cpp` (`stationapi.cpp`, `stationrequest.cpp`, `stationapilist.cpp`, `StationAPISession.cpp`, `extend_rdp.c`, `PackClass.cpp`, `order.cpp`)
  - 3 prebuilt `.lib`: `989crypt.lib`, `rdp.lib`, `dbgutil.lib`
  - Old VS6 project files (`stationapi.dsp`, `stationapi.dsw`)
  - Demo (`stationapidemo/`)
  - HTML documentation (`html/`)
* **API surface**: `StationAPISession`, `StationAPIRequest`, `StationAPIResult`, `StationAPIProtocol`. Speaks SOE's proprietary auth protocol.
* **Linking**: Static. Used **only** by `LoginServer` (via `SessionApiClient`).
* **`SessionApiClient.cpp` excerpt** (`src/engine/server/application/LoginServer/src/shared/SessionApiClient.cpp:1-39`):
  ```cpp
  // SessionApiClient.cpp
  // copyright 2002 Sony Online Entertainment

  #include "FirstLoginServer.h"
  #include "SessionApiClient.h"

  #include "ClientConnection.h"
  #include "ConfigLoginServer.h"
  #include "CSToolConnection.h"
  #include "DatabaseConnection.h"
  #include "LoginServer.h"
  #include "PurgeManager.h"
  #include "serverNetworkMessages/ClaimRewardsMessage.h"
  #include "serverNetworkMessages/ClaimRewardsReplyMessage.h"
  #include "Session/CommonAPI/CommonAPIStrings.h"
  #include "sharedGame/PlatformFeatureBits.h"
  #include "sharedLog/Log.h"
  #include "sharedNetworkMessages/ErrorMessage.h"
  #include "sharedSynchronization/Mutex.h"

  #include <vector>

  std::map<apiTrackingNumber, ClientConnection *> SessionApiClient::m_validationMap;
  ```
* **Integration depth**: **Contained** to one server, gated by `validateStationKey=1` in `loginServer.cfg`. Setting that to `0` short-circuits the whole subsystem — which is exactly what every private-server project does.
* **Replacement cost**: trivial — disable, or replace with a stub that issues fake session tokens.

---

## Both-side / cross-cutting middleware

### 9. PCRE — Perl-compatible regex

* **Where**: `src/external/3rd/library/pcre/4.1/win32/{include,lib}` (`libpcre.a`).
* **API surface**: raw `pcre_compile` / `pcre_exec` plus a memory-allocator hook. Call sites: **5 files**:
  - `src/engine/shared/library/sharedRegex/src/shared/SetupSharedRegex.cpp` — installs the malloc hook
  - `src/engine/server/library/serverGame/src/shared/core/RegexList.cpp` — server-side chat/word filtering
  - `src/engine/shared/library/sharedTemplateDefinition/src/shared/core/TemplateDefinitionFile.h` — template compiler
  - `src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserScene.cpp`
  - `src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserMount.cpp`
* **Linking**: Static, with `PCRE_STATIC` define (`PreprocessorDefinitions="...;PCRE_STATIC"` in `sharedRegex.vcproj`).
* **`SetupSharedRegex.cpp` install function**:
  ```cpp
  void SetupSharedRegex::install() {
      InstallTimer const installTimer("SetupSharedRegex::install");
      DEBUG_FATAL(s_installed, ("SetupSharedRegex already installed."));

      ConfigSharedRegex::install();

      // Save old malloc and free functions.
      s_oldMalloc = pcre_malloc;
      s_oldFree   = pcre_free;

      // Hook up PCRE malloc and free to our memory manager.
      pcre_malloc = &RegexServices::allocateMemory;
      pcre_free   = &RegexServices::freeMemory;

      s_installed = true;
      ExitChain::add(remove, "SetupSharedRegex");
  }
  ```
* **Integration depth**: **Contained-ish.** The `sharedRegex` library is *not* a true facade — it only hooks `pcre_malloc`/`pcre_free`. Consumers `#include "pcre/pcre.h"` directly. But there are only 5 of them, so a replacement is one-day work.
* **Replacement cost**: low. Swap to `std::regex` or `re2` with a thin wrapper.

### 10. libxml2 — XML parsing

* **Where**: `src/external/3rd/library/libxml2-2.6.7.win32/{include,lib}` on Windows; on Linux the system `libxml2.so.2` (which is also dropped into `exe/linux/` as a fallback).
* **API surface**: `xmlParseFile`, `xmlNewDoc`, `xmlDocPtr`, `xmlNodePtr`. Used in **3 files only**:
  - `src/engine/shared/library/sharedXml/src/shared/tree/XmlTreeDocument.cpp`
  - `src/engine/shared/library/sharedXml/src/shared/tree/XmlTreeDocumentList.cpp`
  - `src/engine/shared/library/sharedXml/src/shared/tree/XmlTreeNode.cpp`
* **Linking**: Static (Win32 `libxml2-win32-release.lib`); shared on Linux.
* **Integration depth**: **Contained.** `sharedXml` exposes `XmlTreeDocument` / `XmlTreeNode` as the public API; consumers never see libxml2 types. This is by far the cleanest external integration in the tree.
* **Replacement cost**: trivial — single-library swap.

### 11. zlib — compression

* **Where**: `src/external/3rd/library/zlib/{include,lib}` (`zlib.lib`); also a vendored full-source copy at `zlib-1.2.3/`.
* **API surface**: `deflateInit2_`, `inflate`, `z_stream`. Call sites: **1 file** — `src/engine/shared/library/sharedCompression/src/shared/ZlibCompressor.cpp`.
* **Linking**: Static.
* **Integration depth**: **Contained.** `sharedCompression` exposes a `Compressor` interface; `ZlibCompressor` is one implementation. Used heavily by `TreeFile` (archive decompression) and `sharedNetwork` (packet compression), but always through the abstract interface.
* **Replacement cost**: trivial — and unnecessary; zlib is still current and license-friendly.

### 12. STLPort 4.5.3 — STL replacement

* **Where**: `src/external/3rd/library/stlport453/{stlport,lib/win32}`. Six prebuilt variants in `lib/win32/`:
  ```
  stlport_vc6_static.lib                 stlport_vc6_static.pdb
  stlport_vc6_stldebug_static.lib        stlport_vc6_stldebug_static.pdb
  stlport_vc7_static.lib                 stlport_vc7_static.pdb
  stlport_vc7_stldebug_static.lib        stlport_vc7_stldebug_static.pdb
  stlport_vc71_static.lib                stlport_vc71_static.pdb
  stlport_vc71_stldebug_static.lib       stlport_vc71_stldebug_static.pdb
  ```
  Notable: **no vc8 / VS-2005 binary** — original team built with VC++ 7.1.
* **API surface**: replaces *every* `<vector>`, `<map>`, `<string>` etc. include. `_USE_32BIT_TIME_T=1` is set globally to align with STLPort's expectations. Compile defines are project-wide.
* **Linking**: Static `stlport_vc71_static.lib` (or `stlport_vc6_static.lib` for older toolchain).
* **Integration depth**: **Pervasive at the build level, invisible at the semantic level.** Every `.cpp` in the codebase pulls in STLPort headers transparently. But you can't tell from reading the source — the code uses ordinary STL syntax. Modern MSVC STL usually works as a drop-in if you can rebuild.
* **Replacement cost**: low conceptually, fiddly in practice — you'd hit edge-case incompatibilities (allocator semantics, hash containers, iostreams) and need the warning-as-error gate relaxed during the migration.

### 13. boost — partial header use

* **Where**: `src/external/3rd/library/boost/boost/` — only **4 files**:
  ```
  config.hpp        — 27,605 bytes
  smart_ptr.hpp     — 15,905 bytes
  static_assert.hpp —  3,583 bytes
  utility.hpp       —  3,999 bytes
  ```
* **API surface**: a tiny subset — mostly `boost::shared_ptr` / `boost::scoped_ptr` and a few utility macros.
* **Linking**: Header-only.
* **Integration depth**: **Trivial.** Easily replaceable with `std::shared_ptr` and `std::unique_ptr`.

---

## What about `udplibrary`?

`src/external/3rd/library/udplibrary/` is technically in the third-party
tree, but it's **SOE in-house source**, not licensed middleware. It's the
reliable-UDP transport layer the wire protocol depends on — both server
(`engine/shared/library/sharedNetwork`) and the matching retail client share
it. You cannot replace it without breaking compatibility with retail
clients. Call it "owned, vendored, and load-bearing." For the purposes of
"is this middleware?" — no, it's just a SOE library that happens to live
under `external/3rd/`. Counts: 7 headers, 3 .cpp.

## What about Maya, Alienbrain, NvTriStrip, atiDxtc, DejaInsight, VTune, meshifier, debugHelp, blat?

These are all **build-time / authoring-tool** dependencies, not runtime —
they don't touch the running game.

| Library | Used by | Why excluded from this inventory |
| --- | --- | --- |
| Maya 4–8 SDK | `MayaExporter`, content tools | Authoring time only |
| Alienbrain XDK | TerrainEditor, GodClient | Asset-management integration in editor |
| NvTriStrip | `meshifier` | Mesh strip optimisation at export time |
| atiDxtc / ati_compress | `TextureBuilder` | DXT compression at bake time |
| DejaInsight, VTune | dev profiler hooks | Profiling — not in retail |
| debugHelp | crash reporter | Stack walking on dev workstations |
| blat194 | build email scripts | CI-only |
| videocapture (apart from PICTools) | dev capture | Non-PRODUCTION builds only |

If "runtime" gets interpreted broadly enough to include the editor/tool
processes (which are also runtimes of a sort), Maya/Alienbrain/Qt jump up
the list — but they don't touch `SwgClient.exe` or any of the cluster
servers.

## Bottom line: where rewriting hurts

If anything in this tree ever needs to be modernised aggressively, the cost
lives in just two places:

1. **The IBM Java 1.3 JVM dependency** — pervasive, unfixable without
   porting the JNI bridge, unavoidable because all gameplay rules live in
   the JVM. Plan for this.
2. **The Sony Station / soePlatform / stationapi suite** — not technically
   pervasive in any single server, but in *aggregate* it cuts across
   LoginServer, ChatServer, CustomerServiceServer, TransferServer,
   MetricsServer. Replacing it means writing community equivalents of five
   separate services. Community projects have done this already.

Everything else — DirectX, Bink, Miles, DPVS, Vivox, libMozilla, Logitech
LCD, Oracle OCI, PCRE, libxml2, zlib, boost, STLPort — is either contained
behind a facade or behind a single library boundary, and can be swapped or
stubbed without touching gameplay code.
