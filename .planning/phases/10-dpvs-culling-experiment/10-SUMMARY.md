---
phase: 10-dpvs-culling-experiment
status: complete
completed: 2026-05-15
plans: 7
plans_complete: 7
requirements: [DPVS-01, DPVS-02]
duration: "~6 days (2026-05-10 plan → 2026-05-15 close)"
tags: [dpvs, occlusion-culling, profiling, capture, verdict, source-edit, throwaway]
---

# Phase 10 — DPVS Culling Experiment

**Goal (from ROADMAP):** Measure the cost of `resolveVisibility()` (DPVS occlusion query) on a modern discrete GPU in at least one busy outdoor zone. If disabling occlusion culling produces no fps regression (or a positive gain), remove it permanently from outdoor scenes and document the result. Portal traversal for indoor cells is retained regardless of outcome.

**Outcome:** Verdict captured + applied. DPVS occlusion culling removed globally via Option α. Phase 11 D3D11 work reconsiders the architectural decision per ROADMAP success criterion #6.

---

## Verdict (one-liner)

**`remove` globally (Option α).** Source-of-truth: [docs/recon/10-dpvs-profiling.md](../../../docs/recon/10-dpvs-profiling.md).

Underlying data was scene-conditional (outdoor `remove` 3/3 scenes / indoor `keep` 1/1 scene) but the implementation site is a single `cullingParameters` bitmask applied to the single `ms_dpvsCamera` for ALL rendering — no code-level scene split is available without a larger refactor. Option α chosen: outdoor magnitude (0.94–2.13 ms / 3.3–9.1% wins) × dominant playtime frequency beats indoor regression (0.66 ms / 2.2% loss, below human perception threshold). Phase 11 D3D11 reconsideration captured in ROADMAP criterion #6.

---

## Numbers

22 CSVs, 12,016 frame samples, 4 client launch sessions over 2 calendar days:

| Scene | DPVS impact | Verdict |
|-------|-------------|---------|
| Mos Eisley plaza (sparse, ~8 NPCs) | OFF wins 0.94 ms (3.3%) | `remove` |
| Mos Eisley starport (dense) | OFF wins 2.13 ms (9.1%) | `remove` |
| Walking (moving camera) | OFF wins 0.99 ms (4.2%) | `remove` |
| Cantina V1 (indoor sparse) | ON wins 0.62 ms (2.2%) | `keep` |
| Cantina V2 (indoor dense, 30+ moving NPCs) | ON wins 0.64 ms (2.1%) | `keep` |
| Cantina combined (n=3,724 indoor) | ON wins 0.66 ms (2.2%) | `keep` |

Full tables, capture-quality notes, unified-mechanism rationale, Phase 11 reconsideration plan: see [docs/recon/10-dpvs-profiling.md](../../../docs/recon/10-dpvs-profiling.md).

---

## What landed (permanent source edits per D-13 + D-14)

- `RenderWorld.cpp:909/913` — `OCCLUSION_CULLING` bit stripped from both `#ifdef _DEBUG` and release branches of `cullingParameters`. `VIEWFRUSTUM_CULLING` retained.
- `RenderWorld.cpp/.h` — `ms_disableOcclusionCulling` static + `setDisableOcclusionCulling`/`getDisableOcclusionCulling` setter/getter deleted.
- `ConfigClientGraphics.cpp/.h` — `ms_disableOcclusionCulling` static + `KEY_BOOL(disableOcclusionCulling)` config-key registration + `getDisableOcclusionCulling()` getter deleted.
- `GroundScene.cpp:691` — pre-Phase-10 scene-conditional `setDisableOcclusionCulling(strstr(terrainFilename, "space_") != 0)` caller removed (redundant under Option α; documented in comment).

---

## What got removed (THROWAWAY cleanup per D-15)

