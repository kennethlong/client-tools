# Phase 6: Launch + Login Handshake — Research

**Researched:** 2026-05-05
**Domain:** CMake `configure_file()` config templating; SWG client boot debugging; SOE proprietary UDP login handshake against community LoginServer
**Confidence:** HIGH

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**client.cfg Template:**
- **D-01:** Master template lives at `src/cmake/config/client.cfg.in` — version-controlled. CMake `configure_file()` substitutes variables and writes resolved file to `build/`; POST_BUILD copies to `bin/Debug/client.cfg` and `bin/Release/client.cfg` (replacing Phase 4 placeholder). One source of truth.
- **D-02:** Machine-specific TRE paths via `-DSWG_INSTALL_DIR` CMake cache variable. Template uses `@SWG_INSTALL_DIR@` placeholders. Default: `D:/Code/SWGSource Client v3.0`. User sets `-DSWG_INSTALL_DIR=<path>` once at configure time; re-run cmake to regenerate.
- **D-03:** LoginServer hardcoded as `127.0.0.1:44453`. User's swg-main VM on loopback. If VM IP changes, user edits `bin/Debug/client.cfg` manually or re-templates.
- **D-04:** Full retail TRE file set listed in `client.cfg.in`. Sourced from `D:\Code\SWGSource Client v3.0\client.cfg`. Missing TREs log warnings, not fatals — using full set is safe.
- **D-05:** `[Station] sessionId` MUST be absent. `[Station] gameFeatures=15` MUST be present. `[ClientGame] loginServerAddress0=127.0.0.1` and `loginServerPort0=44453` MUST be present.
- **D-06:** Dead-service middleware disabled in template (CONFIG-03): Vivox `enableVoiceChat=0`, Bink intro skipped, Logitech G15 LCD `enableG15Lcd=false`. Boot must not depend on any of these.

**Boot Milestone Strategy:**
- **D-07:** Two explicit sequential milestones with human-verify checkpoint between them. MVB-1 (login UI) must pass before VM connection. Smaller blast radius — isolates D3D/asset boot issues from network.
- **D-08:** Server startup is **out of scope** for Phase 6 documentation. User has SWGSource VM startup instructions.
- **D-09:** MVB-1 success = **visual confirmation** (login UI on screen with rendered frames) **AND** specific `[SharedDebug] installTimer=true` log marker that identifies Phase 9 completion. Researcher identifies exact log line.

**Boot Failure Triage:**
- **D-10:** **Hybrid triage protocol** — run standalone first, check `Logs/` directory for `installTimer` phase log output; attach VS 2022 debugger only when log is ambiguous or crash needs precise source location.
- **D-11:** **Early crash fallback** — Windows Event Log + minidump in `bin/Debug/`. `MyUnhandledExceptionFilter` in `sharedFoundation` writes minidump on unhandled exception.
- **D-12:** Phase 6 plan includes **triage runbook** for 3-4 most likely boot failure categories: missing runtime DLL, TRE not found, D3D init failure, config key not found.

**NetworkVersionId Pre-Audit:**
- **D-13:** Researcher pre-audits `NetworkVersionId` in both whitengold's `sharedNetworkMessages` and swg-main source before planning first-run step.
- **D-14:** If mismatch found: accept as minimal source edit (FreeCamera.cpp / SwgCuiAllTargets.cpp precedent) — one-liner fix, documented in `docs/build.md` "known source edits".

### Claude's Discretion

(None explicitly listed — most decisions locked. Researcher recommends within the locked frame.)

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within Phase 6 scope.
</user_constraints>

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| LAUNCH-01 | `client.cfg` template documents `[SharedFile] searchPath_NN` entries pointing at user's retail SWG install; runtime resolves `.tre` archives at startup via Phase 1 TreeFile system | §D-04 TRE Search Path Inventory captures the exact 50+ key list from `D:/Code/SWGSource Client v3.0/client.cfg`. §TreeFile loader semantics confirms `searchTree_NN_N`, `searchTOC_NN_N`, `searchPath_NN_N` are all supported by the same loader (TreeFile.cpp:118-148) |
| LAUNCH-03 | `SwgClient_d.exe` boots far enough to render login UI — Phase 9 of 17-phase boot sequence completes | §Phase 9 Boot Marker (D-09) identifies the exact `InstallTimer:` log signature and corrects the boot doc's incorrect config-key claim |
| LAUNCH-04 | Client completes LoginServer handshake against swg-main VM and reaches character-select — `LoginClientId → LoginEnumCluster → EnumerateCharacterId` round-trips successfully | §NetworkVersionId Pre-Audit (D-13) + §Auth Bypass Confirmation (D-05) confirm zero protocol-level risk; LoginConnection wire format identical to swg-main |
| CONFIG-01 | `[Station] sessionId` is unset in shipped client config — plain user/password auth via swg-main's LoginServer; dead SOE Station SSO not invoked | §LoginConnection Code Trace shows the exact branch that activates SSO; absence of `sessionId` key takes plain user/password path |
| CONFIG-02 | `[Station] gameFeatures=15` (all four SKU bits) and other VM-aligned settings | §SKU Bit Math from boot doc Phase 5 confirms 15 = 0b1111 = Base+JTL+RotW+ToOW; user's reference config uses `gameFeatures=65535` (all bits) |
| CONFIG-03 | Optional dead-service middleware disabled via config — Vivox voice off, Bink intro skipped, Logitech G15 LCD disabled | §Dead-Service Disable Keys confirms exact key names: `[ClientGame] skipIntro=true`, `[ClientGame] disableCutScenes=true`, `[ClientUserInterface] disableG15Lcd=true`. Vivox runtime install is not config-gated; voice manager runs but stays unused without server |
</phase_requirements>

---

## Summary

Phase 6 has zero meaningful technical risk left after Phase 5's pre-flight work. **All four high-risk unknowns are now resolved:**

1. **Wire format risk: cleared.** `NetworkVersionId` is identical between whitengold and swg-main — both `"20100225-17:43"` ([VERIFIED: D:/Code/swg-client/src/engine/shared/library/sharedNetworkMessages/src/shared/common/GameNetworkMessage.cpp:21] and [VERIFIED: D:/Code/swg-main/src/engine/shared/library/sharedNetworkMessages/src/shared/common/GameNetworkMessage.cpp:21]). D-14 source edit is **not needed.**
2. **Auth bypass: cleared.** swg-main's `validateClient()` accepts any credentials when `externalAuthURL=""` (default). Client must avoid `[Station] sessionId`. Per `docs/recon/08-phase6-preflight.md` P2.03.
3. **TRE search path list: enumerated.** User's reference config (`D:/Code/SWGSource Client v3.0/client.cfg`) is a 13-line bootstrap that pulls in `login.cfg`, `live.cfg`, `preload.cfg`, `options.cfg`, `user.cfg` via `.include` — total 8 distinct config keys are needed for a working SWGSource bootstrap. **The full per-SKU TRE inventory uses `searchTree_NN_N=` (TRE filenames inside SWG_INSTALL_DIR) plus `searchTOC_NN_N=` (table-of-contents files), not `searchPath_NN_N=` (filesystem paths).** This distinction matters for the template.
4. **Phase 9 log marker: identified.** The actual config key that activates per-phase install timing is `[SharedDebug/InstallTimer] enabled=true` — NOT `[SharedDebug] installTimer=true` as written in the boot doc ([VERIFIED: src/engine/shared/library/sharedDebug/src/shared/InstallTimer.cpp:34]). The Phase 9 marker is `InstallTimer:  <time> <bytes> Game::install` at outer indent (depth 1).

**Primary recommendation:** Author `client.cfg.in` as a single self-contained template (no `.include` chains; the user's SWGSource layered approach is a runtime convenience, not a requirement). Use CMake `configure_file()` with `@ONLY` placeholder substitution (the project's established pattern, already used by the Phase 1 time_t probe). Update the SwgClient POST_BUILD command to copy the generated file from `${CMAKE_CURRENT_BINARY_DIR}/client.cfg` instead of the Phase 4 placeholder. Plan two milestones (MVB-1 = login UI, MVB-3 = character select) with a human-verify checkpoint between them.

---

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| TRE search-path resolution | Frontend client (`sharedFile/TreeFile`) | — | Boot Phase 5 reads `[SharedFile] searchPath/searchTree/searchTOC` keys and mounts archives. Pure client-side; no server interaction. |
| LoginServer address resolution | Frontend client (`SwgCuiLoginScreen::ok` → `LoginConnection`) | — | Reads `[ClientGame] loginServerAddress0..N` + `loginServerPort0..N`; opens UDP to swg-main VM. |
| Authentication | Frontend client (`LoginConnection::onConnectionOpened`) → API/Backend (swg-main `ClientConnection.cpp`) | — | Client sends `LoginClientId(username, password, version)`; server's `validateClient()` accepts unconditionally when `externalAuthURL=""`. |
| Cluster enumeration | API/Backend (LoginServer) → Frontend client (`CuiLoginManager`) | — | Server-pushed `LoginEnumCluster`/`EnumerateCharacterId` messages populate `CuiLoginManager` static maps. Client trusts server. |
| Build-time config templating | Build system (CMake `configure_file()`) | POST_BUILD staging | `@SWG_INSTALL_DIR@` substitution at configure time; POST_BUILD copies result alongside exe. Same tier as Phase 4 DLL staging. |
| Boot phase tracing | Frontend client (`sharedDebug/InstallTimer`) | — | `[SharedDebug/InstallTimer] enabled=true` activates per-phase log lines via `REPORT_LOG_PRINT`. Pure client diagnostic. |

