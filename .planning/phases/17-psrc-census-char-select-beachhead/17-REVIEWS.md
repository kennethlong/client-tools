---
phase: 17
review_round: 5
reviewers: [codex, cursor]
reviewed_at: 2026-05-29T16:53:55Z
plans_reviewed: [17-05-PLAN.md, 17-06a-PLAN.md, 17-06b-PLAN.md, 17-07-PLAN.md]
prior_rounds:
  - "Round 1 review at commit 797e9db57 (`docs: cross-AI review for phase 17`)"
  - "Round 2 replan at commit 6e0eae4e6 (`docs(17): incorporate cross-AI review feedback (codex+cursor)`)"
  - "Round 2 review at commit 85ff0b90b (`docs(17): cross-AI review round 2 (codex+cursor) — convergence NOT achieved`)"
  - "Round 3 replan at commit 6cfe19a7f (`docs(17): round 3 replan — convergence amendments (SR-2 reflect, HIGH-3 shadow, HIGH-4 routing, SR-3 split, plumbing fixes)`)"
  - "Round 3 review at commit (this file's prior state) — CONVERGENCE ACHIEVED for plans 17-01/02/03"
  - "Plans 17-01..17-04 executed; Phase 17 closed as INFRASTRUCTURE COMPLETE at commit 41d7f2fec; post-04 follow-up commit 04ef976 added materialSpecularColor recognition"
  - "Synthesized 17-VERIFICATION.md inventoried GAP-1 (no A/B screenshot), GAP-2 (no wroteDiffuse/wroteEmissive), GAP-3 (0/9 asset-PS binds — COLOR0 register-position mismatch)"
  - "Round 4 plans 17-05/06/07 authored as the gap-closure wave"
  - "Round 4 review (codex+cursor) — CONVERGENCE NOT ACHIEVED; HIGH items fed into the Round-5 replan"
  - "Round 5 replan: 17-06 split into 17-06a (discovery) + 17-06b (mapping/write); 17-07 gained StateCache+PSData.h scope, main()-param-list rewriter, per-VS reflected-input cache, asset-PS-bound attribution log; 17-05 gained 17-05-task3 park point + symmetric grep sets"
verdict_round3: CONVERGENCE ACHIEVED — plans 17-01/02/03 execution-ready
verdict_round4: CONVERGENCE NOT ACHIEVED — gap-closure plans 17-05/06/07 have HIGH-severity concerns that gate execution (writeVarByName sub-channel impossibility, files_modified contract violation, PSRC syntax mismatch, depends_on metadata deadlock)
verdict_round5: CONVERGENCE PARTIAL — Round-4 folds landed substantively (not cosmetic), but execution is gated on a small set of HIGH/MEDIUM amendments both reviewers converged on (COMPATIBLE-log grep contract mismatch 17-05↔17-07; 17-07 missing depends_on 17-06b + merge/rebuild-order stale-DLL hazard; D3D11_REWRITE_VERSION already "21" in live tree so bump must be →"22"; 17-06b can falsely close GAP-2 via any in-bounds write; 17-07 Task-0 spike is comment-only not a compile gate; VertexShaderData.h likely touched but not in files_modified)
---

# Cross-AI Plan Review — Phase 17 (PSRC Census + Char-Select Beachhead) — ROUND 3

Same two reviewers as Rounds 1 and 2 (Codex via `codex exec --sandbox read-only` and Cursor via `cursor-agent -p --mode ask --trust`), reading the live tree at `D:/Code/swg-client-v2` and the Round-3-amended plans authored by commit `6cfe19a7f`. Round 2 verdict was **CONVERGENCE NOT ACHIEVED** — three blocking items (SR-2 hardcoded `updatePS(2,...)`, HIGH-3 stale-shadow-across-draws, HIGH-4 invisible `DEBUG_REPORT_LOG_PRINT`) plus five MEDIUM amendments and several LOWs. This Round 3 review independently verifies whether each of those items is now closed with grep-targeted, live-tree-aligned amendments — without regressing the Round 1/2 closures.

## Consensus

Both reviewers **converge on closure of all three Round 2 blockers and all five MEDIUM amendments**. Both verdicts read as "ready for execution under normal checkpoint discipline" — exactly the Round 2 forecast: "After that, plans would be MEDIUM risk — suitable for execution with normal checkpoint discipline."

| Item | Codex Round 3 | Cursor Round 3 | Consensus |
|------|---------------|----------------|-----------|
| HIGH-1 (non-fatal compile) | CLOSED | CLOSED | ✓ Closed |
| HIGH-2 (DXBC + compile-time reflect) | CLOSED | CLOSED | ✓ Closed |
| HIGH-3 (stale shadow across draws) | CLOSED | CLOSED | ✓ Closed |
| HIGH-4 log routing (`InfoQueue` dual-route) | CLOSED | CLOSED | ✓ Closed |
| HIGH-4 contingency (Option B downgrade) | CLOSED | CLOSED | ✓ Closed |
| HIGH-5 (census file sink) | CLOSED | CLOSED | ✓ Closed |
| SR-1 (D3D9 source-data port) | CLOSED | CLOSED | ✓ Closed |
| **SR-2 (offset-aware reflection upload)** | **CLOSED** | **CLOSED** | **✓ Closed (Round 2 convergent HIGH resolved)** |
| SR-3 (asm STOP-CONDITION, Task 0 split) | CLOSED | CLOSED | ✓ Closed |
| LOWs (8 items combined into Round 3) | CLOSED with minor residue | CLOSED | ✓ Closed |

**Codex verdict:** `CONVERGENCE PARTIAL — minor items, executable with executor discretion.` (One LOW-severity NEW concern — Plan 03 artifact-metadata residue at `17-03-PLAN.md:40-42`.)

**Cursor verdict:** `CONVERGENCE ACHIEVED — ready for execution under normal checkpoint discipline.` (Three MEDIUM NEW concerns — `must_haves.truths` wording drift vs R3-03c, empty-reflection-cache skip path, conditional shadow-mirror vs unconditional staging — all classified as checkpoint-foldable, not Round 4 blockers.)

**Convergent NEW finding (both reviewers):** Plan 03's `must_haves.truths` / artifact metadata still carry text from the pre-R3-03c contract (mentions of `updatePS(2,...)` and `getPerMaterialShadow` attributed to `Direct3d11_StaticShaderData.cpp`). Both reviewers note the **action text and acceptance greps are correct** — only the frontmatter wording is stale. Codex calls it LOW; Cursor calls one variant MEDIUM (the `updatePS(2,...)` wording in truths line 22-24) because an executor scanning `must_haves` first could be misled. Neither reviewer treats it as Round-4-blocking; both recommend a one-line frontmatter cleanup OR an explicit Task 1 start-up note "acceptance block `:561-576` is authoritative."

Plus a few execution edge cases (Cursor MEDIUMs): empty `layouts` from a reflection failure leaves prior GPU state on slot b2 (mitigated by per-shader skip logging in SUMMARY); shadow mirror is conditional on `writeVarByName` success but staging zero-fills unconditionally (mitigated by R3-02e truncation WARN + R3-03g cross-clobber log).

Both reviewers explicitly recommend proceeding to `/gsd-execute-phase 17`, gated on Plan 02 Task 0's `checkpoint:human-verify` census-gate verdict (all-HLSL anchors → continue; asm in anchors → spawn `17-02B-PLAN.md` per SR-3 contract).

---

## Codex Review

### Summary

Round 3 closes the Round 2 blockers in substance: SR-2 is now offset/bind-point driven, HIGH-3 has an unconditional per-pass upload/zero-fill contract, HIGH-4 logging is dual-routed through InfoQueue, and SR-3 is split into a real Task 0 gate. I found no new execution-blocking gaps. One minor metadata residue remains in Plan 03's artifact block: it still lists `getPerMaterialShadow` under `Direct3d11_StaticShaderData.cpp` even though Round 3 says the getter's home is `Direct3d11.cpp`.

### Round 2 Disposition Verification

| ID | Round 3 Verdict | Evidence (file:line) |
|---|---|---|
| HIGH-1 | CLOSED | Non-fatal asset path specified in Plan 02 while fatal install helper remains preserved: `17-02-PLAN.md:16-18`, `17-02-PLAN.md:641-644`; live fatal helper at `Direct3d11_PixelShaderProgramData.cpp:86`, `:241`, `:259`, install path at `:616-623`. |
| HIGH-2 | CLOSED | DXBC blob + cached reflection/accessors specified: `17-02-PLAN.md:18`, `:171-202`; Plan 03 consumes cache and forbids per-draw reflect: `17-03-PLAN.md:21`, `:562-563`; live VS precedent at `Direct3d11_VertexShaderData.cpp:520`, `:612-673`. |
| HIGH-3 | CLOSED | Shared shadow getter plumbing + stale-shadow fix specified: `17-03-PLAN.md:22-24`, `:85-87`, `:573-590`; live current function-local shadow risk at `Direct3d11.cpp:667`, D3D9 per-pass reference at `Direct3d9_StaticShaderData.cpp:835-896`. |
| HIGH-4 log + contingency | CLOSED | InfoQueue dual-route specified and grep-gated: `17-02-PLAN.md:19`, `:63`, `:653-656`, `:695-696`; live precedent at `Direct3d11_PixelShaderProgramData.cpp:523-540`. Contingency correctly downgraded to checkpoint guidance: `17-02-PLAN.md:23`, `:725-759`. |
| HIGH-5 | CLOSED | File sink retained: `17-01-PLAN.md:26-28`, `:195-208`, `:391-403`, `:455-464`. |
| SR-1 | CLOSED | D3D11 source-data cache uses `getMaterial` / `getTextureFactor`: `17-03-PLAN.md:20`, `:557-558`; live APIs at `StaticShader.cpp:510`, `:582`; D3D9 precedent at `Direct3d9_StaticShaderData.cpp:593-702`. |
| SR-2 | CLOSED | Contract A now uses `layout.TotalSize`, `var.StartOffset`, and `updatePS(layout.BindPoint, ...)`: `17-03-PLAN.md:25-27`, `:78`, `:401-461`, `:565-570`; hardcoded `updatePS(2,...)` in `StaticShaderData.cpp` is explicitly forbidden. |
| SR-3 | CLOSED | Task 0 checkpoint owns census gate and STOP/spawn behavior: `17-02-PLAN.md:20-22`, `:62`, `:294-365`, `:808-809`. |
| LOWs | CLOSED with minor residue | TSV length, IFF terminator, BOM/whitespace classification, truncation WARN, source `strlen`, Plan-01 HEAD sha are present: `17-01-PLAN.md:75-78`, `:324-356`, `:401-403`, `:460-464`; `17-02-PLAN.md:64-68`, `:147-164`, `:638-656`. Minor artifact residue at `17-03-PLAN.md:40-48`. |

### Strengths

- Round 3 replaced the SR-2 "presence-only" reflection idea with an actual byte-staging contract keyed by reflected offsets and bind points.
- HIGH-4 is no longer pretending an automatic fallback exists; the plan either logs enough to diagnose linkage or explicitly treats fallback as checkpoint guidance.
- SR-3 is now procedurally enforceable. The executor cannot proceed into CHAR-01 if named anchors include asm without passing through the Task 0 gate.
- The `//hlsl` classifier fix is present in both Plan 01 and Plan 02, including BOM and leading whitespace handling.

### Concerns

- **LOW NEW:** Plan 03's artifact metadata still says `Direct3d11_StaticShaderData.cpp` `contains: "getPerMaterialShadow"` at `17-03-PLAN.md:40-42`, while R3-03h says the getter is defined in `Direct3d11.cpp` and StaticShaderData only calls it at `17-03-PLAN.md:92`. The implementation instructions and verification are clear at `17-03-PLAN.md:85` and `:695`, so this is a cleanup item, not a blocker.

### Suggestions

- Before execution, adjust the Plan 03 artifact block so `Direct3d11_StaticShaderData.cpp` says it calls `getPerMaterialShadow`, while the definition target remains only `Direct3d11.cpp`.
- Keep the existing Plan 03 grep gates exactly as written; they are the important protection against reintroducing Round 2's `updatePS(2,...)` / `UNREF(var)` failure mode.

### Risk Assessment

**MEDIUM** overall. The original blockers are closed, but renderer validation still depends on runtime census data, D3D11 debug logs, and matched A/B screenshots rather than automated tests.

### Verdict on Convergence

**CONVERGENCE PARTIAL — minor items, executable with executor discretion.**

---

## Cursor Review

### Summary

Round 3 closes all three Round 2 blockers with grep-targeted, live-tree-aligned amendments: SR-2 Contract A (offset-aware staging + `layout.BindPoint`), HIGH-3 unconditional per-pass upload with zero-fill on `!pm.m_materialValid`, HIGH-4 InfoQueue dual-route logging, SR-3 Task 0 census gate, getter plumbing, type-name fix, and BOM/whitespace `//hlsl` parity. Round 1/2 closures (non-fatal compile, DXBC+reflect cache, census file sink, SR-1, wave order) are preserved. No Phase 18–22 scope creep detected. Residual gaps are documentation drift in Plan 03 `must_haves.truths` (still mentions `updatePS(2,...)` for the apply path) and a few execution edge cases foldable into checkpoints — not Round 4 architecture blockers.

### Round 2 Disposition Verification

