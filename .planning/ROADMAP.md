# Roadmap: whitengold — SwgClient Modernisation Port

## Milestones

- ✅ **v1.0 Compile + Launch** — Phases 1–6 (CMake foundations → launch/login handshake). Authored in the original whitengold CMake repo; imported here 2026-05-25 for the record.
- ✅ **v2.0 Modernisation** — Phases 7–11 (shipped 2026-05-25). Dead-code removal, STLPort→MSVC STL (via Koogie MSBuild adoption), DPVS verdict, D3D11 renderer plugin. Full detail: `milestones/v2.0-ROADMAP.md`.
- ✅ **v2.1 Decruft** — Phases 12–16 (shipped 2026-05-27). Re-applied the orphaned CLEAN-01..04 dead-code removals against the active Koogie/MSBuild tree; five dormant subsystems unlinked + deleted, client kept bootable under both renderers. Full detail: `milestones/v2.1-ROADMAP.md`. Audit: `milestones/v2.1-MILESTONE-AUDIT.md`.
- ✅ **v2.2 Visual Parity** — Phases 17–23 (shipped 2026-06-12). D3D11 (`rasterMajor=11`) now matches the known-good D3D9 (`rasterMajor=5`) baseline: asset PS pipeline (PSRC recompile + reflection constants), FFP combiner cascade, lighting/gamma parity, round minimap, particles/ribbons, exterior geometry closure, DPVS remeasure (Option α REVISED). 13/13 requirements. Full detail: `milestones/v2.2-ROADMAP.md`. Audit: `milestones/v2.2-MILESTONE-AUDIT.md`.
- ✅ **v2.3 Hardening** — Phases 24–30 (shipped 2026-06-15). Consolidated the v2.2 parity win: DPVS verdict config-gated, machine-portability (`stage/override` + `stage/miles` paths, `client_d.cfg` cleanup), Options-window FATAL fixed, D-15 instrumentation stripped — PLUS a new standalone web-based TRE compare tool (two-install archive-set + merged-virtual-tree diff). 10/12 requirements satisfied; HARD-02 (corner-snap) + HARD-05 clean port + HARD-03 CORNERSNAP-probe removal deferred to x64 (root-caused 32-bit-bound). Full detail: `milestones/v2.3-ROADMAP.md`. Audit: `milestones/v2.3-MILESTONE-AUDIT.md`.
- 🚧 **v3.0 x64 Port** — Phases 31–36 (in progress, started 2026-06-15). Native 64-bit build keeping BOTH renderers (D3D9 gl05/06/07 + D3D11 gl11) bootable to character select — eliminating the 32-bit-bound defects (cantina door-snap float-codegen transient, the chronic address-space OOM) and removing the x64-hostile legacy D3DX dependency. Reference: SWG Restoration's stable x64 D3D9 client (`D:\SWG Restoration\x64`). Absorbs the v2.3 carry-forwards HARD-02 / HARD-05 / HARD-03-CORNERSNAP. 13 requirements (X64-01..04, BITS-01..03, AUDIO-01/02, SHADER-01, VERIFY-01..03).

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

<details>
<summary>✅ v2.1 Decruft (Phases 12–16) — SHIPPED 2026-05-27 (tech_debt)</summary>

- [x] Phase 12: Orphaned Directory & Project Deletes (DECRUFT-01/02/03). trackIR + stationapi dirs deleted, SwgClientSetup + lcdui dropped from `swg.sln`; dual-renderer boot baseline re-established. (3/3 plans, 2026-05-25)
- [x] Phase 13: VideoCapture Library Unlink (DECRUFT-04). Live caller + wrapper + lib tokens removed atomically; vendored `videocapture/` tree deleted; Debug+Release link clean. (3/3 plans, 2026-05-26)
- [x] Phase 14: Voice Chat (Vivox) Source Removal (DECRUFT-05). ~24 source files + 3 messages + 3 vendored voice trees deleted; CR-01 radial-menu ordinal regression caught + fixed. (3/3 plans, 2026-05-26)
- [x] Phase 15: In-Game Browser (XPCOM/Mozilla) Removal & Milestone Gate (DECRUFT-06/07). `libMozilla.vcxproj` dropped (11 GUID locations) + 1,866-file vendored tree deleted; full-removal-set dual-renderer boot gate PASS. (4/4 plans, 2026-05-27)
- [x] Phase 16: v2.1 Tech-Debt Cleanup (INSERTED — post-audit closure, no new reqs). SwgGodClient `989crypt.lib` dead token unlinked; dead `finalUrl` block + orphaned voice-volume API removed; SwgClient link+boot gate clean. (3/3 plans, 2026-05-27)

