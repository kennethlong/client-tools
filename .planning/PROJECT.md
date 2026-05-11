# whitengold — SWG Client Modernisation Port

## Current Milestone: v2.0 — Modernisation

**Goal:** Remove dead/disabled features from the CMake graph, migrate from STLPort 4.5.3 to MSVC STL, profile DPVS occlusion culling, and begin a D3D11 renderer plugin.

**Target features:**
- Phase 7: Dead code removal Track A (directory deletes → CMake unlinks → voice chat + XPCOM removal)
- Phase 8: Dead code removal Track B (~40 orphaned tools wired into CMake)
- Phase 9: STLPort → MSVC STL migration (hash_map sweep, wchar_t, compat shim removal)
- Phase 10: DPVS culling experiment (profile then decide)
- Phase 11: D3D11 renderer plugin (new Direct3d11.dll satisfying Gl_api table)

---

## What This Is

whitengold is the leaked Star Wars Galaxies source tree (NGE-era ~2010 build, leaked
Feb 2015). This project takes the **client** — `SwgClient` and its ~70-project
dependency graph — and ports its build system off Visual Studio 2005 onto a modern
**CMake + Visual Studio 2022** toolchain so it can be compiled, debugged, and
eventually launched against a community-run **SWG-Source** server VM. The long-arc
goal is full feature parity with the live NGE client (ground/space, professions,
housing, GCW, jukebox); v1 (compile + launch) is complete — v2 modernises the build.

## Core Value

**Every change must leave the client bootable to character select.** Dead code removal, STL swap, renderer work — all of it is only valid if the client still runs at the end. Build health is the invariant; modernisation is layered on top.

## Requirements

### Validated

<!-- Recon and prior work that's already locked. -->

- ✓ **Codebase mapped** — `.planning/codebase/` (7 docs, 2265 lines, commit `4349655bb`)
- ✓ **Source structure recon'd** — engine/game/dsrc/external layout documented in
  `STRUCTURE.md` and `CLAUDE.md`
- ✓ **Boot sequence (17 phases) traced** — `docs/recon/05-client-boot-sequence.md`
- ✓ **Build chain analysed** — `docs/research/swgclient-build.md` (project graph,
  flag inventory, third-party dependency map)
- ✓ **Middleware inventory** — animation, foliage, audio, video, physics middleware
  identified and characterised (`docs/research/runtime-middleware.md`,
  `animation-system.md`, `foliage-system.md`)
- ✓ **SWG-Source diff** — confirmed server-only fork, no client patches to adopt;
  build patterns are transferable (`docs/recon/07-swg-source-diff-findings.md`)

### Active

<!-- v1 milestone scope: compile + launch -->

- ✓ **CMake build system** authored parallel to existing `.vcproj`s, mirroring
  swg-main's CMakeLists structure (top-level `CMakeLists.txt` + `cmake/win32/`
  toolchain modules) — Validated in Phase 1 (foundations) + Phase 2 (23 shared libs)
- ✓ **Third-party SDK integration** — `Find*.cmake` modules for DX9, Bink, Miles,
  Vivox, libMozilla XPCOM, DPVS, lcdui, STLPort 4.5.3, libxml2, pcre, zlib, Boost
  — Validated in Phase 1; STLPort compat shims for VS 2022 / Win SDK 26100 resolved
- ✓ **Compile flags ported** — `/Zc:wchar_t-`, `_USE_32BIT_TIME_T=1`, `_MBCS`,
  `DEBUG_LEVEL`, `PRODUCTION`, `/wd4244 /wd4996 /wd4018 /wd4351`, `/MP`, `/Ob1`,
  `/MT[d]` verified across 24 static `.lib` targets — Validated in Phase 2
- ✓ **All 13 client engine libraries build** — `engine/client/library/` complete:
  clientAnimation, clientAudio, clientBugReporting, clientDirectInput, clientGame,
  clientGraphics (P1.11 resolved — d3d9.h from vendored DX9 path), clientObject,
  clientParticle, clientSkeletalAnimation, clientTerrain, clientTextureRenderer,
  clientUserInterface (XPCOM stub gate passed — zero xpcom/xul symbols); plus ui.lib
  — Validated in Phase 3; libMozilla XPCOM stub locks D-04/D-05/D-07
- ✓ **Game client libraries + SwgClient executable link** — `libEverQuestTCG`,
  `swgSharedNetworkMessages`, `swgClientUserInterface` (266 cpp), and `SwgClient`
  WIN32 executable all link cleanly; `SwgClient_d.exe` (34 MB) + `SwgClient_r.exe`
  (16.5 MB) produced; 30 runtime DLLs + `mozilla/` staged via POST_BUILD;
  dumpbin confirms static CRT + zero XPCOM imports — Validated in Phase 4
- ✓ **Debug build first** — `cmake --build build --config Debug` produces
  `SwgClient_d.exe`; binary launches (cursor spins, no missing DLL popup);
  Phase 1 pass/fail gate cleared — Validated in Phase 4 (2026-05-05)
- ✓ **Release build** — same toolchain produces `SwgClient_r.exe` (16.6 MB); clean-clone build verified — Validated in Phase 5 (2026-05-05)
- [ ] **Asset path wired** — client reads `.tre` archives from a retail SWG install
  via `searchPath*` configs in `client.cfg`
- [ ] **Login screen reached** — client boots far enough to render the login UI
  and connect to the local SWG-Source LoginServer
- [ ] **swg-main login flow handshake** — authenticate against the SWG-Source VM's
  LoginServer (whatever station-key stub it ships with) and reach character select

### Out of Scope

<!-- Deferred to future milestones or rejected outright. -->

