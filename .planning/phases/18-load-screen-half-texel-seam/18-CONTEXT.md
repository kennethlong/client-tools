# Phase 18: Load-Screen Half-Texel Seam - Context

**Gathered:** 2026-05-30
**Status:** Ready for planning

<domain>
## Phase Boundary

Eliminate the vertical centerline seam on D3D11 load screens so the fullscreen splash blit matches the known-good D3D9 (`rasterMajor=5`) baseline. This is an architecturally **independent, deterministic 2D-sampling canary** (no PS pipeline, no 3D geometry/skinning/lighting) whose job is to establish the **half-pixel convention once, centrally** for D3D11 — the same convention the Phase 20 round-minimap work will lean on.

**In scope:** UI-01 — no vertical centerline seam on D3D11 load screens, image-independent, fixed centrally (not per-draw +0.5 fudge).

**Out of scope (own phases):** asset PS pipeline / char-select textures (Phase 17), gamma LUT + interior lighting (Phase 19), round minimap + open-world surfaces (Phase 20), particles/ribbon (Phase 21), exterior geometry shards (Phase 22), DPVS remeasure (Phase 23). This phase fixes the *2D screen-space sampling convention*, not any 3D path.

</domain>

<decisions>
## Implementation Decisions

### Root cause + fix site (the central decision)
- **D-01 — Fix site is CONSULT-GATED, not pre-locked.** The ROADMAP success-criterion #3 names `getOneToOneUVMapping`, but codebase scout found that function has **zero callers** anywhere in engine or game, and its D3D11 entry is `STUB(getOneToOneUVMapping)` → `scaffold_fatal_stub` (would FATAL if the load screen routed through it — it doesn't). **Implementing that stub would fix nothing visible.** The real seam is the D3D9 `D3DFVF_XYZRHW` pre-transformed fullscreen quad: the half-pixel offset is *baked into the vertex positions* (captured live in the Phase 11 consult as `pos=(-0.5,-0.5)…(1023.5, 767.5)`), which aligns under D3D9's removed-in-D3D10 -0.5 texel rule but misaligns at the shared edge under D3D11. **Before any edit lands, run a CODEX + Cursor cross-AI consult** (Kenny's standard second-opinion workflow) to confirm the exact central fix location in the current gl11 transformed-vertex→NDC conversion path. Rationale: the Phase 11 XYZRHW diagnosis is strong but predates the current plugin state, and this is a *shared* 2D-path edit (see D-02) — peer sanity-check the site before committing.
- **D-01a — Consult brief contents (for research/planning to assemble):** the Phase 11 XYZRHW diagnosis + fix-path A/B/C options; the dead-`getOneToOneUVMapping` finding (uncalled + fatal-stub); the live transformed-vertex conversion sites in the D3D11 plugin (`Direct3d11_HlslRewrite.cpp`, `Direct3d11_VertexShaderData.cpp/.h`, `Direct3d11_StateCache.cpp`); and the "fix once centrally, not per-draw +0.5" constraint. Save consult outputs under the established `.planning/research/CONSULT-18-*{,-codex,-cursor}.out` naming convention.
- **D-02 — Central, not scattered.** Whatever the consult confirms, the half-pixel convention is corrected **once** at the shared screen-space/viewport convention site — NOT as per-draw `+0.5` fudge factors sprinkled across call sites (ROADMAP criterion #3, locked).

### Validation bar (blast radius)
- **D-03 — Full 2D no-regression sweep.** The transformed-vertex (XYZRHW) path drives **all** 2D UI under D3D11 — HUD, fonts, radial menu, in-game menus, char-select 2D — not just the splash. A central half-pixel convention change therefore touches everything 2D. Validation MUST confirm both: (a) the load-screen seam is gone, AND (b) no shift/blur/misalignment regression appears on HUD, fonts, radial menu, and char-select 2D under D3D11. A "splash fixed but HUD now blurry" outcome is a FAIL.
- **D-04 — Hard gates (carried from Phase 17 D-06).** Both `rasterMajor=5` (D3D9) and `=11` (D3D11) boot without crash; **the D3D9 reference path is byte-for-byte unregressed** (this is a gl11-only change). Keep any diagnostic tombstones visible.

### Evidence / image-independence (criterion #2)
- **D-05 — Multi-image A/B in the COMPARISON dir.** Capture matched `rasterMajor=5`/`=11` pairs across **≥3 distinct splash variants** into the Phase-17 D-05 structured COMPARISON convention (mirroring `docs/research/phase12-baseline/COMPARISON.md`). Manual A/B is the gate; structured naming leaves the door open for a later scripted pixel-diff without recapturing. This *demonstrates* image-independence rather than relying solely on the prior COMPARISON.md observation. **Primary repro = the world loading screen** (Kenny: clearest place to see the seam); confirm it stays gone at higher resolution too (the seam was confirmed present at higher res, i.e. resolution-independent).

### Claude's Discretion
- Exact conversion-site edit (shader-body passthrough vs CPU NDC conversion vs viewport convention) — decided by the D-01 consult outcome + planner/executor, as long as it satisfies D-02 (central) and D-04 (D3D9 unregressed).
- Which 3 splash variants to capture for D-05, and the config/zone path used to reach them deterministically.
- Consult-flag/instrumentation gating mechanism (if any temporary vertex-dump logging is re-added to confirm the seam) — executor's choice, off-by-default, D3D9 untouched.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Root-cause diagnosis (READ FIRST)
- `.planning/phases/11-d3d11-renderer-plugin/11-09.15-CODEX-CONSULT-xyzrhw-transformed-vertex-handling.md` — the live diagnosis of D3D9 `D3DFVF_XYZRHW` pre-transformed vertex handling under D3D11, including the captured loading-screen fullscreen-quad positions and the three candidate fix paths (A: shader-body, B: CPU NDC conversion, C: passthrough VS substitution). This is the seam's root cause.
- `.planning/phases/11-d3d11-renderer-plugin/11-09.15-CODEX-CONSULT-xyzrhw-transformed-vertex-handling-RESPONSES.md` — reviewer responses to the above (if present).
- `docs/research/phase12-baseline/COMPARISON.md` §5 "Loading-screen centerline seam" — the image-independence evidence + the matched-pair screenshot convention to mirror for D-05.

### Requirements + roadmap
- `.planning/ROADMAP.md` § "Phase 18: Load-Screen Half-Texel Seam" — goal, 3 success criteria, the (now-corrected) approach note. NOTE: criterion #3's literal `getOneToOneUVMapping` reference is superseded by D-01 — the function is dead; fix the transformed-vertex path instead.
- `.planning/REQUIREMENTS.md` — UI-01 acceptance text.

### Engine source landmarks (verify before editing)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp:1056` — `STUB(getOneToOneUVMapping)` (the dead/fatal-stub slot named by the ROADMAP — do NOT assume implementing it fixes the seam).
- `src/engine/client/application/Direct3d9/src/win32/Direct3d9.cpp:3676-3682` — D3D9 `getOneToOneUVMapping` with the `+0.5f` half-texel offset (reference for the D3D9 convention; itself also uncalled).
- `src/engine/client/library/clientGraphics/src/win32/Graphics.cpp:1675-1679` — the `Graphics::getOneToOneUVMapping` dispatch through `ms_api`.
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_HlslRewrite.cpp`, `…/Direct3d11_VertexShaderData.cpp` + `.h`, `…/Direct3d11_StateCache.cpp`, `…/Direct3d11_Device.cpp`, `…/Direct3d11_VertexBufferDescriptorMap.cpp` — the D3D11 transformed/screen-space (`xyzrhw`) handling surface; the actual central fix site lives here (exact site = consult outcome).
- `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiSplash.cpp` — the splash/load-screen mediator (the on-screen consumer; not the bug site, but the path that produces the fullscreen quad).

### Consult workflow
- `.planning/research/CONSULT-18-*{,-codex,-cursor}.out` — where the D-01 CODEX+Cursor consult outputs will land (mirrors prior `CONSULT-17-07-*` naming).

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- The Phase 11 D3D11 transformed-vertex handling (the path that already renders 2D UI correctly enough to boot to char-select) is the substrate being adjusted — the fix is a convention correction within it, not a new subsystem.
- The Phase 17 D-05 COMPARISON dir + matched-pair naming convention (`docs/research/phase12-baseline/COMPARISON.md` style) is reused wholesale for D-05 evidence.

### Established Patterns
- **CODEX+Cursor cross-AI consult** (Kenny's standard second-opinion loop) gates the fix-site decision (D-01); outputs saved under `.planning/research/CONSULT-18-*`.
- **gl11-only, D3D9 byte-for-byte** invariant: every D3D11 visual edit must leave `rasterMajor=5` untouched (D-04).

### Integration Points
- The fix lands in the D3D11 plugin's screen-space/transformed-vertex → NDC conversion, which is shared by every 2D draw — hence the D-03 full-2D validation requirement.

</code_context>

<specifics>
## Specific Ideas

- Kenny explicitly wants the fix-site contradiction (dead `getOneToOneUVMapping` vs the real XYZRHW path) settled by a **CODEX + Cursor consult** rather than a unilateral lock — consistent with his established cross-AI second-opinion workflow.
- The captured fullscreen-quad vertices from the Phase 11 consult (`(-0.5,-0.5)…(1023.5,767.5)`) are the concrete fingerprint of the baked D3D9 half-pixel offset — use them as the worked example in the consult brief and to confirm the seam math.
- **Repro (Kenny, 2026-05-30):** the **world loading screen** under `rasterMajor=11` is the clearest, easiest place to see the seam — use it as the primary deterministic capture target for D-05. The seam **persists at higher resolution** (does not scale away), further corroborating that it is a fixed path-level half-pixel offset rather than a per-image or per-resolution quirk.

</specifics>

<deferred>
## Deferred Ideas

- **Implementing `getOneToOneUVMapping` for the Phase 20 minimap** — raised as a future-proofing option, deferred. If the minimap needs a 1:1 UV mapping helper, address it in Phase 20 against that phase's actual call sites rather than pre-building a still-uncalled function here.
- None other — discussion stayed within phase scope.

</deferred>

---

*Phase: 18-load-screen-half-texel-seam*
*Context gathered: 2026-05-30*
