# Roadmap: whitengold — SwgClient Modernisation Port

## Milestones

- ✅ **v1.0 Compile + Launch** — Phases 1–6 (CMake foundations → launch/login handshake). Authored in the original whitengold CMake repo; imported here 2026-05-25 for the record.
- ✅ **v2.0 Modernisation** — Phases 7–11 (shipped 2026-05-25). Dead-code removal, STLPort→MSVC STL (via Koogie MSBuild adoption), DPVS verdict, D3D11 renderer plugin. Full detail: `milestones/v2.0-ROADMAP.md`.
- ✅ **v2.1 Decruft** — Phases 12–16 (shipped 2026-05-27). Re-applied the orphaned CLEAN-01..04 dead-code removals against the active Koogie/MSBuild tree; five dormant subsystems unlinked + deleted, client kept bootable under both renderers. Full detail: `milestones/v2.1-ROADMAP.md`. Audit: `milestones/v2.1-MILESTONE-AUDIT.md`.
- ✅ **v2.2 Visual Parity** — Phases 17–23 (shipped 2026-06-12). D3D11 (`rasterMajor=11`) now matches the known-good D3D9 (`rasterMajor=5`) baseline: asset PS pipeline (PSRC recompile + reflection constants), FFP combiner cascade, lighting/gamma parity, round minimap, particles/ribbons, exterior geometry closure, DPVS remeasure (Option α REVISED). 13/13 requirements. Full detail: `milestones/v2.2-ROADMAP.md`. Audit: `milestones/v2.2-MILESTONE-AUDIT.md`.
- ✅ **v2.3 Hardening** — Phases 24–30 (shipped 2026-06-15). Consolidated the v2.2 parity win: DPVS verdict config-gated, machine-portability (`stage/override` + `stage/miles` paths, `client_d.cfg` cleanup), Options-window FATAL fixed, D-15 instrumentation stripped — PLUS a new standalone web-based TRE compare tool (two-install archive-set + merged-virtual-tree diff). 10/12 requirements satisfied; HARD-02 (corner-snap) + HARD-05 clean port + HARD-03 CORNERSNAP-probe removal deferred to x64 (root-caused 32-bit-bound). Full detail: `milestones/v2.3-ROADMAP.md`. Audit: `milestones/v2.3-MILESTONE-AUDIT.md`.
- ✅ **v3.0 x64 Port** — Phases 31–36 (shipped 2026-06-20). Native 64-bit client keeping BOTH renderers (D3D9 gl05/06/07 + D3D11 gl11) bootable to character select: 64-bit-correctness sweep, D3DX removed from the x64 build, x64 platform + all third-party deps, gl11 x64, Miles 9.3b/9.3v audio port, and the 32-bit address-space OOM eliminated. 12/13 requirements met; VERIFY-01 (cantina door-snap) PARKED at close as a pre-existing, bitness/renderer-independent floor-mesh/portal-seam engine quirk (out-of-scope for the port) + VERIFY-03 probe-strip deferred. **Post-close (2026-06-20) — both RESOLVED:** the door-snap was fixed (`3549c7104`, two stacked bugs — floor-seam graze suppression in `pathWalkCircleGetIds` + chase-cam pull-in rate-limit) with a back-room-camera follow-up (`810b6c9a9`, interior zoom cap), and ALL CORNERSNAP `_DEBUG` probes were stripped from source (VERIFY-03 satisfied). Full detail: `milestones/v3.0-ROADMAP.md`. Audit: `milestones/v3.0-MILESTONE-AUDIT.md`.

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

<details>
<summary>✅ v3.0 x64 Port (Phases 31–36) — SHIPPED 2026-06-20 (passed_with_carryforwards)</summary>

Native 64-bit client keeping BOTH renderers (D3D9 gl05/06/07 + D3D11 gl11) bootable to character select. Definition of done MET and user-verified in-world; 12/13 requirements met.

