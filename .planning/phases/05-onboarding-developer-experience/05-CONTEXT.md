# Phase 5: Onboarding + Developer Experience — Context

**Gathered:** 2026-05-05
**Status:** Ready for planning

<domain>
## Phase Boundary

Author a root `README.md` and `docs/build.md` so a developer with a fresh clone can
build `SwgClient_d.exe` and `SwgClient_r.exe` via two CLI commands or the VS 2022 IDE
without setting any environment variables. Verify that claim by deleting `build/` and
running a clean reconfigure + dual-config build with dumpbin gate re-validation. Complete
two Phase 6 pre-flight research tasks (P2.01 sharedNetworkMessages divergence survey,
P2.03 swg-main auth-bypass shape) and record findings in `docs/recon/08-phase6-preflight.md`.
Tag the repo after the clean-clone gate passes.

**Phase 5 scope (confirmed from ROADMAP.md §Phase 5 SC-1 through SC-5):**
- Author `README.md` at repo root (terse: intro + prereqs + commands + links)
- Author `docs/build.md` (deep: architecture + SDK landscape + build guide combined)
- Clean-clone verification: delete `build/` + cmake configure + dual-config build + dumpbin re-validation
- DEV-03 validation: confirm no DXSDK_DIR / MILES_DIR / MOZILLA_DIR env vars required (resolved from vendored paths by Find modules)
- P2.01: survey sharedNetworkMessages / swgServerNetworkMessages divergences vs swg-main
- P2.03: read swg-main LoginServer for auth-bypass mechanism (validateStationKey=0 shape)
- Record pre-flight findings in `docs/recon/08-phase6-preflight.md`
- Create a git tag after DEV-01 gate passes

**NOT in Phase 5 scope:**
- Full client.cfg with real searchPath_NN TRE entries (Phase 6 — LAUNCH-01)
- Boot sequence debugging / login handshake (Phase 6 — LAUNCH-03 / LAUNCH-04)
- Full LoginClientId → LoginEnumCluster handshake trace (Phase 6 planning work)
- CI / GitHub Actions automation (deferred per REQUIREMENTS.md Out of Scope)
- `/WX` warnings-as-errors re-enable (FLAGS-02: deferred to post-v1)

</domain>

<decisions>
## Implementation Decisions

### README Placement and Scope

- **D-01:** Create two documents — root `README.md` (terse) + `docs/build.md` (deep).
  Root is the GitHub landing page; docs/build.md is the extended reference.

- **D-02:** Root `README.md` content: 2-3 sentence project intro (what whitengold is,
  what it does, where it targets), prerequisites block (VS 2022 17.8+, Win11 SDK,
  CMake 3.27+, Git), exact CLI command sequence (cmake configure + cmake build),
  IDE steps (open build/whitengold.sln → Build Solution), and links to docs/ for
  deeper context. No deep architecture content — keep it skimmable.

- **D-03:** `docs/build.md` content: architecture overview + build guide combined.
  Cover the 70-lib dependency graph and wave order (why the build has waves 0-4),
  the SDK landscape (DX9, Miles, Bink, DPVS, Vivox, XPCOM stub, STLPort), post-build
  staging layout (what lands in bin/Debug/ alongside the exe), known build quirks
  (stlport_vc143_compat shim, two minimal source edits: FreeCamera.cpp + SwgCuiAllTargets.cpp),
  and the runtime asset-path setup (client.cfg searchPath_NN pattern for Phase 6).

- **D-04:** `docs/build.md` is markdown only — no HTML twin. It is a living build
  guide updated as the project evolves, distinct from the static recon docs in
  `docs/*.html` that were generated during the research phase.

### Clean-Clone Verification (DEV-01)

- **D-05:** Verification method: delete the `build/` directory, then run the full
  cmake configure + cmake build sequence from scratch on the same machine. This
  eliminates any cached CMake state and constitutes a clean reconfigure proof
  without the overhead of a separate VM.

- **D-06:** Run both Debug and Release configs in the clean reconfigure to confirm
  neither regressed since Phase 4 (`cmake --build build --config Debug` then
  `cmake --build build --config Release`).

