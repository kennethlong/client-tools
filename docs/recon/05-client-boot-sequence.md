# 05 — SwgClient boot sequence

> Phased reference for the SWG client boot from `WinMain()` to first
> rendered frame. Use this when planning build work, deciding what to
> stub, and identifying minimum-viable-boot milestones.

**Scope:** the retail-style game client (`SwgClient.exe` /
`SwgClient_r.exe` / `SwgClient_o.exe`). Editor tool clients (Particle
Editor, etc.) share most of these phases but follow short-circuit paths
in `Game::install()` covered separately.

**Source-of-truth files:**

| File | Role |
| --- | --- |
| `src/game/client/application/SwgClient/src/win32/WinMain.cpp` | OS entry, memory cap |
| `src/game/client/application/SwgClient/src/win32/ClientMain.cpp` | Subsystem install order |
| `src/engine/shared/library/sharedFoundation/src/win32/SetupSharedFoundation.cpp` | Window + config + registry |
| `src/engine/shared/library/sharedFoundation/src/win32/Os.cpp` | `CreateWindowEx` |
| `src/engine/shared/library/sharedFoundation/src/win32/RegistryKey.cpp` | Product registry path |
| `src/engine/client/library/clientGame/src/shared/core/Game.cpp` | `Game::install`, `Game::run`, scene state machine |
| `src/engine/client/library/clientGame/src/shared/network/GameNetwork.cpp` | All network wiring |
| `src/engine/client/library/clientGame/src/shared/network/LoginConnection.cpp` | Login handshake |
| `src/engine/client/library/clientUserInterface/src/shared/core/CuiLoginManager.cpp` | Cluster/character selection |
| `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiLoginScreen.cpp` | Login form |
| `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiAvatarSelection.cpp` | Char select |

**Companion docs:**

| For | Read |
| --- | --- |
| What links into this client | `docs/research/swgclient-build.md` |
| What runtime middleware is touched in each phase | `docs/research/runtime-middleware.md` |
| What's missing for any kind of build | `docs/gaps.html` |
| Cluster topology (what the client talks to) | `docs/architecture.html` |

---

## At-a-glance phase map

```
Phase 1  Process entry & memory cap        WinMain()
Phase 2  Thread + debug                    ClientMain → Setup{Thread,Debug}
Phase 3  Foundation: config + window       SetupSharedFoundation::install
Phase 4  Foundational subsystems           Compression..Pathfinding
Phase 5  TreeFile + SKU                    SetupSharedFile (sku bits)
Phase 6  Override config from TRE          installConfigFileOverride
Phase 7  Client subsystems (audio/gfx/UI)  SetupClient* + libMozilla
Phase 8  Game::install                     clientGame core managers
Phase 9  Splash + login backdrop           CuiMediatorFactory activate
─── login screen renders ─── (← MVB target)
Phase 10 Login form submission             SwgCuiLoginScreen::ok
Phase 11 LoginConnection handshake         LoginClientId(user, pass)
Phase 12 Cluster + character lists         LoginEnumCluster / EnumerateCharacterId
─── character select rendered ───
Phase 13 Cluster connect                   CuiLoginManager::connectToCluster
Phase 14 ConnectionServer handshake        ConnectionServerConnection
─── waiting for CmdStartScene ───
Phase 15 Scene transition                  CmdStartScene → Game::setScene
Phase 16 GroundScene load                  terrain + snapshot + player object
Phase 17 First rendered frame              Graphics::beginScene → endScene → present
```

The "──" lines are the natural milestones for incremental build work.

---

## Phase 1 — Process entry & memory cap

* **Phase name:** Process entry, memory manager target sizing.
* **Entry point:** `SwgClient/src/win32/WinMain.cpp:124` →
  `WinMain(hInstance, hPrevInstance, lpCmdLine, nCmdShow)`.
* **What it does:** OS hands control to `WinMain`. Before the CRT does
  anything, the function reads the env var `SWGCLIENT_MEMORY_SIZE_MB`.
  If unset, picks 75% of physical RAM clamped to `[250 MB, 750 MB]` in
  dev builds or `[250 MB, 2000 MB]` in production. Calls
  `MemoryManager::setLimit(megabytes, false, false)`.
* **Subsystems initialized:** `MemoryManager` only.
* **External dependencies:**
  - **Env vars:** `SWGCLIENT_MEMORY_SIZE_MB` (optional integer).
  - **WinAPI:** `GetEnvironmentVariable`, `GlobalMemoryStatus`.
  - No files, no registry, no network.
* **Failure modes:** practically none — bad env-var content is silently
  ignored.
* **Stub potential:** N/A; nothing here to stub.

## Phase 2 — Thread + debug bootstrap

* **Phase name:** Thread/debug install.
* **Entry point:** `ClientMain.cpp:137` `SetupSharedThread::install()` →
  `SetupSharedDebug::install(4096)`.
