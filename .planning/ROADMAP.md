# Roadmap: whitengold — SwgClient Modernisation Port (v2)

## Overview

v2 is a **modernisation milestone** targeting the four biggest technical debts exposed by v1 gameplay research. Phase 0 (dead code removal) goes first to shrink the file count before touching everything else. Removing XPCOM specifically cleans the `/Zc:wchar_t-` constraint so the STL swap in Phase 9 has no external ABI boundary to worry about. Phase 10 (DPVS) is a near-free profiling experiment. Phase 11 (D3D11) is the long-tail renderer work.

Phase numbering continues from v1 (ended at Phase 6).

## Phases

- [x] **Phase 7: Dead Code Removal — Track A** - Remove orphaned directories, CMake-unlink config-disabled features, and source-delete voice chat (Vivox) + in-game browser (XPCOM/Mozilla); verify boot after each step
- [ ] **Phase 8: Dead Code Removal — Track B** - Wire ~40 orphaned editor/tool projects into CMake (no pre-built binaries required; authoring CMakeLists.txt for each tool)
- [x] **Phase 9: STLPort → MSVC STL** - Closed 2026-05-10 via Option D: Koogie merge anchor `479d35df3` (mechanically satisfies STL-01..STL-04) + IFF compat-guard port to `koogie-msvc-cpp20-base` (commit `460f4540d`, port-forward of whitengold `dd78832c4`) delivers Tatooine zone-in PASS against SWGSource VM, satisfying STL-05. Active tree: `D:/Code/swg-client-v2/`. Replan-2 wave structure (Waves 1-6) is research history -- superseded by replan-3 two-plan structure (09-01 merge-anchor + char-select gate; 09-02 compat-guard port + Tatooine gate + closeout).
- [ ] **Phase 10: DPVS Culling Experiment** - Profile resolveVisibility() cost on modern GPU; decide remove vs keep; document result
- [ ] **Phase 11: D3D11 Renderer Plugin** - New Direct3d11.dll satisfying existing Gl_api table; both D3D9 and D3D11 plugins selectable at startup

## Phase Details

### Phase 7: Dead Code Removal — Track A
**Goal**: By the end of this phase, all dead/disabled features are removed from the repo and CMake graph in three ordered steps — (1) directory deletes, (2) CMake unlinks, (3) source removals — and the client compiles clean and boots to character select against the SWGSource VM after each step. The XPCOM removal in step 3 is the critical enabling step for Phase 9's wchar_t migration.
**Depends on**: Phase 6 (v1 complete)
**Requirements**: CLEAN-01, CLEAN-02, CLEAN-03, CLEAN-04, CLEAN-05
**Success Criteria** (what must be TRUE):
  1. `src/external/3rd/library/trackIR/`, `src/external/3rd/library/stationapi/`, `src/game/client/application/SwgClientSetup/` no longer exist in the repo
  2. `lcdui_src`, `binkw32`, Bink source files, VideoCapture libs, and `FindVideoCapture.cmake` are absent from all CMakeLists.txt and Find modules; `cmake --build build --config Debug` succeeds with no references to these targets
  3. `CuiVoiceChatManager.cpp/.h`, `SwgCuiVoiceFlyBar.cpp/.h`, `CuiVoiceChatEventHandler`, and `vivoxSharedWrapper` are deleted from source; `vivoxSharedWrapper` does not appear in any `target_link_libraries` call; voice preference keys removed from `CuiPreferences`
  4. `libMozilla` unlinked from CMake; `CuiWebBrowser*`, `UIWebBrowserWidget`, XPCOM bridge source deleted; Mozilla DLLs removed from POST_BUILD staging; `dumpbin /imports SwgClient_d.exe` shows zero references to `xpcom.dll`/`xul.dll`; `/Zc:wchar_t-` flag removed from root CMakeLists.txt
  5. `cmake --build build --config Debug` produces `SwgClient_d.exe` from a clean build after all removals; client boots past login UI and reaches character select against the SWGSource VM
