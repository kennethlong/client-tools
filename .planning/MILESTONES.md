# Milestones

## v3.0 x64 Port (Shipped: 2026-06-20)

**Scope:** Phases 31–36 (6 phases, 23 plans). Audit: `passed_with_carryforwards` — 12/13 requirements met; VERIFY-01 (door-snap) parked as a pre-existing engine quirk + VERIFY-03 deferred (its dependent cleanup); definition of done met and user-verified in-world (`milestones/v3.0-MILESTONE-AUDIT.md`). Timeline: 2026-06-15 → 2026-06-20. (`master` is a live upstream-integration branch — milestone-scoped phase/plan counts are authoritative, not the upstream-inflated diffstat.)

**Delivered:** The first native **64-bit** SWG client that keeps **both** renderer families (D3D9 gl05/06/07 + D3D11 gl11) bootable to character select and playable in-world — eliminating the chronic 32-bit address-space OOM, removing the x64-hostile legacy D3DX dependency, and porting audio to x64 Miles. The 32-bit build remains unregressed under both renderers.

**Key accomplishments:**

1. **64-bit correctness foundation (Phase 31, BITS-01/02/03)** — x87 FPU-control inline asm → `_control87`/`_controlfp`, a tree-wide `__asm` sweep to x64-clean, pointer/integer truncation fixed (C4311/4312/4244 as errors), and an IFF/TRE + network-message data-layout audit (the ~753-error AutoDeltaMap/PackedMap/Set cascade resolved via a uint32_t wire-stable count pin). 9 plans (6 base + 3 gap-closure increments as the scratch-x64 sweep unmasked successive class-(B) layers); has a formal `31-VERIFICATION.md`.
2. **D3DX removed from the x64 build (Phases 32–33, SHADER-01)** — the legacy D3DX shader path is off D3DX on x64: `D3DXAssembleShader`→`D3DAssemble` (GetProcAddress) + `D3DXMATRIX`→DirectXMath, D3DX dropped from the x64 link block, 0 `D3DXCompileShader/D3DXAssembleShader` calls left in `src/engine/`. (32-bit retains the Fix-A SEH guard.)
3. **x64 build platform + D3D9 renderers (Phase 33, X64-01/02/04)** — the `x64` platform added to `swg.sln` + every `.vcxproj`, ~57 boot-path engine/3rd-party StaticLibrary x64 configs, all third-party deps resolved x64 (LIFT import-libs for libxml2/pcre/jpeg; binkw64 swap), and the first fully-linking x64 client (0 unresolved external symbol) booting to character select under rasterMajor 5/6/7.
4. **x64 D3D11 renderer (Phase 34, X64-03)** — gl11 rebuilt x64 (`dumpbin` machine x64, 0 unresolved); the x64 client boots rasterMajor=11 to dressed char-select at the v2.2 visual-parity bar.
5. **Miles 9.3b/9.3v audio port (Phase 35, AUDIO-01/02)** — `clientAudio` ported from the 7.2e to the 9.x API, x64 SDK vendored, per-platform redist/provider set staged; in-game audio (music, 2D UI, 3D positional, reverb) works on x64 (9.3v adopted for x64 to dodge a 9.3b x64 mixer bug), and login/char-select streamed music fixed engine-side.
6. **Verification (Phase 36, VERIFY-02 + door-snap root cause)** — the 32-bit address-space OOM (`MemoryManager` `b0780503` class) confirmed eliminated on x64 (decelerating memory floor, flat handles). The cantina door-snap was found to PERSIST in x64 (falsifying the long-held "x64 fixes it" premise) and was root-caused via a live non-debugger DBWIN capture + a 4-AI crew as a **pre-existing, bitness/renderer-independent floor-mesh/portal-seam collision quirk** (footprint circle clips an uncrossable seam edge → `PWR_HitEdge` → rewind) — orthogonal to the port, parked and fully documented (`research/CONSULT-36-doorsnap-SYNTHESIS.md`; reusable `tools/setup/dbwin-cornersnap-capture.ps1`).

**Known deferred at close (carry-forwards) — BOTH RESOLVED post-close (2026-06-20):** **VERIFY-01** cantina door-snap fix and **VERIFY-03 / plan 36-02** CORNERSNAP `_DEBUG` probe strip were deferred at the milestone audit, then completed the same day via a spike chain. Door-snap fixed in `3549c7104` (two stacked bugs — avatar floor-seam graze suppressed at detection in `pathWalkCircleGetIds` via `cs_seamGrazeEpsilon`; chase-cam instant pull-in rate-limited via `cs_cameraPullInSpeed`) + back-room-camera follow-up `810b6c9a9` (interior zoom cap `freeChaseCameraInteriorMaximumZoom`); ALL CORNERSNAP probes removed from source (VERIFY-03 satisfied). Spike chain `.planning/spikes/001-006/`; handoff `.planning/handoff/2026-06-20-cantina-door-snap-FIX-COMPLETE.md`. (3 prior attempts had been reverted; the spike-first approach succeeded.) **Open artifacts acknowledged at close: 7** (2 debug sessions — cantina-corner-snap parked, safecast-null-cast closed; 5 low/deferred todos incl. the now-delivered x64-port todo) — see STATE.md Deferred Items.

**Archives:** `milestones/v3.0-ROADMAP.md`, `milestones/v3.0-REQUIREMENTS.md`, `milestones/v3.0-MILESTONE-AUDIT.md`. Tag: `v3.0`.

---

## v2.3 Hardening (Shipped: 2026-06-15)