| ID | Round 3 Verdict | Evidence (file:line) |
|----|-----------------|----------------------|
| **HIGH-1** | **CLOSED** | `17-02-PLAN.md:402-432` — `tryCompilePixelShaderFromHlslNoFatal` with FATAL→log-and-return; `:643-644` acceptance preserves `compilePixelShaderFromHlsl` FATAL sites. Live FATAL at `Direct3d11_PixelShaderProgramData.cpp:238-243`, `:259`. |
| **HIGH-2** | **CLOSED** | `17-02-PLAN.md:171-202`, `:507-606` — `m_psBytecodeBlob`, `m_reflectedCbufferLayouts` (+ `TotalSize`), `m_reflectedPSInputs`, ctor-time `D3DReflect`. Plan 03 consumes via `getReflectedCbufferLayouts`; `:563` greps `D3DReflect == 0` in StaticShaderData. VS precedent `Direct3d11_VertexShaderData.cpp:520`, `:611-665`. |
| **HIGH-3** | **CLOSED** | `17-03-PLAN.md:267-282` — file-scope shadow + getter in `Direct3d11.cpp`; `:259-264` — declaration in `Direct3d11_ConstantBuffer.h`. `:392-396` — zero source on `!pm.m_materialValid`; `:461` — unconditional `updatePS(layout.BindPoint, ...)` per layout; `:575` forbids `if (wroteAny)`. Live clobber site `Direct3d11.cpp:667`. **Doc drift:** `must_haves.truths` `:22-24` still say apply flushes via `updatePS(2,...)` — contradicts R3-03c action/acceptance. |
| **HIGH-4 log** | **CLOSED** | `17-02-PLAN.md:557-600` — dual-route `AddApplicationMessage` + `DEBUG_REPORT_LOG_PRINT` for PS-cbuffer and PS-input-sig. `:653-655` acceptance greps InfoQueue route. Precedent `Direct3d11_PixelShaderProgramData.cpp:535-540`. |
| **HIGH-4 contingency** | **CLOSED (Option B)** | `17-02-PLAN.md:23`, `:725-738` — automatic fallback removed from `must_haves.truths`; checkpoint guidance in Task 3. Live: asset PS preferred at `Direct3d11_StateCache.cpp:1107-1109`; fallback only when null `:1111-1122`. |
| **HIGH-5** | **CLOSED** | `17-01-PLAN.md:385-412` — per-record `fopen_s`/`fwrite`/`fclose` to `stage/psrc-census.tsv`; `:403` — `sizeof(header) - 1` (R3-01a). |
| **SR-1** | **CLOSED** | `17-03-PLAN.md:287-351` — construct-time `shader.getMaterial` / `getTextureFactor` → `m_passMaterial`. D3D9 shape `Direct3d9_StaticShaderData.cpp:593-700`; apply reference `:835-897`. |
| **SR-2** | **CLOSED** | `17-03-PLAN.md:418-461` — `writeVarByName` uses `var.StartOffset`, staging sized to `layout.TotalSize`, `updatePS(layout.BindPoint, staging, layout.TotalSize)`. `:565-570` — replaces Round 2 `updatePS(2,` grep with `updatePS(layout.BindPoint` + `var.StartOffset`; requires `updatePS(2,` == 0 in StaticShaderData. Round 2 `UNREF(var)` grep forbidden `:568`. |
| **SR-3** | **CLOSED** | `17-02-PLAN.md:294-366` — Task 0 `checkpoint:human-verify` census gate; asm → spawn `17-02B-PLAN.md` + STOP; Tasks 1–3 downstream. Mid-task stop primitive gone. |
| **LOWs** | **CLOSED** | R3-01a/b/c/d in `17-01-PLAN.md`; R3-02e/f/g in `17-02-PLAN.md`; R3-03e/h in `17-03-PLAN.md`. Wave `01→02→03` unchanged (`17-01:6-7`, `17-02:6-7`, `17-03:6-7`). |

### Strengths

- **Convergent HIGH fully addressed:** SR-2 Contract A is spelled out in sub-step 1e with staging buffer, `StartOffset`, `BindPoint`, and acceptance greps that explicitly reverse Round 2's enshrined `updatePS(2,` bug (`17-03-PLAN.md:565-570`).
- **HIGH-3 semantics match D3D9:** Per-pass zero source values + unconditional upload per reflected layout; `if (wroteAny)` explicitly grep-forbidden (`17-03-PLAN.md:575`).
- **HIGH-4 grepability fixed at the right layer:** InfoQueue dual-route mirrors verified precedent (`Direct3d11_PixelShaderProgramData.cpp:535-540`); boot acceptance targets `stage/d3d11-debug.log` (`17-02-PLAN.md:695-696`).
- **Procedural fragility removed:** SR-3 is a discrete Task 0 checkpoint, not embedded control flow (`17-02-PLAN.md:294-366`).
- **Getter home specified:** `getPerMaterialShadow()` declared in `Direct3d11_ConstantBuffer.h`, defined in `Direct3d11.cpp` (R3-03a `:259-264`, `:267-272`).
- **Classification parity:** BOM + whitespace skip in both Plan 01 census (`17-01-PLAN.md:327-342`) and Plan 02 recompile lane (`17-02-PLAN.md:465-491`).
- **Honest contingency scope:** D-09 downgrade to checkpoint guidance avoids grep-verifying unimplemented StateCache hook (`17-02-PLAN.md:23`, `:725-738`).
- **Landmines preserved:** Iter-44A/44C protected (`17-03-PLAN.md:541-542`, `:579-581`); asm re-assemble blocked; magenta tombstone kept.

### Concerns

- **MEDIUM — NEW: `must_haves.truths` stale vs R3-03c (`17-03-PLAN.md:22-24, :30`).** Frontmatter still says apply() flushes via `updatePS(2,...)`, while sub-step 1e and acceptance require `updatePS(layout.BindPoint, ...)`. Action/acceptance win, but an executor scanning `must_haves` first could mis-implement. Fold into Task 1 start: treat acceptance block `:561-576` as authoritative.

- **MEDIUM — NEW: empty reflection cache skips upload (`17-03-PLAN.md:506-510`).** If Plan 02 leaves `layouts` empty (reflection failure), apply makes no `updatePS` call — prior GPU cbuffer state may persist. Acceptable as non-fatal VS-stance defense, but record per-shader skips in `17-03-SUMMARY.md`; if char-select anchors hit this, STOP before CHAR-02/03.

- **MEDIUM — NEW: shadow mirror is conditional, staging is not (`17-03-PLAN.md:451-456`).** `shadow.material*` updates only when `writeVarByName` succeeds; failed name match leaves stale shadow while staging may be zeroed. Mitigated by R3-02e truncation WARN + Plan 02 PS-cbuffer dump cross-check; sub-step 1g cross-clobber log catches slot-2 incoherence at runtime.

- **LOW — NEW: `warnOnceOnTruncation` is process-global one-shot (`17-02-PLAN.md:436-445`).** Only the first truncated symbol is logged; subsequent truncations are silent. Acceptable for char-select; note in SUMMARY if multiple symbols truncate.

- **LOW — NEW: R3-03g HEAD/EYE detection via `strstr(shaderName, "head"/"eye")` (`17-03-PLAN.md:481-485`).** Fragile if anchor filenames differ; executor should key off Plan 02 SUMMARY anchor names, not rely solely on substring heuristic.

- **LOW — D-04 truth still partially aspirational in frontmatter (`17-03-PLAN.md:22`).** Implementation is generic (Contract A), but `must_haves` line 22 wording implies both paths hard-flush slot 2 — cosmetic only given acceptance greps.

### Suggestions

1. **At Plan 03 Task 1 start:** Patch-read `must_haves.truths` lines 22–24 mentally — implement sub-step 1e + acceptance `:561-576`, not the stale `updatePS(2,...)` apply wording. One-line frontmatter fix would eliminate ambiguity but is not execution-blocking.

2. **Plan 02 Task 2 boot gate:** Capture PS-cbuffer dump lines verbatim into `17-02-SUMMARY.md` with `BindPoint`, `TotalSize`, and `materialDiffuse`/`materialSpecular`/`materialEmissive` offsets — Plan 03 Contract A and R3-03d bind-loop decision depend on this.

3. **Plan 03 empty-layouts path:** If any char-select anchor has empty `layouts`, document and treat as compile/reflection failure — do not claim CHAR-02/03 from magenta/zero-constant draws.

4. **R3-03g proof:** If HEAD/EYE substring heuristic misses, fall back to logging the two passes with `pm.m_materialValid==1` and divergent `passDiffuse` from construct-time SR-1 data.

5. **HIGH-4 contingency:** Keep R3-02d as manual checkpoint only; if id=342/343 spike post-recompile, exercise Task 3 guidance (null `m_d3dPS` for affected shader → `selectFallbackPSForVS` at `StateCache.cpp:1111`) before opening a Phase 18 wiring plan.

### Risk Assessment

**MEDIUM** — Round 2's silent-failure modes (hardcoded slot 2, stale shadow skip, invisible reflection dump) are now specified with grep gates and live-tree precedents. Residual risk is execution-time (actual PSRC layout vs assumed variable names, linkage id=342/343, visual A/B) — appropriate for a beachhead phase with checkpoint discipline, not plan-convergence blockers.

### Verdict on Convergence

**CONVERGENCE ACHIEVED — ready for execution under normal checkpoint discipline.**

All eight Round 3 checklist items are present, grep-targeted, and aligned with the live tree. The three Round 2 consensus blockers (SR-2, HIGH-3 stale shadow, HIGH-4 log + contingency) are closed in plan action text and acceptance criteria. Remaining concerns are frontmatter wording drift and edge-case observability — address at executor checkpoints, not via Round 4 replan. Do **not** run `/gsd-execute-phase 17` until Task 0 census verdict (`17-02-PLAN.md:294-366`) confirms all-HLSL anchors or accepts the 17-02B spawn path.

---

## Consensus Summary

### Agreed Strengths

- **SR-2 Contract A** is implemented properly in `17-03-PLAN.md` sub-step 1e — staging buffer sized to `layout.TotalSize`, `writeVarByName` keyed off `var.StartOffset`, upload via `updatePS(layout.BindPoint, ...)`. Both reviewers explicitly note the Round 2 acceptance bug (`grep "updatePS(2,"` enshrined as passing) is now inverted: acceptance forbids `updatePS(2,` in StaticShaderData.cpp.
- **HIGH-3 unconditional per-pass upload** correctly mirrors D3D9 `Pass::apply` per-pass semantics; the Round 2 `if (wroteAny) updatePS(...)` skip pattern is explicitly grep-forbidden.
- **HIGH-4 dual-route via `ID3D11InfoQueue::AddApplicationMessage`** mirrors the live `emitFirstGeneratorLog` precedent at `Direct3d11_PixelShaderProgramData.cpp:535-540`, making reflection dumps visible under explorer-launched boots.
- **SR-3 task split** removes the procedural fragility — Task 0 owns the census-gate decision as a discrete `checkpoint:human-verify`, not embedded "stop after Task 1" control flow.
- **`getPerMaterialShadow()` plumbing** is now explicitly homed in `Direct3d11_ConstantBuffer.h` (declaration) + `Direct3d11.cpp` (definition), with `Direct3d11_StaticShaderData.cpp` including the header to consume.
- **D-09 contingency Option B downgrade** is honest about what's wired vs what's checkpoint guidance; `must_haves.truths` does not enshrine an unimplemented StateCache hook as a passing acceptance.
- **`//hlsl` classifier parity** between Plan 01 census (`17-01-PLAN.md:327-342`) and Plan 02 recompile lane (`17-02-PLAN.md:465-491`) — both skip leading whitespace + UTF-8 BOM before the 7-byte lowercase compare.
- **Landmines preserved:** Iter-44A depth wiring (`Direct3d11_StaticShaderData.cpp:658-677`) untouched; Iter-44C blend revert preserved; asm re-assemble blocked; magenta tombstone kept; D3D9 register folklore banned.

### Agreed Concerns (LOW — checkpoint-foldable, not Round 4 blockers)

- **`17-03-PLAN.md` frontmatter wording drift.** Both reviewers note that `must_haves.truths` lines 22-24 and the artifact block at `:40-48` still carry text from the pre-R3-03c contract — mentioning `updatePS(2,...)` for the apply path and attributing `getPerMaterialShadow` to `Direct3d11_StaticShaderData.cpp` (the call site) rather than `Direct3d11.cpp` (the definition site). The action text (sub-step 1e) and acceptance greps (`:565-570`) are correct; only the frontmatter prose is stale. Codex grades this LOW; Cursor flags the `updatePS(2,...)` wording in truths as MEDIUM because an executor scanning `must_haves` first could be misled. Both recommend the same fix: one-line frontmatter cleanup, OR explicit Task 1 start-up note "acceptance block `:561-576` is authoritative." Neither treats this as Round-4-blocking.

### Cursor-Only NEW Concerns (MEDIUM — execution-time observability)

- **Empty reflection cache skips upload.** If Plan 02's reflection-cache fill fails for a shader, Plan 03's apply() makes no `updatePS` call for that pass — prior GPU cbuffer state persists on slot b2. Codex didn't flag this; Cursor says it's an acceptable non-fatal stance but anchors the safety net at runtime: log per-shader skips in `17-03-SUMMARY.md`; if char-select anchors hit this, STOP before CHAR-02/03 verification.

- **Shadow mirror conditional, staging unconditional.** The file-scope `s_perMaterialShadow.material*` fields update only when `writeVarByName` succeeds; failed name matches leave stale shadow values while the staging buffer is zeroed for the upload. Cursor classifies this as MEDIUM because it's a subtle coherence question — mitigated by R3-02e truncation WARN + R3-03g cross-clobber flush-time log catching slot-2 incoherence at runtime.

