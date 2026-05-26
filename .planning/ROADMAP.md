# Roadmap: whitengold — SwgClient Modernisation Port

## Milestones

- ✅ **v1.0 Compile + Launch** — Phases 1–6 (CMake foundations → launch/login handshake). Authored in the original whitengold CMake repo; imported here 2026-05-25 for the record.
- ✅ **v2.0 Modernisation** — Phases 7–11 (shipped 2026-05-25). Dead-code removal, STLPort→MSVC STL (via Koogie MSBuild adoption), DPVS verdict, D3D11 renderer plugin. Full detail: `milestones/v2.0-ROADMAP.md`.
- 🚧 **v2.1 Decruft** *(opened 2026-05-25)* — Phases 12–15. Re-apply the orphaned CLEAN-01..04 dead-code removals against the active Koogie/MSBuild tree (`src/build/win32/swg.sln` + `.rsp` files). Cleanup-only; shrinks surface area before upstream-import work. Boot-to-character-select is verified after every removal under both `rasterMajor=5` (D3D9) and `=11` (D3D11).
- 📋 **v2.2 Visual Parity** *(planned, not yet opened)* — close the D3D11 visual gaps catalogued in `docs/research/phase12-baseline/COMPARISON.md`. First phase = asset PS pipeline (the blocker: D3D9 PEXE bytecode rejected by `CreatePixelShader`). *(Reordered after v2.1: cleanup-first shrinks the surface area before visual work + SWG-Source upstream imports.)*

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

<details>
<summary>✅ v1.0 Compile + Launch (Phases 1–6) — historical (whitengold CMake)</summary>

- [x] Phase 1: CMake skeleton — foundations spike & lock
- [x] Phase 2: Shared engine libraries
- [x] Phase 3: Client engine libraries (SDK-heavy tier)
- [x] Phase 4: SwgClient executable link
- [x] Phase 5: Onboarding / developer experience
- [x] Phase 6: Launch + login handshake

Artifacts in `.planning/phases/01..06`. Describes the CMake build superseded by v2's Option-D MSBuild adoption.

</details>

<details>
<summary>✅ v2.0 Modernisation (Phases 7–11) — SHIPPED 2026-05-25 (tech_debt)</summary>

- [x] Phase 7: Dead Code Removal — Track A (CLEAN-01..05). *Done on CMake tree; orphaned by Phase 9 Option-D base swap — re-applied to the MSBuild tree in v2.1 Decruft.*
- [◑] Phase 8: Dead Code Removal — Track B (CLEAN-06). Closed-as-scoped: ~12 tools wired, ~30 deferred.
- [x] Phase 9: STLPort → MSVC STL (STL-01..05). Option D — adopted Koogie MSBuild tree (`479d35df3`); Tatooine zone-in PASS.
- [x] Phase 10: DPVS Culling Experiment (DPVS-01/02). Verdict Option α; D3D11 remeasure deferred.
- [x] Phase 11: D3D11 Renderer Plugin (D3D11-01..05). gl11_d.dll; D3D9+D3D11 selectable; PASS-WITH-DEFERRALS.

Full detail + per-plan history: `milestones/v2.0-ROADMAP.md`. Audit: `milestones/v2.0-MILESTONE-AUDIT.md`.

</details>

### 🚧 v2.1 Decruft (Phases 12–15) — In Progress

**Milestone Goal:** Re-apply the orphaned CLEAN-01..04 dead-code removals against the active Koogie/MSBuild tree, shrinking the client's surface area before any SWG-Source upstream import work. Reference diff template: the original whitengold (swg-client) **Phase 07** removal commits (CLEAN-01..05), retargeted from CMake to MSBuild (`src/build/win32/swg.sln` + `.rsp` response files). Phases are ordered low-risk-first (pure deletes → lib unlinks → live-source surgery) so the boot baseline is re-established before the riskier source removals. **Invariant:** every removal step leaves the client bootable to character select under both `rasterMajor=5` (D3D9) and `=11` (D3D11).

