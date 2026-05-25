# Architecture — Layers, Data Flow & Patterns

Detailed sections separated from `CLAUDE.md` for brevity. The cluster overview, component responsibilities table, pattern overview, and architectural constraints remain in `CLAUDE.md`; this file expands everything below that.

**Source:** `.planning/codebase/ARCHITECTURE.md` | **Analysis Date:** 2026-05-03

---

## Engine / Library Layers

### Engine/Shared Foundation
- **Purpose:** Platform abstraction, memory, threading, file I/O, math, reflection
- **Location:** `src/engine/shared/library/{sharedFoundation,sharedFile,sharedThread,sharedMath,sharedUtility,sharedRandom,sharedDebug,sharedLog}`
- **Contains:** OS abstraction, config parser, CRC strings, memory manager, per-thread storage
- **Depends on:** Win32 API (client-side), POSIX/Linux (server-side)
- **Used by:** All other engine and game layers

### Engine/Shared Network
- **Purpose:** UDP/TCP connection handling, message dispatch, protocol framing
- **Location:** `src/engine/shared/library/sharedNetwork`
- **Contains:** `Connection`, `Service`, `NetworkHandler`, `ConnectionHandler`, message serialization
- **Depends on:** `sharedFoundation`
- **Used by:** Server cluster (inter-process), client ↔ ConnectionServer (player-facing)

### Engine/Shared Game
- **Purpose:** Object template system, shared gameplay types (attributes, effects, transforms)
- **Location:** `src/engine/shared/library/sharedGame`, `src/engine/shared/library/sharedTemplate`
- **Contains:** `ObjectTemplate`, slot system, customization data, movement tables, hardpoints
- **Depends on:** `sharedFoundation`, `sharedFile` (TreeFile for template loading)
- **Used by:** Client (object instantiation), server (template validation), scripts (object queries)

### Engine/Shared Terrain & Foliage
- **Purpose:** Terrain generation, flora (in-house vegetation system, not SpeedTree)
- **Location:** `src/engine/shared/library/sharedTerrain`
- **Contains:** `FloraGroup`, `FloraManager`, static/dynamic flora generators, radial culling
- **Depends on:** `sharedFoundation`, `sharedFile`
- **Used by:** Client terrain renderer, server world snapshot

### Engine/Client Graphics
- **Purpose:** DirectX 9 render loop, shader state, viewport management
- **Location:** `src/engine/client/library/clientGraphics`
- **Contains:** `Graphics::beginScene/endScene`, texture binding, vertex/index buffer submission
- **Depends on:** `sharedFoundation`, DirectX 9 SDK, STLPort
- **Used by:** `clientGame` main loop

### Engine/Client Audio
- **Purpose:** Miles Sound System integration (proprietary middleware)
- **Location:** `src/engine/client/library/clientAudio`
- **Contains:** 3D sound positioning, music system, voice-over playback
- **Depends on:** Miles SDK, `sharedFoundation`
- **Used by:** Game UI, ambient world sounds, combat effects

### Engine/Client Animation
- **Purpose:** Generic playback-script engine for timed actions (sounds, spawns, messages)
- **Location:** `src/engine/client/library/clientAnimation`
- **Contains:** `PlaybackScript`, `PlaybackAction`, priority scheduling
- **Depends on:** `sharedFoundation`
- **Used by:** Skeletal animation callbacks, combat effects sequencing

### Engine/Client Skeletal Animation
- **Purpose:** Character mesh deformation, bone controller system (100% in-house, no Granny)
- **Location:** `src/engine/client/library/clientSkeletalAnimation`
- **Contains:** `Skeleton`, bone transforms, rig definition, blending
- **Depends on:** `sharedFoundation`, `sharedFile`, `clientAnimation`
- **Used by:** Character rendering, emote/combat animation playback

### Engine/Client Terrain
- **Purpose:** Terrain rendering, flora placement, chunk management
- **Location:** `src/engine/client/library/clientTerrain`
- **Contains:** `ClientStaticRadialFloraManager`, heightmap sampling, chunk streaming
- **Depends on:** `sharedTerrain`, `sharedFile`, `clientGraphics`
- **Used by:** GroundScene rendering pipeline

