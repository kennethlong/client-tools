# Roadmap: whitengold — SwgClient Modernisation Port

## Milestones

- ✅ **v1.0 Compile + Launch** — Phases 1–6 (CMake foundations → launch/login handshake). Authored in the original whitengold CMake repo; imported here 2026-05-25 for the record.
- ✅ **v2.0 Modernisation** — Phases 7–11 (shipped 2026-05-25). Dead-code removal, STLPort→MSVC STL (via Koogie MSBuild adoption), DPVS verdict, D3D11 renderer plugin. Full detail: `milestones/v2.0-ROADMAP.md`.
- ✅ **v2.1 Decruft** — Phases 12–16 (shipped 2026-05-27). Re-applied the orphaned CLEAN-01..04 dead-code removals against the active Koogie/MSBuild tree; five dormant subsystems unlinked + deleted, client kept bootable under both renderers. Full detail: `milestones/v2.1-ROADMAP.md`. Audit: `milestones/v2.1-MILESTONE-AUDIT.md`.
- ✅ **v2.2 Visual Parity** — Phases 17–23 (shipped 2026-06-12). D3D11 (`rasterMajor=11`) now matches the known-good D3D9 (`rasterMajor=5`) baseline: asset PS pipeline (PSRC recompile + reflection constants), FFP combiner cascade, lighting/gamma parity, round minimap, particles/ribbons, exterior geometry closure, DPVS remeasure (Option α REVISED). 13/13 requirements. Full detail: `milestones/v2.2-ROADMAP.md`. Audit: `milestones/v2.2-MILESTONE-AUDIT.md`.
- 📋 **Next milestone** — not yet defined; run `/gsd-new-milestone`. Candidate seeds: act on the DPVS verdict (config-gate occlusion), D-15 instrumentation removal, machine-portability (`stage/override` + `stage/miles` paths), SWG-Source community-compat sync, pre-existing Options-window FATAL.

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

### 📋 Next milestone — not yet defined

Run `/gsd-new-milestone` to define requirements + roadmap. Phase numbering continues from 24.

## Progress

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
