# Milestones

## v2.0 Modernisation (Shipped: 2026-05-25)

**Scope:** Phases 7–11 (5 phases, 29 tracked plans). Audit: `tech_debt` — functional PASS, documented deferrals (see `milestones/v2.0-MILESTONE-AUDIT.md`).

**Delivered:** A clean, modern **MSVC / C++20 / MSBuild** SWG client that boots to character select and renders a ground scene under *both* a D3D9 and a new **D3D11** renderer, selectable at runtime via `rasterMajor`.

**Key accomplishments:**

1. **D3D11 renderer plugin (Phase 11)** — `gl11_d.dll` satisfies the full 119-function `Gl_api` table; engine boots under `rasterMajor=11` and renders the world. ~17 plans / ~45 iterations: shader-compile pipeline (vs_4_0/SV_POSITION), cbuffer model, state/input-layout caches, draw dispatch, and visual parity (Iter-38B matrix-transpose + Iter-39C per-pass blend). Closed PASS-WITH-DEFERRALS; D3D9 stays selectable (`rasterMajor=5`).
2. **STLPort → MSVC STL (Phase 9)** — closed via "Option D": adopted Koogie's already-MSVC/C++20-migrated MSBuild tree wholesale (merge `479d35df3`) + ported the SWGSource-VM IFF compat guards. Tatooine zone-in PASS; no `_STLP_*`/stlport conflict surface remains. **This pivot replaced the original CMake/whitengold build with the active MSBuild solution.**
3. **Dead-code removal (Phase 7)** — Vivox/XPCOM/trackIR/lcdui/VideoCapture removal designed + executed on the whitengold CMake tree (CLEAN-01..05). *Carried forward:* the Option-D base swap means these removals don't yet exist in the active MSBuild tree (dead code dormant-but-present; re-removal is backlog).
4. **DPVS culling experiment (Phase 10)** — profiled `resolveVisibility()` on modern HW; verdict Option α (remove outdoor occlusion culling, keep indoor portals), edit live in `RenderWorld.cpp`. D3D11 remeasure deferred until the asset PS pipeline lands.
5. **Tooling/build (Phase 8 + infra)** — ~12 editor/tool projects wired (MSBuild); ~30 deferred. Permanent D3D11 diagnostic infra (InfoQueue file sink, frame-stats, show-window helper).

**Known deferred at close (carried to next milestone / backlog):**
- Asset PS pipeline (D3D9 PEXE bytecode rejected by D3D11) — **the** visual-parity blocker → Visual Parity milestone, Phase 12.
- Gamma LUT, multi-stage sampling, particles/ribbon, square minimap, load-screen half-texel seam (`docs/research/phase12-baseline/COMPARISON.md`).
- Dead-code re-removal against the MSBuild tree (CLEAN-01..04).
- DPVS D3D11 remeasure (SPEC R7); CLEAN-06 ~30 tools.
- 4 low open artifacts acknowledged (2 closed debug sessions + 2 low todos) — see STATE.md Deferred Items.

**Archives:** `milestones/v2.0-ROADMAP.md`, `milestones/v2.0-REQUIREMENTS.md`, `milestones/v2.0-MILESTONE-AUDIT.md`. Tag: `v2.0`.

**Note on history:** Phases 1–6 (v1: CMake foundations → launch/login) and the phase 7–9 paper trail were authored in the original whitengold CMake repo and imported here 2026-05-25 for a complete record. The active build is MSBuild (`src/build/win32/swg.sln`); the CMake build system that v1 and the early-v2 docs describe is superseded.