### Engine/Client UI (SWG User Interface)
- **Purpose:** XML-driven modal/non-modal UI, page stack, mediator pattern
- **Location:** `src/engine/client/library/clientUserInterface`
- **Contains:** `CuiLoginManager`, `CuiMediatorFactory`, page hierarchy
- **Depends on:** Mozilla XPCOM, `sharedFoundation`, `clientGraphics`
- **Used by:** Login/character select, in-game HUD, terminal windows

### Engine/Server Game
- **Purpose:** Server-side object simulation, combat, persistence, script dispatch
- **Location:** `src/engine/server/library/serverGame`
- **Contains:** `GameServer` (main loop), `CreatureObject`, `TangibleObject`, `Client` (per-player state), attribute managers
- **Depends on:** `sharedNetwork`, `sharedGame`, `sharedTemplate`, `serverScript`, `serverDatabase`
- **Used by:** SwgGameServer entry point, all gameplay systems

### Engine/Server Script
- **Purpose:** JNI bridge to embedded JVM, ScriptDispatch for events (OnAttack, OnLoot, etc.)
- **Location:** `src/engine/server/library/serverScript`
- **Contains:** `JavaLibrary`, `GameScriptObject`, `ScriptMethodsX` (50+ method groups), `ScriptFunctionTable`
- **Depends on:** JDK 1.3 (or compat version), `serverGame`, `sharedGame`
- **Used by:** SwgGameServer event handlers (combat, loot, triggers, state changes)

### Engine/Server Database
- **Purpose:** Oracle OCI adapter, connection pooling, prepared-statement caching
- **Location:** `src/engine/server/library/serverDatabase`
- **Contains:** Oracle-specific persistence, load/save object graphs
- **Depends on:** Oracle OCI SDK, `sharedFoundation`
- **Used by:** SwgDatabaseServer, all persistence operations

### Engine/Server Pathfinding
- **Purpose:** A* navigator, waypoint networks, terrain walkability
- **Location:** `src/engine/server/library/serverPathfinding`
- **Contains:** AStar engine, path queries, mesh representation
- **Depends on:** `sharedFoundation`, terrain data
- **Used by:** NPC/creature AI, player command validation

### Game/Client
- **Purpose:** SWG-specific client logic (login screen, avatar picker, gameplay UI)
- **Location:** `src/game/client/application/SwgClient`, `src/game/client/library/swgClientUserInterface`
- **Contains:** `SwgClient` main entry, login/avatar/world UI pages, cosmetics
- **Depends on:** `clientGame`, `clientUserInterface`, DirectX, Miles, XPCOM
- **Used by:** Player-facing client binary

### Game/Server
- **Purpose:** SWG-specific server logic (creature types, combat rules, quest triggers)
- **Location:** `src/game/server/application/SwgGameServer`, `src/game/server/library/swgServerGame`
- **Contains:** SWG content-specific object classes, command handlers, server-side checks
- **Depends on:** `serverGame`, `serverScript`, `sharedGame`
- **Used by:** SwgGameServer main entry

### Scripts (dsrc/)
- **Purpose:** Gameplay rules authored by content designers
- **Location:** `dsrc/sku.0/sys.server/compiled/game/script/`
- **Contains:** `.script` source (Java-flavoured language) compiled to `.scriptbin`, run by embedded JVM
- **Depends on:** `serverScript` JNI layer
- **Used by:** SwgGameServer event dispatch (OnAttack, OnLoot, OnTrigger, state changes)

### Data Tables (dsrc/)
- **Purpose:** Game balance data (drop tables, AI profiles, badges, schematics, NPC attributes)
- **Location:** `dsrc/sku.0/sys.server/compiled/game/datatables/`
- **Contains:** `.tab` (tab-delimited format), loaded into runtime managers
- **Depends on:** Config-driven loader
- **Used by:** Combat, loot, quests, crafting systems

---

## Data Flow

### Primary Request Path (Client → Server → Client)