### Divergent Views (where the reviewers split)

- **Final-verdict label:** Codex `CONVERGENCE PARTIAL`; Cursor `CONVERGENCE ACHIEVED`. The substantive conclusion is identical — both judge the plans executable, both call risk MEDIUM, both recommend `/gsd-execute-phase 17` after the Task 0 census gate. The split is about whether a single LOW frontmatter cleanup item is enough to label the overall verdict "partial" or "achieved." Operationally, treat as **CONVERGENCE ACHIEVED — one frontmatter touch-up recommended before execution.**

- **`must_haves.truths` `updatePS(2,...)` wording severity:** Cursor MEDIUM (executor-mislead risk); Codex implicit LOW (artifact metadata, not truths body). The wording lives in two places — Cursor flagged the truths block at `:22-24`; codex flagged the artifact block at `:40-48`. Both should be cleaned up; both reviewers explicitly note this does not gate execution.

### Recommended Next Action

1. **One-line frontmatter cleanup in `17-03-PLAN.md`** (both reviewers concur): adjust `must_haves.truths:22-24` to say `updatePS(layout.BindPoint, staging.data(), layout.TotalSize)` (matching sub-step 1e); adjust artifact-block at `:40-48` so `Direct3d11_StaticShaderData.cpp` says it CALLS `getPerMaterialShadow` while the definition target remains `Direct3d11.cpp`. **This is a docs-only touch — no plan logic change.**

2. **`/gsd-execute-phase 17`** — the Task 0 census-gate `checkpoint:human-verify` (Plan 02 line 294-366) is the first executor interaction point after Plan 01 ships. The asm-in-anchors verdict gates whether `17-02B-PLAN.md` spawns or whether Plan 02 proceeds straight to the HLSL recompile lane.

3. **At Plan 03 execution start** (per Cursor suggestion 1): treat acceptance block `17-03-PLAN.md:561-576` as authoritative over any stale `must_haves.truths` wording.

4. **At Plan 02 Task 2 boot gate** (per Cursor suggestion 2): capture PS-cbuffer dump lines verbatim into `17-02-SUMMARY.md` with `BindPoint`, `TotalSize`, and `materialDiffuse`/`Specular`/`Emissive` offsets — Plan 03's Contract A and the R3-03d StateCache bind-loop decision depend on this real data.

---

# Round 4 — Gap-Closure Wave (Plans 17-05 / 17-06 / 17-07)

Same two reviewers as Rounds 1–3 (Codex via `codex exec --sandbox read-only` and Cursor via `cursor-agent -p --mode ask --trust`). Both read the gap-closure plans authored after Phase 17 closed as INFRASTRUCTURE COMPLETE (commit `41d7f2fec`). Cursor additionally cross-read live tree state at `Direct3d11_StaticShaderData.cpp:794-896`, `Direct3d11_PixelShaderProgramData.cpp:469-1137`, `Direct3d11_HlslRewrite.cpp:747-767`, `Direct3d11_StateCache.cpp:1121-1137`, and the captured PSRC dump at `evidence/plan-17-04x-psrc-source-dump.txt`.

Prompt: `.planning/tmp/gsd-review-prompt-17.md` (~134 KB, 1040 lines). Codex review: ~11 KB; Cursor review: ~19 KB.

## Codex Review (Round 4)

### Plan 17-05

**Summary:** Solid verification plan and the right antidote to the Iter-44B false-claim problem. Its main weakness is orchestration: it is both a first plan and a final verification plan, so it parks mid-flight while 17-06/17-07 happen. That is acceptable only if the blocked state is explicit in the phase tracker.

**Strengths**
- Correctly separates PRE-gap evidence from POST-gap evidence.
- Keeps D3D9 as the immutable reference and requires `rasterMajor=5` re-boot after code changes.
- Does not let log greps substitute for visual parity.
- Good use of verifier-produced `17-VERIFICATION.md` instead of mutating the synthesized stand-in quietly.

**Concerns**
- **MEDIUM:** Filename contract is inconsistent. Roadmap says pair convention `char_default_d3d11_0003.png`, but artifacts use `char_default_d3d11_0003_preGap.png` and `_postGap.png`. Fine for cycle evidence, but future harness compatibility should define aliases or explicitly bless suffixes.
- **MEDIUM:** "D3D9 baseline is invariant" is mostly true, but 17-06/17-07 modify the D3D11 plugin only by intent, not by proof until after execution. Task 4 re-boots D3D9, but not re-screenshoting means subtle config/camera/user-character drift will not be caught.
- **LOW:** `char_default_d3d9_0003.png` existence is treated as proof of clean D3D9 boot. It proves the client rendered once, not that the boot log was clean.
- **LOW:** "No SUMMARY.md required" conflicts with the surrounding GSD habit and with 17-06/17-07 expecting summaries. Not fatal, but audit discoverability suffers.

**Suggestions**
- Add a stable canonical pair entry in `17-VERIFICATION.md`: "canonical comparison = D3D9 `char_default_d3d9_0003.png` vs D3D11 `char_default_d3d11_0003_postGap.png`."
- In Task 4, require a fresh D3D9 screenshot only if the character/camera/default pose changed or if the post-17-07 D3D9 boot lands on a different visible state.
- Record image dimensions, timestamp, renderer, commit SHA, and gl DLL timestamp in the evidence README or verification table.
- Add a brief `17-05-SUMMARY.md` or a one-line pointer file if the GSD index expects one.

**Risk Assessment:** **LOW-MEDIUM.** The plan is mostly documentation/evidence, but it creates a long-lived parked plan state and depends heavily on disciplined human capture.

### Plan 17-06

**Summary:** The goal is valid, but the technical path is under-specified and possibly aimed at the wrong target. The live code's `writeVarByName` only matches whole reflected variable names and writes full `float4`s, so "packedRegisterN.{xyzw} channel" is not implementable by adding candidate strings alone. This plan needs a discovery/fix split or a concrete offset-write design before execution.

**Strengths**
- Correctly rejects the falsified `material[0/1/2]` hypothesis.
- Keeps scope plugin-local in `Direct3d11_StaticShaderData.cpp:794`.
- Preserves `materialSpecularColor` and the existing diagnostic dump.
- Good insistence on deriving mapping from source or boot evidence rather than guessing.

**Concerns**
- **HIGH:** `writeVarByName("packedRegisterN.x", ...)` will not work unless D3DReflect exposes that exact dotted name. Current code compares exact `var.Name` strings and writes a full `XMFLOAT4` at `StartOffset`; it has no component/channel writer. See `Direct3d11_StaticShaderData.cpp:794`.
- **HIGH:** Writing diffuse/emissive into a whole `packedRegisterN` risks stomping sibling channels. The plan says "single-candidate-per-channel," but the implementation primitive is whole-register, not per-channel.
- **HIGH:** The plan assumes `userConstants` inner reflection can reveal semantic names. If `userConstants` is an array of 17 float4s, reflection may expose only array elements or a single opaque variable, not meaningful "diffuse/emissive" fields.
- **MEDIUM:** The source comment already says char-select PS programs may not directly consume `materialDiffuse`/`materialEmissive`; diffuse may come through `textureFactor` or texture sampling. Forcing the metric to `wroteDiffuse=1 && wroteEmissive=1` may be a false target if the shader never declares those values.
- **MEDIUM:** Path B is a plan-within-a-plan requiring an extra commit, extra boot, and then a mapping commit. That is too much branching for one execute plan.

**Suggestions**
- Split 17-06 into `17-06a discovery` and `17-06b mapping/write`.
- Add a new explicit helper if packed channels are real: `writeVarComponent("packedRegister0", componentIndex, valueComponent)` or `writeVarFloat4AtOffset(baseName, value, mask)`.
- Change success wording from "wroteDiffuse/emissive must land" to "the shader inputs actually consumed by sul_eye/head receive the required material values," then list the concrete reflected variables after discovery.
- Treat `textureFactor` / `textureFactor2` as first-class candidates if source inspection says they drive clothing/eye tint.

**Risk Assessment:** **HIGH.** The target is valid, but the proposed candidate-chain mechanism does not match the current reflection/write primitive.

### Plan 17-07

**Summary:** This plan identifies the right root cause, but the proposed implementation is much riskier than the plan admits. A per-(VS, PS) rewritten asset shader cannot realistically be integrated by touching only `Direct3d11_PixelShaderProgramData.cpp`. The bind decision lives in `Direct3d11_StateCache.cpp:1123`, so the `files_modified` contract is probably false.

**Strengths**
- Keeps `isCompatibleWithVS` and `selectFallbackPSForVS` as safety rails.
- Chooses the lower-blast-radius axis in principle: PS-side adaptation instead of VS-output reordering.
- Correctly recognizes that ctor-time compile does not know the bound VS.
- Good measurable metric: asset PS bind rate from 0/9 to >=8/9 compatible.

**Concerns**
- **HIGH:** The single-file scope is not credible. StateCache currently binds asset PS only if `isCompatibleWithVS(...)` succeeds, else falls back. A rewritten PS must be requested and bound in that path, which likely requires `Direct3d11_StateCache.cpp`.
- **HIGH:** Avoiding `Direct3d11_PixelShaderProgramData.h` may be over-constrained. A clean API like `getOrCompileAssetPSForVS(vsData)` probably belongs on `Direct3d11_PixelShaderProgramData`; file-scope static maps keyed by `this` are a workaround with lifetime/cache risks.
- **HIGH:** Parsing HLSL structs with ad hoc text scanning is fragile. Comments, macros, typedefs, includes, nested structs, multiple entry points, semantic casing, arrays, and `half`/`fixed`/legacy types can break this. "Return input unchanged" is safe, but may leave the plan failing 0/9.
- **MEDIUM:** Reordering the struct fields may not force register assignment if semantics include system values or explicit interpolation modifiers. It likely helps for COLOR/TEXCOORD, but the plan should validate reflected input after compile before claiming success.
- **MEDIUM:** Shader cache key handling is underdeveloped. `tryCompilePixelShaderFromHlslNoFatal` currently hashes source+defines before rewrite. Per-VS rewritten source must include a VS signature salt or the cache can return the wrong blob.
- **MEDIUM:** Success log wording can inflate counts. Logging "Plan 17-07 ... COMPATIBLE" must only happen after D3DReflect on the rewritten PS and a real `isCompatibleWithVS` pass, not merely after compile.

**Suggestions**
- Update `files_modified` up front to include `Direct3d11_StateCache.cpp` and possibly `Direct3d11_PixelShaderProgramData.h`, or explicitly split "prototype hack" from "clean integration."
- Prefer a narrow public method despite the ABI concern, with mandatory full plugin rebuild: `ID3D11PixelShader *getOrCompileRewrittenAssetPSForVS(Direct3d11_VertexShaderData const *vsData);`
- Include VS-output-signature hash in the rewritten source or cache key before `Direct3d11_ShaderCache::hashSource`.
- Add a compile-time postcondition: reflect rewritten PS inputs, run `isCompatibleWithVS`, only cache/bind if compatible.
- Consider a smaller first fix: if every failing pair is only `COLOR0 v0/o1`, test a targeted COLOR0/TEXCOORD declaration-order rewrite before building a general HLSL parser.

**Risk Assessment:** **HIGH.** Axis-(b) is lower risk than VS reordering conceptually, but this concrete plan underestimates integration and parser risk.

### Codex Cross-Plan Assessment (Round 4)

Together, the plans address the right three gaps on paper: 17-05 closes evidence, 17-06 targets material constant upload, and 17-07 targets asset PS bind compatibility. They do **not yet guarantee** CHAR-01/02/03 PASS. They can support PASS only if the POST-gap screenshots show visual parity and the logs prove the asset PS lane, not fallback PS, is carrying the relevant draws.

The wave ordering is mostly correct: PRE capture → code fixes → POST capture → verification authoring. There is no fatal cycle, but 17-05 depends on 17-06/17-07 to finish while 17-06/17-07 depend on 17-05 PRE evidence. The tracker must represent 17-05 as "parked after Task 3," not failed or incomplete.

The two human checkpoints in 17-05 are acceptable for this repo because only Kenny's host has assets. The risk is stale executor state. Mitigate by requiring commit SHAs and `stage/gl11_d.dll` timestamps in both resume signals.

17-06's Path A/B branch should be split. Discovery that may require code instrumentation, boot, evidence ingestion, and then a second implementation commit is not a clean single execute task.

17-07 violates its `files_modified` contract unless execution proves a very clever wrapper path. The honest plan should include `Direct3d11_StateCache.cpp` as likely modified. If a header method is needed, allow it and explicitly require a full plugin rebuild rather than contorting around the ABI trap.

The success metrics are necessary but not sufficient. `>=8/9 COMPATIBLE`, `wroteDiffuse=1`, `wroteEmissive=1`, and three PNGs prove progress. Phase 17 closure still needs: matched visual parity, no magenta/fallback dominance on required surfaces, D3D9 and D3D11 boot confirmation, and an explicit statement for any residual nonmatching pair.

Landmine handling is generally good, but there are inconsistencies: 17-07 says "single .cpp only" while its own Task 1 admits `StateCache.cpp` may be touched; 17-06 says "channel mapping" while the current write helper cannot write channels; 17-05 says D3D9 invariant but does not capture a post-fix D3D9 image unless needed.

