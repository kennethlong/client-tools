# whitengold — SWG Client Modernisation Port

## Current State: v2.3 Hardening SHIPPED (2026-06-15)

**v2.3 Hardening shipped — 10/12 requirements satisfied, the rest root-caused as x64 carry-forwards.** Two independent streams closed. **(A) In-client C++ hardening (Phases 24–27):** DPVS occlusion is config-gated (`[ClientGraphics/Dpvs] occlusionMode = auto|on|off`, auto-gated on POB-cell membership, F11 override preserved) — HARD-01; the staged client is machine-portable (Miles redist vendored + postbuild-repointed with a codec-repair guard, a tracked `client.cfg.template` + `setup-client.ps1` generator de-hardcoding `stage/override`+`stage/miles` and cleaning the Phase-11+ test keys) — PORT-01/02; the D-15 DPVS instrumentation was stripped atomically and the Options-window FATAL fixed — HARD-03 (D-15)/HARD-04. The two remaining client items were **root-caused, not skipped**: the cantina corner-snap (HARD-02) reduced to a 32-bit codegen float transient in the cell→world Y transform (CONSULT-43), and the `D3DXCompileShader`→`D3DCompile` port (HARD-05) was attempted, reverted, and shown to re-fight the entire gl11 shader-modernization battle — both are 32-bit-bound and parked for the x64 milestone (HARD-05 is satisfied-by-Fix-A for v2.3; HARD-03's CORNERSNAP probes are intentionally kept as the x64 acceptance harness). **(B) TRE compare tool (Phases 28–30):** the repo's first web app — an isolated, extractable `tools/tre-compare/` uv package (vendored stdlib parser → `[SharedFile]` scanner → first-hit-wins merged-virtual-tree → set/file diff with `(length, compressedLength)` signal + on-demand xxhash → sqlite cache → 4 FastAPI routes) under a Vite 8 / React 19 / Tailwind 4 / shadcn virtualized tree-diff SPA, served from a single 127.0.0.1 process. SC#4 human-approved on a real 231,086-row cross-distribution diff (SWGSource × SWG Infinity). Audit `gaps_found` (the single gap = the pre-decided HARD-02 x64 deferral), 12/12 integration links WIRED, 2/2 E2E flows complete.

**Prior:** v2.2 Visual Parity (shipped 2026-06-12) — D3D11 (`rasterMajor=11`) reached full visual parity with the known-good D3D9 baseline across characters, interiors, open world, terrain, gamma, UI, and effects (13/13 requirements; asset-PS pipeline + FFP combiner cascade + gamma curve pass + round minimap). See `milestones/v2.2-*`.

## Shipped Milestone: v2.3 — Hardening (closed 2026-06-15)

**Goal (MET, with documented x64 deferrals):** Consolidate the v2.2 parity win — act on the DPVS verdict, strip the accumulated debug instrumentation, make the staged client machine-portable, fix the known runtime crashes/quirks, and ship a modern web-based TRE compare tool for cross-installation data diagnostics. 10/12 requirements fully satisfied; HARD-02 (corner-snap), HARD-05's clean D3DCompile port, and HARD-03's CORNERSNAP-probe removal deferred to the x64 milestone (all root-caused 32-bit-bound). Per-phase detail archived in `milestones/v2.3-ROADMAP.md`; audit `milestones/v2.3-MILESTONE-AUDIT.md`.

**Delivered:**
- **DPVS config-gate (HARD-01)** — occlusion outdoor-on / indoor-off as `occlusionMode = auto|on|off`, F11 override preserved
- **Machine portability (PORT-01/02)** — Miles redist vendored + postbuild-repointed (codec-repair guard); `client.cfg.template` + `setup-client.ps1` de-hardcode stage paths + clean `client_d.cfg`
- **Instrumentation removal + Options FATAL (HARD-03 D-15 / HARD-04)** — `DpvsProfileInstrumentation` grep-zero; Options window no longer FATALs; CORNERSNAP probes kept as the x64 harness
- **32-bit-bound quirks root-caused → x64 (HARD-02 / HARD-05)** — corner-snap = 32-bit codegen float transient; D3DCompile port re-fights the gl11 shader battle (satisfied-by-Fix-A for v2.3)
- **TRE compare tool (TRE-01..05)** — standalone localhost web app: two-install set-delta + merged-virtual-tree file diff, virtualized filterable tree UI, per-file drill-in with the metadata-vs-content honesty distinction

**Key context:**
- Core invariant held: every client-touching phase (24–27) left the client bootable to character select under both `rasterMajor=5` and `=11` (the TRE tool is a standalone web app, outside that invariant)
- The TRE tool's first real use case (SWGSource-vs-whitengold space-asset diff) is unblocked; the parser is vendored from `D:/Code/swg-blender-plugin/swg_pipeline/` (the stale `D:/Code/swg-tools` pointer is wrong)
- Excluded: SWG-Source community-compat sync, gameplay-parity work, Nyquist validation backfill for phases 18/19–22 (milestone audit stands as the verification record)

## Next Milestone: x64 Port (planned)

The v2.3 deferrals converge on one milestone. The cantina door-snap (HARD-02), the clean `D3DXCompileShader`→`D3DCompile` / D3DX-removal port (HARD-05), and the CORNERSNAP-probe removal (HARD-03 remainder) are all bound to the 32-bit toolchain — the x64 build is where the codegen environment changes, D3DX removal becomes unavoidable, and a working x64 reference exists (Restoration). The 64-bit port also addresses the chronic 32-bit OOM crash. Tracked in `todos/pending/2026-06-13-64bit-x64-port.md`. Run `/gsd-new-milestone` to scope it (questioning → research → requirements → roadmap).

## Prior State: v2.1 Decruft SHIPPED (2026-05-27)

v2.1 re-applied the orphaned v2.0 dead-code removals (CLEAN-01..04) against the active Koogie/MSBuild tree: five dormant subsystems — trackIR/stationapi/SwgClientSetup/lcdui (Phase 12), VideoCapture (13), Vivox voice chat (14), and the XPCOM/Mozilla in-game browser (15) — were fully unlinked and deleted, plus a final tech-debt cleanup pass (Phase 16). The client stays bootable to character-select under **both** D3D9 (`rasterMajor=5`) and D3D11 (`=11`) after every removal. Audit: `tech_debt` (7/7 DECRUFT satisfied, 0 blockers, 28/28 integration). See `MILESTONES.md` + `milestones/v2.1-*`.

**Prior:** v2.0 (shipped 2026-05-25) delivered the modern MSVC/C++20/MSBuild client with selectable D3D9 + D3D11 renderers. **Key pivot (Phase 9, "Option D"):** the original CMake/whitengold build was *replaced* by adopting Koogie's already-MSVC/C++20-migrated **MSBuild** tree wholesale (merge `479d35df3`). The active build is `src/build/win32/swg.sln`; the CMake build that v1 + early-v2 docs describe is superseded (kept for history).

## Shipped Milestone: v2.1 — Decruft (closed 2026-05-27)

**Goal (MET):** Re-applied the orphaned CLEAN-01..04 dead-code removals against the active Koogie/MSBuild tree, shrinking the client's surface area before any SWG-Source upstream import work. All 7 DECRUFT requirements satisfied across Phases 12–16; per-phase detail archived in `milestones/v2.1-ROADMAP.md`.

**Target removals (each its own boot-gated step):**
- Vivox voice chat — `vivoxSharedWrapper_debug.lib` (SwgClient `libraries_d.rsp`) + `CuiVoiceChatManager` source
- VideoCapture — `VideoCapture_debug.lib`
- XPCOM/Mozilla in-game browser — `libMozilla.vcxproj` (swg.sln) + `libMozilla/include/public` (`includePaths.rsp`)
- lcdui (G15 LCD) — `lcdui.vcxproj`
- SwgClientSetup — `SwgClientSetup.vcxproj`
- trackIR/ + stationapi/ orphaned directories

**Key context:** These removals were done once on the original whitengold **CMake** tree (Phase 7, CLEAN-01..05) but the Phase 9 "Option D" base swap to Koogie's MSBuild tree orphaned them — the dead code is dormant-but-present in the active build. Reference diff template: the whitengold (swg-client) Phase 07 removal commits. The Mozilla↔`/Zc:wchar_t` interlock is already resolved (Phase 9 went `char16_t`), so XPCOM removal is a clean unlink. **Invariant:** every step leaves the client bootable to character select under both `rasterMajor=5` (D3D9) and `=11` (D3D11).

**Progress:** ✓ Phase 12 complete (2026-05-25) — DECRUFT-01/-02/-03 done: trackIR + stationapi orphan dirs deleted, SwgClientSetup dropped from swg.sln, and lcdui (G15 LCD) fully removed (live UI de-wired to no-op stubs + build graph + dirs). Dual-renderer boot baseline re-established (both renderers to character select). A recurring finding drove 4 deviations: dead modules were still *linked* via inline `.vcxproj` deps (not the vestigial `.rsp` files), and the `/FORCE` linker masks unresolved-symbol false-passes — so removal-step build gates must check for 0 unresolved externals, not just MSBuild exit 0.
✓ Phase 13 complete (2026-05-26) — DECRUFT-04 done: VideoCapture/AudioCapture middleware fully unlinked (live `CuiIoWin` caller + `SwgVideoCapture` wrapper + 15 lib tokens/8 paths removed atomically; all dead `#if 0` source + `.rsp` + 10 editor `.vcxproj` refs purged; vendored `src/external/3rd/library/videocapture/` tree deleted). Repo-wide grep incl `.vcxproj` == 0; Debug+Release link gate clean (2138/0/0, 2104/0/0); dual-renderer boot (D3D9+D3D11) human-confirmed to Tatooine. Bink/Vivox/Miles preserved. Next: Phase 14 (Vivox voice-chat source removal — higher-risk live-UI surgery).
✓ Phase 14 complete (2026-05-26) — DECRUFT-05 done: Vivox voice-chat subsystem fully removed from source and build. ~24 voice source files + 3 voicechat network messages deleted, 10 live callers + 5 in-repo registrations de-wired, 6 CuiPreferences voice keys stripped, vivox/VChat/libsndfile unlinked from SwgClient (3 configs) + all vestigial `.rsp`/editor `.vcxproj` refs purged, and the 3 vendored voice trees (vivox/, vivoxSharedWrapper/, soePlatform/VChatAPI/) deleted. Repo-wide `rg -i vivox src` == 0; Debug+Release link gate clean (0 unresolved); dual-renderer boot (D3D9+D3D11) human-confirmed to character-select. Code review caught + fixed a real regression (CR-01: voice-enum deletion shifted radial-menu ordinals off their retail-datatable rows — fixed with ordinal-preserving placeholders). Deferred (non-Decruft): DEF-14-01 Optimized SAFESEH config defect. Next: Phase 15 (XPCOM/Mozilla removal + milestone gate — highest-risk surgery).
✓ Phase 15 complete (2026-05-27) — DECRUFT-06/-07 done: XPCOM/Mozilla in-game browser fully removed + v2.1 milestone gate PASS. The 3 `SwgCuiWebBrowser*` units (the only `libMozilla::` consumers) deleted, all live callers + 5 `IsA(TUIWebBrowser)` focus sites de-wired, `TUIWebBrowser` deleted outright from `UITypeID.h` (ordinal never serialized — no placeholder, unlike CR-01), TCG browser tie severed (libEverQuestTCG kept), Mozilla-family link tokens stripped from `SwgClient.vcxproj` + `.rsp` + 7 editors + SwgGodClient, `libMozilla.vcxproj` dropped from `swg.sln` (11 GUID locations), and the vendored `libMozilla/` tree (1,866 files: XPCOM/Gecko SDK) deleted. Milestone-wide P12–P15 residue sweep == 0 / KEEP-listed (incl. lcdui A1 cleanup); Debug+Release link gate clean (0 unresolved; Optimized EXEMPT per DEF-14-01); dual-renderer boot (rasterMajor=5 D3D9 + =11 D3D11) human-confirmed to character-select with correct HUD/radial/IME focus. Verified 9/9 must-haves; code review 0 blockers (3 advisory warnings, all the deliberate T-15-02/D-04a TCG-tie severance — TCG revival out of scope). **v2.1 Decruft COMPLETE (Phases 12–15; DECRUFT-01..07 all done).** Next: v2.2 Visual Parity.
✓ Phase 16 complete (2026-05-27) — v2.1 tech-debt cleanup (post-milestone-audit closure, no new product reqs): unlinked the dead SwgGodClient `989crypt.lib` latent-LNK1181 token (soePlatform KEEP-list + the separate live `crypto.lib` preserved), confirmed the 4 editor vcxproj lcdui-clean (verify-only), and removed the cosmetic P12–P15 source residue — the dead `finalUrl` browser-CS URL block (+ now-dead `shellapi.h`/`ConfigClientGame.h` includes) in `SwgCuiHudAction.cpp` and the orphaned voice-volume API (2 statics + 2 REGISTER_OPTION persistence lines + 4 accessors + 4 decls) in `CuiPreferences.cpp/.h`. All grep-zero gates green; SwgClient Debug+Release relink clean (0 unresolved externals, Optimized EXEMPT per DEF-14-01); single rasterMajor=11 (D3D11) boot to character-select human-confirmed (no-behavior-change phase → single boot replaces the dual-renderer matrix). Verified 8/8 must-haves (D-01..D-08); code review 0 blocker/0 warning/1 INFO (the `httpParams` builder is now write-only — 16-02's intentional narrow scope, follow-up-pass candidate). Discovered + ruled out a pre-existing Options-window crash (`checkShowToolbarCommandCooldownTimer` `.ui`-data mismatch from feature commit `d1b3c0eaf` — NOT a Decruft regression; captured as a pending todo). **v2.1 Decruft milestone-audit tech-debt fully closed.**

## Shipped Milestone: v2.2 — Visual Parity (closed 2026-06-12)

**Goal (MET):** Closed the D3D11 visual gaps against the D3D9 baseline — character-select beachhead first (Phase 17 asset-PS pipeline), then outward to interiors, open world, gamma, UI, and effects. All 13 requirements satisfied (CHAR-01..03, UI-01/02, WORLD-01..03, GAMMA-01, FX-01/02, GEO-01, DPVS-01). Every COMPARISON.md gap bucket closed: asset PS pipeline, gamma LUT (pre-Present curve pass), round minimap, multi-stage sampling (FFP combiner cascade), load-screen seam, exterior geometry (closed as no-real-distortion on settled captures), particles/ribbons, DPVS remeasure. Per-phase detail archived in `milestones/v2.2-ROADMAP.md`; ad-hoc 19–22 execution record in `milestones/v2.2-MILESTONE-AUDIT.md` + `.planning/handoff/2026-06-11-cantina-parity-COMPLETE-rendering-bug-list-next.md`.

---

## What This Is

whitengold is the leaked Star Wars Galaxies source tree (NGE-era ~2010 build, leaked
Feb 2015). This project takes the **client** — `SwgClient` and its ~70-project
dependency graph — and modernises it off Visual Studio 2005 onto a current
**MSVC / C++20 / MSBuild + Visual Studio 2022** toolchain (via Koogie's
`MSVC-CPP20-Upgrade` base, adopted in Phase 9) so it can be compiled, debugged, and
launched against a community-run **SWG-Source** server VM. The long-arc
goal is full feature parity with the live NGE client (ground/space, professions,
housing, GCW, jukebox). v1 (compile + launch) and v2.0 (modernisation: dead-code,
STL swap, DPVS verdict, D3D11 renderer) are complete; v2.1 (Decruft) re-applies the
orphaned dead-code removals to the active MSBuild tree; v2.2 targets D3D11 visual parity.

> **Build-system note:** v1 + early-v2 sections below describe the original **CMake**
> port (the "whitengold" tree). Phase 9 "Option D" replaced it with Koogie's
> **MSBuild** solution (`src/build/win32/swg.sln`) — the active build. CMake-specific
> requirements/decisions are historical; the engine-level architecture they describe
> still holds.

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

**v2.3 Hardening (shipped 2026-06-15) — consolidation + first web tool:**

- ✓ **HARD-01** — DPVS occlusion config-gated (`occlusionMode = auto|on|off`, POB-cell auto-gate, F11 override) — v2.3 (Phase 24)
- ✓ **HARD-04** — Options-window FATAL fixed (`checkShowToolbarCommandCooldownTimer` CodeData/.ui mismatch) — v2.3 (Phase 26)
- ✓ **PORT-01/02** — machine-portable stage (Miles redist vendored/repointed; `client.cfg.template` + `setup-client.ps1`; `client_d.cfg` cleaned) — v2.3 (Phase 24)
- ✓ **TRE-01..05** — standalone localhost TRE compare tool: cfg search-path scan, set-delta, merged-virtual-tree file diff, per-file drill-in, virtualized filterable SPA — v2.3 (Phases 28–30)
- ◑ **HARD-03 (D-15)** — D-15 DPVS instrumentation stripped (grep-zero); CORNERSNAP-probe removal deferred to x64 — v2.3 (Phase 26)
- ◑ **HARD-05** — satisfied-by-Fix-A SEH guard; clean D3DCompile/D3DX-removal port deferred to x64 — v2.3 (Phase 27)
- ⏸ **HARD-02** — corner-snap root-caused as a 32-bit codegen float transient (CONSULT-43); deferred to x64

**v2.2 Visual Parity (shipped 2026-06-12) — D3D11 matches the D3D9 baseline:**

- ✓ **CHAR-01/02/03** — char-select skin/clothing/eyes/head at D3D9 parity via the asset-PS pipeline (0/9 → 9/9 binds) — v2.2 (Phase 17)
- ✓ **UI-01** — load-screen half-texel seam eliminated, central single-site fix — v2.2 (Phase 18)
- ✓ **WORLD-01/02/03** — open-world textures, multi-stage FFP combiner cascade, interior lighting parity — v2.2 (ad-hoc 19/20)
- ✓ **GAMMA-01** — D3D9 gamma ramp as pre-Present curve pass (no sRGB double-correction) — v2.2 (ad-hoc 19, `493287510`)
- ✓ **UI-02** — round minimap via `ui_radar.psh` PSRC override, screenshot-verified — v2.2 (ad-hoc 20, `e4f94a0f6`)
- ✓ **FX-01/02** — particles + ribbons at parity (sticky-dot3 + blend-factors re-land), dual-renderer A/B — v2.2 (ad-hoc 21)
- ✓ **GEO-01** — exterior geometry closed as no-real-distortion on settled captures; skeletal twin fixed earlier — v2.2 (ad-hoc 22)
- ✓ **DPVS-01** — DPVS remeasured under D3D11; Option α REVISED (outdoor keep / indoor remove) — v2.2 (Phase 23)

**v2.1 Decruft (shipped 2026-05-27) — dead-code removals re-applied to the MSBuild tree:**

- ✓ **DECRUFT-01** — trackIR + stationapi orphan dirs deleted; ClientHeadTracking de-wired to no-op stubs — v2.1 (Phase 12)
- ✓ **DECRUFT-02** — SwgClientSetup dropped from `swg.sln` + dir deleted — v2.1 (Phase 12)
- ✓ **DECRUFT-03** — lcdui (G15 LCD) fully removed (live UI stubbed + 7 sln deps + `.rsp` purge) — v2.1 (Phase 12)
- ✓ **DECRUFT-04** — VideoCapture unlinked + vendored tree deleted; Debug+Release link clean — v2.1 (Phase 13)
- ✓ **DECRUFT-05** — Vivox voice chat fully removed (source + build + 3 vendored trees); CR-01 radial-ordinal regression fixed — v2.1 (Phase 14)
- ✓ **DECRUFT-06** — XPCOM/Mozilla in-game browser fully removed (`libMozilla.vcxproj` + 1,866-file tree) — v2.1 (Phase 15)
- ✓ **DECRUFT-07** — full-removal-set dual-renderer boot to character-select human-confirmed (D3D9 + D3D11) — v2.1 (Phase 15, re-confirmed Phase 16)

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

- **Tech stack (current, post-v2):** Windows-only, VS 2022, **MSBuild** (`src/build/win32/swg.sln`),
  **C++20**, **MSVC STL** — Linux work out of scope (server-side toolchain anyway).
  *(v1 used CMake + C++17 + STLPort; superseded by Phase 9 Option-D.)*
- **Source edits expected (v2+):** unlike v1's zero-source-edit rule, v2 modernisation
  edits engine source freely (STL swap, D3D11 plugin). Keep diffs focused per phase.
- **Don't modify Koogie's diagnostic patches** without strong reason — fix the caller/data instead.
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
| **Phase 9 "Option D": adopt Koogie's MSVC/C++20 MSBuild tree wholesale** (`479d35df3`) instead of hand-migrating STLPort in the CMake tree | Koogie already solved the MSVC migration; collapsed ~30 risky refactor commits into ~3 | ✓ Good — STL swap done, Tatooine zone-in PASS. Side effect: CMake build + Phase 7 dead-code removals orphaned (active tree is MSBuild) |
| **D3D11 as a second selectable renderer** (`gl11_d.dll`), keep D3D9 (`gl05_d.dll`) buildable (invariant D-05) | De-risks the port; lets D3D9 stay the known-good reference for visual parity | ✓ Good — both selectable via `rasterMajor`; single `Gl_api` loader multiplexes |
| **v2.1 cleanup-first, risk-gradient ordering** (pure deletes → lib unlinks → live-source surgery) before v2.2 visual work | Re-establish the dual-renderer boot baseline on the low-risk deletes before the riskier Vivox/XPCOM source surgery; shrink surface area before upstream imports | ✓ Good — every removal step boot-verified; v2.1 closed clean (7/7 DECRUFT) |
| **`/FORCE` link-grep gate** — grep link output for `unresolved external symbol` (==0), not just MSBuild exit 0 | SwgClient links under `/FORCE`, which downgrades unresolved externals to warnings and still emits a binary with exit 0 — exit 0 is NOT proof of a clean link | ✓ Good — caught 2 real Phase 12 defects (live `989crypt.lib` dep, orphaned ClientHeadTracking callers) |
| **Ordinal-preserving placeholders** for deletions from positional enums/tables that mirror retail-TRE row indices | Mid-sequence enum deletes silently shift surviving ordinals off their datatable rows — a regression the link + boot gate cannot catch | ✓ Good — CR-01 (radial-menu ordinal shift from voice-enum delete) caught in code review + fixed with `RESERVED_*` placeholders (Phase 14) |
| **Recompile `TAG_PSRC` source, never decompile/re-assemble PEXE** (v2.2 core approach) | Re-assembling asm just reproduces the D3D11-rejected D3D9 bytecode; the discarded PSRC text is the real source of truth | ✓ Good — asset-PS lane delivered char-select parity 9/9; the same PSRC-override recipe later fixed face tint, minimap, emissives |
| **Reflection-driven constant upload (D3DReflect), not copied D3D9 register indices** | fxc assigns SM4 registers by first-use order — copied indices silently bind wrong slots (the purple/green bump-arm bug proved it) | ✓ Good — stage→SRV-slot remap + cbuffer offset discovery made multi-texture materials correct |
| **Ad-hoc execution for Phases 19–22** (fix-to-advance debugging sessions instead of plan/execute cycles) | The remaining gaps were entangled runtime bugs best cracked by iterative RenderDoc-driven diagnosis; GSD artifacts would have lagged reality anyway | ✓ Good — all 8 remaining requirements closed in 5 days; milestone audit serves as the verification record. Cost: no per-phase PLAN/VERIFICATION artifacts, Nyquist gaps |
| **Read the D3D9 reference sequence FIRST on any parity bug** (the reference IS the spec) | Symptom-chasing on the broken target invites guessing; diffing against the working renderer cracks it in one read (lighting flicker, blend factors, sticky dot3 all fell this way) | ✓ Good — adopted as default parity-work method (memory: feedback_parity_work_start_from_reference_sequence) |
| **DPVS Option α REVISED, code untouched** (outdoor keep / indoor remove under D3D11) | Both Phase 10 verdicts flipped under the D3D11 driver-overhead profile; acting on a measurement belongs in its own change, not the measurement phase | ✓ Good — operationalized in v2.3 HARD-01 as `occlusionMode = auto|on|off` (auto-gated on POB-cell membership), shipped culling code still untouched |
| **Defer 32-bit-bound quirks to the x64 milestone** (corner-snap HARD-02, clean D3DCompile port HARD-05, CORNERSNAP-probe removal HARD-03) instead of forcing fixes in v2.3 | CONSULT-43 root-caused the residual door-snap as a 32-bit codegen float transient, and the D3DCompile swap re-fights the whole gl11 shader battle — both change correctly only when the codegen environment changes (x64), where D3DX removal is unavoidable and a working x64 reference (Restoration) exists | ✓ Good — root-caused not skipped; Fix-A SEH guard keeps HARD-05's intent met; probes retained as the x64 acceptance harness; audit `gaps_found` reflects the one honest open requirement |
| **TRE compare tool as an isolated, extractable `tools/tre-compare/` package** (uv lib + vendored stdlib parser, zero engine/MSBuild coupling; FastAPI + Vite/React/shadcn, localhost-only) | Keep the new tool outside the client's build graph and boot invariant so it can ship independently and move to its own repo later; read-only v2.3 scope must not preclude future write/extract/repack (TREM-01..03) | ✓ Good — first repo web app shipped; backend importable with zero engine deps; SC#4 verified on a real 231k-row cross-distribution diff |
| **`(length, compressedLength)` as the changed-file signal, never the TOC `crc`** (real xxhash only on drill-in) | The per-entry `crc` is a path-CRC (`Crc::calculate(fileName)`), not a content hash — two archives share it for the same filename while bytes differ; eager full-archive hashing would be prohibitively slow on 100k-entry sets | ✓ Good — cheap tri-state diff + on-demand hash; the metadata-vs-content "honesty distinction" surfaced in the UI (solid-green = content-confirmed only) |

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
*Last updated: 2026-06-15 after **v2.3 Hardening milestone close**. v2.3 shipped + tagged `v2.3` (Phases 24–30; 10/12 requirements satisfied, HARD-02/HARD-05-clean-port/HARD-03-CORNERSNAP deferred to the x64 milestone — all root-caused 32-bit-bound). Delivered: DPVS config-gate, machine portability, Options-FATAL fix, D-15 instrumentation removal, and the repo's first web app (standalone TRE compare tool). Archives: `milestones/v2.3-ROADMAP.md`, `milestones/v2.3-REQUIREMENTS.md`, `milestones/v2.3-MILESTONE-AUDIT.md`. Next: x64 Port milestone (`/gsd-new-milestone`).*