---

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| CMake `configure_file()` | 3.27+ (project minimum) | Substitute `@VAR@` placeholders at configure time, write resolved file to `${CMAKE_CURRENT_BINARY_DIR}` | Native CMake feature; project already uses it for `time_t_probe.cpp.in` (sharedDebug/sharedFoundation/sharedNetwork CMakeLists). [VERIFIED: src/engine/shared/library/sharedDebug/src/CMakeLists.txt:70] |
| CMake `add_custom_command(TARGET ... POST_BUILD)` | 3.27+ | Copy generated config alongside exe per build configuration | Project's established staging pattern from Phase 4 Plan 05. [VERIFIED: src/game/client/application/SwgClient/src/CMakeLists.txt:187] |
| CMake `option()` / `set(... CACHE STRING)` | 3.27+ | Expose `SWG_INSTALL_DIR` to user at configure time | CMake-native; reconfigure-time changeable; matches project convention from Phase 1 (`WHITENGOLD_USE_STLPORT_HEADERS`) [VERIFIED: src/CMakeLists.txt:62] |

### Supporting

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `sharedFile/TreeFile.cpp` | repo-internal | Reads `searchPath_NN_N`, `searchTree_NN_N`, `searchTOC_NN_N` keys at boot | Already wired; Phase 6 only authors the config that this code consumes. [VERIFIED: src/engine/shared/library/sharedFile/src/shared/TreeFile.cpp:118-148] |
| `sharedFoundation/ConfigFile` | repo-internal | INI parser; supports `.include "other.cfg"` directives, multi-value keys via `getKeyString(section, key, index)` | Existing infrastructure; Phase 6 uses it via stock keys |
| `sharedDebug/InstallTimer` | repo-internal | Per-phase install timing log via `[SharedDebug/InstallTimer] enabled=true` | Phase 6 enables this in the template to verify Phase 9 reach |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Single self-contained `client.cfg.in` | Layered `.include` chain (login.cfg / live.cfg / preload.cfg / options.cfg / user.cfg, mirroring SWGSource layout) | The SWGSource layered approach exists because **users edit user.cfg without touching tracked files**. For the whitengold template, the user is also the developer — single file is simpler, reviewable as one diff, no extra files to template. **Recommendation: single-file template.** |
| `configure_file()` | Hand-written `client.cfg` checked into repo with hardcoded paths | Rejected by D-02; user wants `-DSWG_INSTALL_DIR` reconfigurability |
| `configure_file()` | CMake `file(GENERATE ...)` with generator expressions | `file(GENERATE)` is per-config (Debug/Release split), but our template doesn't depend on config — single-pass `configure_file` is correct. `file(GENERATE)` would only matter if we needed `$<CONFIG>` in the cfg content (we don't). |
| POST_BUILD `copy_if_different` | `install(FILES ...)` rule | The project explicitly avoids `install()` per Phase 4 Plan 05 D-04 (no install rules in v1). Stay consistent. |

**Installation:**
No new packages. All capabilities are CMake-native or already present in the repo.

**Version verification:**
N/A — no external packages introduced. CMake version is the project's existing 3.27+ requirement [VERIFIED: src/CMakeLists.txt:1].

---

## Architecture Patterns

### System Architecture Diagram

```
                          ┌─────────────────────────────────────┐
                          │  cmake -S src -B build              │
                          │     -DSWG_INSTALL_DIR=<path>        │
                          └────────────────┬────────────────────┘
                                           │ (configure phase)
                                           ▼
              src/cmake/config/client.cfg.in
              ┌────────────────────────────────────────┐
              │   @SWG_INSTALL_DIR@/sku0_client.toc    │
              │   @SWG_INSTALL_DIR@/sku1_client.toc    │
              │   loginServerAddress0=127.0.0.1        │
              │   loginServerPort0=44453               │
              │   gameFeatures=15                      │
              │   [SharedDebug/InstallTimer] enabled=1 │
              └────────────────┬───────────────────────┘
                               │ configure_file(@ONLY)
                               ▼
              build/<dir>/client.cfg  (resolved)
                               │
                               │ add_custom_command(POST_BUILD)
                               ▼
        ┌───────────────────────────────────────────────┐
        │  bin/Debug/client.cfg                         │
        │  bin/Release/client.cfg                       │
        │   (alongside SwgClient_d.exe / SwgClient_r.exe│
        │    + 30 staged DLLs from Phase 4 Plan 05)     │
        └─────────────┬─────────────────────────────────┘
                      │
                      │ user runs SwgClient_d.exe
                      ▼
        ┌───────────────────────────────────────────────┐
        │  SwgClient WinMain → ClientMain               │
        │   Phase 1-3: process / thread / foundation    │
        │   Phase 4-6: subsystems (zlib/PCRE/file/...)  │
        │   Phase 5: TreeFile mounts .tre archives via  │
        │            searchTree_NN_N keys               │
        │   Phase 7: D3D9 init + Mozilla stub init      │
        │   Phase 8: Game::install (50+ managers)       │
        │   Phase 9: CuiMediatorFactory::activate(      │
        │              Splash/Backdrop)                 │
        │            ─── MVB-1 reached ───              │
        └───────────────┬───────────────────────────────┘
                        │ user types user/pass + clicks OK
                        ▼
        ┌───────────────────────────────────────────────┐
        │  SwgCuiLoginScreen::ok                        │
        │   reads loginServerAddress0/Port0             │
        │   GameNetwork::connectLoginServer(addr, port) │
        └───────────────┬───────────────────────────────┘
                        │ UDP open
                        ▼
        ┌───────────────────────────────────────────────┐
        │  LoginConnection::onConnectionOpened          │
        │   sessionIdKey=null → sendUserName=true       │
        │   send LoginClientId(name, pass, version)     │
        │     where version = NetworkVersionId          │
        │            = "20100225-17:43"                 │
        └───────────────┬───────────────────────────────┘
                        │ UDP wire (identical to swg-main)
                        ▼
        ┌───────────────────────────────────────────────┐
        │  swg-main VM LoginServer::validateClient      │
        │   externalAuthURL == "" → authOK = 1          │
        │   replies LoginEnumCluster +                  │
        │           EnumerateCharacterId                │
        └───────────────┬───────────────────────────────┘
                        │ UDP back to client
                        ▼
        ┌───────────────────────────────────────────────┐
        │  CuiLoginManager populates cluster + char maps│
        │  CuiAvatarSelection mediator activates        │
        │            ─── MVB-3 reached ───              │
        └───────────────────────────────────────────────┘
```

### Recommended Project Structure

```
src/cmake/
├── config/
│   └── client.cfg.in        # NEW: template for runtime client config
├── stubs/
│   ├── libMozilla_stub.cpp     (existing, Phase 3)
│   ├── SwgVideoCapture_stub.cpp (existing, Phase 3)
│   └── time_t_probe.cpp.in     (existing, Phase 1)
└── win32/                       (12 Find modules)

src/game/client/application/SwgClient/
├── CMakeLists.txt           (unchanged — just calls add_subdirectory(src))
├── client.cfg               # OLD Phase 4 placeholder — DELETE in Phase 6
└── src/
    └── CMakeLists.txt       # MODIFIED: add configure_file + update POST_BUILD
```

### Pattern 1: configure_file with @ONLY (project precedent)
**What:** Read `.in` template, substitute `@VAR@` placeholders, write to build directory.
**When to use:** Template files where CMake should ONLY substitute `@VAR@` (not `${VAR}`). This is what Phase 1 already uses for `time_t_probe.cpp.in`.
**Example:**
```cmake
# Source: src/engine/shared/library/sharedDebug/src/CMakeLists.txt:70-74
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/stubs/time_t_probe.cpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/time_t_probe.cpp
    @ONLY
)
```

For Phase 6, in `src/game/client/application/SwgClient/src/CMakeLists.txt`:
```cmake
# Phase 6: generate client.cfg from template at configure time
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/config/client.cfg.in
    ${CMAKE_CURRENT_BINARY_DIR}/client.cfg
    @ONLY
)
```