**Plans**: 3 plans (sequential waves, boot verify after each)
  - [x] 07-01-PLAN.md — Directory deletes (trackIR, stationapi, SwgClientSetup, 3 pre-CU header stubs) — Wave 1, requirements CLEAN-01, CLEAN-05
  - [x] 07-02-PLAN.md — CMake unlinks (lcdui_src/G15 LCD, Bink, VideoCapture) — Wave 2, requirements CLEAN-02, CLEAN-05
  - [x] 07-03-PLAN.md — Vivox + XPCOM source removal (Phase 9 gate) — Wave 3, requirements CLEAN-03, CLEAN-04, CLEAN-05. `/Zc:wchar_t-` deferred to Phase 9 STL-03.

### Phase 8: Dead Code Removal — Track B
**Goal**: By the end of this phase, all ~40 orphaned editor and tool projects have `CMakeLists.txt` files and build as executables under the existing CMake graph. Pre-built binaries in `exe/win32/` serve as reference; this phase authors the build system, not new functionality. Tools that link against client engine libs benefit from the dead code already removed in Phase 7.
**Depends on**: Phase 7
**Requirements**: CLEAN-06
**Success Criteria** (what must be TRUE):
  1. `CMakeLists.txt` exists for every tool in: `engine/client/application/` (18 tools), `engine/shared/application/` (7 tools), `game/client/application/` (SwgGodClient, SwgCsTool, SwgHeadlessClient), `game/server/application/` (5 tools), `external/ours/application/` (LocalizationTool, LocalizationToolCon)
  2. `cmake --build build --config Debug` builds all tool targets without error (link failures on missing assets/Oracle/JVM are acceptable; compile + link of the tool binary itself must succeed)
  3. SwgClient_d.exe continues to compile and boot to character select (no regressions from tool CMake wiring)
**Plans**: 4 plans (wave-grouped by tool tier and dependency)
  - [x] 08-01-PLAN.md — engine/shared/application/ CLI tools (13 tools) + FindQt3.cmake + FindMaya.cmake + 3 new library CMakeLists.txt (sharedStatusWindow, sharedTemplate, sharedTemplateDefinition) — Wave 1, requirements CLEAN-06. **8 of 13 tools building; 5 MFC tools deferred to Phase 9 (STLPort ↔ <afxwin.h> conflict).**
  - [~] 08-02-PLAN.md — engine/client/application/ Qt GUI tools + non-Qt tools + DLL targets + MayaExporter (31 tools) — Wave 2, requirements CLEAN-06. **1 building (DebugWindow), 30 deferred with classified blockers (10 MFC ↔ Phase 9, 4 NGE source/API mismatch, 1 SDK gap, 2 build-system gap, 1 link wiring, 12 Qt batch).**
  - [~] 08-03-PLAN.md — game/client/application/ + game/server/application/ — Wave 3, requirements CLEAN-06. **2 building (SwgBattlefieldTool, SwgSchematicXmlParser); 12 deferred (5 MFC, 1 Qt — SwgGodClient, 2 NGE, 4 server-blockers).**
  - [x] 08-04-PLAN.md — external/ours/application/ + boot regression — Wave 4, requirements CLEAN-06. **LocalizationToolCon building; LocalizationTool deferred to Qt batch. SwgClient_d.exe boot regression PASS.**