- **Modifying C++ source files** — Phase 1/2 are build-config and configuration
  work only; source edits come later (avoid blast radius, keep diff focused on
  CMake authoring)
- **Replacing STLPort 4.5.3 with MSVC STL** — swg-main openly warns this is
  non-trivial; deferred to a future modernisation milestone
- **Replacing DirectX 9 / migrating to D3D11+** — multi-year effort, deferred
- **TRE asset reconstruction** — user has a retail SWG install; community-rebuilt
  client-assets is not needed for v1
- **Server-side modifications** — swg-main is treated as a **black-box network
  endpoint**; if server changes are required they happen upstream in the
  swg-main repo, not here
- **Linux server build** — `src/build/linux/Makefile` and the Oracle/JVM/IBM-Java
  toolchain are server-side and out of scope
- **Editor / tool builds** — ParticleEditor, ConversationEditor, SpaceQuestEditor,
  GodClient, etc. are deferred until SwgClient itself ships
- **Beyond zone-in** — combat, AI, quests, professions, housing, GCW, space,
  jukebox are all future-milestone scope
- **Patcher / live-update infrastructure** — `publish/*.mft` describe a retired
  patcher tree; we ship via direct CMake build, no patcher
- **SOE Station authentication** — the real auth service is gone; the swg-main
  VM stubs it and we accept whatever it provides
- **Customer service / metrics / chat / commodities servers** — these are
  separate processes in the SWG cluster; only LoginServer + Connection +
  PlanetServer + GameServer are needed to reach character select

## Context

**The repo is a frozen archive, not a runnable system.** Per `CLAUDE.md`'s
"What's NOT here" inventory, several gaps prevent end-to-end execution from
this tree alone: no `.tre` asset archives, no Oracle DB content, no working
Linux build environment (gcc 2.95/3.2, IBM Java 1.3, Oracle 9.2), no SOE
Station auth, proprietary SDKs as headers/stubs, internal SOE infra
references. The user's plan accepts these gaps and works around them —
retail SWG install supplies assets, swg-main VM supplies the server tier,
proprietary SDKs are reconstructed from headers + vendored libs where licensed
copies are available.

**Reference repo:** `C:\Code\swg-main` (sibling checkout, populated submodules).
Server-only fork descended from the same SOE source. Authoritative for build
patterns, **not** for client port work (no client patches exist there).

**Target environment:** Windows 10/11 with Visual Studio 2022 (MSVC 14.4x)
and CMake 3.27+. C++17 standard. STLPort 4.5.3 retained for v1.

**Server runtime target:** SWG-Source StellaBellum-style VM, stood up locally
by the user. Out of scope for this repo's commits.

**Asset source:** The user owns a legitimate retail SWG install. Client will
be configured to read its data tree at runtime via `searchPath*` entries.

## Constraints

- **Tech stack:** Windows-only, VS 2022, CMake, C++17, MSVC toolset — Phase 1
  Linux work is out of scope (server-side toolchain anyway)
- **Source-edit budget:** Zero C++ source edits in v1 — keep blast radius small;
  CMake authoring + headers + config only
- **STLPort retained:** Don't swap to MSVC STL in v1 — non-trivial refactor that
  would defeat the "no source edits" principle
- **Static CRT:** `/MT` (Release) / `/MTd` (Debug) per swg-main pattern; matches
  the third-party static libs we link against
- **Server contract is read-only:** swg-main VM is the runtime target; if a
  client behaviour requires a server change, it's deferred or pushed upstream
- **Asset licensing:** Only the user's own retail SWG install is used for `.tre`
  archives; no redistribution
- **Timeline:** No deadline — exploratory hobby cadence; granularity matters more
  than speed

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| **Framing B** (CMake + VS 2022, no source edits) over Framing A (period-correct VS 2005) | Aligns with swg-main's build system, easier onboarding, "minimum change" still holds because we're *adding* CMakeLists not editing source | Phase 1 foundation graph configures and builds with added CMake files only |
| **Debug build first**, Release later | Debug build is the more useful artefact for Phase 2 launch debugging — step-through is required to diagnose boot failures | — Pending |
| **swg-main as build-pattern reference** (server-only, but their CMake structure / flags / C++17 transfer) | They've already solved the modernisation problem for the server tree; client porting can follow the same shape | — Pending |
| **Retail SWG install for assets**, not community client-assets | User owns a legitimate copy; simpler licensing story; closer to original NGE behaviour | — Pending |
| **swg-main VM as black-box server**, no server edits in this repo | Keeps blast radius narrow; if server changes needed, push upstream to swg-main | — Pending |
| **STLPort 4.5.3 retained** for v1 | swg-main warns the swap is non-trivial; defeats "no source edits" principle | Phase 1 uses a build-local staged STLPort layout so VS 2022 can supply modern UCRT/MSVC native headers without editing vendored STLPort |
| **DirectX 9 vendored SDK first** | The repo ships the 2005-era DirectX 9 headers/import libs used by the client; system SDK drift is avoidable | Phase 1 `FindDirectX9.cmake` resolves vendored headers/libs; first real compile remains Phase 3 `clientGraphics` |
| **Mozilla XPCOM stub strategy** | Modern MSVC cannot safely compile the old XPCOM surface; the in-game browser is not required for v1 launch | Phase 1 `FindMozilla.cmake` resolves headers only; do not link `xul.lib`/`xpcom.lib` in client targets |
| **/MT static CRT** | Matches third-party static libs (Bink, Miles, DPVS) we link against | Phase 1 root CMake sets `CMAKE_MSVC_RUNTIME_LIBRARY` to `/MT` and `/MTd` |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-05-07 — v2 milestone started; v1 archived to MILESTONES.md; v2 ROADMAP.md + REQUIREMENTS.md written (Phases 7–11)*