- [x] Phase 31: 64-bit Correctness Foundation (BITS-01/02/03). x87 asm → `_control87`, pointer/int truncation, AutoDelta* uint32_t wire-stable pin; tree compiles x64-clean. (9/9 plans, has 31-VERIFICATION.md; 2026-06-16)
- [x] Phase 32: D3DX → d3dcompiler_47 (SHADER-01). **Met on x64** — D3DX dropped from the x64 link, 0 `D3DXCompileShader/Assemble` in `src/engine/`, both renderers boot x64. (32-01 GATE done; 32-02..05 superseded by the x64-era path landed in P33; 2026-06-18)
- [x] Phase 33: x64 Build Platform + D3D9 Renderers (X64-01/04/02). x64 platform + ~57 lib configs + LIFT import-libs; D3DAssemble + DirectXMath ports; first full x64 link (0 unresolved); gl05/06/07 boot x64. (6/6 plans, 2026-06-18)
- [x] Phase 34: x64 D3D11 Renderer (X64-03). gl11 x64 (machine x64, 0 unresolved); x64 client boots rasterMajor=11 at the v2.2 parity bar. (2/2 plans, 2026-06-18)
- [x] Phase 35: Miles 9.3b Audio Port (AUDIO-01/02). Vendored 9.3b SDK, 7.2e→9.x port, per-platform redist; in-game audio works on x64 (9.3v later adopted for an x64 mixer bug). (4/4 plans, 2026-06-18)
- [~] Phase 36: Verification & CORNERSNAP Cleanup (VERIFY-01/02/03). **VERIFY-02 PASS** (no x64 OOM); **VERIFY-01 PARKED** — door-snap real in x64, root-caused as a pre-existing, bitness/renderer-independent floor-mesh/portal-seam engine quirk (footprint circle clips an uncrossable seam edge → PWR_HitEdge → collision rewind), out-of-scope for the port, carried forward; **VERIFY-03 / 36-02 DEFERRED** (probes kept as the acceptance harness). (1/2 plans, concluded 2026-06-20)

Full detail + success criteria: `milestones/v3.0-ROADMAP.md`. Audit: `milestones/v3.0-MILESTONE-AUDIT.md`.

</details>

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
| 32. D3DX → d3dcompiler_47 | v3.0 | 1/5 | Goal met on x64 (D3DX removed via P33 link; residual plans superseded) | 2026-06-18 |
| 33. x64 Build Platform + D3D9 Renderers | v3.0 | 6/6 | Complete    | 2026-06-18 |
| 34. x64 D3D11 Renderer | v3.0 | 2/2 | Complete   | 2026-06-18 |
| 35. Miles 9.3b Audio Port | v3.0 | 4/4 | Complete    | 2026-06-18 |
| 36. Verification & CORNERSNAP Cleanup | v3.0 | 1/2 | Concluded (VERIFY-02 pass; VERIFY-01 parked as pre-existing engine quirk; VERIFY-03/36-02 deferred) | 2026-06-20 |
| 37. Utinni Engine Entry-Point Advertisement | post-v3.0 | 3/3 | Complete   | 2026-06-21 |
| 38. Utinni Advertised-Client Coverage Completion | post-v3.0 | 0/? | Planning | — |

### Phase 37: Utinni Engine Entry-Point Advertisement (GetEngineHookPoints)

