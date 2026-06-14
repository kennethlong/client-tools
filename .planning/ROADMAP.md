# Roadmap: whitengold — SwgClient Modernisation Port

## Milestones

- ✅ **v1.0 Compile + Launch** — Phases 1–6 (CMake foundations → launch/login handshake). Authored in the original whitengold CMake repo; imported here 2026-05-25 for the record.
- ✅ **v2.0 Modernisation** — Phases 7–11 (shipped 2026-05-25). Dead-code removal, STLPort→MSVC STL (via Koogie MSBuild adoption), DPVS verdict, D3D11 renderer plugin. Full detail: `milestones/v2.0-ROADMAP.md`.
- ✅ **v2.1 Decruft** — Phases 12–16 (shipped 2026-05-27). Re-applied the orphaned CLEAN-01..04 dead-code removals against the active Koogie/MSBuild tree; five dormant subsystems unlinked + deleted, client kept bootable under both renderers. Full detail: `milestones/v2.1-ROADMAP.md`. Audit: `milestones/v2.1-MILESTONE-AUDIT.md`.
- ✅ **v2.2 Visual Parity** — Phases 17–23 (shipped 2026-06-12). D3D11 (`rasterMajor=11`) now matches the known-good D3D9 (`rasterMajor=5`) baseline: asset PS pipeline (PSRC recompile + reflection constants), FFP combiner cascade, lighting/gamma parity, round minimap, particles/ribbons, exterior geometry closure, DPVS remeasure (Option α REVISED). 13/13 requirements. Full detail: `milestones/v2.2-ROADMAP.md`. Audit: `milestones/v2.2-MILESTONE-AUDIT.md`.
- 🚧 **v2.3 Hardening** — Phases 24–30 (in progress, started 2026-06-12). Consolidate the v2.2 parity win: act on the DPVS verdict (config-gate occlusion), machine-portability (`stage/override` + `stage/miles` paths, `client_d.cfg` cleanup), cantina corner-snap fix, D-15 + CORNERSNAP instrumentation removal, Options-window FATAL fix, `D3DXCompileShader`→`D3DCompile` port — PLUS a new standalone web-based TRE compare tool (two-install archive-set + merged-virtual-tree diff). 12 requirements (HARD-01..05, PORT-01/02, TRE-01..05).

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

### 🚧 v2.3 Hardening (Phases 24–30) — In Progress

**Milestone Goal:** Consolidate the v2.2 parity win. Two independent streams: (A) in-client C++ hardening — operationalize the DPVS verdict, make the staged client machine-portable, fix the known runtime crashes/quirks, strip accumulated debug instrumentation, port the D3D9 shader-compile API; (B) a new standalone web-based TRE compare tool for cross-installation data diagnostics. The two streams share zero code and can interleave.

**Core invariant (every client-touching phase, 24–27):** the client stays bootable to character select under BOTH `rasterMajor=5` (D3D9) and `rasterMajor=11` (D3D11) after the phase. The TRE tool (Phases 28–30) is a standalone web app, outside that invariant but inside the milestone.

- [x] **Phase 24: DPVS Config-Gate + Machine Portability** - Occlusion auto-gated on POB-cell membership; de-hardcoded stage paths + cleaned `client_d.cfg`; dual-renderer boot verified
 (completed 2026-06-13)
- [ ] **Phase 25: Cantina Corner-Snap Fix** - Re-entrancy guard stops the same-frame portal ping-pong without breaking fast door traversals (verified via committed CORNERSNAP instrumentation)
- [x] **Phase 26: Instrumentation Removal + Options-Window FATAL** - D-15 DPVS instrumentation stripped atomically (CORNERSNAP probes KEPT as the door-snap harness — deferred to x64/HARD-05); Options window no longer FATALs (completed 2026-06-14)
- [ ] **Phase 27: D3DCompile Port** - `D3DXCompileShader` replaced with `D3DCompile` (Fix B), asm-shader census first, D3D9 visual parity held
- [ ] **Phase 28: TRE Compare Tool — Foundation (Parser + Scanner + Virtual Tree)** - Headless, fully unit-tested backend: vendored parser + cfg search-path scanner + engine-faithful merged-virtual-tree builder
- [ ] **Phase 29: TRE Compare Tool — Diff Engine + API** - Set-level + file-level diff (length/compressedLength signal, on-demand hashing) + FastAPI routes + sqlite index cache
- [ ] **Phase 30: TRE Compare Tool — Frontend SPA** - React/Vite/shadcn virtualized tree-diff UI: install picker, set-delta table, badges, filter, search, per-file detail

## Phase Details

