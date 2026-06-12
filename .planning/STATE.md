---
gsd_state_version: 1.0
milestone: v2.2
milestone_name: Visual Parity
status: executing
last_updated: "2026-06-12T15:06:51.665Z"
last_activity: 2026-06-12 -- Phase 23 planning complete
progress:
  total_phases: 7
  completed_phases: 1
  total_plans: 15
  completed_plans: 10
  percent: 67
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-05-27 after v2.1 close)

**Core value:** Every change must leave the client bootable to character select — and for v2.2, D3D11 (`rasterMajor=11`) must visually match the known-good D3D9 (`rasterMajor=5`) baseline. Never regress the D3D9 reference path.
**Current focus:** Phase 18 — load-screen-half-texel-seam (UI-01)

## Deferred Items (acknowledged at v2.0 close)

NOTE: "Remove dead code (CLEAN-01..04 vs MSBuild)" is now CLOSED — it was the v2.1 Decruft milestone (Phases 12–16, shipped 2026-05-27).

Acknowledged and deferred at v2.0 milestone close (2026-05-25):

| Category | Item | Status |
|----------|------|--------|
| debug | cantina-corner-snap | closed (pre-existing engine quirk; not a regression) |
| debug | safecast-null-cast | closed (resolved 2026-05-15) |
| todo | 2026-05-15-cantina-corner-snap-engine-improvement | low — workaround exists |
| todo | 2026-05-15-swgsource-vs-whitengold-tre-asset-diff | low — informational, post-Phase-11 |
| backlog | Dead-code re-removal (CLEAN-01..04) vs MSBuild tree | **CLOSED — shipped as v2.1 Decruft (Phases 12–16)** |
| backlog | DPVS D3D11 remeasure (SPEC R7); CLEAN-06 ~30 tools | DPVS-01 now scheduled as v2.2 Phase 23; CLEAN-06 tools carried to backlog |

## Deferred Items (acknowledged at v2.1 close)

5 open artifacts acknowledged and deferred at v2.1 Decruft milestone close (2026-05-27). All non-blocking — none are v2.1 regressions:

| Category | Item | Status |
|----------|------|--------|
| debug | cantina-corner-snap | closed (pre-existing SOE engine quirk; not a regression) |
| debug | safecast-null-cast | closed (resolved 2026-05-15, ContrailData D-18 guard) |
| todo | 2026-05-15-cantina-corner-snap-engine-improvement | low — annoying, workaround available |
| todo | 2026-05-15-swgsource-vs-whitengold-tre-asset-diff | low — informational, post-Phase-11 |
| todo | 2026-05-27-options-toolbar-cooldown-ui-data-mismatch-crash | medium — pre-existing Options-window FATAL (`checkShowToolbarCommandCooldownTimer` `.ui`-data mismatch from feature commit `d1b3c0eaf`); OUTSIDE the v2.1 diff, NOT a Decruft regression |

Plus v2.2-coupled deferrals (milestone-audit `tech_debt`): `stage/client_d.cfg` accumulated test-settings cleanup (after v2.2 visual parity); AR-15-01 future-TCG-revival re-evaluation (future v2.x). See `milestones/v2.1-MILESTONE-AUDIT.md`.

## Current Position

Phase: 19
Plan: Not started
Status: Ready to execute
Resume: None — Phase 17 complete + verified + secured. Ready to plan Phase 18 (load-screen half-texel seam, UI-01 — `getOneToOneUVMapping` stub; independent of the PS pipeline). gl11_d.dll = HEAD a0d5ac80f (5/30 13:24). See memory project_phase17_charselect_d3d11_parity_verified.
Last activity: 2026-06-12 -- Phase 23 planning complete

## Accumulated Context

### Roadmap Evolution

- [2026-05-27] **v2.2 Visual Parity ROADMAP CREATED** — 7 phases (17–23), 13/13 requirements mapped (CHAR/WORLD/GAMMA/UI/FX/GEO/DPVS). Standard granularity. Phases continue from 16 → start at 17.
- Phase 16 added: v2.1 tech-debt cleanup (SwgGodClient 989crypt.lib + P12-P15 residue) — from milestone audit

### Decisions

**v2.2 Visual Parity (roadmap):**

