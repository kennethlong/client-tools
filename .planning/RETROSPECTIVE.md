# Project Retrospective

Living retrospective across milestones. Newest milestone first.

## Milestone: v2.2 — Visual Parity

**Shipped:** 2026-06-12
**Phases:** 17–23 (17/18/23 via GSD, 19–22 ad-hoc) | **Plans:** 15 tracked | **Audit:** tech_debt (13/13 requirements, 13/13 integration WIRED, 0 blockers)

### What Was Built
D3D11 visual parity with the D3D9 baseline. Core: the asset pixel-shader pipeline (Phase 17) — recompile the discarded `TAG_PSRC` source instead of the D3D11-rejected PEXE bytecode, reconstruct PS input signatures against the VS skeleton (binds 0/9→9/9), upload constants reflection-driven. Then outward: load-screen seam (18), interior/world lighting parity + FFP combiner-cascade PS + per-pixel fog in all three PS lanes, gamma pre-Present curve pass, round minimap, terrain blend-factors re-land, particles/ribbons, exterior-geometry closure (ad-hoc 19–22), and the DPVS D3D11 remeasure (23) — both Phase 10 verdicts FLIPPED (outdoor keep / indoor remove), Option α revised on paper. Bonus: audio fully restored (missing `stage/miles/` redist), combat kill-crash fix, warning-flood perf drag eliminated.

### What Worked
- **The char-select beachhead strategy.** Proving the PS pipeline on a deterministic, bounded, isolated screen (9 shaders) before world-scale extension converted the milestone's biggest unknown into a measured, verified foundation everything else built on.
- **Read the D3D9 reference sequence FIRST.** The single most productive diagnostic habit of the milestone: the reference renderer IS the spec. Lighting flicker, blend factors, sticky dot3, the Compare[]-table swap — each cracked by reading the D3D9 path, not by probing the broken D3D11 one.
- **RenderDoc CLI/MCP as arbiter.** Pixel-history → draw → shader traces (Capture39 terrain trace, the magenta wall-light hunt) repeatedly converted "three AIs agree on a plausible wrong answer" into ground truth. Cross-renderer light-color dumps arbitrated the blue-face hunt the same way.
- **The shader-override workflow** (`stage/override/` + searchPath > TREs, IFF rebuilder scripts, now git-tracked). Made per-shader fixes (face tint, minimap mask, emissives, tfcl hemispheric sun) shippable without TRE repacks — the milestone's workhorse delivery mechanism.
- **Ad-hoc fix-to-advance for the 19–22 frontier.** The remaining gaps were entangled runtime bugs (a fix for "interior lighting" closed particles too); iterative RenderDoc-driven sessions with handoff docs as state beat plan/execute cycles. The milestone audit served as the verification record.
- **MCP-driven client piloting** (keybd_event walks, 360° panoramas, in-client A/B captures) let verification sweeps run without burning Kenny's time on every check.

