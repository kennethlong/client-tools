# Milestones

## v2.2 Visual Parity (Shipped: 2026-06-12)

**Scope:** Phases 17–23 (13/13 requirements satisfied). Phases 17, 18, 23 ran through GSD (15 plans, full artifacts); the Phase 19–22 work was executed **ad-hoc** on `koogie-msvc-cpp20-base` 2026-06-08..12 — `milestones/v2.2-MILESTONE-AUDIT.md` is their de-facto verification record. Audit: `tech_debt` — functional PASS, 13/13 requirements, 13/13 integration paths WIRED, 0 blockers. Timeline: 16 days (2026-05-27 → 2026-06-12), 119 commits, ~115 files, +13.5k lines.

**Delivered:** The D3D11 renderer (`rasterMajor=11`) now visually matches the known-good D3D9 baseline (`rasterMajor=5`) — characters, interiors, open world, terrain, gamma, UI, and effects — closing every gap bucket catalogued in `docs/research/phase12-baseline/COMPARISON.md`, with the D3D9 reference path unregressed.

**Key accomplishments:**

1. **Asset pixel-shader pipeline (Phase 17, CHAR-01/02/03)** — recompiled the discarded `TAG_PSRC` source instead of the D3D11-rejected PEXE bytecode: `//hlsl` recompile lane + 17-07 PS-input-signature reconstruction (asset-PS bind rate 0/9 → 9/9) + reflection-driven constants (D3DReflect, not copied D3D9 registers) + b0 lighting feed + stage→SRV-slot remap. Char-select renders lit + textured, near-identical to D3D9.
2. **Load-screen half-texel seam (Phase 18, UI-01)** — single central `setViewport` correction (resolution-independent, no per-draw fudge); verified across 3 splash images; D3D9 untouched.
3. **Interior + world parity (ad-hoc 19/20, WORLD-01/02/03)** — cantina at D3D9 parity via the FFP combiner-cascade PS (full texture-stage emulation), per-pixel fog in all three PS lanes, sticky-dot3 + alpha-fade register-leak fixes, ambient/directional lighting parity (8x-crush + unfed-parallels bugs), hemispheric terrain sun, and the blend-factors re-land that fixed dark terrain patches. Verified by Kenny in-client + RenderDoc pixel traces.
4. **Gamma, minimap, effects (ad-hoc 19/20/21, GAMMA-01/UI-02/FX-01/FX-02)** — D3D9's exact gamma ramp as a pre-Present curve pass (identity = off); round minimap via `ui_radar.psh` PSRC override; particles fixed by the sticky-dot3 + additive-blend-factor work; ribbons/swooshes verified undistorted in dual-renderer A/B. Exterior geometry closed as no-real-distortion on settled captures (GEO-01).
5. **DPVS D3D11 remeasure (Phase 23, DPVS-01)** — Phase 10 instrumentation restored CPU-only + occlusion A/B re-gate; 12-CSV live capture. Both verdicts **FLIPPED** vs D3D9: outdoor `remove→keep`, indoor `keep→remove` — Option α premise REVISED (`docs/recon/23-dpvs-d3d11-profiling.md`); shipped branch untouched, acting on the verdict is a deferred follow-up.
6. **Bonus (no REQ assigned)** — full audio restoration + warning-flood perf-drag elimination (missing `stage/miles/` codec redist; 141k→1.8k log lines); combat kill-crash fix (`d549a8acd`, erase-then-++ iterator UB woken by the MSVC-STL migration); shader-override workflow now tracked in git (`stage/override/`).

**Known deferred at close: 7 open artifacts acknowledged (see STATE.md Deferred Items)** — 2 closed debug sessions; 4 pre-existing/low todos (incl. the medium pre-existing Options-window FATAL); the deliberate config-gate-DPVS-occlusion follow-up. Plus the audit `tech_debt` list: D-15 instrumentation removal, machine-specific `stage/override` + `stage/miles` paths, blend-factors-everywhere scene-sweep risk, missing Nyquist VALIDATIONs (18, 19–22), `client_d.cfg` test-settings cleanup.

---