The entire Phase 10 measurement harness, in one revert-shaped commit (151167d2c, -726 lines):
- `DpvsProfileInstrumentation.{h,cpp}` — engine module deleted entirely
- 3 `Gl_api` function pointers (`dpvsGpuTimingBegin/End/PollResult`) + their Direct3d9 plugin implementation (timestamp query pool, lazy create, slot rotation, disjoint guard, release on shutdown)
- 3 engine-side `Graphics::dpvsGpuTiming*` forwarders
- Wave 3 hook sites: RenderWorld GPU/CPU bracket around `resolveVisibility()`, Game.cpp `onFrameEnd` hook after `Graphics::present()`, CuiIoWin `#ifdef _DEBUG` F10/F11 keybind intercept, SwgCuiCommandParserDefault `/setrunlabel` console command
- clientGraphics.vcxproj `ClCompile` + `ClInclude` entries for DpvsProfileInstrumentation

**Phase 11 recovery anchor:** tag `phase-10-instrumentation-pre-cleanup` at commit `9f2ec3715` preserves the pre-cleanup HEAD. `git revert <D-15-commit>` restores everything if Phase 11 DPVS remeasurement wants to reuse the harness. (Alternative: Phase 11 D3D11 reimplements cleanly under `ID3D11Query` — the query pool was Direct3d9-specific anyway.)

---

## Cross-cutting findings (orthogonal — not Phase 10 work, but discovered during it)

Two new todos in `.planning/todos/pending/` for `/gsd-debug` sessions:

1. **`2026-05-14-safecast-null-dynamic-cast-world-load.md`** — Koogie's stricter `assert((t) != nullptr)` in `SafeCast.h:29` fires two CRT dialogs on world load that Ignore-twice gets past. Real null cast underneath, likely tied to a SWGSource compat-guard gap (ContrailData / NebulaManagerClient / POB candidates from D-18 unported list). Tracked for `/gsd-debug` with reproduction protocol + suspect candidates.

2. **`2026-05-15-cantina-corner-snap-regression.md`** — Camera/character snap when transitioning between cantina sub-cells. Bisected to a ~5-hour window on 2026-05-08 (between v1 milestone Debug build and `build-v145`). Phase 10 boot smoke surfaced the smoking-gun mechanism: `DEBUG_WARNING` routes to `OutputDebugString` (Report.cpp:145), which has ~50-200 µs per-call overhead from Windows' global event-check mutex even without a debugger attached. Concentrated warning bursts on first-time cell load = visible snap. **One-line fix candidate** captured: `if (IsDebuggerPresent()) OutputDebugString(buffer);`.

---

## Cross-references

- **Long-form verdict record:** [docs/recon/10-dpvs-profiling.md](../../../docs/recon/10-dpvs-profiling.md) (canonical published home; per D-16)
- **Plan-level closeouts:** 10-01-SUMMARY.md through 10-07-SUMMARY.md in this directory
- **Capture evidence (per-scene tables, capture-quality notes):** 10-05-capture-evidence.md
- **Verdict gate file:** 10-05-analysis.txt (`verdict = remove` line consumed by plan 10-06 Task 3 conditional gate)
- **Phase 11 reconsideration:** ROADMAP §Phase 11 success criterion #6 (DPVS remeasurement under D3D11; three architectural options to evaluate then)
- **Phase 11 recovery anchor:** git tag `phase-10-instrumentation-pre-cleanup` at commit `9f2ec3715`
- **CONTEXT decisions:** 10-CONTEXT.md (D-01 through D-19)
- **Research record:** 10-RESEARCH.md
- **Tooling:** `tools/dpvs-profile/test-protocol.md` (capture procedure) + `tools/dpvs-profile/analysis.py` (verdict-line emitter)

---

## Closeout

**Phase 10 complete: 7 of 7 plans (100%).** DPVS-01 (documentation) and DPVS-02 (conditional source removal) both satisfied.

The phase delivered its goal: measured DPVS occlusion-query cost on modern hardware against a real server, captured a clean verdict, applied the verdict-driven source edits, removed the measurement scaffolding, and documented the result in the docs/recon/ tree for the long-term record. Two orthogonal investigations were spun out as `/gsd-debug` candidates with concrete reproduction protocols and lead theories.

Closing this phase clears the active milestone work to:
- `/gsd-debug` SafeCast.h:29 null cast dialogs
- `/gsd-debug` cantina corner-snap (with the OutputDebugString smoking-gun lead)
- Then Phase 11 D3D11 Renderer Plugin