Full detail + per-plan history + success criteria: `milestones/v2.1-ROADMAP.md`. Audit: `milestones/v2.1-MILESTONE-AUDIT.md`.

</details>

<details>
<summary>✅ v2.2 Visual Parity (Phases 17–23) — SHIPPED 2026-06-12 (tech_debt)</summary>

- [x] Phase 17: PSRC Census + Char-Select Beachhead (CHAR-01/02/03). Asset-PS pipeline proven end-to-end: `TAG_PSRC` recompile lane + 17-07 PS-input-signature rewrite (binds 0/9 → 9/9) + reflection-driven constants + b0 lighting feed + stage→SRV-slot remap; char-select lit + textured at D3D9 parity. (7/9 plans — 17-08 folded into gap work, 17-05 T4–5 delivered via VERIFICATION; 2026-05-30)
- [x] Phase 18: Load-Screen Half-Texel Seam (UI-01). Single central `setViewport` correction, resolution-independent, 3 splash variants verified in-client. (3/3 plans, 2026-05-30)
- [x] Phase 19: Gamma LUT + Interior Lighting (GAMMA-01, WORLD-03). **AD-HOC** — gamma as a pre-Present curve pass replicating D3D9's exact ramp (`493287510`); interior lighting parity via ambient/directional fixes + sticky-dot3 + alpha-fade leak fixes; cantina Kenny-verified. (2026-06-08..12)
- [x] Phase 20: Open-World PS Extension + Minimap (WORLD-01/02, UI-02). **AD-HOC** — FFP combiner-cascade PS (`f34282592`), per-pixel fog all three PS lanes, terrain blend-factors re-land (`fcfbb2226`), hemispheric sun (`44ade0ae4`), round minimap (`e4f94a0f6`). (2026-06-08..12)
- [x] Phase 21: Particles & Ribbon Effects (FX-01/02). **AD-HOC** — particles fixed by sticky-dot3 + additive blend factors; ribbons verified undistorted in dual-renderer A/B; instrumentation criterion moot (no defect remained to scope). (2026-06-11..12)
- [x] Phase 22: Exterior Geometry (GEO-01). **AD-HOC** — closed as no-real-distortion on settled captures (MCP pano sweep + plaza A/Bs); skeletal twin previously fixed (`905fb5d64`/`3089299ca`). (2026-06-11..12)
- [x] Phase 23: DPVS D3D11 Remeasure (DPVS-01). Phase 10 instrumentation restored CPU-only; 12-CSV live A/B. Both verdicts FLIPPED vs D3D9: outdoor `keep` / indoor `remove` — Option α REVISED in `docs/recon/23-dpvs-d3d11-profiling.md`; shipped culling code untouched. (3/3 plans, 2026-06-12)

Full detail + success criteria: `milestones/v2.2-ROADMAP.md`. Audit (also the de-facto verification record for ad-hoc 19–22): `milestones/v2.2-MILESTONE-AUDIT.md`.

</details>

<details>
<summary>✅ v2.3 Hardening (Phases 24–30) — SHIPPED 2026-06-15 (gaps_found: HARD-02 deferred-to-x64)</summary>

Two independent streams: (A) in-client C++ hardening (24–27), (B) a standalone TRE compare tool (28–30). The client stays bootable to character select under both `rasterMajor=5` (D3D9) and `=11` (D3D11) after every client-touching phase.