## v2.1 Decruft (Shipped: 2026-05-27)

**Scope:** Phases 12–16 (5 phases — incl. inserted Phase 16 tech-debt closure; 16 plans, 31 tasks). Audit: `tech_debt` — functional PASS, 7/7 DECRUFT requirements satisfied, 28/28 cross-phase integration (0 blocker), dual-renderer boot human-confirmed (see `milestones/v2.1-MILESTONE-AUDIT.md`).

**Delivered:** Re-applied the orphaned v2.0 dead-code removals (CLEAN-01..04) against the active Koogie/MSBuild tree — five dormant subsystems fully unlinked and deleted with the client kept bootable to character-select under both renderers after every step.

**Key accomplishments:**

1. **Orphaned directory & project deletes (Phase 12, DECRUFT-01/02/03)** — deleted `stationapi/` + `trackIR/` (ClientHeadTracking de-wired from NPClient.h to no-op stubs), dropped `SwgClientSetup.vcxproj` (+53-file dir) and the Logitech G15 `lcdui` subsystem from `swg.sln` (own blocks + all 7 deps) and every `.rsp`/inline-vcxproj dep across SwgClient + 6 editors. Dual-renderer boot baseline re-established.
2. **VideoCapture unlink (Phase 13, DECRUFT-04)** — atomically removed the live `CuiIoWin` caller + `SwgVideoCapture` wrapper + 15 lib tokens/8 paths, purged all dead `#if 0` source + `.rsp` + 10 editor `.vcxproj` refs, and deleted the vendored `videocapture/` tree. Debug+Release link clean (2138/0/0, 2104/0/0); Bink/Vivox/Miles preserved.
3. **Vivox voice-chat removal (Phase 14, DECRUFT-05)** — deleted ~24 source files + 3 voicechat network messages, de-wired 10 live callers + 5 registrations, stripped 6 `CuiPreferences` keys, unlinked vivox/VChatAPI/Base_vchat/libsndfile, and deleted the 3 vendored voice trees (138 files, 47,201 lines). Code review caught + fixed CR-01 (voice-enum deletion shifted radial-menu ordinals off their retail-datatable rows → ordinal-preserving placeholders).
4. **XPCOM/Mozilla in-game browser removal + milestone gate (Phase 15, DECRUFT-06/07)** — deleted the 3 `SwgCuiWebBrowser*` units + `TUIWebBrowser` enum, severed the TCG browser tie (libEverQuestTCG kept), dropped `libMozilla.vcxproj` from `swg.sln` (11 GUID locations), and deleted the 1,866-file vendored `libMozilla/` (XPCOM/Gecko SDK) tree. Full P12–P15 residue sweep == 0 / KEEP-listed; full-removal-set dual-renderer boot to character-select human-confirmed.
5. **v2.1 tech-debt closure (Phase 16)** — post-audit cleanup (no new product reqs): unlinked the dead SwgGodClient `989crypt.lib` token (KEEP-list + live `crypto.lib` preserved), removed the dead `finalUrl` browser-CS block (+2 dead includes) and the orphaned `CuiPreferences` voice-volume API. SwgClient Debug+Release relink clean; D3D11 boot re-confirmed. Discovered + ruled out a pre-existing Options-window crash (outside the v2.1 diff).

**Known deferred at close (5 open artifacts acknowledged — see STATE.md Deferred Items):** 2 closed debug sessions (cantina-corner-snap, safecast-null-cast); cantina corner-snap engine-improvement todo (low, workaround); SWG-Source vs whitengold TRE asset-diff note (low, informational); pre-existing Options-window crash todo (medium — `checkShowToolbarCommandCooldownTimer` `.ui`-data mismatch from feature commit `d1b3c0eaf`, NOT a Decruft regression). Plus v2.2-coupled deferrals: `stage/client_d.cfg` test-settings cleanup; AR-15-01 future-TCG-revival re-evaluation.

**Archives:** `milestones/v2.1-ROADMAP.md`, `milestones/v2.1-REQUIREMENTS.md`, `milestones/v2.1-MILESTONE-AUDIT.md`. Tag: `v2.1`.

---

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