### Phase 24: DPVS Config-Gate + Machine Portability
**Goal**: Operationalize the Phase 23 DPVS verdict behind config and make a fresh clone+build produce a bootable, machine-independent client.
**Depends on**: Phase 23 (DPVS verdict doc `docs/recon/23-dpvs-d3d11-profiling.md` is the spec)
**Requirements**: HARD-01, PORT-01, PORT-02
**Success Criteria** (what must be TRUE):
  1. With auto mode set, the DPVS occlusion bit is enabled outdoors and disabled inside POB cells (matching the Phase 23 verdict), and an explicit config override can force either mode regardless of cell membership
  2. A fresh clone + build boots to character select with no machine-specific absolute paths — `stage/override` and `stage/miles` handling is documented and/or automated (miles redist absence is detected/handled, not a silent half-dead-audio failure)
  3. `stage/client_d.cfg` contains no leftover Phase-11+ test settings, and the client boots clean to character select under both `rasterMajor=5` and `rasterMajor=11`
**Plans**: 3 plans (2 waves)
  - [x] 24-01-PLAN.md — DPVS occlusionMode knob (HARD-01) + D-07 engine-default flips (multi-stream VB, D3D11 Bloom no-op)
  - [x] 24-02-PLAN.md — Miles redist reconcile + postbuild repoint + D-12 one-shot absence warning (PORT-01)
  - [x] 24-03-PLAN.md — cfg template + setup-client.ps1 generator + PORT-02 key cleanup + fresh-clone & dual-renderer gate

### Phase 25: Cantina Corner-Snap Fix
**Goal**: Eliminate the cantina corner-snap by stopping the same-frame portal ping-pong, while preserving legitimate fast door traversals.
**Depends on**: Phase 24 (clean dual-renderer boot baseline; CORNERSNAP instrumentation already committed `a9b419daf`)
**Requirements**: HARD-02
**Success Criteria** (what must be TRUE):
  1. Walking into a cantina corner no longer snaps/teleports the player — the A→B→A same-frame cell oscillation is suppressed by a re-entrancy guard
  2. Fast legitimate door traversals still complete correctly (no regression — the guard targets the reversal pattern specifically, not a blanket one-transition-per-frame cap)
  3. The committed CORNERSNAP `_DEBUG` instrumentation reports zero ping-pong frames in the previously-failing cantina corner under both renderers
**Plans**: 1 plan (2 waves)
  - [ ] 25-01-PLAN.md — frame-scoped A->B->A reversal guard in CellProperty::positionChanged (HARD-02) + dual-renderer CORNERSNAP capture verify

### Phase 26: Instrumentation Removal + Options-Window FATAL
**Goal**: Strip the now-superseded debug instrumentation from shipped code paths and fix the pre-existing Options-window crash.
**Depends on**: Phase 25 (CORNERSNAP probes are the acceptance harness for HARD-02 and must survive until it is verified); Phase 24 (D-15 DPVS profiling purpose superseded by the shipped verdict)
**Requirements**: HARD-03 (PARTIAL — D-15 only), HARD-04
**Scope note (2026-06-13):** HARD-03 is split. Phase 26 removes ONLY the D-15 DPVS instrumentation. The CORNERSNAP `_DEBUG` probes are KEPT — Phase 25's door-snap fix was reverted and the snap is parked for x64, so the probes remain the acceptance harness and their removal is deferred to the x64/HARD-05 work.
**Success Criteria** (what must be TRUE):
  1. The D-15 DPVS instrumentation is gone from shipped code paths — callers, config-flag registrations, and build-graph entries removed atomically, with Debug+Release link clean (zero unresolved externals, link output grepped — not just MSBuild exit 0). The CORNERSNAP probes are intentionally LEFT IN PLACE (CollisionResolve.cpp + CellProperty.cpp) and must remain compilable.
  2. Opening the Options window no longer FATALs — the `checkShowToolbarCommandCooldownTimer` CodeData/.ui mismatch (from feature commit `d1b3c0eaf`) is fixed
  3. The client boots to character select and the Options window opens cleanly under both `rasterMajor=5` and `rasterMajor=11`
**Plans**: 2 plans (1 wave)
  - [x] 26-01-PLAN.md — atomically remove D-15 DpvsProfileInstrumentation (CORNERSNAP probes KEPT) + Debug/Release /FORCE link-grep + dual-renderer boot (HARD-03 D-15)
  - [x] 26-02-PLAN.md — verify/audit Options-window FATAL fix (checkShowToolbarCommandCooldownTimer, commit d1b3c0eaf) + dual-renderer Options-open smoke (HARD-04)