- [x] **Phase 12: Orphaned Directory & Project Deletes** — Delete trackIR/stationapi/SwgClientSetup/lcdui from the MSBuild tree (low-risk deletes + `swg.sln`/`.rsp` drops); re-establish the boot baseline. (completed 2026-05-25)
- [x] **Phase 13: VideoCapture Library Unlink** — Drop `VideoCapture_debug.lib` from the SwgClient `.rsp` files and purge any source/include references (low/medium-risk lib unlink). (completed 2026-05-26)
- [ ] **Phase 14: Voice Chat (Vivox) Source Removal** — Remove `vivoxSharedWrapper` link + `CuiVoiceChatManager`/`SwgCuiVoiceFlyBar`/`CuiVoiceChatEventHandler` source + voice preference keys (higher-risk live-UI surgery).
- [ ] **Phase 15: In-Game Browser (XPCOM/Mozilla) Removal & Milestone Gate** — Drop `libMozilla.vcxproj`, purge the XPCOM include path + browser source + staged Mozilla DLLs, then run the full cross-cutting dual-renderer boot gate (highest-risk surgery + milestone close).

## Phase Details

### Phase 12: Orphaned Directory & Project Deletes
**Goal**: Remove the four low-risk dead modules (orphaned directories + standalone projects) from the active MSBuild tree, re-establishing a clean dual-renderer boot baseline before the riskier source surgery.
**Depends on**: Phase 11 (v2.0 close — D3D11 + D3D9 both selectable)
**Requirements**: DECRUFT-01, DECRUFT-02, DECRUFT-03
**Success Criteria** (what must be TRUE):
  1. `src/external/3rd/library/trackIR/` and `src/external/3rd/library/stationapi/` no longer exist on disk; grep finds zero project or `.rsp` references to either directory.
  2. `SwgClientSetup.vcxproj` is absent from `swg.sln` and `src/game/client/application/SwgClientSetup/` is deleted; no project depends on or builds it.
  3. `lcdui.vcxproj` is absent from `swg.sln` and all lcdui include/lib references are purged from the `.rsp` files (G15 LCD was already `disableG15Lcd=true`).
  4. `swg.sln` builds clean with the four modules gone (no missing-project or unresolved-reference errors).
  5. Client boots to character select against the SWGSource VM under **both** `rasterMajor=5` (D3D9) and `rasterMajor=11` (D3D11) after the deletes.
**Plans**: 3 plans

Plans:
**Wave 1**
- [x] 12-01-PLAN.md — Baseline build + stationapi & trackIR orphan deletes (DECRUFT-01); dual-renderer boot gate

**Wave 2** *(blocked on Wave 1 completion)*
- [x] 12-02-PLAN.md — SwgClientSetup project removed from swg.sln + dir deleted (DECRUFT-02); dual-renderer boot gate

**Wave 3** *(blocked on Wave 2 completion)*
- [x] 12-03-PLAN.md — lcdui de-wire (live UI source + swg.sln 7 deps + .rsp purge) + both dirs deleted (DECRUFT-03); dual-renderer boot gate

**Cross-cutting constraints:**
- Client boots to character select under rasterMajor=5 AND rasterMajor=11

### Phase 13: VideoCapture Library Unlink
**Goal**: Unlink the dormant VideoCapture middleware so the client no longer links `VideoCapture_debug.lib`, with no source/include residue.
**Depends on**: Phase 12
**Requirements**: DECRUFT-04
**Success Criteria** (what must be TRUE):
  1. `VideoCapture_debug.lib` is removed from SwgClient `libraries_d.rsp` and the matching Release `.rsp`; grep finds zero `VideoCapture` references across `.rsp`, source, and include paths.
  2. `swg.sln` builds clean (Debug and Release) with VideoCapture unlinked — no unresolved-symbol or missing-lib link errors.
  3. Client boots to character select against the SWGSource VM under **both** `rasterMajor=5` (D3D9) and `rasterMajor=11` (D3D11) after the unlink.
**Plans**: 3 plans