- **D-07:** Gate criteria — both exes produced AND dumpbin gates re-run:
  - `bin/Debug/SwgClient_d.exe` and `bin/Release/SwgClient_r.exe` exist
  - `dumpbin /imports SwgClient_d.exe` shows zero `MSVCR*/VCRUNTIME*/MSVCRTD.dll` refs (static CRT)
  - `dumpbin /imports SwgClient_d.exe` shows zero `xpcom.dll`/`xul.dll` refs (XPCOM stub)
  - Same checks on `SwgClient_r.exe`
  This is the same bar as Phase 4's ARTIF-03 — consistent gate, no new criteria.

- **D-08:** After both dumpbin gates pass, create a git tag (e.g., `v1-build-verified`
  or `phase5-complete`) marking the clean-clone verification state.

### Phase 6 Pre-Flight Research

- **D-09:** P2.01 scope — key divergences only. Identify files added, removed, or
  renamed between whitengold's `sharedNetworkMessages/` and swg-main's equivalent.
  Flag any messages relevant to the login flow (LoginClientId, LoginEnumCluster,
  EnumerateCharacterId message types). Do NOT do a full line-by-line diff of every
  message file — server-side-only messages are noise for Phase 6.

- **D-10:** P2.03 scope — auth bypass mechanism only. Read swg-main's LoginServer
  source to understand what `validateStationKey=0` does (what code path it enables,
  what the server does instead of calling the Sony Station endpoint). Record the
  exact client.cfg entry the client must send, and the server-side expectation.
  Do NOT trace the full LoginClientId → EnumerateCharacterId handshake; that is
  Phase 6 planning scope.

- **D-11:** Record both P2.01 and P2.03 findings in a single new document:
  `docs/recon/08-phase6-preflight.md`. This continues the recon/ numbering convention
  (05 = boot sequence, 06 = next-session handoff, 07 = swg-source diff findings,
  08 = Phase 6 pre-flight). Phase 6 planning agent reads this document before planning.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase Goals and Success Criteria
- `.planning/ROADMAP.md` §Phase 5 — authoritative success criteria (5 items), exact
  cmake command that must work from clean clone, P2.01 and P2.03 pre-flight mandates,
  and the second-machine-or-fresh-VM proof requirement (satisfied via D-05 delete+reconfigure)
- `.planning/REQUIREMENTS.md` §DEV-01, §DEV-02, §DEV-03, §DEV-04 — the four requirements
  owned by Phase 5; DEV-05 (.gitignore) was satisfied in Phase 1 but verify it's current

### Project-level Decisions
- `.planning/PROJECT.md` §Key Decisions — locked decisions from Phases 1-4 (STLPort
  fallback strategy, DX9 vendored path, Mozilla XPCOM stub) that must be represented
  accurately in docs/build.md
- `.planning/PROJECT.md` §Constraints — source-edit budget, static CRT, STLPort retained,
  no server edits — must be stated in README and build guide so contributors understand limits
- `.planning/STATE.md` — full accumulated decisions log from Phases 1-4; use for build.md's
  "known quirks" section (stlport_vc143_compat, FreeCamera.cpp, SwgCuiAllTargets.cpp edits)

### Build Guide Source Material
- `docs/research/swgclient-build.md` — complete ~70-project dependency graph, per-config
  library list, SDK locations, compiler settings; primary reference for docs/build.md
  architecture section
- `docs/recon/05-client-boot-sequence.md` — 17-phase boot sequence; useful context for
  docs/build.md's "what you built" section (what SwgClient_d.exe does when launched)
- `docs/recon/07-swg-source-diff-findings.md` — prior swg-main diff; read before P2.01
  to avoid re-covering ground already noted here

### Phase 4 Outputs (verify these before writing verification steps)
- `src/game/client/application/SwgClient/src/CMakeLists.txt` — POST_BUILD staging command;
  document what it places in bin/Debug/ and bin/Release/ for docs/build.md