**Scope:** Phases 24–30 (7 phases, 19 plans, 35 tasks). Audit: `gaps_found` — 10/12 requirements fully satisfied, +1 partial-by-design (HARD-03 D-15), +1 documented x64 deferral (HARD-02); 7/7 phases executed (6 verified + 1 closed-by-deferral), 12/12 integration links WIRED, 2/2 E2E flows complete, 0 silent gaps (`milestones/v2.3-MILESTONE-AUDIT.md`). Two independent streams: (A) in-client C++ hardening (Phases 24–27), (B) a new standalone web-based TRE compare tool (Phases 28–30). Timeline: 2026-06-12 → 2026-06-15. (`master` is a live upstream-integration branch — milestone-scoped phase/plan counts are authoritative, not the upstream-inflated diffstat.)

**Delivered:** Consolidated the v2.2 visual-parity win — operationalized the DPVS verdict behind config, made the staged client machine-portable, fixed the Options-window FATAL, stripped superseded debug instrumentation, and characterized the remaining 32-bit-bound quirks (corner-snap, D3DCompile port) as root-caused x64 carry-forwards — **and** shipped the repo's first web app, a localhost TRE compare tool for cross-installation archive diagnostics. The client stays bootable to character select under both `rasterMajor=5` (D3D9) and `=11` (D3D11) after every client-touching phase.

**Key accomplishments:**

1. **DPVS config-gate + machine portability (Phase 24, HARD-01/PORT-01/PORT-02)** — `[ClientGraphics/Dpvs] occlusionMode = auto|on|off`: auto-gates the occlusion bit on POB-cell membership (outdoor on / indoor off, per the Phase 23 verdict), F11 override preserved. Miles audio redist vendored + postbuild-repointed with a codec-repair guard (no more silent half-dead-audio on a fresh clone); a tracked `client.cfg.template` + `setup-client.ps1` generator de-hardcodes `stage/override`+`stage/miles` and cleans the accumulated Phase-11+ test keys. Dual-renderer boot user-verified.
2. **Instrumentation removal + Options-window FATAL (Phase 26, HARD-03 D-15 / HARD-04)** — `DpvsProfileInstrumentation` removed atomically (commit `6c95fa990`, grep-zero; Debug+Release `/FORCE` link-grep = 0 unresolved); the CORNERSNAP `_DEBUG` probes intentionally KEPT as the x64 corner-snap acceptance harness. The pre-existing Options-window FATAL (`checkShowToolbarCommandCooldownTimer` CodeData/.ui mismatch from feature commit `d1b3c0eaf`) confirmed fixed (`5fce7bb83`); dual-renderer Options-open smoke passed.
3. **32-bit-bound quirks root-caused → x64 (Phases 25 + 27, HARD-02 / HARD-05)** — the cantina corner-snap (CONSULT-43): interior snap resolved-by-config; residual **door-snap** diagnosed as a 32-bit build/codegen float transient in the cell→world Y transform — not a portal-replay re-entrancy bug — so it is parked for x64 (ship-D3D11 or 64-bit is the real fix). The `D3DXCompileShader`→`D3DCompile` swap (Fix B) was attempted and reverted (`c0f890875`): gl05/gl07 re-fight the entire gl11 shader-modernization battle, x64-milestone-sized work; the proven Fix-A SEH guard already catches the modern-toolchain FP fault, so HARD-05's intent is met and the clean port moves to x64.
4. **TRE compare tool — headless backend (Phases 28–29, TRE-01..04)** — an isolated, extractable `tools/tre-compare/` uv package (zero engine/MSBuild coupling): vendored stdlib parser (`tre_reader.py`/`tre_decrypt.py`, all TREE variants + COT2000/SearchTOC), a `[SharedFile]` hand-parser yielding engine-faithful `searchTree`/`searchTOC`/`searchPath` precedence, a first-hit-wins merged-virtual-tree builder with `fixUpFileName` + per-node-type tombstones, a pure set/file diff engine (`(length, compressedLength)` tri-state signal — never the path-CRC — + on-demand xxhash drill-in), a stdlib-sqlite3 parse-skip cache, and 4 stateless FastAPI routes. Unit-tested against the real `stage/client.cfg`; 72 backend tests green.
5. **TRE compare tool — frontend SPA (Phase 30, TRE-05)** — the repo's **first frontend**: a Vite 8 / React 19 / TS 6 / Tailwind 4 / shadcn (zinc, dark) linked master-detail SPA — install pickers → cross-filtering set-delta strip → virtualized merged file-tree (TanStack Virtual, roll-up badges, hide-identical/search/filter) → auto-verdict detail panel enforcing the metadata-vs-content honesty distinction — served from the single 127.0.0.1 FastAPI process via a last-mounted `SpaStaticFiles`. SC#4 human-approved on a real 231,086-row cross-distribution diff (SWGSource × SWG Infinity).

**Known deferred at close (x64 carry-forwards — all documented + tracked):** HARD-02 residual door-snap, HARD-03 CORNERSNAP-probe removal (the harness for HARD-02), and HARD-05's clean D3DCompile / D3DX-removal port — all bound to the 32-bit toolchain and naturally homed in the future x64 milestone (`todos/pending/2026-06-13-64bit-x64-port.md`). **Open artifacts acknowledged at close: 8** (2 debug sessions — cantina-corner-snap parked, safecast-null-cast closed; 5 low/deferred todos; Phase-24 UAT which is `passed`/0 pending) — see STATE.md Deferred Items. Doc-hygiene + WARNING-level tech-debt (backend bare-500 on missing archive, relative-searchTree anchoring, code-review WR/IN items) catalogued in the audit `tech_debt`.

**Archives:** `milestones/v2.3-ROADMAP.md`, `milestones/v2.3-REQUIREMENTS.md`, `milestones/v2.3-MILESTONE-AUDIT.md`. Tag: `v2.3`.

---

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