### Pattern 2: SWG_INSTALL_DIR cache variable
**What:** User-overridable path on cmake command line, persisted in CMakeCache.txt.
**When to use:** When the resolved value is per-user or per-machine and should not be hardcoded.
**Example:**
```cmake
# Add to top of src/CMakeLists.txt (or just before add_subdirectory(game))
set(SWG_INSTALL_DIR "D:/Code/SWGSource Client v3.0" CACHE PATH
    "Path to SWGSource retail client install (TRE archives root). \
     Override with -DSWG_INSTALL_DIR=<path>.")

if(NOT EXISTS "${SWG_INSTALL_DIR}")
    message(WARNING
        "SWG_INSTALL_DIR='${SWG_INSTALL_DIR}' does not exist. \
         The generated client.cfg will reference a missing tree. \
         Set -DSWG_INSTALL_DIR=<path> to your SWG install root.")
endif()
```

The `CACHE PATH` (not just `CACHE STRING`) gets a folder-picker in cmake-gui; `set(... CACHE PATH ...)` does NOT overwrite a value already in the cache, so user's `-DSWG_INSTALL_DIR=...` survives reconfigures.

### Pattern 3: POST_BUILD update — copy_if_different generated config
**What:** Replace the Phase 4 placeholder line with one referencing the configure_file output.
**When to use:** Always — Phase 4 Plan 05 D-05 staged a placeholder; Phase 6 replaces it.
**Example (current Phase 4 line, to be REPLACED):**
```cmake
# CURRENT — src/game/client/application/SwgClient/src/CMakeLists.txt:250-252
COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/../client.cfg"
    "$<TARGET_FILE_DIR:SwgClient>/client.cfg"
```

**Phase 6 replacement:**
```cmake
COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_BINARY_DIR}/client.cfg"
    "$<TARGET_FILE_DIR:SwgClient>/client.cfg"
```

The `${CMAKE_CURRENT_BINARY_DIR}` here resolves to `build/game/client/application/SwgClient/src/` (where `configure_file` writes its output). Note: `CMAKE_CURRENT_BINARY_DIR` inside POST_BUILD refers to the *target's* binary dir, which is the same dir where `configure_file()` was invoked. Confirmed safe.

### Anti-Patterns to Avoid

- **Hardcoding TRE absolute paths in the tracked template.** D-02 explicitly forbids this — use `@SWG_INSTALL_DIR@` placeholders. Hardcoded paths break the second user (or VM rebuild) immediately.
- **Hardcoding SWG_INSTALL_DIR via `set()` without `CACHE`.** A non-cache `set()` inside `CMakeLists.txt` overrides the user's `-D` flag every reconfigure. Always use `set(... CACHE PATH ...)`.
- **Using `${VAR}` syntax in `client.cfg.in` template content.** With `@ONLY`, only `@VAR@` is substituted; `${VAR}` is left literal. Other CMake configure_file modes substitute both, but mixing modes confuses future maintainers. **Use `@VAR@` exclusively.**
- **Replicating the SWGSource `.include` chain.** Their layered cfg exists so non-developer users can edit `user.cfg` without touching tracked files. For our template, the same `cmake -DSWG_INSTALL_DIR=…` ergonomics handle that — keep it one file.
- **Setting `[ClientGame] loginServerAddress=` (without index).** The actual key the code reads is `loginServerAddress0`, `loginServerAddress1`, etc. (zero-indexed list). `SwgCuiLoginScreen::ok` walks the index `0..N` until it finds no key. [VERIFIED: src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiLoginScreen.cpp:249]
- **Putting `[Station] sessionId=` in the template (even commented).** Per D-05; the parser ignores commented lines, but we want the documented intent to be obvious to the reader.
- **Trusting the boot doc's `[SharedDebug] installTimer=true` claim.** The actual key is `[SharedDebug/InstallTimer] enabled=true` ([VERIFIED: src/engine/shared/library/sharedDebug/src/shared/InstallTimer.cpp:34]). The boot doc has a typo. Use the verified key in the template.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Per-user paths in tracked config | A `gitignored` `client.cfg` checked in via `.gitignore` exception with manual edits | CMake `configure_file()` + `SWG_INSTALL_DIR` cache var | Reproducible from clean clone; documented in CMakeCache.txt; survives `git clean` |
| INI parsing in custom code | Custom config layer above `ConfigFile` | Existing `sharedFoundation/ConfigFile` | Already handles `.include` chains, multi-value keys via index, comment characters, section namespacing. Boot doc Phase 3 confirms it's load-bearing. |
| TRE archive enumeration | Manual `glob` of `*.tre` files at boot | `searchTree_NN_N` keys | Already-shipped loader handles per-SKU split, priority ordering, missing-file warnings. Hand-rolling glob would lose the priority semantics. |
| Boot phase progress reporting | Custom `printf` instrumentation | `[SharedDebug/InstallTimer] enabled=true` | Already instruments every `Setup*::install` call site with depth-aware indenting. The 17-phase trace IS the InstallTimer output. |
| Validating LoginServer reachability before connect | Custom UDP ping before `LoginConnection` | The existing `LoginConnection::onConnectionClosed` callback | Connection-fails-silently is the existing UX; adding a pre-flight check adds two failure modes. The login form already shows "connection lost" on timeout. |
| Custom auth bypass for community servers | Local stub auth server alongside whitengold | swg-main VM's `externalAuthURL=""` default | Already done by SWGSource StellaBellum upstream. Adding our own bypass duplicates server logic for no gain. |

**Key insight:** Phase 6 is **almost entirely configuration**. The hardest engineering work was done in Phases 1-5: CMake authoring, library link closure, DLL staging. The remaining task is *telling the binary which paths to load and which IP to connect to*. Resist the urge to add abstraction layers; this is a config-authoring + boot-debug phase.

---

## Code Examples

Verified patterns from official sources.

### configure_file() with @ONLY (existing project precedent)
```cmake
# Source: D:/Code/swg-client/src/engine/shared/library/sharedDebug/src/CMakeLists.txt:70-74
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/stubs/time_t_probe.cpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/time_t_probe.cpp
    @ONLY
)
```

### Cache variable with default (existing project precedent)
```cmake
# Source: D:/Code/swg-client/src/CMakeLists.txt:62
option(WHITENGOLD_USE_STLPORT_HEADERS
    "Place vendored STLPort headers before MSVC headers" ON)
```