**Bottom line (Codex):** 17-05 is strong, 17-06 needs redesign around actual reflected variable/write mechanics, and 17-07 needs a more honest integration scope. I would not execute 17-06/17-07 as written without tightening those two contracts first.

---

## Cursor Review (Round 4)

Review grounded in the plan text plus live tree state (`Direct3d11_StaticShaderData.cpp`, `Direct3d11_PixelShaderProgramData.cpp`, `Direct3d11_HlslRewrite.cpp`, `Direct3d11_StateCache.cpp`, `evidence/plan-17-04x-psrc-source-dump.txt`).

### Plan 17-05 — A/B Verify (GAP-1)

**Summary:** Well-structured verification plan that correctly treats missing screenshots as the blocking audit gap and enforces Iter-44B discipline. The PRE/post capture split is sound. The main weaknesses are dependency metadata that can deadlock Wave 6, weak capture reproducibility spec, and an implicit assumption that PRE-gap D3D11 will look dramatically broken when fallback PS may already produce partial texture.

**Strengths**
- Correctly centers GAP-1 as the formal close blocker; no grep-only PASS claims.
- PRE-gap + POST-gap + invariant D3D9 baseline is the right experimental design.
- Task 3 handoff explicitly parks 17-05 while 17-06/17-07 run — good orchestration intent.
- No source edits; D3D9 path untouched; ABI trap avoided.
- Task 5 acceptance criteria are concrete (frontmatter, PNG citations, grep counts).
- Explicit PARTIAL/DEFERRED escape hatch if POST-gap still fails — honest.

**Concerns**

| Severity | Issue |
|----------|-------|
| **HIGH** | **`depends_on: ["17-05"]` on 17-06/17-07 conflicts with 17-05 parking at Task 3.** If the orchestrator treats `depends_on: ["17-05"]` as "plan complete," you get a cycle: 17-05 waits on 17-06/17-07 at Task 4; 17-06/17-07 wait on 17-05 complete. Frontmatter should be `depends_on: ["17-04", "17-05-task3"]` or wave-only ordering. |
| **MEDIUM** | **No capture reproducibility contract.** Tasks 2/4 say "char-select default pose" and ScreenShotHelper, but don't pin character slot, camera, UI state, resolution, or whether screenshots are full-frame vs cropped. Phase 12 `COMPARISON.md` pairs are useless for pixel diff if framing drifts between D3D9 and D3D11 boots. |
| **MEDIUM** | **Naming drift from ROADMAP.** Success criterion #5 references `char_default_d3d11_0003.png`; plan uses `_preGap` / `_postGap`. Fine internally, but Phase 20 harness needs an explicit alias rule or it will miss files. |
| **MEDIUM** | **PRE-gap visual narrative may be wrong.** Current bind path uses `selectFallbackPSForVS` → Variant T (textured passthrough when TEXCOORD0 exists). PRE-gap may show textures but wrong lighting/head composite — not necessarily magenta/untextured. Task 5 prose should distinguish "fallback-PS partial parity" vs "asset-PS parity." |
| **LOW** | **Task 1 boot-gate greps omit `wroteEmissive=1` and `wroteSpecular=1` in Task 2** (only 5 greps). Task 4 adds them — asymmetric baseline table. |
| **LOW** | **`verified_by: pending /gsd:verify-work 17` after Task 5** — doc is verifier-produced but still not independently verified; milestone audit may over-read "closure." |

**Suggestions**
- Fix dependency wording in 17-06/17-07 frontmatter: `depends_on: ["17-04", "17-05-pre-gap-evidence"]` — "Runnable after 17-05 Task 3 commit; does NOT require 17-05 Task 5."
- Add to `evidence/README.md`: "Same character slot, default camera, no UI panels obscuring face, 1920×1080 (or document actual), full client framebuffer."
- Task 5 must require stating **which PS lane** delivered POST-gap visuals (grep `Plan 17-07 .* COMPATIBLE` vs fallback-only).
- Add symlink or copy step: `char_default_d3d11_0003.png` → `char_default_d3d11_0003_postGap.png` for harness compatibility.

**Risk Assessment: MEDIUM** — Process/orchestration risk (dependency deadlock, capture variance) exceeds technical risk; plan is otherwise appropriate for GAP-1.

### Plan 17-06 — packedRegister / Diffuse+Emissive (GAP-2)

**Summary:** Thorough discovery-first framing, but **likely mis-targeted for char-select visual parity**. Live code and PSRC dumps already show char-select shaders consume `textureFactor` / `textureFactor2` and **interpolated `COLOR0` (`vertexDiffuse`)**, not cbuffer `materialDiffuse` / `materialEmissive`. Rule D in `Direct3d11_HlslRewrite.cpp` preserves original declaration names — it does **not** contain a `c[N] → packedRegister[K].channel` table, so Path A is a dead end. Closing GAP-2 as defined (`wroteDiffuse=1` + `wroteEmissive=1`) may satisfy metrics without moving CHAR-01/02/03.

**Strengths**
- Correctly falsifies `material[0/1/2]` hypothesis; aligns with discovery dump.
- Plugin-local scope (`Direct3d11_StaticShaderData.cpp` only) — no ABI/header trap.
- Path B (extend discovery dump for `userConstants` inner fields) is the right fallback when Rule D is opaque.
- Discovery dump self-suppression on `wrote(D,S,E)=(1,1,1)` is a good failure signal.
- Preserves `materialSpecularColor` (04ef976) — don't regress specular.

**Concerns**

| Severity | Issue |
|----------|-------|
| **HIGH** | **GAP-2 may not be on the critical path for char-select.** Comments at `887:896:Direct3d11_StaticShaderData.cpp` state diffuse comes from texture sampler + `textureFactor.rgb`; emissive lives in `packedRegisterN` / `userConstants` engine state; `material*` lookups "will silently miss for char-select shaders." PSRC confirms `h_simple_pp_ps20.psh` uses `textureFactor.rgb`, not material diffuse cbuffer. |
| **HIGH** | **Rule D does not document the mapping Path A needs.** `747:767:Direct3d11_HlslRewrite.cpp` only wraps `: register(cN)` → `packoffset(cN)` preserving names. `packedRegister0..4` in reflection means the **input** `.inc` declares those names — not that Rule D maps diffuse/emissive into them. Path A will almost certainly fall through to Path B every time. |
| **HIGH** | **`writeVarByName` cannot write sub-channels.** Lambda at `794:812:Direct3d11_StaticShaderData.cpp` does exact `strcmp` on top-level names and writes a full `XMFLOAT4`. Plan text references `packedRegisterN.{xyzw}` but implementation only supports whole-variable names like `"packedRegister2"`. |
| **MEDIUM** | **Conflicts with "single-candidate-per-channel" comment at lines 863–866** while Task 1 adds `||` chains — same tension 17-04 already had. Wrong first-hit candidate could write garbage to an unrelated packed slot. |
| **MEDIUM** | **Path B should be the primary plan, not a branch.** Two-commit + extra Kenny boot adds latency; consider splitting `17-06a` (dump extension) / `17-06b` (mapping) upfront. |
| **MEDIUM** | **Success metric `wroteDiffuse=1` conflates "write landed" with "visual fix."** `vertexDiffuse` lighting uses VS→PS interpolator (GAP-3), not cbuffer diffuse upload. |
| **LOW** | **`textureFactor` / `textureFactor2` already hit** but are `(void)`-discarded in logging — no visibility in 17-05 grep set despite being char-select-relevant. |

**Suggestions**
- Re-scope 17-06 objective: "Close GAP-2 **instrumentation completeness** (D/S/E flags + dump suppression). Visual CHAR impact expected **low** for char-select; primary visual driver is GAP-3 + existing `textureFactor` path."
- Start with Path B; treat Path A as "confirm Rule D preserves names" only.
- If mapping lives in `userConstants`, verify D3DReflect exposes dotted paths — arrays of 17×float4 may not reflect inner semantics; may need **engine-side slot index** from D3D9 upload path, not PS reflection alone.
- Extend `writeVarByName` or add offset write helper if mapping is sub-vector within `packedRegisterN`.
- Add Task 1 acceptance: document PSRC consumption per anchor (`h_simple` → `textureFactor`; `h_color2_specmap` → `materialSpecularColor` + `textureFactor`) so 17-05 Task 5 doesn't attribute visual delta to the wrong gap.

**Risk Assessment: MEDIUM–HIGH** — Low regression risk (plugin-local), but **high risk of false closure**: metrics pass, visuals unchanged, effort spent on non-critical path.

### Plan 17-07 — PS Input-Signature Rewrite (GAP-3)

**Summary:** Correctly identifies the beachhead-critical gap (0/9 asset-PS binds). Axis-(b) is directionally right vs disturbing `buildHlslForVSOutputs`. But the plan **underestimates PSRC syntax** (parameter-list `main()`, not struct inputs), **contradicts "`isCompatibleWithVS` unchanged"** with per-VS rewrite needs, and **will require bind-path changes** in practice (`Direct3d11_StateCache.cpp` or a new resolver API). This is the highest-complexity plan in the wave and the one most likely to slip schedule or pass grep metrics without visual parity.

**Strengths**
- Targets the actual blocker: register-position mismatch (`COLOR0` at v0 vs o1).
- Axis-(b) scoped to PS recompile lane — avoids Phase 11 VS-output reorder regressions.
- Preserves `selectFallbackPSForVS` safety net — correct.
- `buildHlslForVSOutputs` at `469:492:Direct3d11_PixelShaderProgramData.cpp` is the right template for sort-by-register discipline.
- Per-VS lazy recompile (b.1) matches `getOrCompilePSForVS` precedent.
- Non-fatal degrade path via `tryCompilePixelShaderFromHlslNoFatal` is appropriate.
- No header touch — ABI trap avoided.

**Concerns**

| Severity | Issue |
|----------|-------|
| **HIGH** | **Parser targets struct inputs; char-select PSRC uses parameter lists.** Evidence: `h_simple_pp_ps20.psh`, `h_color2_specmap_cbmp_ps20.psh` use `float4 main(in float3 vertexDiffuse : COLOR0, in float2 tcs_MAIN : TEXCOORD0, ...)`. Task 1 step 1a ("struct bound to `main` parameter type") will no-op or fail on dominant char-select shaders. Rewriter must reorder **`main()` parameter declarations**, not struct fields. |
| **HIGH** | **`isCompatibleWithVS` unchanged vs per-VS rewritten PS is a logical contradiction.** Validator reads `ps->getReflectedPSInputs()` from **ctor-time** compile (`1060:1081:Direct3d11_PixelShaderProgramData.cpp`). Rewritten per-VS PS has different input registers but cached `m_reflectedPSInputs` stays wrong → validator still INCOMPATIBLE unless you also cache per-VS reflection OR bypass the gate. Acceptance criterion "`isCompatibleWithVS` definition unchanged" fights the fix. |
| **HIGH** | **Bind integration requires StateCache or new resolver — not optional.** Current path at `1121:1137:Direct3d11_StateCache.cpp`: asset PS binds only if `getPixelShader()` && `isCompatibleWithVS`. Step 4's "lookup rewrite cache before fallback" **must** change this call site or replace it with e.g. `resolveAssetPSForVS(vs, ps)`. `files_modified: [Direct3d11_PixelShaderProgramData.cpp]` understates scope. |
| **MEDIUM** | **Cache key must include VS signature hash.** `tryCompilePixelShaderFromHlslNoFatal` hashes source only (`306:307`). Per-VS rewrite without hash salt → wrong PS served across VS pairs. |
| **MEDIUM** | **`>= 8/9 COMPATIBLE` via new log lines ≠ asset PS actually bound.** Plan can emit `Plan 17-07 ... COMPATIBLE` from rewrite path while StateCache still binds fallback if wiring is wrong. Need bind-path assertion (e.g. log `PSSetShader asset-PS ptr=...` vs fallback ptr). |
| **MEDIUM** | **HLSL parameter reorder is fragile.** Macros, `#if`, multi-line params, `inout`, legacy `ps_1_x` profiles — string rewrite can break compile. Plan mentions non-fatal fallback but not a census of parse failure rate. |
| **MEDIUM** | **Axis-(b) "lower risk" is only true vs axis-(a) — not vs complexity baseline.** Per-(VS,PS) lazy recompile + parse + cache + bind routing is a large single-task change for one `.cpp` file. |
| **LOW** | **Log grep contract:** Task 4 expects `Plan 17-07 .* COMPATIBLE vs=` but existing compat logs use `Plan 17-04 Task 1 VS<->PS pair compatibility: COMPATIBLE`. Plan accounts for this — ensure executor doesn't double-count. |

**Suggestions**
- Split Task 1 into explicit subtasks:
  1. `rewriteMainParameterOrderForVSOutputs()` — handle parameter-list PSRC (primary).
  2. Optional struct-path for rare assets.
  3. `resolvePixelShaderForVS(vs, ps)` returning `{assetRewritten | assetNative | fallback}`.
  4. StateCache one-line swap to call resolver (add to `files_modified` upfront).
- Change acceptance criteria from "`isCompatibleWithVS` unchanged" to: "`isCompatibleWithVS` unchanged for **native** ctor compile; rewritten PS validated via **per-VS reflected inputs** before bind."
- Add boot metric: `asset-PS bound=` count distinct from `COMPATIBLE` log count.
- Spike: manually reorder one PSRC (`h_simple_pp_ps20.psh`) parameter list to match VS register order, recompile, confirm `isCompatibleWithVS` true — de-risk before full cache impl.