- [2026-05-27] **Char-select beachhead FIRST (Phase 17).** Prove the asset-PS pipeline on the deterministic, isolated character-select screen (textures + eyes + head) before any open-world work. Use `sul_eye.sht` + `sul_*_head.sht` PLUS one single-stage control (body/clothing) to separate "pipeline works" from "multi-sampler works."
- [2026-05-27] **Phase 17's gating first task = PSRC language census on the REAL asset tree** (retail-TRE extraction; the repo checkout has no extracted `.psh`/`.sht`). Classify `//hlsl` vs asm — the HLSL:asm ratio decides the lane mix and gates the rest of the phase.
- [2026-05-27] **Asset-PS approach (per CODEX+Cursor consult, SUPERSEDES SUMMARY.md on the asm lane):** primary lane = recompile discarded `TAG_PSRC` `//hlsl` via existing `compilePixelShaderFromHlsl` (mirror the VS compile stack); secondary lane = port asm `PSRC` → HLSL → `ps_4_0` (re-assembling asm just reproduces the rejected D3D9 bytecode — named landmine); FFP `TextureOperation` generator is tertiary/narrow ONLY for genuine FFP-only passes. Per-pass `Pass::apply` constants upload **reflection-driven (D3DReflect)**, NOT via copied D3D9 register indices.
- [2026-05-27] **Independent/parallel gaps:** load-screen half-texel seam (Phase 18, UI-01) is small/early/standalone (`getOneToOneUVMapping` stub), independent of the PS pipeline. Gamma LUT (Phase 19) scheduled AFTER the PS pipeline so it's tuned on correctly-shaded content; NO sRGB-view gamma (double-correction trap — keep SRVs `_UNORM`).
- [2026-05-27] **Satellite gaps are SEPARATE from the PS pipeline, not assumed to fall out of it:** interior flat-white (Phase 19, WORLD-03) also needs the simplified `Direct3d11_LightManager` + gamma, not just PS; "eyes through back of head" was already fixed by Iter-44A depth wiring (verify by screenshot in Phase 17, don't re-fold into PS scope); stencil is its own state-parity item; exterior/skeletal shard distortion (Phase 22, GEO-01) gated on a fully-settled re-capture (0013/0014 were mid-load LOD smear) with a single-stream-fix vs flip-to-multistream decision.
- [2026-05-27] **DPVS D3D11 remeasure (Phase 23, DPVS-01) is STRICTLY LAST** — meaningless until geometry renders cleanly.
- [2026-05-27] **Validation = D3D9-vs-D3D11 screenshot diff** against the `docs/research/phase12-baseline/COMPARISON.md` matched pairs. Success = "matches `rasterMajor=5`", NOT "no magenta". Every parity claim needs a matched pair (Iter-44B minimap over-claim lesson). Do NOT mark UI-02 minimap done without a diff.
- [2026-05-27] **Cross-cutting landmines for executors:** boot-gate BOTH `rasterMajor=5` and `=11` on any `ShaderImplementation.cpp` edit; keep D3D9 `load_0000` PSRC behavior byte-for-byte identical except storing text; mirror the intentional D3D9 `Compare[]` swap (`C_GreaterOrEqual`↔`C_NotEqual`), don't "fix" it; every new cbuffer matrix needs `XMMatrixTranspose`; persistent baked RT stays `B8G8R8A8_UNORM`; do NOT re-enable per-pass blend factors early (Iter-44C amplification regression); feed generator/rewrite/profile changes into the `.cso` cache key/version.

**v2.1 Decruft (roadmap):**

- [2026-05-25] v2.1 framed as **cleanup-only**: re-apply the orphaned CLEAN-01..04 removals to the active MSBuild tree. Visual Parity reordered to v2.2 (cleanup-first shrinks surface area before upstream imports). Reference diff template: the original whitengold (swg-client) **Phase 07** removal commits, retargeted CMake → MSBuild.
- [2026-05-25] Phase ordering is **risk-gradient, low-first**: pure deletes (Phase 12) → lib unlink (Phase 13) → live-source surgery (Phases 14 Vivox, 15 XPCOM). Re-establishes the boot baseline before the riskier source removals.
- [2026-05-25] DECRUFT-07 (dual-renderer boot gate) is a cross-cutting milestone gate owned by the final phase (15) but **verified incrementally** after every removal in Phases 12–15 — mirrors v2.0 CLEAN-05.

### Pending Todos

1 pending:

- [Sync community compat fixes from SWG-Source/client-tools master](todos/pending/2026-05-08-sync-swg-source-community-compat.md) — future milestone (not v2.2)

### Blockers/Concerns

- **[v2.2 — census gate]** Phase 17's first deliverable (PSRC census) MUST run against the real retail-TRE asset extraction — this repo checkout has no extracted `.psh`/`.sht`. Neither consult reviewer could verify asset contents from the tree. The HLSL:asm ratio is the single biggest open unknown and gates the lane mix.
- **[v2.2 — screenshot-diff discipline]** Every parity win requires a matched D3D9/D3D11 pair from the same scene/pose. The Iter-44B minimap over-claim propagated a false win through a full plan cycle. Do NOT mark UI-02 done without a diff.
- **[v2.2 — boot invariant]** Every shared-`clientGraphics` edit must boot-test BOTH `rasterMajor=5` AND `rasterMajor=11` before being claimed done. Debug exe reads `client_d.cfg` (not `client.cfg`) — set rasterMajor there for each smoke (memory feedback_debug_exe_reads_client_d_cfg).
- **[Phase 12 — /FORCE false-pass, still applies]** SwgClient links under `/FORCE`, which downgrades unresolved externals to WARNINGS and still emits a binary with exit 0. `MSBuild exit 0` is NOT proof of a clean link — grep the link output for `unresolved external symbol` (must be 0).

## Deferred Items

Items carried from v1 close:

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| Build warnings | Nested CMake minimum-version deprecation warnings | Deferred (historical CMake tree) | Phase 1 |
| Build warnings | `crypto` C4530 exception-unwind warnings | Deferred | Phase 1 |
| Runtime | 156k DEBUG_WARNINGs at exit (mostly per-frame nebula lightning) | Deferred | v1 post-launch |
| Runtime | 8,152 memory leaks at exit (singletons/caches, expected) | Deferred | v1 post-launch |

## Session Continuity

Last session: 2026-05-27T20:59:43.979Z
Resume (2026-05-27): **v2.2 Visual Parity ROADMAP CREATED** (Phases 17–23; 13/13 requirements mapped 100%). v2.1 Decruft shipped + tagged `v2.1`. Repo: swg-client-v2 (MSBuild/Koogie) is the single source of truth.

**v2.2 Visual Parity — the plan (7 phases):**

1. **Phase 17** (CHAR-01/02/03) — PSRC census on the real asset tree (gating first task), then prove the recompile + reflection-driven-constant pipeline on char-select: textures, eyes, multi-stage head. Primary lane = `//hlsl` PSRC recompile; secondary = asm→HLSL port; tertiary/narrow = FFP generator. UI hint.
2. **Phase 18** (UI-01) — load-screen half-texel centerline-seam fix (`getOneToOneUVMapping` stub). Independent of Phase 17; safe early/parallel canary. UI hint.
3. **Phase 19** (GAMMA-01, WORLD-03) — gamma LUT post-pass (D3D9 `pow()` ramp, NO sRGB views) + interior lighting via per-pass light constants + simplified `Direct3d11_LightManager` parity. After Phase 17.
4. **Phase 20** (WORLD-01, WORLD-02, UI-02) — extend the PS pipeline to open-world surfaces + multi-stage `TextureOperation` cascade + round minimap (screenshot-diff verified). After Phase 17.
5. **Phase 21** (FX-01, FX-02) — particles (blend/additive/alpha) + ribbon/swoosh (instrument the draw path first; stretch is NOT a transform bug). After Phases 17/20.
6. **Phase 22** (GEO-01) — exterior static-mesh shard distortion, gated on a fully-settled re-capture (0013/0014 were mid-load LOD smear) + single-stream-fix vs multistream-flip decision. After Phase 20.
7. **Phase 23** (DPVS-01) — DPVS D3D11 remeasure + keep/remove verdict (SPEC R7 deferral). STRICTLY LAST.

Validation throughout = D3D9-vs-D3D11 screenshot diff against `docs/research/phase12-baseline/COMPARISON.md` matched pairs. Success = matches `rasterMajor=5`, not "no magenta".

**Next action:** `/gsd-plan-phase 17` — plan the PSRC census + char-select beachhead. Census tool is the gating first deliverable; needs the real retail-TRE asset extraction.

Known unrelated long-tail (deferred): 0x087a armor_marauder async crash (cross-client, retry works); pre-existing Options-window cooldown-UI crash (commit `d1b3c0eaf`, NOT a regression). Koogie's uncommitted Direct3d9.cpp Utinni vtable probe in the working tree is a separate sidequest — leave untouched.