For Phase 6, prefer `CACHE PATH` (vs `option()`'s BOOL):
```cmake
set(SWG_INSTALL_DIR "D:/Code/SWGSource Client v3.0" CACHE PATH
    "SWGSource retail client install root (TRE archives + DLLs).")
```

### POST_BUILD copy_if_different (existing Phase 4 pattern)
```cmake
# Source: D:/Code/swg-client/src/game/client/application/SwgClient/src/CMakeLists.txt:250-253
COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/../client.cfg"
    "$<TARGET_FILE_DIR:SwgClient>/client.cfg"
VERBATIM
```

### Sample client.cfg.in skeleton
```ini
# Source: synthesised from D:/Code/SWGSource Client v3.0/{client,login,live,preload,options,user}.cfg
# This file is generated from src/cmake/config/client.cfg.in via CMake configure_file().
# DO NOT edit the staged copy in bin/<config>/client.cfg — re-run cmake to regenerate.

[SharedFile]
    maxSearchPriority=12

    # Per-SKU TOC files (table-of-contents, points at .tre archives by name)
    searchTOC_03_3=@SWG_INSTALL_DIR@/sku3_client.toc
    searchTOC_02_2=@SWG_INSTALL_DIR@/sku2_client.toc
    searchTOC_01_1=@SWG_INSTALL_DIR@/sku1_client.toc
    searchTOC_00_0=@SWG_INSTALL_DIR@/sku0_client.toc

    # Per-SKU TRE search trees (specific archives by filename, priority 0)
    searchTree_00_8=@SWG_INSTALL_DIR@/swgsource_3.0.tre
    searchTree_00_7=@SWG_INSTALL_DIR@/disable_wayfar_dearic_snow.tre

[Station]
    # CONFIG-01: sessionId is INTENTIONALLY OMITTED.
    # Setting it activates the dead SOE Station SSO path; do not add it.
    gameFeatures=15
    subscriptionFeatures=1

[ClientGame]
    # LAUNCH-01 / D-03: VM connection
    loginServerAddress0=127.0.0.1
    loginServerPort0=44453

    # CONFIG-03: skip Bink intro + cinematics for faster boot to login UI
    skipIntro=true
    skipSplash=true
    disableCutScenes=true

[ClientUserInterface]
    # CONFIG-03: disable Logitech G15 LCD support
    disableG15Lcd=true

[SharedDebug/InstallTimer]
    # D-09: per-phase install timing — needed for MVB-1 verification.
    # Logs each Setup*::install duration to the SwgClient log file.
    enabled=true

[SwgClient]
    allowMultipleInstances=true

[SharedFoundation]
    # Override registry path so this private build does not collide with
    # an installed retail SWG (boot doc Phase 3 footnote).
    ProductRegistryPath=Software/whitengold/Default
```

(This is a **research-grade skeleton**, not the final template. The plan-author owns the final key list, but every key shown here is verified from one of: user's reference config, repo source, or the boot doc.)

### LoginConnection wire format (informational — what the client sends)
```cpp
// Source: D:/Code/swg-client/src/engine/client/library/clientGame/src/shared/network/LoginConnection.cpp:53-79
void LoginConnection::onConnectionOpened()
{
    GameNetworkConnection::onConnectionOpened();
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
}
```

Note: PRODUCTION=0 (Debug build) sends `RequestExtendedClusterInfo` — **prefer Debug build for first-run** because the response carries extra cluster metadata useful for diagnosis.

---

## TRE Search Path Inventory (D-04)

> Captured from `D:/Code/SWGSource Client v3.0/client.cfg` and its `.include` chain — the user's working SWGSource bootstrap. **All paths are relative to `@SWG_INSTALL_DIR@`** which substitutes to the user's install root at configure time.

### From `client.cfg` (root, `[SharedFile]`)
| Key | Value | Notes |
|-----|-------|-------|
| `searchTree_00_8` | `swgsource_3.0.tre` | Priority 8, SKU 0; SWGSource overlay TRE (community content patches) |

### From `live.cfg` (`[SharedFile]`)
| Key | Value | Notes |
|-----|-------|-------|
| `maxSearchPriority` | `12` | Caps the priority-loop range in `TreeFile::install` (default 20) |
| `searchTree_00_7` | `disable_wayfar_dearic_snow.tre` | Disables snow on Wayfar/Dearic for visual consistency |
| `searchTOC_03_3` | `sku3_client.toc` | Trials of Obi-Wan TOC (SKU 3, priority 3) |
| `searchTOC_02_2` | `sku2_client.toc` | Rage of the Wookies TOC (SKU 2, priority 2) |
| `searchTOC_01_1` | `sku1_client.toc` | Jump to Lightspeed TOC (SKU 1, priority 1) |
| `searchTOC_00_0` | `sku0_client.toc` | Base game TOC (SKU 0, priority 0) |

**Live.cfg also contains commented-out ILM TREs (`ILM_animation.tre`, `ILM_visuals.tre`, etc.) — these are visual mods. Do NOT enable in template; user can uncomment manually.**

### Key insight: TOC-driven, not path-driven

Unlike the whitengold internal `exe/win32/common.cfg` (which uses `searchPath_NN_N=` pointing at filesystem directories like `../../data/sku.0/sys.shared/exported/...`), the SWGSource production client uses `searchTOC_NN_N=` which point to `.toc` files. A `.toc` is a manifest that lists the TRE archives for that SKU — the loader reads the `.toc` and mounts each TRE listed inside.

**Implication for the template:** We need **6 keys total** for a working bootstrap, not 50+:
- 4 × `searchTOC_NN_N=@SWG_INSTALL_DIR@/skuN_client.toc` (one per SKU)
- 1 × `searchTree_00_8=@SWG_INSTALL_DIR@/swgsource_3.0.tre` (SWGSource overlay)
- 1 × `searchTree_00_7=@SWG_INSTALL_DIR@/disable_wayfar_dearic_snow.tre` (snow disable; optional but tracked in user's known-good config)
- 1 × `maxSearchPriority=12`

**No `searchPath_NN_N=` keys are needed** — the user's config does not use them. The internal `exe/win32/common.cfg` is a SOE *build-time* config (referencing `data/sku.N/sys.client/...` directories that exist before TRE packaging). The retail client uses `searchTOC` instead.

### From `preload.cfg` (`[PreloadedAssets]`)

The user's `preload.cfg` lists ~30 `texture=`, ~16 `objectTemplate=`, 1 `shaderTemplate=`, 1 `appearanceTemplate=` entries. **These are runtime preload hints, not search paths.** The `SetupSharedUtility` Phase 4 install reads `[PreloadedAssets]` and warms the asset cache.

**Recommendation:** Do not include `[PreloadedAssets]` in the v1 template. The login UI (MVB-1) does not require any preloaded assets — boot will succeed without them. If the user reports first-frame stuttering after MVB-3 success, add the preload section in a follow-up iteration. Keeps the template minimal.

### From `options.cfg` (auto-generated by `SwgClientSetup_r.exe`)

```ini
[ClientAudio] soundProvider="5.1 Speakers"
[ClientGame] skipIntro=1
[ClientGraphics] useHardwareMouseCursor=1, screenWidth=1920, screenHeight=1080,
                  useSafeRenderer=0, rasterMajor=7
[Direct3d9] fullscreenRefreshRate=75
[SharedUtility] cache=misc/cache_large.iff
```

**Recommendation:** Include `[ClientGame] skipIntro=true` in the template (CONFIG-03). Do NOT hardcode `screenWidth=1920` / `screenHeight=1080` — the boot doc Phase 7 default is 1024x768 (windowed), which is less likely to cause D3D init failures on smaller monitors. User can override in the staged file post-launch via SwgClientSetup-style flow. **`useSafeRenderer=0` and `rasterMajor=7` are graphics-tuning hints** — leave defaults to reduce blast radius.

---

## Phase 9 Boot Marker (D-09)

### The actual config key

The boot doc claims `[SharedDebug] installTimer=true` activates per-phase timing. **This is incorrect.** The verified config key is:

```ini
[SharedDebug/InstallTimer]
    enabled=true
```

[VERIFIED: src/engine/shared/library/sharedDebug/src/shared/InstallTimer.cpp:34]
```cpp
ms_enabled = ConfigFile::getKeyBool("SharedDebug/InstallTimer", "enabled", false);
```

### The exact log marker

`InstallTimer::manualExit()` emits via:
```cpp
REPORT_LOG_PRINT(ms_enabled, ("InstallTimer:%*c%6.4f %d %s\n",
    ms_indent * 2, ' ',
    m_performanceTimer.getElapsedTime(),
    static_cast<int>(endingNumberOfBytesAllocated - m_startingNumberOfBytesAllocated),
    m_description));
```

So a typical line looks like:
```
InstallTimer:    0.0234 1234567 SetupClientGraphics::install
```
- `%*c` = indent spaces (`ms_indent * 2`)
- `%6.4f` = elapsed seconds, 4 decimal places
- `%d` = bytes allocated during install (signed)
- `%s` = description string passed to constructor

### Phase 9 completion = `Game::install` done

`Game::install` ([src/engine/client/library/clientGame/src/shared/core/Game.cpp:692]) is wrapped in:
```cpp
InstallTimer const installTimer("Game::install");
```

Phase 9 (Splash + Backdrop activation, lines 961-964) happens *inside* `Game::install`, just before the function returns. The `InstallTimer` destructor runs as `Game::install` exits, emitting:

```
InstallTimer:  <time>  <bytes>  Game::install
```

(at indent `ms_indent * 2 = 2` spaces — the second-outermost level, since `rootInstallTimer` at depth 1 is the parent.)

**MVB-1 verification criterion:**
1. **Visual:** SwgClient_d.exe window shows the SWG splash + login backdrop with the user/password prompt rendered at 1024x768 (default).
2. **Log:** the `Logs/SwgClient_*.log` file contains a line matching `InstallTimer:  *Game::install` near the end of the boot trace, before any login attempt.

### Where the log file lives

`SetupSharedLog::install("SwgClient")` is called from ClientMain.cpp:246. The log filename prefix is `"SwgClient"`. The actual output directory depends on `[SharedLog]` config and the working directory; typical is `Logs/SwgClient_YYYY-MM-DD-HH-MM-SS.log` relative to CWD. Phase 6 plan should verify the actual log path on first boot via `dir /s SwgClient*.log`.

---

## NetworkVersionId Pre-Audit (D-13 + D-14)

### Verification

| Repo | File | Line | Value |
|------|------|------|-------|
| whitengold | `src/engine/shared/library/sharedNetworkMessages/src/shared/common/GameNetworkMessage.cpp` | 21 | `"20100225-17:43"` |
| swg-main | `D:/Code/swg-main/src/engine/shared/library/sharedNetworkMessages/src/shared/common/GameNetworkMessage.cpp` | 21 | `"20100225-17:43"` |

[VERIFIED: side-by-side grep, 2026-05-05]

### Decision

**D-13 risk: CLEARED.** Versions are byte-identical.
**D-14 source edit: NOT NEEDED.** No `FreeCamera.cpp`-style one-liner required.

The Phase 5 P2.01 finding ("8 files in `clientLoginServer/` are byte-for-byte identical") combined with this `NetworkVersionId` match means the wire format is confirmed identical. There is **zero protocol-level divergence** between the whitengold client binary and a swg-main LoginServer.

### Override path (informational)

`LoginClientId` constructor reads:
```cpp
// Source: src/engine/shared/library/sharedNetworkMessages/src/shared/clientLoginServer/ClientLoginMessages.cpp:21
version(ConfigVersion ? ConfigVersion : NetworkVersionId)
```
where:
```cpp
#define ConfigVersion ConfigFile::getKeyString("SharedNetworkMessages", "version", 0)
```

If the user ever wants to test with a custom version (e.g., to confirm `LoginIncorrectClientId` behaviour), they can set `[SharedNetworkMessages] version="something-else"` in `client.cfg`. **The template should NOT set this key** — falling back to `NetworkVersionId` ensures wire compatibility.

---

## Auth Bypass Confirmation (D-05)

### Client-side decision tree (boot doc Phase 11 + verified code)

```cpp
// Source: src/engine/client/library/clientGame/src/shared/network/LoginConnection.cpp:53-69
void LoginConnection::onConnectionOpened() {
    CuiLoginManager::setOverrideSessionIdKey(std::string());  // CLEAR override
    bool sendUserName = true;
    if (CuiLoginManager::getSessionIdKey() && !ConfigClientGame::getEnableAdminLogin())
        sendUserName = false;
    LoginClientId id(sendUserName ? GameNetwork::getUserName() : "",
                     GameNetwork::getUserPassword());
    send(id, true);
}
```

**Three paths:**

1. **Plain user/pass (THE ONE WE WANT):** `getSessionIdKey()` returns nullptr (because `[Station] sessionId` is unset). `sendUserName = true`. Client sends `LoginClientId(<typed username>, <typed password>, "20100225-17:43")`.

2. **Admin login override:** If `[ClientGame] enableAdminLogin=true` AND a sessionId is set, sends username from `GameNetwork::getUserName()`. Out of scope; do not enable.

3. **Dead Station SSO:** If `[Station] sessionId` is set AND admin login is OFF, sends *empty username* with the password field (the LoginServer is supposed to recognise the user via session token). **This path is dead** — the swg-main LoginServer expects username/password and would either reject or accept-with-an-empty-username. Per D-05, do not activate.

### Server-side validation (P2.03 from `docs/recon/08-phase6-preflight.md`)

When `externalAuthURL=""` (the StellaBellum default):
```
authOK = 1  (unconditional)
```

Any username + any password is accepted. The username is hashed (`std::hash<std::string>`) to derive a `stationId` (uint32) for the session.

**Verification status: HIGH confidence — direct read of swg-main `ClientConnection.cpp` and `ConfigLoginServer.cpp` at commit `91f0357` (2020-08-26) per Phase 5 Plan 03.**

### What the client does NOT need

- The client does not need to know that auth is bypassed.
- The client does not need to send any special "bypass" flag.
- The client needs **only** the absence of `[Station] sessionId` and a valid `[ClientGame] loginServerAddress0`/`loginServerPort0` pair.

### Phase 6 client.cfg auth-related keys (final)

```ini
[Station]
# DO NOT add sessionId. Adding it activates the dead SOE Station SSO path
# in LoginConnection::onConnectionOpened (sendUserName=false branch).
gameFeatures=15
subscriptionFeatures=1

[ClientGame]
loginServerAddress0=127.0.0.1
loginServerPort0=44453
# loginClientID and loginClientPassword may be auto-filled if user wants;
# blank means user types into the login form.
loginClientID=
loginClientPassword=
```

---

## Dead-Service Disable Keys (CONFIG-03)

### Verified config keys (NOT what D-06 says)

D-06 in CONTEXT.md lists `enableVoiceChat=0`, "Bink intro skipped", `enableG15Lcd=false`. **Two of these key names are wrong.** Verified from source:

| D-06 claim | Verified key | Section | Default |
|------------|-------------|---------|---------|
| `enableVoiceChat=0` | **No such key** — Vivox install always runs unconditionally; `voiceChatEnabled` is a CuiPreferences user-pref (not a boot-time config key). [VERIFIED: src/engine/client/library/clientUserInterface/src/shared/core/CuiVoiceChatManager.cpp:601 + CuiPreferences.cpp:390] | n/a | n/a |
| Bink intro skipped | `skipIntro=true` | `[ClientGame]` | `false` (intro plays) |
| (additional) | `disableCutScenes=true` | `[ClientGame]` | `false` |
| `enableG15Lcd=false` | `disableG15Lcd=true` | `[ClientUserInterface]` | `false` |

[VERIFIED: src/engine/client/library/clientGame/src/shared/core/ConfigClientGame.cpp:993,994,1006]
[VERIFIED: src/engine/client/library/clientUserInterface/src/shared/core/ConfigClientUserInterface.cpp:244]
[VERIFIED: src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiG15Lcd.cpp:324]

### Vivox: install runs but does nothing harmful at boot

`CuiManagerManager::install` calls `CuiVoiceChatManager::install` (line 105) which calls `SwgVivox::install()`. This in turn loads Vivox APIs from the staged DLLs (`vivoxsdk.dll`, `vivoxplatform.dll`, both already in `bin/<config>/`). Without a Vivox server connection, the manager just spins idle.

**No `client.cfg` key disables this at boot time.** The `voiceChatEnabled` user preference in `CuiPreferences` (line 390) controls runtime use, not install. For Phase 6 this is fine — boot completes regardless. If the user reports voice-related crashes during MVB-1, that's a separate issue (not config-fixable).

### Bink intro: handled via skipIntro

`Intro::Intro` (Phase 9 Splash playback) checks `ConfigClientGame::getSkipIntro()`. When true, it sets `m_state = S_end` and never calls `load("scene/intro.iff")`. The `scene/intro.iff` asset lives in a TRE archive — if not present, `load()` would fail. **Skipping intro reduces TRE asset dependency at MVB-1 reach.** Recommendation: keep `skipIntro=true` in template.

### G15 LCD: disabled cleanly

`SwgCuiG15Lcd::initializeLcd` (line 321-324) returns early if `ConfigClientUserInterface::getDisableG15Lcd()` is true. The whitengold `lcdui_src` library still links (Phase 4), but never calls into `lgLcd.dll` runtime APIs.

### Final CONFIG-03 keys for template

```ini
[ClientGame]
skipIntro=true
disableCutScenes=true
skipSplash=true   # also helps; user's known-good config has it

[ClientUserInterface]
disableG15Lcd=true
```

**Note:** Vivox is not disable-able via config. This is a known limitation; CONFIG-03 phrasing should be updated to reflect "Bink + G15 disabled; Vivox left in idle install state."

---

## Common Pitfalls

### Pitfall 1: searchPath vs searchTree vs searchTOC confusion

**What goes wrong:** Author writes `searchPath_NN_N=@SWG_INSTALL_DIR@/<file>.tre` thinking it points at a TRE; runtime fails to mount.

**Why it happens:** `searchPath` is for filesystem **directories**; `searchTree` is for **specific TRE archive files**; `searchTOC` is for **table-of-contents files** that list TREs. The boot doc and CLAUDE.md both refer to "searchPath_NN" as the canonical syntax, which is misleading.

**How to avoid:** Use the user's working SWGSource config as ground truth. SWGSource uses `searchTOC_NN_N=*.toc` + `searchTree_NN_N=*.tre`, NOT `searchPath_NN_N=`. Audit each key name against `TreeFile.cpp` (lines 118-148 — three distinct loops, three distinct functions).

**Warning signs:** Boot log shows "TreeFile: 0,0,0=opened" or `FATAL` on `misc/asynchronous_loader_data_*.iff` not found in Phase 9 — none of the assets are mounting because the keys are the wrong family.

### Pitfall 2: Trusting the boot doc's installTimer key

**What goes wrong:** Author writes `[SharedDebug] installTimer=true` per the boot doc; no per-phase log lines appear.

**Why it happens:** The boot doc has a typo. The actual code reads `[SharedDebug/InstallTimer] enabled` (note the section namespacing with `/` and the key name `enabled`).

**How to avoid:** Always cross-check config key claims against `ConfigFile::getKey*` call sites in source. For boot tracing, verify `InstallTimer.cpp:34`.

**Warning signs:** Boot is silent (no `InstallTimer:` lines in log) despite the key supposedly being set. The log file is otherwise normal.

### Pitfall 3: Adding `[Station] sessionId` "for completeness"

**What goes wrong:** Author adds `sessionId=` (commented or empty) to the template documentation, hopes user understands. Later user uncomments out of curiosity.

**Why it happens:** Templating instinct says "show all the keys, comment the dangerous ones". For sessionId this is wrong — D-05 explicitly forbids it being present in any form.

**How to avoid:** Document the key's existence in a leading comment block, but **do not write the key line** (commented or otherwise). A line with `#sessionId=` is one keystroke away from being active.

**Warning signs:** Boot reaches login UI but submitting credentials triggers `LoginIncorrectClientId` or "Connection lost" with no obvious cause; client log shows zero-length username being sent.

### Pitfall 4: Wrong working directory at runtime

**What goes wrong:** SwgClient launches from a path that doesn't contain `client.cfg`; `ConfigFile::isEmpty()` returns true; `FATAL("Config file not specified")`.

**Why it happens:** `SetupSharedFoundation::install` calls `ConfigFile::loadFile("client.cfg")` with a relative path. It resolves relative to CWD, not the exe directory. Running via `cmd /c "bin\Debug\SwgClient_d.exe"` from the project root sets CWD to project root, where there is no `client.cfg`.

**How to avoid:** Always launch from `bin/Debug/` or `bin/Release/`. VS 2022 Debug-launch sets working directory to exe dir by default (verify in project Properties → Debugging → Working Directory). For CLI testing: `cd build/bin/Debug && SwgClient_d.exe`.

**Warning signs:** Modal dialog "Config file not specified" appears within 1 second of launch; no log file is written (sharedFoundation fataled before SetupSharedLog).

### Pitfall 5: Assuming POST_BUILD `${CMAKE_CURRENT_BINARY_DIR}` resolves to the global build dir

**What goes wrong:** Author writes `${CMAKE_BINARY_DIR}/client.cfg` in POST_BUILD, expecting it to find the configure_file output; copy fails because the file isn't there.

**Why it happens:** `configure_file()` writes to `${CMAKE_CURRENT_BINARY_DIR}` which is the per-target build dir (`build/game/client/application/SwgClient/src/`). `${CMAKE_BINARY_DIR}` is the top-level (`build/`).

**How to avoid:** Both `configure_file()` destination AND the POST_BUILD copy_if_different source MUST use `${CMAKE_CURRENT_BINARY_DIR}` and they MUST be in the same CMakeLists.txt scope (so that "current" resolves the same way). Putting `configure_file` in `src/CMakeLists.txt` (top-level) and POST_BUILD in `src/game/.../SwgClient/src/CMakeLists.txt` would mismatch.

**Warning signs:** Build succeeds but staging step fails with "file does not exist" pointing at an unexpected directory. Easy to fix once spotted.

### Pitfall 6: SWG_INSTALL_DIR path with spaces

**What goes wrong:** User's path is `D:/Code/SWGSource Client v3.0/` (contains space). Some CMake operations break on unquoted spaces.

**Why it happens:** The user's actual install path contains a space. `set(SWG_INSTALL_DIR "D:/Code/SWGSource Client v3.0" CACHE PATH "...")` is fine. The placeholder substitution `@SWG_INSTALL_DIR@/sku0_client.toc` is fine. But if anywhere downstream the path is used in a shell command without quoting (e.g., `${CMAKE_COMMAND} -E copy ${SWG_INSTALL_DIR}/foo bar`), the space breaks parsing.

**How to avoid:** Quote any shell-argument usage in CMake: `${CMAKE_COMMAND} -E copy "${SWG_INSTALL_DIR}/foo" "$<TARGET_FILE_DIR:Foo>/foo"`. The `client.cfg.in` template substitution itself is space-safe — INI parser reads to end-of-line. **However:** when SWG client mounts a TRE at runtime, it uses Windows `CreateFile` with the path; `CreateFile` accepts spaces fine. No runtime issue. Only build-time CMake commands need quoting.

**Warning signs:** CMake-time error like "could not find file 'D:/Code/SWGSource' (truncated at space)". Quote the variable in the offending command.

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `[Station] sessionId=...` for SOE Station SSO via `sdt-session*.station.sony.com:3000` | Plain user/password via `LoginClientId` message | SOE shutdown 2011; Station auth servers offline | Phase 6 client.cfg must NOT include sessionId; swg-main `validateClient` accepts any credentials when `externalAuthURL=""` |
| `searchPath_NN_N=` filesystem directories | `searchTOC_NN_N=` + `searchTree_NN_N=` archive files | Switched at retail TRE packaging time (~2005) | Use the SWGSource config as ground truth, not the internal `exe/win32/common.cfg` |
| `[SharedDebug] installTimer=true` (boot doc claim) | `[SharedDebug/InstallTimer] enabled=true` (verified code) | n/a — this is just a doc-vs-code mismatch | Always grep source for the actual config-key string |
| `validateStationKey=1` server-side | Code path commented out in swg-main; setting has no effect | swg-main fork after 2011 | Server-side: do not configure; client-side: not affected |
| Manually-edited `client.cfg` | CMake `configure_file()` + cache variable | Phase 6 (this phase) | Reproducible from clean clone; user's machine path is one CMake flag |

**Deprecated/outdated:**
- **`docs/recon/05-client-boot-sequence.md` config-key claim:** "[SharedDebug] installTimer=true" — should read "[SharedDebug/InstallTimer] enabled=true". Plan 6 SHOULD include a doc fix to `docs/recon/05-client-boot-sequence.md` to correct this. **[ASSUMED]** that the boot doc author conflated the InstallTimer namespace name with a key name; verifiable by source grep.

---

## Project Constraints (from CLAUDE.md)

- **House rule: Treat the tree as read-only history.** Phase 6 honours this — the only Phase 6 work that touches `src/` is build-system files (CMakeLists.txt, the new `src/cmake/config/client.cfg.in`). No C++ source edits.
- **Watch for SOE-internal hostnames, IP masks, DSNs, station credentials in `exe/linux/*.cfg`.** Phase 6 plan must NOT pull from `loginServer.cfg`'s old SOE values (`sdt-session1.station.sony.com:3000`, `sdswgp5b`, `64.37.*`). Reference config is **the user's `D:/Code/SWGSource Client v3.0/client.cfg`**, not the leak's `exe/linux/`.
- **Don't search the live tree by file content blindly.** Phase 6 research scoped greps to specific subtrees (`sharedNetworkMessages`, `clientUserInterface`, `clientGame/src/shared/core`).
- **Source-edit budget: zero C++ source edits in v1.** Phase 6 NetworkVersionId pre-audit confirms no D-14 source edit needed. The two existing minimal edits (FreeCamera.cpp, SwgCuiAllTargets.cpp) remain the only source modifications.
- **Static CRT (`/MT` Release / `/MTd` Debug):** Phase 6 does not touch CRT linkage. POST_BUILD copy operations don't link anything.
- **Server contract is read-only:** Phase 6 plan must NOT propose any swg-main server changes. Auth bypass already provided by upstream.
- **Asset licensing:** TRE archive paths reference user's own retail SWG install; no redistribution. The template uses `@SWG_INSTALL_DIR@` placeholder — the actual paths exist only in the user's local `bin/<config>/client.cfg`.

---

## Runtime State Inventory

> Phase 6 is config-authoring + boot-debug; not a rename/refactor. Most categories N/A. The relevant inventory is "what runtime state does the SwgClient binary expect at first launch":

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | None — first launch reads cfg, opens TREs, opens UDP. No persistent state from prior runs | None |
| Live service config | swg-main VM's LoginServer must be running on `127.0.0.1:44453` (or the IP the user puts in client.cfg). VM startup is OUT OF SCOPE per D-08. | User starts VM before MVB-3 attempt; Phase 6 plan does NOT document VM bring-up. |
| OS-registered state | `HKCU\Software\Sony Online Entertainment\Default` (registry write at Phase 3) — created if absent, no privileged write needed. Recommend overriding via `[ConfigSharedFoundation] ProductRegistryPath` to a private path so it doesn't collide with a retail SWG install on the same user account. | Optional cleanup: include `ProductRegistryPath=Software/whitengold/Default` in template. |
| Secrets/env vars | `SWGCLIENT_MEMORY_SIZE_MB` (optional) — unset, defaults to 75% RAM clamped [250,750] MB in Debug builds | None — defaults are fine |
| Build artifacts | `bin/Debug/client.cfg` from Phase 4 placeholder — Phase 6 replaces it. Phase 4 already staged 30 DLLs + `mozilla/` chrome subtree. | Phase 6 plan deletes the old `src/game/client/application/SwgClient/client.cfg` placeholder; CMake `configure_file()` writes the new one to `${CMAKE_CURRENT_BINARY_DIR}` |

---

## Boot Failure Triage Runbook (D-12)

> The four most likely failure categories ranked by frequency-during-first-bring-up. Each entry: log signature, probable cause, fix.

### Category 1: Missing runtime DLL

**Log signature:** Modal dialog from Windows: *"The code execution cannot proceed because <Mss32.dll | binkw32.dll | xpcom.dll | dpvs.dll | ...> was not found."* No `Logs/` file written (process aborts before SetupSharedLog).

**Probable cause:** Phase 4 Plan 05's POST_BUILD step skipped (clean build incomplete) OR a DLL was added to expectations without staging. Common offenders: `lgLcd.dll` (Logitech G15) is NOT staged but is `LoadLibrary`'d if `disableG15Lcd` is not true.

**Fix:**
1. Verify all 30 DLLs from Phase 4 Plan 05 list are present in `bin/Debug/` via `dir bin\Debug\*.dll`.
2. If a DLL is missing: re-run Phase 4 POST_BUILD (`cmake --build build --config Debug --target SwgClient`) — if same DLL still missing, check Phase 4 plan 05 for inclusion in copy_if_different list.
3. If DLL is in list but not in `bin/Debug/`: source `${SWG_WIN32_DIR}` (= `src/../exe/win32/`) — verify file exists in source tree.

**Confidence:** HIGH — dumpbin confirmed zero-import-error in Phase 4 Plan 05 verification (2026-05-04). This category is unlikely to recur unless a new DLL dependency emerges (e.g., from `rasterMajor=` config change pulling a new gl0X plug-in).

### Category 2: TRE archive not found

**Log signature:** First few lines of `Logs/SwgClient_*.log` succeed (process started, foundation installed). Then warning lines like:
```
TreeFile: warning, opening 'sku0_client.toc' failed
```
followed by FATALs at Phase 9:
```
FATAL: misc/asynchronous_loader_data_<n>.iff not found
```
or silently rendering a black window (no Splash backdrop).

**Probable cause:**
- `SWG_INSTALL_DIR` points at a path that doesn't exist OR doesn't contain the `.toc` files
- CMake variable was set at configure time but the directory was later moved/deleted
- Path contains characters CMake didn't escape correctly (rare — TreeFile uses Win32 `CreateFile` which is permissive)

**Fix:**
1. Print the resolved value: in CMakeLists.txt add `message(STATUS "SWG_INSTALL_DIR=${SWG_INSTALL_DIR}")` — confirms what's substituted into client.cfg.
2. Inspect the staged `bin/Debug/client.cfg` and verify each `searchTOC_NN_N=` and `searchTree_NN_N=` resolves to an existing file.
3. Run with `[SharedFile] logTreeFileOpens=true` (and `[SharedFile] reportTreeFilePaths=true` in non-PRODUCTION) to dump every TreeFile open attempt to log.
4. Compare against user's known-working SWGSource client launch — the file list must match.

**Confidence:** HIGH — `TreeFile::install` is well-instrumented; the exact warning text is stable.

### Category 3: D3D9 device init failure

**Log signature:** Boot reaches Phase 7 (`SetupClientGraphics::install`), then process exits cleanly without window appearing. `Logs/SwgClient_*.log` contains lines from `SetupClientGraphics` then no further `Game::install` line (because the wrapping `if (SetupClientGraphics::install(...))` short-circuits).

**Probable cause:**
- GPU has no D3D9 compatibility profile (very rare on Win10/11 — typically only headless VMs)
- `[ClientGraphics] rasterMajor=N` mismatch: `gl0N_*.dll` plug-in not present (we ship gl05/gl06/gl07; if user puts rasterMajor=4 there's no gl04_*.dll)
- Display adapter has 0 hardware monitors (e.g., RDP without graphics passthrough)

**Fix:**
1. Force windowed mode in template: `[SharedFoundation] windowed=1`. Eliminates fullscreen-mode-switch failures.
2. Confirm `rasterMajor` is unset in template (defaults to highest supported by GPU).
3. Run `dxdiag.exe` and confirm DirectX 9 runtime reports a valid D3D9 device.
4. If running over RDP: log out and run from physical console; D3D9 over RDP needs explicit rendering hints not configured in this build.

**Confidence:** MEDIUM — the failure is well-defined but the recovery depends on user environment.

### Category 4: Config key not found / value malformed

**Log signature:** Any of:
- Modal dialog "Config file not specified" → CWD is wrong (see Pitfall 4) OR client.cfg is empty
- Boot reaches Phase 9 but login UI shows "Connection lost" immediately on submit → `loginServerAddress0` empty or unreachable
- `Logs/SwgClient_*.log` line: `WARNING: ConfigFile: missing key '<key>' in section '<section>' (using default <default>)` — usually benign, but cumulative warnings indicate a malformed cfg

**Probable cause:**
- POST_BUILD didn't run (no client.cfg in `bin/Debug/`)
- `configure_file()` substituted `@SWG_INSTALL_DIR@` to literal empty string (cache var was `set()` to "" somewhere)
- Hand-edits to staged `client.cfg` in `bin/Debug/` introduced syntax errors (e.g., a key without an `=`)

**Fix:**
1. `dir bin\Debug\client.cfg` — file exists?
2. `type bin\Debug\client.cfg` — content sane? `@SWG_INSTALL_DIR@` placeholder still present (substitution failed) or replaced with empty string?
3. `cmake -L build/` — list all cache variables; verify `SWG_INSTALL_DIR` is set to a real path.
4. Rebuild: `cmake -S src -B build -DSWG_INSTALL_DIR="<path>"` then `cmake --build build --config Debug --target SwgClient`.

**Confidence:** HIGH — symptoms are stable; common during first plan execution.

---

## Open Questions

1. **Where exactly does `Logs/SwgClient_*.log` land at runtime?**
   - What we know: `SetupSharedLog::install("SwgClient")` runs at ClientMain.cpp:246. Log filename prefix is "SwgClient".
   - What's unclear: The log directory is computed by `sharedLog/LogManager` based on cfg + CWD; haven't traced the exact resolution. Likely `<cwd>/Logs/` based on common SOE pattern.
   - Recommendation: Phase 6 plan's MVB-1 verification step does `dir /s SwgClient*.log` from project root after first launch attempt. Either it's in `bin/Debug/Logs/` or it's flat in `bin/Debug/`. Document the actual location in Phase 6 plan after first run.

2. **Does the user need `SwgClientSetup_r.exe` to generate `options.cfg` for first run?**
   - What we know: SwgClientSetup is a separate Win32 binary (`src/game/client/application/SwgClientSetup`) that writes `options.cfg` with auto-detected display/audio settings. The user's reference install has it. Phase 4 does not build SwgClientSetup.
   - What's unclear: Whether SwgClient_d.exe boots cleanly with NO `options.cfg` (relying on `client.cfg` defaults + cfg defaults from `defaults.cfg` if mounted in TRE).
   - Recommendation: First launch ATTEMPT should be without options.cfg; if D3D9 init fails, try copying user's existing `D:/Code/SWGSource Client v3.0/options.cfg` to `bin/Debug/options.cfg` and re-launch. If that fixes it, the Phase 6 plan should include either (a) ship a hand-authored static `options.cfg` next to client.cfg, or (b) build SwgClientSetup as a follow-on. **Likely (a) is sufficient and 5 minutes of work.**

3. **Will `binkw32.dll` `LoadLibrary` succeed when intro is skipped?**
   - What we know: `VideoList::install` (ClientMain.cpp:320) registers Bink as a video provider via `LoadLibrary("binkw32.dll")`. This happens regardless of `skipIntro`. binkw32.dll IS staged in Phase 4 Plan 05.
   - What's unclear: Does Bink dynamically depend on any audio init that fails when Mss32 isn't running? Probably no — Phase 4 verified Mss32 imports.
   - Recommendation: No special action; if a Bink-related error surfaces in the log, document and fix in MVB-1 iteration.

4. **Does CuiVoiceChatManager::install crash without a Vivox server?**
   - What we know: `SwgVivox::install` is the platform-specific entry; the symbol resolves at link time (Phase 4 succeeded). At runtime, Vivox SDK can run idle without a server.
   - What's unclear: Whether the Vivox SDK attempts an outbound connection at install time (some SDKs phone home for license validation).
   - Recommendation: First launch will reveal this. If `voicechat`/`vivox` strings appear in FATALs, mitigation is to investigate adding a `[SwgClient] disableVoiceChat` style hook (would require understanding SwgVivox source — but `SwgVivox.cpp` doesn't exist in whitengold; it's defined in Vivox lib only).

---

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| CMake | Build templating | ✓ (project minimum 3.27) | 3.27+ | — |
| Visual Studio 2022 + Win11 SDK | Build verification | ✓ (Phase 5 verified) | v143 toolset | — |
| User's SWGSource client install | Runtime TRE archives | ✓ at `D:/Code/SWGSource Client v3.0/` | v3.0 | None — Phase 6 cannot proceed without it |
| swg-main VM (LoginServer) | MVB-3 verification only | Assumed ✓ — user has SWGSource VM startup instructions per D-08 | swg-main commit `91f0357` (or newer) | If down: Phase 6 stops at MVB-1 (login UI), MVB-3 deferred until user starts VM |
| Network: 127.0.0.1:44453 outbound UDP | LoginConnection | Always ✓ (loopback) | — | If firewall blocks: Phase 6 plan includes Windows Firewall exception step for SwgClient_d.exe |

**Missing dependencies with no fallback:**
- None. SWGSource client is verified to exist at `D:/Code/SWGSource Client v3.0/` — files listed in earlier section.

**Missing dependencies with fallback:**
- swg-main VM not running → Phase 6 MVB-3 can't proceed; MVB-1 (login UI render) is independent and can succeed without VM.

---

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | None — there is no automated test framework in the whitengold tree |
| Config file | None |
| Quick run command | Manual: `cmake --build build --config Debug --target SwgClient` then run `bin/Debug/SwgClient_d.exe` |
| Full suite command | Same — Phase 6 verification is manual + log-grep |

**Phase 6 is a manual-verification phase by nature.** There are no unit tests because:
- The work is config authoring (no logic to assert)
- The verification is visual (login UI rendered) + behavioural (handshake completes)
- Adding an automated test framework is out of scope per Phase 0/1 decisions

### Phase Requirements → Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| LAUNCH-01 | TRE archives mount at boot from `searchTOC_NN_N` paths | Manual + log-grep | `findstr /C:"TreeFile" bin\Debug\Logs\SwgClient_*.log` | ❌ Wave 0 (no log file until first run) |
| LAUNCH-03 | MVB-1 reached: login UI rendered + InstallTimer log marker | Manual visual + log-grep | `findstr /C:"InstallTimer" /C:"Game::install" bin\Debug\Logs\SwgClient_*.log` | ❌ Wave 0 |
| LAUNCH-04 | MVB-3 reached: cluster + character roster shown | Manual visual + log-grep | `findstr /C:"LoginEnumCluster" /C:"EnumerateCharacterId" bin\Debug\Logs\SwgClient_*.log` | ❌ Wave 0 |
| CONFIG-01 | `[Station] sessionId` absent | Static cfg-grep | `findstr /C:"sessionId" bin\Debug\client.cfg` (must return empty) | ❌ Wave 0 (cfg not staged yet) |
| CONFIG-02 | `[Station] gameFeatures=15` present | Static cfg-grep | `findstr /C:"gameFeatures" bin\Debug\client.cfg` | ❌ Wave 0 |
| CONFIG-03 | Vivox+Bink+G15 disabled keys present | Static cfg-grep | `findstr /C:"skipIntro" /C:"disableG15Lcd" bin\Debug\client.cfg` | ❌ Wave 0 |

### Sampling Rate
- **Per task commit:** `cmake --build build --config Debug --target SwgClient` (verifies POST_BUILD restages client.cfg without link-step damage)
- **Per wave merge:** `findstr` set above against `bin\Debug\client.cfg`
- **Phase gate:** Manual MVB-1 attempt + manual MVB-3 attempt; visual + log-grep verification

### Wave 0 Gaps
- [x] No actual gaps — Phase 6's "tests" are file/log greps that exist at the moment of execution. There are no test files to author ahead of time.

*(If formalised later: a Phase 7+ could add a Python or PowerShell driver that programmatically verifies cfg content + log markers. Out of v1 scope.)*

---

## Security Domain

> `security_enforcement: true` and `security_asvs_level: 1` per `.planning/config.json`.

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | yes | The auth flow is `LoginClientId(username, password, version)` over UDP. Both whitengold client and swg-main server are decade-old code with no modern auth controls. Per CONTEXT.md and CLAUDE.md, this is **explicitly accepted out-of-scope** — the swg-main VM provides a community auth bypass (`externalAuthURL=""`); this phase does not implement new auth. |
| V3 Session Management | no | Sessions are managed by ConnectionServer post-MVB-3 (out of Phase 6 scope) |
| V4 Access Control | no | Phase 6 reaches character-select (read-only roster); no privileged operations |
| V5 Input Validation | yes (limited) | The client.cfg is **trusted input** authored by the developer at build time. Runtime input (typed username/password at login UI) is server-side concern. |
| V6 Cryptography | no | No new cryptographic operations introduced. The SOE UDP library has its own optional encryption layer (already present, not modified). |

### Known Threat Patterns for whitengold/swg-main stack

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Plaintext password over UDP | Information disclosure | Accepted; loopback (127.0.0.1) means no over-network exposure for first-run. swg-main has SOE UDP library encryption available but not enforced at v1. |
| `sessionId` activates dead SOE Station SSO | Tampering / spoofing (would let attacker spoof session) | **Mitigation: explicit absence of `[Station] sessionId` in template** (CONFIG-01) |
| TRE archive substitution (malicious .tre) | Tampering | Out of scope — the user owns and curates their own SWG install; supply chain is trusted |
| client.cfg edits at runtime by other process | Tampering | Out of scope — local single-user dev environment; defense-in-depth not required at v1 |

**No new security risks introduced by Phase 6.** The phase merely connects an already-authored client to an already-running server with a documented community auth bypass. All identified concerns are documented as accepted in the project's threat model (CLAUDE.md "What's NOT here" section, item 3).

---

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | The boot doc's `[SharedDebug] installTimer=true` is a typo, not a deprecated alternative key | §State of the Art, §Phase 9 Boot Marker | LOW — even if both keys were once valid, the verified key works for Phase 6's purpose; the wrong key just produces silent boot |
| A2 | Vivox's `SwgVivox::install` does not phone home or block at boot | §Open Questions Q4, §Dead-Service Disable Keys | MEDIUM — if it does, MVB-1 reach is delayed and may need additional config; mitigations exist (per Vivox SDK docs, runtime config can disable) |
| A3 | `bin/Debug/client.cfg` working directory at launch will resolve config keys correctly | §Pitfall 4 | LOW — verified by Phase 4 Plan 05 placeholder testing already; cmd-line launch from `bin/Debug/` is the documented happy path |
| A4 | `searchTree_00_8=swgsource_3.0.tre` and `searchTree_00_7=disable_wayfar_dearic_snow.tre` are required for SWGSource VM compatibility, not just the user's preference | §TRE Search Path Inventory | LOW — these TREs override base game content; if missing, boot still completes but visuals differ. Plan can verify by reading the SWGSource VM's expected client manifest. |
| A5 | The user's `D:/Code/SWGSource Client v3.0/` is the authoritative TRE source | §Environment Availability | LOW — confirmed by D-04 + the directory listing showing 50+ `.tre` and `.toc` files |
| A6 | `[ClientGraphics] screenWidth/Height` defaults of 1024x768 are safer than user's 1920x1080 | §TRE Search Path Inventory (options.cfg discussion) | LOW — both are valid; 1024x768 is more permissive on weak GPUs but adds rendering iteration if user wants fullscreen 1080p. User can edit staged file. |

**If this table is empty:** N/A — six small assumptions remain, all LOW/MEDIUM risk. None block Phase 6 planning.

---

## Sources

### Primary (HIGH confidence)
- **whitengold source code:** Direct read of cited files (TreeFile.cpp, GameNetworkMessage.cpp, ClientLoginMessages.cpp, LoginConnection.cpp, ClientMain.cpp, Game.cpp, InstallTimer.cpp, ConfigClientGame.cpp, ConfigClientUserInterface.cpp, SwgCuiG15Lcd.cpp, SwgCuiLoginScreen.cpp) — line numbers cited inline.
- **swg-main source code (sibling checkout):** `D:/Code/swg-main/src/engine/shared/library/sharedNetworkMessages/src/shared/common/GameNetworkMessage.cpp:21` for NetworkVersionId comparison.
- **`docs/recon/08-phase6-preflight.md`:** Phase 5 Plan 03 deliverable; contains P2.01 (wire format identical) and P2.03 (auth bypass via empty externalAuthURL). Both findings re-verified in this research.
- **`docs/recon/05-client-boot-sequence.md`:** 17-phase boot trace; Phase 9 source-of-truth — except for the `[SharedDebug] installTimer=true` typo flagged in §State of the Art.
- **User's working SWGSource config:** `D:/Code/SWGSource Client v3.0/{client,login,live,preload,options,user}.cfg` — directly read; provides verified working production cfg.
- **CMake docs (configure_file):** [VERIFIED via project precedent: src/engine/shared/library/sharedDebug/src/CMakeLists.txt:70] — pattern is already used in the project.

### Secondary (MEDIUM confidence)
- **`.planning/STATE.md`:** Phase 5 locked decisions (P2.01 wire format, P2.03 auth bypass); cross-referenced with primary source.
- **`.planning/PROJECT.md` Key Decisions table:** Phase 1-5 locks (CMake + VS 2022, no source edits, STLPort, /MT static CRT, Mozilla XPCOM stub).
- **`docs/build.md`:** Documents the FreeCamera.cpp + SwgCuiAllTargets.cpp known source edits (relevant for D-14 mitigation pattern).

### Tertiary (LOW confidence)
- **Boot doc claim "[SharedDebug] installTimer=true":** Marked as INCORRECT (typo) in §State of the Art; verified key is `[SharedDebug/InstallTimer] enabled=true`.
- **Open question Q4 (Vivox install behavior):** No primary source available — `SwgVivox.cpp` is not in the whitengold tree (linked from Vivox lib). To be discovered at first MVB-1 attempt.

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all components are CMake-native or already in the project; no new dependencies introduced.
- Architecture: HIGH — boot sequence confirmed by source read; wire format verified identical to swg-main.
- Pitfalls: HIGH — six pitfalls catalogued, each tied to a specific verification source.
- TRE search path inventory: HIGH — direct read of user's working config.
- NetworkVersionId pre-audit: HIGH — both repos read; values match exactly.
- Phase 9 log marker: HIGH — InstallTimer.cpp read; key name and message format verified.
- Auth bypass: HIGH — boot doc + Phase 5 P2.03 + LoginConnection.cpp triangulated.
- Triage runbook: MEDIUM — categories are well-defined but specific log strings will be confirmed at first MVB-1 attempt.
- Vivox boot behavior: MEDIUM — Open Question Q4.

**Research date:** 2026-05-05
**Valid until:** 2026-06-05 (estimate — stable retro codebase, but swg-main upstream could change auth defaults; recheck Phase 5 P2.03 if more than 30 days since survey)