### Phase 27: D3DCompile Port
**Goal**: Replace `D3DXCompileShader` with `D3DCompile` (Fix B) in the D3D9 plugin, superseding the Phase-19 SEH guard where the path is ported.
**Depends on**: Phase 26 (clean tree); independent of Phases 25/26 in code surface but scheduled last in the hardening stream as the most complex item. Requires an asm-shader census as its first task.
**Requirements**: HARD-05
**Success Criteria** (what must be TRUE):
  1. An asm-shader census (count of `.vsh` / `D3DXAssembleShader` call sites) is produced first and scopes the assembly-path handling — the port does not silently drop the asm path (which would null the VS and skip draws)
  2. D3D9 HLSL shader compilation runs through `D3DCompile` (with a reimplemented `ID3DInclude` handler and `d3dcompiler_47.dll` staged) instead of `D3DXCompileShader`
  3. D3D9 visual parity is held against an A/B baseline (no shader-compile regression), and the Phase-19 SEH guard is retained for any path still on D3DX and removed only where the port supersedes it
**Plans**: TBD

### Phase 28: TRE Compare Tool — Foundation (Parser + Scanner + Virtual Tree)
**Goal**: Stand up the headless, fully unit-tested backend foundation for the TRE compare tool — parser reuse, cfg search-path scanning, and engine-faithful virtual-tree merging.
**Depends on**: Nothing (independent stream; can run concurrently with Phases 24–27)
**Requirements**: TRE-01
**Success Criteria** (what must be TRUE):
  1. The tool scaffold lives at `tools/tre-compare/` with the parser vendored from `D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py` (+ `tre_decrypt.py`), reading all TREE format variants plus COT2000/SearchTOC without rewriting
  2. A scanner hand-parses `client.cfg` `[SharedFile]` (repeated keys handled — not stdlib configparser) and yields a priority-ordered node list matching the engine's `searchTree`/`searchTOC`/`searchPath` precedence
  3. The virtual-tree builder merges archives with first-hit-wins precedence (higher priority number searched first), length-0 tombstone handling, and `fixUpFileName` canonicalization — with unit tests against the real `stage/client.cfg` proving correct override shadowing and tombstone behavior
**Plans**: TBD

### Phase 29: TRE Compare Tool — Diff Engine + API
**Goal**: Build the set-level + file-level diff engine and the FastAPI surface over the proven virtual tree, so the UI works against real data from day one.
**Depends on**: Phase 28
**Requirements**: TRE-02, TRE-03, TRE-04
**Success Criteria** (what must be TRUE):
  1. A user can request a set-level compare between two installations and get which TRE archives are added/removed/changed, with size/version deltas
  2. A user can request a file-level compare of the merged virtual trees with per-file added/removed/changed status, where "changed" is detected via `(length, compressedLength)` — NOT the TOC `crc` field (a path-CRC, not a content hash)
  3. A user can drill into any file via the API and get its metadata, winning archive, shadowed copies in other archives, and an on-demand real content hash (xxhash) computed only on drill-in — never eager full-archive hashing
  4. A sqlite index cache keyed by `(abspath, mtime, size)` makes re-runs instant, and the API returns correct diff JSON for the SWGSource-vs-whitengold install pair
**Plans**: TBD

### Phase 30: TRE Compare Tool — Frontend SPA
**Goal**: Ship the modern web UI over the proven running API — a virtualized, filterable install-vs-install tree-diff that solves the space-asset diagnosis use case end-to-end.
**Depends on**: Phase 29
**Requirements**: TRE-05
**Success Criteria** (what must be TRUE):
  1. A user can pick two installations, see a set-delta table, and browse a virtualized merged file-tree (TanStack Virtual — no hang on real 10k–100k-entry data) with added/removed/changed badges and folder roll-up
  2. A user can filter by status (hide-identical), search by name/path, and read summary stats
  3. A user can select any file and see a detail panel with the winning archive, shadowed copies, sizes, and CRC display
  4. The tool runs end-to-end as a single localhost server and answers the SWGSource-vs-whitengold space-asset diff
**Plans**: TBD
**UI hint**: yes

## Progress

**Execution Order:**
Client-hardening stream (24 → 25 → 26 → 27) and TRE-tool stream (28 → 29 → 30) are independent and may interleave. Within each stream, phases execute in numeric order.

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
| 24. DPVS Config-Gate + Machine Portability | v2.3 | 3/3 | Complete    | 2026-06-13 |
| 25. Cantina Corner-Snap Fix | v2.3 | 0/1 | Planned | - |
| 26. Instrumentation Removal + Options FATAL | v2.3 | 2/2 | Complete   | 2026-06-14 |
| 27. D3DCompile Port | v2.3 | 0/TBD | Not started | - |
| 28. TRE Tool — Foundation | v2.3 | 0/TBD | Not started | - |
| 29. TRE Tool — Diff Engine + API | v2.3 | 0/TBD | Not started | - |
| 30. TRE Tool — Frontend SPA | v2.3 | 0/TBD | Not started | - |