- `src/cmake/stubs/libMozilla_stub.cpp` — XPCOM stub; document its existence in build.md
  (community-standard pattern, not linking real xul/xpcom)
- `.planning/phases/04-swgclient-executable-link/04-CONTEXT.md` — D-03/D-04/D-05 DLL
  staging decisions; D-06/D-07 STLPort linkage decisions; carry these into build.md

### Phase 6 Pre-flight Research Targets
- `C:\Code\swg-main` §engine/shared/library/sharedNetworkMessages — diff target for P2.01;
  compare directory structure and file list vs `src/engine/shared/library/sharedNetworkMessages/`
- `C:\Code\swg-main` §LoginServer source — read for validateStationKey=0 implementation
  (P2.03); likely in LoginServer/src/shared/ or similar
- `exe/linux/loginServer.cfg` — contains `validateStationKey=1` and `sessionServer=` entries;
  reference for documenting the bypass config in pre-flight findings

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `docs/` site structure: `docs/index.html` is the navigation hub linking to all research
  HTML pages. `docs/build.md` slots in as a markdown sibling; it does NOT need to be
  wired into the HTML site (markdown only per D-04).
- `src/cmake/win32/Find*.cmake` — 12 Find modules, all default to vendored SDK paths under
  `src/external/3rd/library/<name>/`. No environment variables required — verify this is
  documented clearly in both README and build.md.
- Phase 4 POST_BUILD command in `src/game/client/application/SwgClient/src/CMakeLists.txt`
  stages 30 DLLs + `mozilla/` subtree + placeholder `client.cfg` alongside both exes.
  This is the "what's in bin/Debug/ after a successful build" content for docs/build.md.

### Established Patterns
- Exact cmake configure command (locked Phase 1, verified Phase 4):
  `cmake -S src -B build -G "Visual Studio 17 2022" -A Win32 -T host=x64,v143`
- Exact cmake build commands:
  `cmake --build build --config Debug`
  `cmake --build build --config Release`
- Debug build first, Release after (established Phase 1, consistent through Phase 4)
- Dumpbin gate commands from Phase 4 Plan 05 — reuse verbatim for DEV-01 gate.

### Integration Points
- `README.md` lives at repo root (D:/Code/swg-client/README.md). There is no existing
  README.md in the repo currently — Phase 5 creates it.
- `docs/build.md` is a new file alongside existing `docs/*.html` research pages.
- `docs/recon/08-phase6-preflight.md` is a new file alongside existing recon docs
  (05, 06, 07 series).

</code_context>

<specifics>
## Specific Requirements

- Root README must state the configure command verbatim:
  `cmake -S src -B build -G "Visual Studio 17 2022" -A Win32 -T host=x64,v143`
  (The `-T host=x64,v143` flag is non-obvious and must not be omitted)
- IDE step: "Open `build/whitengold.sln` in VS 2022, select SwgClient target, Build Solution"
- Prerequisites: VS 2022 17.8+ (Community edition fine), Windows 11 SDK 10.0.26100+,
  CMake 3.27+, Git — no other installs required; all SDKs are vendored
- Git tag: create after both dumpbin gates pass (name TBD by planner — suggest `v1-build-verified`)
- docs/build.md must explicitly note the two minimal C++ source edits made in v1:
  FreeCamera.cpp (static_cast<real> type cast) and SwgCuiAllTargets.cpp (single-space
  UDL fix) — so contributors understand INV-01 was upheld with minimal exceptions

</specifics>

<deferred>
## Deferred Ideas

- Full CI / GitHub Actions automation — solo hobby project, manual builds sufficient;
  deferred to post-v1 polish per REQUIREMENTS.md Out of Scope
- `/WX` warnings-as-errors re-enable — deferred to post-v1 per FLAGS-02
- Full LoginClientId → EnumerateCharacterId handshake trace from swg-main source —
  Phase 6 planning scope; P2.03 covers only the auth-bypass mechanism, not the
  full message flow

</deferred>

---

*Phase: 5-Onboarding Developer Experience*
*Context gathered: 2026-05-05*