### What Was Inefficient
- **Correlated multi-AI consensus.** All three AIs (Claude+CODEX+Cursor) repeatedly converged on the SAME wrong hypothesis (cape-spike ring designs ×7, blue-face alpha/hemispheric/R↔B turns). The fix was de-anchoring re-tasks + an empirical arbiter (RenderDoc / sentinel writes / light-color dumps) — consensus ≠ correctness.
- **The cape-spike hunt.** 7 dynamic-VB ring designs failed because the VB was never the variable (collide() corrupting the static INDEX buffer via D3D11's copy-out lock semantics). Lesson: characterize the actual GPU fetch window before redesigning a subsystem.
- **ABI cascade traps.** Shared-header struct edits (17-01) crashed stale plugin DLLs twice (gl11 4 days stale; the Release boot-crash took 8 runtime experiments + 2 consult rounds to pin to an EXE/DLL offset mismatch). Rebuild ALL plugin vcxprojs on any shared-header touch.
- **Artifact drift, again.** Phases 19–22 produced no GSD artifacts; REQUIREMENTS checkboxes vs traceability table disagreed; an earlier same-day audit went stale within hours of Phase 23 landing. Accepted cost — but the close required reconciliation passes.

### Patterns Established
- **Parity-work default:** characterize the reference sequence first, diff the target, THEN hypothesize.
- **Three PS lanes** (native asset PS → 17-07 rewritten asset PS → generated fallback incl. FFP combiner cascade) with the magenta fallback kept as a visible diagnostic tombstone — magenta on screen = a shader failed PS-gen, go look.
- **PSRC `//hlsl` override recipe** for any ps.1.1 tombstone (texren bakes, minimap mask, emissives): reauthor → IFF rebuild → `stage/override/pixel_program/`.
- **Empirical arbiters over consensus:** RenderDoc capture-diff, cross-renderer state dumps, sentinel writes — break correlated-AI deadlocks with ground truth, not more opinions.

### Key Lessons
1. The reference implementation is the spec — read it before hypothesizing about the broken port.
2. Multi-AI agreement is not evidence; an empirical arbiter is. Budget for capture/dump tooling early in any parity milestone.
3. D3D9→D3D11 porting bug classes that recurred: copy-out lock semantics (collide IB corruption), partial-WRITE_DISCARD cbuffer tails, register-leak defaults (alpha-fade, sticky dot3), first-use-order SM4 register assignment vs fixed D3D9 stages, and blend ENABLE without blend FACTORS.
4. Ad-hoc execution works when handoff docs carry state and a milestone audit closes the verification gap — but reconcile planning artifacts at close, not mid-flight.

### Cost Observations
- Model: Opus/Fable (quality profile) + 4-consultant crew (Cursor/Codex/Sonnet/Opus) on the hard walls; RenderDoc MCP + windows-mcp piloting reduced human-verify round-trips.
- Timeline: 16 days (2026-05-27 → 06-12), 119 commits; the 19–22 frontier compressed into a 5-day fix-to-advance sprint.

---

## Milestone: v2.1 — Decruft

**Shipped:** 2026-05-27
**Phases:** 12–16 (5, incl. inserted Phase 16) | **Plans:** 16 | **Audit:** tech_debt (functional PASS, 7/7 DECRUFT, 0 blockers)

### What Was Built
Re-applied the orphaned v2.0 dead-code removals (CLEAN-01..04) against the active Koogie/MSBuild tree: five dormant subsystems fully unlinked + deleted — trackIR/stationapi/SwgClientSetup/lcdui (Phase 12), VideoCapture (13), Vivox voice chat (14), XPCOM/Mozilla in-game browser (15) — followed by a tech-debt cleanup pass (16: SwgGodClient 989crypt token, dead finalUrl block, orphaned voice-volume API). ~2,000+ files deleted (incl. the 1,866-file libMozilla tree + 138-file vivox trees); client kept bootable to character-select under both renderers after every removal.

### What Worked
- **Risk-gradient phase ordering** (pure deletes → lib unlinks → live-source surgery). Re-establishing the dual-renderer boot baseline on the cheap Phase 12 deletes before the riskier Vivox/XPCOM source surgery meant any regression had an unambiguous origin.
- **The `/FORCE` link-grep gate.** Grepping link output for `unresolved external symbol` (==0) rather than trusting MSBuild exit 0 caught two real Phase 12 defects (the live `989crypt.lib` dep; orphaned ClientHeadTracking callers). Became the standard removal-validation primitive for the whole milestone.
- **Code review catching what gates can't.** CR-01 — the voice-enum deletion silently shifting surviving radial-menu ordinals off their retail-datatable rows — is invisible to both the link gate and the boot gate. Code review caught it; ordinal-preserving placeholders fixed it; the lesson propagated forward to Phase 15's TUIWebBrowser enum delete.
- **Incremental boot gates.** DECRUFT-07 was a cross-cutting gate owned by Phase 15 but verified after every removal, so the milestone-end gate was a confirmation, not a discovery.
- **The milestone audit earned its keep.** It surfaced real residue (989crypt latent LNK1181, dead source) that became Phase 16, rather than shipping it as silent debt.

### What Was Inefficient
- **Validation docs lagged reality.** Phases 12/13/16 VALIDATION.md were authored as strategy drafts and never flipped after execution — `draft`/`nyquist_compliant: false` while the phases were verified-passed. Finalised retroactively at milestone close (this session). Cheap to fix, but recurring drift.
- **MSYS `sed` backslash/back-reference gotcha** on Windows `.vcxproj` path purges cost time across Phases 13–15 (a path like `...\3rd\...` mis-parses `\3` as a back-reference). Resolved by matching segments by substring+delimiter, not escaping literal backslashes.
- **CLI count drift.** `milestone.complete` reported 4 phases/13 plans (it doesn't see the inserted Phase 16); the MILESTONES.md entry needed hand-correction to 5 phases/16 plans, and the auto-extracted accomplishments included noise lines that needed curating.

### Patterns Established
- **Removal-validation triad:** per-token `rg` grep-zero (seconds) → MSBuild Debug+Release link-grep `unresolved external symbol`==0 (minutes) → manual dual-renderer boot to character-select. The project's standard dead-code-removal verification (no unit-test harness exists in this C++ tree).
- **Ordinal-preserving placeholders** (`RESERVED_*`) for any deletion from a positional enum/table that mirrors a retail-TRE row index — never mid-sequence deletes.
- **KEEP-list discipline** during broad token sweeps (soePlatform libs, crypto.lib, binkw32, Miles) to avoid the adjacency trap of deleting a live dep next to a dead one.

### Key Lessons
1. On a `/FORCE`-linked target, exit 0 is not proof of a clean link — grep the link log.
2. Deletions from positional enums/tables that mirror retail-TRE row indices are a regression class that build + boot gates cannot catch; only review or a placeholder convention catches them.
3. Finalise validation/Nyquist docs at phase close, not at milestone close — the drift is cheap individually but compounds into a retroactive sweep.

### Cost Observations
- Model: Opus (quality profile). Cross-AI consults available but less needed than v2.0's D3D11 work — removal is more mechanical than renderer bring-up.
- Solo hobby cadence; 2026-05-25 → 05-27 (3 days wall-clock for the removal phases + audit + tech-debt close).

---

## Milestone: v2.0 — Modernisation

**Shipped:** 2026-05-25
**Phases:** 7–11 (5) | **Plans:** 29 tracked | **Audit:** tech_debt (functional PASS)

### What Was Built
A modern MSVC/C++20/MSBuild SWG client booting to character select and rendering a ground scene under both a D3D9 and a new D3D11 renderer (runtime-selectable via `rasterMajor`). Dead-code removal (designed), STLPort→MSVC STL migration (via wholesale adoption of Koogie's tree), DPVS profiling verdict, and the long-tail D3D11 renderer plugin.

### What Worked
- **Option D (Phase 9 pivot).** Rather than hand-migrate STLPort per-file, adopting Koogie's already-migrated MSBuild tree wholesale (merge `479d35df3`) collapsed ~30 commits of risky refactor into ~3. Pragmatic leverage of upstream community work.
- **Cross-AI consults (CODEX + Cursor).** Repeatedly unblocked opaque D3D11 walls (X4016 register collision, the WRITE_DISCARD-garbage cbuffer hypothesis, SRV0 binding, trianglefan index expansion) where textual rewrite had hit its limit.
- **Empirical "draw activity observable" vs "visible geometry" bar separation** (CODEX insight) kept the shader-binding arc honest about what each iteration actually proved.
- **Diagnostic infrastructure as first-class** — InfoQueue file sink, frame-stats, show-window helper, screenshot baselines — made fix-to-advance debugging tractable on a 20-year-old engine.
- **Fix-to-advance discipline.** Solo, ad-hoc, problem-driven cadence shipped a working renderer faster than a rigid plan would have.

### What Was Inefficient
- **Artifact-vs-reality drift.** Ad-hoc work + the Option-D base swap left planning docs (REQUIREMENTS/PROJECT) describing the abandoned CMake/whitengold tree while the active tree is MSBuild. The milestone audit surfaced this; docs were re-anchored at close. Accepted cost of pragmatic velocity.
- **Phase artifacts scattered across repos.** Phases 1–9 lived in the old whitengold repo; only 10–11 were here until the 2026-05-25 consolidation import.
- **D3D11 iteration sprawl.** Phase 11 ran ~45 iterations across many sub-plans (11-09.x) — the visual-parity cascade was hard to bound; the real blocker (asset PS pipeline) was only named late.

### Patterns Established
- Renderer selection via a single `Gl_api` loader (`Graphics.cpp`) multiplexing DLL families by `rasterMajor` — D3D9 and D3D11 coexist; D-05 (keep D3D9 buildable) as a standing invariant.
- D3D11 cbuffer matrices transposed at upload (col-vec engine vs row-vec bytecode).
- Matched D3D9-vs-D3D11 screenshot baselines as the visual-parity verification method (vs a formal VERIFICATION.md flow).

### Key Lessons
1. Adopting upstream work wholesale can beat re-deriving it — but it orphans the prior approach's artifacts; reconcile the docs at the boundary, not later.
2. On a legacy engine, a deterministic 2D repro (the load-screen half-texel seam) is worth more than a dozen messy in-world captures for isolating a class of bug.
3. Functional reality (builds/boots/renders) is the source of truth; planning docs are scaffolding, not contract.

### Cost Observations
- Model: Opus (quality profile). Cross-AI consults (CODEX/Cursor) used for opaque-error unblocks.
- Sessions: many; long-running solo hobby cadence over ~3 weeks (2026-05-05 → 05-25).

---

## Cross-Milestone Trends

| Milestone | Phases | Audit | Headline |
|-----------|--------|-------|----------|
| v2.0 Modernisation | 7–11 | tech_debt | MSVC/C++20/MSBuild client; D3D9+D3D11 selectable |
| v2.1 Decruft | 12–16 | tech_debt | 5 dormant subsystems unlinked + deleted; client stays bootable both renderers |
| v2.2 Visual Parity | 17–23 | tech_debt | D3D11 matches D3D9 baseline; asset-PS pipeline + 12 visual gaps closed; DPVS verdict revised |

**Recurring trend:** every milestone closes `tech_debt` (never `gaps_found`, never pristine `passed`) — functional reality ships ahead of artifact hygiene, and the audit + close reconciliation absorbs the drift. Working as intended for a solo hobby cadence; the per-close reconciliation cost is stable (~1 session).