Plans:
**Wave 1** *(parallel — zero file overlap)*
- [x] 13-01-PLAN.md — Atomic link-unit removal: CuiIoWin live caller + SwgVideoCapture wrapper + SwgClient.vcxproj lib tokens/paths (3 configs); Debug+Release link-grep gate (DECRUFT-04 crit #2)
- [x] 13-02-PLAN.md — Dead `#if 0` source residue + clientGame/clientAudio include-path purge + all `.rsp` + 10 editor `.vcxproj` reference purge (DECRUFT-04 crit #1)

**Wave 2** *(blocked on Wave 1 — vendored-tree delete sequenced last)*
- [x] 13-03-PLAN.md — Delete vendored `videocapture/` tree (D-03) + full-repo zero-ref grep + Debug+Release link gate + dual-renderer boot gate (DECRUFT-04 crit #1/#2/#3)

### Phase 14: Voice Chat (Vivox) Source Removal
**Goal**: Fully remove the Vivox voice-chat subsystem from source and build — the link, the manager/UI source, and the voice preference keys — without breaking the live `CuiPreferences` surface.
**Depends on**: Phase 13
**Requirements**: DECRUFT-05
**Success Criteria** (what must be TRUE):
  1. `vivoxSharedWrapper_debug.lib` is unlinked from the SwgClient `.rsp` files; grep finds zero `vivox` / `Vivox` references across `.rsp`, source, and include paths.
  2. `CuiVoiceChatManager.cpp/.h`, `SwgCuiVoiceFlyBar.cpp/.h`, and `CuiVoiceChatEventHandler` are deleted; grep finds zero references to those symbols in the build.
  3. Voice-related preference keys are stripped from `CuiPreferences` and no remaining caller references a removed voice symbol or key.
  4. `swg.sln` builds clean (Debug and Release) with Vivox gone.
  5. Client boots to character select against the SWGSource VM under **both** `rasterMajor=5` (D3D9) and `rasterMajor=11` (D3D11) after the removal.
**Plans**: TBD
**UI hint**: yes

### Phase 15: In-Game Browser (XPCOM/Mozilla) Removal & Milestone Gate
**Goal**: Fully remove the XPCOM/Mozilla in-game browser from source, project, include path, and stage copy list — the highest-risk live-UI surgery — then run the cross-cutting dual-renderer boot gate that closes the milestone.
**Depends on**: Phase 14
**Requirements**: DECRUFT-06, DECRUFT-07
**Success Criteria** (what must be TRUE):
  1. `libMozilla.vcxproj` is dropped from `swg.sln` and `libMozilla/include/public` is removed from `includePaths.rsp`; grep finds zero `xpcom` / `xul` / `Mozilla` references in project, `.rsp`, and include configuration.
  2. `CuiWebBrowser*`, `UIWebBrowserWidget`, and the XPCOM bridge code are deleted; grep finds zero references to those symbols, and any Mozilla DLLs are removed from POST_BUILD / stage copy lists.
  3. `swg.sln` builds clean (Debug and Release) with the browser gone — no unresolved-symbol, missing-project, or missing-DLL errors.
  4. After the **full** DECRUFT-01..06 removal set, the client compiles clean and boots to character select against the SWGSource VM under **both** `rasterMajor=5` (D3D9) and `rasterMajor=11` (D3D11) — the milestone-wide cross-cutting gate (mirrors v2.0 CLEAN-05).
**Plans**: TBD
**UI hint**: yes

### 📋 v2.2 Visual Parity (planned)

**Milestone Goal:** Close the D3D11 visual gaps so it matches the D3D9 baseline. Requirements to be derived from `docs/research/phase12-baseline/COMPARISON.md` via `/gsd-new-milestone`.

- [ ] **Asset PS pipeline** — re-author/bridge engine pixel shaders so D3D11 binds real asset shaders (D3D9 PEXE bytecode is rejected by `CreatePixelShader`; currently falls back to magenta PS) — **the** blocker. Then: gamma LUT, multi-stage sampling, load-screen half-texel seam, minimap, particles/ribbon.

## Progress

**Execution Order:**
Phases execute in numeric order: 12 → 13 → 14 → 15

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1–6 Foundations → Login | v1.0 | — | Complete (historical) | — |
| 7. Dead Code A | v2.0 | — | Complete (CMake tree) | 2026-05 |
| 8. Dead Code B | v2.0 | — | Closed-as-scoped | 2026-05-08 |
| 9. STLPort → MSVC STL | v2.0 | — | Complete (Option D) | 2026-05-10 |
| 10. DPVS Experiment | v2.0 | — | Complete (Option α) | 2026-05 |
| 11. D3D11 Renderer | v2.0 | — | Complete (PASS-WITH-DEFERRALS) | 2026-05-24 |
| 12. Orphaned Deletes | v2.1 | 3/3 | Complete    | 2026-05-25 |
| 13. VideoCapture Unlink | v2.1 | 3/3 | Complete    | 2026-05-26 |
| 14. Vivox Removal | v2.1 | 1/3 | In Progress|  |
| 15. XPCOM Removal + Gate | v2.1 | 0/TBD | Not started | - |