1. **Client input** (`src/game/client/application/SwgClient/src/win32/WinMain.cpp`) — player action (move, attack, use item)
2. **GameNetwork encoding** (`src/engine/client/library/clientGame/src/shared/network/GameNetwork.cpp`) — message serialized
3. **UDP dispatch** to ConnectionServer (custom UDP protocol via `src/external/3rd/library/udplibrary`)
4. **ConnectionServer receives** → routes to PlanetServer/SwgGameServer based on object zone
5. **SwgGameServer processes** (`src/engine/server/library/serverGame/src/shared/core/GameServer.cpp`) — main game loop updates object state
6. **Script dispatch** (`src/engine/server/library/serverScript/src/shared/GameScriptObject.cpp`) — fires OnAttack/OnLoot into `.scriptbin` via embedded JVM
7. **Persistence write** (`src/engine/server/library/serverDatabase`) → Oracle (async, batched)
8. **Server broadcasts state** to all observing players via ConnectionServer
9. **Client receives update** → `clientGame` state sync → graphics update → rendered frame

### Login Flow

1. `SwgCuiLoginScreen::ok()` (`src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiLoginScreen.cpp`)
2. `LoginConnection::LoginClientId` message sent to LoginServer
3. LoginServer validates username/password (or stubs to test mode)
4. Returns list of playable clusters (galaxies)
5. `CuiLoginManager::connectToCluster()` (`src/engine/client/library/clientUserInterface/src/shared/core/CuiLoginManager.cpp`) — connects to CentralServer
6. CentralServer returns character list
7. `SwgCuiAvatarSelection` displays choices
8. Player picks character → `CmdStartScene` sent to PlanetServer
9. PlanetServer loads zone snapshot, spawns player object, sends `CmdStartScene` back
10. Client enters GroundScene, terrain + flora load, first frame renders

### State Management

**Server:** `GameServer::update()` loop (real-time tick-based, ~10 Hz typical)
- Polls all objects for movement, ability timers, state machines
- Collects and dispatches network updates
- Triggers event callbacks (OnCollide, OnTrigger, etc.)

**Client:** `Game::run()` loop (graphics sync, ~60 Hz or capped)
- Receives object snapshots from server
- Interpolates transforms between ticks
- Renders current state
- Queues user input for next server frame

**Persistence:** Async write-back from SwgGameServer → SwgDatabaseServer → Oracle
- Batched updates, eventual consistency (not real-time)
- Player logout triggers full state save

---

## Key Abstractions

### ObjectTemplate
- **Purpose:** Defines object class structure (attributes, slots, customizations, appearance)
- **Location:** `dsrc/sku.0/sys.shared/compiled/game/object/` (`.iff` template files), `src/engine/shared/library/sharedTemplate/`
- **Pattern:** Template → Instance graph; both client and server validate against same templates to ensure sync

### TreeFile (Virtual Filesystem)
- **Purpose:** Abstraction over `.tre` archive files (game assets, scripts, templates)
- **Location:** `src/engine/shared/library/sharedFile/src/shared/file/TreeFile.cpp`
- **Pattern:** Both client and server use `TreeFile::exists(filename)` and `TreeFile::getFile()` to load without caring if data is in-repo or packed archives

### ScriptDispatch (Event → Callback)
- **Purpose:** Routes gameplay events to JVM-compiled `.scriptbin`
- **Location:** `src/engine/server/library/serverScript/src/shared/GameScriptObject.cpp`, `ScriptMethodsX.cpp` (50+ method groups)
- **Pattern:** C++ calls `JavaLibrary::dispatchMessage()` with event name + parameters → JVM callback executes → returns result back to C++

### Appearance (Model + Texture Binding)
- **Purpose:** Encapsulates mesh + material setup for both characters and static objects
- **Location:** `clientSkeletalAnimation` (character rigs), `clientTerrain` (flora meshes)
- **Pattern:** Template → AppearanceTemplate → loaded via TreeFile → bound at render time

---

## Entry Points

### Server: SwgGameServer
- **Location:** `src/game/server/application/SwgGameServer/src/linux/main.cpp`
- **Triggers:** TaskManager spawns on startup or restart
- **Responsibilities:**
  1. Load shared/server foundation (thread, debug, config, file, network, math)
  2. Install serverGame core managers
  3. Install serverScript (JVM init)
  4. Load dsrc/ templates, scripts, data tables
  5. Connect to CentralServer, PlanetServer
  6. Enter main loop: `GameServer::update()` (polling objects, dispatching events, networking)