> **Standalone post-v3.0 phase** (not part of the shipped v3.0 x64 Port). Outside-project integration request from Utinni Phase 24; the exe-side game-logic twin of the already-shipped graphics `gl11_r.dll!GetHookPoints`. Spec: `.planning/handoff/2026-06-20-utinni-engine-entrypoint-advertisement-spec.md`. **32-bit only** (x64 is Utinni's deferred half).

**Goal:** `SwgClient` (all 32-bit flavors `_r`/`_d`/`_o`) exports one new undecorated `extern "C" __cdecl GetEngineHookPoints()` returning a name→pointer table of engine functions/globals, each row populated at **compile time** by `&EngineSymbol` (so addresses are correct-by-construction and survive every rebuild). The injected Utinni modding overlay resolves engine hook points **by name** instead of hardcoded SWGEmu RVAs — fixing its first-detour `0xC0000005 target=0x00401000` crash on our from-source client. The client stays fully **Utinni-agnostic**: a pure read-only getter, inert when Utinni is not injected. Mirror the shipped graphics pattern (`Direct3d11.cpp:856-888`). Acceptance per spec §7: export present + undecorated (`dumpbin`), first-detour crash gone, **zero-missing coverage** on the required name set, **no SWGEmu regression**.

**Requirements**: EPA-02 (advertised-path discovery / first-detour crash fixed), EPA-03 (DX11 overlay kickoff off the contract), EPA-04 (graceful degradation / coverage gate). Maps to the outside Utinni Phase-24 contract.
**Depends on:** Phase 36 (none functionally — the shipped `gl11_r.dll!GetHookPoints` graphics twin is its only related prior art).

**Plans (full scope — P0/P1/P2):**
- [x] 37-01 **P0 — Contract spike.** Lock the shared header (`utinni_engine_hookpoints.h` + `.inc` X-macro, `UTINNI_HOOKPOINTS_VERSION`), the PMF→`void*` extractor (union helper guarded by `static_assert(sizeof(PMF)==sizeof(void*))`), and the undecorated exe export (`extern "C" __cdecl __declspec(dllexport)` ALONE — no `.def`/`/EXPORT`, mirroring `Direct3d11.cpp:856-888`; `dumpbin`-verified). See `37-01-PLAN.md`. Prove end-to-end with ~5 representative rows: a static fn, a member fn via the helper, a global, and `config::loadOverrideConfig` (the row that fixes Utinni's first-detour crash).
- [x] 37-02 **P1 — MVP catalog (~70 endpoints).** config/client/graphics/game/scene/cui/command_parser, each mapped to a confirmed engine symbol and signature-checked against `D:/Code/Utinni/UtinniCore` typedefs; `.inc` shared with Utinni; coverage gate green for the MVP required set. Unblocks Utinni's Phase 18/19 live-smokes. See `37-02-PLAN.md`.
- [x] 37-03 **P2 — Full catalog (~160 more) + globals + open-items triage.** object/template/appearance/terrain/worldSnapshot/camera/collision/hud/ui-ctors/misc; read-only globals as `&g` (or accessor where file-static, per §8 #3); §8 #1 mid-function-patch endpoints get function-entry equivalents or are deferred as SWGEmu-only; virtual/vtable endpoints (e.g. `cui::io::processEvent`, virtual `Object::addToWorld`) skipped — Utinni resolves those off the live vtable. See `37-03-PLAN.md`.

### Phase 38: Utinni Advertised-Client Coverage Completion (Provider Thunks + Shims)

> **Standalone post-v3.0 phase** (32-bit only). The provider-side follow-on to Phase 37, driven by the Utinni Phase-24 post-live-smoke reconciliation. Source-of-truth handoffs: `.planning/handoff/2026-06-22-utinni-advertised-client-coverage-status.md` (coverage backlog), `.planning/handoff/2026-06-22-utinni-dx11-advertised-client-gap.md` (DX11 gap), `.planning/handoff/2026-06-20-utinni-engine-entrypoint-advertisement-spec.md` (contract). Decisions: `38-CONTEXT.md`.

**Goal:** Finish the provider half of the Utinni integration so the advertised `SwgClient` covers the full **`&fn`-addressable** engine entry-point set, retiring the subsystems that the MVP left gated off on the advertised client. Concretely: (1) free-function `__thiscall`-emulating thunks for multiple-inheritance classes whose PMFs are inflated past the `sizeof(void*)` guard — `groundScene::*` (NetworkScene MI; unblocks terrain editor + TJT scene-change) and `cui::chatWindow::*`; (2) external-linkage forwarder shims for file-local exe statics — `client::{wndProc,writeMiniDump,writeCrashLog,setupStartDataInstall}`, `config::{setModalChat,getModalChat}`; (3) fold in the parked WR-05 `consoleHelper::sendInput` thunk; (4) populate the remaining plain-`&fn`-addressable full-catalog rows where source-confirmable. Every new name is **source-confirmed** (a wrong `&fn` is worse than a missing row) and **bumps `UTINNI_HOOKPOINTS_VERSION`**; the `.inc` + `.h` re-sync byte-exact into `D:/Code/Utinni`. The client stays fully **Utinni-agnostic** (pure read-only getters; nothing calls into Utinni) and must **not regress** the standalone D3D11 client or the SWGEmu path. DX11 readiness is **evidence-only** (the gl11 provider is proven correct — `GetHookPoints` exports undecorated on `_r`/`_d`; `Direct3d11_Device::create()` sets device+context before swapChain and `destroy()` resets swapChain first, so the all-three-live invariant holds across the swapChain's whole non-null lifetime; the `0xC0000005 target=0x34` is consumer-side). Verify each deliverable with `dumpbin /exports` + a clean staged build (0 unresolved externals); the maintainer runs the live inject-smoke.

**Requirements**: EPA-05 (MI-class `__thiscall` thunks — groundScene + chatWindow coverage), EPA-06 (external-linkage shims — client/config file-local statics addressable), EPA-07 (remaining addressable full-set population + `UTINNI_HOOKPOINTS_VERSION` bump + coverage gate green for the grown required set), EPA-08 (DX11 provider-correctness evidence handback). Maps to the outside Utinni Phase-24 coverage-completion follow-on.
**Depends on:** Phase 37 (the shipped `GetEngineHookPoints` contract + shared `.inc`/`.h` it extends).
**Out of scope (consumer-side / deferred — note in handoff-back, do not implement here):** virtual-method resolution off the live vtable (`object::{addToWorld,removeFromWorld,setParentCell}`, `cui::io::processEvent`); Utinni's `directx11.cpp tryInstall()` poll-until-device+context guard + the three-pointer diagnostic log; dropping Utinni's per-subsystem `installable()`/group skips; the mid-function-patch features (Issue #11 chat routing, UI cascade, debug-cam suppress, `.trn`-name NOP) — each needs a joint cooperative-API decision with Utinni.