* **What it does:** Initialise the per-thread storage system (TLS keys
  for the engine's per-thread allocator, `PerThreadData`) and the
  in-house debug system (warning, fatal, report log machinery, ring
  buffer of 4096 entries).
* **Subsystems initialized:** `sharedThread`, `sharedDebug`.
* **External dependencies:** none (pure C++/Win32 thread setup).
* **Failure modes:**
  - `TlsAlloc` failure → `FATAL` immediately (process exits).
  - Should never happen on a working OS install.
* **Stub potential:** none required — these are pure in-process libs.

## Phase 3 — Foundation: config, window, registry

* **Phase name:** SharedFoundation install.
* **Entry point:** `SetupSharedFoundation::install(data)` in
  `sharedFoundation/src/win32/SetupSharedFoundation.cpp:180`.
  Configured via `ClientMain.cpp:152-167` with
  `data.configFile = "client.cfg"`, `windowName`, icons, frame rate
  60 Hz, mini-dump if `ApplicationVersion::isPublishBuild()` or
  `isBootlegBuild()`.
* **What it does:** Sets the unhandled-exception filter (writes
  `*-<ver>-<timestamp>.{txt,mdmp,log}` for crash reports), installs
  the command-line parser, parses `client.cfg`, then installs the
  full foundation stack including registry handles and the **main
  Win32 window**.
* **Subsystems initialized (in order):**
  1. `SetUnhandledExceptionFilter(MyUnhandledExceptionFilter)` —
     installs the SEH handler that writes minidumps if
     `data.writeMiniDumps`.
  2. `CommandLine::install`, then absorbs `lpCmdLine` and any
     post-`--` "section.key=value" pairs.
  3. `ConfigFile::install`, then `ConfigFile::loadFile("client.cfg")`.
  4. `MemoryManager::registerDebugFlags()`, `Profiler::registerDebugFlags()`
     (non-PROD), `FatalInstall()`.
  5. `DebugMonitor::install()` (non-PROD only).
  6. `ConfigSharedFoundation::install(defaults)` — loads frame-rate
     limit, demo mode, verbose-warning flags from cfg.
  7. `MemoryBlockManager::install`.
  8. `ExitChain::install`, `Report::install`, `Clock::install`,
     `CrashReportInformation::install`.
  9. **`RegistryKey::install(productRegistryKey)`** — opens / creates
     `HKCU\Software\Sony Online Entertainment\Default` and
     `HKLM\Software\Sony Online Entertainment\Default`. The cfg-file
     key `[ConfigSharedFoundation] ProductRegistryPath=...` overrides
     the default. Keys are *created* if absent (no fail-if-missing).
  10. `PersistentCrcString::install`, `CrcLowerString::install`,
      `WatchedByList::install`.
  11. `Os::install(hInstance, windowName, normalIcon, smallIcon)` —
      `RegisterClassEx` + `CreateWindow(windowName, windowName,
      WS_POPUP, 0, 0, 640, 480, ...)`. The window is initially 640×480
      and is resized later in `SetupClientGraphics`.
  12. `StaticCallbackEntry::install`, `setFatalVersionString`.
* **External dependencies:**
  - **Config file `client.cfg`** — must exist in the working
    directory or be findable by the relative open. Not finding it
    causes `ConfigFile::isEmpty()` to return true and
    `ClientMain.cpp:174` to hard-fatal:
    `FATAL(true, ("Config file not specified"))`. Note: the call here
    is `FATAL(...)` regardless of file existence; ConfigFile does not
    fatal if the file is missing — *empty contents* trigger the fatal.
  - **Registry:** `HKCU\Software\Sony Online Entertainment\Default`
    and `HKLM\...\Default` (created if missing — no privileged write
    needed for HKCU; HKLM is read-only via `AF_READ`).
  - **WinAPI:** `RegOpenKeyEx`, `RegCreateKeyEx`, `RegisterClassEx`,
    `CreateWindow`, `SetUnhandledExceptionFilter`.
  - **Files:** any `.include` directives inside `client.cfg`.
* **Failure modes:**
  - Missing `client.cfg` → `FATAL("Config file not specified")` →
    visible message-box dialog with crash dump.
  - `RegisterClassEx`/`CreateWindow` failure → `FATAL` and exit.
  - `RegistryKey` open failures are turned into FATAL with the registry
    error code printed.
  - HKLM key creation **does** fatal if it can't open a sub-key — be
    aware: if the key tree literally doesn't exist and the user lacks
    HKLM admin rights, this would FATAL. In practice the engine
    creates only relative subkeys under
    `Software\Sony Online Entertainment\Default`, which an unprivileged
    user can usually create under HKCU but not HKLM. Inspect
    `RegistryKey::install` if your test machine has restrictive HKLM
    policies.
* **Stub potential:**
  - The registry path is fully overridable from `client.cfg` via
    `ProductRegistryPath`. Set this to a private path to avoid sharing
    state with retail-installed SWG.
  - The window class name is whatever `data.windowName` is. Custom
    builds can rename freely.
  - There are no SOE-specific external services touched in this phase.

## Phase 4 — Foundational subsystem installs

* **Phase name:** Compression / regex / file / math / util / random / log / image / network / object / game / terrain / xml / pathfinding.
* **Entry point:** `ClientMain.cpp:208–292` — a sequence of
  `SetupX::install(...)` calls.
* **What it does:** Set up every cross-cutting engine library in
  dependency order. Most are pure in-process initialisation; the only
  one that touches an external resource is `SetupSharedFile`.
* **Subsystems initialized (in order):**
  1. `SetupSharedCompression::install(numberOfThreadsAccessingZlib=3)`
     — zlib + thread-safety wrappers (`sharedCompression/ZlibCompressor.cpp`).
  2. `SetupSharedRegex::install` — wires `pcre_malloc`/`pcre_free` to
     the engine's MemoryManager.
  3. `SetupSharedFile::install(true, skuBits)` — see Phase 5.
  4. `installConfigFileOverride()` — see Phase 6.
  5. `SetupSharedMath::install`.
  6. `SetupSharedUtility` (`m_allowFileCaching = true`).
  7. `SetupSharedRandom::install(time(NULL))` — non-deterministic seed
     by default.
  8. `SetupSharedLog::install("SwgClient")` — log filename prefix.
  9. `SetupSharedImage::install(default data)`.
  10. `SetupSharedNetwork::install` — initializes the SOE custom UDP
      stack (`udplibrary`) and the cross-message dispatcher.
  11. `SetupSharedNetworkMessages::install` +
      `SetupSwgSharedNetworkMessages::install`.
  12. `SetupSharedObject` with slot-id manager + customisation +
      movement-table data.
  13. `SetupSharedGame` (game scheduler, mount-valid scale ranges,
      debug-bad-strings hook).
  14. `CommoditiesAdvancedSearchAttribute::install`.
  15. `SwgCuiAuctionFilter::buildAttributeFilterDisplayString`.
  16. `SetupSharedTerrain` (game data variant).
  17. `SetupSharedXml`.
  18. `SetupSharedPathfinding`.
* **External dependencies:**
  - **Network:** `SetupSharedNetwork` opens UDP ports for the SOE UDP
    library but does not yet **connect** anywhere.
  - **Files:** none in this batch beyond what `SetupSharedFile` does.
* **Failure modes:**
  - Out-of-memory during install → FATAL.
  - Each `Setup*::install` is heavily defensive — failures usually
    fatal cleanly with a message.
* **Stub potential:** none of this talks to SOE infrastructure.

## Phase 5 — TreeFile + SKU detection

* **Phase name:** TRE search-path setup.
* **Entry point:** `ClientMain.cpp:218–230`:
  ```cpp
  uint32 skuBits = 0;
  if ((Game::getGameFeatureBits() & ClientGameFeature::Base) != 0)
      skuBits |= BINARY1(0001);
  if ((Game::getGameFeatureBits() & ClientGameFeature::SpaceExpansionRetail) != 0)
      skuBits |= BINARY1(0010);
  if ((Game::getGameFeatureBits() & ClientGameFeature::Episode3ExpansionRetail) != 0)
      skuBits |= BINARY1(0100);
  if ((Game::getGameFeatureBits() & ClientGameFeature::TrialsOfObiwanRetail) != 0)
      skuBits |= BINARY1(1000);
  SetupSharedFile::install(true, skuBits);
  ```
* **What it does:** Computes which SKUs (base game + JTL + RotW + ToOW)
  the client is licensed for, then mounts the corresponding `.tre`
  archives into the search path. The `Game::getGameFeatureBits()` value
  was set immediately above from `[Station] gameFeatures` and
  `[ClientGame] gameBitsToClear`, with a beta/pre-order
  `setJtlRetailIfBetaIsSet` shim and Episode 3 / Trials of Obi-Wan
  pre-order → retail upgrade.
* **Subsystems initialized:** `sharedFile/TreeFile`.
* **External dependencies:**
  - **Files:** every `.tre` archive on the configured search path.
    The path stack comes from `client.cfg` `[SharedFile]` keys
    (`searchPath0/1/2`) and the `[Tre]` section.
  - **Config keys read:** `[Station] gameFeatures`,
    `[Station] subscriptionFeatures`, `[ClientGame] gameBitsToClear`,
    `[ClientGame] setJtlRetailIfBetaIsSet`.
* **Failure modes:**
  - **Missing `.tre` files → silent.** The TreeFile system serves a
    log-warning when individual files don't open, but Phase 5 itself
    does not require any files. The first FATAL for a missing asset
    happens later — typically at `installConfigFileOverride` (Phase 6)
    when the engine tries to open `misc/override.cfg`, or at
    `AsynchronousLoader::install("misc/asynchronous_loader_data_*.iff")`
    in Phase 8.
  - If `Game::getGameFeatureBits()` is 0, `skuBits` is 0, and most
    asset opens fail downstream.
* **Stub potential:**
  - You can craft a tiny private `.tre` containing only the files the
    boot needs, instead of using retail content. That's how the editor
    tools' template configs work — see `template_AnimationEditor.cfg`
    in `exe/win32/` for a minimal search path example.
  - If you set `[Station] gameFeatures=15` (all bits) you get all four
    SKUs enabled regardless of subscription. This is the standard
    private-server hack.

## Phase 6 — Override config from TRE

* **Phase name:** Config-file override pass.
* **Entry point:** `ClientMain.cpp:232` `installConfigFileOverride()`
  (private static in `ClientMainNamespace`).
* **What it does:**
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
  Looks for `misc/override.cfg` *inside the TreeFile search path* and,
  if found, layers its key/value pairs over the loaded `client.cfg`.
  This is how live-team patches force-override settings without
  shipping a new client.
* **External dependencies:**
  - **Files:** optional `misc/override.cfg` inside any mounted `.tre`.
* **Failure modes:** none — if the file is missing the function is a
  no-op.
* **Stub potential:** ignore (just don't ship the file). Useful as an
  injection point if you need to push a patcher-style override.

## Phase 7 — Client subsystems: audio, graphics, libMozilla, input, animation

* **Phase name:** Client-side subsystems install.
* **Entry point:** `ClientMain.cpp:297–365`.
* **What it does:** Brings up audio (Miles), graphics (DirectX 9
  through the `Direct3d9` plug-in DLL), the embedded Mozilla browser,
  the video manager (Bink), DirectInput, then all the visual-content
  subsystems.
* **Subsystems initialized (in order):**
  1. `SetupClientAudio::install` — boots Miles, opens digital audio
     device (no `Mss32.dll`? see *failure modes*).
  2. `SetupClientGraphics::install(setupGraphicsData)` —
     `screenWidth=1024`, `screenHeight=768`, `alphaBufferBitDepth=0`.
     This **resizes the window** and creates the D3D9 device through
     the `gl0X_*.dll` plug-in.
  3. **`libMozilla::init`** — `GetCurrentDirectory` then resolves
     `<cwd>\mozilla`. Loads XPCOM DLLs (`xul.dll`, `xpcom.dll`,
     `nspr4.dll`, etc.). Hard-fatal on failure:
     ```cpp
     if (!libMozilla::init(Os::getWindow(), sPath.c_str()))
         DEBUG_FATAL(true, ("Mozilla init failed.\n"));
     ```
  4. `VideoList::install(Audio::getMilesDigitalDriver())` — registers
     Bink as a video provider, sharing audio mix with Miles. Bink is
     loaded via `LoadLibrary("binkw32.dll")`.
  5. `SetupClientDirectInput::install(hInstance, Os::getWindow(),
     DIK_LCONTROL, Graphics::isWindowed)`.
  6. `SetupClientObject`, `SetupClientAnimation`,
     `SetupClientSkeletalAnimation`, `SetupClientTextureRenderer`,
     `SetupClientTerrain`, `SetupClientParticle`.
* **External dependencies:**
  - **DLLs (`<cwd>\`):** `Mss32.dll`, `binkw32.dll`, the chosen
    `gl0X_*.dll`, `dpvs.dll`/`dpvsd.dll`, `lgLcd.lib`-companion
    runtime.
  - **DLLs (`<cwd>\mozilla\`):** `xul.dll`, `xpcom.dll`, `nspr4.dll`,
    `plc4.dll`, `plds4.dll`, `nss3.dll`, `nssckbi.dll`, `smime3.dll`,
    `softokn3.dll`, `ssl3.dll`, `js3250.dll`, `gksvggdiplus.dll`, plus
    `mozilla\chrome\` tree.
  - **Display:** D3D9-capable adapter; `dxdiag` settings honoured via
    `defaults.cfg`/`options.cfg`.
* **Failure modes:**
  - **No `Mss32.dll`:** `SetupClientAudio` fails to open driver →
    audio disabled but boot continues; some downstream code may FATAL.
  - **No D3D9 device available:** `SetupClientGraphics::install`
    returns false. The wrapping `if (...)` in `ClientMain.cpp:318`
    skips the rest of game setup; the process exits cleanly without
    a window appearing.
  - **`libMozilla::init` failure:** `DEBUG_FATAL` (in DEBUG builds
    only). In RELEASE the assertion is off, but operations on the
    browser later will null-deref. This is a real problem area for
    private-server work.
  - **Missing `binkw32.dll`:** Bink is late-bound, so absence is
    survivable until the first cinematic plays.
* **Stub potential:**
  - **libMozilla:** *can* be stubbed by replacing the wrapper with an
    init that returns true and noop'd browser widgets. Almost every
    private-server build does this.
  - **Bink:** stub `Bink::bindBink` to return false; cinematics don't
    play but the game runs.
  - **Vivox:** the wrapper has a "disabled" mode driven by config.
  - **DPVS:** required at present — replacement is harder.

## Phase 8 — `Game::install`

* **Phase name:** clientGame core managers.
* **Entry point:** `Game::run()` calls `Game::install(A_client)` at the
  top of the loop (`Game.cpp:1039`). This pulls in dozens of
  client-side managers.
* **What it does:** Registers crash-report dynamic strings, profession
  manager statics, then either takes a tool short-circuit
  (Particle/Animation editors etc.) or proceeds with full client
  install.
* **Subsystems initialized (full client path, `Game.cpp:777–833`):**
  - `MoodManagerClient`, `ObjectAttributeManager`,
    `DraftSchematicManager`, `AuctionManagerClient`,
    `PlanetMapManagerClient`, `ResourceIconManager`,
    `ClientRegionManager`, `QuestJournalManager`, `RoadmapManager`,
    `QuestManager`.
  - `CommandCppFuncs::install`, `ClientCommandQueue::install`.
  - `ClientCombatPlaybackManager::install("combat/combat_manager.iff")`
    — reads from TreeFile.
  - `CellProperty::setAccessAllowedHookFunction` (portal access).
  - `SkeletalAppearance2` callback hook.
  - `GameNetwork::install` — final network setup, but no connection
    yet.
  - `CutScene::install`.
* **External dependencies:**
  - **Files (TreeFile):** `combat/combat_manager.iff` and various
    other manager-specific IFFs.
* **Failure modes:**
  - Missing manager IFFs → FATAL with file-not-found message.
  - DataLint mode (debug only) replaces install with bulk asset scan.
* **Stub potential:** none of this is SOE-infrastructure-bound; all
  resources are local TRE content.

## Phase 9 — Splash + login backdrop

* **Phase name:** First UI display — login screen renders.
* **Entry point:** `Game.cpp:961`:
  ```cpp
  IGNORE_RETURN(CuiMediatorFactory::activate(CuiMediatorTypes::Splash));
  IGNORE_RETURN(CuiMediatorFactory::activate(CuiMediatorTypes::Backdrop));
  preloadAssets();
  ```
* **What it does:** Activate the Splash mediator (logo / cinematic
  intro) and Backdrop (the Star-field-looking login background). The
  splash plays an intro Bink, then yields to the login UI, which is
  itself a `SwgCuiLoginScreen` mediator activated lazily.
  `preloadAssets` warms commonly-needed templates, textures, and
  shaders so the user doesn't see hitches on first character-render.
* **Subsystems initialized:** `CuiManager`, `AsynchronousLoader`,
  `SwgClientUserInterface`, `SwgCuiManager`. CuiManager itself was
  installed in Phase 8; here we *activate* the first mediators.
* **External dependencies:**
  - **Files (TreeFile):** every UI XML, image, and string-table the
    Splash + Backdrop touch — from `ui/`, `texture/ui/`, `string/en/`.
  - **Files:** `misc/asynchronous_loader_data_<shaderCapMajor>.iff`.
  - **Config:** `[ClientUserInterface]` keys, font choices, etc.
* **Failure modes:**
  - Missing UI assets → FATAL or visible "missing texture" placeholder.
  - Bink intro not present → splash skipped silently.
* **Stub potential:** This is the **MVB target** — the smallest set of
  phases that produces a visible application. See "Minimum Viable
  Boot" section below.

> **MVB milestone reached:** at the end of Phase 9 the user sees a
> login screen.

## Phase 10 — User submits login form

* **Phase name:** Login form submission.
* **Entry point:** `SwgCuiLoginScreen::ok()` (the OK / Login button
  callback) in
  `swgClientUserInterface/src/shared/page/SwgCuiLoginScreen.cpp:230+`.
* **What it does:** Reads the typed username + password (or, if
  `[Station] sessionId` is present, uses that as a session token).
  Walks `[ClientGame] loginServerAddress0..N` and `loginServerPort0..N`
  from the config; picks one at random (`Random::random(N - 1)`);
  saves the choice into `ConfigClientGame`; calls
  `GameNetwork::connectLoginServer(addr, port)` — which constructs a
  `LoginConnection` over **UDP** (`setupData.useTcp = false`).
* **Subsystems initialized:** none new — uses already-installed
  `GameNetwork` (`sharedNetwork/udplibrary`).
* **External dependencies:**
  - **Config keys:** `[ClientGame] loginServerAddress0..N`,
    `loginServerPort0..N`. Falls back to
    `ConfigClientGame::getLoginServerAddress` /
    `getLoginServerPort` if the list is empty.
  - **Network:** outbound UDP to `loginServerAddress:loginServerPort`.
  - **Optional config:** `[Station] sessionId` (for Sony Station SSO).
* **Failure modes:**
  - No `loginServerAddress` configured → connects to empty string;
    `LoginConnection` fails to open → user sees the cancel/timeout
    UI loop.
  - Wrong port / firewalled → silent timeout shown as
    `LoginConnectionClosed` event.
* **Stub potential:**
  - Set `loginServerAddress0=127.0.0.1` and `loginServerPort0=44453`
    in your private `client.cfg`.
  - Ensure `[Station] sessionId` is unset for plain user/pass; or
    set it to a known fake token if your private LoginServer accepts
    bypass.

## Phase 11 — LoginConnection handshake (Station auth divergence here)

* **Phase name:** Login server handshake.
* **Entry point:** `LoginConnection::onConnectionOpened()` in
  `clientGame/.../LoginConnection.cpp:53`.
* **What it does:**
  ```cpp
  CuiLoginManager::setOverrideSessionIdKey(std::string());

  bool sendUserName = true;
  if (CuiLoginManager::getSessionIdKey() && !ConfigClientGame::getEnableAdminLogin())
      sendUserName = false;

  LoginClientId id(sendUserName ? GameNetwork::getUserName() : "",
                   GameNetwork::getUserPassword());
  send(id, true);

  #if PRODUCTION != 1
      GenericValueTypeMessage< int > msg("RequestExtendedClusterInfo", 0);
      send(msg, true);
  #endif

  emitMessage("LoginConnectionOpened");
  ```
  Two paths:
  1. **Station auth path** — if `[Station] sessionId` is set in
     config and admin login isn't on, the username is *not* sent.
     The LoginServer is expected to look up the user via a Station
     session token validated against `sdt-session*.station.sony.com`.
     **This path is dead** for any private server — Sony retired the
     session servers.
  2. **Plain user/pass path** — username + password are sent in the
     clear (UDP, but `LoginConnection` rides the SOE UDP library
     which has its own optional encryption). The LoginServer can
     validate against its own database without SOE infrastructure.
* **Subsystems initialized:** none new.
* **External dependencies:**
  - **Network (server-side perspective):** `sdt-session1.station.sony.com:3000`
    if Station auth is active. The client itself does not contact this
    address — the LoginServer does. But the client *expects* the
    LoginServer to honour the session token, so the auth model must
    match.
* **Failure modes:**
  - Station auth attempted with no session servers → LoginServer
    rejects → client gets `LoginIncorrectClientId` →
    `disconnectLoginServer()` → login form re-shows error.
  - Network version mismatch → `LoginIncorrectClientId` with reason
    `(client / server network version mismatch)` →
    `disconnectLoginServer()`.
  - Successful auth → server emits `LoginEnumCluster` and
    `EnumerateCharacterId` (Phase 12).
* **Stub potential:**
  - **Critical SOE-specific assumption.** For a private setup,
    *do not set* `[Station] sessionId` in the client cfg. The
    LoginServer (even the original SOE source) honours
    `validateStationKey=0` which short-circuits Station validation.
    See `loginServer.cfg` in `exe/linux/`.
  - For private Station-style auth (e.g. an OAuth bridge), implement
    `setOverrideSessionIdKey()` and pre-fill the override before
    `connectLoginServer()`. The community swg-site / swg-ostrich
    projects do exactly this.

## Phase 12 — Cluster + character lists

* **Phase name:** Cluster enumeration and character list.
* **Entry point:** Server-pushed messages handled in
  `CuiLoginManager.cpp` (`onLoginEnumCluster`,
  `onEnumerateCharacterId`).
* **What it does:** The server replies to the successful `LoginClientId`
  with:
  - `LoginEnumCluster` — list of galaxy/cluster names with host/port
    pairs (one per `ConnectionServer` cluster front door) plus a
    cluster id.
  - `EnumerateCharacterId` — list of characters tied to this account,
    each with `(NetworkId, clusterName, characterName, characterType)`.
  - `LoginClusterStatus` — per-cluster up/down state, ping address
    & port, max-character flags.
  - `ClusterGroup` / `LoginEnumCluster_Chardata` (extended info in
    non-PRODUCTION).
* **Subsystems initialized:** none new — populates `CuiLoginManager`'s
  static maps and emits `Messages::AvatarListChanged`,
  `Messages::ClusterListChanged`.
* **External dependencies:**
  - **Network:** UDP from LoginServer (still on the same connection).
  - The LoginServer at this point queries its Oracle login schema for
    the character list — see `serverDatabase` ports/adapters.
* **Failure modes:**
  - Server-side DB failure → empty character list → user sees the char
    select screen with no characters.
  - Disconnect mid-handshake → `LoginConnection::onConnectionClosed` →
    UI returns to login form.
* **Stub potential:** purely server-side concern; the client trusts
  whatever the LoginServer says.

> **MVB extension:** at the end of Phase 12 the user sees the cluster /
> character select screen. This is a useful *second* milestone after
> "login screen renders".

## Phase 13 — Cluster connect

* **Phase name:** ConnectionServer connect.
* **Entry point:** `CuiLoginManager::connectToCluster(ClusterInfo&)` in
  `CuiLoginManager.cpp:1029`:
  ```cpp
  GameNetwork::connectConnectionServer(cinfo.getHost(), cinfo.getPort(), cinfo.name);
  s_connectedCluster = cinfo.id;
  ```
* **What it does:** Opens a *second* SOE-UDP connection — this one to
  the chosen cluster's `ConnectionServer`. The character has not been
  picked yet; this is the "I want to play on this galaxy" step. The
  ConnectionServer will then route a session through to a
  `SwgGameServer`.
* **Subsystems initialized:** `ConnectionServerConnection`.
* **External dependencies:**
  - **Network:** outbound UDP to `<connectionServerHost>:<port>`
    addresses returned by the LoginServer's cluster list.
  - **Pings:** `[ClientUserInterface] connectionServerPingPeriodMs`
    drives a separate UDP ping socket sending 4-byte timestamps to
    every cluster's ping address. The cluster list includes
    `pingAddress`/`pingPort` per cluster.
* **Failure modes:**
  - Cluster down → ping returns nothing → cluster shows as offline in
    UI.
  - Wrong host → connect attempt times out.
  - Successful → `Messages::ConnectionServerConnectionChanged` emitted;
    LoginConnection is typically *kept open* in parallel until the
    handoff completes.
* **Stub potential:** entirely controlled by what the LoginServer
  reports as cluster info. Set up the LoginServer cluster table to
  point at `127.0.0.1:<connserver-port>` for local play.

## Phase 14 — Character selection → ConnectionServer handshake

* **Phase name:** Character select submission.
* **Entry point:** `SwgCuiAvatarSelection.cpp` — the "Play" button
  fires `SelectCharacter` (a `GenericValueTypeMessage<NetworkId>`) on
  the ConnectionServer.
* **What it does:** Server validates the character belongs to the
  account, sets the player's network id, then begins the scene-load
  sequence by emitting `CmdStartScene`.
* **Subsystems initialized:** none new.
* **External dependencies:**
  - **Network:** ConnectionServer UDP.
  - **Configuration server side:** account/character mapping in Oracle.
* **Failure modes:**
  - Invalid character → `ErrorMessage` from server → UI banner.
  - Server too busy → `LoginQueueUpdate` → user sits in queue.
* **Stub potential:** server-side.

## Phase 15 — Scene transition

* **Phase name:** `CmdStartScene` reception → `Game::setScene`.
* **Entry point:** `GameNetwork::onReceive` matches `"CmdStartScene"`
  → `GameNetwork::receiveCmdStartScene` →
  `Game::setScene(false, sceneName, objectId, templateName,
   startPosition, startYaw, time, disableSnapshot)` in
  `Game.cpp:1298`.
* **What it does:** Constructs a `MultiPlayerSceneCreator` (`Game.cpp:241`).
  Optionally plays a `cutScene` (intro Bink keyed off the terrain
  filename via `_getCutSceneFromTerrainFilename`) before the scene
  build. Calls `_startScene()` which invokes `sc->create()` and stores
  the resulting `GroundScene*` as the active scene.
* **Subsystems initialized:** `GroundScene`, `CutScene` (if any).
* **External dependencies:**
  - **Files (TreeFile):** the `<sceneName>.trn` file (the planet
    terrain, e.g. `terrain/tatooine.trn`), plus the
    `<templateName>.iff` for the player object (e.g.
    `object/creature/player/shared_human_male.iff`), plus all asset
    dependencies.
  - **Network:** ConnectionServer UDP continues.
* **Failure modes:**
  - Missing `.trn` → FATAL.
  - Missing player template → FATAL.
  - Missing cut-scene Bink → silent skip.
* **Stub potential:** all asset content — same gap as Phase 5. With
  retail TRE archives present this works as-is.

## Phase 16 — GroundScene load

* **Phase name:** Terrain + world snapshot + player object load.
* **Entry point:** `GroundScene::GroundScene(...)` in
  `clientGame/src/shared/scene/GroundScene.cpp`.
* **What it does:** Loads the procedural terrain, registers cells/
  portals with DPVS via `RenderWorld`, kicks off async loads for
  meshes, instantiates the player creature, hooks up cameras and HUD.
  Receives subsequent server messages (`UpdateContainmentMessage`,
  `BaselinesMessage`, etc.) that sync the player's inventory, skills,
  buffs, etc.
* **Subsystems initialized:** `GroundScene`, `WorldSnapshotParser`,
  `Camera`, HUD widgets.
* **External dependencies:**
  - **Files (TreeFile):** `*.ws` world snapshot for the planet,
    countless mesh / animation / particle / shader IFFs, the planet's
    `.trn` definition.
  - **Network:** continuous ConnectionServer UDP (containment, baseline,
    chat).
* **Failure modes:**
  - Mesh asset miss → renders default red placeholder appearance.
  - Snapshot version mismatch → FATAL.
* **Stub potential:** the loading screen showing here is the place
  most people debug "something's wrong with my asset tree" issues.

## Phase 17 — First rendered frame

* **Phase name:** First frame.
* **Entry point:** `Game::runGameLoopOnce(false, NULL, 0, 0)` in
  `Game.cpp:1065`, called from `Game::run`.
* **What it does:** Standard frame loop:
  ```cpp
  Os::update();                    // pump Win32 messages
  VideoPlaybackManager::service();
  GameScheduler::alter(elapsedTime);
  DirectInput::update();
  GameNetwork::update();           // network tick
  IoWinManager::processEvents();
  CuiManager::update();
  Audio::alter(elapsedTime, sound);
  TextureRendererManager::alter();

  Graphics::beginScene();
  Appearance::beginNewFrame();
  IoWinManager::draw();            // scene draw
  Graphics::endScene();
  VideoPlaybackManager::performBlitting();
  Graphics::present();
  ```
* **Subsystems initialized:** none — steady-state.
* **External dependencies:** the GPU.
* **Failure modes:** D3D9 device lost (alt-tab) is handled
  internally; permanent driver crash → exit.
* **Stub potential:** N/A.

---

## SOE-specific infrastructure assumptions — what to redirect / stub

| Phase | Assumption | What to do |
| --- | --- | --- |
| 3 | Registry path `Software\Sony Online Entertainment\Default` | Override via `[ConfigSharedFoundation] ProductRegistryPath=Software\YourProject\YourBranch` in `client.cfg`. Optional but tidy. |
| 5 | `[Station] gameFeatures` controls SKU bits | Set to `15` (all four bits) for unrestricted access. |
| 7 | `<cwd>\mozilla\` must contain XPCOM runtime | Either ship the XPCOM DLLs intact (they're in `exe/win32/mozilla/`), stub `libMozilla::init` to return true, or replace with CEF. |
| 7 | `binkw32.dll` is `LoadLibrary`'d | Ship it (already in `exe/win32/`) or stub the bind to disable cinematics. |
| 10 | `[ClientGame] loginServerAddress0..N` | Set to your private LoginServer IP. |
| 11 | `[Station] sessionId` triggers Station SSO | **Leave unset.** Use plain user/pass through `LoginClientId`. |
| 11 | LoginServer expected to validate Station session against `sdt-session*.station.sony.com` | LoginServer-side: set `loginServer.cfg` `validateStationKey=0`. Client doesn't see this — only the server does. |
| 12 | Cluster list comes from LoginServer's Oracle DB | Server-side concern; populate the cluster table with your private galaxies. |
| 13 | ConnectionServer ping addresses come from cluster list | Server-side concern. |
| Phase 7 / Vivox | Voice service expects external Vivox server | Disable via config; voice is non-essential. |
| Crash reporter | Writes minidump locally; some configs ship them via SMTP/HTTP | Inspect `commonLiveOptions.cfg` and unset any `crashReportEndpoint`. |

---

## Minimum viable boot — first build milestone

**Goal:** the smallest set of phases that compile and run far enough to
display a usable login screen. This is the natural shape of the first
sprint.

**Milestones in order of ambition:**

| Milestone | Reaches | Required phases | Required components | What can be stubbed |
| --- | --- | --- | --- | --- |
| **MVB-0: Process starts, window appears, exits cleanly** | end of Phase 3 | 1, 2, 3 | `sharedThread`, `sharedDebug`, `sharedFoundation`, `sharedMath`, `sharedFile` (built but not loaded), STLPort, registry write to HKCU | Everything else — provide a stub `Game::run` that opens window then quits. |
| **MVB-1: Splash + login backdrop renders** | end of Phase 9 | 1–9 | + `sharedCompression` (zlib), `sharedRegex` (PCRE), `sharedNetwork` foundation (UDP can be unused), `sharedObject`, `sharedGame`, `sharedTerrain`, `sharedXml`, `sharedPathfinding`, `clientAudio` (Miles can be a stub), `clientGraphics` (real DX9 + DPVS required), `clientObject`, `clientAnimation`, `clientSkeletalAnimation`, `clientTerrain`, `clientParticle`, `clientUserInterface`, `swgClientUserInterface`, plus Mozilla wrapper (real or stub). | Vivox (disable), Bink (no cinematics), TrackIR (already vestigial), G15 LCD. |
| **MVB-2: Login form submits to a real server** | end of Phase 12 | 1–12 | + `clientGame` networking (`GameNetwork`, `LoginConnection`), the SOE UDP library (`udplibrary`). Server side: a working private LoginServer (from `engine/server/application/LoginServer/` or a community equivalent). | Station auth (use plain user/pass), CommodityAPI, Vivox. |
| **MVB-3: Cluster select + character select render** | end of Phase 12+UI | 1–12 + char-select page | + `swgClientUserInterface/SwgCuiAvatarSelection`. | Avatar 3D preview can fall back to a generic mesh if your TRE is sparse. |
| **MVB-4: Walking around in the world** | end of Phase 17 | 1–17 | + `clientGame` scene + `GroundScene` + the full `sharedTerrain`/`clientTerrain` pipeline + retail-class TRE assets + a working `SwgGameServer` and `ConnectionServer`. | Voice, in-game web browser. |

The realistic first sprint targets **MVB-1**:
* Compile ~12 client libraries + ~22 shared libraries (the inventory in
  `docs/research/swgclient-build.md` lists them).
* Stub the four most painful dependencies in this order:
  1. `libMozilla::init` → return true; provide a no-op
     `SwgCuiWebBrowserWidget`.
  2. Vivox → use the wrapper's "disabled" mode.
  3. Bink → stub `Bink::bindBink` to fail; cinematics disabled.
  4. Logitech LCD → noop `SwgCuiG15Lcd`.
* Get a private `client.cfg` that doesn't require live SOE servers
  (no `[Station] sessionId`, `loginServerAddress0=127.0.0.1`,
  `[Station] gameFeatures=15`).
* Provide a TRE archive with the bare minimum content the splash and
  backdrop need: `ui/`, `texture/ui/`, `string/en/login.stf`,
  `font/`, plus the `misc/asynchronous_loader_data_<n>.iff` for the
  shader caps the GPU reports. Minimal community asset bundles often
  list this set.

If MVB-1 takes more than four weeks, **the project shape probably needs
re-evaluating** — that timeline is the proxy for whether a community
fork would be a better starting point than a from-scratch build of
this tree.

---

## PRODUCTION vs non-PRODUCTION boot differences

The `PRODUCTION` macro is a 0/1 build switch. Some highlights:

| Concern | `PRODUCTION == 0` (dev) | `PRODUCTION == 1` (retail) |
| --- | --- | --- |
| Window title | `SwgClient (<branch>.<version>)` | `Star Wars Galaxies` |
| Memory cap | clamped to 750 MB | clamped to 2000 MB |
| `data.demoMode` | false | true (changes warning summary) |
| `DebugMonitor::install` | yes | no |
| `Profiler::registerDebugFlags` | yes | no |
| `VTune::beginFrame` per frame | yes | no |
| `RequestExtendedClusterInfo` to LoginServer | yes (sent) | no |
| Editor entry paths (Particle/AnimationEditor/ NpcEditor/ TestIoWin) | available | compiled out |
| Menu viewer / single-player ground scene from cfg | available | compiled out |
| DataLint mode | available | compiled out |
| `SwgVideoCapture` / `SwgAudioCapture` | available | compiled out |
| `Graphics::pixSetMarker` calls | yes | no |
| PIX counters | enabled | disabled |
| `setJtlRetailIfBetaIsSet` shim | active | active (unconditional) |

**Implications for build work:**

* Keep `PRODUCTION == 0` while developing — you get the editor entry
  paths, debug menu, DataLint, video capture, and the
  `RequestExtendedClusterInfo` ping that returns extra cluster
  metadata.
* Avoid `PRODUCTION == 1` until you actually need the smaller window
  title / no-debug-marker code path. The two builds are
  source-compatible; only the flag changes.
* The dev build *is not* less secure than retail in any meaningful way
  — there's no anti-cheat in either path. The only "telemetry"
  difference is the extended cluster info request.

---

## Cross-references

| To find | Look at |
| --- | --- |
| Which library each `Setup*::install` belongs to | `docs/client.html` (subsystem map) |
| Whether a 3rd-party SDK touched in Phase 7 is replaceable | `docs/research/runtime-middleware.md` |
| Why `swg.sln` is the unit of build, not `SwgClient.vcproj` alone | `docs/research/swgclient-build.md` |
| What the Linux server cluster expects from the client | `docs/architecture.html` |
| The dead-end SOE auth path in detail | `docs/gaps.html` (gap #3) |

---

## Closing notes for the build engineer

* The boot is **deeply linear and well-instrumented**. Almost every
  install function calls `InstallTimer const installTimer("...")` so
  if you run the client with `[SharedDebug/InstallTimer] enabled=true` in the
  cfg, you get a profile of every phase by name. Use this religiously
  during MVB work to find which phase is hanging.
* `client.cfg` is the most important file in the project. Treat it as
  a build artefact: keep it under version control, parametrise host
  names, and never check the SOE-shipped one in.
* The login phase is the only place the client makes assumptions
  about *who* the server is. Once a `LoginConnection` is open, every
  message is opaque-payload-with-name; matching wire-protocol versions
  is what makes a private server compatible.
* If you're getting "login screen renders but submit hangs," the
  problem is almost always one of: (a) wrong `loginServerAddress` cfg,
  (b) firewalled UDP, (c) version mismatch on `LoginClientId`. In
  that order.