### Phase 9: STLPort → MSVC STL
**Goal**: By the end of this phase, STLPort 4.5.3 is completely removed from the CMake graph and build output — no prebuilt `.lib` files referenced, no `_STLP_*` defines, no `stlport_vc143_compat.cpp` compat shim, no `/FORCE:MULTIPLE` linker flag. `hash_map`/`hash_set` usage is updated to `unordered_map`/`unordered_set` across ~40 files. `Unicode::unicode_char_t` is `wchar_t`, `/Zc:wchar_t` is enabled. The client compiles and **zones into Tatooine** (not just character-select per replan-2 D-11) against the SWGSource VM at 192.168.1.200:44453 from `build-v145/bin/Debug/SwgClient_d.exe` (v145 toolchain per CLAUDE.md "Build toolchain paths") with the 2016 SWGSource v3.0 community-rebuilt proprietary plugin DLLs staged.
**Depends on**: Phase 7 (XPCOM removal enables clean wchar_t; compat shim removal is safe once STLPort gone)
**Requirements**: STL-01, STL-02, STL-03, STL-04, STL-05
**Success Criteria** (what must be TRUE):
  1. `FindSTLPort.cmake` deleted; no `STLPORT_*` variables in any CMakeLists.txt; `stlport/` include path absent from all `target_include_directories` calls
  2. `stlport_vc143_compat.cpp` deleted; `/FORCE:MULTIPLE` and `/NODEFAULTLIB:stlport_vc71_static.lib` absent from link flags; `cmake --build build-v145 --config Debug` links with no duplicate-symbol errors
  3. `Get-ChildItem ... | Select-String 'std::hash_(map|set|multimap)<'` returns zero results in client tier (server tier excluded per CLAUDE.md "server is read-only contract"); all affected files include `<unordered_map>` / `<unordered_set>` instead
  4. `Unicode::unicode_char_t` is `typedef wchar_t unicode_char_t` (not `unsigned short`); `/Zc:wchar_t-` absent from root CMakeLists.txt; built-in `__wchar_t` default restored
  5. `dumpbin /imports build-v145/bin/Debug/SwgClient_d.exe` shows zero stlport symbols; client zones into Tatooine cleanly against SWGSource VM with the 2016 DLL set staged

**Plans**: 3 plans (Wave 0 baseline DONE; replan-3 two-plan structure adopted 2026-05-10)

  - [x] 09-00-PLAN.md — Wave 0 pre-migration baseline capture (BEFORE-STL grep, runtime FPS + leak count, CRC + std::hash dump, DataTable round-trip, sort stability) — DONE 2026-05-08, requirements STL-05
  - [x] 09-01-PLAN.md (replan-3) — Merge anchor + char-select boot gate: Koogie merge `479d35df3` empirically validated end-to-end via VS 2026/v145 MSBuild on `D:/Code/swg-client-v2/src/build/win32/swg.sln`, staged to `D:/Code/swg-client-v2/stage/`, char-select PASS against SWGSource VM, before-imports-replan3.txt baseline shows zero stlport_*. DONE 2026-05-10, satisfies STL-01..STL-04 mechanically per CONTEXT.md D-14.
  - [x] 09-02-PLAN.md (replan-3) — IFF compat-guard port + Tatooine zone-in gate + closeout: ported whitengold `dd78832c4` IFF chunk-size graceful guard into v2 as commit `460f4540d` on `koogie-msvc-cpp20-base`; Tatooine zone-in PASS against SWGSource VM; after-imports-replan3.txt baseline still shows zero stlport_*; .planning/ top-level files adopted into v2. DONE 2026-05-10, satisfies STL-05.

  **Replan-2 wave structure (research history — superseded by replan-3):**
  The previous wave-based per-file STL sweep (Waves 1-6, archived plans 09-01..09-06 under replan-2 numbering) is preserved in `.planning/phases/09-stlport-msvc-stl/archive/` as research documentation of the per-file approach. Option D (replan-3) supersedes it by adopting Koogie's already-migrated tree wholesale + porting only the SWGSource-VM compat guards, reducing Phase 9 scope from ~30+ commits to ~3 commits across 2 days.

  **Cross-cutting constraints (replan-3):**
  - **Source/build/runtime tree:** `D:/Code/swg-client-v2/` branch `koogie-msvc-cpp20-base` -- whitengold `src/` is read-only research history.
  - **Build entry point:** MSBuild on `D:/Code/swg-client-v2/src/build/win32/swg.sln` with `/p:Configuration=Debug /p:Platform=Win32 /p:PlatformToolset=v145` (NOT CMake -- Koogie did not port to CMake).
  - **Per CLAUDE.md "static CRT" + Option D mechanical satisfaction:** the EXE links MSVC STL statically (/MTd) so NO MSVCP/VCRUNTIME/UCRTBASE imports appear -- strictly stronger proof of MSVC-STL adoption than dynamic-CRT linkage would be.
  - Per CONTEXT.md D-18 (bisect-first): compat-guard ports beyond the IFF guard are conditional on Task 2's Tatooine boot test surfacing the matching FATAL signature. ContrailData/Nebula/POB candidates remained unported in 09-02 (NO-OP outcome).
  - Per CONTEXT.md D-19 (post-Phase-9 PR cadence): each guard commit on `koogie-msvc-cpp20-base` is a future PR candidate against SWG-Source/master, deferred to post-Phase-9 followthrough per the 30-day outreach protocol.
  - Threats T-9-01/02/03 (persisted CRC determinism, sort stability, allocator/leak drift) from Wave 0 baselines were SUPERSEDED -- the Koogie merge anchor's mechanical satisfaction of STL-01..STL-04 + the Tatooine zone-in PASS together close the threat surface; no per-file diff gates were required.

