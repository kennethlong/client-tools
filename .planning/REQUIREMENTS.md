# whitengold — v2 Requirements

**Milestone:** v2.0 — Modernisation (dead code removal, STL swap, DPVS, D3D11)
**Defined:** 2026-05-07
**Core Value:** A clean, modern CMake build with no dead middleware weight, native MSVC STL, and a path toward a D3D11 renderer — all while keeping the client bootable to character select at every step.

---

## v2 Requirements

### CLEAN — Dead Code Removal (Phases 7–8)

- [x] **CLEAN-01**: Orphaned directories deleted from repo — `src/external/3rd/library/trackIR/`, `src/external/3rd/library/stationapi/`, `src/game/client/application/SwgClientSetup/` — none linked or built; zero dev value
- [x] **CLEAN-02**: CMake graph unlinked from config-disabled features — `lcdui_src` (G15 LCD, `disableG15Lcd=true`), `binkw32`+`Bink/` sources (all cutscene flags already set), VideoCapture libs + `FindVideoCapture.cmake` (`PRODUCTION==0` debug command only)
- [x] **CLEAN-03**: Voice chat (Vivox) fully removed from source and CMake — `CuiVoiceChatManager.cpp/.h`, `SwgCuiVoiceFlyBar.cpp/.h`, `CuiVoiceChatEventHandler`, `vivoxSharedWrapper` unlinked from CMake; voice-related preference keys stripped from `CuiPreferences`
- [x] **CLEAN-04**: In-game browser (XPCOM/Mozilla) fully removed from source and CMake — `libMozilla` unlinked, `CuiWebBrowser*`, `UIWebBrowserWidget`, XPCOM bridge code deleted; Mozilla DLLs removed from POST_BUILD copy list. Note: `/Zc:wchar_t-` flag removal deferred to STL-03 (unicode_char_t migration required first; XPCOM constraint lifted)
- [x] **CLEAN-05**: Client compiles clean and boots to character select against the SWGSource VM after each removal step (Track A gate — verified incrementally; ~2x faster world load after 16 dead DLLs removed from import table)
- [ ] **CLEAN-06**: ~40 orphaned editor/tool projects wired into CMake — `engine/client/application/` (ParticleEditor, TerrainEditor, AnimationEditor, Viewer, WorldSnapshotViewer, LightningEditor, SoundEditor, NpcEditor, ClientEffectEditor, DatabaseObjectViewer, ShipComponentEditor, SwooshEditor, TemplateEditor, QuestEditor, RemoteDebugTool, BugTool, CharacterInfoTool, Headless), `engine/shared/application/` (ArmorExporterTool, DataTableTool, LabelHashTool, StringFileTool, WeaponExporterTool, CoreWeaponExporterTool, WordCountTool), `game/client/application/` (SwgGodClient, SwgCsTool, SwgHeadlessClient), `game/server/application/` (SwgConversationEditor, SwgSpaceQuestEditor, SwgSpaceZoneEditor, SwgDraftSchematicEditor, SwgBattlefieldTool), `external/ours/application/` (LocalizationTool, LocalizationToolCon)

### STL — STLPort → MSVC STL Migration (Phase 9)

- [ ] **STL-01**: STLPort 4.5.3 include paths, prebuilt libs, `FindSTLPort.cmake` INTERFACE target, and `stlport_vc143_compat.cpp` compat shim removed from all CMake targets; `_STLP_*` defines dropped globally
- [ ] **STL-02**: `hash_map`/`hash_set` replaced with `unordered_map`/`unordered_set` in ~40 source files (~191 references); mechanical find-and-replace with include updates
- [ ] **STL-03**: `Unicode::unicode_char_t` changed from `unsigned short` to `wchar_t`; `/Zc:wchar_t-` removed, `/Zc:wchar_t` re-enabled (clean after CLEAN-04 removed the XPCOM PRUnichar boundary)
- [ ] **STL-04**: `/FORCE:MULTIPLE` linker flag and `/NODEFAULTLIB:stlport_vc71_static.lib` removed; clean link with no duplicate-symbol workarounds
- [ ] **STL-05**: Client compiles and boots to character select with MSVC STL replacing STLPort end-to-end; dumpbin confirms no stlport symbols remain

### DPVS — Occlusion Culling Experiment (Phase 10)

- [ ] **DPVS-01**: `disableOcclusionCulling` config flag (`RenderWorld.cpp` line 78) exposed via `client.cfg` and tested; frame time (GPU + CPU) measured with and without `resolveVisibility()` in a busy outdoor zone (Mos Eisley cantina plaza or equivalent)
- [ ] **DPVS-02**: If profiling shows no regression (fps neutral or positive): DPVS occlusion culling removed permanently from outdoor (world cell) scenes; portal traversal retained for indoor cells; result documented in `docs/recon/`