**Risk Assessment: HIGH** — Beachhead-critical, but implementation plan has unresolved architecture (bind path, reflection cache, PSRC parse shape) that can yield grep-green / visual-red outcomes.

### Cursor Cross-Plan Assessment (Round 4)

**Do 17-05 + 17-06 + 17-07 close GAP-1/2/3 and enable CHAR PASS?**

| Gap | Likely closed? | CHAR impact |
|-----|----------------|-------------|
| **GAP-1** | **Yes**, if Kenny captures and Task 5 is honest | Enables formal verdicts; doesn't fix visuals |
| **GAP-2** | **Maybe** (metrics only) | **Weak** for char-select per live code + PSRC — `textureFactor` already works; eyes/head depend on interpolators + asset PS |
| **GAP-3** | **Conditional** — plan is necessary but current spec may fail on PSRC syntax / bind wiring | **Primary** driver for CHAR-01/03; CHAR-02 (eyes) likely needs GAP-3 + correct SRV/stage composite, not emissive cbuffer |

**Verdict:** Together they can move Phase 17 from NOT-VERIFIED → evidence-backed PARTIAL/PASS, but **automatic PASS on all CHAR-0x is not guaranteed** even if all grep metrics hit. Most likely outcome: **CHAR-01 PARTIAL→PASS if GAP-3 lands; CHAR-02/03 PARTIAL** until head multi-stage and eye palette are visually confirmed in A/B narrative.

**Wave ordering** is correct in intent; the hidden cycle is the `depends_on: ["17-05"]` metadata. 17-05 Task 4 also requires the **combined** gl11_d.dll from 17-06 AND 17-07 — parallel commits need explicit merge/build instruction ("deploy gl11_d.dll built from commit containing both").

**Two-checkpoint structure in 17-05:** Acceptable but fragile. Executor blocked twice on Kenny is expected for this repo (no TRE in CI worktree). Mitigations: time-box checkpoints; allow Task 5 draft after Task 2 with DEFERRED CHAR verdicts updated after Task 4; don't block 17-06/17-07 on Task 4.

**17-06 Path A vs Path B — split plans?** Path B should be default; recommend explicit split into `17-06a` (Discovery dump extension + Kenny boot) and `17-06b` (Mapping commit after evidence).

**17-07 cache options + StateCache drift:** Yes, violates stated `files_modified` contract in practice. Recommend updating 17-07 frontmatter to: `files_modified: [Direct3d11_PixelShaderProgramData.cpp, Direct3d11_StateCache.cpp]`.

**Success metrics vs Iter-44B discipline:**

| Metric | Sufficient for phase close? |
|--------|------------------------------|
| 3 PNGs + written diff | **Necessary** for Iter-44B — good |
| `wroteDiffuse=1` + `wroteEmissive=1` | **Insufficient alone** — may be irrelevant for char-select |
| `>= 8/9 Plan 17-07 COMPATIBLE` | **Insufficient alone** — must prove asset PS bound, not just logged compat after rewrite |
| D3D9 boot clean | **Necessary** D-06 — good |

For PASS (not PARTIAL): Task 5 must show POST-gap D3D11 **visually matches** D3D9 on four axes **and** attribute which fixes drove delta (GAP-3 >> GAP-2 for char-select).

**Is axis-(b) genuinely lower-risk than axis-(a)?** Yes, **relative to axis-(a)** — VS reorder touches settled Iter-3 fallback synthesis and broad VS lane. No, **in absolute terms** — parameter-list HLSL rewrite + per-VS cache + bind-path integration is still a large, brittle change. Axis-(b) is the right choice, but not "low risk"; it's "lower risk than the alternative."

**Bottom line (Cursor):**
- **17-05:** Solid verification scaffold; fix `depends_on` deadlock and capture reproducibility before execute.
- **17-06:** Well-written discovery plan targeting a gap that **may not drive char-select visuals**; demote to instrumentation/completeness or re-scope before investing Path B boots.
- **17-07:** Correct strategic target (asset-PS bind rate), but **under-specified on PSRC syntax, bind-path integration, and reflection caching** — highest execution risk in the wave.

Recommended execution priority: Fix 17-05 metadata → **17-07 (with revised parser + StateCache/resolver scope)** → 17-05 POST capture → 17-06 in parallel or after if time-boxed (metrics hygiene, not beachhead-critical).

Expected phase outcome if plans run as written without revisions: GAP-1 closed with evidence; GAP-2 closed on greps with uncertain visual impact; GAP-3 **at risk**; Phase 17 closes as **PARTIAL** with honest A/B, not blanket CHAR PASS.

---

## Round 4 Consensus

Both reviewers independently converge on the same architectural concerns. Where the two diverge is mostly tone and table-vs-prose presentation — there are no substantive disagreements on the HIGH-severity items.

### Agreed Strengths (raised by both)

- **17-05:** PRE/POST capture design is correct; honoring Iter-44B no-false-pre-claim is enforced; PARTIAL/DEFERRED escape hatch is appropriate.
- **17-06:** Falsifies the `material[0/1/2]` hypothesis correctly; plugin-local scope avoids ABI trap; preserves `materialSpecularColor` (04ef976).
- **17-07:** Targets the correct root cause (register-position mismatch); axis-(b) is the right strategic direction over axis-(a); preserves `selectFallbackPSForVS` safety net; uses `tryCompilePixelShaderFromHlslNoFatal` non-fatal degrade path.

### Agreed HIGH-Severity Concerns (raised by both)

1. **`writeVarByName` cannot write sub-channels (HIGH × 2 — both reviewers).** Lambda at `Direct3d11_StaticShaderData.cpp:794` compares whole `var.Name` strings and writes a full `XMFLOAT4` at `StartOffset`. Plan 17-06's text references `packedRegisterN.{xyzw}` channel writes but the implementation primitive does not exist. The plan needs either (a) a new `writeVarComponent` / `writeVarFloat4AtOffset` helper added before the candidate-chain extension, or (b) a rescope that writes only whole reflected variables.

2. **17-07 `files_modified` contract is dishonest (HIGH × 2 — both reviewers).** The bind decision lives in `Direct3d11_StateCache.cpp:1121-1137` — the rewritten-PS lookup MUST happen at or below that call site. The single-file scope (`Direct3d11_PixelShaderProgramData.cpp` only) is not credible. `Direct3d11_StateCache.cpp` should be in `files_modified` upfront, and `Direct3d11_PixelShaderProgramData.h` should be allowed (with explicit ABI rebuild discipline) rather than contorting around it with `this`-keyed file-scope-static maps.

3. **GAP-2 closure may not move char-select visuals (HIGH × 2 — both reviewers).** Live code comments + the captured PSRC dump both show that char-select shaders (`h_simple_pp_ps20.psh`, `h_color2_specmap_cbmp_ps20.psh`) consume `textureFactor.rgb` and interpolated `COLOR0` (`vertexDiffuse`), NOT cbuffer `materialDiffuse` / `materialEmissive`. Closing GAP-2 as defined (`wroteDiffuse=1` + `wroteEmissive=1`) may satisfy metrics without moving CHAR-01/02/03. The plan's success metric conflates "write landed" with "visual fix."