### Phase 10: DPVS Culling Experiment
**Goal**: Measure the cost of `resolveVisibility()` (DPVS occlusion query) on a modern discrete GPU in at least one busy outdoor zone. If disabling occlusion culling produces no fps regression (or a positive gain), remove it permanently from outdoor scenes and document the result. Portal traversal for indoor cells is retained regardless of outcome.
**Depends on**: Phase 7 (clean build; Phase 9 optional — experiment can run before or after STL swap)
**Requirements**: DPVS-01, DPVS-02
**Success Criteria** (what must be TRUE):
  1. `disableOcclusionCulling` config key is read from `client.cfg` and wired to `RenderWorld.cpp` line 78 (or the equivalent existing flag); toggling it at runtime does not crash the client
  2. Frame time measurements (GPU + CPU) taken with and without `resolveVisibility()` in at least one busy outdoor zone; results recorded in `docs/recon/10-dpvs-profiling.md`
  3. If result is "remove": DPVS `resolveVisibility()` call removed from `RenderWorldCamera.cpp` (or equivalent); `#include dpvs` headers removed from outdoor rendering path; DPVS library still present in CMake (needed for indoor portal cells); client boots and renders without crash
**Plans**: 7 plans (planned 2026-05-10; wave-grouped instrumentation → capture → verdict → cleanup)
  - [x] 10-01-PLAN.md — Wave 0 scaffolding: `tools/dpvs-profile/analysis.py` + `tools/dpvs-profile/test-protocol.md` + `docs/recon/10-dpvs-profiling.md` skeleton + baseline build/boot smoke — requirements DPVS-01
  - [x] 10-02-PLAN.md — Wave 1 GPU-timing plumbing across plugin DLL boundary: 3 new Gl_api function pointers (dpvsGpuTimingBegin/End/PollResult) + Direct3d9.cpp double-buffered query pool + Graphics::* forwarders — requirements DPVS-01
  - [x] 10-03-PLAN.md — Wave 2 engine module: `DpvsProfileInstrumentation.{h,cpp}` (CSV writer + run-label sanitizer + overlay + ExitChain teardown) + SetupClientGraphics install hook + vcxproj wire — requirements DPVS-01
  - [x] 10-04-PLAN.md — Wave 3 integration (autonomous: false): RenderWorld brackets resolveVisibility with GPU/CPU timer + `getDisableOcclusionCulling` getter + Game::run onFrameEnd hook + CuiIoWin `_DEBUG` F10/F11 intercept + `/setrunlabel` console command + smoke checkpoint — requirements DPVS-01
  - [ ] 10-05-PLAN.md — Wave 4 capture session (autonomous: false): 6 captures × ~10s in Mos Eisley plaza per D-08, alternating ON-OFF; run `analysis.py`; record `verdict = remove|keep` line — requirements DPVS-01
  - [ ] 10-06-PLAN.md — Wave 5 verdict writeup + conditional D-13/D-14 source edits + boot smoke (autonomous: false): populate `docs/recon/10-dpvs-profiling.md`; if verdict=remove apply OCCLUSION_CULLING bit strip at RenderWorld.cpp:903/906 and delete `disableOcclusionCulling` config-key plumbing — requirements DPVS-01, DPVS-02
  - [ ] 10-07-PLAN.md — Wave 6 unconditional D-15 cleanup commit (autonomous: false): delete `DpvsProfileInstrumentation.{cpp,h}` + revert all instrumentation edits across 8 files in one revert-shaped commit; author `10-SUMMARY.md`; final boot smoke — requirements DPVS-01, DPVS-02