### Server: Core Logic
- **Location:** `src/engine/server/library/serverGame/src/shared/core/GameServer.cpp` (~11 000 lines)
- **Triggers:** Called from main loop in SwgGameServer
- **Responsibilities:** Real-time simulation (movement, ability timers, object lifecycle, zone transitions)

### Client: WinMain
- **Location:** `src/game/client/application/SwgClient/src/win32/WinMain.cpp:124`
- **Triggers:** User double-clicks `SwgClient.exe`
- **Responsibilities:**
  1. Memory cap via `MemoryManager::setLimit()`
  2. Bootstrap foundation (config file read, window creation, registry)
  3. Install graphics/audio/UI subsystems
  4. Initialise Game core
  5. Enter `Game::run()` loop

### Client: 17-Phase Boot Sequence
- **Reference:** `docs/recon/05-client-boot-sequence.md` (comprehensive phase map)
- **Critical milestones:**
  - Phase 3: Foundation (config, window, registry)
  - Phase 6: Override config from TreeFile
  - Phase 7: Graphics, audio, UI subsystems
  - Phase 9: Splash screen visible (MVB-0 target)
  - Phase 15: GroundScene loaded
  - Phase 17: First frame rendered

---

## Anti-Patterns

### Untethered Singleton Managers
**What happens:** Managers like `ObjectIdManager`, `NameManager`, `ResourceTypeManager` live as static-initialised singletons with no clear lifetime ownership. If code tries to access them before `install()` or after `remove()`, crashes result.

**Why it's wrong:** Hard to reason about initialisation order, especially in the multi-threaded server context.

**Better approach:** See `ExitChain::install()` pattern in `src/engine/shared/library/sharedFoundation/` — explicit registration of setup/teardown in dependency order.

### Oracle OCI Direct Calls
**What happens:** Database access scattered across `serverGame` (object save/load) and `serverDatabase`. OCI calls (binding, execute, fetch) are synchronous and block the game loop if network is slow.

**Why it's wrong:** No connection pooling abstraction. Each thread opens its own connection. No query timeout.

**Better approach:** All database ops should go through `serverDatabase` connection pool with async/batch semantics. See `src/engine/server/library/serverDatabase/` for the intended abstraction.

### Direct Object-Graph Mutation During Iteration
**What happens:** Game loop iterates over `CreatureObject` list and deletes objects during iteration (logout, despawn). Iterator invalidation bugs are common.

**Why it's wrong:** Crashes, ghosts, undefined behaviour if another thread or callback reads the list.

**Better approach:** Use a deletion queue (`InstantDeleteList`) — mark for deletion, process in a safe phase after iteration. See `serverGame/InstantDeleteList.cpp`.

### Hardcoded Object IDs / Singletons by ID
**What happens:** Code refers to "player object is always 0x12345678" or "the CentralServer is always NetworkId(1, 0)". If IDs change, everything breaks.

**Why it's wrong:** Fragile, hard to test, impossible to run multiple zones on one machine.

**Better approach:** Use `NamedObjectManager` or `ObjectIdManager` to query by semantic name. See `src/engine/server/library/serverGame/src/shared/core/NamedObjectManager.cpp`.

---

## Cross-Cutting Concerns

**Logging:** Dual system
- **Debug log:** Ring buffer in memory (`sharedDebug/SetupSharedDebug.cpp`), written to file on crash or exit
- **Persistent log:** `sharedLog/LogManager.cpp` writes to disk (server-side), optional rotation

**Validation:** `ObjectTemplate` provides schema. `ContainerInterface` validates item slots. `CommandCppFuncs` validates permission bits before dispatch.

**Authentication:** LoginServer validates username/password (or stubs to open-play mode). Per-object permission checks via bit flags (`PRIVATE`, `MONSTER_ATTACKABLE`, etc.) in `TangibleObject::isAttackable()` style methods.

---

*Architecture analysis: 2026-05-03*