4. **17-07 PSRC syntax mismatch (HIGH — Cursor; corroborated by Codex's parser-fragility HIGH).** Plan 17-07 Task 1 step 1a targets "the struct bound to `main` parameter type" — but the captured PSRC dump shows the dominant char-select form is `float4 main(in float3 vertexDiffuse : COLOR0, in float2 tcs_MAIN : TEXCOORD0, ...)` (parameter-list, not struct). The rewriter must reorder **`main()` parameter declarations**, not struct fields, on the dominant char-select asset set. Codex's "ad hoc text scanning is fragile" concern is the same risk; recommends a targeted COLOR0/TEXCOORD-only spike first.

5. **17-06 Path A is a dead end (HIGH — Cursor; corroborated by Codex's "Path B is a plan-within-a-plan" MEDIUM).** Rule D at `Direct3d11_HlslRewrite.cpp:747-767` only wraps `: register(cN)` → `packoffset(cN)` preserving original declaration names — it does NOT contain a `c[N] → packedRegister[K].channel` mapping table. The `packedRegister0..4` reflection names mean the **input** `.inc` declares them that way; Path A will always fall through to Path B. Recommendation: split into 17-06a / 17-06b.

6. **17-07 `isCompatibleWithVS` unchanged contradicts per-VS rewrite (HIGH — Cursor; corroborated by Codex's MEDIUM postcondition concern).** The validator reads `m_reflectedPSInputs` from ctor-time compile (`Direct3d11_PixelShaderProgramData.cpp:1060-1081`). A rewritten per-VS PS has different input registers, but the cached `m_reflectedPSInputs` stays from the ctor-time reflection — so the validator still returns INCOMPATIBLE unless per-VS reflection is also cached OR the validator is bypassed for the rewritten lane. The acceptance criterion `"isCompatibleWithVS definition unchanged"` fights the fix.

### Agreed MEDIUM-Severity Concerns (raised by both)

- **17-05 dependency metadata creates a cycle.** `depends_on: ["17-05"]` on 17-06/17-07 conflicts with 17-05 parking at Task 3. Cursor escalates this to HIGH; Codex calls it acceptable IF the tracker represents "parked after Task 3" correctly. Both agree the metadata should be amended: `depends_on: ["17-04", "17-05-task3"]` or wave-only ordering.

- **17-06 should be split.** Both recommend `17-06a discovery` + `17-06b mapping` instead of a single conditional Task 2.

- **17-05 capture reproducibility under-specified.** No pin for character slot, camera, UI state, resolution. Pixel diff is useless if framing drifts between D3D9 and D3D11 boots.

- **17-07 cache key needs VS-signature-hash salt.** `tryCompilePixelShaderFromHlslNoFatal` hashes source-only — per-VS rewrite without salt → wrong PS served across VS pairs.

- **17-07 success metric inflation risk.** `>= 8/9 Plan 17-07 COMPATIBLE` log lines ≠ asset PS actually bound. Need a bind-path assertion (e.g. log `PSSetShader asset-PS ptr=...` vs fallback ptr) distinct from the rewrite log.

- **Naming drift from ROADMAP.** Plan uses `_preGap` / `_postGap` suffixes; ROADMAP success criterion #5 references `char_default_d3d11_0003.png`. Need explicit alias rule for Phase 20 harness.

- **17-05 Task 2 grep set is asymmetric** vs Task 4 (Task 2 omits `wroteEmissive=1` + `wroteSpecular=1`).

### Divergent Views (where the reviewers split)

- **17-05 overall risk:** Codex LOW-MEDIUM, Cursor MEDIUM. The substantive concerns are nearly identical; Cursor weights the orchestration cycle higher.
- **17-06 overall risk:** Codex HIGH, Cursor MEDIUM-HIGH. Both flag the same HIGH concerns; Cursor adds the "may not move CHAR visuals" framing as an explicit risk.
- **17-07 overall risk:** Both HIGH. No divergence.
- **17-07 wrapper-approach feasibility:** Codex says the wrapper approach (keeping StateCache untouched) is implausible — "single-file scope is not credible." Cursor says the wrapper-only approach **without** StateCache change requires a new bind resolver called from `applyPreDrawState`, which is still a second file unless function-pointer injection (none documented) — substantively the same conclusion.

### Round 4 Verdict

**CONVERGENCE NOT ACHIEVED — gap-closure plans need amendment before execution.**

The three gap-closure plans should NOT be executed as written. Required amendments before `/gsd:execute-phase 17 --gaps`:

1. **17-05 frontmatter:** change 17-06/17-07 `depends_on` from `["17-04", "17-05"]` to `["17-04", "17-05-task3"]` (or equivalent post-Task-3 partial dependency); add capture-reproducibility contract to `evidence/README.md`; add Task 4 step requiring `asset-PS bound=` vs fallback-PS attribution in 17-VERIFICATION.md Task 5.
2. **17-06 rescope:** either (a) split into `17-06a discovery` + `17-06b mapping/write`, defaulting to Path B (Rule D documented to NOT contain the mapping); OR (b) demote the plan to "instrumentation completeness — close GAP-2 metrics; visual char-select impact expected LOW" and explicitly call out that GAP-3 + existing `textureFactor` path is the primary visual driver. Add a `writeVarComponent` / `writeVarFloat4AtOffset` helper before the candidate-chain extension if sub-channel writes are genuinely required.
3. **17-07 rewrite:** add `Direct3d11_StateCache.cpp` (and possibly `Direct3d11_PixelShaderProgramData.h`) to `files_modified`; replace "parser targets struct" with "parser targets `main()` parameter list (primary form)" + struct path as the rare-asset fallback; relax `"isCompatibleWithVS definition unchanged"` to `"isCompatibleWithVS unchanged for native ctor compile; rewritten PS validated via per-VS reflected inputs before bind"`; add bind-path assertion log (`PSSetShader asset-PS ptr=...`) distinct from the rewrite log; pre-execution spike (manually reorder one PSRC parameter list, recompile, confirm validator passes) to de-risk before full implementation.

### Recommended Next Action

- **`/gsd:plan-phase 17 --gaps --reviews`** — re-plan the gap-closure wave with the Round 4 review feedback folded in. Both reviewers' HIGH-severity items should be addressed in the replan; the MEDIUM items should be folded in as suggestions.
- **DO NOT** `/gsd:execute-phase 17 --gaps` until the replan converges.
- Optional pre-replan: a **`writeVarByName` capability audit** (does it support sub-channel writes? if not, document the new helper signature) and a **PSRC syntax census** (count `main()` parameter-list vs struct-input shapes across char-select PSes from the captured dump at `evidence/plan-17-04x-psrc-source-dump.txt`) — both feed the replan with concrete data rather than speculation.


---

# Round 5 — Gap-Closure Replan Review (Plans 17-05 / 17-06a / 17-06b / 17-07)

Reviewers: **codex** + **cursor** (claude skipped for independence — review run from inside Claude Code). Reviewed at 2026-05-29T16:53:55Z. Both reviewed the Round-5 replans against the Round-4 HIGH/MEDIUM findings; Codex had read-only repo `exec` access, Cursor read the live tree.

## Codex Review (Round 5)

**Plan 17-05**
**Summary:** Sound as the wave orchestrator. The Round-4 dependency-cycle, capture reproducibility, canonical naming, and metric-inflation concerns are materially folded in. It correctly separates evidence capture from renderer fixes and no longer treats GAP-2 greps as visual parity.

**Strengths**
- Uses `17-05-task3` partial dependency, which makes the park/resume workflow orchestratable.
- Adds capture reproducibility pins and canonical `char_default_d3d11_0003.png` alias.
- Requires lane attribution via `asset-PS bound=` / `fallback-PS bound=`, not just `COMPATIBLE`.
- Explicitly frames GAP-2 as instrumentation-only for char-select.

**Concerns**
- **MEDIUM:** Task 4 does not require a fresh post-fix D3D9 screenshot, only a boot. That is probably acceptable, but if `SwgClient_d.exe` is rebuilt by 17-07, the “D3D9 baseline is invariant” claim is weaker than the evidence.
- **MEDIUM:** The PRE/POST grep set is documented, but the final verifier can still overfit prose from Kenny’s subjective visual delta. Require the verifier to state “PASS unavailable” if screenshot framing differs.
- **LOW:** Task 3 says commit exactly two PNGs, but Task 1’s README may be uncommitted unless the executor combines commits. Harmless, but the commit boundaries are fuzzy.
- **LOW:** “No SUMMARY.md required” is still a small audit discoverability gap, though `17-VERIFICATION.md` is a defensible replacement.

**Suggestions**
- In Task 4, require a fresh D3D9 screenshot if `SwgClient_d.exe` was rebuilt or if resolution/gamma/character slot changed.
- Add a verifier rule: if PRE/POST/D3D9 framing differs, CHAR verdicts cap at PARTIAL.
- Record commit SHA and `gl11_d.dll` / `SwgClient_d.exe` timestamps in `17-VERIFICATION.md`.

**Risk Assessment:** **LOW-MEDIUM.** Process risk remains, but the Round-4 folds are real.

---

**Plan 17-06a**
**Summary:** The split into discovery is the right response to Round 4, and Path B is now the default. The main remaining risk is that the plan assumes `Direct3d11_StaticShaderData.cpp` can access enough reflection metadata to walk inner types without touching PSData APIs. If cached layouts are name/offset/size only, the “re-reflect at dump-time” option may not be implementable inside the declared single-file scope.

**Strengths**
- Correctly demotes GAP-2 to instrumentation completeness, not char-select visual delivery.
- Correctly abandons Rule-D Path A as the primary path.
- Keeps the one-shot diagnostic pattern and plugin-local scope.
- Adds a blocking human evidence checkpoint before mapping.

**Concerns**
- **HIGH:** Option B says re-run `D3DReflect` from `Direct3d11_StaticShaderData.cpp` if cached metadata lacks type info, but the plan does not prove this file has access to PS bytecode or a PSData reflection API. If not, single-file scope breaks.
- **HIGH:** Even if `userConstants` reflection is walked, char-select shaders may optimize away unused material fields. The discovery may not expose future open-world diffuse/emissive semantics, so “unambiguous mapping evidence” is overclaimed.
- **MEDIUM:** D3D reflection type member sizes are not as simple as the plan implies. `D3D11_SHADER_TYPE_DESC` gives offsets, members, rows/columns/elements, but the implementation must compute sizes carefully for arrays/nested structs.
- **MEDIUM:** The gate fires only for head/eye anchors. If those anchors do not reflect the relevant fields, 17-06b has no safe mapping, yet the wave still expects GAP-2 closure.
- **LOW:** The plan promises a `17-06a-SUMMARY.md`, but it is not in `files_modified` or a concrete task acceptance block.

**Suggestions**
- Add a precondition: verify cached cbuffer layout includes type/member metadata, or explicitly add/read a PSData accessor before claiming single-file scope.
- Change success wording to “captures whatever inner reflection is available,” not “unambiguous diffuse/emissive mapping.”
- If only flat `userConstants[N]` appears, require 17-06b to defer unless engine source maps N to a named semantic with high confidence.
- Put `17-06a-SUMMARY.md` in `files_modified`.

**Risk Assessment:** **MEDIUM-HIGH.** The plan direction is right, but reflection access and semantic usefulness are not guaranteed.

---

**Plan 17-06b**
**Summary:** The new offset-write helper directly addresses the Round-4 `writeVarByName` limitation. However, the proposed helper is too permissive: an absolute-offset write that only checks `offset + 16 <= TotalSize` will return true for any cbuffer of sufficient size, short-circuiting fallbacks and potentially writing the wrong field.

**Strengths**
- Correctly depends on 17-06a evidence and blocks if the summary is missing.
- Adds an explicit offset helper instead of pretending dotted names work.
- Preserves `materialSpecularColor` and existing fallback candidates.
- Correctly states GAP-2 is not the char-select visual driver.

**Concerns**
- **HIGH:** `writeVarFloat4AtOffset` validates only bounds, not that the layout actually contains `userConstants` at the expected base/size. A wrong layout can still return true and short-circuit `material[0]` / `materialDiffuse` fallbacks.
- **HIGH:** The threat model says fallbacks mitigate a wrong offset, but the candidate chain uses `||`; if the offset helper returns true, fallbacks are not attempted. That mitigation is false.
- **HIGH:** Case B allows deriving diffuse/emissive from engine source if reflection is flat, but no concrete source contract is cited. If the source does not name the slots, this becomes guessing under another name.
- **MEDIUM:** The helper signature in the truth statement includes `parentName, memberName`, but the actual lambda only takes `absoluteOffset`. The safer named validation was dropped.
- **MEDIUM:** Offset overflow should be checked as `absoluteOffset <= layout.TotalSize - sizeof(XMFLOAT4)`, not `absoluteOffset + sizeof(...) > layout.TotalSize`.
- **LOW:** Case C adds a latent helper even when mapping is deferred. That is low risk but arguably unnecessary churn.

**Suggestions**
- Make the helper validate parent identity: find top-level `userConstants`, confirm expected `StartOffset`, `Size`, and that target offset lies inside it.
- Return false unless the parent var exists; then fallbacks remain meaningful.
- Add separate booleans/logs for “offset candidate attempted” vs “offset candidate accepted.”
- In Case B, require a cited source line mapping slot index to semantic; otherwise defer.

**Risk Assessment:** **HIGH** until the helper validates the parent region. This is the biggest missed correctness issue in the 06a/06b split.

---

**Plan 17-07**
**Summary:** The Round-4 folds are mostly real: parameter-list syntax is primary, StateCache is in scope, per-VS reflected inputs are cached, cache salting is acknowledged, and bind attribution is added. But the implementation plan still has serious correctness gaps in the spike, parser semantics, bind priority wording, and cache/resource ownership.

**Strengths**
- Correctly includes `Direct3d11_StateCache.cpp` and `Direct3d11_PixelShaderProgramData.h`.
- Correctly replaces ctor-time `m_reflectedPSInputs` with per-VS reflected inputs for rewritten PS validation.
- Adds distinct `asset-PS bound=` logging, closing the Round-4 metric-inflation issue.
- Adds VS-signature hash salting and rewrite-version bump.
- Keeps fallback PS as a safety net.

**Concerns**
- **HIGH:** Task 0 spike is not a real executable spike. It describes adding comments and “mentally” rewriting/validating; no harness or callable test path proves compilation/reflection/validator behavior.
- **HIGH:** The parser rule “PSRC parameters with no VS-output match stay at the FRONT” can shift register assignment and break matched params. Non-system unmatched inputs should generally cause rewrite failure/fallback, not be kept ahead of matched inputs.
- **HIGH:** The plan does not specify COM lifetime/release behavior for cached `ID3D11PixelShader*` entries. A per-VS cache can leak shaders unless destructor/cache cleanup releases them.
- **MEDIUM:** Bind priority is internally inconsistent. Truth text says `rewritten > native > fallback`, but the implementation says `native > rewritten > fallback`, which is the priority requested and the safer behavior.
- **MEDIUM:** `Direct3d11_VertexShaderData.{h,cpp}` may need edits for `computeOutputSignatureHash()`, but they are not declared in `files_modified`.
- **MEDIUM:** “Return input unchanged” conflates parse failure with “already in correct order.” The builder needs an explicit result enum: unchanged-compatible, rewritten, failed.
- **MEDIUM:** Per-VS cache keyed by raw VS pointer plus hash needs invalidation/lifetime assumptions documented. Pointer reuse after destruction would be bad if VSData objects are not process-lifetime stable.
- **LOW:** Full solution `/t:Rebuild` is conservative, but the plan’s “all plugin vcxprojs” wording is imprecise; it likely rebuilds more than plugins.

**Suggestions**
- Replace Task 0 with a small debug-only helper or local unit-style harness that actually compiles the sample and reflects input registers.
- Treat unmatched non-system semantics as rewrite failure. Handle `SV_Position` / system-value semantics explicitly.
- Add cache entry destructor/release logic and document ownership.
- Fix all wording to `native > rewritten > fallback`.
- Add `Direct3d11_VertexShaderData.cpp/.h` to `files_modified` if the hash helper is not already present.
- Make the rewrite helper return `{status, text}` instead of using string equality as control flow.

**Risk Assessment:** **HIGH.** It is the right architecture, but the parser/cache/bind implementation still has enough sharp edges to produce grep-green / visual-red outcomes.

---

**Cross-Plan Assessment**
**Summary:** Round-4 feedback was not merely cosmetic. The replan genuinely fixes the dependency cycle, splits 17-06, demotes GAP-2 visual claims, adds StateCache/header scope to 17-07, targets parameter-list PSRC, adds per-VS reflected inputs, and adds bind attribution. However, 17-06b’s offset helper is unsafe as specified, and 17-07 remains high-risk despite the improved architecture.

**Strengths**
- The wave is now conceptually coherent: 17-05 PRE evidence → 17-06a discovery → 17-06b mapping → 17-07 bind activation → 17-05 POST verification.
- The plans now correctly separate “metric closed” from “visual fixed.”
- The `17-05-task3` partial dependency is real and should avoid the Round-4 deadlock.
- `asset-PS bound=` is the right anti-inflation metric.

**Concerns**
- **HIGH:** 17-07 does not depend on 17-06b in metadata, even though the narrative says Wave 7 is serial and final `gl11_d.dll` must contain both. If orchestrated in parallel, the deployed artifact can miss one plan’s changes.
- **HIGH:** 17-06b can falsely close GAP-2 by writing any in-bounds offset and short-circuiting fallbacks.
- **HIGH:** 17-07’s Task 0 spike is mostly procedural theater unless turned into an actual compile/reflect check.
- **MEDIUM:** Several promised summaries are outside `files_modified` and not concrete tasks, weakening audit closure.
- **MEDIUM:** Grep gates remain necessary but gameable. The final authority must be screenshots plus bind-path attribution, not log count thresholds alone.
- **MEDIUM:** If 17-06a cannot expose semantic names, 17-06b must be allowed to close DEFERRED without delaying 17-07/17-05 POST verification.

**Suggestions**
- Make execution order explicit: `17-07 depends_on: ["17-04", "17-05-task3", "17-06b"]`, or add a final full rebuild after both 06b and 07.
- Tighten 17-06b helper validation before execution.
- Replace 17-07 Task 0 with an actual temporary harness or remove it and classify the first implementation boot as the spike.
- Add all summary files and potential `VertexShaderData` files to frontmatter.
- Final Phase 17 PASS should require: matched A/B visual parity, `asset-PS bound >= 8`, D3D9 boot clean, and explicit residual-gap notes. Anything less is PARTIAL.

**Overall Risk:** **HIGH for execution as written**, mostly due to 17-06b and 17-07. The replan is much better than Round 4, but I would require the 06b helper safety fix and the 17-07 spike/parser/cache clarifications before calling this execution-ready.

---

## Cursor Review (Round 5)

# Phase 17 Gap-Closure Wave (Round 5) — Cross-AI Plan Review

Independent review of the **Round-5 replans** (17-05, 17-06a, 17-06b, 17-07) against Round-4 HIGH/MEDIUM findings and the live tree at `D:/Code/swg-client-v2`.

**Bottom line:** Round-4 folds are **mostly real**, not cosmetic. The split (06a/06b), `17-05-task3` partial dependency, instrumentation-vs-visual framing, `writeVarFloat4AtOffset`, StateCache bind wiring, parameter-list rewriter, and per-VS reflected-input cache all address the right blockers. **Convergence is partial:** several cross-plan grep contracts, build/merge ordering, and 17-07 implementation details still have gaps that can produce **grep-green / visual-red** outcomes.

---

## Round-4 Fold Verification (summary)

| Round-4 item | Folded? | Verdict |
|---|---|---|
| Dependency cycle (`17-05-task3`) | Yes | Real in frontmatter for 06a/07; orchestrator must understand partial deps |
| Capture reproducibility | Yes | Task 1 contract is substantive |
| Success-metric inflation | Partial | `asset-PS bound=` added; `Plan 17-07 COMPATIBLE` grep still mismatched |
| 06 split + Path B default | Yes | 06a/06b split is the right structure |
| `writeVarByName` can't do nested writes | Yes | 06b adds offset helper with bounds check |
| GAP-2 not char-select visual driver | Yes | Honest in all four plans |
| 07 `files_modified` dishonest | Yes | StateCache + PSData.h now declared |
| PSRC parameter-list vs struct | Yes | 07 targets `main()` params; dump confirms 22× `float4 main`, 0 struct-bound |
| HIGH-6 ctor-time `m_reflectedPSInputs` | Yes | Per-VS cache + overload planned |
| Cache-key VS salt | Yes | Specified; live tree already at `D3D11_REWRITE_VERSION=21` |
| Pre-exec spike | Partial | Task 0 is manual/comment-only, not a compile gate |

---

## Plan 17-05 — Evidence / Verification Orchestrator

### Summary
The strongest plan in the wave. PRE/POST capture design, Iter-44B discipline, instrumentation-vs-visual split, symmetric grep sets, and canonical alias handling are all sound. Remaining risk is **process/orchestration**, not renderer logic.

### Strengths
- **`17-05-task3` park point** breaks the Round-4 dependency deadlock cleanly; downstream plans declare `depends_on: ["17-04", "17-05-task3"]`.
- **Capture contract** (slot, camera, UI, resolution, gamma) is concrete enough to reject bad captures.
- **Dual-axis Task 5 verdicts** (instrumentation / visual / lane attribution) directly address Round-4 HIGH-3.
- **PRE-gap narrative honesty:** fallback Variant T may already show partial texture — not necessarily magenta.
- **No source edits** — D-06 safe.

### Concerns
- **MEDIUM — `17-05-task3` resolver support unproven.** Plans assume GSD's orchestrator resolves a named partial dependency from a handoff string. If it only understands full plan completion, the cycle returns. Task 3 requires literal `17-05-task3` in handoff — verify orchestrator implements this.
- **MEDIUM — POST-gap deploy contract underspecified.** Task 4 says deploy gl11 built from 17-06a+06b+17-07 but does not mandate **merge order** (17-07 full rebuild should be last) or record **commit SHA + gl11_d.dll timestamp** in evidence README — both cited in Round-4 suggestions, not folded.
- **MEDIUM — Idle-animation frame drift.** "Capture within 3 seconds" does not guarantee same idle frame across D3D9 / PRE / POST; diff noise can false-fail or false-pass CHAR verdicts.
- **MEDIUM — `Plan 17-07 .* COMPATIBLE vs=` grep may not fire.** Live validator logs use `Plan 17-04 Task 1 VS<->PS pair compatibility: COMPATIBLE vs=...` (see ```1297:1300:src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.cpp```). 17-07 does not require new COMPATIBLE logs with a `Plan 17-07` prefix; POST-gap grep for `Plan 17-07 .* COMPATIBLE` may count **zero** even when rewrites succeed. `COMPATIBLE vs=` alone would work on existing logs.
- **LOW — Task 2 accepts D3D9 boot from PNG existence only** — no log-clean gate for D3D9 baseline boot.
- **LOW — `verified_by: pending`** — 17-VERIFICATION.md is executor-authored, not independently verified; milestone readers may over-read closure.

### Suggestions
1. Task 4 deploy step: **"Build/deploy gl11_d.dll from a single commit containing 17-06b + 17-07; 17-07 rebuild MUST be last."** Record SHA + DLL mtime in README.
2. Fix grep set: either grep `COMPATIBLE vs=` (works today) **or** require 17-07's overload to emit `Plan 17-07 COMPATIBLE vs=`.
3. Add evidence README field: **expected PRE-gap appearance** (fallback-PS partial texture vs magenta).
4. Document orchestrator behavior for `17-05-task3` in STATE/handoff (already started in `.planning/handoff/phase-17-char-select.md`).

### Risk Assessment
**LOW–MEDIUM** — Process-heavy, technically safe; grep/orchestration gaps can block or mis-score closure without code risk.

---

## Plan 17-06a — GAP-2 Discovery (userConstants inner walk)

### Summary
Correctly demotes Path A, extends the existing 17-04 dump, and stays plugin-local. The discovery half is well-scoped. Live code confirms **Option (a) is impossible** — cached layout vars carry only `Name/StartOffset/Size`, no type descriptors (```66:78:src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.h```), so **Option (b) re-reflect at dump time is mandatory**, not executor discretion.

### Strengths
- Extends existing gated dump at ```925:943:src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp``` — no parallel instrumentation.
- Path A dead-end documented with live code comments at ```887:896:Direct3d11_StaticShaderData.cpp``` matching PSRC (`textureFactor.rgb`, not cbuffer diffuse).
- One-shot / depth-2 / ~34 lines — log budget sane.
- Checkpoint produces evidence 06b can block on without guessing.

### Concerns
- **HIGH — Reflection may not yield semantic inner names.** `userConstants` may reflect as opaque blob or `float4[17]` with synthetic indices only. Case (C)-like outcome is common; 06b's diffuse/emissive mapping may require **D3D9 c-register folklore** (engine source), contradicting the "no guessing" story.
- **MEDIUM — Option (b) not pinned in acceptance.** Task 1 should **require** dump-time `D3DReflect` on bound PS bytecode (or stored `m_psBytecodeBlob`) since cache lacks type info.
- **MEDIUM — Anchor gating via `strstr(..., "head"/"eye")`** — fragile; dump may miss body/clothing anchors that also exercise GAP-2 metrics.
- **MEDIUM — Blocks wave autonomy** — 06a is non-autonomous; 06b cannot run until Kenny checkpoint + SUMMARY lands.
- **LOW — `wroteSpecular` already 9+** — dump gate `incomplete = !(D&&S&&E)` fires on every anchor until diffuse+emissive land; expected, but noisy.

### Suggestions
1. Task 1 acceptance: **grep for `GetMemberTypeByIndex` AND `m_psBytecodeBlob` or `D3DReflect`** — forbid cache-only inner walk.
2. If array-only reflection: 06a SUMMARY must flag **"Case B — slot indices need D3D9 upload trace"** explicitly.
3. Consider one body-shader anchor in dump gate (census-derived), not only head/eye substrings.

### Risk Assessment
**MEDIUM** — Low regression risk; discovery may not produce mappable names, delaying or deferring 06b without failing char-select visuals (which is fine per HIGH-3).

---

## Plan 17-06b — GAP-2 Mapping / Write

### Summary
Round-4 HIGH-1 is **substantively closed**: `writeVarFloat4AtOffset` with `offset + 16 <= layout.TotalSize` mirrors ```806:807:src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp```. Evidence-driven mapping, Case C deferral, and preserved scalar fallbacks are correct. Success is **instrumentation completeness**, not CHAR visuals — appropriately scoped.

### Strengths
- **Blocks without 06a-SUMMARY** — no guessing path.
- **Case A/B/C branching** with honest DEFERRED outcome.
- **Single-candidate-per-channel** preserved vs old multi-index stomp risk.
- Plugin-local; no ABI trap.

### Concerns
- **MEDIUM — Case B secondary source is weak.** `setPixelShaderUserConstants_impl` (```679:704:src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp```) copies engine arrays into `shadow.userConstants[i]` but does **not document which index is diffuse vs emissive**. Cross-referencing D3D9 register layout is plausible but **not spelled out** in the plan — executor may still guess.
- **MEDIUM — Whole `float4` write at arbitrary offset** can stomp adjacent engine state if index wrong; bounds check prevents OOB but not semantic corruption. Mitigated by HIGH-3 (char-select PSes don't read these cbuffers) — acceptable for beachhead, risky for Phase 20.
- **MEDIUM — `wroteDiffuse=1` may remain 0 post-06b** and still be correct (Task 4 already caveats this). Risk: executor treats 0 as failure vs expected instrumentation outcome.
- **LOW — 06b wave-7 parallel with 17-07** — both rebuild `gl11_d.dll`; no explicit `depends_on` between them. OK if merge commit + 17-07 rebuild last; not stated in 06b.

### Suggestions
1. Case B: cite concrete D3D9 reference (e.g. `Direct3d9_StaticShaderData.cpp` upload order or `PSCR_userConstant` indices) as **required read_first**, not optional.
2. 06b SUMMARY must state: **"wroteDiffuse=0 acceptable on char-select if PSRC consumes textureFactor only."**
3. Add orchestrator note: **06b commit must be in the same branch as 17-07 before Kenny POST-gap boot.**

### Risk Assessment
**MEDIUM** — Implementation is straightforward; mapping evidence may not support Case A; false metric closure is possible but visually harmless on char-select.

---

## Plan 17-07 — GAP-3 PS Rewrite + Bind Path

### Summary
The **highest-value and highest-risk** plan. Round-4 architectural fixes are largely folded: StateCache bind site (```1121:1137:src/engine/client/application/Direct3d11/src/win32/Direct3d11_StateCache.cpp```), parameter-list primary form, per-VS reflected inputs, cache salt, bind attribution logs, Task 0 spike, full-plugin rebuild for header touch. This is the **primary char-select visual driver** alongside existing `textureFactor` uploads (already hitting via `writeVarByName("textureFactor", ...)` at ```907:908:Direct3d11_StaticShaderData.cpp```).

### Strengths
- Root cause correctly identified: register-position strict match at ```1245:1251:src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.cpp```.
- **Bind priority** `rewritten > native > fallback` at the actual `PSSetShader` decision point.
- **Per-VS cache stores reflected inputs** — fixes HIGH-6 without mutating ctor-time cache.
- **`m_program->m_psrcText` is accessible** (public on `ShaderImplementationPassPixelShaderProgram`) for lazy rewrite — plan's `m_psrcText` wording is slightly off but workable.
- Task 2 checkpoint before 17-05 POST capture — good fail-fast.
- Shared-header ABI trap explicitly handled with full-solution Rebuild.

### Concerns
- **HIGH — COMPATIBLE log / grep contract mismatch with 17-05.** 17-05 Task 4 expects `Plan 17-07 .* COMPATIBLE vs=`. Task 1 adds `asset-PS bound=` but **does not specify** `Plan 17-07 COMPATIBLE vs=` lines from `isCompatibleWithVS_withExplicitPSInputs`. Existing success path logs **Plan 17-04** prefix. POST-gap acceptance can fail while binds work, or pass on wrong metric.
- **HIGH — Parameter-list reorder ≠ guaranteed register fix.** Plan assumes declaration-order → `vN` assignment. Correct for typical `ps_2_0`/`ps_4_0` without explicit `register(vN)` on params (char-select PSRC matches — ```139:144:.planning/phases/17-psrc-census-char-select-beachhead/evidence/plan-17-04x-psrc-source-dump.txt```). **Not validated by Task 0** — spike is comment-only, no compile/reflect in CI.
- **MEDIUM — `D3D11_REWRITE_VERSION` already `"21"`** in live code (```153:153:src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.cpp```). Plan says bump 20→21; executor must bump to **22** or cache invalidation for 17-07 changes won't happen.
- **MEDIUM — `Direct3d11_VertexShaderData.h` likely touched** for `computeOutputSignatureHash()` but **not in `files_modified`** — ABI/rebuild scope incomplete.
- **MEDIUM — Full-solution `/t:Rebuild`** is correct for ABI trap but expensive and may surface unrelated breakage; plan has no narrowed target list if Rebuild fails mid-graph.
- **MEDIUM — Parser fragility:** multi-line params, `#include` expansion, `inout`, macros, profiles like `ps_1_1` (```167:168:evidence/plan-17-04x-psrc-source-dump.txt```) — non-fatal fallback preserves boot but can leave **0/9 rewritten**.
- **MEDIUM — `>= 8/9` threshold** hides one persistent INCOMPATIBLE pair; Task 5 should document which shader pair remains on fallback and whether it affects eyes/head.
- **LOW — Per-VS cache keyed on VS pointer** — fine for template singletons; hash salt on outputs is the real guard against wrong PS blob.
- **LOW — Task 1 is a single monolithic task** — high executor blast radius; spike doesn't gate with automated compile.

### Suggestions
1. **Align logs with 17-05 greps:** emit `Plan 17-07 COMPATIBLE vs=` / `INCOMPATIBLE vs=` from the overload **or** change 17-05 to grep existing `Plan 17-04 Task 1 ... COMPATIBLE vs=` for native path + `Plan 17-07 ...` only for rewritten path.
2. Bump **`D3D11_REWRITE_VERSION` to `"22"`** explicitly in acceptance criteria.
3. Add **`Direct3d11_VertexShaderData.h/.cpp`** to `files_modified` if hash helper is new.
4. Task 0: require **one automated compile + D3DReflect register dump** (even a unit-style harness in PSData.cpp under `#if 0` test block) before Task 1 proceeds.
5. Task 1 acceptance: post-rewrite **`Register` match** for COLOR0/TEXCOORD0 on at least `h_simple_pp_ps20.psh` against a known INCOMPATIBLE VS from Kenny's log.

### Risk Assessment
**HIGH** — Correct strategic target; implementation + verification contracts still have holes that can yield fallback-PS visuals with inflated bind logs or vice versa.

---

## Cross-Plan Assessment

### Wave ordering (Waves 5 → 6 → 7)

```
17-05 Tasks 1–3 (PRE evidence) ──► 17-05-task3 park
         ├─► 17-06a (discovery + Kenny boot) ──► 17-06b (mapping write)
         └─► 17-07 (rewrite + bind)     [parallel with 06b after 06a checkpoint]
17-05 Task 4–5 (POST evidence + VERIFICATION) ◄── requires merged 06b + 07 gl11
```

**Sequencing is orchestratable** if:
1. Orchestrator honors `17-05-task3` (not full 17-05 complete).
2. **17-07 lands last** (header touch + full rebuild) or POST-gap uses a merge commit containing both 06b and 07.
3. Kenny runs **one** POST-gap boot on the combined DLL.

**Stale-DLL hazard:** Real but mitigated if 17-07's full Rebuild is the final step. **Not documented as a hard gate in 17-05 Task 4 step 1** — fold incompletely.

**06b graceful dependency on 06a-SUMMARY:** Real — 06b Task 1 precondition blocks cleanly.

### Do these four plans close Phase 17 / CHAR-01/02/03?

| Gap | Likely closed? | CHAR impact |
|-----|----------------|-------------|
| **GAP-1** (evidence) | **Yes**, if Kenny captures | Enables honest verdict; doesn't fix visuals |
| **GAP-2** (cbuffer writes) | **Partial** (metrics/instrumentation) | **Low** on char-select — PSRC uses `textureFactor` + `COLOR0` |
| **GAP-3** (asset PS binds) | **Conditional** on 17-07 parser + bind wiring | **Primary** driver for CHAR-01/03; CHAR-02 also needs correct interpolators + composites |

**Expected outcome if executed as written:**
- GAP-1: closed with evidence.
- GAP-2: closed on greps **or** DEFERRED (Case C) — likely **no visible delta** on char-select.
- GAP-3: **best effort** — if parameter reorder works, CHAR-01 moves toward PASS; eyes/head (CHAR-02/03) may remain PARTIAL until multi-stage + palette confirmed in A/B narrative.
- Phase 17: **PARTIAL → PASS plausible**, not guaranteed blanket CHAR PASS.

### Verifiability — are grep gates measuring the right thing?

| Metric | Gameable / wrong? |
|--------|-------------------|
| 4 PNGs + written diff | **Good** — necessary for Iter-44B |
| `wroteDiffuse/Emissive=1` | **Misleading alone** on char-select — plans now say so |
| `Plan 17-07 COMPATIBLE vs=` | **Broken contract** unless 17-07 adds those log lines |
| `asset-PS bound=` | **Good** if emitted at `PSSetShader` — distinguishes validator vs bind |
| `COMPATIBLE vs=` (17-04 prefix) | **Works today** but 17-05 doesn't use it consistently |

### Scope vs over-engineering

- **17-05:** appropriately scoped; slightly heavy grep arithmetic in acceptance — fine.
- **06a/06b:** right-sized for instrumentation; not over-engineered given HIGH-3 honesty.
- **17-07:** large single Task 1 (parser + cache + overload + StateCache + full rebuild) — **under-decomposed** for HIGH risk; struct-bound fallback may be YAGNI for char-select (0 instances) but cheap as second parser path.

### Axis-(b) vs axis-(a)

**Yes, lower risk than VS reordering** — confirmed. **Not low risk in absolute terms** — parameter-list rewrite + per-VS cache + shared-header rebuild is still a major change.

---

## Overall Verdict

**CONVERGENCE PARTIAL — executable with targeted amendments, not blind `/gsd:execute-phase 17 --gaps`.**

Round-4 HIGH items are **mostly substantively folded**, not lipstick:
- Dependency cycle → `17-05-task3` ✓
- 06 split + Path B + offset helper ✓
- 07 StateCache + parameter-list + per-VS reflection cache ✓
- Metric inflation → `asset-PS bound=` ✓ (partial)

**Remaining blockers before execution:**

1. **Unify COMPATIBLE log grep** across 17-05 and 17-07 (HIGH).
2. **Mandate POST-gap deploy from merge commit; 17-07 rebuild last; record SHA/DLL mtime** (MEDIUM).
3. **06a: require re-reflect path** (cached layout has no type info) (MEDIUM).
4. **07: bump rewrite version to 22; add VertexShaderData.h to scope if needed; strengthen Task 0 to real compile** (MEDIUM).
5. **Confirm orchestrator supports `17-05-task3`** (MEDIUM).

**Recommended execution priority:** 17-05 Tasks 1–3 → **17-07** (with grep/version fixes) → 06a → Kenny discovery → 06b → 17-07 Task 2 bind checkpoint → 17-05 Tasks 4–5.

**Phase 17 beachhead goal:** These plans **can** prove end-to-end asset PS pipeline on char-select **if GAP-3 lands and POST A/B shows parity** — but automatic CHAR-01/02/03 PASS is **not guaranteed** even with all grep metrics green. Plan for **PARTIAL with honest lane attribution** as the likely floor, PASS as the ceiling.

---

## Round 5 Consensus

Both reviewers ran independently — **Codex** (prompt + read-only repo `exec` access) and **Cursor** (live-tree read via `--mode ask`). They converged tightly: the Round-4 HIGH/MEDIUM folds are **substantive, not cosmetic**, but execution is gated on a small, well-defined amendment set. Neither reviewer recommends a blind `/gsd:execute-phase 17 --gaps`.

**Overall verdict: CONVERGENCE PARTIAL** — executable after targeted amendments. Per-plan risk: 17-05 **LOW–MEDIUM**, 17-06a **MEDIUM**, 17-06b **MEDIUM**, 17-07 **HIGH**.

### Agreed Strengths (both reviewers)

- The wave is now coherent: 17-05 PRE evidence → 17-06a discovery → 17-06b mapping → 17-07 bind activation → 17-05 POST verification.
- `17-05-task3` partial-dependency park point genuinely breaks the Round-4 dependency deadlock.
- Plans correctly separate **"metric closed" from "visual fixed"** (Round-4 HIGH-3 honored across all four).
- `asset-PS bound=` is the right anti-inflation metric — it distinguishes "validator returned COMPATIBLE" from "PSSetShader actually bound the asset PS."
- The 17-06 split (06a/06b) and Path-B default are the right structure; `writeVarFloat4AtOffset` with `offset + 16 <= layout.TotalSize` correctly closes Round-4 HIGH-1.
- 17-07 puts the bind decision where it actually lives (StateCache.cpp:1121-1137), targets `main()` parameter-list form (dump-confirmed 22× `float4 main`, 0 struct-bound), and caches per-VS reflected inputs to fix HIGH-6 without mutating the ctor-time cache.

### Agreed Concerns (raised by both, or raised by one and verified — highest priority)

- **HIGH — COMPATIBLE-log grep contract mismatch (17-05 ↔ 17-07).** 17-05 Task 4 greps `Plan 17-07 .* COMPATIBLE vs=`, but 17-07 only specifies the new `asset-PS bound=` log; the existing validator emits a **`Plan 17-04`** prefix (Cursor cites `Direct3d11_PixelShaderProgramData.cpp:1297-1300`). POST-gap acceptance can read **zero** even when rewrites succeed — grep-green/visual-red or the inverse. *(Cursor HIGH, Codex via "grep gates gameable / measuring wrong thing".)* **Fix:** either emit `Plan 17-07 COMPATIBLE vs=` from the overload, or change 17-05 to grep the existing `COMPATIBLE vs=` (works today) for the native path and `Plan 17-07 ...` only for the rewritten path.
- **HIGH — 17-07 has no `depends_on: 17-06b`; merge/rebuild order is a stale-DLL hazard.** Both plans are Wave 7 and both rebuild `gl11_d.dll`; the narrative says "serial, 17-07 last" but nothing in metadata enforces it. If orchestrated in parallel, the deployed DLL can miss one plan's changes. *(Codex HIGH, Cursor MEDIUM + cross-plan.)* **Fix:** add `17-07 depends_on: ["17-04","17-05-task3","17-06b"]` **or** mandate a single merge commit with 17-07's full Rebuild as the final step, and record the SHA + `gl11_d.dll` mtime in evidence/README before Kenny's POST-gap boot.
- **HIGH — 17-06b can falsely close GAP-2.** `writeVarFloat4AtOffset` short-circuits the scalar fallbacks; writing *any* in-bounds offset sets `wroteDiffuse=1` even if the offset is the wrong slot — green metric, wrong (or semantically corrupting) write. Bounds check prevents OOB, not mis-identification. Mitigated on char-select by HIGH-3 (those PSes don't read these cbuffers) but risky for Phase 20. *(Codex HIGH, Cursor MEDIUM.)* **Fix:** tighten helper validation; require the Case-B slot identification to cite a concrete D3D9 upload-order reference (not "plausible folklore"); allow honest Case-C DEFERRED.
- **HIGH — 17-07 Task 0 spike is procedural theater.** It is comment-only ("mentally rewrite", record result as a comment) — no actual compile + D3DReflect register-position check gates Task 1. The core assumption (declaration-order → `vN` register assignment) is therefore unvalidated before the monolithic Task 1 lands. *(Both HIGH.)* **Fix:** make Task 0 a real compile + reflect on one known-INCOMPATIBLE char-select pair (e.g. `h_simple_pp_ps20.psh`), or reclassify the first implementation boot as the spike.
- **MEDIUM (verified by Cursor against live tree) — `D3D11_REWRITE_VERSION` is already `"21"`.** 17-07 says "bump 20→21"; live code is at `"21"` (`Direct3d11_PixelShaderProgramData.cpp:153`). As written the bump is a **no-op** and stale `.cso` caches won't invalidate. **Fix:** bump to `"22"` explicitly in the acceptance criterion.
- **MEDIUM — `Direct3d11_VertexShaderData.h/.cpp` likely touched but not in `files_modified`.** `computeOutputSignatureHash()` is probably new; if so it's a second header touch with its own ABI/rebuild scope. *(Both.)* **Fix:** add it to frontmatter (or confirm the helper already exists during read_first).
- **MEDIUM — 17-06a Option (a) is impossible; re-reflect is mandatory.** Cursor verified the cached layout vars carry only `Name/StartOffset/Size`, no type descriptors (`Direct3d11_PixelShaderProgramData.h:66-78`), so the dump-time inner walk **must** re-run `D3DReflect` on the bound PS bytecode — it is not executor discretion. **Fix:** make Option (b)/re-reflect a hard acceptance requirement (grep for `GetMemberTypeByIndex` AND `D3DReflect`/`m_psBytecodeBlob`).
- **MEDIUM — `17-05-task3` orchestrator support is unproven.** Both plans assume GSD's orchestrator resolves a named *partial* dependency from a handoff string. If it only understands full-plan completion, the Round-4 cycle returns. **Fix:** confirm/implement partial-dep resolution (a start exists at `.planning/handoff/phase-17-char-select.md`).
- **MEDIUM — Summaries/audit-trail gaps.** Several promised SUMMARY files sit outside `files_modified` and aren't concrete tasks; final Phase-17 PASS authority must be screenshots + bind-path attribution, not log thresholds alone.

### Divergent / Unique Views

- **Cursor (repo-verified, weight heavily):** exact live-tree citations — REWRITE_VERSION already `"21"`; COMPATIBLE-log prefix is `Plan 17-04` not `Plan 17-07`; cached layout has no type info; `setPixelShaderUserConstants_impl` (`Direct3d11.cpp:679-704`) does **not** document which userConstants index is diffuse vs emissive (Case-B identification weaker than the plan implies); anchor gating via `strstr("head"/"eye")` may miss body/clothing anchors. Recommends execution order **17-05 T1-3 → 17-07 → 06a → 06b → 17-07 T2 → 17-05 T4-5**.
- **Codex (unique):** per-VS cache keyed on raw VS pointer needs documented lifetime/invalidation (pointer reuse after destruction is a latent bug); the rewrite helper should return `{status, text}` rather than using string-equality as control flow; unmatched non-system semantics should be treated as rewrite *failure* and `SV_Position`/system-values handled explicitly; "all plugin vcxprojs" wording is imprecise (the full `/t:Rebuild` rebuilds more than plugins).
- **No genuine disagreements** — where one reviewer rated an item HIGH and the other MEDIUM (06b false-closure; 17-07 depends_on), the lower rating reflects the char-select HIGH-3 mitigation, not a dispute about the defect.

### Recommended Next Action

- **`/gsd:plan-phase 17 --gaps --reviews`** — fold the Round-5 amendments in. Both reviewers' HIGH items gate execution; the MEDIUMs should land as concrete acceptance-criteria edits, not prose.
- Minimum HIGH set to clear before `/gsd:execute-phase 17 --gaps`:
  1. Unify the COMPATIBLE-log grep contract across 17-05 and 17-07.
  2. Enforce 17-07-after-17-06b ordering (depends_on or single merge commit + final full rebuild) and record SHA/DLL mtime.
  3. Make 17-07 Task 0 a real compile+reflect gate; bump `D3D11_REWRITE_VERSION` to `"22"`; add `Direct3d11_VertexShaderData.h` to scope if the hash helper is new.
  4. Tighten 17-06b so a wrong-offset write can't masquerade as GAP-2 closure; pin 17-06a re-reflect as mandatory.
  5. Confirm the orchestrator honors the `17-05-task3` partial dependency.
- **DO NOT** `/gsd:execute-phase 17 --gaps` until these converge.