- [x] Phase 24: DPVS Config-Gate + Machine Portability (HARD-01/PORT-01/PORT-02). Occlusion auto-gated on POB-cell membership (`occlusionMode = auto|on|off`, F11 override preserved); Miles redist vendored + postbuild-repointed; `client.cfg.template` + `setup-client.ps1` de-hardcode stage paths + clean `client_d.cfg`. Dual-renderer boot verified. (3/3 plans, 2026-06-13)
- [~] Phase 25: Cantina Corner-Snap Fix (HARD-02). **Closed-by-deferral 2026-06-14** — CONSULT-43: interior snap resolved-by-config; residual door-snap root-caused as a 32-bit codegen float transient (cell→world Y), parked for x64. Planned re-entrancy guard 25-01 superseded by the diagnostic finding, not executed.
- [x] Phase 26: Instrumentation Removal + Options-Window FATAL (HARD-03 D-15 / HARD-04). D-15 DPVS instrumentation stripped atomically (`6c95fa990`, grep-zero; CORNERSNAP probes KEPT as the x64 door-snap harness); Options window FATAL confirmed fixed. (2/2 plans, 2026-06-14)
- [x] Phase 27: D3DCompile Port (HARD-05). Satisfied-by-Fix-A — D3DCompile swap attempted + reverted (`c0f890875`, re-fights the full gl11 shader battle); Fix-A SEH guard retained; census + A/B baseline kept as x64 inputs. (3/3 plans, 2026-06-14)
- [x] Phase 28: TRE Compare Tool — Foundation (TRE-01). Headless, fully unit-tested backend: vendored parser + cfg search-path scanner + engine-faithful merged-virtual-tree builder. (4/4 plans, 2026-06-14)
- [x] Phase 29: TRE Compare Tool — Diff Engine + API (TRE-02/03/04). Set-level + file-level diff (length/compressedLength signal, on-demand xxhash) + 4 FastAPI routes + sqlite index cache. (3/3 plans, 2026-06-15)
- [x] Phase 30: TRE Compare Tool — Frontend SPA (TRE-05). React/Vite/shadcn virtualized tree-diff UI: install picker, set-delta table, badges, filter, search, per-file detail; SC#4 human-approved on a real 231,086-row diff (SWGSource × SWG Infinity). (3/3 plans, 2026-06-15)

Full detail + success criteria: `milestones/v2.3-ROADMAP.md`. Audit: `milestones/v2.3-MILESTONE-AUDIT.md`.

</details>

### 🚧 v3.0 x64 Port (Phases 31-36) - In Progress

**Milestone Goal:** Port the modernized client to a native **x64** build, keeping **both** renderers
(D3D9 gl05/06/07 + D3D11 gl11) bootable to character select, to eliminate the 32-bit-bound defects
root-caused across v2.x (the cantina door-snap float-codegen transient, the chronic address-space OOM
crash) and remove the x64-hostile legacy D3DX dependency. **Core invariant (extended):** every
client-touching phase must reach boot-to-character-select parity in the **x64** build under both
`rasterMajor=5` (D3D9) and `=11` (D3D11), and must never regress either renderer in the 32-bit build.
Debug exe reads `client_d.cfg`. A proven reference exists: SWG Restoration ships a stable x64 D3D9
client at `D:\SWG Restoration\x64` (door-snap-free; the third-party x64-lib availability map);
the Miles 9.3b SDK is cloned at `D:/Code/milesss-v9.3b/`.

**Dependency chain (why this order):** the 64-bit-correctness source work (BITS) and the D3DX removal
(SHADER) are *prerequisites* for a clean x64 build - D3DX is x64-hostile and the x87 inline asm /
truncation defects block compile/link - so they front-load (Phases 31-32) before the first linking
x64 client (Phase 33). The two renderers are separately-verifiable boot gates (D3D9 in 33, D3D11 in
34). The Miles 7.2e->9.3b port is the one third-party that needs real code work, a self-contained chunk
(Phase 35). Verification + CORNERSNAP cleanup come last (Phase 36): VERIFY-01 (door-snap) and VERIFY-02
(OOM) can only be checked once the x64 build runs in-world, and VERIFY-03 (strip the CORNERSNAP probes)
must come AFTER VERIFY-01 confirms the door-snap clean against them - they are its acceptance harness.