### D3D11 — Renderer Plugin (Phase 11)

- [ ] **D3D11-01**: New `Direct3d11` CMake project producing `Direct3d11.dll` satisfying the existing 119-function `Gl_api` function-pointer table; both `Direct3d9.dll` and `Direct3d11.dll` functional; switch by loading different DLL at startup
- [ ] **D3D11-02**: `IDirect3DDevice9` resource management replaced with `ID3D11Device` / `ID3D11DeviceContext`; manual resource management replacing `D3DPOOL_MANAGED`; lost-device recovery paths removed
- [ ] **D3D11-03**: Vertex/pixel shader recompilation — VS/PS 1.1–2.0 HLSL → HLSL 4.0 (`SV_POSITION` semantics, updated constant buffer model)
- [x] **D3D11-04**: FFP pixel shader generator for `D3DTSS_*` texture combiner operations (replaces fixed-function pipeline; SWG uses texture combiners extensively for material combinations) — **SATISFIED 2026-05-16 by DESCOPE verdict from Plan 11-01 D-04 two-phase spike. Combined static-analysis non-empty + Phase B runtime-empty evidence per CONTEXT D-04a → Plan 11-05 (Wave 5) MUST OMIT Direct3d11_FfpGenerator.{h,cpp}. Verdict doc: `.planning/phases/11-d3d11-renderer-plugin/11-01-ffp-spike-finding.md`. Plan summary: `.planning/phases/11-d3d11-renderer-plugin/11-01-SUMMARY.md`. Commit chain: 69af9adb6 → 0293ef310 → 6c11640bc → 266e173b3 → 200cc7694 → 82f068a4a.**
- [ ] **D3D11-05**: Client renders a ground scene using the D3D11 plugin; visual parity with D3D9 baseline on at least one reference zone

---

## v3+ Requirements (deferred)

These are acknowledged but out of scope for v2:

- **32-bit → 64-bit** address space migration (client; server is already 64-bit in SWGSource)
- **Linux server build** target reactivation (server-side toolchain, out of scope for client port)
- **vcpkg adoption** for system-replaceable deps (libxml2, pcre, zlib, Boost)
- **CI / GitHub Actions** — solo dev cadence; GPU passthrough for smoke tests is its own research project
- **`/WX` warnings-as-errors** — re-enable after STL migration settles
- **Asset pipeline / TRE tooling** — community tools exist; not blocking client boot
- **Gameplay feature parity** — combat, AI, quests, professions, housing, GCW, jukebox — long-arc post-v2

---

## Out of Scope (v2)

| Feature | Reason |
|---------|--------|
| Removing DPVS library entirely | Phase 10 is profile-then-decide; library stays in CMake even if culling disabled |
| Server-side modifications | swg-main VM is a black-box endpoint; changes pushed upstream if needed |
| Modifying dsrc scripts or datatables | Server-only content; out of client port scope |
| Replacing Miles Sound System | No drop-in alternative; non-trivial; deferred post-D3D11 |
| Replacing Bink video codec | Already disabled via config; removal is CLEAN-02; replacement is v3+ |

---

## Traceability

| REQ-ID | Phase | Notes |
|--------|-------|-------|
| CLEAN-01 | Phase 7 | Directory deletes — trivial, no CMake changes |
| CLEAN-02 | Phase 7 | CMake unlinks — lcdui, bink, videocapture |
| CLEAN-03 | Phase 7 | Vivox source + CMake removal |
| CLEAN-04 | Phase 7 | XPCOM source + CMake removal; enables STL-03 |
| CLEAN-05 | Phase 7 | Cross-cutting boot gate for Track A |
| CLEAN-06 | Phase 8 | Tool CMake integration (~40 projects) |
| STL-01 | Phase 9 | STLPort CMake removal |
| STL-02 | Phase 9 | hash_map/hash_set → unordered_* |
| STL-03 | Phase 9 | wchar_t migration (depends on CLEAN-04) |
| STL-04 | Phase 9 | FORCE:MULTIPLE removal |
| STL-05 | Phase 9 | Boot verification with MSVC STL |
| DPVS-01 | Phase 10 | Profiling experiment |
| DPVS-02 | Phase 10 | Removal decision (conditional) |
| D3D11-01 | Phase 11 | Plugin scaffold + Gl_api table |
| D3D11-02 | Phase 11 | Resource management |
| D3D11-03 | Phase 11 | Shader recompilation |
| D3D11-04 | Phase 11 | FFP pixel shader generator (SATISFIED 2026-05-16 by DESCOPE verdict; see Plan 11-01 SUMMARY) |
| D3D11-05 | Phase 11 | Ground scene render verification |

**Coverage:** 17 / 17 requirements mapped to phases (100%).

---
*Requirements defined: 2026-05-07 — v2 milestone start*