### Phase 11: D3D11 Renderer Plugin
**Goal**: A new `Direct3d11.dll` plugin satisfies the existing 119-function `Gl_api` function-pointer table loaded by `clientGraphics`. Both `Direct3d9.dll` and `Direct3d11.dll` are functional and selectable via config at startup. At minimum, the client renders a ground scene using the D3D11 plugin with visual parity to the D3D9 baseline.
**Depends on**: Phase 7 (clean build baseline), Phase 9 (optional — cleaner codebase helps), Phase 10 (DPVS verdict baseline established under D3D9; Phase 11 success criterion #6 re-measures it)
**Requirements**: D3D11-01, D3D11-02, D3D11-03, D3D11-04, D3D11-05
**Success Criteria** (what must be TRUE):
  1. `src/engine/client/library/clientGraphics/Direct3d11/CMakeLists.txt` produces `Direct3d11.dll` that exports the same 119-function table as `Direct3d9.dll`; both DLLs staged to `build/bin/Debug/` by POST_BUILD
  2. Switching `renderer=Direct3d11` in `client.cfg` loads `Direct3d11.dll` instead of `Direct3d9.dll`; client does not crash at startup
  3. All vertex and pixel shaders compile under HLSL 4.0 (`vs_4_0` / `ps_4_0`) with `SV_POSITION` semantics; shader compilation errors resolve before runtime
  4. A pixel shader generator covers the `D3DTSS_*` texture combiner operations used by SWG's material system; at least the common cases (MODULATE, ADD, SELECTARG1) produce visually correct output
  5. Client renders at least one ground scene (character select planet scene or zone entry) using the D3D11 plugin; basic geometry, terrain, and character models are visible (no requirement for pixel-perfect parity with D3D9)
  6. **DPVS remeasurement + architectural decision under D3D11 per CONTEXT D-12 + `docs/recon/10-dpvs-profiling.md` "Phase 11 reconsideration":** re-run the Phase 10 capture protocol (`tools/dpvs-profile/test-protocol.md`) against the D3D11 renderer. Phase 10's underlying data was scene-conditional (`remove` outdoor 3/3 scenes, `keep` indoor 1/1 scene) but the implementation choice was Option α — apply `remove` globally — because the `cullingParameters` bitmask in `RenderWorld.cpp` (lines 908 `#ifdef _DEBUG` / 911 release) is a single value applied to the single `ms_dpvsCamera` for all rendering. The indoor regression was accepted as below human perception (0.66 ms / 2.2%) in exchange for the larger outdoor wins (0.94–2.13 ms / 3.3–9.1%) that dominate gameplay time. Phase 11 must decide between three options once D3D11 numbers are in: (a) keep Option α — if D3D11 numbers still favor outdoor; (b) revert globally to `keep` — if D3D11 makes outdoor net-positive again, restore the bit at both 908 and 911; (c) implement runtime scene-aware split — pass different `cullingParameters` per cell context at the camera-setup site (non-trivial refactor; justify only if D3D11 numbers show a large gap in both directions). Phase 10 instrumentation is removed by plan 10-07 (THROWAWAY per D-15) — Phase 11 must restore equivalent instrumentation or adopt a different measurement approach. Verdict + decision documented in a new `docs/recon/11-dpvs-d3d11-remeasure.md`.
**Plans**: 9 plans (planned 2026-05-15; wave-grouped spike → scaffold → device → resources → shaders → state/draw → smoke → DPVS remeasure → visual-parity + closeout)
  - [x] 11-01-PLAN.md — Wave 1 FFP spike (two-phase: static analysis + runtime instrumentation; D-04a verdict drives Plan 05 generator inclusion) — requirements D3D11-04. **Complete 2026-05-16 — verdict DESCOPE; Plan 11-05 input directive: OMIT Direct3d11_FfpGenerator.{h,cpp}. Bonus deliverable: build-system fix `266e173b3` auto-stages debug binaries to `stage/` for all D3D9 variants + SwgClient.**
  - [x] 11-02-PLAN.md — Wave 2 plugin scaffold + engine-side range-check (Graphics.cpp:65-66 TAG_DX11 + :209-215 range extension) + sln integration + .gitignore + D3D9 baseline reference screenshots for SPEC R6 — requirements D3D11-01. **Complete 2026-05-16 — gl11_d.dll loads + exports GetApi + Direct3d11::install runs + FATALs at scaffold_fatal_stub from Graphics::install (call chain ClientMain → SetupClientGraphics → Graphics.cpp:320 → Direct3d11.cpp:62). D-05 protection PASSED (D3D9 char-select + world load clean). 2 D3D9 baseline PNGs committed at docs/recon/11-d3d11-screenshots/ at world coords (3467,5,-4850) outdoor + (3455,5,-4834) cantina interior. Bonus deliverable: vendored atlmfc include in SwgClient.vcxproj (`dbd7c62dc`) retires the CLI MSBuild .rc compile trap; combined with Plan 11-01's auto-stage cp fix the manual rebuild workflow is fully closed. Commits: 2c518e832 → db2116594 → dbd7c62dc → plan-close.**
  - [x] 11-03-PLAN.md — Wave 3 D3D11 device + DXGI flip-model swap chain + clear-to-color MVP (beginScene/endScene/clearViewport/present slots) — requirements D3D11-01, D3D11-02. **Complete 2026-05-16 — Direct3d11_Device.{h,cpp} created (device + IDXGISwapChain1 with DXGI_SWAP_EFFECT_FLIP_DISCARD + BGRA8 + D24_UNORM_S8_UINT depth-stencil); 5 per-frame Gl_api slots wired (beginScene with Pitfall-3 unconditional OMSetRenderTargets / endScene no-op / clearViewport dark-blue MVP / present vsync=1 with D-13 FATAL on DEVICE_REMOVED / displayModeChanged ResizeBuffers + recreate RTV-DSV); 2 Rule-3 no-op shims (setBrightnessContrastGamma + update); Rule-2 install-time initial clear+present. FATAL boundary advanced from Plan 11-02 baseline (Graphics.cpp:320 setBrightnessContrastGamma) to createTextureData inside TextureList::install (Plan 11-04 target). CHECKPOINT APPROVED with caveat (visible dark-blue deferred to Plan 11-04 because ShowWindow fires after SetupClientGraphics::install completes, which createTextureData FATAL aborts before reaching). D-05 + D-13 invariants maintained. Commits: `28c1f64c4` (Task 1 device + swap chain) → `802ea9c4d` (Task 2 install path + slot bindings + Rule-3 no-ops + Rule-2 install-time clear+present) → plan-close.**
  - [x] 11-04-PLAN.md — Wave 4 resource layer (texture, render target, static/dynamic VB/IB; 6 resource types; ComPtr ownership; D-13 invariant enforced) — requirements D3D11-02. **Complete 2026-05-17 — 14 new source files in Direct3d11/src/win32/ (Direct3d11_TextureData + Direct3d11_RenderTarget + 4 buffer pairs + Direct3d11_VertexBufferDescriptorMap helper); ID3D11Texture2D/Cube/Texture3D + paired ID3D11ShaderResourceView via ComPtr; full DXGI_FORMAT translation for all 17 TF_* engine formats + 2 sentinels with static_assert checking table length; CheckFormatSupport probe in install replaces D3D9 CheckDeviceFormat; staging-texture lock/unlock pattern for engine-fills-after-create path; BIND_RENDER_TARGET|BIND_SHADER_RESOURCE dual-bind on persistent 512x512 baked surface; CopySubresourceRegion replaces D3D9 GetRenderTargetData + system-memory intermediate; dynamic VB ring sized via IDXGIAdapter::GetDesc.DedicatedVideoMemory; Map(WRITE_DISCARD) on wrap / Map(WRITE_NO_OVERWRITE) on append per Pitfall 5; dynamic IB parameterless ctor + setSize-driven ring recreation. 8 Gl_api factory / re-target / setSize slots wired (replaces scaffold_fatal_stub bindings): createTextureData + createStaticVertexBufferData + createDynamicVertexBufferData + createStaticIndexBufferData + createDynamicIndexBufferData + setDynamicIndexBufferSize + setRenderTarget + copyRenderTargetToNonRenderTargetTexture. 2 deviations: (1) Rule 3 - engine-side `friend class Direct3d11_TextureData` on TextureGraphicsData::LockData (Texture.h:35) mirroring existing D3D8/D3D9 friend declarations; (2) Rule 1 - fix static_assert off-by-one on translationTable length (TF_Count + 2 covers TF_Native sentinel). D-05 maintained (`git diff 606697fb4..HEAD -- src/engine/client/application/Direct3d9/` empty). D-13 maintained (14 grep hits all in `//` comment lines; zero functional uses). gl11_d.dll grew from 1,046,016 to 1,125,376 bytes (+77 KB). No Kenny smoke this plan per `autonomous: true` framing — visible-window evidence deferred to Plan 11-05's checkpoint. Commits: `d70432717` (Task 1 texture + render target) → `6ea862275` (Task 2 VB + IB + descriptor map) → `44627b6c2` (Task 3 install path + 8 slot bindings) → plan-close.**
  - [ ] 11-05-PLAN.md — Wave 5 shader layer (D3DCompile vs_5_0/ps_5_0 + .cso disk cache + cbuffer wrapper + conditional Direct3d11_FfpGenerator per D-04a; Pitfalls 1+2+6 enforced) — requirements D3D11-03, D3D11-04
  - [ ] 11-06-PLAN.md — Wave 6 state cache + draw dispatch + input-layout cache + light manager + metrics; cbuffer migration complete (zero SetVertexShaderConstantF/SetPixelShaderConstantF) — requirements D3D11-01, D3D11-02, D3D11-03
  - [ ] 11-07-PLAN.md — Wave 7 subsystem coverage smoke (iterative fix-by-fix loop; Pitfall taxonomy reference; ≥5 min Tatooine outdoor + cantina interior; iteration log) — requirements D3D11-01..D3D11-05
  - [ ] 11-08-PLAN.md — Wave 8 DPVS remeasurement (SPEC R7): external-tool spike (PIX/Nsight) or fallback to restored Phase 10 instrumentation; verdict in docs/recon/11-dpvs-d3d11-remeasure.md; conditional RenderWorld.cpp:902-908 source edit — requirements D3D11-05
  - [ ] 11-09-PLAN.md — Wave 9 visual-parity D3D11 reference screenshots + comparison-notes.md PASS verdict (SPEC R6) + Phase 11 SUMMARY + ROADMAP/REQUIREMENTS/STATE closeout updates — requirements D3D11-05

## Progress

**Execution Order:**
Phases execute in numeric order: 7 → 8 → 9 → 10 → 11
(Phase 9 depends on Phase 7 XPCOM removal; Phase 10 can run before or after Phase 9; Phase 11 runs last)

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 7. Dead Code Removal — Track A | 3/3 | Complete | 2026-05-07 |
| 8. Dead Code Removal — Track B | 4/4 | Closed-as-scoped (12 tools wired, ~30 deferred to Phase 9 + 12.x) | 2026-05-08 |
| 9. STLPort → MSVC STL | 3/3 | Complete (Option D adopted: Koogie tree as v2 base + whitengold IFF compat-guard port; Tatooine zone-in PASS) | 2026-05-10 |
| 10. DPVS Culling Experiment | 4/7 | In Progress|  |
| 11. D3D11 Renderer Plugin | 4/9 | In Progress (Plan 11-01 complete 2026-05-16: D-04a verdict DESCOPE; Plan 11-02 complete 2026-05-16: plugin scaffold + plumbing FATAL verified + D3D9 baseline screenshots; Plan 11-03 complete 2026-05-16: D3D11 device + DXGI swap chain + clear-to-color MVP; FATAL boundary advanced to createTextureData; Plan 11-04 complete 2026-05-17: D3D11 resource layer — textures + VB/IB + render targets — with 8 Gl_api factory slots wired; Plan 11-05 unblocked) | — |
