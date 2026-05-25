# Roadmap: whitengold — SwgClient Modernisation Port

## Milestones

- ✅ **v1.0 Compile + Launch** — Phases 1–6 (CMake foundations → launch/login handshake). Authored in the original whitengold CMake repo; imported here 2026-05-25 for the record.
- ✅ **v2.0 Modernisation** — Phases 7–11 (shipped 2026-05-25). Dead-code removal, STLPort→MSVC STL (via Koogie MSBuild adoption), DPVS verdict, D3D11 renderer plugin. Full detail: `milestones/v2.0-ROADMAP.md`.
- 📋 **v2.1 Visual Parity** *(planned, not yet opened)* — close the D3D11 visual gaps catalogued in `docs/research/phase12-baseline/COMPARISON.md`. Phase 12 = asset PS pipeline (the blocker).

## Phases

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

- [x] Phase 7: Dead Code Removal — Track A (CLEAN-01..05). *Done on CMake tree; not re-applied to active MSBuild tree — backlog.*
- [◑] Phase 8: Dead Code Removal — Track B (CLEAN-06). Closed-as-scoped: ~12 tools wired, ~30 deferred.
- [x] Phase 9: STLPort → MSVC STL (STL-01..05). Option D — adopted Koogie MSBuild tree (`479d35df3`); Tatooine zone-in PASS.
- [x] Phase 10: DPVS Culling Experiment (DPVS-01/02). Verdict Option α; D3D11 remeasure deferred.
- [x] Phase 11: D3D11 Renderer Plugin (D3D11-01..05). gl11_d.dll; D3D9+D3D11 selectable; PASS-WITH-DEFERRALS.

Full detail + per-plan history: `milestones/v2.0-ROADMAP.md`. Audit: `milestones/v2.0-MILESTONE-AUDIT.md`.

</details>

### 📋 v2.1 Visual Parity (planned)

- [ ] Phase 12: Asset PS pipeline — re-author/bridge engine pixel shaders so D3D11 binds real asset shaders (D3D9 PEXE bytecode is rejected by `CreatePixelShader`). Then: gamma LUT, multi-stage sampling, load-screen half-texel seam, minimap, particles. Requirements to be derived from `docs/research/phase12-baseline/COMPARISON.md` via `/gsd-new-milestone`.

## Progress

| Phase | Milestone | Status | Completed |
|-------|-----------|--------|-----------|
| 1–6 Foundations → Login | v1.0 | Complete (historical) | — |
| 7. Dead Code A | v2.0 | Complete (CMake tree) | 2026-05 |
| 8. Dead Code B | v2.0 | Closed-as-scoped | 2026-05-08 |
| 9. STLPort → MSVC STL | v2.0 | Complete (Option D) | 2026-05-10 |
| 10. DPVS Experiment | v2.0 | Complete (Option α) | 2026-05 |
| 11. D3D11 Renderer | v2.0 | Complete (PASS-WITH-DEFERRALS) | 2026-05-24 |
| 12. Asset PS pipeline | v2.1 | Not started | — |
