---
phase: 17
review_round: 4
reviewers: [codex, cursor]
reviewed_at: 2026-05-29T08:40:00Z
plans_reviewed: [17-01-PLAN.md, 17-02-PLAN.md, 17-03-PLAN.md, 17-05-PLAN.md, 17-06-PLAN.md, 17-07-PLAN.md]
prior_rounds:
  - "Round 1 review at commit 797e9db57 (`docs: cross-AI review for phase 17`)"
  - "Round 2 replan at commit 6e0eae4e6 (`docs(17): incorporate cross-AI review feedback (codex+cursor)`)"
  - "Round 2 review at commit 85ff0b90b (`docs(17): cross-AI review round 2 (codex+cursor) — convergence NOT achieved`)"
  - "Round 3 replan at commit 6cfe19a7f (`docs(17): round 3 replan — convergence amendments (SR-2 reflect, HIGH-3 shadow, HIGH-4 routing, SR-3 split, plumbing fixes)`)"
  - "Round 3 review at commit (this file's prior state) — CONVERGENCE ACHIEVED for plans 17-01/02/03"
  - "Plans 17-01..17-04 executed; Phase 17 closed as INFRASTRUCTURE COMPLETE at commit 41d7f2fec; post-04 follow-up commit 04ef976 added materialSpecularColor recognition"
  - "Synthesized 17-VERIFICATION.md inventoried GAP-1 (no A/B screenshot), GAP-2 (no wroteDiffuse/wroteEmissive), GAP-3 (0/9 asset-PS binds — COLOR0 register-position mismatch)"
  - "Round 4 plans 17-05/06/07 authored as the gap-closure wave"
verdict_round3: CONVERGENCE ACHIEVED — plans 17-01/02/03 execution-ready
verdict_round4: CONVERGENCE NOT ACHIEVED — gap-closure plans 17-05/06/07 have HIGH-severity concerns that gate execution (writeVarByName sub-channel impossibility, files_modified contract violation, PSRC syntax mismatch, depends_on metadata deadlock)
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
