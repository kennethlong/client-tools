# Phase 6: Launch + Login Handshake — Context

**Gathered:** 2026-05-05
**Status:** Ready for planning

<domain>
## Phase Boundary

Wire `SwgClient_d.exe` to the user's retail SWG asset tree and a local swg-main
VM, then drive the 17-phase client boot sequence to two sequential milestones:

- **MVB-1** — login UI visible (Phase 9 of boot): splash + backdrop + LoginScreen
  mediator active, window rendering frames
- **MVB-3** — character select reached: `LoginClientId → LoginEnumCluster →
  EnumerateCharacterId` message exchange round-trips; cluster + character roster
  displayed (v1 finish line)

Work streams:
1. **Config authoring** — `client.cfg.in` template with TRE search paths + login
   settings + dead-service disable flags (SC-1, SC-2, SC-3 / CONFIG-01/02/03)
2. **Boot debugging to MVB-1** — get through the 17-phase boot to login UI
   (SC-4 / LAUNCH-03)
3. **Login handshake to MVB-3** — authenticate against swg-main VM and reach
   character select (SC-5 / LAUNCH-04)

**Out of Phase 6 scope:**
- Server-side VM configuration or startup (user has SWGSource VM instructions)
- Beyond character select (combat, world entry, zone-in)
- Any additional CMake authoring not directly required for client.cfg staging

</domain>

<decisions>
## Implementation Decisions

### client.cfg Template

- **D-01:** Master template lives at `src/cmake/config/client.cfg.in` —
  version-controlled in the repo. CMake `configure_file()` substitutes variables
  and writes the resolved file to `build/`; POST_BUILD copies it to
  `bin/Debug/client.cfg` and `bin/Release/client.cfg` (replacing the Phase 4
  placeholder). One source of truth.