- [x] **Phase 31: 64-bit Correctness Foundation** - x87 inline asm to intrinsics + pointer/int truncation + struct-packing/serialization-width audit; the tree compiles x64-clean (BITS-01/02/03)
 (completed 2026-06-16)
- [ ] **Phase 32: D3DX to d3dcompiler_47** - port the legacy D3DX shader-compile path to `D3DCompile` and remove x64-hostile D3DX from the build; both renderers still compile/load shaders (SHADER-01)
- [ ] **Phase 33: x64 Build Platform + D3D9 Renderers** - add the `x64` platform to the solution + every `.vcxproj`, resolve all third-party x64 libs, ship the first linking x64 client; D3D9 (gl05/06/07) boots to character select under rasterMajor=5/6/7 (X64-01/04/02)
- [ ] **Phase 34: x64 D3D11 Renderer** - rebuild gl11 as x64; the x64 client boots to character select under rasterMajor=11 (X64-03)
- [ ] **Phase 35: Miles 9.3b Audio Port** - vendor + port clientAudio from the 7.2e to the 9.3b API and stage the x64 redist/provider set; in-game audio works on the x64 client (AUDIO-01/02)
- [ ] **Phase 36: Verification & CORNERSNAP Cleanup** - confirm the door-snap + OOM-crash classes resolved against the Restoration x64 reference, then strip the CORNERSNAP `_DEBUG` probes (VERIFY-01/02/03)

#### Phase 31: 64-bit Correctness Foundation
**Goal**: The whole source tree compiles x64-clean - x64-illegal inline asm replaced with intrinsics, pointer/integer truncation defects fixed, and data-layout (struct packing / hardcoded `sizeof` / serialization width) audited for IFF/TRE + network-message correctness - so the build path is ready for the x64 platform add.
**Depends on**: Nothing (first phase of v3.0; builds on the shipped 32-bit tree)
**Requirements**: BITS-01, BITS-02, BITS-03
**Success Criteria** (what must be TRUE):
  1. The x87 FPU-control inline asm (`FloatingPointUnit.cpp` `__asm fnstcw/fldcw`) is gone, replaced with `_controlfp`/`_control87`, and a tree-wide `__asm` sweep shows no x64-illegal inline asm survivors in the build path
  2. The touched code compiles with truncation warnings (C4311 / C4312 / C4244) treated as errors - no `(int)pointer` / `DWORD`-holds-pointer survivors
  3. IFF/TRE and network-message struct packing / hardcoded-`sizeof` / serialization-width assumptions are audited and corrected; the 32-bit build still loads assets, parses saved data, and exchanges network messages correctly (no data-layout regression)
  4. The 32-bit client still boots to character select under both `rasterMajor=5` and `=11` after the source changes (no regression while x64 is not yet building)