- **D-02:** Machine-specific TRE paths handled via `-DSWG_INSTALL_DIR` CMake
  cache variable. The template uses `@SWG_INSTALL_DIR@` placeholders for all
  `searchPath_NN` entries. Default candidate: `D:/Code/SWGSource Client v3.0`
  (user's installed SWGSource client). User sets `-DSWG_INSTALL_DIR=<path>` once
  at configure time; re-run cmake to regenerate client.cfg.

- **D-03:** LoginServer address hardcoded in template as `127.0.0.1`, port
  `44453`. User's swg-main VM runs on loopback; no CMake variable needed. If VM
  IP ever changes, user edits `bin/Debug/client.cfg` manually (or re-templates).

- **D-04:** Full retail TRE file set listed in `client.cfg.in`. TRE filename list
  sourced from `D:\Code\SWGSource Client v3.0\client.cfg` (user's working
  reference config). Researcher MUST read that file to get the exact searchPath
  list. Missing TREs log as warnings, not fatals — using the full set is safe.

- **D-05:** `[Station] sessionId` MUST be absent from the template (activates
  dead SOE Station SSO path if present). `[Station] gameFeatures=15` MUST be
  present (all four SKU bits). `[ClientGame] loginServerAddress0=127.0.0.1` and
  `loginServerPort0=44453` MUST be present.

- **D-06:** Dead-service middleware disabled in template (CONFIG-03): Vivox
  `enableVoiceChat=0`, Bink intro video skipped, Logitech G15 LCD
  `enableG15Lcd=false`. Boot must not depend on any of these.

### Boot Milestone Strategy

- **D-07:** Two explicit sequential milestones with a human-verify checkpoint
  between them. MVB-1 (login UI) must pass before attempting VM connection.
  Smaller blast radius — isolates D3D/asset boot issues from network issues.

- **D-08:** Server startup is **out of scope** for Phase 6 documentation. User
  has SWGSource VM startup instructions. Phase 6 focuses purely on client-side
  config + boot debugging + login handshake.

- **D-09:** MVB-1 success criterion = **visual confirmation** (login UI on screen
  with rendered frames) **AND** a specific `[SharedDebug] installTimer=true` log
  marker that identifies Phase 9 completion. Researcher identifies the exact log
  line from `docs/recon/05-client-boot-sequence.md` before planning.

### Boot Failure Triage

- **D-10:** **Hybrid triage protocol** — run standalone first, check `Logs/`
  directory for `installTimer` phase log output to see how far boot progressed;
  attach VS 2022 debugger only when log is ambiguous or crash is reproducible and
  needs precise source location.

- **D-11:** **Early crash fallback** (crash before Phase 3 / before log is
  written) — check Windows Event Log for fault address + look for mini-dump in
  `bin/Debug/`. The `MyUnhandledExceptionFilter` in `sharedFoundation` writes a
  minidump on unhandled exception. Combined, they identify the failing module
  without needing full debugger session overhead.

- **D-12:** Phase 6 plan includes a **triage runbook** for the 3-4 most likely
  boot failure categories: missing runtime DLL, TRE file not found (searchPath
  misconfigured), D3D device init failure, config key not found. Each entry in
  the runbook includes: log signature, probable cause, and fix.

### NetworkVersionId Pre-Audit

- **D-13:** Researcher **pre-audits** `NetworkVersionId` in both whitengold's
  `sharedNetworkMessages` and swg-main source before planning the first-run step.
  If the values match, the risk is cleared before any debugging session. If they
  mismatch, Phase 6 plans fix it before first run.

- **D-14:** If mismatch found: **accept as a minimal source edit** following the
  FreeCamera.cpp / SwgCuiAllTargets.cpp precedent — one-liner fix, documented in
  `docs/build.md` under "known source edits". This keeps the "no behavioral
  change" principle and the zero-meaningful-source-edit intent intact.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase Goals and Success Criteria
- `.planning/ROADMAP.md` §Phase 6 — 5 success criteria (SC-1 through SC-5),
  requirements LAUNCH-01/LAUNCH-03/LAUNCH-04 and CONFIG-01/02/03; authoritative
  scope and finish line definition
- `.planning/REQUIREMENTS.md` §LAUNCH-01, §LAUNCH-03, §LAUNCH-04, §CONFIG-01,
  §CONFIG-02, §CONFIG-03 — the six requirements owned by Phase 6

### Pre-Flight Research (MANDATORY — read before planning)
- `docs/recon/08-phase6-preflight.md` — P2.01 (wire format identical between
  whitengold + swg-main; no protocol adaptation needed) and P2.03 (auth bypass:
  `externalAuthURL=""` → any credentials accepted; client must NOT send
  `[Station] sessionId`). Contains the swg-main `validateClient()` decision tree
  and ConfigLoginServer defaults.
- `docs/recon/05-client-boot-sequence.md` — 17-phase boot sequence trace; Phase
  9 = splash + login backdrop visible (MVB-1 target). Researcher must identify
  the exact `installTimer` log marker for Phase 9 completion.

### User's Reference Config (CRITICAL for D-04)
- `D:\Code\SWGSource Client v3.0\client.cfg` — user's working SWGSource client
  config; contains the actual TRE searchPath file list and any other settings
  already tuned for this server. **Researcher must read this file to get the
  exact TRE filename list** for the `client.cfg.in` template.

### Project-Level Decisions
- `.planning/PROJECT.md` §Key Decisions — locked decisions from Phases 1-5
  (STLPort, DX9, XPCOM stub, static CRT, no source edits)
- `.planning/STATE.md` — Phase 5 locked decisions; specifically: P2.01 wire
  format identical, P2.03 auth bypass shape, Phase 4 POST_BUILD DLL staging

### Client Login Code
- `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiLoginScreen.cpp`
  — client-side login UI; `ok()` sends `LoginClientId(username, password)`
- `src/engine/client/library/clientGame/src/shared/network/LoginConnection.cpp`
  — `LoginConnection`: network connection to LoginServer; handles
  `LoginClientToken` / `LoginIncorrectClientId` responses

### NetworkVersionId Pre-Audit Targets
- `src/engine/shared/library/sharedNetworkMessages/src/shared/clientLoginServer/`
  — whitengold's login message implementations; look for `NetworkVersionId`
  constant or GameNetworkMessage version field
- `C:\Code\swg-main\src\engine\shared\library\sharedNetworkMessages\src\shared\clientLoginServer\`
  — swg-main's equivalent; compare version values

### Build Integration
- `src/game/client/application/SwgClient/src/CMakeLists.txt` — Phase 4
  POST_BUILD staging command; must be updated to stage `configure_file`-generated
  client.cfg instead of the Phase 4 placeholder
- `.planning/phases/04-swgclient-executable-link/04-CONTEXT.md` — D-03/D-04/D-05
  DLL staging decisions; D-06/D-07 STLPort linkage decisions; context for
  POST_BUILD update

### Architecture References
- `.planning/codebase/INTEGRATIONS.md` §Authentication & Identity, §Game Server
  Cluster — login flow integration context; client auth path via LoginConnection
- `.planning/codebase/ARCHITECTURE.md` §Login Flow — 10-step login sequence from
  `SwgCuiLoginScreen::ok()` through `LoginClientId` → cluster list → character
  select

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `src/game/client/application/SwgClient/src/CMakeLists.txt` — Phase 4
  POST_BUILD command stages DLLs + placeholder `client.cfg`. Phase 6 updates this
  command to stage the `configure_file`-generated client.cfg instead.
- `exe/win32/tatooine.cfg` — existing per-planet SWG config; documents the
  `[SharedFile] searchPath_NN` key format and value style expected by ConfigFile.
- `exe/linux/loginServer.cfg` — historical SOE login config; historical reference
  for key names (do NOT use the values — they reference defunct SOE infrastructure).

### Established Patterns
- `configure_file()` + `@VAR@` substitution: not yet used in this project but
  CMake-native; consistent with the existing Find module pattern of resolving
  vendored paths without env vars.
- POST_BUILD `add_custom_command`: established in Phase 4 Plan 05 for DLL
  staging; extend the existing command or add a sibling command for client.cfg.
- "Minimal source edit" precedent: FreeCamera.cpp (static_cast type fix) and
  SwgCuiAllTargets.cpp (UDL space fix). If NetworkVersionId requires a source
  edit, follow the same one-liner pattern and document in `docs/build.md`.
- `[SharedDebug] installTimer=true`: boot-phase timing log key; must appear in
  client.cfg (or already in a config loaded at boot) to produce the Phase 9
  marker log.

### Integration Points
- `bin/Debug/client.cfg` and `bin/Release/client.cfg` — POST_BUILD destination;
  already staged by Phase 4 Plan 05; Phase 6 replaces the placeholder content.
- `[ClientGame] loginServerAddress0` / `loginServerPort0` — these keys are read
  by `LoginConnection.cpp`; must be present in the config loaded at boot time.
- `[SharedFile] searchPath_NN` — read by `TreeFile::install()` during boot Phase
  6 (config override from TreeFile); these must resolve to actual `.tre` files or
  boot will log warnings and possibly fail to load required UI assets.

</code_context>

<specifics>
## Specific Requirements

- **TRE asset source:** `D:\Code\SWGSource Client v3.0\` — a sibling directory
  of the repo at `D:\Code\swg-client\`. The user's working SWGSource client
  install. All `searchPath_NN` entries in `client.cfg.in` point into this directory.
- **Reference config to read:** `D:\Code\SWGSource Client v3.0\client.cfg` —
  researcher reads this to get the exact searchPath list and any other tuned
  settings.
- **Default cmake variable:** `-DSWG_INSTALL_DIR` should default to
  `D:/Code/SWGSource Client v3.0` (with option to override at configure time).
- **VM connection:** `loginServerAddress0=127.0.0.1`, `loginServerPort0=44453`
  (loopback; VM on same machine as client).
- **No `[Station] sessionId`** must ever appear in the shipped template — it
  activates the dead SOE SSO path.
- **Boot log:** `[SharedDebug] installTimer=true` must be set in the client.cfg
  (or verified to already be present in a config loaded at boot) so the Phase 9
  log marker is produced.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within Phase 6 scope.

</deferred>

---

*Phase: 6-Launch Login Handshake*
*Context gathered: 2026-05-05*