**Plans**: 6 base plans in 3 waves + 3 gap-closure increments (31-07/08/09, authored mid-execution as the scratch-x64 sweep unmasked successive pre-existing class-(B) layers)
  - [x] 31-01-PLAN.md — scratch Debug|x64 compile-only harness (D-01 worklist generator; gitignored; in-scope TU manifest)
  - [x] 31-02-PLAN.md — BITS-01 FPU + SSE math (FloatingPointUnit _control87, SseMath ×13 + Transform naked-SSE → _mm_*)
  - [x] 31-03-PLAN.md — BITS-01 misc __asm sweep (CollisionUtils/Fatal/Clock/ProfilerTimer/VeCritsec + DebugHelp _M_X64 fork)
  - [x] 31-04-PLAN.md — BITS-02 pointer/int truncation (StaticShader/MemoryManager/Os + Direct3d11 re-truncation; width-correct types)
  - [x] 31-05-PLAN.md — BITS-03 serialization/layout (Archive map uint32_t pin + TargaFormat/AssetCustomization static_asserts)
  - [x] 31-07-PLAN.md — gap-closure 1: the 11 enumerated NEW class-(B) survivors (ByteOrder/RegexServices/MemoryManagerHook __asm, SSE skinning, PathSearch/StatusWindow functional fixes, time_t display audits)
  - [x] 31-08-PLAN.md — gap-closure 2: the ~753 AutoDeltaMap/PackedMap/Set C2665/C2668 cascade (uint32_t wire-stable count pin across all four AutoDelta* families); failing TUs 154→55
  - [x] 31-09-PLAN.md — gap-closure 3 (CAPPED convergence): 4 genuine width members (CuiCombatManager/MeshConstructionHelper/TcpClient/TcpServer) + Unicode cluster reclassified tool-only + harness artifacts confirmed; 55→51, 0 NEW class-(B) → class-(B) source COMPLETE, NO 31-10
  - [x] 31-06-PLAN.md — phase gate: full scratch-x64 sweep + Phase 33 residual worklist (D-02) + 32-bit link-grep (0 unresolved external) + dual-renderer boot smoke (D-08/SC#4) — CERTIFIED 2026-06-16: BITS-01/02/03 complete; boot smoke APPROVED (both renderers zoned into Tatooine, no regression); residual class-(A) tail handed to P33/P35/P36

#### Phase 32: D3DX to d3dcompiler_47
**Goal**: The legacy D3DX shader-compile path is ported to `d3dcompiler_47` (`D3DCompile`) and D3DX is removed from the build, eliminating the x64-hostile dependency that blocks a clean x64 link - landing the v2.3-deferred HARD-05 as a true prerequisite for the first x64 client.
**Depends on**: Phase 31
**Requirements**: SHADER-01
**Success Criteria** (what must be TRUE):
  1. The `D3DXCompileShader` call sites are ported to `D3DCompile` (`d3dcompiler_47`); an asm-shader census is done first so the assembly path is explicitly handled and no VS is silently nulled (no skipped draws)
  2. D3DX (`d3dx9*`) is removed from the build's link/include path - `dumpbin`/grep shows no D3DX dependency in the shader-compile path
  3. Both renderers still compile and load their shaders correctly in the 32-bit build; the client boots to character select and renders under both `rasterMajor=5` and `=11` (the Fix-A SEH guard retained until the asm path is also off D3DX)
**Plans**: 5 plans in 3 execution waves (Wave 0: census GATE + HLSL port in parallel → Wave 1: asm port OR fallback (XOR on the GATE verdict) → Wave 2: D3DX-removal verify + Fix-A disposition + dual-renderer validation). Tri-state GATE verdict: PASS / FAIL-WITH-FOLLOWUP.
  - [x] 32-01-PLAN.md — Wave 0: D-06 census + D3DAssemble dialect probe GATE with a bytecode size+hash diff vs D3DXAssembleShader (tri-state: PASS→asm port / FAIL-WITH-FOLLOWUP→fallback; gates Wave 1)
  - [ ] 32-02-PLAN.md — Wave 0 (parallel): HLSL gl11-ruleset lift (Rules A/B/C verbatim, Rule D DISABLED for D3D9, Rule E conditional — no Rule F exists) + D3DXCompileShader→D3DCompile (BACKWARDS_COMPATIBILITY only, NO PACK_MATRIX_ROW_MAJOR) + vs_1_1 handled + 9 override macros fixed + constant-layout/flag audit + render-correctness (Tatooine bump/dot3)
  - [ ] 32-03-PLAN.md — Wave 1 (IF GATE PASS): asm path D3DXAssembleShader→D3DAssemble (GetProcAddress)
  - [ ] 32-04-PLAN.md — Wave 1 (IF GATE FAIL-WITH-FOLLOWUP): sanctioned D-03 fallback — asm stays on UNGUARDED D3DXAssembleShader (NOT Fix-A-guarded; Fix-A wraps the HLSL path only), asm port scheduled as a HARD must-land-before-Phase-33 follow-up
  - [ ] 32-05-PLAN.md — Wave 2: dumpbin/grep compile-path D3DX-removal proof (non-compile D3DX retained, D-05 — 13 reproducible call sites) + Fix-A disposition per branch + dual-renderer (rasterMajor 5 AND 11) validation; PARTIAL SHADER-01 (compile-path + Win32 only; does NOT by itself unblock the Phase-33 x64 link)
**UI hint**: yes

#### Phase 33: x64 Build Platform + D3D9 Renderers
**Goal**: A native 64-bit client builds and boots - the `x64` platform is added to the solution and every `.vcxproj`, all third-party dependencies resolve as x64, and the D3D9 renderer plugins (gl05/06/07) carry the x64 client to character select. This is the first fully-linking x64 client.
**Depends on**: Phase 32 (D3DX must be gone before the x64 link is clean)
**Requirements**: X64-01, X64-04, X64-02
**Success Criteria** (what must be TRUE):
  1. `SwgClient_d.exe` / `SwgClient_r.exe` build as x64 binaries - `dumpbin` confirms `machine (x64)`; the `x64` platform exists in `src/build/win32/swg.sln` and every dependent `.vcxproj`
  2. All third-party dependencies resolve as x64 (dpvs, bink, pcre, libxml2, icu, jpeg, zlib, discord-rpc, ...) - the x64 client launches with no missing-DLL popup or load failure at boot (Restoration's `D:\SWG Restoration\x64\` is the availability reference)
  3. The gl05 / gl06 / gl07 plugins build as x64 and the x64 client boots to character select under `rasterMajor=5`, `=6`, and `=7`
  4. The 32-bit build still boots to character select under both renderers (the x64 add does not regress Win32)
**Plans**: 6 plans in 5 waves (Wave 1: x64 foundations + asm-D3DAssemble port parallel -> Wave 2: non-compile D3DX -> DirectXMath -> Wave 3: dpvs + engine-lib platform-add -> Wave 4: plugin/exe link + Miles stub + first x64 link -> Wave 5: stage + boot validation)
  - [x] 33-01-PLAN.md - x64 foundations: committed x64-platform.props (N1 guardrail) + libxml2/pcre/jpeg x64 import-libs + tinyxml x64 build + staged-DLL provenance checklist
  - [x] 33-02-PLAN.md - D3DX-removal precondition (asm): ported D3DXAssembleShader -> D3DAssemble (static-local-cached GetProcAddress, 0 unresolved external; x64 d3dcompiler_47.dll exports D3DAssemble per reviews #7a) + Fix-A RETAIN (D-05); gl05 //asm Tatooine render smoke APPROVED 2026-06-17 — D3D9 compile surface now FULLY off D3DX (436189d8a/e057b6d3a)
  - [ ] 33-03-PLAN.md - D3DX-removal precondition (non-compile): D3DXMATRIX/Multiply/Transpose -> DirectXMath (preserve the :4031 transpose), own-impl surface-copy/mesh/save, drop d3dx9 includes; gl05 Tatooine A/B
  - [ ] 33-04-PLAN.md - platform-add: dpvs !_M_X64 CPU-detect guard + x64 config; swg.sln x64 configs + the ~57 boot-path engine/3rd-party StaticLibrary x64 configs (import x64-platform.props, isolated x64 OutDirs)
  - [ ] 33-05-PLAN.md - link: gl05/06/07 + SwgClient x64 link blocks (D3DX/bink/Miles dropped, LIFT libs wired) + Miles #if _M_X64 stub + Bink binkw64.dll swap + the first full x64 link (0 unresolved external symbol)
  - [ ] 33-06-PLAN.md - boot validation: stage-x64/ + dumpbin-x64 every binary + x64 boot to char-select under rasterMajor 5/6/7 + A1-DBGHELP/SSE-ALIGN runtime confirm + 32-bit non-regression (rasterMajor 5 + 11)
**UI hint**: yes (N/A - build-platform phase, no frontend surface; the "UI" substring was a roadmapper false-positive)

#### Phase 34: x64 D3D11 Renderer
**Goal**: The D3D11 renderer plugin (gl11) builds as x64 and carries the x64 client to character select under `rasterMajor=11`, bringing the second renderer to x64 parity.
**Depends on**: Phase 33
**Requirements**: X64-03
**Success Criteria** (what must be TRUE):
  1. The gl11 plugin builds as x64 (`gl11_d.dll` / `gl11_r.dll`, `dumpbin` machine x64)
  2. The x64 client boots to character select under `rasterMajor=11` and renders the world at the v2.2 visual-parity bar
  3. The 32-bit gl11 build still boots to character select under `rasterMajor=11` (no Win32 regression)
**Plans**: TBD
**UI hint**: yes

#### Phase 35: Miles 9.3b Audio Port
**Goal**: The client links the vendored x64 Miles 9.3b SDK with the `clientAudio` call sites ported from the 7.2e API, and the x64 redist + provider set is staged so in-game audio (music, 2D UI, 3D positional, reverb/room-type) works without the half-dead-audio / warning-flood failure mode.
**Depends on**: Phase 33 (needs a linking x64 client to link the x64 Miles lib into)
**Requirements**: AUDIO-01, AUDIO-02
**Success Criteria** (what must be TRUE):
  1. The Miles 9.3b SDK is vendored into `src/external/3rd/library/miles-9.3b/` and the `clientAudio` call sites (`Audio.cpp`, `SoundObject3d.cpp`, `FirstClientAudio.h`) are ported from 7.2e to the 9.x API (incl. the `AIL_room_type` `, 0` signature edit); the x64 client links `mss64.lib`
  2. The x64 Miles redist + provider set (`mss64.dll` + `mss64mp3.asi` / `mss64ogg.asi` / `mss64dsp.flt` / `binkawin64.asi`) is staged next to the x64 exe
  3. In-game audio works on the x64 client - music, 2D UI, 3D positional, and reverb/room-type - with no half-dead-audio / warning-flood failure mode
  4. The 32-bit client (still on Miles 7.2e) is unregressed, or the 7.2e->9.3b swap is applied consistently and audio still works in Win32
**Plans**: TBD

#### Phase 36: Verification & CORNERSNAP Cleanup
**Goal**: Confirm the two headline 32-bit-bound defect classes are resolved by the x64 build - the cantina door-snap (against the kept CORNERSNAP probes and the clean Restoration x64 reference) and the address-space OOM crash - then strip the CORNERSNAP `_DEBUG` instrumentation now that it has served as the door-snap acceptance harness, completing the deferred half of HARD-03.
**Depends on**: Phase 34 (x64 must run in-world under both renderers); Phase 35 (full-session play needs working audio)
**Requirements**: VERIFY-01, VERIFY-02, VERIFY-03
**Success Criteria** (what must be TRUE):
  1. The cantina door-snap is confirmed resolved in the x64 build - verified against the kept CORNERSNAP `_DEBUG` probes and the clean Restoration x64 D3D9 reference behavior (door-exit cell->world handoff stable, no one-frame Y transient)
  2. An extended world-load / play session on the x64 build no longer hits the `MemoryManager` OOM FATAL (`b0780503` class) that 32-bit address-space exhaustion produced
  3. The CORNERSNAP `_DEBUG` instrumentation (`CollisionResolve.cpp` + `CellProperty.cpp`) is removed from shipped code paths **after** the door-snap is verified clean - link-grep shows 0 unresolved on removal
  4. After cleanup, the x64 client still boots to character select and traverses the cantina door cleanly under both `rasterMajor=5` and `=11`
**Plans**: TBD


## Progress

**Execution Order:**
Within v2.3, the client-hardening stream (24 → 25 → 26 → 27) and TRE-tool stream (28 → 29 → 30) were independent and interleaved. Within each stream, phases executed in numeric order.

v3.0 x64 Port executes in strict numeric order 31 → 32 → 33 → 34 → 35 → 36, gated by the dependency chain: correctness foundation + D3DX removal (31–32) before the first x64 link (33); D3D11 (34) after D3D9 (33); audio (35) after a linking x64 client (33); verification + CORNERSNAP cleanup (36) last.

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1–6 Foundations → Login | v1.0 | — | Complete (historical) | — |
| 7. Dead Code A | v2.0 | — | Complete (CMake tree) | 2026-05 |
| 8. Dead Code B | v2.0 | — | Closed-as-scoped | 2026-05-08 |
| 9. STLPort → MSVC STL | v2.0 | — | Complete (Option D) | 2026-05-10 |
| 10. DPVS Experiment | v2.0 | — | Complete (Option α) | 2026-05 |
| 11. D3D11 Renderer | v2.0 | — | Complete (PASS-WITH-DEFERRALS) | 2026-05-24 |
| 12. Orphaned Deletes | v2.1 | 3/3 | Complete | 2026-05-25 |
| 13. VideoCapture Unlink | v2.1 | 3/3 | Complete | 2026-05-26 |
| 14. Vivox Removal | v2.1 | 3/3 | Complete | 2026-05-26 |
| 15. XPCOM Removal + Gate | v2.1 | 4/4 | Complete | 2026-05-27 |
| 16. v2.1 Tech-Debt Cleanup | v2.1 | 3/3 | Complete | 2026-05-27 |
| 17. PSRC Census + Char-Select Beachhead | v2.2 | 7/9 | Complete | 2026-05-30 |
| 18. Load-Screen Half-Texel Seam | v2.2 | 3/3 | Complete | 2026-05-30 |
| 19. Gamma LUT + Interior Lighting | v2.2 | ad-hoc | Complete (ad-hoc) | 2026-06-12 |
| 20. Open-World PS Extension + Minimap | v2.2 | ad-hoc | Complete (ad-hoc) | 2026-06-12 |
| 21. Particles & Ribbon Effects | v2.2 | ad-hoc | Complete (ad-hoc) | 2026-06-12 |
| 22. Exterior Geometry / Skeletal Shards | v2.2 | ad-hoc | Complete (ad-hoc) | 2026-06-12 |
| 23. DPVS D3D11 Remeasure | v2.2 | 3/3 | Complete | 2026-06-12 |
| 24. DPVS Config-Gate + Machine Portability | v2.3 | 3/3 | Complete | 2026-06-13 |
| 25. Cantina Corner-Snap Fix | v2.3 | 0/1 | Closed-by-deferral (→x64) | 2026-06-14 |
| 26. Instrumentation Removal + Options FATAL | v2.3 | 2/2 | Complete | 2026-06-14 |
| 27. D3DCompile Port | v2.3 | 3/3 | Complete (satisfied-by-Fix-A) | 2026-06-14 |
| 28. TRE Tool — Foundation | v2.3 | 4/4 | Complete | 2026-06-14 |
| 29. TRE Tool — Diff Engine + API | v2.3 | 3/3 | Complete | 2026-06-15 |
| 30. TRE Tool — Frontend SPA | v2.3 | 3/3 | Complete | 2026-06-15 |
| 31. 64-bit Correctness Foundation | v3.0 | 9/9 | Complete   | 2026-06-16 |
| 32. D3DX → d3dcompiler_47 | v3.0 | 1/5 | In Progress|  |
| 33. x64 Build Platform + D3D9 Renderers | v3.0 | 2/6 | In Progress|  |
| 34. x64 D3D11 Renderer | v3.0 | 0/TBD | Not started | - |
| 35. Miles 9.3b Audio Port | v3.0 | 0/TBD | Not started | - |
| 36. Verification & CORNERSNAP Cleanup | v3.0 | 0/TBD | Not started | - |
